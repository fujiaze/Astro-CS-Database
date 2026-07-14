# healpix_browser_qt 模块开发 Memory

## 模块定位
- 替代 healpix_browser_cpp（C++ HTTP 后端）+ healpix_browser_web（WebGL 前端）
- 三层架构：core/（无 Qt）→ widgets/（Qt6）→ app/（demo exe）
- 消除 HTTP + base64 通讯开销，C++ 内存直传 OpenGL 纹理

## 设计文档
- 核心算法：docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md
- UI 前端：docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md
- 实现计划：docs/superpowers/plans/2026-07-13-cpp-qt-browser.md

## 当前进度
- [x] Task 1: 模块初始化与目录结构
- [x] Task 2: HealpixMath（球面坐标转换，2026-07-13）
- [x] Task 3: STFEngine（显示拉伸引擎，2026-07-13）
- [x] Task 4: BrowserBackend（数据加载与按需子叶，2026-07-13）
- [x] Task 5: GLRenderer（OpenGL 渲染核心，2026-07-13）
- [x] Task 6: AbstractView（Qt widget 基类，2026-07-14）
- [x] Task 7: SingleFrameView（单帧 2D 切面投影 widget，2026-07-14）
- [x] Task 8: SphereView（球面 3D 渲染 widget，2026-07-14）
- [x] Task 9: MainWindow（主窗口，2026-07-14）
- [x] Task 10: STFPanel + main.cpp（demo exe，2026-07-14）
- [x] Task 11: demo 验证（2026-07-14, Qt6 安装+cmake build+运行 .hiss 加载成功）
- [x] Task 13: 归档 WebGL 浏览器 + 文档更新（2026-07-14）
- [x] 视角与像素修复（2026-07-14, 虚拟轨迹球+菱形像素+no_data跳过）
- [x] 视角控制重写（2026-07-14, 赤道仪相机+look_at_matrix 矩阵存储bug修复+菱形像素几何修正）
- [ ] Task 12: 性能验证（待后续优化）

## 视角控制重写（2026-07-14, 最终方案）
**问题**: 虚拟轨迹球方案仍有左右拖动产生 roll（画面绕视线轴旋转感）

**根因 1**: `look_at_matrix` 的 column-major 存储顺序错误（存了 V^T 而非 V）
- 旧: col0=(sx,sy,sz,...), col1=(ux,uy,uz,...), col2=(-fx,-fy,-fz,...) → 转置矩阵
- 新: col0=(sx,ux,-fx,...), col1=(sy,uy,-fy,...), col2=(sz,uz,-fz,...) → 正确
- 验证: forward(1,0,0) 应映射到 cam(0,0,-1); 北天极右拖 y 不变

**根因 2**: 导航逻辑过度复杂（携带 up / 双向量 / 轨迹球）
- 最终方案: **赤道仪相机**（最简）
- yaw (左右): center_ra_ += dx × FOV × DRAG_RATIO (绕极轴 Z, side=forward×up 指向 -Y/西, 画面右=西, 抓画面拖视线向东)
- pitch (上下): center_dec_ += dy × FOV × DRAG_RATIO (绕赤纬轴 east, 抓画面拖鼠标下移看更北)
- up 始终从 ra/dec 重算 north-up（绝不携带, 绝不 roll）
- dec clamp 到 ±89.9° 避免极区数值问题

**根因 3**: 菱形像素 pix_size 公式错误（偏大 4.1 倍, 产生摩尔纹）
- 旧: `pix_size = 4*180/(3*nside) = 240/nside` 度（错误, 这是 nside=1 的像素边长）
- 新: `pix_size = √(π/3)/nside × 180/π ≈ 58.6/nside` 度（HEALPix 等面积正方形边长, 用于 bbox）
- 菱形半对角线: `h = √(π/6)/nside` rad（等面积正方形边长 / √2）

**黑色缝隙修复** (drizzle 已修复, 浏览器外扩系数 1.25 → 1.02):
- 历史方案: drizzle pixfrac=0.8 收缩源像素 80% → 填充 64% 面积 → 留 36% 缝隙, 浏览器外扩菱形 25% (`h × 1.25` ≈ 1/pixfrac=1/0.8) 覆盖缝隙
- 2026-07-14 drizzle 已改为 pixfrac=1.0 (不收缩源像素, 消除固有缝隙), 见 drizzle memory.md
- 浏览器外扩系数同步调整: 1.25 → 1.02 (仅覆盖浮点误差导致的亚像素缝隙, 不再需要 25% 覆盖)
- gl_renderer.cpp build_hiss_polygon_mesh: `h = √(π/6)/nside × 1.02`

