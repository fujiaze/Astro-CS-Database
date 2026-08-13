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
#include "hips_view.h"
#include "hips_browser_backend.h"
#include "logger.h"  // WP-H: LOG_INFO/LOG_WARN

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>
// WP-H 步骤14: HISS Tile 浏览 UI
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QImage>
#include <QPixmap>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QVariant>
#include <QCoreApplication>
#include <cmath>

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
    setup_toolbar();
    setup_status_bar();
    setup_stf_panel();
    setup_hiss_tile_panel();  // WP-H: HISS Tile 浏览面板
    // V9: HiPS 预设下拉框（默认隐藏，打开 HiPS 后启用）
    hips_preset_combo_ = new QComboBox(this);
    hips_preset_combo_->setToolTip("HiPS 预设视图");
    hips_preset_combo_->setVisible(false);
    statusBar()->addPermanentWidget(hips_preset_combo_);
    connect(hips_preset_combo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::on_hips_preset);

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

    QAction* hips_open_action = new QAction("Open &HiPS Directory...", this);
    connect(hips_open_action, &QAction::triggered, this,
            &MainWindow::on_hips_open);
    file_menu->addAction(hips_open_action);

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

    view_menu->addSeparator();

    // 经纬线网格开关 (checkbox)
    grid_toggle_action_ = new QAction("&经纬线网格 (30°)", this);
    grid_toggle_action_->setCheckable(true);
    grid_toggle_action_->setChecked(false);
    grid_toggle_action_->setShortcut(QKeySequence("Ctrl+G"));
    connect(grid_toggle_action_, &QAction::toggled, this, &MainWindow::on_grid_toggle);
    view_menu->addAction(grid_toggle_action_);

    // ---- STF 菜单 ----
    QMenu* stf_menu = menuBar()->addMenu("&STF");

    QAction* auto_action = new QAction("&Auto Stretch", this);
    auto_action->setShortcut(QKeySequence("Ctrl+A"));
    connect(auto_action, &QAction::triggered, this, &MainWindow::on_auto_stretch_clicked);
    stf_menu->addAction(auto_action);
}

