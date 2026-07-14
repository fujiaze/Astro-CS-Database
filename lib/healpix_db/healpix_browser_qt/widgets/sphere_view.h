// sphere_view.h - 球面 3D 渲染 widget (healpix_browser_qt)
// 功能: .hcsd 文件的球面渲染, 鼠标拖动旋转 + 滚轮缩放 + 触摸支持
// 用途: 显示全天球面 HEALPix 数据, 旋转视角, 缩放视场
// 依赖: Qt6::Gui (QMouseEvent/QWheelEvent/QTouchEvent), core/ (GLRenderer + HealpixMath)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §3.3, §5.2, §5.3
// 交互: 拖动 center_ra -= dx * 0.3, center_dec += dy * 0.3 (clamp [-90,90])
//       缩放 zoom *= exp(-delta_y * 0.0015), fov_deg = 180 / zoom
//       触摸: 单指拖动, 双指捏合 = 缩放

#ifndef SPHERE_VIEW_H
#define SPHERE_VIEW_H

#include "abstract_view.h"

class QTouchEvent;

class SphereView : public AbstractView {
    Q_OBJECT

public:
    explicit SphereView(QWidget* parent = nullptr);

    void get_view_params(ViewParams& out) const override;

    // 重置视角到默认 (RA=0, Dec=0, zoom=1)
    void reset_view();

    // .hiss 模式: 根据数据 bbox 设置初始视角（对准数据中心，zoom 自适应）
    void set_initial_view_from_bbox();

    // 设置渲染模式（.hiss=HISS_POLYGON, .hcsd=SPHERE）
    void set_render_mode(RenderMode mode) { render_mode_ = mode; }

protected:
    void handle_mouse_press(QMouseEvent* event) override;
    void handle_mouse_move(QMouseEvent* event) override;
    void handle_mouse_release(QMouseEvent* event) override;
    void handle_wheel(QWheelEvent* event) override;
    RenderParams build_render_params() override;

    // 重载 paintGL: 首次渲染后检查 pending_initial_view_（mesh 构建完成后重新设置视角）
    void paintGL() override;

    // 触摸事件 (重载, 非 final, 子类可进一步定制)
    bool event(QEvent* event) override;

private:
    // 视角状态 (球面模式专用)
    double center_ra_;       // 视角中心赤经 (度)
    double center_dec_;      // 视角中心赤纬 (度)
    double zoom_;            // 缩放 (1.0 = 全天, >1 放大)
    double fov_deg_;         // 视场角 (= 180 / zoom)

    // 渲染模式（.hiss=HISS_POLYGON, .hcsd=SPHERE）
    RenderMode render_mode_ = RenderMode::SPHERE;

    // .hiss 模式: 首次渲染前 bbox 未知，设置此标志，paintGL 后重新设置视角
    bool pending_initial_view_ = false;

    // 拖动状态
    bool is_dragging_;
    int last_mouse_x_;
    int last_mouse_y_;

    // 触摸状态
    bool is_touching_;
    double last_touch_x_;
    double last_touch_y_;
    double last_touch_dist_;  // 双指距离 (捏合缩放)

    // 旋转灵敏度 (度/像素, 与 view-controller.js 一致)
    static constexpr double ROTATE_SPEED = 0.3;
    // 缩放限制
    static constexpr double MIN_ZOOM = 0.5;
    static constexpr double MAX_ZOOM = 100.0;
    // 缩放灵敏度
    static constexpr double ZOOM_SPEED = 0.0015;

    // 触摸事件处理
    void handle_touch_begin(QTouchEvent* event);
    void handle_touch_update(QTouchEvent* event);
    void handle_touch_end(QTouchEvent* event);

    // 屏幕坐标 → 天球坐标 (用于状态栏显示鼠标处坐标)
    // 实现: 球面投影逆变换 (透视投影 + lookAt 矩阵逆)
    void screen_to_sky(int x, int y, double& ra, double& dec);
};

#endif // SPHERE_VIEW_H
