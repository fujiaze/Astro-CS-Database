# C++ Qt 浏览器 - UI 前端工程设计

> **日期**：2026-07-13
> **状态**：设计阶段，待实现
> **范围**：本文档覆盖**UI 前端**（widgets/ + app/）的设计。核心算法库见 `2026-07-13-cpp-qt-browser-core-design.md`。
> **原则**：UI 与算法严格分离。UI 是前端（Qt6 widget + demo exe），算法是后端核心（core/，无 Qt 依赖）。UI 层仅负责窗口管理、事件桥接、Qt OpenGL 上下文管理，不含任何业务/算法逻辑。

---

## 1. 定位与边界

### 1.1 UI 层职责

- **窗口管理**：创建 QMainWindow、菜单栏、状态栏、控制面板
- **OpenGL 上下文管理**：QOpenGLWidget 创建上下文 + makeCurrent + swapBuffers
- **事件处理**：鼠标（拖动/滚轮/移动）、键盘、触摸事件 → 更新 ViewParams → 触发重绘
- **控制面板**：STF 滑块、预设按钮、文件选择对话框
- **文件路由**：根据扩展名（.hiss / .hcsd）选择对应 widget

### 1.2 UI 层不做什么

- 不做数据加载（调用 `BrowserBackend`）
- 不做坐标转换（调用 `HealpixMath`）
- 不做拉伸计算（调用 `STFEngine`）
- 不做 OpenGL 渲染逻辑（调用 `GLRenderer`）
- 不做 ud_grade 降采样（调用 `BrowserBackend`）

### 1.3 与 core/ 的依赖关系

```
widgets/ 依赖 → core/（C++ 类接口）
           依赖 → Qt6::OpenGLWidgets, Qt6::Core, Qt6::Gui
app/      依赖 → widgets/ + core/
           依赖 → Qt6::Widgets（QMainWindow, QFileDialog, QStatusBar）
```

---

## 2. Qt6 环境与依赖

### 2.1 Qt6 模块

| 模块 | 用途 |
|------|------|
| `Qt6::Core` | 基础类型（QString, QObject, 信号槽） |
| `Qt6::Gui` | QMouseEvent, QWheelEvent, QKeyEvent |
| `Qt6::Widgets` | QMainWindow, QFileDialog, QSlider, QPushButton, QStatusBar, QMenuBar |
| `Qt6::OpenGLWidgets` | QOpenGLWidget（OpenGL 上下文 + widget） |

### 2.2 OpenGL 版本

- 目标：OpenGL 3.3 Core Profile（QSurfaceFormat 设置）
- QOpenGLWidget 默认提供兼容性上下文，需显式请求 Core Profile
- 着色器用 `#version 330 core`

### 2.3 构建工具

- **CMake**（推荐 Qt6 项目）：`CMakeLists.txt`
- 或 **qmake**（备选）：`.pro` 文件
- 编译器：MSYS2 MinGW64 g++ 16.1.0（与 core/ 一致）

---

## 3. widgets/ 类设计

### 3.1 AbstractView（抽象基类）

**职责**：QOpenGLWidget 子类，封装 core/ 调用骨架，定义通用接口。子类实现具体交互逻辑。

