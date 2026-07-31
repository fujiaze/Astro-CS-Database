// main_window.h - HEALPix 浏览器主窗口 (healpix_browser_qt app)
// 功能: QMainWindow 容器, 菜单栏 + 文件选择 + STF 控制面板 + 状态栏
// 用途: demo exe 入口容器, 按扩展名路由 .hiss/.hcsd → SphereView (统一球面渲染)
// 依赖: Qt6::Widgets (QMainWindow/QFileDialog/QMessageBox/QStatusBar/QMenuBar)
//       widgets/ (AbstractView/SphereView), core/ (BrowserBackend)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.1, §8, §9

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <memory>
#include "healpix_browser_core.h"

class AbstractView;
class STFPanel;
class QAction;
class QDockWidget;
class QListWidget;
class QComboBox;
class QLineEdit;
class QPushButton;
class QImage;

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
    void on_zoom_in();             // 工具栏 放大
    void on_zoom_out();            // 工具栏 缩小
    void on_stf_changed(const STFParams& params);
    void on_view_changed(double center_ra, double center_dec, double zoom);
    void on_mouse_moved(double ra, double dec);
    void on_auto_stretch_clicked();
    void on_grid_toggle(bool checked);  // View > 经纬线网格 checkbox

    // WP-H 步骤14: HISS Tile 浏览
    void on_tile_selected(int row);          // Tile 列表选中
    void on_tile_layer_changed(int index);   // signal/support/SNR 切换
    void on_query_pixel_clicked();           // ra/dec 查询像素值

private:
    void setup_menu();
    void setup_toolbar();
    void setup_status_bar();
    void setup_stf_panel();
    void setup_hiss_tile_panel();  // WP-H: HISS Tile 浏览面板
    void open_file(const QString& path);
    void close_file();
    // 切换当前 view (单帧/球面), 同时只能显示一个
    void set_view(AbstractView* view);
    // 断开旧 view 的信号槽, 清理资源
    void disconnect_view();

    // WP-H: 渲染当前 Tile 为 QImage (signal/support 两种图层)
    void render_current_tile();

    // 辅助: 判断 backend 是否有打开的文件
    bool file_path_empty() const;

    // 数据源 (app 拥有所有权)
    std::unique_ptr<BrowserBackend> backend_;

    // 当前 view (单帧或球面, 同时只显示一个; 所有权归本类)
    AbstractView* current_view_;

    // STF 控制面板 (DockWidget, 所有权归 QMainWindow)
    STFPanel* stf_panel_;

    // View 菜单 actions
    QAction* grid_toggle_action_ = nullptr;  // 经纬线网格开关

    // 状态栏标签
    QLabel* status_file_;
    QLabel* status_view_;
    QLabel* status_mouse_;

    // ---- WP-H 步骤14: HISS Tile 浏览 UI ----
    QDockWidget* hiss_tile_dock_ = nullptr;     // Tile 浏览面板 (右侧 Dock)
    QListWidget* tile_list_widget_ = nullptr;    // Tile 目录列表
    QComboBox*   tile_layer_combo_ = nullptr;    // 图层选择 (signal/support/SNR)
    QLabel*      tile_render_label_ = nullptr;   // 2D 渲染区域
    QLineEdit*   ra_input_ = nullptr;            // ra 输入框
    QLineEdit*   dec_input_ = nullptr;           // dec 输入框
    QPushButton* query_pixel_btn_ = nullptr;     // 查询按钮
    QLabel*      pixel_value_label_ = nullptr;   // 查询结果显示

    // 当前选中 Tile 的数据 (缓存, 切换图层时复用)
    uint64_t current_tile_parent_ipix_ = 0;
    float*   current_tile_signal_ = nullptr;     // malloc (由 backend 分配)
    uint8_t* current_tile_support_ = nullptr;    // malloc
    uint32_t current_tile_n_signal_ = 0;
};

#endif // MAIN_WINDOW_H
