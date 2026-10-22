// ACSD v2 GUI —— 离屏冒烟模式
//
// 用法：acsd-gui --smoke --config <config.json> --cli <exe> --evidence <out.json>
// 在 QT_QPA_PLATFORM=offscreen 下真实启动窗口、加载配置、连接 CLI、收 run_finished、发取消。
#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace acsd {
namespace gui {

struct SmokeOptions {
  QString configPath;
  QString cliPath;
  QString evidencePath;
  QStringList scenarios{QStringLiteral("normal"), QStringLiteral("cancel"), QStringLiteral("crash"),
                        QStringLiteral("terminate")};
  int cancelDelayMs = 400;
  int timeoutMs = 20000;
};

// 执行冒烟；返回 0 表示全部场景符合预期。证据始终写入 evidencePath。
int runSmoke(const SmokeOptions& options, const QJsonObject& applicationContext);

}  // namespace gui
}  // namespace acsd
