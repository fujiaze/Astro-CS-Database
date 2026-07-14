// main_window.cpp - HEALPix 浏览器主窗口实现 (healpix_browser_qt app)
// 功能: 实现 QMainWindow 容器, 菜单栏 + 文件选择 + STF 控制面板 + 状态栏
// 用途: demo exe 入口容器, 按扩展名路由 .hiss/.hcsd → SphereView (统一球面渲染)
// 依赖: Qt6::Widgets, widgets/ (AbstractView/SphereView), app/ (STFPanel)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §4.1, §8, §9

#include "main_window.h"
#include "stf_panel.h"
#include "abstract_view.h"
// #include "single_frame_view.h"  // 已废弃，.hiss 改用 SphereView
#include "sphere_view.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>

// ============================================================================
// 构造 / 析构
// ============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      backend_(std::make_unique<BrowserBackend>()),
      current_view_(nullptr),
      stf_panel_(nullptr),
      status_file_(nullptr),
      status_view_(nullptr),
      status_mouse_(nullptr) {

    setWindowTitle("HEALPix Browser (Qt)");
    resize(1280, 800);

    setup_menu();
    setup_status_bar();
    setup_stf_panel();

    // 占位中心 widget (无文件时显示提示)
    auto* placeholder = new QLabel("尚未打开文件\n\nFile > Open 选择 .hiss 或 .hcsd 文件", this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: #888; font-size: 16px;");
    setCentralWidget(placeholder);
}

MainWindow::~MainWindow() {
    close_file();
}

// ============================================================================
// UI 设置
// ============================================================================

void MainWindow::setup_menu() {
    // ---- File 菜单 ----
    QMenu* file_menu = menuBar()->addMenu("&File");

    QAction* open_action = new QAction("&Open...", this);
    open_action->setShortcut(QKeySequence::Open);
    connect(open_action, &QAction::triggered, this, &MainWindow::on_file_open);
    file_menu->addAction(open_action);

    QAction* close_action = new QAction("&Close", this);
    close_action->setShortcut(QKeySequence::Close);
    connect(close_action, &QAction::triggered, this, &MainWindow::on_file_close);
    file_menu->addAction(close_action);

    file_menu->addSeparator();

    QAction* exit_action = new QAction("E&xit", this);
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &MainWindow::on_exit);
    file_menu->addAction(exit_action);

    // ---- View 菜单 ----
    QMenu* view_menu = menuBar()->addMenu("&View");

    QAction* reset_action = new QAction("&Reset View", this);
    reset_action->setShortcut(QKeySequence("F5"));
    connect(reset_action, &QAction::triggered, this, &MainWindow::on_view_reset);
    view_menu->addAction(reset_action);

    // ---- STF 菜单 ----
    QMenu* stf_menu = menuBar()->addMenu("&STF");

    QAction* auto_action = new QAction("&Auto Stretch", this);
    auto_action->setShortcut(QKeySequence("Ctrl+A"));
    connect(auto_action, &QAction::triggered, this, &MainWindow::on_auto_stretch_clicked);
    stf_menu->addAction(auto_action);
}

void MainWindow::setup_status_bar() {
    status_file_ = new QLabel("文件: (未打开)", this);
    status_view_ = new QLabel("视角: -", this);
    status_mouse_ = new QLabel("鼠标: -", this);

    statusBar()->addWidget(status_file_, 2);
    statusBar()->addWidget(status_view_, 2);
    statusBar()->addPermanentWidget(status_mouse_, 2);
}

void MainWindow::setup_stf_panel() {
    stf_panel_ = new STFPanel(this);
    addDockWidget(Qt::RightDockWidgetArea, stf_panel_);

    connect(stf_panel_, &STFPanel::stf_changed,
            this, &MainWindow::on_stf_changed);
    connect(stf_panel_, &STFPanel::auto_stretch_requested,
            this, &MainWindow::on_auto_stretch_clicked);
}

// ============================================================================
// 文件操作
// ============================================================================

void MainWindow::on_file_open() {
    // 文件过滤器
    QString filter = "HEALPix 文件 (*.hiss *.hcsd);;单帧存储 (*.hiss);;天球数据库 (*.hcsd);;所有文件 (*.*)";
    QString path = QFileDialog::getOpenFileName(this, "打开 HEALPix 文件",
                                                 QString(), filter);
    if (path.isEmpty()) {
        return;
    }
    open_file(path);
}

void MainWindow::open_file_from_cli(const QString& path) {
    open_file(path);
}

