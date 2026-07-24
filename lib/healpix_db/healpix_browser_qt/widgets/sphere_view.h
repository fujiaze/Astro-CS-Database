// sphere_view.h - 球面 3D 渲染 widget (healpix_browser_qt)
// 功能: 球面渲染(.hiss像素多边形/.hcsd球面网格), 球心相机, 切平面导航+滚轮/+-键FOV+箭头键朝向
// 用途: 显示天球 HEALPix 数据, 从球心向外看, 旋转朝向, 调整FOV
// 依赖: Qt6::Gui (QMouseEvent/QWheelEvent/QKeyEvent/QTouchEvent), core/ (GLRenderer + HealpixMath)
// 设计文档: docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md §3.3, §5.2, §5.3
//           docs/superpowers/specs/2026-07-14-sphere-view-tangent-plane-navigation.md
// 交互: 球心相机, 相机朝向由(center_ra, center_dec)决定
//   拖动/箭头键: 切平面离散旋转(gnomonic逆变换), 极区无奇点
//   滚轮/+-键: 改变FOV(放大/缩小视场)
//   触摸: 单指拖动, 双指捏合=缩放
//   经纬线: 30°网格, 可通过set_grid_visible开关

#ifndef SPHERE_VIEW_H
#define SPHERE_VIEW_H

#include "abstract_view.h"

class QTouchEvent;

class SphereView : public AbstractView {
    Q_OBJECT

public:
    explicit SphereView(QWidget* parent = nullptr);

    void get_view_params(ViewParams& out) const override;

    // 重置视角到默认 (RA=0, Dec=0, FOV=60°)
    void reset_view();

    // .hiss 模式: 根据数据 bbox 设置初始视角（对准数据中心，FOV 自适应使 patch 占视野 60%）
    void set_initial_view_from_bbox();

    // 从外部数据 bbox 设置初始视角 (不依赖 renderer, 用于 SPHERE 模式)
    // ra/dec: 数据中心, w/h: 数据宽高 (度)
    void set_initial_view_from_data(double ra, double dec, double w, double h);

    // 设置渲染模式（.hiss=HISS_POLYGON, .hcsd=SPHERE）
    void set_render_mode(RenderMode mode) { render_mode_ = mode; }

    // 经纬线网格开关
    void set_grid_visible(bool visible);

    // 放大/缩小 (FOV /= FOV_STEP 放大, FOV *= FOV_STEP 缩小)
    void zoom_in();
    void zoom_out();

protected:
    void handle_mouse_press(QMouseEvent* event) override;
    void handle_mouse_move(QMouseEvent* event) override;
    void handle_mouse_release(QMouseEvent* event) override;
    void handle_wheel(QWheelEvent* event) override;
    RenderParams build_render_params() override;

    // 键盘事件: 箭头键朝向 + +-键 FOV
    void keyPressEvent(QKeyEvent* event) override;

    // 重载 paintGL: 首次渲染后检查 pending_initial_view_（mesh 构建完成后重新设置视角）
    void paintGL() override;

    // 触摸事件 (重载, 非 final, 子类可进一步定制)
    bool event(QEvent* event) override;

private:
    // 视角状态 (球心相机, 双向量四元数导航)
    double center_ra_;       // 相机朝向赤经 (度, 从 forward 反算, 用于日志/screen_to_sky)
    double center_dec_;      // 相机朝向赤纬 (度)
    double fov_deg_;         // 视场角 (度), 小=放大, 大=缩小

    // 相机朝向双向量 (自由滚动, 左右不旋转)
    // forward: 视线方向 (单位向量, 从球心向外)
    // up: 画面上方 (单位向量, 与 forward 正交)
    // 左右拖动: forward 绕 up 旋转, up 不变 → 画面不旋转
    // 上下拖动: forward+up 都绕 right 旋转, 保持正交
    double forward_x_ = 1.0;
    double forward_y_ = 0.0;
    double forward_z_ = 0.0;
    double up_x_ = 0.0;
    double up_y_ = 0.0;
    double up_z_ = 1.0;

    // 渲染模式（.hiss=HISS_POLYGON, .hcsd=SPHERE）
    RenderMode render_mode_ = RenderMode::SPHERE;

    // 经纬线网格可见性
    bool grid_visible_ = false;

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

    // FOV 限制
    static constexpr double MIN_FOV = 0.01;   // 最小FOV(最大放大, 去掉限制)
    static constexpr double MAX_FOV = 50.0;   // 最大FOV(限制50°避免球面旋转畸变)
    // 滚轮FOV灵敏度
    static constexpr double FOV_SPEED = 0.0015;
    // 拖动灵敏度: 拖动速度 = FOV * DRAG_RATIO (每像素拖动旋转FOV的百分比)
    static constexpr double DRAG_RATIO = 0.003;
    // 箭头键步进: 每次按键旋转 FOV * ARROW_RATIO
    static constexpr double ARROW_RATIO = 0.1;
    // +-键 FOV 步进倍率
    static constexpr double FOV_STEP = 1.2;

    // 触摸事件处理
    void handle_touch_begin(QTouchEvent* event);
    void handle_touch_update(QTouchEvent* event);
    void handle_touch_end(QTouchEvent* event);

    // 拖动旋转 (赤道仪相机, 最简方案):
    //   yaw (左右): ra 增量, 绕世界 Z 轴
    //   pitch (上下): dec 增量, 绕 right 轴
    //   up 始终 north-up 重算 (绝不携带, 绝不 roll)
    //   速度 = FOV × DRAG_RATIO, 抓画面拖模式
    void apply_drag_rotation(int dx, int dy);

    // 从 forward_ 反算 (ra, dec) 并更新 center_ra_/center_dec_
    void update_ra_dec_from_forward();

    // 初始化 forward_/up_ 为 north-up (给定 ra, dec)
    // forward = (cos_dec*cos_ra, cos_dec*sin_ra, sin_dec)
    // up = 球面切平面 north 基向量, 极区兜底用 (0,1,0)
    void init_forward_up_north_up(double ra_deg, double dec_deg);

    // 屏幕坐标 → 天球坐标 (用于状态栏显示鼠标处坐标)
    // 实现: 球面投影逆变换 (透视投影 + lookAt 矩阵逆)
    void screen_to_sky(int x, int y, double& ra, double& dec);
};

#endif // SPHERE_VIEW_H
