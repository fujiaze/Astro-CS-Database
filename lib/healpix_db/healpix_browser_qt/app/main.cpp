// main.cpp - HEALPix 浏览器 demo exe 入口 (healpix_browser_qt app)
// 功能: QApplication 入口, 创建 MainWindow, 解析命令行参数
// 用途: 启动 demo 程序, 可选命令行参数直接打开文件
// 依赖: Qt6::Widgets (QApplication/QCommandLineParser), app/ (MainWindow)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.3
// 用法: healpix_browser_qt.exe [file.hiss|file.hcsd]

#include <QApplication>
#include <QCommandLineParser>
#include <QStringList>
#include "main_window.h"

int main(int argc, char* argv[]) {
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

    return app.exec();
}