**MAX_FOV 限制**:
- 旧: 170°（全天, 球面 yaw 旋转畸变明显）
- 新: 50°（小 FOV 下 yaw≈平移, 消除旋转感, 限制纬度线畸变）

**关键参数**:
- DRAG_RATIO = 0.003 (拖动速度 = FOV × 0.3%/像素)
- FOV 范围 [0.5°, 50°]
- North-up 初始化: 打开文件/reset_view 时 up=球面 north 切平面基

**修改文件**:
- core/gl_renderer.cpp - look_at_matrix column-major 存储修正 + build_hiss_polygon_mesh pix_size/h 公式修正 + 菱形外扩 1.25
- widgets/sphere_view.cpp - apply_drag_rotation 改为赤道仪相机 (直接维护 center_ra_/center_dec_)
- widgets/sphere_view.h - MAX_FOV=50, 注释更新

**验证**:
- Python 模拟确认: 右拖北天极 y 不变 (no roll), 下拖 x 不变 (no yaw)
- 编译成功, 程序启动加载 .hiss 正常
- 用户确认: 左右方向反转修复后所有拖动正常

## 视角与像素修复（2026-07-14, 已被最终方案取代）
**问题**: 上下拖动方向反 + 左右拖动旋转感 + 矩形近似摩尔纹 + 网格畸变 + 不流畅

**修复方案演进**:
1. eta 符号翻转（抓画面拖模式, 上下跟手）
2. Rodrigues 旋转 + 携带式 up（失败, 左右仍旋转）
3. 双向量四元数（forward 绕 up / forward+up 绕 right, 失败, 左右仍旋转）
4. **虚拟轨迹球（最终方案, 成功）**

**虚拟轨迹球算法** (sphere_view.cpp apply_drag_rotation):
- forward + up 双向量（north-up 初始化: up=球面 north 切平面基, 极区兜底 Y 轴）
- 屏幕拖动 (dx, dy) → 单个旋转轴 + 角度
- right = forward × up（画面右方, 右手系）
- 视线位移 = -dx*right + dy*up（抓画面拖: 视线反向于画面位移）
- 旋转轴 axis = forward × 视线位移 = dy*right - dx*up
- 旋转角度 angle = |dx,dy| × FOV × DRAG_RATIO (rad)
- forward 和 up 同时绕 axis 旋转相同角度（保持正交, 无 roll）
- 优点: 左右/上下统一为单个旋转, 不分 yaw/pitch, 画面不旋转

**菱形像素修复** (gl_renderer.cpp build_hiss_polygon_mesh):
- 球面切平面基构造菱形角点（替代矩形近似）
- 中心笛卡尔 c + east/north 切平面基向量
- 4 角点十字方向（下/右/上/左）, 投影回单位球
- 无 cos_dec 发散, 真实 HEALPix 菱形

**no_data 不渲染** (gl_renderer.cpp build_hiss_polygon_mesh):
- 跳过 value<=0 的像素（与片元着色器 uNoData 阈值一致）
- 提升渲染流畅性, 避免无效多边形占用 GPU 资源
- 日志输出 skipped_no_data 统计

**renderer 用传入 forward/up** (gl_renderer.cpp render_hiss_polygon/render_grid):
- 不再从 ra/dec 重算 forward, 直接用 widget 层传入的 forward_*
- 消除 widget 和 renderer 之间的状态不一致

**关键参数**:
- DRAG_RATIO = 0.003 (拖动速度 = FOV × 0.3%/像素)
- FOV 范围 [0.5°, 170°]
- North-up 初始化: 打开文件/reset_view 时 up=球面 north 切平面基

**修改文件**:
- core/browser_backend.h - ViewParams 加 forward_x/y/z 字段
- core/gl_renderer.cpp - render_hiss_polygon/render_grid 用传入 forward + build_hiss_polygon_mesh 跳过 no_data
- widgets/sphere_view.h - 声明 apply_drag_rotation / update_ra_dec_from_forward / init_forward_up_north_up
- widgets/sphere_view.cpp - 切平面导航 → 双向量四元数 → 虚拟轨迹球（最终）


## Task 11 + 13: 验证与归档（2026-07-14）
**Qt6 安装**:
- 通过 MSYS2 pacman 安装 mingw-w64-x86_64-qt6-base
- Qt6 CMake 配置位于 C:/msys64/mingw64/lib/cmake/Qt6