```cpp
// widgets/abstract_view.h
#ifndef ABSTRACT_VIEW_H
#define ABSTRACT_VIEW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <memory>
#include "browser_backend.h"
#include "gl_renderer.h"
#include "stf_engine.h"

class AbstractView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit AbstractView(QWidget* parent = nullptr);
    virtual ~AbstractView();

    // 绑定数据源（app 层调用）
    void set_backend(BrowserBackend* backend);

    // STF 控制（app 层调用）
    void set_stf_params(const STFParams& params);
    void auto_stretch();  // 基于当前数据自动计算 STF

    // 获取当前视角（子类维护，供状态栏显示）
    virtual void get_view_params(ViewParams& out) const = 0;

signals:
    // 视角变化信号（状态栏更新坐标）
    void view_changed(double center_ra, double center_dec, double zoom);
    // 鼠标位置变化信号（状态栏显示鼠标处坐标）
    void mouse_moved(double ra, double dec);
    // STF 参数变化信号（控制面板同步）
    void stf_changed(const STFParams& params);

protected:
    // QOpenGLWidget 重载
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 子类实现的交互逻辑
    virtual void handle_mouse_press(QMouseEvent* event) = 0;
    virtual void handle_mouse_move(QMouseEvent* event) = 0;
    virtual void handle_mouse_release(QMouseEvent* event) = 0;
    virtual void handle_wheel(QWheelEvent* event) = 0;

    // Qt 事件重载（转发到 handle_*）
    void mousePressEvent(QMouseEvent* event) override final;
    void mouseMoveEvent(QMouseEvent* event) override final;
    void mouseReleaseEvent(QMouseEvent* event) override final;
    void wheelEvent(QWheelEvent* event) override final;

    // 子类实现的渲染参数构建
    virtual RenderParams build_render_params() = 0;

    // 共享状态
    BrowserBackend* backend_;
    std::unique_ptr<GLRenderer> renderer_;
    STFParams stf_params_;
    float data_min_, data_max_, no_data_value_;

    // 数据范围计算（首次 paintGL 时调用）
    // 实现：遍历 backend 数据采样（.hiss 全量；.hcsd 取前 N 个子叶）求 min/max
    // 与 auto_stretch() 配合：auto_stretch 内部调用 STFEngine::auto_stretch，
    //   该函数已计算 median/MAD，data_min/data_max 在此处一并求得用于 GPU 归一化
    void compute_data_range();

private:
    bool gl_initialized_;
};

#endif
```

**设计要点**：
- `mousePressEvent` 等声明为 `final`，强制子类实现 `handle_*` 而非直接重载 Qt 事件，避免遗漏基类逻辑
- `backend_` 为裸指针（app 层拥有所有权），view 不负责释放
- `renderer_` 为 unique_ptr，view 独占 GLRenderer 实例
- `compute_data_range()` 在首次 paintGL 时调用，遍历数据求 min/max（或采样）

### 3.2 SingleFrameView（单帧 2D 切面投影）

**职责**：.hiss 文件的 2D 切面投影渲染，拖动平移 + 滚轮缩放。

```cpp
// widgets/single_frame_view.h
#ifndef SINGLE_FRAME_VIEW_H
#define SINGLE_FRAME_VIEW_H

#include "abstract_view.h"

class SingleFrameView : public AbstractView {
    Q_OBJECT

public:
    explicit SingleFrameView(QWidget* parent = nullptr);

    void get_view_params(ViewParams& out) const override;

    // 设置初始视角（根据数据边界框自动计算）
    void init_view_from_data();

protected:
    void handle_mouse_press(QMouseEvent* event) override;
    void handle_mouse_move(QMouseEvent* event) override;
    void handle_mouse_release(QMouseEvent* event) override;
    void handle_wheel(QWheelEvent* event) override;
    RenderParams build_render_params() override;

private:
    // 视角状态（单帧模式专用）
    double center_ra_, center_dec_;   // 视角中心（度）
    double zoom_;                     // 缩放（1.0 = 初始，>1 放大）
    double data_fov_deg_;             // 数据本身 FOV（初始 zoom=1 时显示范围）

    // 拖动状态
    bool is_dragging_;
    int last_mouse_x_, last_mouse_y_;

    // 拖动灵敏度（FOV/canvasWidth，cos(dec) 修正 RA）
    double drag_sensitivity() const;

    // 视角变化检查（避免 STF 更新触发的无谓纹理重建）
    bool view_changed_since_last_render();
    double last_render_center_ra_, last_render_center_dec_, last_render_zoom_;
};

#endif
```

**交互逻辑**（移植自 `view-controller.js` 单帧模式）：
- 拖动：`delta_ra = -dx × drag_sensitivity × cos(center_dec)`，`delta_dec = dy × drag_sensitivity`
- 缩放：`zoom *= exp(-delta_y × 0.001)`，clamp 到 [0.5, 100]
- 视角变化检测：比较 `center_ra/center_dec/zoom` 与上次渲染值，仅视角变化时重建纹理

**渲染参数构建**：
```cpp
RenderParams SingleFrameView::build_render_params() {
    RenderParams p;
    p.mode = RenderMode::SINGLE_FRAME;
    p.view.center_ra = center_ra_;
    p.view.center_dec = center_dec_;
    p.view.zoom = zoom_;
    p.view.fov_deg = data_fov_deg_ / zoom_;
    p.stf = stf_params_;
    p.data_min = data_min_;
    p.data_max = data_max_;
    p.no_data_value = no_data_value_;
    p.viewport_w = width();
    p.viewport_h = height();
    return p;
}
```