void MainWindow::open_file(const QString& path) {
    if (path.isEmpty()) {
        return;
    }

    // 关闭旧文件
    close_file();

    // 打开新文件
    if (backend_->open_file(path.toStdString()) != 0) {
        QMessageBox::critical(this, "错误",
                              QString("无法打开文件:\n%1").arg(path));
        return;
    }

    // 按扩展名路由 (实际上 backend 已根据 Magic 判断 hiss/hcsd)
    // .hiss 和 .hcsd 都用 SphereView（球面渲染），区别在 render_mode
    AbstractView* view = nullptr;
    if (backend_->is_hiss()) {
        view = new SphereView(this);
        static_cast<SphereView*>(view)->set_render_mode(RenderMode::HISS_POLYGON);
        // 注: set_initial_view_from_bbox 需在 renderer 初始化后调用，
        //     这里先设置标志，首次 paintGL 后由 auto_stretch 触发渲染
        //     bbox 在 build_hiss_polygon_mesh 时计算，首次 render 时生效
        static_cast<SphereView*>(view)->set_initial_view_from_bbox();
    } else if (backend_->is_hcsd()) {
        view = new SphereView(this);
        static_cast<SphereView*>(view)->set_render_mode(RenderMode::SPHERE);
        static_cast<SphereView*>(view)->reset_view();
    } else {
        QMessageBox::warning(this, "警告", "不支持的文件格式");
        backend_->close_file();
        return;
    }

    // 绑定数据源
    view->set_backend(backend_.get());
    view->auto_stretch();  // 打开后自动拉伸

    // 同步数据动态范围到 STFPanel (滑块映射用)
    float dmin = 0.0f, dmax = 1.0f;
    view->get_data_range(dmin, dmax);
    stf_panel_->set_data_range(dmin, dmax);

    set_view(view);

    // 更新状态栏
    QString file_name = QFileInfo(path).fileName();
    QString filter_name = QString::fromStdString(backend_->get_filter());
    QString file_info = QString("文件: %1").arg(file_name);
    if (!filter_name.isEmpty()) {
        file_info += QString("  [%1]").arg(filter_name);
    }
    file_info += QString("  nside=%1").arg(backend_->get_nside());
    status_file_->setText(file_info);
}

void MainWindow::on_file_close() {
    close_file();
    status_file_->setText("文件: (未打开)");
    status_view_->setText("视角: -");
    status_mouse_->setText("鼠标: -");

    // 显示占位 widget
    auto* placeholder = new QLabel("尚未打开文件\n\nFile > Open 选择 .hiss 或 .hcsd 文件", this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color: #888; font-size: 16px;");
    setCentralWidget(placeholder);
}

void MainWindow::close_file() {
    disconnect_view();
    if (backend_) {
        backend_->close_file();
    }
}

void MainWindow::on_exit() {
    close();
}

// ============================================================================
// View 操作
// ============================================================================

void MainWindow::set_view(AbstractView* view) {
    if (!view) return;

    // 先断开旧 view
    disconnect_view();

    // 连接新 view 信号槽
    connect(view, &AbstractView::view_changed,
            this, &MainWindow::on_view_changed);
    connect(view, &AbstractView::mouse_moved,
            this, &MainWindow::on_mouse_moved);
    connect(view, &AbstractView::stf_changed,
            this, [this](const STFParams& params) {
                // view 内部 STF 变化 → 同步到控制面板 (避免回环, stf_panel 用 blockSignals)
                stf_panel_->blockSignals(true);
                stf_panel_->update_from_params(params);
                stf_panel_->blockSignals(false);
            });

    current_view_ = view;
    setCentralWidget(view);
    view->setFocus();
}

void MainWindow::disconnect_view() {
    if (current_view_) {
        // centralWidget 所有权归 QMainWindow, setCentralWidget(nullptr) 会删除旧 widget
        takeCentralWidget();  // 移除并持有所有权
        delete current_view_;
        current_view_ = nullptr;
    }
}

void MainWindow::on_view_reset() {
    if (!current_view_) return;
    // 按类型调用 reset
    if (auto* v = qobject_cast<SphereView*>(current_view_)) {
        // 根据 backend 类型调用不同的 reset
        if (backend_ && backend_->is_hiss()) {
            v->set_initial_view_from_bbox();
        } else {
            v->reset_view();
        }
    }
}

// ============================================================================
// STF 控制
// ============================================================================

void MainWindow::on_stf_changed(const STFParams& params) {
    // 来自 STFPanel 的滑块变化 → 转发给 view
    if (current_view_) {
        current_view_->set_stf_params(params);
    }
}

void MainWindow::on_auto_stretch_clicked() {
    if (current_view_) {
        current_view_->auto_stretch();
    }
}

// ============================================================================
// 状态栏更新
// ============================================================================

void MainWindow::on_view_changed(double ra, double dec, double zoom) {
    status_view_->setText(QString("视角: RA=%1° Dec=%2° zoom=%3x")
                          .arg(ra, 0, 'f', 1)
                          .arg(dec, 0, 'f', 1, '+')
                          .arg(zoom, 0, 'f', 2));
}

void MainWindow::on_mouse_moved(double ra, double dec) {
    status_mouse_->setText(QString("鼠标: RA=%1° Dec=%2°")
                           .arg(ra, 0, 'f', 2)
                           .arg(dec, 0, 'f', 2, '+'));
}
