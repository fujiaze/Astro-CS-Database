// sphere_view.cpp - 球面 3D 渲染 widget 实现 (healpix_browser_qt)
// 功能: 球心相机渲染 (.hiss像素多边形/.hcsd球面网格), 双向量四元数导航+滚轮/+-键改FOV+箭头键朝向
// 用途: 显示天球 HEALPix 数据, 相机在球心(0,0,0)向外看, 遵守赤道坐标系约定
//   坐标系: 右手系 X=春分点(RA=0,Dec=0) Y=RA=90° Z=北天极
//   球面→笛卡尔: x=cos(dec)cos(ra) y=cos(dec)sin(ra) z=sin(dec)
// 导航: 双向量四元数 (forward + up), Rodrigues 旋转
//   左右拖动: forward 绕 up 旋转 (up 不变 → 画面不旋转)
//   上下拖动: forward+up 都绕 right=forward×up 旋转 (保持正交, 抓画面拖模式)
//   速度 = FOV × DRAG_RATIO
// 依赖: Qt6::Gui, core/ (GLRenderer + HealpixMath)

#include "sphere_view.h"
#include "logger.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
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
      fov_deg_(60.0),  // 缺省 FOV, 加载数据后由 set_initial_view_from_bbox 覆盖
      is_dragging_(false),
      last_mouse_x_(0),
      last_mouse_y_(0),
      is_touching_(false),
      last_touch_x_(0.0),
      last_touch_y_(0.0),
      last_touch_dist_(0.0) {
    // 启用触摸事件 + 键盘焦点
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setFocusPolicy(Qt::StrongFocus);  // 允许键盘事件
    setCursor(Qt::OpenHandCursor);
}

// ============================================================================
// 视角控制
// ============================================================================

void SphereView::reset_view() {
    center_ra_ = 0.0;
    center_dec_ = 0.0;
    // 按渲染模式设缺省 FOV:
    //   HISS_POLYGON(.hiss): 50° 缺省 (会被 set_initial_view_from_bbox 覆盖)
    //   SPHERE(.hcsd): 50° 缺省 (限制最大FOV避免球面旋转畸变)
    if (render_mode_ == RenderMode::SPHERE) {
        fov_deg_ = 50.0;
    } else {
        fov_deg_ = 50.0;
    }
    // 重置 forward/up 为 north-up (ra=0, dec=0)
    init_forward_up_north_up(center_ra_, center_dec_);
    LOG_INFO("reset_view: center=(0,0) fov=%.1f mode=%d forward=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f)",
             fov_deg_, static_cast<int>(render_mode_),
             forward_x_, forward_y_, forward_z_,
             up_x_, up_y_, up_z_);
    emit view_changed(center_ra_, center_dec_, fov_deg_);
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

    set_initial_view_from_data(ra, dec, w, h);
}

void SphereView::set_initial_view_from_data(double ra, double dec, double w, double h) {
    center_ra_ = ra;
    center_dec_ = dec;
    // FOV 自适应: 使 patch 占视野约 85% (数据更大更突出)
    // 球心相机看球面, 顶点距相机=1.0, patch 角宽度≈max_dim 度
    // 要让 patch 占视野 85%: fov = max_dim / 0.85
    double max_dim = std::max(w, h);
    if (max_dim > 0.001) {
        fov_deg_ = std::clamp(max_dim / 0.85, MIN_FOV, MAX_FOV);
    } else {
        fov_deg_ = 60.0;
    }
    // 初始化 forward/up 为 north-up (ra=data_center, dec=data_center)
    init_forward_up_north_up(center_ra_, center_dec_);
    LOG_INFO("set_initial_view_from_data: center=(%.4f,%.4f) size=%.4fx%.4f fov=%.3f forward=(%.3f,%.3f,%.3f) up=(%.3f,%.3f,%.3f)",
             center_ra_, center_dec_, w, h, fov_deg_,
             forward_x_, forward_y_, forward_z_,
             up_x_, up_y_, up_z_);
    emit view_changed(center_ra_, center_dec_, fov_deg_);
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
    out.zoom = 1.0;  // 兼容字段, SphereView 用 fov_deg_ 控制缩放
    out.fov_deg = fov_deg_;
}

// ============================================================================
// 鼠标交互 (球心相机: 拖动改变朝向, 速度=FOV×DRAG_RATIO)
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

    // 双向量四元数导航 (球心相机, 抓画面拖模式):
    //   左右: forward 绕 up 旋转 (up 不变, 画面不旋转)
    //   上下: forward+up 绕 right=forward×up 旋转 (抓画面拖模式)
    // 速度 = FOV × DRAG_RATIO
    apply_drag_rotation(dx, dy);

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
    // 滚轮改 FOV: 滚轮向上(放大) → FOV 减小, 滚轮向下(缩小) → FOV 增大
    // fov_deg *= exp(-delta_y × FOV_SPEED)
    double delta_y = event->angleDelta().y();
    double factor = std::exp(-delta_y * FOV_SPEED);
    fov_deg_ = std::clamp(fov_deg_ * factor, MIN_FOV, MAX_FOV);

    emit view_changed(center_ra_, center_dec_, fov_deg_);
    request_render();
}