### 3.3 SphereView（球面 3D 渲染）

**职责**：.hcsd 文件的球面渲染，鼠标拖动旋转 + 滚轮缩放 + 触摸支持。

```cpp
// widgets/sphere_view.h
#ifndef SPHERE_VIEW_H
#define SPHERE_VIEW_H

#include "abstract_view.h"

class SphereView : public AbstractView {
    Q_OBJECT

public:
    explicit SphereView(QWidget* parent = nullptr);

    void get_view_params(ViewParams& out) const override;

    // 重置视角到默认（RA=0, Dec=0, zoom=1）
    void reset_view();

protected:
    void handle_mouse_press(QMouseEvent* event) override;
    void handle_mouse_move(QMouseEvent* event) override;
    void handle_mouse_release(QMouseEvent* event) override;
    void handle_wheel(QWheelEvent* event) override;
    RenderParams build_render_params() override;

private:
    // 视角状态（球面模式专用）
    double center_ra_, center_dec_;   // 视角中心（度）
    double zoom_;                     // 缩放（1.0 = 全天，>1 放大）
    double fov_deg_;                  // 视场角（= 180 / zoom）

    // 拖动状态
    bool is_dragging_;
    int last_mouse_x_, last_mouse_y_;

    // 旋转灵敏度（0.3 度/像素，与 view-controller.js 一致）
    static constexpr double ROTATE_SPEED = 0.3;
    // 缩放限制
    static constexpr double MIN_ZOOM = 0.5;
    static constexpr double MAX_ZOOM = 100.0;
    // 缩放灵敏度
    static constexpr double ZOOM_SPEED = 0.0015;

    // 触摸支持
    bool is_touching_;
    int last_touch_x_, last_touch_y_;
    double last_touch_dist_;  // 双指距离（捏合缩放）

    void handle_touch_begin(QTouchEvent* event);
    void handle_touch_update(QTouchEvent* event);
    void handle_touch_end(QTouchEvent* event);

    // 鼠标位置 → 天球坐标（用于状态栏显示）
    void screen_to_sky(int x, int y, double& ra, double& dec);
};

#endif
```

**交互逻辑**（移植自 `view-controller.js` 球面模式）：
- 拖动：`center_ra -= dx × ROTATE_SPEED`，`center_dec += dy × ROTATE_SPEED`（clamp [-90, 90]）
- 缩放：`zoom *= exp(-delta_y × ZOOM_SPEED)`，`fov_deg = 180 / zoom`
- 触摸：单指拖动 = 鼠标拖动；双指捏合 = 缩放（`zoom *= pinch_scale`）

**screen_to_sky**：将屏幕坐标反投影到球面（球面投影逆变换），用于状态栏显示鼠标处天球坐标。

### 3.4 widgets/ 目录结构

```
widgets/
├── abstract_view.h
├── abstract_view.cpp
├── single_frame_view.h
├── single_frame_view.cpp
├── sphere_view.h
├── sphere_view.cpp
└── widgets.pri              ← qmake include 文件（或 CMakeLists.txt 片段）
```

---

## 4. app/ demo 设计

### 4.1 MainWindow（主窗口）

**职责**：QMainWindow 容器，菜单栏 + 文件选择 + STF 控制面板 + 状态栏。

```cpp
// app/main_window.h
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <memory>
#include "browser_backend.h"

class AbstractView;
class STFPanel;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    void open_file_from_cli(const QString& path);  // 命令行参数打开文件

private slots:
    void on_file_open();           // File > Open 菜单
    void on_file_close();          // File > Close
    void on_exit();                // File > Exit
    void on_stf_changed(const STFParams& params);
    void on_view_changed(double center_ra, double center_dec, double zoom);
    void on_mouse_moved(double ra, double dec);

private:
    void setup_menu();
    void setup_status_bar();
    void open_file(const QString& path);
    void close_file();
    void set_view(AbstractView* view);  // 切换当前 view（单帧/球面）

    // 数据源（app 拥有）
    std::unique_ptr<BrowserBackend> backend_;

    // 当前 view（单帧或球面，同时只显示一个）
    AbstractView* current_view_;
    QWidget* view_container_;

    // 控制面板
    STFPanel* stf_panel_;

    // 状态栏标签
    QLabel* status_file_;
    QLabel* status_view_;
    QLabel* status_mouse_;
};

#endif
```

