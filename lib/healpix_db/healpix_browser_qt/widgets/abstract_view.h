// abstract_view.h - HEALPix 浏览器 Qt widget 抽象基类 (healpix_browser_qt)
// 功能: QOpenGLWidget 子类, 封装 core/ 调用骨架, 管理 OpenGL 上下文与事件转发
// 用途: 为 SingleFrameView / SphereView 提供通用基类, 强制子类实现 handle_* 接口
// 依赖: Qt6::OpenGLWidgets, Qt6::Gui (QMouseEvent/QWheelEvent),
//       core/ (BrowserBackend + GLRenderer + STFEngine)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §3.1
// 编译: 通过 CMake (AUTOMOC ON), 链接 Qt6::OpenGLWidgets + healpix_browser_core

#ifndef ABSTRACT_VIEW_H
#define ABSTRACT_VIEW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <memory>
#include "healpix_browser_core.h"

class AbstractView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit AbstractView(QWidget* parent = nullptr);
    virtual ~AbstractView();

    // 绑定数据源 (app 层调用, backend 所有权归 app 层)
    void set_backend(BrowserBackend* backend);

    // STF 控制 (app 层调用)
    void set_stf_params(const STFParams& params);
    // 基于当前数据自动计算 STF (打开文件后调用一次)
    void auto_stretch();

    // 获取数据动态范围 (供 STFPanel 滑块映射用)
    void get_data_range(float& out_min, float& out_max) const {
        out_min = data_min_;
        out_max = data_max_;
    }

    // 获取当前视角 (子类维护, 供状态栏显示)
    virtual void get_view_params(ViewParams& out) const = 0;

signals:
    // 视角变化信号 (状态栏更新坐标)
    void view_changed(double center_ra, double center_dec, double zoom);
    // 鼠标位置变化信号 (状态栏显示鼠标处坐标)
    void mouse_moved(double ra, double dec);
    // STF 参数变化信号 (控制面板同步)
    void stf_changed(const STFParams& params);

protected:
    // ---- QOpenGLWidget 重载 ----
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // ---- 子类实现的交互逻辑 ----
    virtual void handle_mouse_press(QMouseEvent* event) = 0;
    virtual void handle_mouse_move(QMouseEvent* event) = 0;
    virtual void handle_mouse_release(QMouseEvent* event) = 0;
    virtual void handle_wheel(QWheelEvent* event) = 0;

    // Qt 事件重载 (final, 转发到 handle_*, 避免子类直接重载绕过基类逻辑)
    void mousePressEvent(QMouseEvent* event) override final;
    void mouseMoveEvent(QMouseEvent* event) override final;
    void mouseReleaseEvent(QMouseEvent* event) override final;
    void wheelEvent(QWheelEvent* event) override final;

    // 子类实现的渲染参数构建
    virtual RenderParams build_render_params() = 0;

    // 触发重绘 (子类在状态变化后调用)
    void request_render();

    // 共享状态
    BrowserBackend* backend_;                  // 裸指针, app 层拥有所有权
    std::unique_ptr<GLRenderer> renderer_;     // 独占 GLRenderer 实例
    STFParams stf_params_;                     // 当前 STF 参数
    float data_min_;                           // 数据动态范围 (归一化用)
    float data_max_;
    float no_data_value_;                      // 无数据标记 (默认 0.0)

    // 数据范围计算 (首次 paintGL 或 auto_stretch 时调用)
    // 实现: .hiss 全量遍历; .hcsd 采样前 N 个子叶
    void compute_data_range();

private:
    bool gl_initialized_;
    bool data_range_computed_;  // 防止重复计算
};

#endif // ABSTRACT_VIEW_H