void MainWindow::setup_toolbar() {
    QToolBar* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    // 放大按钮
    QAction* zoom_in_action = new QAction("🔍+", this);
    zoom_in_action->setToolTip("放大 (FOV /= 1.2)");
    zoom_in_action->setShortcut(QKeySequence("Ctrl++"));
    connect(zoom_in_action, &QAction::triggered, this, &MainWindow::on_zoom_in);
    toolbar->addAction(zoom_in_action);

    // 缩小按钮
    QAction* zoom_out_action = new QAction("🔍-", this);
    zoom_out_action->setToolTip("缩小 (FOV *= 1.2)");
    zoom_out_action->setShortcut(QKeySequence("Ctrl+-"));
    connect(zoom_out_action, &QAction::triggered, this, &MainWindow::on_zoom_out);
    toolbar->addAction(zoom_out_action);

    toolbar->addSeparator();

    // 重置视角按钮
    QAction* reset_action = new QAction("⟳", this);
    reset_action->setToolTip("重置视角 (F5)");
    reset_action->setShortcut(QKeySequence("F5"));
    connect(reset_action, &QAction::triggered, this, &MainWindow::on_view_reset);
    toolbar->addAction(reset_action);

    // Auto Stretch 按钮
    QAction* auto_stretch_action = new QAction("Auto STF", this);
    auto_stretch_action->setToolTip("自动拉伸 (Ctrl+A)");
    auto_stretch_action->setShortcut(QKeySequence("Ctrl+A"));
    connect(auto_stretch_action, &QAction::triggered, this, &MainWindow::on_auto_stretch_clicked);
    toolbar->addAction(auto_stretch_action);

    // V9: Signal/Support 图层切换（仅 HiPS 模式显示）
    toolbar->addSeparator();
    layer_toggle_action_ = new QAction("Support", this);
    layer_toggle_action_->setCheckable(true);
    layer_toggle_action_->setToolTip("切换 Signal / Support 图层");
    layer_toggle_action_->setVisible(false);
    connect(layer_toggle_action_, &QAction::toggled, this,
            &MainWindow::on_hips_layer_toggle);
    toolbar->addAction(layer_toggle_action_);

    // V9: 拉伸曲线（HiPS 模式）
    stretch_combo_ = new QComboBox(this);
    stretch_combo_->addItems({"Linear", "Sqrt", "Log", "Asinh"});
    stretch_combo_->setCurrentIndex(3);  // 默认 Asinh
    stretch_combo_->setToolTip("显示拉伸曲线 (HiPS)");
    stretch_combo_->setVisible(false);
    connect(stretch_combo_,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::on_hips_stretch_changed);
    toolbar->addWidget(stretch_combo_);
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
// WP-H 步骤14: HISS Tile 浏览面板
// 左侧 Dock: Tile 列表 + 图层选择 + 2D 渲染 + ra/dec 查询
// ============================================================================

void MainWindow::setup_hiss_tile_panel() {
    hiss_tile_dock_ = new QDockWidget("HISS Tile 浏览器", this);
    hiss_tile_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto* container = new QWidget(hiss_tile_dock_);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    // ---- Tile 列表 ----
    auto* tile_group = new QGroupBox("Tile 目录", container);
    auto* tile_layout = new QVBoxLayout(tile_group);
    tile_list_widget_ = new QListWidget(tile_group);
    tile_list_widget_->setMaximumHeight(150);
    tile_layout->addWidget(tile_list_widget_);
    layout->addWidget(tile_group);

    // ---- 图层选择 ----
    auto* layer_group = new QGroupBox("图层", container);
    auto* layer_layout = new QVBoxLayout(layer_group);
    tile_layer_combo_ = new QComboBox(layer_group);
    tile_layer_combo_->addItem("Signal");
    tile_layer_combo_->addItem("Support");
    tile_layer_combo_->addItem("SNR");
    layer_layout->addWidget(tile_layer_combo_);
    layout->addWidget(layer_group);

    // ---- 2D 渲染区域 ----
    auto* render_group = new QGroupBox("Tile 渲染", container);
    auto* render_layout = new QVBoxLayout(render_group);
    tile_render_label_ = new QLabel(render_group);
    tile_render_label_->setAlignment(Qt::AlignCenter);
    tile_render_label_->setMinimumSize(256, 256);
    tile_render_label_->setStyleSheet("background-color: #000; border: 1px solid #444;");
    tile_render_label_->setText("(选择 Tile 查看)");
    render_layout->addWidget(tile_render_label_);
    layout->addWidget(render_group, 1);  // stretch=1 让渲染区域可扩展

    // ---- ra/dec 查询 ----
    auto* query_group = new QGroupBox("像素查询 (ra/dec)", container);
    auto* query_form = new QFormLayout(query_group);
    ra_input_ = new QLineEdit(query_group);
    ra_input_->setPlaceholderText("0~360");
    dec_input_ = new QLineEdit(query_group);
    dec_input_->setPlaceholderText("-90~90");
    query_form->addRow("RA (度):", ra_input_);
    query_form->addRow("Dec (度):", dec_input_);

    query_pixel_btn_ = new QPushButton("查询", query_group);
    query_form->addRow("", query_pixel_btn_);

    pixel_value_label_ = new QLabel("(未查询)", query_group);
    pixel_value_label_->setWordWrap(true);
    query_form->addRow("结果:", pixel_value_label_);
    layout->addWidget(query_group);

    hiss_tile_dock_->setWidget(container);
    addDockWidget(Qt::LeftDockWidgetArea, hiss_tile_dock_);
    hiss_tile_dock_->hide();  // 默认隐藏, 打开 .hiss 文件时显示

    // 信号槽连接
    connect(tile_list_widget_, &QListWidget::currentRowChanged,
            this, &MainWindow::on_tile_selected);
    connect(tile_layer_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_tile_layer_changed);
    connect(query_pixel_btn_, &QPushButton::clicked,
            this, &MainWindow::on_query_pixel_clicked);
}

// WP-H: Tile 列表选中, 读取 signal/support 数据并渲染
void MainWindow::on_tile_selected(int row) {
    if (row < 0 || !backend_ || !backend_->is_hiss_header_loaded()) {
        return;
    }

    // 释放旧 Tile 数据
    if (current_tile_signal_) {
        // current_tile_signal_ 是 malloc 分配的, 用 free 释放
        std::free(current_tile_signal_);
        current_tile_signal_ = nullptr;
    }
    if (current_tile_signal_f64_) {
        std::free(current_tile_signal_f64_);
        current_tile_signal_f64_ = nullptr;
    }
    if (current_tile_support_) {
        std::free(current_tile_support_);
        current_tile_support_ = nullptr;
    }
    current_tile_n_signal_ = 0;
    current_tile_parent_ipix_ = 0;

    // 从 Tile 列表获取 parent_ipix (存在 item 的 data 中)
    QListWidgetItem* item = tile_list_widget_->item(row);
    if (!item) return;
    uint64_t parent_ipix = item->data(Qt::UserRole).toULongLong();
    current_tile_parent_ipix_ = parent_ipix;

    // 读取 signal (按文件精度模式选择 FP32/FP64, R11 HISS-105)
    HissTileData tile;
    if (backend_->is_fp64()) {
        if (backend_->read_tile_signal_f64(parent_ipix, tile) == 0) {
            current_tile_signal_f64_ = tile.signal_f64;
            current_tile_n_signal_ = tile.n_signal;
        }
    } else {
        if (backend_->read_tile_signal(parent_ipix, tile) == 0) {
            current_tile_signal_ = tile.signal;
            current_tile_n_signal_ = tile.n_signal;
        }
    }
    // 读取 support (可选)
    HissTileData tile_sup;
    if (backend_->read_tile_support(parent_ipix, tile_sup) == 0) {
        current_tile_support_ = tile_sup.support;
    }

    render_current_tile();

    QString info = QString("Tile %1: %2 像素")
                   .arg((qulonglong)parent_ipix)
                   .arg(current_tile_n_signal_);
    status_view_->setText(info);
}

// WP-H: 切换图层 (signal/support/SNR), 重新渲染
void MainWindow::on_tile_layer_changed(int index) {
    (void)index;
    render_current_tile();
}

// WP-H: 查询像素值 (ra/dec)
void MainWindow::on_query_pixel_clicked() {
    if (!backend_ || file_path_empty()) {
        pixel_value_label_->setText("未打开文件");
        return;
    }

    bool ra_ok = false, dec_ok = false;
    double ra = ra_input_->text().toDouble(&ra_ok);
    double dec = dec_input_->text().toDouble(&dec_ok);
    if (!ra_ok || !dec_ok || ra < 0.0 || ra > 360.0 || dec < -90.0 || dec > 90.0) {
        pixel_value_label_->setText("输入无效 (RA: 0~360, Dec: -90~90)");
        return;
    }

    uint8_t support = 0;
    if (backend_->is_fp64()) {
        double signal = 0.0;
        if (backend_->query_pixel_f64(ra, dec, signal, support) == 0) {
            pixel_value_label_->setText(
                QString("signal=%1 (FP64)\nsupport=%2").arg(signal, 0, 'g', 6).arg((int)support));
        } else {
            pixel_value_label_->setText("查询失败 (像素不在数据范围内)");
        }
    } else {
        float signal = 0.0f;
        if (backend_->query_pixel(ra, dec, signal, support) == 0) {
            pixel_value_label_->setText(
                QString("signal=%1 (FP32)\nsupport=%2").arg(signal, 0, 'g', 6).arg((int)support));
        } else {
            pixel_value_label_->setText("查询失败 (像素不在数据范围内)");
        }
    }
}

// WP-H: 渲染当前 Tile 为灰度图
void MainWindow::render_current_tile() {
    if (current_tile_n_signal_ == 0) {
        tile_render_label_->setText("(无数据)");
        return;
    }

    int layer = tile_layer_combo_ ? tile_layer_combo_->currentIndex() : 0;
    // NESTED 排序: Tile 内像素按 2^depth x 2^depth 块排列
    // 简化: 按 sqrt(n) x sqrt(n) 排列 (n_leaf_per_tile 是 4^k, sqrt 是 2^k)
    int n = (int)current_tile_n_signal_;
    int side = (int)std::round(std::sqrt((double)n));
    if (side < 1) side = 1;

    // 计算数据范围 (自动拉伸)
    float dmin = 1e30f, dmax = -1e30f;
    if (layer == 0 && (current_tile_signal_ || current_tile_signal_f64_)) {
        // Signal 图层 (FP32 或 FP64 缓冲)
        for (uint32_t i = 0; i < current_tile_n_signal_; ++i) {
            float v = current_tile_signal_f64_ ? (float)current_tile_signal_f64_[i]
                                               : current_tile_signal_[i];
            if (v < dmin) dmin = v;
            if (v > dmax) dmax = v;
        }
    } else if (layer == 1 && current_tile_support_) {
        // Support 图层 (uint8)
        for (uint32_t i = 0; i < current_tile_n_signal_; ++i) {
            float v = (float)current_tile_support_[i];
            if (v < dmin) dmin = v;
            if (v > dmax) dmax = v;
        }
    } else {
        tile_render_label_->setText("(该图层无数据)");
        return;
    }

    float range = dmax - dmin;
    if (range < 1e-6f) range = 1.0f;

    // 构造 QImage (灰度 8-bit)
    QImage img(side, side, QImage::Format_Grayscale8);
    img.fill(0);

    for (int i = 0; i < side; ++i) {
        for (int j = 0; j < side; ++j) {
            int idx = i * side + j;
            if (idx >= n) break;
            float v = 0.0f;
            if (layer == 0 && (current_tile_signal_ || current_tile_signal_f64_)) {
                v = current_tile_signal_f64_ ? (float)current_tile_signal_f64_[idx]
                                             : current_tile_signal_[idx];
            } else if (layer == 1 && current_tile_support_) {
                v = (float)current_tile_support_[idx];
            }
            float norm = (v - dmin) / range;
            if (norm < 0.0f) norm = 0.0f;
            if (norm > 1.0f) norm = 1.0f;
            img.setPixel(j, i, (uint)(norm * 255.0f + 0.5f));
        }
    }

    // 放大显示 (最近邻插值, 保持像素清晰)
    int display_size = 256;
    QPixmap pm = QPixmap::fromImage(img).scaled(display_size, display_size,
                                                 Qt::KeepAspectRatio,
                                                 Qt::FastTransformation);
    tile_render_label_->setPixmap(pm);

    QString layer_name = (layer == 0) ? "Signal" : (layer == 1) ? "Support" : "SNR";
    QString info = QString("%1\n%2x%2 (n=%3)\n范围: %4~%5")
                   .arg(layer_name).arg(side).arg(n)
                   .arg(dmin, 0, 'g', 4).arg(dmax, 0, 'g', 4);
    tile_render_label_->setToolTip(info);
}

// 辅助: 判断 backend 是否有打开的文件
bool MainWindow::file_path_empty() const {
    return backend_->get_file_path().empty();
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

    // V9: HiPS 产品集目录（内含 signal/support）→ HiPS 2D 视图
    QFileInfo fi(path);
    if (fi.isDir() &&
        (QDir(path).exists("signal") || QDir(path).exists("properties"))) {
        open_hips(path);
        return;
    }

    // 打开新文件
    if (backend_->open_file(path.toStdString()) != 0) {
        QMessageBox::critical(this, "错误",
                              QString("无法打开文件:\n%1").arg(path));
        return;
    }

    // 按扩展名路由 (实际上 backend 已根据 Magic 判断 hiss/hcsd)
    // .hiss 和 .hcsd 都用 SphereView 球面渲染, 视角相关加载 (LOD 金字塔)
    AbstractView* view = nullptr;
    if (backend_->is_hiss()) {
        view = new SphereView(this);
        // .hiss 也用 SPHERE 模式 (视角相关渲染 + LOD 金字塔)
        // 不再用 HISS_POLYGON 全量多边形模式 (大数据集会卡死)
        static_cast<SphereView*>(view)->set_render_mode(RenderMode::SPHERE);

        // 从 backend 获取数据 bbox, 设置初始视角到数据位置
        double bbox_ra, bbox_dec, bbox_w, bbox_h;
        if (backend_->get_data_bbox(bbox_ra, bbox_dec, bbox_w, bbox_h) == 0 &&
            bbox_w > 0.0 && bbox_h > 0.0) {
            static_cast<SphereView*>(view)->set_initial_view_from_data(
                bbox_ra, bbox_dec, bbox_w, bbox_h);
        } else {
            static_cast<SphereView*>(view)->reset_view();
        }
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

    // WP-H 步骤14: 如果是 .hiss 文件, 加载 Tile 目录并显示 Tile 浏览面板
    if (backend_->is_hiss()) {
        HissHeader header;
        if (backend_->load_hiss(path.toStdString(), header) == 0) {
            // 填充 Tile 列表
            tile_list_widget_->clear();
            for (uint64_t i = 0; i < header.n_tiles; ++i) {
                uint64_t parent_ipix = header.tile_ipix_list.empty() ? i :
                                        header.tile_ipix_list[i];
                QString label = QString("Tile %1: parent=%2")
                                .arg(i).arg((qulonglong)parent_ipix);
                auto* item = new QListWidgetItem(label);
                item->setData(Qt::UserRole, (qulonglong)parent_ipix);
                tile_list_widget_->addItem(item);
            }
            // 显示 Tile 浏览面板
            if (hiss_tile_dock_) hiss_tile_dock_->show();
            LOG_INFO("HISS Tile 目录已加载: %llu 个 Tile",
                     (unsigned long long)header.n_tiles);
        } else {
            LOG_WARN("load_hiss 失败, Tile 浏览面板不可用");
            if (hiss_tile_dock_) hiss_tile_dock_->hide();
        }
    } else {
        // .hcsd 文件, 隐藏 Tile 浏览面板
        if (hiss_tile_dock_) hiss_tile_dock_->hide();
        tile_list_widget_->clear();
    }
}

// ============================================================================
// V9: HiPS 产品集模式
// ============================================================================

void MainWindow::open_hips(const QString& path) {
    hips_backend_ = std::make_unique<HipsBrowserBackend>();
    if (hips_backend_->open_product(path.toStdString()) != 0) {
        QMessageBox::critical(this, "错误",
                              QString("无法打开 HiPS 产品集:\n%1").arg(path));
        hips_backend_.reset();
        return;
    }

    auto* v = new HipsView(this);
    v->set_backend(hips_backend_.get());

    // 初始视角：GC 三 panel 默认整幅；其余默认 equator
    const std::string base = QFileInfo(path).fileName().toStdString();
    if (base.find("gc") != std::string::npos ||
        base.find("3panel") != std::string::npos) {
        v->jump_to(272.5, -17.2, 15.0, 0);
    } else {
        v->jump_to(0.0, 0.0, 60.0, 0);
    }
    v->set_stretch("asinh", true);

    set_hips_view(v);
    hips_mode_ = true;

    status_file_->setText(
        QString("文件: %1  [HiPS order=%2 tiles=%3]")
            .arg(QFileInfo(path).fileName())
            .arg(hips_backend_->get_hips_order())
            .arg((qulonglong)hips_backend_->get_n_tiles()));
    status_view_->setText(QString("视角: RA %.3f° Dec %.3f° FOV %.2f°")
                              .arg(v->sky()->center_ra())
                              .arg(v->sky()->center_dec())
                              .arg(v->sky()->fov()));

    layer_toggle_action_->setVisible(true);
    layer_toggle_action_->setChecked(false);
    if (stretch_combo_) stretch_combo_->setVisible(true);
    if (hiss_tile_dock_) hiss_tile_dock_->hide();
    stf_panel_->set_data_range(0.0f, 1.0f);
    populate_hips_presets();
}

void MainWindow::set_hips_view(HipsView* view) {
    if (!view) return;
    disconnect_view();
    connect(view, &HipsView::viewChanged, this,
            &MainWindow::on_hips_view_changed);
    connect(view, &HipsView::mouseMoved, this,
            &MainWindow::on_hips_mouse_moved);
    connect(view, &HipsView::layerChanged, this,
            &MainWindow::on_hips_layer_changed);
    hips_view_ = view;
    setCentralWidget(view);
    view->setFocus();
}

void MainWindow::populate_hips_presets() {
    if (!hips_preset_combo_) return;
    hips_preset_combo_->blockSignals(true);
    hips_preset_combo_->clear();
    const std::string base =
        hips_backend_ ? QFileInfo(
                            QString::fromStdString(hips_backend_->get_root()))
                            .fileName()
                            .toStdString()
                      : "";
    if (base.find("gc") != std::string::npos ||
        base.find("3panel") != std::string::npos) {
        struct P { const char* n; double ra, dec, fov; int layer; };
        const P presets[] = {
            {"GC Wide", 272.5, -17.2, 15.0, 0},
            {"Overlap 1-2", 272.5, -15.66, 3.0, 0},
            {"Overlap 2-3", 272.5, -20.72, 3.0, 0},
            {"Seam Close-up", 272.5, -15.66, 2.0, 0},
            {"Support View", 272.5, -17.2, 15.0, 1},
        };
        for (const auto& p : presets) {
            hips_preset_combo_->addItem(p.n, QVariant::fromValue(
                QPointF(QPointF(p.ra, p.dec))));
            // 用 item data 存 (fov, layer) 的编码：fov*100 + layer
            hips_preset_combo_->setItemData(
                hips_preset_combo_->count() - 1,
                QVariant(p.fov * 100.0 + p.layer),
                Qt::UserRole + 1);
        }
    } else if (base.find("truth") != std::string::npos) {
        const char* names[] = {"Equator", "Wrap 0/360", "Polar",
                               "Multi-face"};
        const double ra[] = {45.0, 0.0, 0.0, 45.0};
        const double dec[] = {0.0, 0.0, 88.0, 0.0};
        const double fov[] = {30.0, 45.0, 25.0, 60.0};
        for (int i = 0; i < 4; ++i) {
            hips_preset_combo_->addItem(
                names[i],
                QVariant::fromValue(QPointF(ra[i], dec[i])));
            hips_preset_combo_->setItemData(
                hips_preset_combo_->count() - 1,
                QVariant(fov[i] * 100.0), Qt::UserRole + 1);
        }
    } else {
        hips_preset_combo_->addItem("Sky (equator)", QVariant());
        hips_preset_combo_->setItemData(0, QVariant(6000.0),
                                        Qt::UserRole + 1);
    }
    hips_preset_combo_->setVisible(true);
    hips_preset_combo_->blockSignals(false);
}

void MainWindow::on_hips_open() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, "选择 HiPS 产品集目录（含 signal/support）");
    if (!dir.isEmpty()) open_file(dir);
}