**文件路由逻辑**：
```cpp
void MainWindow::open_file(const QString& path) {
    if (backend_->open_file(path.toStdString()) != 0) {
        QMessageBox::critical(this, "错误", "无法打开文件");
        return;
    }

    // 关闭旧 view
    if (current_view_) {
        close_file();
    }

    // 按扩展名路由
    AbstractView* view = nullptr;
    if (backend_->is_hiss()) {
        view = new SingleFrameView(this);
        dynamic_cast<SingleFrameView*>(view)->init_view_from_data();
    } else if (backend_->is_hcsd()) {
        view = new SphereView(this);
    } else {
        QMessageBox::warning(this, "警告", "不支持的文件格式");
        return;
    }

    view->set_backend(backend_.get());
    view->auto_stretch();  // 打开后自动拉伸

    set_view(view);
    status_file_->setText(QString("文件: %1").arg(path));
}
```

### 4.2 STFPanel（STF 控制面板）

**职责**：QDockWidget，包含 STF 滑块 + 预设按钮 + 自动拉伸按钮。

```cpp
// app/stf_panel.h
#ifndef STF_PANEL_H
#define STF_PANEL_H

#include <QDockWidget>
#include "stf_engine.h"

class QSlider;
class QPushButton;
class QComboBox;

class STFPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit STFPanel(QWidget* parent = nullptr);

signals:
    void stf_changed(const STFParams& params);

private slots:
    void on_slider_changed();
    void on_preset_clicked();
    void on_auto_stretch_clicked();

private:
    void setup_ui();
    void update_sliders(const STFParams& params);
    STFParams collect_params();

    QSlider* shadows_slider_;      // 0-1000 → [0, 1)
    QSlider* highlights_slider_;   // 0-1000 → (0, 1]
    QSlider* midtones_slider_;     // 1-999 → (0, 1)
    QSlider* compression_slider_;  // 0-1000 → [0, 1]
    QComboBox* preset_combo_;      // linear/sqrt/asinh/log
    QPushButton* auto_button_;     // 自动拉伸
};

#endif
```

**UI 布局**：
```
┌─ STF 控制面板 ──────────────────┐
│ 预设: [linear      ▼]           │
│                                 │
│ Shadows    [====|==========]    │
│ Highlights [==========|====]    │
│ Midtones   [======|======]      │
│ Compression[==|==========]      │
│                                 │
│         [自动拉伸]              │
└─────────────────────────────────┘
```

**滑块精度**：QSlider 范围 0-1000（int），映射到 [0, 1] float，精度 0.001。

### 4.3 main.cpp（入口）

```cpp
// app/main.cpp
#include <QApplication>
#include <QCommandLineParser>
#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("HEALPix Browser");
    app.setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument("file", "可选：直接打开的文件路径 (.hiss/.hcsd)");
    parser.process(app);

    MainWindow window;
    window.show();

    // 命令行参数指定文件时直接打开
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        // 延迟到事件循环启动后打开（此时 MainWindow 已 show）
        QMetaObject::invokeMethod(&window, "open_file_from_cli",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, args.first()));
    }

    return app.exec();
}
```

### 4.4 app/ 目录结构

```
app/
├── main.cpp
├── main_window.h
├── main_window.cpp
├── stf_panel.h
├── stf_panel.cpp
└── app.pri
```

---

## 5. 事件处理与交互

### 5.1 事件流（单帧模式）

```
QMouseEvent (mousePress)
  → SingleFrameView::mousePressEvent (final, 基类)
  → SingleFrameView::handle_mouse_press (子类)
  → 记录 last_mouse_x/y, is_dragging=true
  → 不触发重绘

QMouseEvent (mouseMove, 拖动中)
  → SingleFrameView::handle_mouse_move
  → 计算 delta, 更新 center_ra/center_dec
  → emit view_changed(center_ra, center_dec, zoom)
  → update()  → 触发 paintGL
  → GLRenderer.render(backend, params)
  → 视角变化检测 → 重建 1024×1024 纹理
  → 绘制全屏四边形

QWheelEvent (滚轮)
  → SingleFrameView::handle_wheel
  → zoom *= exp(-delta * 0.001)
  → emit view_changed(...)
  → update() → paintGL
```

