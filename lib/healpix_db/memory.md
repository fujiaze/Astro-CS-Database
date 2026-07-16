# healpix_db - 模块开发memory

## 模块职责
Healpix天球分块数据库，提供LOD金字塔分层、球面浏览器可视化与Drizzle重投影功能，支持大规模天文图像的天球投影与多帧叠加。

## 当前版本
- 版本号：v1.0
- 最新commit：7129e32
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Healpix-Database
- 默认分支：main
- 关联仓库：
  - Healpix-Mosaic-Cpp（healpix_stack）：多帧叠加合成 - https://github.com/fujiaze/Healpix-Mosaic-Cpp
  - Healpix-Drizzle-Cpp（healpix_drizzle）：Drizzle重投影 - https://github.com/fujiaze/Healpix-Drizzle-Cpp

## 依赖列表
- C++17, OpenMP
- astro_image_io.dll（PipelineFrame命名块容器 + FITS读写）
- Qt6 + OpenGL 3.3 Core（healpix_browser_qt 球面浏览器）
- healpix_io.dll（.hiss/.hcsd 格式读写）

## 关键决策记录
- **模块拆分为3个独立仓库**：Healpix-Database核心、Healpix-Mosaic叠加、Healpix-Drizzle重投影，职责单一便于独立维护
- **hp_drizzle_run命名块直通**：Drizzle执行入口直接对接PipelineFrame命名块容器，避免临时文件中转，与管线引擎无缝集成
- **LOD金字塔分层**：天球分块按层级组织（LOD 0/1/2...），按视口动态加载，支持大规模图像高效浏览
- **球面浏览器 Qt6 + OpenGL 重构**：替代旧版 PyQt5+vispy / WebGL+HTTP 架构，C++ 内存直传 OpenGL 纹理消除通讯开销，UI 与算法严格分离 (core/ 无 Qt 依赖)

## 进度日志
### 2026-07-14 STF预设修复 + .hiss球面渲染 (spec: stf-moire-fix)

**STF预设修复 (A1)**:
- `get_preset` 接口扩展: 接收 data_min/data_max，返回原始像素值
- 预设=全数据范围+曲线形状 (对齐 Siril 行为)
- shadows=data_min, highlights=data_max (全范围可见)
- linear/sqrt/asinh/log 预设只改 midtones/compression

**.hiss 改球面渲染 (消除摩尔纹)**:
- 废弃 TAN 投影展平 + 1024纹理 + GL_LINEAR (产生摩尔纹)
- 新方案: HEALPix 像素多边形球面渲染 (每像素4角点球面四边形)
- 964279 像素 → 5785674 顶点, 61MB VBO
- 复用 sphere_program_ 着色器 (aPosition+aValue+STF)
- distance=3.0 固定, FOV=60/zoom 控制缩放
- 视角变化仅更新 MVP 矩阵, 不重建网格

**架构变更**:
- RenderMode: SINGLE_FRAME 废弃 → HISS_POLYGON (新)
- .hiss 和 .hcsd 都用 SphereView (球面渲染)
- SingleFrameView 归档到 widgets/archive/
- SphereView 新增: set_initial_view_from_bbox + set_render_mode + paintGL 重载
- pending_initial_view_ 机制: 首次渲染后 mesh 构建完成再设置视角

**STF单位体系** (2026-07-14 早些时候修复):
- to_uniforms: 不再归一化, 直传原始像素值
- compute_data_range: 百分位数 0.5%/99.5% 避免异常值
- STFPanel 滑块映射 [data_min, data_max] 原始像素值

### 2026-07-14 Qt6 浏览器重构完成 (spec: cpp-qt-browser)

**新模块**: `healpix_browser_qt/` - C++ + Qt6 三层架构浏览器
- 三层: core/ (无 Qt 依赖) → widgets/ (Qt6 QOpenGLWidget) → app/ (demo exe)
- 消除 HTTP + base64 通讯开销，C++ 内存直传 OpenGL 纹理
- 13 个 Task 全部完成 (Task 1-11, 13; Task 12 性能验证待后续)

