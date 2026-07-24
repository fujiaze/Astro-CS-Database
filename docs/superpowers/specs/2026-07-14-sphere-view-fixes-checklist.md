# Checklist: SphereView 视角与像素渲染修复

## 修复 1: 拖动方向
- [ ] sphere_view.cpp handle_mouse_move: eta_deg = dy * drag_speed
- [ ] sphere_view.cpp handle_touch_update: eta_deg = dy * drag_speed
- [ ] 箭头键保持视线导引语义 (不改)

## 修复 2: 自由滚动
- [ ] sphere_view.h: 新增 QVector3D camera_up_ 成员
- [ ] sphere_view.h: 新增 init_north_up() 私有方法声明
- [ ] sphere_view.cpp: 实现 init_north_up(ra,dec) 计算 north-up 向量
- [ ] sphere_view.cpp: reset_view 调用 init_north_up
- [ ] sphere_view.cpp: set_initial_view_from_bbox 调用 init_north_up
- [ ] sphere_view.cpp: apply_tangent_displacement 用 Rodrigues 旋转更新 camera_up_
- [ ] sphere_view.cpp: build_render_params 填充 up_x/y/z
- [ ] gl_renderer.h: ViewParams 加 up_x/up_y/up_z
- [ ] gl_renderer.cpp render_hiss_polygon: 用 params.view.up_* 代替重算
- [ ] gl_renderer.cpp render_grid: 用 params.view.up_*

## 修复 3: 菱形像素
- [ ] gl_renderer.cpp build_hiss_polygon_mesh: 用切平面基构造菱形角点
- [ ] 删除 d_ra = pix_size/cos_dec 矩形近似
- [ ] 三角形顺序: 下→右→上, 下→上→左

## 验证
- [ ] 编译通过 (MSYS2 MinGW)
- [ ] 启动程序加载 .hiss
- [ ] 上下拖动方向正确
- [ ] 左右拖动无旋转
- [ ] 极区拖动无奇点
- [ ] 放大见菱形像素
- [ ] 无摩尔纹
- [ ] 截图复查网格