### 5.2 事件流（球面模式）

```
QMouseEvent (mousePress)
  → SphereView::handle_mouse_press
  → 记录 last_mouse_x/y, is_dragging=true

QMouseEvent (mouseMove, 拖动中)
  → SphereView::handle_mouse_move
  → center_ra -= dx * 0.3, center_dec += dy * 0.3 (clamp)
  → emit view_changed(...)
  → emit mouse_moved(screen_to_sky(x, y))  ← 状态栏坐标
  → update() → paintGL
  → GLRenderer.render → backend.get_required_leaves → 按需加载子叶 → 球面绘制

QWheelEvent
  → SphereView::handle_wheel
  → zoom *= exp(-delta * 0.0015)
  → update() → paintGL（子叶可能变化）
```

### 5.3 触摸事件（球面模式）

```cpp
void SphereView::handle_touch_update(QTouchEvent* event) {
    const auto& points = event->points();
    if (points.size() == 1) {
        // 单指拖动
        int dx = points[0].position().x() - last_touch_x_;
        int dy = points[0].position().y() - last_touch_y_;
        center_ra_ -= dx * ROTATE_SPEED;
        center_dec_ = std::clamp(center_dec_ + dy * ROTATE_SPEED, -90.0, 90.0);
        last_touch_x_ = points[0].position().x();
        last_touch_y_ = points[0].position().y();
    } else if (points.size() == 2) {
        // 双指捏合缩放
        double dist = QLineF(points[0].position(), points[1].position()).length();
        if (last_touch_dist_ > 0) {
            double scale = dist / last_touch_dist_;
            zoom_ = std::clamp(zoom_ * scale, MIN_ZOOM, MAX_ZOOM);
        }
        last_touch_dist_ = dist;
    }
    update();
}
```

---

## 6. QOpenGLWidget 上下文管理

### 6.1 上下文创建

```cpp
void AbstractView::initializeGL() {
    initializeOpenGLFunctions();  // QOpenGLFunctions_3_3_Core

    renderer_ = std::make_unique<GLRenderer>();
    if (renderer_->init() != 0) {
        qCritical() << "GLRenderer 初始化失败";
        return;
    }

    gl_initialized_ = true;
    compute_data_range();  // 首次计算数据范围
}
```

### 6.2 上下文切换

QOpenGLWidget 自动管理上下文：
- `paintGL()` 调用前自动 `makeCurrent()`
- `paintGL()` 调用后自动 `doneCurrent()`
- 多 widget 时每个 widget 有独立上下文（不共享资源，除非显式共享）

**本设计每个 view 独占一个 GLRenderer**，不跨 widget 共享 OpenGL 资源，避免上下文共享复杂性。

### 6.3 资源释放

```cpp
void AbstractView::aboutToDestroy() {
    // widget 销毁前（Qt 信号，需在析构函数手动触发）
    if (gl_initialized_ && renderer_) {
        makeCurrent();
        renderer_->cleanup();
        doneCurrent();
    }
}

AbstractView::~AbstractView() {
    // Qt 保证 widget 销毁前 OpenGL 上下文仍有效（在 ~QOpenGLWidget 之前）
    // 但需手动 makeCurrent 才能调用 OpenGL 命令
    if (gl_initialized_ && renderer_) {
        makeCurrent();
        renderer_->cleanup();
        doneCurrent();
    }
}
```

---

## 7. 构建配置

### 7.1 CMakeLists.txt（推荐）

