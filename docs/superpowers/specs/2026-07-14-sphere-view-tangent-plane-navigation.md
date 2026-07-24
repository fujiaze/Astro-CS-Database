# HEALPix 浏览器球心相机切平面导航重构 Spec

**日期**: 2026-07-14
**任务类型**: debug + 工程重构
**影响范围**: 仅 `.hiss` 多边形模式（SphereView）
**状态**: 待确认

## 1. 症状

- `.hiss` 多边形模式加载后画面显示正常（含已知摩尔纹）
- 南北拖动正常
- **东西拖动出现旋转感**，极区附近失效
- 当前实现: [sphere_view.cpp#L150-L153](file:///f:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp#L150-L153) 直接 `center_ra_ -= dx * drag_speed`

## 2. 根因

当前视角控制用 `(center_ra, center_dec)` 直接增减：

```cpp
center_ra_ -= dx * drag_speed;   // 东西拖动
center_dec_ -= dy * drag_speed;  // 南北拖动
```

问题：
1. **极点奇点**: RA 在 Dec=±90° 无定义（所有 RA 指向同一点）
2. **cos(dec) 因子缺失**: 相同 RA 增减对应实际天球角度 = `ΔRA × cos(Dec)`，极区 `cos(Dec)→0` 导致东西拖动"变钝"
3. **旋转感**: 拖动东西时，视线沿赤纬线扫，但用户感觉画面在"转"——因为赤纬线在极区收敛，视觉上像绕极点旋转

HEALPix 解决的是**数据索引**奇点（球面像素化均匀），**视角控制**仍需用球面几何处理极点。

## 3. 修复方案

**切平面离散旋转（gnomonic 逆变换）**

### 3.1 核心思路

- 状态仍为 `(center_ra, center_dec, fov_deg)`
- 拖动/箭头键产生**切平面角度位移** `(dx_deg, dy_deg)`
- 用 gnomonic（心射切面）投影逆变换，将切平面位移转换为球面新坐标 `(ra1, dec1)`
- 切平面以 `(center_ra, center_dec)` 为切点，局部坐标系 X=东(Y轴)、Y=北(Z轴)
- 恒定速度: 每次按键/拖动 `dx_deg`/`dy_deg` 是固定角度，对应球面大圆弧固定角度

### 3.2 gnomonic 逆变换公式

给定中心 `(ra0, dec0)`（弧度），切平面位移 `(xi, eta)`（弧度，xi=东向、eta=北向）：

```
rho = sqrt(xi^2 + eta^2)
c = atan(rho)
dec1 = asin(cos(c)*sin(dec0) + eta*sin(c)*cos(dec0)/rho)
ra1 = ra0 + atan2(xi*sin(c), rho*cos(dec0)*cos(c) - eta*sin(dec0)*sin(c))
```

边界处理:
- `rho < 1e-10`: `dec1=dec0, ra1=ra0`（无位移）
- `|dec0| > 89.999°`: clamp 到 89.999° 避免数值问题
- `dec1` clamp 到 [-89.999°, 89.999°]
- `ra1` 归一化到 [0, 360°)

### 3.3 交互映射

| 输入 | 切平面位移 | 球面效果 |
|---|---|---|
| 鼠标右拖 (dx>0) | xi = -dx × FOV × DRAG_RATIO | 视线向西转（看到的内容向东移） |
| 鼠标下拖 (dy>0) | eta = -dy × FOV × DRAG_RATIO | 视线向南转 |
| 上箭头 | eta = +FOV × ARROW_RATIO | 视线向北转（按上向上） |
| 下箭头 | eta = -FOV × ARROW_RATIO | 视线向南转 |
| 左箭头 | xi = -FOV × ARROW_RATIO | 视线向西转 |
| 右箭头 | xi = +FOV × ARROW_RATIO | 视线向东转 |
| 滚轮上 | FOV ×= 0.9 | 放大 |
| 滚轮下 | FOV ×= 1.1 | 缩小 |

### 3.4 经纬线网格

- **密度**: RA 每 30° 一条（0,30,60,...,330），Dec 每 30° 一条（-60,-30,0,30,60）
- **样式**: 半透明绿色（RGBA 0,1,0,0.3），线宽 1px
- **开关**: 主窗口工具栏或菜单加 checkbox
- **实现**: CPU 端生成网格顶点（球面采样），独立 VAO/VBO，单独着色器（无 STF，固定颜色）
- **可见范围**: 全天网格，配合球心相机 FOV 自动裁剪

### 3.5 改动文件

| 文件 | 改动 |
|---|---|
| `widgets/sphere_view.h` | 新增 `apply_tangent_displacement(double xi_deg, double eta_deg)` 方法 |
| `widgets/sphere_view.cpp` | 重写 `handle_mouse_move`/`handle_wheel`/`keyPressEvent`/`handle_touch_update`，调用 `apply_tangent_displacement`；删除直接 RA/Dec 增减 |
| `core/gl_renderer.h` | 新增 `render_grid()` 方法、网格 VAO/VBO、着色器 |
| `core/gl_renderer.cpp` | 实现 `render_grid()`：生成 30° 网格顶点 + 独立着色器绘制 |
| `app/main_window.h` | 新增 `QAction* grid_toggle_` |
| `app/main_window.cpp` | 工具栏加"经纬线"开关，连接 SphereView 的 `set_grid_visible(bool)` |

### 3.6 不变项

- 相机模型仍为球心相机（已在上一阶段实现）
- `gl_renderer.cpp` 的 `render_hiss_polygon`/`render_sphere` MVP 矩阵不变
- STF 着色器不变
- `pending_initial_view_` 机制不变

## 4. 验收标准

1. **极区东西拖动正常**: 在 Dec=±80° 附近东西拖动，画面平滑移动无旋转感
2. **箭头键恒定速度**: 上箭头每次按下视线恒定上移 `FOV×0.1` 度，极区不卡顿
3. **经纬线开关**: checkbox 切换可见性，30° 网格正确对齐天球
4. **初始视角自适应**: `.hiss` 加载后 FOV=`max_dim/0.6`，居中 bbox
5. **编译通过**: MSYS2 MinGW g++ -std=c++17 无错误
6. **无回归**: `.hiss` 渲染、STF、摩尔纹状态与重构前一致

## 5. 不做项

- 不改 `.hcsd` 天球模式（用户未要求）
- 不改 STF 着色器
- 不加 roll（画面始终保持正立）
- 不加坐标系切换（仅赤道坐标系）
