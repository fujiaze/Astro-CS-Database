// ACSD v2 GUI —— 子进程运行器（acsd normalize|mosaic|export --json config.json）
//
// 进程边界：只启动 CLI 子进程，读取 stdout 的 JSONL 事件流，写 stdin 的取消控制消息。
// 不解析内部 C++ 对象，不做科学运算。
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringConverter>
#include <QVector>
#include <optional>

#include "protocol.h"

namespace acsd {
namespace gui {

enum class RunState {
  Idle,
  Running,
  Cancelling,
  Succeeded,
  Partial,
  InvalidInput,
  Unsupported,
  Failed,
  Cancelled,
  Aborted,
  ProtocolError
};

QString runStateName(RunState state);
QString runStateDescription(RunState state);

// 版本探测结果。
struct ProbeResult {
  bool executed = false;
  int exitCode = -1;
  QByteArray stdoutBytes;
  CliVersion version;
  QString error;
};

// 同步探测 `acsd --version --json`。
ProbeResult probeCliVersion(const QString& cliPath, const QString& workingDir, int timeoutMs = 5000);

// 读取 `acsd template <command>` 的配置模板（静态模板，不构造科学数据）。
// 成功时返回解析后的配置对象，失败时返回空对象并给出原因。
QJsonObject fetchCliTemplate(const QString& cliPath, const QString& command, const QString& workingDir,
                             QString* error);

class RunController : public QObject {
  Q_OBJECT

 public:
  explicit RunController(QObject* parent = nullptr);
  ~RunController() override;

  // 启动一次运行：`<cli> <command> --json <configPath>`。
  bool start(const QString& cliPath, const QString& command, const QString& configPath,
             const QString& workingDir, QString* error);

  // 取消：写 {"protocol":"acsd.control/2","command":"cancel","run_id":"..."}\n。
  // 若尚未收到任何事件（未知 run_id），取消请求排队，收到首个事件后立即发出。
  bool requestCancel();

  // 用户主动强制终止：杀掉子进程并把本次运行标记为 aborted。
  void forceTerminate();

  // 供冒烟使用：杀掉子进程但**不**标记为用户中止（模拟外部崩溃、系统 OOM kill）。
  // 这种情况下缺少终事件，运行状态必须是「失败」，不能推断成功。
  void simulateExternalKill();

  void setProcessEnvironment(const QProcessEnvironment& environment);
  QProcessEnvironment processEnvironment() const;

  RunState state() const { return state_; }
  const QVector<AcsdEvent>& events() const { return events_; }
  const QString& runId() const { return runId_; }
  bool cancelSent() const { return cancelSent_; }
  bool cancelPending() const { return cancelPending_; }
  bool cancelAcknowledged() const { return cancelAcknowledged_; }
  QString lastError() const { return lastError_; }
  QString fatalReason() const { return fatal_; }
  std::optional<int> exitCode() const { return exitCode_; }
  int processExitCode() const { return processExitCode_; }
  QJsonObject summary() const { return summary_; }
  QString cliPath() const { return cliPath_; }
  QString command() const { return command_; }
  QString configPath() const { return configPath_; }
  qint64 elapsedMs() const { return timer_.isValid() ? timer_.elapsed() : 0; }

  // 供 GUI 显示的告警（seq 跳号等）。
  QStringList warnings() const { return warnings_; }

 signals:
  void eventReceived(const AcsdEvent& event);
  void stateChanged(RunState state);
  void runFinished(RunState state);
  void logMessage(const QString& message);

 private:
  void setState(RunState state);
  void handleLine(const QString& line);
  QProcess* activeProcess() const { return process_; }
  void attachProcess(QProcess* process);
  void writeCancelControl();
  void finalize(RunState state, const QString& reason);
  void recordWarning(const QString& text);

 private slots:
  void onReadyRead();
  void onProcessError(QProcess::ProcessError error);
  void onFinished(int exitCode, QProcess::ExitStatus status);

 private:
  // 每次运行一个独立的 QProcess：复用同一个对象会把上一轮进程的退出
  // 混进下一轮的终事件判定（实测会吞掉新运行的 runFinished）。
  QProcess* process_ = nullptr;
  QProcessEnvironment environment_;  // 每次运行创建子进程时套用
  QStringDecoder decoder_{QStringConverter::Utf8};
  EventParser parser_;
  QVector<AcsdEvent> events_;
  QStringList warnings_;
  QElapsedTimer timer_;
  QString pending_;  // 跨 chunk 的半行事件文本

  RunState state_ = RunState::Idle;
  QString runId_;
  QString fatal_;
  QString lastError_;
  QString cliPath_;
  QString command_;
  QString configPath_;
  std::optional<int> exitCode_;
  QJsonObject summary_;

  bool cancelSent_ = false;
  bool cancelPending_ = false;
  bool cancelAcknowledged_ = false;
  bool userAborted_ = false;
  bool terminalSeen_ = false;
  bool finishedEmitted_ = false;
  int processExitCode_ = -1;
};

}  // namespace gui
}  // namespace acsd