void MainWindow::on_hips_preset(int index) {
    if (!hips_view_ || index < 0) return;
    const QVariant posv = hips_preset_combo_->itemData(index);
    const QVariant ov = hips_preset_combo_->itemData(index, Qt::UserRole + 1);
    if (!posv.canConvert<QPointF>() || !ov.isValid()) return;
    const QPointF p = posv.value<QPointF>();
    const double code = ov.toDouble();
    const double fov = code / 100.0;
    const int layer = (int)std::round(code - fov * 100.0);
    std::fprintf(stderr, "[preset] idx=%d ra=%.2f dec=%.2f fov=%.2f layer=%d\n",
                 index, p.x(), p.y(), fov, layer);
    hips_view_->jump_to(p.x(), p.y(), fov, layer);
    layer_toggle_action_->setChecked(layer == 1);
}

void MainWindow::on_hips_layer_toggle(bool checked) {
    if (!hips_view_) return;
    hips_view_->set_layer(checked ? 1 : 0);
}

void MainWindow::on_hips_stretch_changed(int index) {
    if (!hips_view_ || index < 0) return;
    const char* names[] = {"linear", "sqrt", "log", "asinh"};
    hips_view_->set_stretch(names[index], hips_auto_range_);
}

void MainWindow::on_hips_view_changed(double ra, double dec, double fov) {
    status_view_->setText(
        QString("视角: RA %1° Dec %2° FOV %3° order=%4")
            .arg(ra, 0, 'f', 3)
            .arg(dec, 0, 'f', 3)
            .arg(fov, 0, 'f', 2)
            .arg(hips_view_ ? hips_view_->sky()->target_order() : 0));
}