**编译验证**:
- `cmake -B build -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -G "MinGW Makefiles"` 成功
- `cmake --build build` 全部编译成功 (0 error, 0 warning)
- 产物: libhealpix_browser_core.a + libhealpix_browser_qt_widgets.a + healpix_browser_qt.exe (1.15MB)
- 修复 1 个编译错误: sphere_view.h 缺少 `class QTouchEvent;` 前向声明

**运行验证**:
- healpix_browser_qt.exe 启动成功 (PID 30872)
- MainWindowTitle="HEALPix Browser (Qt)", Responding=True, WorkingSet=52MB
- 加载 .hiss 文件 (nside=8192, npix=964279, filter=Red) 未崩溃
- 截图保存: C:\Users\fujia\AppData\Local\Temp\codex-shot-2026-07-13_22-56-01.png (110KB)

**运行时依赖**:
- Qt6 DLLs (C:\msys64\mingw64\bin)
- healpix_io.dll (..\..\healpix_io\) - 必须在 PATH 中
- 启动命令需设置 PATH: `$env:Path = "C:\msys64\mingw64\bin;..\..\healpix_io;$env:Path"`

**归档操作**:
- 创建 lib/healpix_db/archive/ 目录
- 移动 healpix_browser_cpp/ → archive/ (杀掉 2 个旧 browser_cpp.exe 进程后)
- 移动 healpix_browser_web/ → archive/
- 创建 archive/README.md 说明归档原因与移植内容
- 更新 healpix_browser_qt/README.md (完整三层架构说明 + 编译/运行/嵌入指南)
- 保留 healpix_browser/ (PyQt5 旧版) 和 healpix_lod/ 不动 (设计文档规定)

## Task 6-10: UI 层实现详情（2026-07-14）
**新建文件 (8个)**:

**widgets/ 层 (3个文件 + 1个 CMake)**:
- `widgets/abstract_view.h` - QOpenGLWidget 子类基类声明
- `widgets/abstract_view.cpp` - 实现 (OpenGL 上下文初始化/事件转发/数据范围计算/auto_stretch)
- `widgets/single_frame_view.h` - 单帧 2D 切面投影 widget 声明
- `widgets/single_frame_view.cpp` - 实现 (init_view_from_data/drag/wheel zoom [0.5,100]/cos(dec) RA 修正)
- `widgets/sphere_view.h` - 球面 3D 渲染 widget 声明
- `widgets/sphere_view.cpp` - 实现 (drag/wheel/touch_event/screen_to_sky 近似映射)
- `CMakeLists.txt` - 三层构建 (core 静态库 + widgets 静态库 + app demo exe)

**app/ 层 (4个文件)**:
- `app/main_window.h` - QMainWindow 主窗口声明
- `app/main_window.cpp` - 实现 (菜单栏/状态栏/STFPanel dock/文件路由 .hiss→SingleFrame/.hcsd→Sphere/signal-slot)
- `app/stf_panel.h` - STF 控制面板 QDockWidget 声明
- `app/stf_panel.cpp` - 实现 (4 滑块 Shadows/Highlights/Midtones/Compression + 4 预设 linear/sqrt/asinh/log + 自动拉伸按钮)
- `app/main.cpp` - QApplication 入口 (QCommandLineParser 解析可选文件参数)

**关键设计决策**:
- AbstractView 继承 QOpenGLWidget + QOpenGLFunctions_3_3_Core, Q_OBJECT 宏 (需 AUTOMOC)
- mousePressEvent/mouseMoveEvent/mouseReleaseEvent/wheelEvent 声明为 final, 强制子类实现 handle_*
- backend_ 为裸指针 (app 层 MainWindow 拥有所有权)
- renderer_ 为 unique_ptr, view 独占 GLRenderer 实例
- compute_data_range() 在首次 paintGL 或 auto_stretch 时调用:
  - .hiss: 全量遍历求 min/max
  - .hcsd: 采样前 4 个子叶求 min/max (避免全量加载成本)
