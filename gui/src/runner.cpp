#include "runner.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

#include <cmath>

namespace acsd {
namespace gui {
namespace {

bool isExitCodeAllowed(int code) {
  return code == 0 || code == 2 || code == 3 || code == 4 || code == 5 || code == 6;
}

QStringList partialKeys() {
  return {QStringLiteral("failed"), QStringLiteral("failures"), QStringLiteral("unsolved"),
          QStringLiteral("cancelled"),   QStringLiteral("canceled"),  QStringLiteral("unpublished"),
          QStringLiteral("incomplete"),  QStringLiteral("partial"),   QStringLiteral("errors"),
          QStringLiteral("error_count"), QStringLiteral("unsupported")};
}

// run_summary 里若存在非零的 failed/unsolved/cancelled 等计数，则不能只当 exit0 成功。
bool summaryIndicatesPartial(const QJsonValue& value, QString* reason) {
  if (value.isObject()) {
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
      const QString key = it.key().toLower();
      const QJsonValue child = it.value();
      if (child.isBool() && key == QStringLiteral("complete") && !child.toBool()) {
        if (reason != nullptr) {
          *reason = QStringLiteral("summary.complete=false");
        }
        return true;
      }
      if (child.isDouble() && partialKeys().contains(key)) {
        const double number = child.toDouble();
        if (std::fabs(number) > 0.0) {
          if (reason != nullptr) {
            *reason = QStringLiteral("summary.%1=%2").arg(it.key()).arg(number);
          }
          return true;
        }
      }
      if (summaryIndicatesPartial(child, reason)) {
        return true;
      }
    }
  } else if (value.isArray()) {
    for (const QJsonValue& child : value.toArray()) {
      if (summaryIndicatesPartial(child, reason)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

QString runStateName(RunState state) {
  switch (state) {
    case RunState::Idle: return QStringLiteral("空闲");
    case RunState::Running: return QStringLiteral("运行中");
    case RunState::Cancelling: return QStringLiteral("取消中");
    case RunState::Succeeded: return QStringLiteral("成功");
    case RunState::Partial: return QStringLiteral("部分成功");
    case RunState::InvalidInput: return QStringLiteral("输入无效");
    case RunState::Unsupported: return QStringLiteral("能力不支持");
    case RunState::Failed: return QStringLiteral("失败");
    case RunState::Cancelled: return QStringLiteral("已取消");
    case RunState::Aborted: return QStringLiteral("已中止");
    case RunState::ProtocolError: return QStringLiteral("协议错误");
  }
  return QStringLiteral("未知");
}

QString runStateDescription(RunState state) {
  switch (state) {
    case RunState::Idle: return QStringLiteral("没有正在进行的运行");
    case RunState::Running: return QStringLiteral("子进程运行中，事件流已连接");
    case RunState::Cancelling: return QStringLiteral("已发出取消请求，等待子进程在安全边界给出 run_finished");
    case RunState::Succeeded: return QStringLiteral("收到 run_finished 且 exit_code=0");
    case RunState::Partial: return QStringLiteral("收到 run_finished，但仅部分目标达成");
    case RunState::InvalidInput: return QStringLiteral("run_finished exit_code=2：配置或输入无效");
    case RunState::Unsupported: return QStringLiteral("run_finished exit_code=3：能力不支持或模型不可用");
    case RunState::Failed: return QStringLiteral("运行失败");
    case RunState::Cancelled: return QStringLiteral("run_finished exit_code=5：已取消");
    case RunState::Aborted: return QStringLiteral("用户主动终止，未收到终止事件，按中止处理");
    case RunState::ProtocolError: return QStringLiteral("事件协议违规，不能推断成功");
  }
  return QString();
}

ProbeResult probeCliVersion(const QString& cliPath, const QString& workingDir, int timeoutMs) {
  ProbeResult result;
  if (cliPath.trimmed().isEmpty()) {
    result.error = QStringLiteral("未设置 CLI 可执行文件路径");
    return result;
  }
  QProcess process;
  if (!workingDir.trimmed().isEmpty()) {
    process.setWorkingDirectory(workingDir);
  }
  process.start(cliPath, {QStringLiteral("--version"), QStringLiteral("--json")});
  if (!process.waitForStarted(timeoutMs)) {
    result.error = QStringLiteral("无法启动 CLI 做版本探测：%1").arg(process.errorString());
    return result;
  }
  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished(1000);
    result.error = QStringLiteral("版本探测超时");
    return result;
  }
  result.executed = true;
  result.exitCode = process.exitCode();
  result.stdoutBytes = process.readAllStandardOutput();
  result.version = parseVersionPayload(result.stdoutBytes);
  if (result.version.detail.isEmpty()) {
    result.version.detail = QStringLiteral("无版本信息");
  }
  if (result.exitCode != 0 && !result.version.known) {
    result.error = QStringLiteral("CLI 版本探测返回 %1，未解析出版本").arg(result.exitCode);
  }
  return result;
}

QJsonObject fetchCliTemplate(const QString& cliPath, const QString& command, const QString& workingDir,
                             QString* error) {
  if (cliPath.trimmed().isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("未设置 CLI 可执行文件路径");
    }
    return QJsonObject();
  }
  QProcess process;
  if (!workingDir.trimmed().isEmpty()) {
    process.setWorkingDirectory(workingDir);
  }
  process.start(cliPath, {QStringLiteral("template"), command});
  if (!process.waitForStarted(5000)) {
    if (error != nullptr) {
      *error = QStringLiteral("无法启动 CLI 读取模板：%1").arg(process.errorString());
    }
    return QJsonObject();
  }
  if (!process.waitForFinished(5000)) {
    process.kill();
    process.waitForFinished(1000);
    if (error != nullptr) {
      *error = QStringLiteral("读取 CLI 模板超时");
    }
    return QJsonObject();
  }
  const QByteArray payload = process.readAllStandardOutput();
  if (process.exitCode() != 0) {
    if (error != nullptr) {
      *error = QStringLiteral("CLI 模板命令返回 %1：%2")
                   .arg(process.exitCode())
                   .arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
    }
    return QJsonObject();
  }
  QJsonParseError perr{};
  const QJsonDocument document = QJsonDocument::fromJson(payload, &perr);
  if (perr.error != QJsonParseError::NoError || !document.isObject()) {
    if (error != nullptr) {
      *error = QStringLiteral("CLI 模板不是合法 JSON 对象：%1").arg(perr.errorString());
    }
    return QJsonObject();
  }
  return document.object();
}

RunController::RunController(QObject* parent) : QObject(parent) {}

RunController::~RunController() {
  // 析构时若子进程仍在运行，必须先结束它，否则 QProcess 析构会崩。
  if (process_ != nullptr) {
    if (process_->state() != QProcess::NotRunning) {
      process_->kill();
      process_->waitForFinished(2000);
    }
    delete process_;
    process_ = nullptr;
  }
}

void RunController::setProcessEnvironment(const QProcessEnvironment& environment) {
  environment_ = environment;
  if (process_ != nullptr) {
    process_->setProcessEnvironment(environment);
  }
}

QProcessEnvironment RunController::processEnvironment() const {
  return environment_;
}

bool RunController::start(const QString& cliPath, const QString& command, const QString& configPath,
                          const QString& workingDir, QString* error) {
  if (state_ == RunState::Running || state_ == RunState::Cancelling) {
    if (error != nullptr) {
      *error = QStringLiteral("已有运行在进行中");
    }
    return false;
  }
  if (cliPath.trimmed().isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("未设置 CLI 可执行文件路径");
    }
    return false;
  }
  if (configPath.trimmed().isEmpty()) {
    if (error != nullptr) {
      *error = QStringLiteral("未设置配置文件路径");
    }
    return false;
  }

  events_.clear();
  warnings_.clear();
  parser_.reset();
  decoder_.resetState();
  pending_.clear();
  runId_.clear();
  fatal_.clear();
  lastError_.clear();
  exitCode_.reset();
  summary_ = QJsonObject();
  cancelSent_ = false;
  cancelPending_ = false;
  cancelAcknowledged_ = false;
  userAborted_ = false;
  terminalSeen_ = false;
  finishedEmitted_ = false;
  processExitCode_ = -1;
  cliPath_ = cliPath;
  command_ = command;
  configPath_ = configPath;

  // 上一次运行的子进程可能还在退出；先断开它的信号再等它结束，
  // 否则它的退出会被当成本轮的终事件（会吞掉本轮的 runFinished）。
  if (process_ != nullptr) {
    process_->disconnect(this);
    if (process_->state() != QProcess::NotRunning) {
      const bool finished = process_->waitForFinished(5000);
      if (!finished || process_->state() != QProcess::NotRunning) {
        lastError_ = QStringLiteral("上一次的子进程仍未结束（state=%1，waitForFinished=%2），无法启动新运行")
                         .arg(static_cast<int>(process_->state()))
                         .arg(finished ? "true" : "false");
        if (error != nullptr) {
          *error = lastError_;
        }
        setState(RunState::Failed);
        return false;
      }
    }
    delete process_;
    process_ = nullptr;
  }

  QProcess* process = new QProcess(this);
  if (!workingDir.trimmed().isEmpty()) {
    process->setWorkingDirectory(workingDir);
  }
  process->setProcessChannelMode(QProcess::SeparateChannels);
  if (!environment_.isEmpty()) {
    process->setProcessEnvironment(environment_);
  }
  attachProcess(process);

  const QStringList arguments{QStringLiteral("%1").arg(command), QStringLiteral("--json"), configPath};
  process->start(cliPath, arguments);
  if (!process->waitForStarted(5000)) {
    lastError_ = QStringLiteral("无法启动子进程：%1").arg(process->errorString());
    if (error != nullptr) {
      *error = lastError_;
    }
    setState(RunState::Failed);
    return false;
  }
  timer_.start();
  setState(RunState::Running);
  emit logMessage(QStringLiteral("已启动：%1 %2 --json %3").arg(cliPath, command, configPath));
  return true;
}

bool RunController::requestCancel() {
  if (state_ != RunState::Running && state_ != RunState::Cancelling) {
    lastError_ = QStringLiteral("当前没有正在进行的运行，无法取消");
    return false;
  }
  if (cancelSent_) {
    return true;
  }
  if (runId_.isEmpty()) {
    // 取消消息必须带 run_id；尚未收到事件时排队，首个事件到达后立即发出。
    cancelPending_ = true;
    emit logMessage(QStringLiteral("取消请求已排队（等待 run_id）"));
    return true;
  }
  writeCancelControl();
  return true;
}

void RunController::writeCancelControl() {
  if (cancelSent_ || runId_.isEmpty()) {
    return;
  }
  const QByteArray line = cancelControlLine(runId_);
  process_->write(line);
  if (!process_->waitForBytesWritten(1000)) {
    recordWarning(QStringLiteral("取消控制消息未能在 1 秒内写入子进程 stdin"));
  }
  cancelSent_ = true;
  cancelPending_ = false;
  setState(RunState::Cancelling);
  emit logMessage(QStringLiteral("已发送取消控制消息：%1").arg(QString::fromUtf8(line).trimmed()));
}

void RunController::forceTerminate() {
  if (state_ != RunState::Running && state_ != RunState::Cancelling) {
    return;
  }
  userAborted_ = true;
  emit logMessage(QStringLiteral("用户主动终止子进程（标记 aborted）"));
  process_->kill();
}

void RunController::simulateExternalKill() {
  if (state_ != RunState::Running && state_ != RunState::Cancelling) {
    return;
  }
  emit logMessage(QStringLiteral("模拟外部终止子进程（崩溃场景，不标记 aborted），pid=%1")
                      .arg(process_->processId()));
  process_->kill();
}

void RunController::onReadyRead() {
  const QByteArray data = process_->readAllStandardOutput();
  if (data.isEmpty()) {
    return;
  }
  const QString text = decoder_.decode(data);
  if (decoder_.hasError()) {
    const QString reason = QStringLiteral("事件流包含非法 UTF-8 字节");
    if (fatal_.isEmpty()) {
      fatal_ = reason;
    }
    recordWarning(reason);
  }
  pending_ += text;
  int index = pending_.indexOf(QLatin1Char('\n'));
  while (index >= 0) {
    const QString line = pending_.left(index);
    pending_.remove(0, index + 1);
    handleLine(line);
    index = pending_.indexOf(QLatin1Char('\n'));
  }
}

void RunController::handleLine(const QString& line) {
  const AcsdEvent event = parser_.parseLine(line);
  if (!event.valid && !event.fatal) {
    if (!event.error.isEmpty()) {
      recordWarning(event.error);
    }
    return;
  }
  if (event.fatal) {
    if (fatal_.isEmpty()) {
      fatal_ = event.error;
    }
    lastError_ = event.error;
  }
  events_.append(event);
  emit eventReceived(event);

  if (!event.runId.isEmpty() && runId_.isEmpty()) {
    runId_ = event.runId;
    if (cancelPending_) {
      writeCancelControl();
    }
  }
  for (const QString& warning : parser_.warnings()) {
    if (!warnings_.contains(warning)) {
      warnings_.append(warning);
      emit logMessage(QStringLiteral("协议告警：%1").arg(warning));
    }
  }
  if (event.kind == EventKind::RunFinished) {
    terminalSeen_ = true;
    // 终事件之后关闭子进程的写通道：真实 CLI 的控制线程阻塞在读 stdin 上，
    // 若管道一直开着，进程会在退出清理里等到 stdin 关闭才结束（实测挂住）。
    // 关闭写通道让控制线程读到 EOF，子进程即可正常退出；取消通道只在运行期需要。
    if (process_->state() != QProcess::NotRunning) {
      process_->closeWriteChannel();
    }
    exitCode_ = event.exitCode;
    summary_ = event.summary;
    if (event.exitCode.has_value() && *event.exitCode == 5) {
      cancelAcknowledged_ = true;
    }
    finalize(RunState::Succeeded, QString());
  }
}

void RunController::onProcessError(QProcess::ProcessError error) {
  if (error == QProcess::FailedToStart) {
    lastError_ = QStringLiteral("子进程无法启动：%1").arg(process_->errorString());
    finalize(RunState::Failed, lastError_);
  } else if (error == QProcess::Crashed) {
    // 崩溃不是协议违规：缺终事件一律按「失败」处理，不推断成功。
    lastError_ = QStringLiteral("子进程崩溃：%1").arg(process_->errorString());
    finalize(RunState::Failed, lastError_);
  }
}

void RunController::onFinished(int exitCode, QProcess::ExitStatus status) {
  processExitCode_ = exitCode;
  emit logMessage(QStringLiteral("子进程结束：退出码 %1，状态 %2")
                      .arg(exitCode)
                      .arg(status == QProcess::CrashExit ? QStringLiteral("崩溃") : QStringLiteral("正常")));
  // 冲刷解码器与半行缓冲。
  onReadyRead();
  const QString tail = decoder_.decode(QByteArray());
  if (!tail.isEmpty()) {
    pending_ += tail;
  }
  if (decoder_.hasError()) {
    const QString reason = QStringLiteral("事件流结尾存在非法 UTF-8 字节");
    if (fatal_.isEmpty()) {
      fatal_ = reason;
    }
    recordWarning(reason);
  }
  if (!pending_.trimmed().isEmpty()) {
    handleLine(pending_);
    pending_.clear();
  }
  if (userAborted_) {
    finalize(RunState::Aborted, QStringLiteral("用户主动终止子进程"));
    return;
  }
  if (!terminalSeen_) {
    // 意外崩溃可能缺 terminal —— 必须标失败，不能推断成功。
    // 这不是协议违规（CLI 根本没机会发事件），因此不进 fatal_，状态就是「失败」。
    const QString reason = QStringLiteral("子进程已结束但没有收到 run_finished（退出码 %1，可能崩溃）").arg(exitCode);
    if (lastError_.isEmpty()) {
      lastError_ = reason;
    }
    finalize(RunState::Failed, reason);
    return;
  }
  if (status == QProcess::CrashExit || !isExitCodeAllowed(exitCode)) {
    const QString reason =
        QStringLiteral("子进程异常退出码 %1，与 run_finished 不一致").arg(exitCode);
    if (fatal_.isEmpty()) {
      fatal_ = reason;
    }
    finalize(RunState::Failed, reason);
    return;
  }
  if (exitCode_ != std::optional<int>(exitCode)) {
    recordWarning(QStringLiteral("进程退出码 %1 与 run_finished.exit_code %2 不一致；以事件流为准")
                      .arg(exitCode)
                      .arg(exitCode_.value_or(-1)));
  }
  finalize(state_, QString());
}

void RunController::recordWarning(const QString& text) {
  if (text.isEmpty() || warnings_.contains(text)) {
    return;
  }
  warnings_.append(text);
  emit logMessage(QStringLiteral("告警：%1").arg(text));
}

void RunController::finalize(RunState state, const QString& reason) {
  RunState finalState = state;
  if (!fatal_.isEmpty() && finalState != RunState::Aborted) {
    finalState = RunState::ProtocolError;
  }
  if (finalState == RunState::Succeeded && exitCode_.has_value()) {
    switch (*exitCode_) {
      case 0: {
        QString partialReason;
        if (summaryIndicatesPartial(summary_, &partialReason)) {
          finalState = RunState::Partial;
          recordWarning(QStringLiteral("exit_code=0 但 %1，不能只当成功").arg(partialReason));
        }
        break;
      }
      case 2: finalState = RunState::InvalidInput; break;
      case 3: finalState = RunState::Unsupported; break;
      case 4: finalState = RunState::Failed; break;
      case 5: finalState = RunState::Cancelled; break;
      case 6: finalState = RunState::Partial; break;
      default: finalState = RunState::Failed; break;
    }
  }
  if (!reason.isEmpty() && lastError_.isEmpty()) {
    lastError_ = reason;
  }
  if (finalState == RunState::ProtocolError && fatal_.isEmpty()) {
    fatal_ = reason;
  }
  setState(finalState);
  if (!finishedEmitted_) {
    finishedEmitted_ = true;
    emit runFinished(finalState);
  }
}

void RunController::attachProcess(QProcess* process) {
  process_ = process;
  connect(process, &QProcess::readyReadStandardOutput, this, &RunController::onReadyRead);
  connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
    const QByteArray data = process->readAllStandardError();
    if (!data.isEmpty()) {
      emit logMessage(QString::fromUtf8(data).trimmed());
    }
  });
  connect(process, &QProcess::errorOccurred, this, &RunController::onProcessError);
  connect(process, &QProcess::finished, this, &RunController::onFinished);
}

void RunController::setState(RunState state) {
  if (state_ == state) {
    return;
  }
  state_ = state;
  emit stateChanged(state);
}

}  // namespace gui
}  // namespace acsd