**core/ 层 (无 Qt 依赖, 纯 C++17 + OpenGL 3.3 Core)**:
- `HealpixMath` - pix2ang_nest/ang2pix_nest/query_disc/ud_grade (5/5 测试通过)
  - 关键 bug 修复: fact2 系数 4.0/(3*npface) → 1.0/(3*npface)
- `STFEngine` - MTF + MAD 自动拉伸 + 4 预设 + asinh 压缩 (4/4 测试通过)
- `BrowserBackend` - .hiss 全量/.hcsd 按需 + ud_grade 降采样 (4/4 测试通过)
- `GLRenderer` - 球面网格 + 单帧 TAN 投影 + STF 着色器 (编译通过 0 error)

**widgets/ 层 (Qt6 QOpenGLWidget 封装)**:
- `AbstractView` - 抽象基类，OpenGL 上下文 + 事件转发骨架
- `SingleFrameView` - .hiss 单帧 2D 切面投影 (拖动+滚轮缩放)
- `SphereView` - .hcsd 球面 3D 渲染 (拖动+缩放+触摸)

**app/ 层 (demo exe)**:
- `MainWindow` - 主窗口 (菜单/状态栏/STFPanel dock/文件路由)
- `STFPanel` - 4 滑块 + 4 预设 + 自动拉伸按钮
- `main.cpp` - QApplication 入口 (命令行参数解析)

**编译验证**:
- Qt6 安装: `pacman -S mingw-w64-x86_64-qt6-base`
- CMake 配置: `cmake -B build -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -G "MinGW Makefiles"`
- 编译成功: 0 error, 0 warning
- 产物: libhealpix_browser_core.a + libhealpix_browser_qt_widgets.a + healpix_browser_qt.exe (1.15MB)

**运行验证**:
- 启动成功 MainWindowTitle="HEALPix Browser (Qt)" WorkingSet=52MB Responding=True
- 加载 .hiss 文件 (nside=8192, npix=964279, filter=Red) 未崩溃
- 运行时依赖: Qt6 DLLs + healpix_io.dll 需在 PATH 中

**归档操作**:
- 创建 `archive/` 目录
- 移动 `healpix_browser_cpp/` → `archive/` (HTTP 后端, 数据层逻辑已移植到 core/)
- 移动 `healpix_browser_web/` → `archive/` (WebGL 前端, 渲染/交互逻辑已移植到 core/+widgets/)
- 创建 `archive/README.md` 说明归档原因与移植内容
- 保留 `healpix_browser/` (PyQt5 旧版) 和 `healpix_lod/` 不动 (设计文档规定)

**关键设计决策**:
- UI 与算法严格分离: core/ 无 Qt 依赖, 可嵌入任意 C++ 工程
- OpenGL 3.3 Core Profile, 着色器内嵌 GLSL 3.30 字符串
- nside=8192 用 uint64_t 避免 npface 溢出
- 球面渲染方案 B: CPU 端每顶点查值作为 attribute
- 单帧 TAN 投影: 1024×1024 R32F 纹理 + 全屏四边形
- 双 widget 独立类 (SingleFrameView + SphereView), 无模式混用

### 2026-07-13 格式统一·浏览器重构·模块架构变化（spec: format-unification-browser-perf）

**格式体系统一**:
- 旧格式 → 新格式: .ahpx → .hiss（单帧存储）、.ahps → .hcsd（天球数据库）、.ahpl 废弃
- .hiss: Magic "HISS" + JSON头(zstd) + ipix数组(uint64) + pixel数组(float32)
- .hcsd: Magic "HCSD" + JSON头(zstd) + 子叶块索引(49152×24B) + ipix数组 + pixel数组
- 子叶块索引: nside=64 分区，O(1) 定位，支持浏览器按需加载
- 格式规范文档: `healpix_io/FORMAT_SPEC.md`