void MainWindow::on_hips_mouse_moved(double ra, double dec) {
    const double ra_h = ra / 15.0;
    const int hh = (int)ra_h;
    const double mm = (ra_h - hh) * 60.0;
    const int mi = (int)mm;
    const double ss = (mm - mi) * 60.0;
    const int dd = (int)std::fabs(dec);
    const double dm = (std::fabs(dec) - dd) * 60.0;
    const int dmi = (int)dm;
    const double dss = (dm - dmi) * 60.0;
    status_mouse_->setText(
        QString("RA %1h%2m%3s  Dec %4%5°%6'%7\"")
            .arg(hh, 2, 10, QChar('0'))
            .arg(mi, 2, 10, QChar('0'))
            .arg(ss, 4, 'f', 1, QChar('0'))
            .arg(dec < 0 ? "-" : "+")
            .arg(dd, 2, 10, QChar('0'))
            .arg(dmi, 2, 10, QChar('0'))
            .arg(dss, 4, 'f', 1, QChar('0')));
}

void MainWindow::on_hips_layer_changed(int layer) {
    layer_toggle_action_->setChecked(layer == 1);
    status_file_->setText(status_file_->text().section("  [", 0, 0) +
                          QString("  [layer=%1]")
                              .arg(layer == 0 ? "signal" : "support"));
}

