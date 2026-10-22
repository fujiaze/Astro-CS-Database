// ACSD v2 GUI —— 无窗口协议检查器
//
// 复用 GUI 的同一份协议/运行代码（acsd_gui_core），对假 CLI 回放的 JSONL 做检查：
//   * 事件是否严格按 seq 递增解析；
//   * 是否恰好收到一个 run_finished；
//   * 取消控制消息是否按 acsd.control/2 发出；
//   * 缺终事件时是否被判失败（不推断成功）。
//
// 用法：
//   acsd-protocol-check --cli <exe> --config <config.json> --mode normal|cancel|crash|terminate
//                        [--out <file.json>] [--cancel-delay-ms N]
#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

#include "config.h"
#include "protocol.h"
#include "runner.h"

namespace {

QTextStream& out() {
  static QTextStream stream(stdout);
  return stream;
}

QString normalizedLine(const acsd::gui::AcsdEvent& event) {
  QString block = event.block.has_value() ? QString::number(*event.block) : QStringLiteral("-");
  QString frame = event.hasFrameId ? event.frameId : QStringLiteral("-");
  return QStringLiteral("%1|%2|%3|%4/%5|%6|%7")
      .arg(event.seq)
      .arg(event.kindName)
      .arg(event.code)
      .arg(event.completed)
      .arg(event.total)
      .arg(frame, block, event.message);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("acsd-protocol-check"));

  QCommandLineParser parser;
  parser.addHelpOption();
  QCommandLineOption cliOption(QStringList{QStringLiteral("cli")}, QStringLiteral("CLI 可执行文件"),
                                QStringLiteral("path"));
  QCommandLineOption configOption(QStringList{QStringLiteral("config")}, QStringLiteral("配置文件"),
                                   QStringLiteral("path"));
  QCommandLineOption modeOption(QStringList{QStringLiteral("mode")}, QStringLiteral("场景"),
                                 QStringLiteral("name"), QStringLiteral("normal"));
  QCommandLineOption outOption(QStringList{QStringLiteral("out")}, QStringLiteral("证据 JSON"),
                                QStringLiteral("path"));
  QCommandLineOption cancelDelayOption(QStringList{QStringLiteral("cancel-delay-ms")},
                                        QStringLiteral("取消延迟"), QStringLiteral("ms"),
                                        QStringLiteral("300"));
  parser.addOption(cliOption);
  parser.addOption(configOption);
  parser.addOption(modeOption);
  parser.addOption(outOption);
  parser.addOption(cancelDelayOption);
  parser.process(app);

  const QString cliPath = parser.value(cliOption);
  const QString configPath = parser.value(configOption);
  const QString mode = parser.value(modeOption);
  const int cancelDelay = parser.value(cancelDelayOption).toInt();
  if (cliPath.isEmpty() || configPath.isEmpty()) {
    out() << "需要 --cli 与 --config\n";
    out().flush();
    return 2;
  }

  acsd::gui::RunController runner;
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert(QStringLiteral("FAKE_ACSD_MODE"), mode);
  runner.setProcessEnvironment(environment);

  QString error;
  if (!runner.start(cliPath, QStringLiteral("normalize"), configPath, QString(), &error)) {
    out() << "启动失败：" << error << "\n";
    out().flush();
    return 2;
  }

  QEventLoop loop;
  QTimer::singleShot(cancelDelay, &loop, [&]() {
    if (mode == QStringLiteral("cancel") || mode == QStringLiteral("terminate")) {
      runner.requestCancel();
    }
    if (mode == QStringLiteral("terminate")) {
      QTimer::singleShot(300, &loop, [&]() { runner.forceTerminate(); });
    }
  });
  QTimer::singleShot(20000, &loop, &QEventLoop::quit);
  QObject::connect(&runner, &acsd::gui::RunController::runFinished, &loop, [&]() { loop.quit(); });
  loop.exec();

  const QVector<acsd::gui::AcsdEvent>& events = runner.events();
  bool terminal = false;
  int terminalCount = 0;
  int previousSeq = -1;
  bool seqStrictlyIncreasing = true;
  for (const acsd::gui::AcsdEvent& event : events) {
    out() << normalizedLine(event) << "\n";
    if (!event.valid) {
      out() << "非法事件：" << event.error << "\n";
    }
    if (event.seq <= previousSeq) {
      seqStrictlyIncreasing = false;
    }
    previousSeq = event.seq;
    if (event.kind == acsd::gui::EventKind::RunFinished) {
      ++terminalCount;
      terminal = true;
    }
  }
  out() << "state=" << acsd::gui::runStateName(runner.state())
        << " exit_code=" << (runner.exitCode().has_value() ? *runner.exitCode() : -1)
        << " cancel_sent=" << (runner.cancelSent() ? "yes" : "no")
        << " cancel_acked=" << (runner.cancelAcknowledged() ? "yes" : "no")
        << " terminal_count=" << terminalCount
        << " seq_increasing=" << (seqStrictlyIncreasing ? "yes" : "no")
        << " events=" << events.size() << "\n";
  if (!runner.fatalReason().isEmpty()) {
    out() << "fatal=" << runner.fatalReason() << "\n";
  }
  for (const QString& warning : runner.warnings()) {
    out() << "warning=" << warning << "\n";
  }
  out().flush();

  QJsonObject report;
  report.insert(QStringLiteral("mode"), mode);
  report.insert(QStringLiteral("state"), acsd::gui::runStateName(runner.state()));
  report.insert(QStringLiteral("state_code"), static_cast<int>(runner.state()));
  if (runner.exitCode().has_value()) {
    report.insert(QStringLiteral("exit_code"), *runner.exitCode());
  }
  report.insert(QStringLiteral("cancel_sent"), runner.cancelSent());
  report.insert(QStringLiteral("cancel_acknowledged"), runner.cancelAcknowledged());
  report.insert(QStringLiteral("terminal_count"), terminalCount);
  report.insert(QStringLiteral("seq_strictly_increasing"), seqStrictlyIncreasing);
  report.insert(QStringLiteral("fatal_reason"), runner.fatalReason());
  report.insert(QStringLiteral("summary"), runner.summary());
  QJsonArray lines;
  for (const acsd::gui::AcsdEvent& event : events) {
    lines.append(normalizedLine(event));
  }
  report.insert(QStringLiteral("stream"), lines);
  if (!parser.value(outOption).isEmpty()) {
    QFile file(parser.value(outOption));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      file.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
      file.close();
    }
  }
  return 0;
}