**新增模块**:
- `healpix_io/` - .hiss/.hcsd 格式读写 C++ DLL + Python 绑定（替代 ahpx_io）
  - C API: hiss_write/hiss_read/hcsd_write/hcsd_read/hcsd_read_leaf/hio_free
  - Python: HissWriter/HissReader/HcsdWriter/HcsdReader + 便捷函数
- `healpix_browser_cpp/` - C++ 渲染后端 + HTTP 服务器（替代 PyQt5 浏览器）
  - BrowserBackend 类: open_file/get_required_leaves/load_leaf/ud_grade
  - 按需子叶加载: 中心 nside=8192，中间 nside=2048，边缘 nside=256
  - HTTP 服务器: localhost:18080, /api/file_info, /api/leaf, /api/all_data
- `healpix_browser_web/` - WebGL 前端（替代 PyQt5 浏览器）
  - 球面 WebGL 渲染 (UV 球面 64×128 分段)
  - 视角交互 (鼠标拖动旋转、滚轮缩放)
  - STF 拉伸控制面板 (移植自 stf.py)
  - 单帧模式 (.hiss) + 球面模式 (.hcsd)

**废弃模块**:
- `healpix_lod/` - LOD 金字塔（README 标记废弃，LOD 应为内存数据结构，由浏览器 C++ 后端 ud_grade 动态生成）
- `healpix_browser/` - PyQt5 浏览器（README 标记废弃，被 healpix_browser_cpp + healpix_browser_web 替代）
- `astro_image_io/src/ahpx/` - .ahpx 格式读写（DEPRECATED.md 标记废弃，被 healpix_io 替代）

**I/O 改造**:
- drizzle 输出从 .ahpx 改为 .hiss（drizzle_engine.cpp 调用 hiss_write）
- 内存 sigma-clip 堆叠（healpix_stack 新增 hp_stack_hiss，废弃 .ahps 输出 .hcsd）
- 废弃 .ahps: 内存 sigma-clip 不落盘（count/sum/sum_sq 三数组，与帧数无关）
- 废弃 .ahpl: LOD 金字塔应为内存数据结构（浏览器 C++ 后端 ud_grade 动态生成）

**浏览器架构重构**:
- 旧架构: PyQt5 + vispy（healpix_browser）
- 新架构: WebView2/系统浏览器 + WebGL 前端（healpix_browser_web）+ C++ HTTP 服务器后端（healpix_browser_cpp）
- 通信方式: HTTP API (localhost:18080)
- 优势: 无 PyQt5/vispy 依赖，GPU 加速渲染，按需子叶加载## 进度日志
### 2026-07-13 文件打开支持验证完成 (Task 9)
- 验证 browser_cpp.exe + WebGL 前端的文件打开功能 (.hiss/.hcsd)
- 确认 C++ 后端: 命令行参数接受 file.hiss/file.hcsd, Magic 检测 (HISS/HCSD) 自动切换模式
  - .hiss 模式: hiss_read 全量加载到内存, all_ipix_/all_pixel_ 缓存
  - .hcsd 模式: hcsd_read 仅读头信息, 释放像素数据, 按需 hcsd_read_leaf 加载
- 确认前端 main.js init(): `this.mode = info.is_hiss ? 'single' : 'sphere'` 自动模式切换
  - 单帧模式: getAllData() 全量加载 → renderSingleFrame()
  - 球面模式: getRequiredLeaves() + loadLeaf() 按需加载
- 补充 browser_main.cpp 错误处理:
  - 添加 check_healpix_io_dll() 防御性检查 (GetModuleHandleA + 文件存在性)
  - 添加目标文件存在性检查 (open_file 前预检)
  - open_file 失败时输出可能原因提示
- 修复 build.ps1: 添加复制 mingw 运行时 DLL 步骤 (libgcc_s_seh-1/libstdc++-6/libwinpthread-1)
  - 原因: browser_cpp.exe 隐式依赖 mingw 运行时 DLL, 非 mingw 环境运行报 0xC0000135 (STATUS_DLL_NOT_FOUND)