void MainWindow::capture_hips_screenshot(const QString& out_png,
                                         const QString& preset, int layer,
                                         bool exit_after) {
    std::fprintf(stderr, "[capture] png=%s preset=%s layer=%d hips_view=%p\n",
                 out_png.toStdString().c_str(), preset.toStdString().c_str(),
                 layer, (void*)hips_view_);
    if (!hips_view_) return;
    if (!preset.isEmpty()) {
        const int idx = hips_preset_combo_->findText(preset);
        std::fprintf(stderr, "[capture] findText idx=%d combo_count=%d\n", idx,
                     hips_preset_combo_->count());
        if (idx >= 0) hips_preset_combo_->setCurrentIndex(idx);
    }
    if (layer >= 0) hips_view_->set_layer(layer);
    hips_view_->mark_dirty();
    hips_view_->repaint();
    QCoreApplication::processEvents();
    const bool ok = hips_view_->save_snapshot(out_png);
    LOG_INFO("main_window", "screenshot %s -> %d", out_png.toStdString().c_str(),
             ok ? 1 : 0);
    if (exit_after) QTimer::singleShot(100, this, &MainWindow::on_exit);
}

void MainWindow::jump_to_view(double ra, double dec, double fov) {
    if (hips_view_) {
        hips_view_->jump_to(ra, dec, fov, -1);
        std::fprintf(stderr, "[view] ra=%.3f dec=%.3f fov=%.2f\n", ra, dec, fov);
    }
}

