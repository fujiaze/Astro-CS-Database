// sphere_view.cpp - 球面 3D 渲染 widget 实现 (healpix_browser_qt)
// 功能: .hcsd 文件的球面渲染, 鼠标拖动旋转 + 滚轮缩放 + 触摸支持
// 用途: 显示全天球面 HEALPix 数据, 移植自 healpix_browser_web/js/view-controller.js 球面模式
// 依赖: Qt6::Gui, core/ (GLRenderer + HealpixMath)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §3.3, §5.2, §5.3

#include "sphere_view.h"
#include "logger.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QLineF>
#include <QPointF>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 构造
// ============================================================================

SphereView::SphereView(QWidget* parent)
    : AbstractView(parent),
      center_ra_(0.0),
      center_dec_(0.0),
      zoom_(1.0),
      fov_deg_(180.0),
      is_dragging_(false),
      last_mouse_x_(0),
      last_mouse_y_(0),
      is_touching_(false),
      last_touch_x_(0.0),
      last_touch_y_(0.0),
      last_touch_dist_(0.0) {
    // 启用触摸事件
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setCursor(Qt::OpenHandCursor);
}

// ============================================================================
// 视角控制
// ============================================================================

void SphereView::reset_view() {
    center_ra_ = 0.0;
    center_dec_ = 0.0;
    zoom_ = 1.0;
    fov_deg_ = 180.0 / zoom_;
    emit view_changed(center_ra_, center_dec_, zoom_);
    request_render();
}

void SphereView::set_initial_view_from_bbox() {
    // 从 renderer 获取 .hiss 数据 bbox
    // 注: renderer 可能尚未初始化（首次 paintGL 前），用默认值兜底
    double ra = 0.0, dec = 0.0, w = 0.0, h = 0.0;
    if (renderer_) {
        renderer_->get_hiss_bbox(ra, dec, w, h);
    }

    // 如果 bbox 无效（renderer 未初始化或 mesh 未构建），设置 pending 标志
    if (w <= 0.0 || h <= 0.0) {
        LOG_INFO("set_initial_view_from_bbox: bbox 未知 (renderer 未初始化或 mesh 未构建)，设置 pending_initial_view_");
        pending_initial_view_ = true;
        return;
    }

    center_ra_ = ra;
    center_dec_ = dec;
    // zoom 自适应: 使 patch 占视野约 60%
    // 球面渲染中 zoom=1 对应 fov=180°，zoom=N 对应 fov=180/N
    // 要让 patch (max_dim 度) 占视野 60%: fov = max_dim / 0.6 → zoom = 180*0.6/max_dim = 108/max_dim
    double max_dim = std::max(w, h);
    if (max_dim > 0.001) {
        zoom_ = std::clamp(108.0 / max_dim, MIN_ZOOM, MAX_ZOOM);
    } else {
        zoom_ = 1.0;
    }
    fov_deg_ = 180.0 / zoom_;
    LOG_INFO("set_initial_view_from_bbox: center=(%.4f,%.4f) zoom=%.3f fov=%.3f",
             center_ra_, center_dec_, zoom_, fov_deg_);
    emit view_changed(center_ra_, center_dec_, zoom_);
    request_render();
}

void SphereView::paintGL() {
    // 先调用父类 paintGL 执行渲染（包括首次 build_hiss_polygon_mesh）
    AbstractView::paintGL();

    // .hiss 模式: 首次渲染后 mesh 已构建，bbox 已计算，重新设置初始视角
    if (pending_initial_view_ && renderer_) {
        double ra, dec, w, h;
        renderer_->get_hiss_bbox(ra, dec, w, h);
        if (w > 0.0 && h > 0.0) {
            LOG_INFO("paintGL: 检测到 mesh 已构建，触发 pending initial view");
            pending_initial_view_ = false;
            set_initial_view_from_bbox();  // 此时 bbox 有效，会重新设置视角并 request_render
        }
    }
}

void SphereView::get_view_params(ViewParams& out) const {
    out.center_ra = center_ra_;
    out.center_dec = center_dec_;
    out.zoom = zoom_;
    out.fov_deg = fov_deg_;
}

// ============================================================================
// 鼠标交互 (移植自 view-controller.js 球面模式)
// ============================================================================

void SphereView::handle_mouse_press(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        is_dragging_ = true;
        last_mouse_x_ = event->position().x();
        last_mouse_y_ = event->position().y();
        setCursor(Qt::ClosedHandCursor);
    }
}

void SphereView::handle_mouse_move(QMouseEvent* event) {
    if (!is_dragging_) {
        return;
    }

    int dx = event->position().x() - last_mouse_x_;
    int dy = event->position().y() - last_mouse_y_;
    last_mouse_x_ = event->position().x();
    last_mouse_y_ = event->position().y();

    // 拖动方向: 鼠标右移 → 视角左移 (球面绕轴旋转)
    //   center_ra -= dx * ROTATE_SPEED (鼠标右移 dx>0 → ra 减小)
    //   center_dec += dy * ROTATE_SPEED (鼠标下移 dy>0 → dec 增大, 即视角向下转)
    center_ra_ -= dx * ROTATE_SPEED;
    center_dec_ += dy * ROTATE_SPEED;

    // 归一化 ra 到 [0, 360)
    while (center_ra_ < 0.0) center_ra_ += 360.0;
    while (center_ra_ >= 360.0) center_ra_ -= 360.0;
    // clamp dec 到 [-90, 90]
    if (center_dec_ < -90.0) center_dec_ = -90.0;
    if (center_dec_ > 90.0) center_dec_ = 90.0;

    emit view_changed(center_ra_, center_dec_, zoom_);

    // 鼠标位置 → 天球坐标 (状态栏显示)
    double ra, dec;
    screen_to_sky(event->position().x(), event->position().y(), ra, dec);
    emit mouse_moved(ra, dec);

    request_render();
}