- auto_stretch() 采样上限 100000 像素 (避免大数据集耗时)
- SingleFrameView 拖动: delta_ra = -dx * sensitivity / cos(dec), delta_dec = +dy * sensitivity
- SphereView 拖动: center_ra -= dx * 0.3, center_dec += dy * 0.3 (clamp [-90, 90])
- SphereView 触摸: 单指拖动 + 双指捏合缩放 (QTouchEvent)
- SphereView screen_to_sky: 球面投影逆变换近似 (FOV/canvas 尺寸线性映射)
- STFPanel 滑块: QSlider 范围 0-1000 (int), 映射到 [0, 1] float, 精度 0.001
- STFPanel update_from_params: 用 blockSignals 避免回环
- MainWindow 文件路由: backend.is_hiss() → SingleFrameView, backend.is_hcsd() → SphereView
- MainWindow 命令行参数: QMetaObject::invokeMethod QueuedConnection 延迟到事件循环后打开

**编译依赖**:
- Qt6::Core/Gui/Widgets/OpenGLWidgets (需 MSYS2: pacman -S mingw-w64-x86_64-qt6-base)
- OpenGL 3.3 Core Profile
- healpix_io.dll (../healpix_io/)
- AUTOMOC ON (Q_OBJECT 宏需要 MOC 处理)

**编译方式**:
```
cmake -B build -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build
```

## Task 5: GLRenderer 实现详情（2026-07-13）
**新建文件 (3个)**:
- `core/gl_renderer.h` - GLRenderer 类声明（RenderMode/RenderParams + init/render/update_stf/cleanup）
- `core/gl_renderer.cpp` - 实现（OpenGL 3.3 Core 渲染核心，无 Qt 依赖）
- `include/healpix_browser_core.h` - 统一头文件（引入 browser_backend/stf_engine/healpix_math/gl_renderer）

**关键设计决策**:
- OpenGL 函数加载: wglGetProcAddress 加载 1.2+ 函数指针, opengl32.lib 链接 1.1 函数
- 1.2+ 常量手动 #define（Windows <GL/gl.h> 仅含 1.1）
- 着色器源码内嵌 R"()" 原始字符串（GLSL 3.30 core）
- 球面渲染方案 B: CPU 端每顶点查值（ang2pix_nest → unordered_map 查找），作为 vertex attribute 上传
- 单帧渲染: TAN (gnomonic) 逆投影构建 1024x1024 R32F 纹理 + 全屏四边形 TRIANGLE_FAN
- 四边形用 glDrawArrays(GL_TRIANGLE_FAN, 0, 4) 而非 glDrawElements（无需 IBO）
- 矩阵运算: column-major，标准 perspective/look_at/multiply 实现

**着色器**:
- 球面: 顶点(aPosition vec3 + aValue float) → MVP → 片元 STF(mtf + asinhCompress)
- 四边形: 顶点(aPosition vec2 + aTexCoord vec2) → 片元纹理采样 + STF

**编译**: g++ -O2 -std=c++17 -Wall -Wextra -Icore -Iinclude -I../healpix_io/include
       -c core/gl_renderer.cpp -o core/gl_renderer.o -lopengl32 -lgdi32
**验证**: 编译 0 error 0 warning ✓, 无 Qt 依赖 ✓, 接口完整 ✓

**解决的问题**:
- M_PI 未定义: 添加 #define _USE_MATH_DEFINES + #include <cmath> + fallback #define M_PI
- wglGetProcAddress cast-function-type 警告: 先转 void* 再转目标函数指针类型
- 四边形 IBO 缺失: 改用 glDrawArrays(GL_TRIANGLE_FAN, 0, 4)，无需 IBO
- 注释行末反斜杠导致 -Wcomment 警告: 去掉行尾 \

## Task 2: HealpixMath 实现详情（2026-07-13）
**新建文件 (3个)**:
- `core/healpix_math.h` - HealpixMath 类声明（pix2ang_nest/ang2pix_nest/query_disc/ud_grade/angular_distance）
- `core/healpix_math.cpp` - 实现（NESTED 排序，三区域分块：北极/赤道/南极）
- `tests/test_healpix_math.cpp` - 5 项单元测试

**关键 bug 修复**: fact2 系数错误
- 错误: `fact2 = 4.0 / (3.0 * npface)` （多了一个因子 4）
- 正确: `fact2 = 1.0 / (3.0 * npface)`
- 影响: 极区 z 值计算错误，导致 pix2ang→ang2pix 往返一致性失败
- 验证: astropy_healpix nside=8 ipix=63 中心 z=0.994792，修复后一致