- 创建 test_browser.py 测试脚本 (Python + requests + subprocess)
  - 测试 1: .hiss 文件 → file_info 返回 is_hiss=true, all_data 返回 10 像素 ✓
  - 测试 2: .hcsd 文件 → file_info 返回 is_hiss=false, all_data 返回 400 ✓
  - 测试 3: 不存在文件 → 返回码=1, 输出友好错误信息 ✓
- 测试结果: 3/3 全部通过

### 2026-07-13 healpix_browser_cpp C++ 渲染后端实现完成 (Task 7)
- 新建基于 WebView2/系统浏览器 + winsock2 HTTP 服务器的 C++ 后端, 替代 PyQt5 浏览器
- 7 个文件: include/(browser_backend.h 已存在 + http_server.h) + src/(browser_backend.cpp + http_server.cpp + browser_main.cpp) + Makefile + build.ps1 + README.md
- BrowserBackend: .hiss 全量加载 / .hcsd 按需 hcsd_read_leaf 加载
- 视角相关压缩: 中心 nside=8192 / 中间 2048 / 边缘 256
- ud_grade 降采样: NESTED 排序 ipix_coarse = ipix_fine >> (2*log2(ratio)), 4 相邻像素均值合并
- ipix_to_angle: HEALPix NESTED pix2ang (nside=64), 用 jrll/jpll 查找表 + 三区公式 (北极帽/赤道/南极帽)
- HTTP 服务器 (winsock2): GET / 静态文件 + 4 个 JSON API (/api/file_info, /api/required_leaves, /api/leaf, /api/all_data)
- 静态文件从 ../healpix_browser_web/ 读取, 路径遍历防护
- 编译: browser_cpp.exe 316.5 KB, 链接 healpix_io.dll + ws2_32
- 测试: .hiss 文件 (nside=8192, n_pix=964279) 加载成功, 降采样验证通过 (8192->2048: 16384->1024 像素, 8192->256: 16384->16 像素), required_leaves 找到有数据子叶, all_data 返回全量数据
- 修复 ud_grade shift 计算 bug: `r >>= 2` 改为 `r >>= 1` (shift = 2*log2(ratio))
- build.ps1 需保存为 UTF-8 with BOM (PowerShell 5.x 用 GBK 解析无 BOM 的 UTF-8 会失败)

### 2026-07-13 healpix_browser_web WebGL 前端实现完成 (Task 8)
- 新建基于 WebGL 的浏览器前端，替代 PyQt5 浏览器 (healpix_browser)
- 10 个文件: index.html + css/style.css + js/(stf/api-client/webgl-renderer/view-controller/main).js + shaders/(vertex/fragment).glsl + README.md
- 球面渲染: UV 球面 64×128 分段, GPU 内 STF+MTF+uint8 binning (移植自 sphere_renderer.py)
- STF 拉伸: 移植自 stf.py (MTF 公式 + MAD 自动 + 4 预设 + asinh 压缩)
- 子叶动态加载: leafTextures Map, 视角变化时按需加载+自动清理
- 单帧模式: Mollweide 投影 1024×512 R32F 纹理
- 视角交互: 鼠标拖动旋转 + 滚轮指数缩放 + 触摸支持
- WebGL 2.0 优先, 向下兼容 WebGL 1.0
- 通过 fetch API 与 C++ 后端 (Task 7, localhost:18080) 通信
- 纯 HTML/CSS/JS 无框架, 深色主题

### 2026-07-13 healpix_io C++ DLL 实现完成
- 实现 .hiss 单帧存储 + .hcsd 天球数据库读写模块 (Task 2)
- 5个 API 函数: hiss_write/hiss_read/hcsd_write/hcsd_read/hcsd_read_leaf + hio_free
- zstd 静态链接 (-lzstd -static), JSON 头压缩 level=5, 数组不压缩
- 子叶块索引: 49152 项 (12×64²), 每项 24 字节, O(1) 定位, 支持按需加载
- 编译: healpix_io.dll 1778.8 KB, 6个导出符号验证通过
- 测试: 4/4 通过 (.hiss往返 + .hcsd往返 + .hcsd按子叶读取 + .hiss空数据)
- 文件: include/healpix_io.h, src/healpix_io.cpp, Makefile, build.ps1, test_healpix_io.py