void SphereView::handle_mouse_release(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        is_dragging_ = false;
        setCursor(Qt::OpenHandCursor);
    }
}

void SphereView::handle_wheel(QWheelEvent* event) {
    // 滚轮缩放: zoom *= exp(-delta_y * 0.0015)
    // fov_deg = 180 / zoom (zoom 越大 → fov 越小 → 放大)
    double delta_y = event->angleDelta().y();
    double factor = std::exp(-delta_y * ZOOM_SPEED);
    zoom_ = std::clamp(zoom_ * factor, MIN_ZOOM, MAX_ZOOM);
    fov_deg_ = 180.0 / zoom_;

    emit view_changed(center_ra_, center_dec_, zoom_);
    request_render();
}

// ============================================================================
// 触摸事件
// ============================================================================

bool SphereView::event(QEvent* event) {
    switch (event->type()) {
        case QEvent::TouchBegin:
            handle_touch_begin(static_cast<QTouchEvent*>(event));
            return true;
        case QEvent::TouchUpdate:
            handle_touch_update(static_cast<QTouchEvent*>(event));
            return true;
        case QEvent::TouchEnd:
            handle_touch_end(static_cast<QTouchEvent*>(event));
            return true;
        default:
            return AbstractView::event(event);
    }
}

void SphereView::handle_touch_begin(QTouchEvent* event) {
    const auto& points = event->points();
    is_touching_ = true;
    if (points.size() == 1) {
        last_touch_x_ = points[0].position().x();
        last_touch_y_ = points[0].position().y();
        last_touch_dist_ = 0.0;
    } else if (points.size() == 2) {
        last_touch_dist_ = QLineF(points[0].position(), points[1].position()).length();
    }
}

void SphereView::handle_touch_update(QTouchEvent* event) {
    const auto& points = event->points();
    if (points.size() == 1) {
        // 单指拖动
        double dx = points[0].position().x() - last_touch_x_;
        double dy = points[0].position().y() - last_touch_y_;
        center_ra_ -= dx * ROTATE_SPEED;
        center_dec_ = std::clamp(center_dec_ + dy * ROTATE_SPEED, -90.0, 90.0);
        while (center_ra_ < 0.0) center_ra_ += 360.0;
        while (center_ra_ >= 360.0) center_ra_ -= 360.0;
        last_touch_x_ = points[0].position().x();
        last_touch_y_ = points[0].position().y();
        emit view_changed(center_ra_, center_dec_, zoom_);
    } else if (points.size() == 2) {
        // 双指捏合缩放
        double dist = QLineF(points[0].position(), points[1].position()).length();
        if (last_touch_dist_ > 0) {
            double scale = dist / last_touch_dist_;
            zoom_ = std::clamp(zoom_ * scale, MIN_ZOOM, MAX_ZOOM);
            fov_deg_ = 180.0 / zoom_;
            emit view_changed(center_ra_, center_dec_, zoom_);
        }
        last_touch_dist_ = dist;
    }
    request_render();
}

void SphereView::handle_touch_end(QTouchEvent* event) {
    is_touching_ = false;
    last_touch_dist_ = 0.0;
}

// ============================================================================
// 渲染参数构建
// ============================================================================

RenderParams SphereView::build_render_params() {
    RenderParams p;
    p.mode = render_mode_;  // .hiss=HISS_POLYGON, .hcsd=SPHERE
    p.view.center_ra = center_ra_;
    p.view.center_dec = center_dec_;
    p.view.zoom = zoom_;
    p.view.fov_deg = fov_deg_;
    p.stf = stf_params_;
    p.data_min = data_min_;
    p.data_max = data_max_;
    p.no_data_value = no_data_value_;
    p.viewport_w = width();
    p.viewport_h = height();
    return p;
}

// ============================================================================
// 屏幕坐标 → 天球坐标
// ============================================================================

void SphereView::screen_to_sky(int x, int y, double& ra, double& dec) {
    // 球面投影逆变换: 屏幕中心 → (center_ra_, center_dec_)
    // 简化实现: 用 FOV/画布尺寸做线性映射 (球面投影小角度近似)
    // 完整实现需透视投影 + lookAt 逆矩阵, 但状态栏坐标显示精度要求不高

    int w = width();
    int h = height();
    if (w <= 0 || h <= 0) {
        ra = center_ra_;
        dec = center_dec_;
        return;
    }

    // 屏幕中心相对偏移 (像素)
    double dx = static_cast<double>(x) - w / 2.0;
    double dy = static_cast<double>(y) - h / 2.0;

    // 像素 → 度 (用 FOV/画布尺寸近似)
    double deg_per_pixel_x = fov_deg_ / static_cast<double>(w);
    double deg_per_pixel_y = fov_deg_ / static_cast<double>(h);

    // RA 方向 cos(dec) 修正
    double cos_dec = std::cos(center_dec_ * M_PI / 180.0);
    if (std::abs(cos_dec) < 1e-6) cos_dec = 1e-6;

    // 屏幕坐标 → 天球坐标 (近似)
    ra = center_ra_ - dx * deg_per_pixel_x / cos_dec;
    dec = center_dec_ + dy * deg_per_pixel_y;

    // 归一化
    while (ra < 0.0) ra += 360.0;
    while (ra >= 360.0) ra -= 360.0;
    if (dec < -90.0) dec = -90.0;
    if (dec > 90.0) dec = 90.0;
}
