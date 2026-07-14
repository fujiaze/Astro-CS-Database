# healpix_browser_qt

C++ + Qt6 实现的 HEALPix 浏览器，替代旧版 WebGL + HTTP 后端架构（已归档至 `../archive/`）。

## 架构

三层分离，UI 与算法严格解耦：

- **`core/`** - 核心算法库（纯 C++17 + OpenGL 3.3 Core，无 Qt 依赖）
  - `HealpixMath` - 球面坐标转换（pix2ang_nest / ang2pix_nest / query_disc / ud_grade）
  - `STFEngine` - 显示拉伸引擎（MTF + MAD 自动拉伸 + 4 预设 + asinh 压缩）
  - `BrowserBackend` - 数据加载与按需子叶（.hiss 全量 / .hcsd 按需 + ud_grade 降采样）
  - `GLRenderer` - OpenGL 渲染核心（球面网格 + 单帧 TAN 投影 + STF 着色器）
- **`widgets/`** - Qt6 QOpenGLWidget 封装层（UI 前端）
  - `AbstractView` - 抽象基类，封装 OpenGL 上下文 + 事件转发骨架
  - `SingleFrameView` - .hiss 单帧 2D 切面投影（拖动平移 + 滚轮缩放）
  - `SphereView` - .hcsd 球面 3D 渲染（拖动旋转 + 滚轮缩放 + 触摸支持）
- **`app/`** - demo 可执行程序
  - `MainWindow` - 主窗口（菜单栏 + 状态栏 + STF 控制面板 + 文件路由）
  - `STFPanel` - STF 控制面板（4 滑块 + 4 预设 + 自动拉伸按钮）
  - `main.cpp` - QApplication 入口（命令行参数解析）

## 编译

### 前置依赖

- MSYS2 MinGW64 g++ 16.1.0+（`C:\msys64\mingw64\bin`）
- Qt6 Base（`pacman -S mingw-w64-x86_64-qt6-base`）
- CMake 3.16+
- healpix_io.dll（位于 `../healpix_io/`）

### core 静态库（Makefile，无 Qt 依赖）

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
make core
make tests    # 编译单元测试
# 手动运行测试（Makefile run_tests 在 Windows 下有语法问题）
./tests/test_healpix_math.exe
./tests/test_stf_engine.exe
./tests/test_browser_backend.exe
```

### 完整程序（CMake，需 Qt6）

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake -B build -DCMAKE_PREFIX_PATH="C:/msys64/mingw64" -G "MinGW Makefiles"
cmake --build build
```

产物：
- `build/libhealpix_browser_core.a` - core 静态库
- `build/libhealpix_browser_qt_widgets.a` - widgets 静态库
- `build/healpix_browser_qt.exe` - demo 可执行

## 运行 demo

```powershell
$env:Path = "C:\msys64\mingw64\bin;..\..\healpix_io;$env:Path"
cd build
.\healpix_browser_qt.exe                                   # 启动空窗口
.\healpix_browser_qt.exe "path\to\file.hiss"               # 直接打开单帧文件
.\healpix_browser_qt.exe "path\to\file.hcsd"               # 直接打开球面数据库
```

文件路由：
- `.hiss` → `SingleFrameView`（2D TAN 切面投影）
- `.hcsd` → `SphereView`（3D UV 球面 64×128 分段）

## 测试

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cd tests
./test_healpix_math.exe       # 5/5: pix2ang/ang2pix 往返一致性, query_disc, ud_grade
./test_stf_engine.exe         # 4/4: MTF 公式, 4 预设, MAD 自动拉伸, GPU uniform 归一化
./test_browser_backend.exe    # 4/4: .hiss 加载, .hcsd 接口, ud_grade, ipix_to_angle 静态方法
```

注：`test_browser_backend.exe` 需要 `healpix_io.dll` 在 PATH 中（`$env:Path += ";..\..\healpix_io"`）。

## 关键设计

### 与旧架构对比

| 旧架构（已归档） | 新架构 |
|------------------|--------|
| `healpix_browser_cpp` + winsock2 HTTP 服务器 | core/ 直接 C++ 内存调用 |
| `healpix_browser_web` + WebGL + base64 | Qt6 QOpenGLWidget + OpenGL 3.3 Core |
| 跨进程 HTTP + JSON + base64 序列化 | 同进程 C++ 内存直传 OpenGL 纹理 |
| 单一模式（下拉框切换单帧/球面） | 双 widget 独立类（按文件扩展名路由） |

### UI 与算法分离

- **core/** 无 Qt 依赖，可被任意 C++ 工程链接（`grep -r "Q" core/` 应无 Qt 类型引用）
- **widgets/** 仅负责窗口管理 + 事件桥接 + Qt OpenGL 上下文，不含业务/算法逻辑
- **app/** demo 程序，验证完整功能并可被外部 Qt 工程参考嵌入

### 嵌入大工程

```cpp
#include "healpix_browser_core.h"
#include "sphere_view.h"

// 在大工程的 QMainWindow 中
auto backend = std::make_unique<BrowserBackend>();
backend->open_file("sky.hcsd");

auto* view = new SphereView(this);
view->set_backend(backend.get());
view->auto_stretch();
setCentralWidget(view);
```

## 关键约束

- core/ 无 Qt 依赖（可独立编译验证）
- OpenGL 3.3 Core Profile
- nside=8192 精度用 uint64_t
- STF MTF 公式：`((m-1)*x) / ((2m-1)*x - m)`
- 球面渲染用方案 B（CPU 端每顶点查值作为 attribute）
- 单帧 TAN 投影：1024×1024 R32F 纹理 + 全屏四边形

## 设计文档

- 核心算法：`docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md`
- UI 前端：`docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md`
- 实现计划：`docs/superpowers/plans/2026-07-13-cpp-qt-browser.md`
- 模块记忆：`memory.md`