### 2026-07-12 hp_drizzle_run命名块直通完成
- 完成hp_drizzle_run命名块直通接口，对接PipelineFrame
- Drizzle重投影无需临时文件，直接从命名块读输入、写输出
- 推送至GitHub：commit 7129e32

### 2026-07-13 仓库结构整理完成
- 关联仓库重命名：Healpix-Drizzle-C-Python- → Healpix-Drizzle-Cpp，Healpix-Mosaic-C-Python- → Healpix-Mosaic-Cpp
- 关联仓库分支统一为main
- 本地healpix_drizzle/healpix_stack建立独立git仓库
- 文档刷新并重新推送
- 最新commit: 28ac468

### 已归档/废弃模块记录（2026-07-15，从 PROJECT_ARCHITECTURE.md 迁入）

> 来源：PROJECT_ARCHITECTURE.md §2.1 模块清单（第48-75行）+ §2.3 ASCII 图（第220-226行）+ 附录目录结构（第1480-1553行）。
> 本节集中记录 healpix_db 仓内已废弃/归档模块，便于后续查阅，避免重复散落于架构文档。

#### 1. healpix_lod（LOD 金字塔，已废弃）
- **路径**：`lib/healpix_db/healpix_lod/`
- **职责**：旧版 LOD 金字塔（.ahpl 文件 + 多级降采样）
- **废弃原因**：LOD 应为内存数据结构，由浏览器 C++ 后端 ud_grade 动态生成，无需落盘 .ahpl 文件
- **替代方案**：`healpix_browser_qt` 内存 ud_grade（按视口动态降采样）

#### 2. healpix_browser（PyQt5 浏览器，已废弃）
- **路径**：`lib/healpix_db/healpix_browser/`
- **职责**：旧版 PyQt5 + vispy 球面浏览器
- **废弃原因**：依赖 PyQt5/vispy 较重，通信开销大
- **替代方案**：`healpix_browser_qt`（Qt6 + OpenGL 3.3 Core，C++ 内存直传纹理，UI 与算法分离）

#### 3. healpix_browser_cpp（C++ HTTP 后端，已归档到 archive/）
- **原路径**：`lib/healpix_db/healpix_browser_cpp/`
- **现位置**：`lib/healpix_db/healpix_browser_qt/archive/`（已移动）
- **职责**：C++ 渲染后端 + winsock2 HTTP 服务器（按需子叶加载 + ud_grade 降采样）
- **归档原因**：HTTP + base64 通讯开销大，由 `healpix_browser_qt` 的 C++ 内存直传替代
- **替代方案**：`healpix_browser_qt` core/ 层（数据层逻辑已移植到 `BrowserBackend`）

#### 4. healpix_browser_web（WebGL 前端，已归档到 archive/）
- **原路径**：`lib/healpix_db/healpix_browser_web/`
- **现位置**：`lib/healpix_db/healpix_browser_qt/archive/`（已移动）
- **职责**：WebGL 前端（HTML/JS/CSS + STF + 球面渲染）
- **归档原因**：与 C++ HTTP 后端配套的浏览器前端方案，随 `healpix_browser_cpp` 一并归档
- **替代方案**：`healpix_browser_qt` widgets/ 层（渲染/交互逻辑已移植到 `SphereView`/`SingleFrameView`）

#### 5. astro_image_io/src/ahpx（.ahpx 读写，已废弃）
- **路径**：`lib/astro_image_io/src/ahpx/`
- **职责**：旧版 .ahpx 单帧格式读写（目录内含 DEPRECATED.md）
- **废弃原因**：.ahpx 格式被 .hiss 替代（格式体系统一：.ahpx→.hiss、.ahps→.hcsd、.ahpl 废弃）
- **替代方案**：`healpix_io`（.hiss / .hcsd 格式读写 C++ DLL + Python 绑定）

