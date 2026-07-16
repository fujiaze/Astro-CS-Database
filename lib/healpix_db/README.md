# Healpix Database

版本：v2.0 | 2026-07-16

天文巡天数据的 HEALPix 球面存储与可视化系统。Qt6+OpenGL 浏览器提供交互式可视化，支持 .hiss 单帧与 .hcsd 球面数据库浏览。

## GitHub 仓库
- 仓库地址：https://github.com/fujiaze/Healpix-Database
- 默认分支：main
- 关联仓库：Healpix-Mosaic-C-Python-（稀疏堆栈）、Healpix-Drizzle-C-Python-（Drizzle 重投影）

## 模块

| 模块 | 功能 | 技术 | 状态 |
|------|------|------|------|
| `healpix_browser_qt/` | 球面可视化浏览器（STF 拉伸 + 球面/单帧渲染 + 30°经纬线网格） | C++ (Qt6 + OpenGL 3.3 Core) | 活跃 |
| `healpix_io/` | .hiss/.hcsd 文件 I/O（已归档，API 并入 astro_image_io） | C++ | 已归档（2026-07-16） |
| `healpix_stack/` | 稀疏 HEALPix 堆栈存储（独立仓库本地副本，.gitignore 忽略） | C++ | 活跃（独立仓库） |
| `healpix_drizzle/` | 球面 Drizzle 重投影（独立仓库本地副本，.gitignore 忽略） | C++ | 活跃（独立仓库） |
| `docs/` | Drizzle 算法概述文档 | - | 活跃（部分过时） |
| `archive/` | 历史归档（healpix_browser_cpp + healpix_browser_web + legacy） | - | 归档 |

### 归档内容

- `archive/healpix_browser_cpp/` - C++ HTTP 后端 + WebGL 前端（2026-07-13 归档，被 healpix_browser_qt 替代）
- `archive/healpix_browser_web/` - WebGL 前端（2026-07-13 归档，被 healpix_browser_qt 替代）
- `archive/legacy/healpix_browser_python/` - PyQt5+vispy 浏览器（2026-07-16 归档，被 healpix_browser_qt 替代）
- `archive/legacy/healpix_lod/` - LOD 金字塔（2026-07-16 归档，被 healpix_browser_qt 内存 ud_grade 替代）
- `archive/legacy/tests/` - 端到端集成测试（2026-07-16 归档，依赖已删除模块）

## 关联仓库

| 仓库 | 职责 |
|------|------|
| [Astro-Image-IO-C](https://github.com/fujiaze/Astro-Image-IO-C) | 统一 I/O 层：FITS/XISF 读取、.ahpx/.hiss/.hcsd 格式、压缩（zstd/lz4）、PipelineFrame |
| [Healpix-Mosaic-C-Python-](https://github.com/fujiaze/Healpix-Mosaic-C-Python-) | 稀疏 HEALPix 堆栈存储（sigma-clip + SNR 加权合并） |
| [Healpix-Drizzle-C-Python-](https://github.com/fujiaze/Healpix-Drizzle-C-Python-) | 球面 Drizzle 重投影（WCS+SIP → HEALPix，通量守恒） |

## 依赖

### healpix_browser_qt 编译依赖
- g++ (C++17, MSYS2 MinGW64)
- Qt6 (Core/Gui/Widgets/OpenGLWidgets)
- OpenGL 3.3 Core
- [astro_image_io](https://github.com/fujiaze/Astro-Image-IO-C)（提供 HEALPix I/O 兼容 API：aio_hiss_*/aio_hcsd_*/aio_hio_*）

## 构建

### healpix_browser_qt

```bash
# 1. 先构建 astro_image_io.dll (在 lib/astro_image_io/ 下)
cd lib/astro_image_io
powershell -ExecutionPolicy Bypass -File build.ps1

# 2. 构建 healpix_browser_qt (在 lib/healpix_db/healpix_browser_qt/ 下)
cd lib/healpix_db/healpix_browser_qt
cmake -B build -DCMAKE_PREFIX_PATH=C:/msys64/mingw64
cmake --build build

# 或用 Makefile (仅 core + 测试，不需 Qt6)
make
```

### 部署（含 Qt6 DLL 打包）

```bash
cd lib/healpix_db/healpix_browser_qt
powershell -ExecutionPolicy Bypass -File deploy.ps1
```

## 使用

### 球面浏览器

```bash
cd lib/healpix_db/healpix_browser_qt
# 启动（需 astro_image_io.dll 在 PATH 或同目录）
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
$env:QT_PLUGIN_PATH = "C:\msys64\mingw64\share\Qt6\plugins"
Start-Process -FilePath build\healpix_browser_qt.exe -ArgumentList "`"<hiss或hcsd路径>`""
```

或用启动脚本：
```bash
lib/healpix_db/healpix_browser_qt/run_healpix.bat <hiss或hcsd路径>
```

支持：
- 单帧浏览（.hiss 文件，STF 拉伸）
- 球面数据库浏览（.hcsd 文件，拖动旋转，滚轮缩放）
- 30° 经纬线网格（Ctrl+G 开关）
- 赤道仪相机导航（yaw 绕赤经轴/pitch 绕赤纬轴，north-up 无 roll）
- LOD 动态 nside（FOV 自适应分辨率）
- 菱形像素 drizzle（pixfrac=1.0，无缝隙）

## 文件格式

### .hiss（单帧存储）
HEALPix 单帧数据（ipix + pixel + 可选 snr），由 astro_image_io 的 aio_hiss_write/read 管理。

### .hcsd（球面数据库）
HEALPix 球面数据库（含子叶块索引，支持按需加载），由 astro_image_io 的 aio_hcsd_write/read/read_leaf 管理。

## 技术特点

- **HEALPix 等面积像素化**：NESTED scheme，支持 nside 最大 8192+
- **稀疏存储**：只存有数据的像素，未观测天区零开销
- **全链路压缩**：zstd level 5 + lz4，由 astro_image_io 统一提供
- **Qt6+OpenGL 3.3 Core**：三层架构（core 无 Qt 依赖 + widgets Qt6 + app demo）
- **非破坏性 STF 拉伸**：MTF 公式，0.5%/99.5% 分位自动计算
- **LOD 动态 nside**：FOV 自适应，防止欠采样
- **赤道仪相机**：north-up 无 roll，MAX_FOV=50° 限制球面畸变