// ============================================================================
// 键盘交互 (箭头键精细朝向 + +-键 FOV)
// ============================================================================

void SphereView::keyPressEvent(QKeyEvent* event) {
    int key = event->key();
    bool handled = true;

    // 箭头键: 旋转步进 = FOV × ARROW_RATIO (每次按键产生 FOV 的 10% 旋转)
    // 转换为像素位移传给 apply_drag_rotation
    int dx = 0, dy = 0;
    double step_pixels = ARROW_RATIO / DRAG_RATIO;  // 步进对应的像素数
    int step = static_cast<int>(step_pixels + 0.5);
    if (step < 1) step = 1;

    switch (key) {
        case Qt::Key_Left:
            dx = -step;  // 向西转
            break;
        case Qt::Key_Right:
            dx = +step;  // 向东转
            break;
        case Qt::Key_Up:
            dy = -step;  // 向北转 (按上向上, 与鼠标上移方向一致)
            break;
        case Qt::Key_Down:
            dy = +step;  // 向南转
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:  // = 键 (Shift 不按时)
            // 放大: FOV 减小
            fov_deg_ /= FOV_STEP;
            break;
        case Qt::Key_Minus:
            // 缩小: FOV 增大
            fov_deg_ *= FOV_STEP;
            break;
        default:
            handled = false;
            break;
    }

    if (handled) {
        // 箭头键产生旋转位移
        if (dx != 0 || dy != 0) {
            apply_drag_rotation(dx, dy);
        } else {
            // 仅 FOV 变化 (+-键)
            fov_deg_ = std::clamp(fov_deg_, MIN_FOV, MAX_FOV);
            emit view_changed(center_ra_, center_dec_, fov_deg_);
        }
        request_render();
    } else {
        AbstractView::keyPressEvent(event);
    }
}

// ============================================================================
// 放大/缩小 (工具栏按钮调用)
// ============================================================================

void SphereView::zoom_in() {
    fov_deg_ /= FOV_STEP;
    fov_deg_ = std::clamp(fov_deg_, MIN_FOV, MAX_FOV);
    emit view_changed(center_ra_, center_dec_, fov_deg_);
    request_render();
}

void SphereView::zoom_out() {
    fov_deg_ *= FOV_STEP;
    fov_deg_ = std::clamp(fov_deg_, MIN_FOV, MAX_FOV);
    emit view_changed(center_ra_, center_dec_, fov_deg_);
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
        // 单指拖动: 双向量旋转 (抓画面模式), 速度 = FOV × DRAG_RATIO
        double dx = points[0].position().x() - last_touch_x_;
        double dy = points[0].position().y() - last_touch_y_;
        apply_drag_rotation(static_cast<int>(dx), static_cast<int>(dy));
        last_touch_x_ = points[0].position().x();
        last_touch_y_ = points[0].position().y();
    } else if (points.size() == 2) {
        // 双指捏合: 改 FOV (捏合放大 → dist 减小 → FOV 减小)
        double dist = QLineF(points[0].position(), points[1].position()).length();
        if (last_touch_dist_ > 0) {
            double scale = last_touch_dist_ / dist;  // 捏合收拢 scale>1 → FOV 减小
            fov_deg_ = std::clamp(fov_deg_ * scale, MIN_FOV, MAX_FOV);
            emit view_changed(center_ra_, center_dec_, fov_deg_);
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
    p.view.zoom = 1.0;  // 兼容字段, 不使用
    p.view.fov_deg = fov_deg_;
    // 双向量四元数导航: 传 forward + up (renderer 直接使用不重算)
    p.view.forward_x = forward_x_;
    p.view.forward_y = forward_y_;
    p.view.forward_z = forward_z_;
    p.view.up_x = up_x_;
    p.view.up_y = up_y_;
    p.view.up_z = up_z_;
    p.stf = stf_params_;
    p.data_min = data_min_;
    p.data_max = data_max_;
    p.no_data_value = no_data_value_;
    // viewport 用物理像素 (高DPI屏幕 devicePixelRatio>1 时, framebuffer 是物理像素大小)
    qreal dpr = devicePixelRatio();
    if (dpr < 1.0) dpr = 1.0;
    p.viewport_w = (int)(width() * dpr);
    p.viewport_h = (int)(height() * dpr);
    p.grid_visible = grid_visible_;
    return p;
}

// ============================================================================
// init_forward_up_north_up - 初始化 forward_/up_ 为 north-up (给定 ra, dec)
// forward = (cos_dec*cos_ra, cos_dec*sin_ra, sin_dec)
// up = 球面切平面 north 基向量 (-sin_dec*cos_ra, -sin_dec*sin_ra, cos_dec)
// 极区 (|dec|≈90°) 兜底: up 退化, 用 (0,1,0) 替代
// ============================================================================

void SphereView::init_forward_up_north_up(double ra_deg, double dec_deg) {
    double ra = ra_deg * M_PI / 180.0;
    double dec = dec_deg * M_PI / 180.0;
    double cd = std::cos(dec);
    double sd = std::sin(dec);
    double cr = std::cos(ra);
    double sr = std::sin(ra);
    forward_x_ = cd * cr;
    forward_y_ = cd * sr;
    forward_z_ = sd;
    // 球面 north 切平面基向量: ∂forward/∂dec 归一化
    double nx = -sd * cr;
    double ny = -sd * sr;
    double nz = cd;
    double nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nlen < 1e-10) {
        // 极区兜底: dec=±90° 时 north 退化, 用 (0,1,0) 替代
        up_x_ = 0.0; up_y_ = 1.0; up_z_ = 0.0;
    } else {
        up_x_ = nx / nlen;
        up_y_ = ny / nlen;
        up_z_ = nz / nlen;
    }
}

