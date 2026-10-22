#include "protocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

#include <cmath>

namespace acsd {
namespace gui {
namespace {

const QString kConfigSchema = QStringLiteral("acsd.config/2");
const QString kEventProtocol = QStringLiteral("acsd.events/2");
const QString kControlProtocol = QStringLiteral("acsd.control/2");

bool isInteger(const QJsonValue& value, qint64* out) {
  if (!value.isDouble()) {
    return false;
  }
  const double d = value.toDouble();
  if (!std::isfinite(d) || d != std::floor(d)) {
    return false;
  }
  if (std::fabs(d) > 9.007199254740992e15) {
    return false;
  }
  *out = static_cast<qint64>(d);
  return true;
}

EventKind kindFromName(const QString& name, bool* known) {
  *known = true;
  if (name == QStringLiteral("run_started")) return EventKind::RunStarted;
  if (name == QStringLiteral("frame_started")) return EventKind::FrameStarted;
  if (name == QStringLiteral("block_progress")) return EventKind::BlockProgress;
  if (name == QStringLiteral("frame_finished")) return EventKind::FrameFinished;
  if (name == QStringLiteral("product_published")) return EventKind::ProductPublished;
  if (name == QStringLiteral("issue")) return EventKind::Issue;
  if (name == QStringLiteral("run_finished")) return EventKind::RunFinished;
  *known = false;
  return EventKind::Unknown;
}

// 从 "acsd.events/2" 取 major；不是该形态返回 nullopt。
std::optional<int> protocolMajorOf(const QString& text) {
  static const QRegularExpression re(QStringLiteral("^acsd\\.([a-z]+)/(\\d+)$"));
  const QRegularExpressionMatch m = re.match(text);
  if (!m.hasMatch()) {
    return std::nullopt;
  }
  bool ok = false;
  const int major = m.captured(2).toInt(&ok);
  if (!ok) {
    return std::nullopt;
  }
  return major;
}

QByteArray jsonEscape(const QString& text) {
  QString escaped;
  escaped.reserve(text.size() + 8);
  for (const QChar ch : text) {
    const ushort u = ch.unicode();
    switch (u) {
      case '"': escaped += QLatin1String("\\\""); break;
      case '\\': escaped += QLatin1String("\\\\"); break;
      case '\b': escaped += QLatin1String("\\b"); break;
      case '\f': escaped += QLatin1String("\\f"); break;
      case '\n': escaped += QLatin1String("\\n"); break;
      case '\r': escaped += QLatin1String("\\r"); break;
      case '\t': escaped += QLatin1String("\\t"); break;
      default:
        if (u < 0x20) {
          escaped += QStringLiteral("\\u%1").arg(u, 4, 16, QChar('0'));
        } else {
          escaped += ch;
        }
    }
  }
  return escaped.toUtf8();
}

// 递归收集版本负载里的 acsd.*/N 协议串与数值 major。
void collectVersion(const QJsonValue& value, CliVersion* out) {
  if (value.isObject()) {
    const QJsonObject obj = value.toObject();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
      const QString key = it.key();
      const QJsonValue child = it.value();
      if (child.isString()) {
        const QString text = child.toString();
        const auto major = protocolMajorOf(text);
        if (major.has_value()) {
          out->known = true;
          if (key == QStringLiteral("events") || text == kEventProtocol) {
            out->eventMajor = major;
          } else if (key == QStringLiteral("control") || key == QStringLiteral("controls") ||
                     text == kControlProtocol) {
            out->controlMajor = major;
          } else if (key == QStringLiteral("config") || text == kConfigSchema) {
            out->configMajor = major;
          } else if (!out->eventMajor.has_value() && text.startsWith(QStringLiteral("acsd.events/"))) {
            out->eventMajor = major;
          }
        }
      } else if (child.isDouble()) {
        const double number = child.toDouble();
        const bool integral = std::isfinite(number) && number == std::floor(number) &&
                              number > 0.0 && number < 1.0e9;
        const int major = integral ? static_cast<int>(number) : 0;
        if (integral) {
          if (key == QStringLiteral("event_major") || key == QStringLiteral("events_major")) {
            out->known = true;
            out->eventMajor = major;
          } else if (key == QStringLiteral("control_major")) {
            out->known = true;
            out->controlMajor = major;
          } else if (key == QStringLiteral("config_major")) {
            out->known = true;
            out->configMajor = major;
          } else if (key == QStringLiteral("major") && !out->known) {
            out->known = true;
            out->eventMajor = major;
          }
        }
      } else {
        collectVersion(child, out);
      }
    }
  } else if (value.isArray()) {
    for (const QJsonValue& child : value.toArray()) {
      collectVersion(child, out);
    }
  }
}

}  // namespace

QString configSchemaString() { return kConfigSchema; }
QString eventProtocolString() { return kEventProtocol; }
QString controlProtocolString() { return kControlProtocol; }

QString eventKindName(EventKind kind) {
  switch (kind) {
    case EventKind::RunStarted: return QStringLiteral("run_started");
    case EventKind::FrameStarted: return QStringLiteral("frame_started");
    case EventKind::BlockProgress: return QStringLiteral("block_progress");
    case EventKind::FrameFinished: return QStringLiteral("frame_finished");
    case EventKind::ProductPublished: return QStringLiteral("product_published");
    case EventKind::Issue: return QStringLiteral("issue");
    case EventKind::RunFinished: return QStringLiteral("run_finished");
    case EventKind::Unknown: break;
  }
  return QStringLiteral("unknown");
}

void EventParser::reset() {
  lastSeq_ = -1;
  runId_.clear();
  terminalCount_ = 0;
  fatal_.clear();
  warnings_.clear();
}

AcsdEvent EventParser::parseLine(const QString& line) {
  AcsdEvent ev;
  const QString trimmed = line.trimmed();
  if (trimmed.isEmpty()) {
    ev.error = QStringLiteral("空事件行（已忽略）");
    return ev;
  }

  QJsonParseError perr{};
  const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &perr);
  if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
    ev.error = QStringLiteral("事件行不是合法 JSON 对象：%1").arg(perr.errorString());
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  const QJsonObject obj = doc.object();

