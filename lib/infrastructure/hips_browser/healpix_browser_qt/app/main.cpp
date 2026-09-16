// main.cpp - HEALPix 浏览器 demo exe 入口 (healpix_browser_qt app)
// 功能: QApplication 入口, 创建 MainWindow, 解析命令行参数
// 用途: 启动 demo 程序, 可选命令行参数直接打开文件
// 依赖: Qt6::Widgets (QApplication/QCommandLineParser), app/ (MainWindow)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.3
// 用法: healpix_browser_qt.exe [file.hiss|file.hcsd|hips_dir]
// healpix_browser_qt.exe --hips <dir> [--preset <name>]
// [--layer signal|support] [--screenshot <png>] [--exit]
// 部署: windeployqt 部署 Qt6 DLL 和 plugins 到 exe 同级目录, 双击即可启动

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <QTimer>
#include <cstdlib>
#include <cstdio>
#include "main_window.h"
#include "logger.h"

int main(int argc, char* argv[]) {
    // Qt 部署后, exe 同级目录有 platforms/qwindows.dll, Qt 默认自动查找
    // 无需手动设置插件路径, 双击即可启动

    QApplication app(argc, argv);
    app.setApplicationName("HEALPix Browser");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Astro CS");

    // 命令行参数解析
    QCommandLineParser parser;
    parser.setApplicationDescription("HEALPix 浏览器 (Qt 版)");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file",
        "可选: 直接打开的文件路径 (.hiss/.hcsd) 或 HiPS 产品集目录");
    QCommandLineOption hips_opt("hips", "HiPS 产品集根目录", "dir");
    QCommandLineOption std_hips_opt("standard-hips",
                                    "标准单层 HiPS 根目录（signal-only 兼容）",
                                    "dir");
    QCommandLineOption preset_opt("preset", "预设视图名", "name");
    QCommandLineOption layer_opt("layer", "图层 signal|support", "layer");
    QCommandLineOption shot_opt("screenshot", "保存截图 PNG", "path");
    QCommandLineOption exit_opt("exit", "截图后退出");
    QCommandLineOption view_opt("view", "跳转视图 ra,dec,fov", "ra,dec,fov");
    QCommandLineOption win_shot_opt("window-screenshot",
                                    "整窗截图（含状态栏）", "path");
    QCommandLineOption lod_opt("lod", "LOD 模式 strict-leaf|hierarchy",
                               "mode");
    QCommandLineOption reset_stf_opt(
        "reset-stf", "重新计算 Auto Global robust 显示标尺");
    QCommandLineOption stf_mode_opt(
        "stf-mode", "Auto STF 模式 global|view（默认 global）", "mode");
    QCommandLineOption lock_stf_opt(
        "lock-stf", "锁定当前 STF 标尺（禁止 auto/reset/模式切换重算）");
    parser.addOption(hips_opt);
    parser.addOption(std_hips_opt);
    parser.addOption(preset_opt);
    parser.addOption(layer_opt);
    parser.addOption(shot_opt);
    parser.addOption(exit_opt);
    parser.addOption(view_opt);
    parser.addOption(win_shot_opt);
    parser.addOption(lod_opt);
    parser.addOption(reset_stf_opt);
    parser.addOption(stf_mode_opt);
    parser.addOption(lock_stf_opt);
    parser.process(app);

    // 创建主窗口
    MainWindow window;
    window.show();

    // 命令行参数指定文件时, 延迟到事件循环启动后打开
    // (此时 MainWindow 已 show, OpenGL 上下文创建就绪)
    QString target;
    if (parser.isSet(hips_opt)) {
        target = parser.value(hips_opt);
    } else if (parser.isSet(std_hips_opt)) {
        target = parser.value(std_hips_opt);
    } else {
        const QStringList args = parser.positionalArguments();
        if (!args.isEmpty()) target = args.first();
    }
    const QString preset = parser.value(preset_opt);
    const QString layer = parser.value(layer_opt);
    const QString shot = parser.value(shot_opt);
    const bool exit_after = parser.isSet(exit_opt);
    const QString view = parser.value(view_opt);
    const QString win_shot = parser.value(win_shot_opt);
    const QString lod = parser.value(lod_opt);
    const bool reset_stf = parser.isSet(reset_stf_opt);
    const QString stf_mode = parser.value(stf_mode_opt);
    const bool lock_stf = parser.isSet(lock_stf_opt);

    if (!target.isEmpty()) {
        QMetaObject::invokeMethod(&window, "open_file_from_cli",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, target));
    }
    if (!shot.isEmpty()) {
        // 延迟执行：等 open_file 完成且窗口渲染
        QTimer::singleShot(1200, &window, [&window, shot, preset, layer,
                                           exit_after]() {
            const int lyr = (layer == "support") ? 1 : 0;
            window.capture_hips_screenshot(shot, preset, lyr, exit_after);
        });
    }
    if (lod == "strict-leaf" || lod == "hierarchy") {
        QTimer::singleShot(950, &window, [&window, lod]() {
            window.set_lod_mode(lod);
        });
    }
    if (reset_stf) {
        QTimer::singleShot(950, &window, [&window]() {
            window.reset_auto_stf();
        });
    }
    if (!stf_mode.isEmpty()) {
        QTimer::singleShot(950, &window, [&window, stf_mode]() {
            window.set_auto_stf_mode(stf_mode);
        });
    }
    if (lock_stf) {
        QTimer::singleShot(1000, &window, [&window]() {
            window.set_stf_locked(true);
        });
    }
    if (!view.isEmpty()) {
        double ra = 0, dec = 0, fov = 8.0;
        if (std::sscanf(view.toStdString().c_str(), "%lf,%lf,%lf", &ra, &dec,
                        &fov) == 3) {
            QTimer::singleShot(900, &window, [&window, ra, dec, fov]() {
                window.jump_to_view(ra, dec, fov);
            });
        }
    }
    if (!win_shot.isEmpty()) {
        QTimer::singleShot(1400, &window, [&window, win_shot]() {
            window.capture_window_screenshot(win_shot, true);
        });
    }

    int ret = app.exec();

    // 程序退出时将内存日志缓冲写入文件 (若设置 BROWSER_LOG_FILE 环境变量)
    const char* log_path = std::getenv("BROWSER_LOG_FILE");
    if (log_path) {
        browser_log::flush_to_file(log_path);
    }

    return ret;
}