// ============================================================================
// apply_drag_rotation - 赤道仪相机 (最简方案)
//   yaw (左右): center_ra_ 增量, 绕极轴 Z
//   pitch (上下): center_dec_ 增量, 绕赤纬轴 east
//   up 始终 north-up (从 ra/dec 重算, 绝不携带)
//   速度 = FOV × DRAG_RATIO, 抓画面拖模式
// ============================================================================

void SphereView::apply_drag_rotation(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        emit view_changed(center_ra_, center_dec_, fov_deg_);
        return;
    }

    double drag_speed = fov_deg_ * DRAG_RATIO;  // 度/像素

    // yaw (左右): ra 旋转, 绕极轴 Z
    //   抓画面拖: 鼠标右移 dx>0 → 画面向右(西, side=forward×up 指向-Y/西) → 视线向东 → ra 增大
    center_ra_ += dx * drag_speed;

    // pitch (上下): dec 旋转, 绕赤纬轴 east
    //   抓画面拖: 鼠标下移 dy>0 → 看更北 → dec 增大
    center_dec_ += dy * drag_speed;

    // clamp dec 避免极区数值问题
    if (center_dec_ > 89.9) center_dec_ = 89.9;
    if (center_dec_ < -89.9) center_dec_ = -89.9;

    // 归一化 ra 到 [0, 360)
    while (center_ra_ < 0.0) center_ra_ += 360.0;
    while (center_ra_ >= 360.0) center_ra_ -= 360.0;

    // 从 ra/dec 重算 forward/up (north-up, 绝不携带)
    init_forward_up_north_up(center_ra_, center_dec_);

    LOG_DEBUG("apply_drag_rotation: dx=%d dy=%d -> ra=%.4f dec=%.4f",
             dx, dy, center_ra_, center_dec_);

    emit view_changed(center_ra_, center_dec_, fov_deg_);
}

// ============================================================================
// update_ra_dec_from_forward - 从 forward_ 反算 (ra, dec)
// dec = asin(forward_z), ra = atan2(forward_y, forward_x)
// ============================================================================

void SphereView::update_ra_dec_from_forward() {
    double fz = forward_z_;
    if (fz > 1.0) fz = 1.0;
    if (fz < -1.0) fz = -1.0;
    double dec_rad = std::asin(fz);
    double ra_rad = std::atan2(forward_y_, forward_x_);

    center_dec_ = dec_rad * 180.0 / M_PI;
    center_ra_ = ra_rad * 180.0 / M_PI;
    // 归一化 ra 到 [0, 360)
    while (center_ra_ < 0.0) center_ra_ += 360.0;
    while (center_ra_ >= 360.0) center_ra_ -= 360.0;
}

// ============================================================================
// 经纬线网格开关
// ============================================================================

void SphereView::set_grid_visible(bool visible) {
    if (grid_visible_ != visible) {
        grid_visible_ = visible;
        LOG_INFO("set_grid_visible: %s", visible ? "ON" : "OFF");
        request_render();
    }
}

// ============================================================================
// 屏幕坐标 → 天球坐标 (状态栏显示用, 近似)
// ============================================================================

void SphereView::screen_to_sky(int x, int y, double& ra, double& dec) {
    // 球心相机透视投影近似: 屏幕中心 → (center_ra_, center_dec_)
    // 简化实现: 用 FOV/画布尺寸做线性映射 (小角度近似)
    // 完整实现需透视投影 + lookAt 逆矩阵, 状态栏精度要求不高

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

    // 像素 → 度 (用 FOV/画布尺寸近似, 取较小边作基准)
    double ref_size = static_cast<double>(std::min(w, h));
    double deg_per_pixel = fov_deg_ / ref_size;

    // RA 方向 cos(dec) 修正
    double cos_dec = std::cos(center_dec_ * M_PI / 180.0);
    if (std::abs(cos_dec) < 1e-6) cos_dec = 1e-6;

    // 屏幕坐标 → 天球坐标 (近似)
    //   鼠标右移 dx>0 → RA 增大 (向右看天球东方)
    //   鼠标上移 dy<0 → Dec 增大 (向上看天球北方)
    ra = center_ra_ + dx * deg_per_pixel / cos_dec;
    dec = center_dec_ - dy * deg_per_pixel;

    // 归一化
    while (ra < 0.0) ra += 360.0;
    while (ra >= 360.0) ra -= 360.0;
    if (dec < -90.0) dec = -90.0;
    if (dec > 90.0) dec = 90.0;
}