void MainWindow::capture_window_screenshot(const QString& out_png,
                                           bool exit_after) {
    repaint();
    QCoreApplication::processEvents();
    const QPixmap pm = grab();
    const bool ok = pm.save(out_png);
    std::fprintf(stderr, "[window-shot] %s -> %d\n",
                 out_png.toStdString().c_str(), ok ? 1 : 0);
    if (exit_after) QTimer::singleShot(100, this, &MainWindow::on_exit);
}

void MainWindow::set_lod_mode(const QString& mode) {
    if (!hips_view_) return;
    const bool strict = (mode == "strict-leaf");
    hips_view_->set_lod_mode(strict);
    std::fprintf(stderr, "[lod] mode=%s strict=%d\n",
                 mode.toStdString().c_str(), strict ? 1 : 0);
}

void MainWindow::reset_auto_stf() {
    if (!hips_view_) return;
    hips_view_->refresh_auto_range();
    std::fprintf(stderr, "[stf] reset auto-global robust range\n");
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
    // WP-H: 清理 HISS Tile 浏览数据
    if (current_tile_signal_) { std::free(current_tile_signal_); current_tile_signal_ = nullptr; }
    if (current_tile_support_) { std::free(current_tile_support_); current_tile_support_ = nullptr; }
    current_tile_n_signal_ = 0;
    current_tile_parent_ipix_ = 0;
    if (tile_list_widget_) tile_list_widget_->clear();
    if (tile_render_label_) tile_render_label_->setText("(选择 Tile 查看)");
    if (pixel_value_label_) pixel_value_label_->setText("(未查询)");
    if (hiss_tile_dock_) hiss_tile_dock_->hide();
    if (backend_) {
        backend_->close_file();
    }
    if (hips_backend_) {
        hips_backend_->close();
        hips_backend_.reset();
    }
    if (layer_toggle_action_) layer_toggle_action_->setVisible(false);
    if (hips_preset_combo_) hips_preset_combo_->setVisible(false);
    if (stretch_combo_) stretch_combo_->setVisible(false);
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
    if (hips_view_) {
        takeCentralWidget();
        delete hips_view_;
        hips_view_ = nullptr;
        hips_mode_ = false;
        return;
    }
    if (current_view_) {
        // centralWidget 所有权归 QMainWindow, setCentralWidget(nullptr) 会删除旧 widget
        takeCentralWidget();  // 移除并持有所有权
        delete current_view_;
        current_view_ = nullptr;
    }
}

