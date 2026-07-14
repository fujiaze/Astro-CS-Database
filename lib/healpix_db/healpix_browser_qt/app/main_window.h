// main_window.h - HEALPix 浏览器主窗口 (healpix_browser_qt app)
// 功能: QMainWindow 容器, 菜单栏 + 文件选择 + STF 控制面板 + 状态栏
// 用途: demo exe 入口容器, 按扩展名路由 .hiss → SingleFrameView, .hcsd → SphereView
// 依赖: Qt6::Widgets (QMainWindow/QFileDialog/QMessageBox/QStatusBar/QMenuBar)
//       widgets/ (AbstractView/SingleFrameView/SphereView), core/ (BrowserBackend)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.1, §8, §9

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <memory>
#include "healpix_browser_core.h"

class AbstractView;
class STFPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    // 命令行参数打开文件 (由 main.cpp 触发)
    void open_file_from_cli(const QString& path);

private slots:
    void on_file_open();           // File > Open 菜单
    void on_file_close();          // File > Close
    void on_exit();                // File > Exit
    void on_view_reset();          // View > Reset
    void on_stf_changed(const STFParams& params);
    void on_view_changed(double center_ra, double center_dec, double zoom);
    void on_mouse_moved(double ra, double dec);
    void on_auto_stretch_clicked();

private:
    void setup_menu();
    void setup_status_bar();
    void setup_stf_panel();
    void open_file(const QString& path);
    void close_file();
    // 切换当前 view (单帧/球面), 同时只能显示一个
    void set_view(AbstractView* view);
    // 断开旧 view 的信号槽, 清理资源
    void disconnect_view();

    // 数据源 (app 拥有所有权)
    std::unique_ptr<BrowserBackend> backend_;

    // 当前 view (单帧或球面, 同时只显示一个; 所有权归本类)
    AbstractView* current_view_;

    // STF 控制面板 (DockWidget, 所有权归 QMainWindow)
    STFPanel* stf_panel_;

    // 状态栏标签
    QLabel* status_file_;
    QLabel* status_view_;
    QLabel* status_mouse_;
};

#endif // MAIN_WINDOW_H
