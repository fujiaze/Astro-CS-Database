// ACSD v2 GUI —— 协议层（只消费 CLI 配置 / JSONL 事件 / 控制协议）
//
// 规范来源：docs/engineering/rebuild/API.md「CLI 配置、事件与 GUI」节、
//           schemas/events.schema.json、schemas/config.schema.json。
// 本文件不解析任何内部 C++ 对象，不做科学运算，不依赖旧 ABI。
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

namespace acsd {
namespace gui {

// ---- 协议常量 -------------------------------------------------------------
inline constexpr int kConfigMajor = 2;
inline constexpr int kEventMajor = 2;
inline constexpr int kControlMajor = 2;

QString configSchemaString();   // "acsd.config/2"
QString eventProtocolString();   // "acsd.events/2"
QString controlProtocolString(); // "acsd.control/2"

// ---- 事件 -----------------------------------------------------------------
enum class EventKind {
  RunStarted,
  FrameStarted,
  BlockProgress,
  FrameFinished,
  ProductPublished,
  Issue,
  RunFinished,
  Unknown
};

QString eventKindName(EventKind kind);

struct AcsdEvent {
  bool valid = false;   // 语法与必填字段是否合法
  bool fatal = false;   // 协议违规：必须把本次 run 判为失败，不得推断成功
  QString error;        // 违规原因
  int seq = -1;
  EventKind kind = EventKind::Unknown;
  QString kindName;
  QString runId;
  bool hasFrameId = false;
  QString frameId;
  std::optional<qint64> block;
  QString code;
  bool hasAssessment = false;
  QJsonObject assessment;
  qint64 completed = 0;
  qint64 total = 0;
  QString message;
  std::optional<int> exitCode;  // 仅 run_finished
  bool hasSummary = false;
  QJsonObject summary;          // 仅 run_finished
  bool isTerminal() const { return kind == EventKind::RunFinished; }
};

// 逐行事件解析器：强制 protocol major、seq 严格递增、run_id 唯一、
// 每 run 恰一个 run_finished（含 exit_code 与 summary）。
class EventParser {
 public:
  void reset();

  // 解析一行 stdout（不含换行）。空行忽略；结构性违规返回 fatal=true 的事件。
  AcsdEvent parseLine(const QString& line);

  bool sawTerminal() const { return terminalCount_ > 0; }
  int terminalCount() const { return terminalCount_; }
  QString fatalReason() const { return fatal_; }
  QString runId() const { return runId_; }
  QStringList warnings() const { return warnings_; }

 private:
  int lastSeq_ = -1;
  QString runId_;
  int terminalCount_ = 0;
  QString fatal_;
  QStringList warnings_;
};

// ---- 控制协议 -------------------------------------------------------------
// 取消消息严格按 API.md 的字面形态：
//   {"protocol":"acsd.control/2","command":"cancel","run_id":"..."}\n
QByteArray cancelControlLine(const QString& runId);

// ---- 版本探测 -------------------------------------------------------------
struct CliVersion {
  bool known = false;
  std::optional<int> eventMajor;
  std::optional<int> controlMajor;
  std::optional<int> configMajor;
  QString productVersion;
  QString detail;
};

// 宽松解析 `acsd --version --json` 的输出：接受 "acsd.events/2" 形式的协议串
// 或数值 major 字段。解析不出 major 时 known=false（调用方据事件流再校验）。
CliVersion parseVersionPayload(const QByteArray& payload);

struct ConfigVersion {
  bool ok = false;
  int major = -1;
  QString detail;
};

// 从配置对象的 schema 字段（"acsd.config/2"）取 major。
ConfigVersion parseConfigSchema(const QJsonObject& configObject);

}  // namespace gui
}  // namespace acsd
