// ACSD v2 GUI —— 入口
//
// 独立进程，只经子进程协议工作：
//   acsd normalize|mosaic|export --json config.json
//   stdout: JSONL 事件流（acsd.events/2）
//   stdin : 取消控制消息（acsd.control/2）
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QJsonObject>
#include <QStatusBar>

#include "mainwindow.h"
#include "smoke.h"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("acsd-gui"));
  QCoreApplication::setApplicationVersion(QStringLiteral("2.0.0"));
  QCoreApplication::setOrganizationName(QStringLiteral("ACSD"));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("ACSD v2 控制台：只消费 CLI 配置、JSONL 事件与产品目录，不做科学运算"));
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption smokeOption(QStringList{QStringLiteral("smoke")},
                                 QStringLiteral("离屏冒烟模式（自检协议，不进入交互界面）"));
  QCommandLineOption configOption(QStringList{QStringLiteral("c"), QStringLiteral("config")},
                                   QStringLiteral("acsd.config/2 配置文件"), QStringLiteral("path"));
  QCommandLineOption cliOption(QStringList{QStringLiteral("x"), QStringLiteral("cli")},
                                QStringLiteral("acsd 可执行文件"), QStringLiteral("path"));
  QCommandLineOption evidenceOption(QStringList{QStringLiteral("e"), QStringLiteral("evidence")},
                                    QStringLiteral("冒烟证据 JSON 输出"), QStringLiteral("path"));
  QCommandLineOption scenarioOption(QStringList{QStringLiteral("s"), QStringLiteral("scenario")},
                                     QStringLiteral("冒烟场景（逗号分隔）"), QStringLiteral("list"),
                                     QStringLiteral("normal,cancel,crash,terminate"));
  QCommandLineOption cancelDelayOption(QStringList{QStringLiteral("cancel-delay-ms")},
                                        QStringLiteral("发出取消的延迟（毫秒）"), QStringLiteral("ms"),
                                        QStringLiteral("400"));
  parser.addOption(smokeOption);
  parser.addOption(configOption);
  parser.addOption(cliOption);
  parser.addOption(evidenceOption);
  parser.addOption(scenarioOption);
  parser.addOption(cancelDelayOption);
  parser.process(app);

  if (parser.isSet(smokeOption)) {
    acsd::gui::SmokeOptions options;
    options.configPath = parser.value(configOption);
    options.cliPath = parser.value(cliOption);
    options.evidencePath = parser.value(evidenceOption);
    options.cancelDelayMs = parser.value(cancelDelayOption).toInt();
    if (options.configPath.isEmpty() || options.cliPath.isEmpty()) {
      qWarning("--smoke 需要 --config 与 --cli");
      return 2;
    }
    const QStringList scenarios = parser.value(scenarioOption).split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (!scenarios.isEmpty()) {
      options.scenarios = scenarios;
    }
    QJsonObject context;
    context.insert(QStringLiteral("application_name"), QCoreApplication::applicationName());
    context.insert(QStringLiteral("application_version"), QCoreApplication::applicationVersion());
    context.insert(QStringLiteral("qt_version"), QString::fromLatin1(qVersion()));
    return acsd::gui::runSmoke(options, context);
  }

  acsd::gui::MainWindow window;
  window.show();
  if (parser.isSet(cliOption)) {
    QString error;
    if (!window.setCliPath(parser.value(cliOption), &error)) {
      window.statusBar()->showMessage(error);
    }
  }
  if (parser.isSet(configOption)) {
    QString error;
    if (!window.loadConfigFile(parser.value(configOption), &error)) {
      window.statusBar()->showMessage(error);
    }
  }
  return app.exec();
}