void MainWindow::on_view_reset() {
    if (hips_view_) {
        const std::string base =
            hips_backend_
                ? QFileInfo(QString::fromStdString(hips_backend_->get_root()))
                      .fileName()
                      .toStdString()
                : "";
        if (base.find("gc") != std::string::npos ||
            base.find("3panel") != std::string::npos) {
            hips_view_->jump_to(272.5, -17.2, 15.0, -1);
        } else {
            hips_view_->jump_to(0.0, 0.0, 60.0, -1);
        }
        return;
    }
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

void MainWindow::on_grid_toggle(bool checked) {
    // 经纬线网格开关: 转发给当前 SphereView
    if (current_view_) {
        if (auto* v = qobject_cast<SphereView*>(current_view_)) {
            v->set_grid_visible(checked);
        }
    }
}

void MainWindow::on_zoom_in() {
    if (hips_view_) {
        hips_view_->sky()->set_view(
            hips_view_->sky()->center_ra(), hips_view_->sky()->center_dec(),
            hips_view_->sky()->fov() * 0.8, 0.0);
        hips_view_->sky()->set_size(width(), height());
        hips_view_->mark_dirty();
        return;
    }
    if (current_view_) {
        if (auto* v = qobject_cast<SphereView*>(current_view_)) {
            v->zoom_in();
        }
    }
}

void MainWindow::on_zoom_out() {
    if (hips_view_) {
        hips_view_->sky()->set_view(
            hips_view_->sky()->center_ra(), hips_view_->sky()->center_dec(),
            hips_view_->sky()->fov() / 0.8, 0.0);
        hips_view_->sky()->set_size(width(), height());
        hips_view_->mark_dirty();
        return;
    }
    if (current_view_) {
        if (auto* v = qobject_cast<SphereView*>(current_view_)) {
            v->zoom_out();
        }
    }
}

// ============================================================================
// STF 控制
// ============================================================================

void MainWindow::on_stf_changed(const STFParams& params) {
    // 来自 STFPanel 的滑块变化 → 转发给 view
    if (hips_view_) {
        hips_auto_range_ = false;
        hips_view_->set_manual_range(params.shadows, params.highlights);
        return;
    }
    if (current_view_) {
        current_view_->set_stf_params(params);
    }
}

void MainWindow::on_auto_stretch_clicked() {
    if (hips_view_) {
        hips_auto_range_ = true;
        hips_view_->set_stretch(
            stretch_combo_->currentText().toLower().toStdString(), true);
        return;
    }
    if (current_view_) {
        current_view_->auto_stretch();
    }
}

// ============================================================================
// 状态栏更新
// ============================================================================

void MainWindow::on_view_changed(double ra, double dec, double fov) {
    // 第三个参数语义变更: zoom → fov (SphereView 用 FOV 控制缩放)
    status_view_->setText(QString("视角: RA=%1° Dec=%2° FOV=%3°")
                          .arg(ra, 0, 'f', 1)
                          .arg(dec, 0, 'f', 1, '+')
                          .arg(fov, 0, 'f', 1));
}

void MainWindow::on_mouse_moved(double ra, double dec) {
    status_mouse_->setText(QString("鼠标: RA=%1° Dec=%2°")
                           .arg(ra, 0, 'f', 2)
                           .arg(dec, 0, 'f', 2, '+'));
}