  // 1) protocol —— major 不兼容是致命问题。
  const QJsonValue protocolValue = obj.value(QStringLiteral("protocol"));
  if (!protocolValue.isString()) {
    ev.error = QStringLiteral("事件缺少 protocol 字段");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  const QString protocol = protocolValue.toString();
  const auto major = protocolMajorOf(protocol);
  if (!major.has_value()) {
    ev.error = QStringLiteral("未知协议标识：%1").arg(protocol);
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  if (protocol != kEventProtocol) {
    ev.error = QStringLiteral("事件协议 major 不兼容：%1（GUI 要求 %2）")
                   .arg(protocol)
                   .arg(kEventProtocol);
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }

  // 2) run_id —— 一次 run 内唯一。
  const QJsonValue runIdValue = obj.value(QStringLiteral("run_id"));
  if (!runIdValue.isString() || runIdValue.toString().isEmpty()) {
    ev.error = QStringLiteral("事件缺少 run_id");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  const QString runId = runIdValue.toString();
  if (!runId_.isEmpty() && runId != runId_) {
    ev.error = QStringLiteral("同一流内 run_id 变化：%1 → %2").arg(runId_, runId);
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  runId_ = runId;

  // 3) seq —— 从 0 严格递增。
  qint64 seq = 0;
  if (!isInteger(obj.value(QStringLiteral("seq")), &seq) || seq < 0) {
    ev.error = QStringLiteral("事件 seq 非法");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  if (seq <= lastSeq_) {
    ev.error = QStringLiteral("事件 seq 非严格递增：%1 ≤ %2").arg(seq).arg(lastSeq_);
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  if (lastSeq_ < 0 && seq != 0) {
    warnings_.append(QStringLiteral("首个事件 seq=%1，规范要求从 0 开始").arg(seq));
  } else if (seq != lastSeq_ + 1) {
    warnings_.append(QStringLiteral("seq 跳号：%1 → %2").arg(lastSeq_).arg(seq));
  }
  lastSeq_ = static_cast<int>(seq);
  ev.seq = static_cast<int>(seq);

  // 4) event —— 枚举内的种类。
  const QJsonValue eventValue = obj.value(QStringLiteral("event"));
  if (!eventValue.isString()) {
    ev.error = QStringLiteral("事件缺少 event 字段");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  bool kindKnown = false;
  const EventKind kind = kindFromName(eventValue.toString(), &kindKnown);
  if (!kindKnown) {
    ev.error = QStringLiteral("未知事件类型：%1").arg(eventValue.toString());
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  ev.kind = kind;
  ev.kindName = eventKindName(kind);
  ev.runId = runId;

  // 5) 其余必填字段：code / assessment / completed / total / message。
  const QJsonValue codeValue = obj.value(QStringLiteral("code"));
  if (!codeValue.isString()) {
    ev.error = QStringLiteral("事件缺少 code 字段");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  ev.code = codeValue.toString();

  const QJsonValue assessmentValue = obj.value(QStringLiteral("assessment"));
  if (assessmentValue.isNull()) {
    ev.hasAssessment = false;
  } else if (assessmentValue.isObject()) {
    ev.hasAssessment = true;
    ev.assessment = assessmentValue.toObject();
  } else {
    ev.error = QStringLiteral("assessment 必须是对象或 null");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }

  qint64 completed = 0;
  qint64 total = 0;
  if (!isInteger(obj.value(QStringLiteral("completed")), &completed) ||
      !isInteger(obj.value(QStringLiteral("total")), &total) || completed < 0 || total < 0) {
    ev.error = QStringLiteral("事件 completed/total 非法");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  ev.completed = completed;
  ev.total = total;

  const QJsonValue messageValue = obj.value(QStringLiteral("message"));
  if (!messageValue.isString()) {
    ev.error = QStringLiteral("事件缺少 message 字段");
    ev.fatal = true;
    if (fatal_.isEmpty()) fatal_ = ev.error;
    return ev;
  }
  ev.message = messageValue.toString();

  // 6) 可选字段。
  if (obj.contains(QStringLiteral("frame_id"))) {
    const QJsonValue frameValue = obj.value(QStringLiteral("frame_id"));
    if (frameValue.isString()) {
      ev.hasFrameId = true;
      ev.frameId = frameValue.toString();
    } else if (!frameValue.isNull()) {
      ev.error = QStringLiteral("frame_id 必须是字符串或 null");
      ev.fatal = true;
      if (fatal_.isEmpty()) fatal_ = ev.error;
      return ev;
    }
  }
  if (obj.contains(QStringLiteral("block"))) {
    const QJsonValue blockValue = obj.value(QStringLiteral("block"));
    if (blockValue.isNull()) {
      ev.block.reset();
    } else {
      qint64 block = 0;
      if (!isInteger(blockValue, &block) || block < 0) {
        ev.error = QStringLiteral("block 非法");
        ev.fatal = true;
        if (fatal_.isEmpty()) fatal_ = ev.error;
        return ev;
      }
      ev.block = block;
    }
  }

  // 7) run_finished 必须带 exit_code 与 summary，且每 run 恰一个。
  if (kind == EventKind::RunFinished) {
    if (terminalCount_ > 0) {
      ev.error = QStringLiteral("同一 run 出现第二个 run_finished");
      ev.fatal = true;
      if (fatal_.isEmpty()) fatal_ = ev.error;
      return ev;
    }
    qint64 code = 0;
    if (!isInteger(obj.value(QStringLiteral("exit_code")), &code)) {
      ev.error = QStringLiteral("run_finished 缺少 exit_code");
      ev.fatal = true;
      if (fatal_.isEmpty()) fatal_ = ev.error;
      return ev;
    }
    if (code != 0 && code != 2 && code != 3 && code != 4 && code != 5 && code != 6) {
      ev.error = QStringLiteral("run_finished exit_code 非法：%1").arg(code);
      ev.fatal = true;
      if (fatal_.isEmpty()) fatal_ = ev.error;
      return ev;
    }
    ev.exitCode = static_cast<int>(code);
    const QJsonValue summaryValue = obj.value(QStringLiteral("summary"));
    if (!summaryValue.isObject()) {
      ev.error = QStringLiteral("run_finished 缺少 summary 对象");
      ev.fatal = true;
      if (fatal_.isEmpty()) fatal_ = ev.error;
      return ev;
    }
    ev.hasSummary = true;
    ev.summary = summaryValue.toObject();
    ++terminalCount_;
  }

  ev.valid = true;
  return ev;
}

QByteArray cancelControlLine(const QString& runId) {
  QByteArray line;
  line += "{\"protocol\":\"";
  line += kControlProtocol.toUtf8();
  line += "\",\"command\":\"cancel\",\"run_id\":\"";
  line += jsonEscape(runId);
  line += "\"}\n";
  return line;
}

CliVersion parseVersionPayload(const QByteArray& payload) {
  CliVersion out;
  QJsonParseError perr{};
  const QJsonDocument doc = QJsonDocument::fromJson(payload, &perr);
  if (perr.error != QJsonParseError::NoError) {
    out.detail = QStringLiteral("版本输出不是合法 JSON：%1").arg(perr.errorString());
    return out;
  }
  if (!doc.isObject()) {
    out.detail = QStringLiteral("版本输出不是 JSON 对象");
    return out;
  }
  const QJsonObject obj = doc.object();
  collectVersion(obj, &out);
  if (obj.contains(QStringLiteral("version")) && obj.value(QStringLiteral("version")).isString()) {
    out.productVersion = obj.value(QStringLiteral("version")).toString();
  }
  if (!out.known) {
    out.detail = QStringLiteral("版本输出中没有 acsd.*/N 协议标识或 major 字段");
  } else {
    QStringList parts;
    if (out.eventMajor.has_value()) parts.append(QStringLiteral("events/%1").arg(*out.eventMajor));
    if (out.controlMajor.has_value()) parts.append(QStringLiteral("control/%1").arg(*out.controlMajor));
    if (out.configMajor.has_value()) parts.append(QStringLiteral("config/%1").arg(*out.configMajor));
    out.detail = parts.join(QStringLiteral(", "));
  }
  return out;
}

ConfigVersion parseConfigSchema(const QJsonObject& configObject) {
  ConfigVersion out;
  const QJsonValue schemaValue = configObject.value(QStringLiteral("schema"));
  if (!schemaValue.isString()) {
    out.detail = QStringLiteral("配置缺少 schema 字段");
    return out;
  }
  const QString schema = schemaValue.toString();
  const auto major = protocolMajorOf(schema);
  if (!major.has_value()) {
    out.detail = QStringLiteral("配置 schema 标识非法：%1").arg(schema);
    return out;
  }
  out.major = *major;
  out.ok = true;
  out.detail = schema;
  return out;
}

}  // namespace gui
}  // namespace acsd
