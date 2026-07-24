# SphereView 视角与像素渲染修复

日期: 2026-07-14
类型: debug + 工程重构
状态: 已实施

## 1. 症状

1. 上下拖动方向相反（鼠标上移画面却向北走）
2. 左右拖动产生旋转感（极区 up 向量重算导致画面旋转）
3. 放大后看不到 HEALPix 菱形像素，出现摩尔纹
4. 天球网格畸变严重（待其他修复后复查）
5. 不流畅（no_data 像素也参与渲染）

## 2. 根因

### 根因 A: eta 符号反
`sphere_view.cpp` handle_mouse_move:
```cpp
double eta_deg = -dy * drag_speed;  // ❌ 反了
```
"抓画面拖"要求: 鼠标上移 dy<0 → 看更南 → eta<0。当前 `-dy` 使 dy<0→eta>0 向北。

### 根因 B: 帧间重算 up 导致旋转
原方案用 Rodrigues 旋转 + 携带式 up，但左右拖动时 forward 沿赤纬线移动，
旋转轴≈Z 轴，up 绕 Z 旋转导致画面倾斜。后改为双向量四元数（forward 绕 up / forward+up 绕 right）
仍存在左右旋转感。最终改用虚拟轨迹球方案。

### 根因 C: 矩形近似 + cos_dec 发散
`gl_renderer.cpp` build_hiss_polygon_mesh 用矩形角点:
```cpp
double d_ra = pix_size / cos_dec;  // 极区发散
corners = {ra±d_ra/2, dec±d_dec/2}  // 矩形非菱形
```
HEALPix 像素是菱形（钻石形），矩形拼接成连续面看不出像素边界，且极区 d_ra 发散致畸变/摩尔纹。

### 根因 D: no_data 像素参与渲染
所有像素（包括 value<=0 的无效像素）都生成多边形，浪费 GPU 资源，导致不流畅。

## 3. 修复方案（最终）

### 修复 1: 翻转 eta 符号
`sphere_view.cpp` handle_mouse_move / handle_touch_update:
```cpp
double eta_deg = dy * drag_speed;  // 去负号, 抓画面拖
```

### 修复 2: 虚拟轨迹球导航（最终方案）
**核心思想**: 屏幕拖动 (dx, dy) 统一为单个旋转轴 + 角度，不分 yaw/pitch，画面不旋转。

**算法** (sphere_view.cpp apply_drag_rotation):
1. forward + up 双向量（north-up 初始化: up = 球面 north 切平面基, 极区兜底 Y 轴）
2. right = forward × up（画面右方, 右手系）
3. 视线位移 = -dx*right + dy*up（抓画面拖: 视线反向于画面位移）
4. 旋转轴 axis = forward × 视线位移 = dy*right - dx*up
5. 旋转角度 angle = |dx,dy| × FOV × DRAG_RATIO (rad)
6. forward 和 up 同时绕 axis 旋转相同角度（保持正交, 无 roll）

**方向验证**:
- 鼠标右移 dx>0, dy=0: axis = -dx*up = -up → forward 绕 -up 转 → 向西转 ✓ (抓画面拖)
- 鼠标上移 dx=0, dy<0: axis = dy*right = -right → forward 绕 -right 转 → 向南转 ✓ (抓画面拖)

**废弃方案**:
- 切平面 gnomonic 逆变换：极区无奇点但左右旋转感
- Rodrigues 旋转 + 携带式 up：左右拖动 up 绕 Z 轴旋转
- 双向量四元数 (forward 绕 up / forward+up 绕 right)：左右仍有旋转感

### 修复 3: 球面切平面菱形像素
`gl_renderer.cpp` build_hiss_polygon_mesh:
对每像素:
1. 中心 (ra,dec) → 笛卡尔 c = (cos_dec*cos_ra, cos_dec*sin_ra, sin_dec)
2. 切平面基:
   - east = (-sin_ra, cos_ra, 0)
   - north = (-sin_dec*cos_ra, -sin_dec*sin_ra, cos_dec)
3. 半边长 h = pix_size_rad / 2
4. 4 角点 (菱形十字方向):
   - 下 = c - h*north
   - 右 = c + h*east
   - 上 = c + h*north
   - 左 = c - h*east
5. 三角形顺序: 下→右→上, 下→上→左
6. 每角点归一化投影回单位球

### 修复 4: no_data 不渲染
`gl_renderer.cpp` build_hiss_polygon_mesh:
- 跳过 value<=0 的像素（与片元着色器 uNoData 阈值一致）
- 日志输出 skipped_no_data 统计

### 修复 5: renderer 用传入 forward/up
`gl_renderer.cpp` render_hiss_polygon / render_grid:
- 不再从 ra/dec 重算 forward，直接用 widget 层传入的 forward_*
- 消除 widget 和 renderer 之间的状态不一致

## 4. 改动文件

| 文件 | 改动 |
|---|---|
| core/browser_backend.h | ViewParams 加 forward_x/y/z + up_x/y/z 字段 |
| widgets/sphere_view.h | forward_/up_ 双向量成员 + apply_drag_rotation/update_ra_dec_from_forward/init_forward_up_north_up 声明 |
| widgets/sphere_view.cpp | 切平面导航 → 虚拟轨迹球, 删除 apply_tangent_displacement/init_north_up |
| core/gl_renderer.cpp | build_hiss_polygon_mesh 菱形+跳过no_data, render_hiss_polygon/render_grid 用传入 forward/up |

## 5. 验收标准

- [x] 上下拖动: 鼠标上移→画面上移→看更南内容 (抓画面模式)
- [x] 左右拖动: 无旋转感 (虚拟轨迹球统一旋转)
- [x] 极区拖动: 无奇点 (forward/up 双向量, 极区兜底 Y 轴)
- [x] 放大后: 能看到一个个菱形(钻石形) HEALPix 像素
- [x] 无摩尔纹
- [x] 编译通过, 无回归
- [x] no_data 区域不渲染, 提升流畅性
- [x] 网格改善 (用户确认)
