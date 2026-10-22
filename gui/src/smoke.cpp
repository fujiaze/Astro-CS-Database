#include "smoke.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include "mainwindow.h"
#include "runner.h"

namespace acsd {
namespace gui {
namespace {

QTextStream& out() {
  static QTextStream stream(stdout);
  return stream;
}

bool hasTerminalEvent(const QJsonArray& events) {
  for (const QJsonValue& value : events) {
    const QJsonObject event = value.toObject();
    if (event.value(QStringLiteral("event")).toString() == QStringLiteral("run_finished")) {
      return true;
    }
  }
  return false;
}

// 每个场景的预期：来自 API.md 的进程协议（终事件缺失必须判失败）。
bool scenarioSatisfied(const QString& scenario, const QJsonObject& snapshot, QString* detail) {
  const QJsonArray events = snapshot.value(QStringLiteral("events")).toArray();
  const QString state = snapshot.value(QStringLiteral("state")).toString();
  const bool terminal = hasTerminalEvent(events);
  if (scenario == QStringLiteral("normal")) {
    if (!terminal) {
      *detail = QStringLiteral("未收到 run_finished");
      return false;
    }
    if (snapshot.value(QStringLiteral("exit_code")).toInt(-1) != 0) {
      *detail = QStringLiteral("exit_code 不是 0");
      return false;
    }
    if (state != QStringLiteral("成功")) {
      *detail = QStringLiteral("状态不是成功：%1").arg(state);
      return false;
    }
    return true;
  }
  if (scenario == QStringLiteral("cancel")) {
    if (!snapshot.value(QStringLiteral("cancel_sent")).toBool()) {
      *detail = QStringLiteral("没有发出取消控制消息");
      return false;
    }
    if (!terminal) {
      *detail = QStringLiteral("取消后没有收到 run_finished");
      return false;
    }
    if (snapshot.value(QStringLiteral("exit_code")).toInt(-1) != 5) {
      *detail = QStringLiteral("取消后的 exit_code 不是 5");
      return false;
    }
    if (state != QStringLiteral("已取消")) {
      *detail = QStringLiteral("状态不是已取消：%1").arg(state);
      return false;
    }
    return true;
  }
  if (scenario == QStringLiteral("crash")) {
    if (terminal) {
      *detail = QStringLiteral("崩溃场景竟然收到 run_finished");
      return false;
    }
    if (state != QStringLiteral("失败")) {
      *detail = QStringLiteral("崩溃场景未标失败，状态=%1").arg(state);
      return false;
    }
    return true;
  }
  if (scenario == QStringLiteral("terminate")) {
    if (state != QStringLiteral("已中止")) {
      *detail = QStringLiteral("主动终止后状态不是已中止：%1").arg(state);
      return false;
    }
    return true;
  }
  *detail = QStringLiteral("未知场景");
  return false;
}

}  // namespace

int runSmoke(const SmokeOptions& options, const QJsonObject& applicationContext) {
  QJsonObject report;
  report.insert(QStringLiteral("mode"), QStringLiteral("offscreen-smoke"));
  report.insert(QStringLiteral("platform"), QString::fromLatin1(qgetenv("QT_QPA_PLATFORM")));
  report.insert(QStringLiteral("cli"), options.cliPath);
  report.insert(QStringLiteral("config"), options.configPath);
  report.insert(QStringLiteral("application"), applicationContext);

  MainWindow window;
  window.setWindowTitle(QStringLiteral("ACSD v2 控制台（冒烟）"));
  window.show();
  QCoreApplication::processEvents();

  QString error;
  if (!window.setCliPath(options.cliPath, &error)) {
    out() << "[smoke] 失败：无法设置 CLI 路径：" << error << "\n";
    out().flush();
    report.insert(QStringLiteral("error"), error);
    return 2;
  }

  // 把配置复制到临时目录，避免污染仓库；运行用配置也写在该目录，
  // 使配置中的相对路径仍按「相对 config 所在目录」解析。
  QTemporaryDir temporaryDir;
  if (!temporaryDir.isValid()) {
    out() << "[smoke] 失败：无法创建临时目录\n";
    out().flush();
    return 2;
  }
  const QString temporaryConfig = temporaryDir.filePath(QStringLiteral("config.json"));
  if (!QFile::copy(options.configPath, temporaryConfig)) {
    QFile source(options.configPath);
    if (!source.open(QIODevice::ReadOnly)) {
      out() << "[smoke] 失败：无法读取配置：" << options.configPath << "\n";
      out().flush();
      return 2;
    }
    QFile target(temporaryConfig);
    target.open(QIODevice::WriteOnly);
    target.write(source.readAll());
    target.close();
  }
  if (!window.loadConfigFile(temporaryConfig, &error)) {
    out() << "[smoke] 失败：加载配置被拒绝：" << error << "\n";
    out().flush();
    report.insert(QStringLiteral("error"), error);
    return 2;
  }
  window.setWorkingDirectory(temporaryDir.path());
  const ProbeResult probe = window.probeVersion();
  out() << "[smoke] 版本探测：executed=" << (probe.executed ? "yes" : "no")
        << " detail=" << probe.version.detail << "\n";
  out().flush();

  QJsonArray scenarioReports;
  bool allOk = true;
  for (const QString& scenario : options.scenarios) {
    window.setProcessEnvironmentVariable(QStringLiteral("FAKE_ACSD_MODE"), scenario);
    if (!window.startRun(&error)) {
      out() << "[smoke] 场景 " << scenario << " 启动失败：" << error << "\n";
      out().flush();
      allOk = false;
      continue;
    }
    QEventLoop loop;
    QTimer::singleShot(options.cancelDelayMs, &loop, [&]() {
      if (scenario == QStringLiteral("cancel") || scenario == QStringLiteral("terminate")) {
        window.cancelRun();
      }
      if (scenario == QStringLiteral("terminate")) {
        QTimer::singleShot(300, &loop, [&]() { window.forceTerminate(); });
      }
    });
    QTimer::singleShot(options.timeoutMs, &loop, &QEventLoop::quit);
    QObject::connect(window.runner(), &RunController::runFinished, &loop, [&]() { loop.quit(); });
    loop.exec();

    const QJsonObject snapshot = window.evidenceSnapshot();
    QString detail;
    const bool ok = scenarioSatisfied(scenario, snapshot, &detail);
    allOk = allOk && ok;
    QJsonObject scenarioReport;
    scenarioReport.insert(QStringLiteral("scenario"), scenario);
    scenarioReport.insert(QStringLiteral("ok"), ok);
    scenarioReport.insert(QStringLiteral("detail"), detail);
    scenarioReport.insert(QStringLiteral("run"), snapshot);
    scenarioReports.append(scenarioReport);
    out() << "[smoke] 场景 " << scenario << "：" << (ok ? "通过" : "不通过") << "（" << detail << "）"
          << " state=" << snapshot.value(QStringLiteral("state")).toString()
          << " exit_code=" << snapshot.value(QStringLiteral("exit_code")).toInt(-1)
          << " cancel_sent=" << snapshot.value(QStringLiteral("cancel_sent")).toBool()
          << " events=" << snapshot.value(QStringLiteral("events")).toArray().size() << "\n";
    out().flush();
  }
  report.insert(QStringLiteral("scenarios"), scenarioReports);
  report.insert(QStringLiteral("all_ok"), allOk);

  // ---- 产品浏览：读一份产品目录里的 manifest.json ----
  {
    const QString productDir = temporaryDir.filePath(QStringLiteral("product"));
    QDir().mkpath(productDir);
    QFile manifest(QDir(productDir).filePath(QStringLiteral("manifest.json")));
    manifest.open(QIODevice::WriteOnly | QIODevice::Truncate);
    manifest.write(QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QStringLiteral("acsd.product/2")},
        {QStringLiteral("major"), 2},
        {QStringLiteral("minor"), 0},
        {QStringLiteral("stage"), QStringLiteral("normalize")},
        {QStringLiteral("product_id"), QStringLiteral("sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")},
        {QStringLiteral("revision"), 1},
        {QStringLiteral("root_object"), QStringLiteral("sha256:fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210")},
        {QStringLiteral("profile"), QStringLiteral("full")},
        {QStringLiteral("capabilities"), QJsonArray{QStringLiteral("signal"), QStringLiteral("noise_unknown")}},
        {QStringLiteral("objects"),
         QJsonArray{QJsonObject{{QStringLiteral("hash"), QStringLiteral("sha256:aa")},
                                {QStringLiteral("path"), QStringLiteral("objects/aa.json")},
                                {QStringLiteral("type"), QStringLiteral("SignalEstimate")},
                                {QStringLiteral("major"), 2},
                                {QStringLiteral("minor"), 0}}}},
        {QStringLiteral("blocks"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("blk-0")},
                                {QStringLiteral("path"), QStringLiteral("blocks/blk-0.f64.zst")},
                                {QStringLiteral("dtype"), QStringLiteral("f64")},
                                {QStringLiteral("shape"), QJsonArray{196608}}}}},
        {QStringLiteral("configuration_hash"), QStringLiteral("sha256:bb")},
        {QStringLiteral("algorithm_set"),
         QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("drizzle")},
                                {QStringLiteral("version"), QStringLiteral("1")},
                                {QStringLiteral("source_hash"), QStringLiteral("sha256:cc")}}}},
        {QStringLiteral("complete"), true},
    }).toJson(QJsonDocument::Indented));
    manifest.close();

    const ProductManifestView view = window.loadProductManifest(productDir);
    QJsonObject productReport;
    productReport.insert(QStringLiteral("ok"), view.ok);
    productReport.insert(QStringLiteral("stage"), view.stage);
    productReport.insert(QStringLiteral("profile"), view.profile);
    productReport.insert(QStringLiteral("complete"), view.complete);
    QJsonArray capabilities;
    for (const QString& capability : view.capabilities) {
      capabilities.append(capability);
    }
    productReport.insert(QStringLiteral("capabilities"), capabilities);
    productReport.insert(QStringLiteral("object_count"), view.objects.size());
    productReport.insert(QStringLiteral("block_count"), view.blocks.size());
    productReport.insert(QStringLiteral("error"), view.error);
    const bool productOk = view.ok && view.stage == QStringLiteral("normalize") &&
                           view.profile == QStringLiteral("full") && view.complete &&
                           view.capabilities.size() == 2 && view.objects.size() == 1 &&
                           view.blocks.size() == 1;
    productReport.insert(QStringLiteral("satisfied"), productOk);
    report.insert(QStringLiteral("product"), productReport);
    allOk = allOk && productOk;
    out() << "[smoke] 产品浏览：" << (productOk ? "通过" : "不通过")
          << " stage=" << view.stage << " profile=" << view.profile
          << " complete=" << (view.complete ? "true" : "false")
          << " capabilities=" << view.capabilities.size()
          << " objects=" << view.objects.size() << " blocks=" << view.blocks.size() << "\n";
    out().flush();
  }

  // ---- 版本探测：不兼容的 config major 必须拒绝启动并给原因 ----
  {
    QFile badConfig(temporaryDir.filePath(QStringLiteral("bad-config.json")));
    badConfig.open(QIODevice::WriteOnly | QIODevice::Truncate);
    badConfig.write(QJsonDocument(QJsonObject{
        {QStringLiteral("schema"), QStringLiteral("acsd.config/3")},
        {QStringLiteral("command"), QStringLiteral("normalize")},
    }).toJson(QJsonDocument::Indented));
    badConfig.close();
    QString reason;
    const bool loaded = window.loadConfigFile(temporaryDir.filePath(QStringLiteral("bad-config.json")), &reason);
    QJsonObject versionReport;
    versionReport.insert(QStringLiteral("loaded"), loaded);
    versionReport.insert(QStringLiteral("reason"), reason);
    const bool versionOk = !loaded && reason.contains(QStringLiteral("不兼容"));
    versionReport.insert(QStringLiteral("satisfied"), versionOk);
    report.insert(QStringLiteral("incompatible_config"), versionReport);
    allOk = allOk && versionOk;
    out() << "[smoke] 不兼容配置拒绝启动：" << (versionOk ? "通过" : "不通过") << "（" << reason << "）\n";
    out().flush();
  }

  // ---- 输入分组 ----
  {
    const int groups = window.inputGroupCount();
    const bool groupsOk = groups >= 2;  // 「全部输入」+ 每个 frame_id 一组
    report.insert(QStringLiteral("input_groups"), groups);
    allOk = allOk && groupsOk;
    out() << "[smoke] 输入分组：" << groups << " 组（" << (groupsOk ? "通过" : "不通过") << "）\n";
    out().flush();
  }

  report.insert(QStringLiteral("all_ok"), allOk);

  if (!options.evidencePath.isEmpty()) {
    QFile evidence(options.evidencePath);
    QDir().mkpath(QFileInfo(options.evidencePath).absolutePath());
    if (evidence.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      evidence.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
      evidence.close();
      out() << "[smoke] 证据已写入：" << options.evidencePath << "\n";
    } else {
      out() << "[smoke] 证据写入失败：" << options.evidencePath << "\n";
      allOk = false;
    }
  }
  out() << "[smoke] 总体结果：" << (allOk ? "全部场景符合预期" : "存在不符合预期的场景") << "\n";
  out().flush();
  return allOk ? 0 : 1;
}

}  // namespace gui
}  // namespace acsd