```cmake
cmake_minimum_required(VERSION 3.16)
project(healpix_browser_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)
find_package(OpenGL REQUIRED)

# core 静态库
add_library(healpix_browser_core STATIC
    core/browser_backend.cpp
    core/stf_engine.cpp
    core/healpix_math.cpp
    core/gl_renderer.cpp
)
target_include_directories(healpix_browser_core PUBLIC core include ../healpix_io/include)
target_link_libraries(healpix_browser_core PUBLIC OpenGL::GL)

# widgets 静态库
add_library(healpix_browser_qt_widgets STATIC
    widgets/abstract_view.cpp
    widgets/single_frame_view.cpp
    widgets/sphere_view.cpp
)
target_include_directories(healpix_browser_qt_widgets PUBLIC widgets)
target_link_libraries(healpix_browser_qt_widgets PUBLIC
    healpix_browser_core
    Qt6::Core Qt6::Gui Qt6::OpenGLWidgets
)

# demo exe
add_executable(healpix_browser_qt
    app/main.cpp
    app/main_window.cpp
    app/stf_panel.cpp
)
target_link_libraries(healpix_browser_qt PRIVATE
    healpix_browser_qt_widgets
    healpix_browser_core
    Qt6::Core Qt6::Gui Qt6::Widgets
)
```

### 7.2 Qt6 安装路径

MSYS2 安装 Qt6：
```
pacman -S mingw-w64-x86_64-qt6-base
```

CMake 配置时指定 Qt6 路径：
```
cmake -DCMAKE_PREFIX_PATH="C:/msys64/mingw64" ..
```

---

## 8. UI 改进实现（PROJECT_ARCHITECTURE.md §10.7）

### 8.1 单帧/球面独立入口

**现有问题**：WebGL 浏览器通过下拉框切换单帧/球面模式，交互逻辑差异大导致混用冲突。

**新设计**：
- `SingleFrameView` 和 `SphereView` 是完全独立的 QOpenGLWidget 子类
- `MainWindow` 根据文件扩展名自动选择 view，同时只显示一个
- 两个 view 不共享状态，无模式混用风险
- File > Open 可随时切换文件（关闭旧 view → 创建新 view）

### 8.2 文件夹交互 UI

**现有问题**：WebGL 浏览器文件由 C++ 后端命令行参数指定，前端无法选择。

**新设计**：
- `MainWindow::on_file_open()` 调用 `QFileDialog::getOpenFileName()`
- 过滤器：`"HEALPix 文件 (*.hiss *.hcsd);;单帧存储 (*.hiss);;天球数据库 (*.hcsd)"`
- 支持命令行参数直接打开文件（main.cpp 解析）
- 状态栏显示当前文件路径

### 8.3 最近打开列表（可选，后续扩展）

```cpp
// main_window.h 新增
QMenu* recent_menu_;
QStringList recent_files_;  // 最多 10 个

void update_recent_menu();
void on_recent_file_triggered(const QString& path);
```

---

## 9. 状态栏与坐标显示

### 9.1 状态栏布局

```
┌─────────────────────────────────────────────────────────────────┐
│ 文件: test.hcsd  │  视角: RA=180.5° Dec=+30.2° zoom=2.5x  │  鼠标: RA=180.3° Dec=+30.5°  │
└─────────────────────────────────────────────────────────────────┘
```

### 9.2 信号槽连接

```cpp
// MainWindow 构造函数
connect(view, &AbstractView::view_changed, this, &MainWindow::on_view_changed);
connect(view, &AbstractView::mouse_moved, this, &MainWindow::on_mouse_moved);

void MainWindow::on_view_changed(double ra, double dec, double zoom) {
    status_view_->setText(QString("视角: RA=%1° Dec=%2° zoom=%3x")
                          .arg(ra, 0, 'f', 1)
                          .arg(dec, 0, 'f', 1, '+')  // 带正号
                          .arg(zoom, 0, 'f', 2));
}

void MainWindow::on_mouse_moved(double ra, double dec) {
    status_mouse_->setText(QString("鼠标: RA=%1° Dec=%2°")
                           .arg(ra, 0, 'f', 2)
                           .arg(dec, 0, 'f', 2, '+'));
}
```

---

## 10. 嵌入大工程指南

### 10.1 嵌入方式

大工程（如 Qt6 主程序）嵌入浏览器：

```cpp
// 大工程的某个窗口
#include "healpix_browser_core.h"
#include "sphere_view.h"

class MyMainWindow : public QMainWindow {
    void open_browser() {
        // 1. 创建数据源
        auto backend = std::make_unique<BrowserBackend>();
        backend->open_file("sky.hcsd");

        // 2. 创建 view widget
        auto* view = new SphereView(this);
        view->set_backend(backend.get());
        view->auto_stretch();

        // 3. 嵌入到布局
        setCentralWidget(view);

        // 4. backend 生命周期管理（由 MyMainWindow 持有）
        backend_ = std::move(backend);
    }
};
```

