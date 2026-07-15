// main.cpp - HEALPix 浏览器 demo exe 入口 (healpix_browser_qt app)
// 功能: QApplication 入口, 创建 MainWindow, 解析命令行参数
// 用途: 启动 demo 程序, 可选命令行参数直接打开文件
// 依赖: Qt6::Widgets (QApplication/QCommandLineParser), app/ (MainWindow)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.3
// 用法: healpix_browser_qt.exe [file.hiss|file.hcsd]
// 部署: windeployqt 部署 Qt6 DLL 和 plugins 到 exe 同级目录, 双击即可启动

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>
#include <cstdlib>
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
        "可选: 直接打开的文件路径 (.hiss/.hcsd)");
    parser.process(app);

    // 创建主窗口
    MainWindow window;
    window.show();

    // 命令行参数指定文件时, 延迟到事件循环启动后打开
    // (此时 MainWindow 已 show, OpenGL 上下文创建就绪)
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        QMetaObject::invokeMethod(&window, "open_file_from_cli",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, args.first()));
    }

    int ret = app.exec();

    // 程序退出时将内存日志缓冲写入文件 (若设置 BROWSER_LOG_FILE 环境变量)
    const char* log_path = std::getenv("BROWSER_LOG_FILE");
    if (log_path) {
        browser_log::flush_to_file(log_path);
    }

    return ret;
}