#### 归档位置说明
- `healpix_browser_cpp/` 与 `healpix_browser_web/` 已移至 `lib/healpix_db/archive/` 目录（2026-07-13 归档）
- `archive/README.md` 记录归档原因与移植内容映射（HTTP 后端 → core/、WebGL 前端 → core/+widgets/）
- `healpix_browser/`（PyQt5+vispy）已于 2026-07-16 归档至 `archive/legacy/healpix_browser_python/`
- `healpix_lod/` 已于 2026-07-16 归档至 `archive/legacy/healpix_lod/`（被 healpix_browser_qt 内存 ud_grade 替代）
- `healpix_browser_cpp/` 顶层重复副本已于 2026-07-16 删除（archive/ 下保留正式归档）

#### 保留原因
- 以上废弃/归档模块代码保留供参考，未删除，但不再用于主管线
- 保留目的：供历史方案对比、回溯设计决策、必要时查阅旧实现细节

#### 编译依赖说明（healpix_drizzle 引用 healpix_stack）
- `healpix_drizzle`（独立仓库 Healpix-Drizzle-Cpp 本地副本）编译时仍引用 `healpix_stack`（独立仓库 Healpix-Mosaic-Cpp 本地副本）的 `healpix_core.cpp`
- 即：`healpix_drizzle` 与 `healpix_stack` 物理上位于 `lib/healpix_db/` 下（在 `healpix_db/.gitignore` 中忽略），存在跨仓库编译依赖

## 遗留代码归档与依赖迁移（2026-07-16）

**spec**: docs/superpowers/specs/2026-07-16-healpix-db-legacy-archive.md + checklist.md

**操作内容**:
1. **删除冗余**: `healpix_browser_cpp/` 顶层删除（与 archive/healpix_browser_cpp/ 字节级重复）
2. **归档遗留代码到 archive/legacy/**:
   - `healpix_browser/`（PyQt5+vispy）→ `archive/legacy/healpix_browser_python/`
   - `healpix_lod/` → `archive/legacy/healpix_lod/`（被 healpix_browser_qt 内存 ud_grade 替代）
   - `tests/test_e2e_integration.py` → `archive/legacy/tests/`（依赖已删除模块，静默 skip）
   - 创建 `archive/legacy/README.md` 说明归档原因
3. **依赖迁移**: healpix_browser_qt 依赖从 `../healpix_io/` 迁移至 `lib/astro_image_io/`（aio 模块）
   - CMakeLists.txt: HIO_DIR → AIO_DIR（../../astro_image_io），链接库名 healpix_io → astro_image_io，添加 AIO_ENABLE_HEALPIX 定义
   - Makefile: 同上
   - deploy.ps1: healpix_io.dll → astro_image_io.dll
   - browser_backend.cpp: #include "healpix_io.h" → #include "aio_healpix_io.h"
   - 5 个源码注释中的路径同步更新

**验证结果**:
- astro_image_io.dll 构建成功（2923.7 KB，9 个 HEALPix I/O 符号全部导出）
- healpix_browser_qt Makefile 编译成功（core 静态库 + 3 测试）
- healpix_browser_qt CMake 完整构建成功（34/34：core + widgets + app + 测试）
- test_healpix_math 5/5 ALL PASS
- hiss_read 兼容宏验证通过（成功读取 nside=8192 n_pix=965048 的 .hiss 文件）

**独立仓库 commit 更新**（与文档记录不一致，已更新）:
- healpix_stack: 5f6b201 → 027b64f（文档已更新）
- healpix_drizzle: e7c1d1f → ecf8758（文档已更新）

**关键发现**:
- astro_image_io 的 aio_healpix_io.h 末尾有兼容宏（#define hiss_read aio_hiss_read 等），但函数声明和宏都被 #ifdef AIO_ENABLE_HEALPIX 包裹，编译时必须定义此宏
- DLL 名称从 healpix_io.dll 变为 astro_image_io.dll，链接库名从 -lhealpix_io 变为 -lastro_image_io
- 路径从 ../healpix_io/ 变为 ../../astro_image_io/（跨 lib/healpix_db/ 到 lib/astro_image_io/）