### 10.2 仅嵌入 core/（不用 Qt widget）

若大工程不用 Qt，仅用 core/：

```cpp
#include "healpix_browser_core.h"

// 大工程自建 OpenGL 上下文（GLFW/SDL/Win32）
// 然后调用 core/ 接口
BrowserBackend backend;
backend.open_file("sky.hcsd");

GLRenderer renderer;
renderer.init();  // 在大工程的 OpenGL 上下文 makeCurrent 后调用

RenderParams params;
// ... 填充参数 ...
renderer.render(backend, params);
```

### 10.3 编译嵌入

CMake 大工程链接：
```cmake
target_link_libraries(my_app PRIVATE
    healpix_browser_core    # core 静态库
    # 或 healpix_browser_qt_widgets  # 含 Qt widget
    healpix_io              # .hiss/.hcsd 读写
)
```

---

## 11. 验证标准（UI 部分）

1. **功能完整性**：
   - File > Open 可选择 .hiss / .hcsd 文件
   - .hiss → SingleFrameView 自动显示（2D 切面投影）
   - .hcsd → SphereView 自动显示（3D 球面）
   - 拖动/缩放交互正确（单帧 2D + 球面 3D）
   - STF 滑块实时更新渲染
   - 4 预设按钮正确切换
   - 自动拉伸按钮基于数据计算
   - 状态栏显示文件名/视角/鼠标坐标

2. **UI 改进（§10.7）**：
   - 单帧/球面为独立 widget 类，无模式混用
   - QFileDialog 文件选择可用
   - 命令行参数打开文件可用

3. **嵌入就绪**：
   - demo exe 验证完整功能
   - widgets/ 可被外部 Qt 工程链接
   - core/ 可被非 Qt 工程链接

4. **无 Qt 依赖检查**：
   - `grep -r "Q" core/` 应无 Qt 类型/头文件引用（注释除外）
   - core/ 编译不依赖 Qt6

---

## 12. 开放问题

### 12.1 Qt6 vs qmake

推荐 CMake（Qt6 官方推荐）。若项目其他模块用 Makefile，可考虑为 UI 层单独用 CMake，core/ 仍用 Makefile（与现有模块一致）。

### 12.2 触摸支持优先级

球面模式设计了触摸事件（单指拖动 + 双指捏合），但 demo exe 主要用鼠标。触摸支持在桌面端为可选，若实现成本高可后续补全。

### 12.3 多文件同时打开

当前设计同时只显示一个文件（单帧或球面）。若需多文件对比（如左右分屏），需扩展 MainWindow 为 MDI（多文档接口），后续按需实现。

### 12.4 主题与样式

当前用 Qt 默认深色主题（与系统一致）。若需自定义天文软件风格深色主题，可用 QSS（Qt Style Sheets）定制，但不影响功能。

---

## 附录：UI 层与 core 层调用关系

```
┌──────────────────────────────────────────────────────────────┐
│ app/MainWindow                                               │
│   ├─ QFileDialog → path                                      │
│   ├─ backend_->open_file(path)                    ──→ core/  │
│   ├─ new SingleFrameView / SphereView                        │
│   ├─ view->set_backend(backend_)                  ──→ widget │
│   ├─ view->auto_stretch()                         ──→ widget │
│   └─ STFPanel::stf_changed → view->set_stf_params ──→ widget │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ widgets/SingleFrameView / SphereView                         │
│   ├─ initializeGL → renderer_->init()             ──→ core/  │
│   ├─ paintGL → renderer_->render(backend, params) ──→ core/  │
│   ├─ handle_mouse_move → 更新 ViewParams → update()          │
│   └─ handle_wheel → 更新 zoom → update()                    │
└──────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────┐
│ core/GLRenderer                                              │
│   ├─ backend.get_required_leaves(view)            ──→ core/  │
│   ├─ backend.load_leaf(ipix, nside)               ──→ core/  │
│   ├─ HealpixMath.ang2pix_nest → 顶点查值          ──→ core/  │
│   ├─ glDrawElements (OpenGL 3.3)                             │
│   └─ 着色器内 MTF + asinh + uint8                            │
└──────────────────────────────────────────────────────────────┘
```