**编译**: g++ -O2 -std=c++17 -Wall -Wextra
**测试**: 5/5 PASS ✓
- test_pix2ang_nest_nside1: nside=1 ipix=0 dec≈41.81°
- test_ang2pix_roundtrip: nside=1/2/8/64 往返一致
- test_query_disc: nside=64 RA=0 Dec=0 radius=10° → 378 ipix
- test_angular_distance: 0°/180°/90° 验证
- test_ud_grade: nside 4→2，4 像素均值合并

## Task 4: BrowserBackend 实现详情（2026-07-13）
**新建文件 (3个)**:
- `core/browser_backend.h` - ViewParams/LeafData 结构体 + BrowserBackend 类声明
- `core/browser_backend.cpp` - 实现（移植自 healpix_browser_cpp，用 HealpixMath 替代内部坐标计算）
- `tests/test_browser_backend.cpp` - 4 项单元测试

**关键改动（vs healpix_browser_cpp）**:
- ipix_to_angle/angular_distance 改为 static，转发到 HealpixMath（支持任意 nside）
- 添加 get_filter() 方法（从 meta_json 解析 "filter" 字段）
- 添加 release_leaf() 方法（统一释放 malloc 分配的 LeafData）
- 去掉 HTTP 服务器相关代码
- 使用 LOG_INFO/LOG_ERROR 替代 std::cout/std::cerr
- get_required_leaves 使用 HealpixMath::pix2ang_nest(64, ...) 计算子叶坐标

**编译**: g++ -O2 -std=c++17 -Icore -Iinclude -I../healpix_io/include
**测试**: 4/4 PASS ✓（需 healpix_io.dll 在 PATH 中）
- test_open_hiss: 成功加载真实 .hiss 文件（nside=8192, npix=964279, filter=Red）
- test_open_hcsd: 接口验证（无 .hcsd 测试文件）
- test_ud_grade: nside 4→2 降采样正确
- test_ipix_to_angle_static: 静态方法转发正确（nside=1 ipix=0 ra=45 dec=41.81）

## Task 3: STFEngine 实现详情（2026-07-13）
**新建文件 (3个)**:
- `core/stf_engine.h` - STFParams 结构体 + STFEngine 类声明（mtf/get_preset/auto_stretch/to_uniforms）
- `core/stf_engine.cpp` - 实现（MTF 公式 + 4 预设 + MAD 自动拉伸 + GPU uniform 转换）
- `tests/test_stf_engine.cpp` - 4 项单元测试

**编译**: `make tests/test_stf_engine.exe` 成功（g++ -O2 -std=c++17 -Wall -Wextra -fopenmp）
**测试**: 4/4 PASS ✓
- test_mtf: 验证 MTF 三个不动点（0, 1, m→0.5）
- test_presets: 验证 linear/sqrt/asinh/log 四个预设的 midtones/compression
- test_auto_stretch: 1000 像素合成数据，shadows=27.76, highlights=72.24, midtones=0.500
- test_to_uniforms: 原始像素值正确归一化到 [0,1]（shadows=0.1, highlights=1.0）

**关键设计决策**:
- MTF 公式：((m-1)*x) / ((2m-1)*x - m)，m=0.5 退化线性，结果 clamp [0,1]
- MAD 自动拉伸：sigma=1.4826*MAD，shadows=median-3σ，highlights=median+3σ，midtones 归一化中位数
- 复杂度 O(n)：用 std::nth_element 而非全排序
- 边界处理：空数据/全相同值（sigma→0）/range→0 均有兜底
- 无 Qt 依赖：仅 STL（algorithm/cmath），core/ 可独立测试
- 日志：auto_stretch/to_uniforms 通过 logger.h 输出 INFO/DEBUG/WARN

## 关键约束
- core/ 无 Qt 依赖（grep -r "Q" core/ 应无 Qt 类型引用）
- OpenGL 3.3 Core Profile
- nside=8192 精度用 uint64_t
- STF MTF 公式：((m-1)*x) / ((2m-1)*x - m)
- 球面渲染用方案 B（顶点查值，CPU 端每顶点查值作为 attribute）
- PowerShell 脚本必须设置 UTF-8 编码
- 文件读写必须用 -Encoding UTF8

## 依赖
- healpix_io.dll（hiss_read/hcsd_read/hcsd_read_leaf）位于 ../healpix_io/
- Qt6（widgets/app 层，MSYS2: mingw-w64-x86_64-qt6-base）
- OpenGL32, gdi32（系统）

## 编译环境
- MSYS2 MinGW64 g++ 16.1.0
- Windows 10+
- Qt6 路径: C:/msys64/mingw64
