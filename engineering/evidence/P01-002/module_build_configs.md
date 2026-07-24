# P01-002 模块构建配置清单

> **证据编号**: P01-002
> **生成日期**: 2026-07-24
> **用途**: 为 `dependencies.lock.json` 生成提供活跃模块构建配置数据
> **数据源**: 各模块 `build.ps1` / `Makefile` / `CMakeLists.txt`（只读采集，未修改任何文件）
> **权威规则**: build.ps1 优先；Makefile 标注过时差异；data_pipeline 标注 no_build

## 1. 总览

| # | 模块 | 路径 | 构建系统 | 输出 | 权威源 |
|---|------|------|----------|------|--------|
| 1 | astro_image_io | lib/astro_image_io | build.ps1+Makefile | astro_image_io.dll | build.ps1 |
| 2 | calibration | lib/calibration | build.ps1+Makefile | astro_calibration.dll | build.ps1 |
| 3 | dynamic_psf | lib/dynamic_psf | Makefile | dynamic_psf.dll | Makefile |
| 4 | gaia_xpsd_client | lib/gaia_xpsd_client | Makefile | gaia_client.dll | Makefile |
| 5 | star_detector | lib/star_detector | Makefile | star_detector.dll | Makefile |
| 6 | snr_estimator | lib/snr_estimator/cpp | build.ps1+Makefile | snr_estimator.dll | build.ps1 |
| 7 | photometric_calib | lib/photometric_calib/cpp | build.ps1+Makefile | photometric_calib.dll | build.ps1 |
| 8 | plate_solve_ipv | lib/plate_solve/cpp/ipv | build.ps1+Makefile | ipv_solver.dll | build.ps1 |
| 9 | healpix_drizzle | lib/healpix_db/healpix_drizzle | Makefile | healpix_drizzle.dll | Makefile |
| 10 | healpix_stack | lib/healpix_db/healpix_stack | build.ps1+Makefile | healpix_stack.dll | build.ps1 |
| 11 | healpix_browser_qt | lib/healpix_db/healpix_browser_qt | CMake+Makefile | healpix_browser_qt.exe | CMakeLists.txt |
| 12 | orchestrator | lib/orchestrator/cpp | Makefile | orchestrator.exe | Makefile |
| 13 | data_pipeline | lib/data_pipeline | no_build | — | — |

**统计**: 13 个模块 | 11 DLL + 1 EXE + 1 静态库 + 1 无构建

## 2. 构建系统分布

| 构建系统 | 数量 | 模块 |
|----------|------|------|
| build.ps1 + Makefile | 6 | astro_image_io, calibration, snr_estimator, photometric_calib, plate_solve_ipv, healpix_stack |
| Makefile only | 5 | dynamic_psf, gaia_xpsd_client, star_detector, healpix_drizzle, orchestrator |
| CMake + Makefile | 1 | healpix_browser_qt |
| no_build | 1 | data_pipeline |

## 3. 各模块详细配置

### 3.1 astro_image_io

| 项目 | build.ps1（权威） | Makefile（过时） |
|------|-------------------|------------------|
| 源文件 | 11 个（含 healpix/aio_healpix_io.cpp） | 10 个（缺 healpix 源） |
| CFLAGS | -O2 -std=c++17 -Wall -fopenmp -shared | -O2 -march=native -Wall -std=c++17 -fopenmp |
| 链接库 | -lm -lzstd -llz4 | -lm |
| 输出 | astro_image_io.dll | astro_image_io.dll |
| include | include, src | include, src |
| 定义 | AIO_ENABLE_FITS/XISF/AHPX/HEALPIX/COMPRESSOR/PIPELINE, HAS_ZSTD, HAS_LZ4 | 无 |

**差异**: Makefile 缺 healpix 源文件、条件定义、zstd/lz4 库；多 -march=native
**备注**: build.ps1 由 aio_build_config.json 驱动（预设: default/full/minimal/healpix），所有定义和库均为条件性

### 3.2 calibration

| 项目 | build.ps1（权威） | Makefile（严重过时） |
|------|-------------------|---------------------|
| 源文件 | 4 个（master_generator, calibrator, cosmetic_corrector, ac_api） | 1 个（cpp/cosmetic_corrector.cpp） |
| CFLAGS | -O2 -march=native -Wall -std=c++17 -shared -fopenmp | -O3 -march=native -ffast-math -funroll-loops -fopenmp -Wall -std=c++17 |
| 输出 | astro_calibration.dll | cosmetic_corrector.dll |
| include | include | cpp |

**差异**: Makefile 严重过时 — 输出名不同、仅 1 源文件 vs 4、include 目录不同（-Icpp vs -Iinclude）

### 3.3 dynamic_psf

| 项目 | 值 |
|------|----|
| 源文件 | src/dpsf_psf.cpp, src/dpsf_image.cpp, src/dpsf_log.cpp |
| CFLAGS | -O2 -march=native -Wall -std=c++17 -fopenmp |
| 链接库 | -lm |
| 输出 | dynamic_psf.dll |
| include | include, src |
| 定义 | 无 |
**备注**: 仅 Makefile，无外部依赖

### 3.4 gaia_xpsd_client

| 项目 | 值 |
|------|----|
| 源文件 | src/gaia_client.c |
| CFLAGS | -O2 -march=native -Wall |
| 链接库 | -lz -fopenmp |
| 输出 | gaia_client.dll（还有 libgaia_client.a 静态库 + gaia_client_demo exe） |
| include | src |
| 定义 | 无 |
**备注**: C 模块（使用 gcc）。多目标：静态库 + DLL + demo exe。依赖 zlib

### 3.5 star_detector

| 项目 | 值 |
|------|----|
| 源文件 | src/sdet_api.cpp, sdet_detector.cpp, sdet_image.cpp, sdet_log.cpp, sdet_background.cpp |
| CFLAGS | -Wall -std=c++17 -fopenmp -march=native -ffp-contract=fast -funroll-loops -O3 |
| 链接库 | -lgsl -lgslcblas -lm |
| 输出 | star_detector.dll |
| include | include, src |
| 定义 | 无 |
**备注**: 依赖 GSL（gsl_multifit_nlinear）。有可选 LTO 和 PCH 目标

### 3.6 snr_estimator

| 项目 | build.ps1（权威） | Makefile |
|------|-------------------|----------|
| 源文件 | src/snr_estimator.cpp | src/snr_estimator.cpp |
| CFLAGS | -O2 -std=c++17 -Wall -fPIC -fopenmp -shared | -O2 -std=c++17 -Wall -fPIC -fopenmp |
| 输出 | snr_estimator.dll | snr_estimator.dll |
**差异**: 无 — 两者一致

### 3.7 photometric_calib

| 项目 | build.ps1（权威） | Makefile |
|------|-------------------|----------|
| 源文件 | 5 个（pc_api, star_matcher, image_corrector, wcs_transform, spectrum_integrator） | 相同 |
| CFLAGS | -O2 -std=c++17 -fopenmp -fPIC -Wall -shared -fopenmp | -O2 -std=c++17 -fopenmp -fPIC -Wall |
| 链接库 | -lm（直接链接 gaia_client.dll） | -lm（直接链接 gaia_client.dll） |
| 输出 | photometric_calib.dll | photometric_calib.dll |
| include | include, src, ../../gaia_xpsd_client/src | 相同 |
**差异**: 无 — 两者一致。依赖 gaia_client.dll

### 3.8 plate_solve_ipv

| 项目 | build.ps1（权威） | Makefile |
|------|-------------------|----------|
| 源文件 | 13 个（ipv_select ~ ipv_entry） | 相同 13 个 |
| CFLAGS | -std=c++17 -O3 -ffast-math -funroll-loops -march=native -Wall -Wextra -mstackrealign -fopenmp | -std=c++17 -O2 -march=native -Wall -Wextra -fopenmp |
| 定义 | IPV_EXPORTS, __USE_MINGW_ANSI_STDIO=1 | IPV_EXPORTS |
| 链接库 | -lkernel32 | -lkernel32 |
| 输出 | ipv_solver.dll | ipv_solver.dll |
**差异**: 源文件一致；build.ps1 优化更激进（-O3 -ffast-math -funroll-loops -mstackrealign）+ 多 __USE_MINGW_ANSI_STDIO=1

### 3.9 healpix_drizzle

| 项目 | 值 |
|------|----|
| 源文件 | fits_reader.cpp, wcs_sip.cpp, poly_clip.cpp, drizzle_engine.cpp, hp_drizzle_api.cpp（模块根目录） |
| 静态依赖 | ../healpix_stack/healpix_core.cpp, ../healpix_stack/gradient/snr_evaluator.cpp |
| CFLAGS | -O3 -march=native -ffast-math -funroll-loops -Wall -std=c++17 -fopenmp |
| 链接库 | -lm, -L$(AIO_DIR) -lastro_image_io |
| 输出 | healpix_drizzle.dll |
| include | ., ../../astro_image_io/include, ../../astro_image_io/src, ../healpix_stack |
| 定义 | AIO_ENABLE_HEALPIX |
**备注**: 仅 Makefile。静态编译 healpix_core + snr_evaluator。链接 astro_image_io.dll

### 3.10 healpix_stack

| 项目 | build.ps1（权威） | Makefile（过时） |
|------|-------------------|------------------|
| 源文件 | 13 个（含 5 gradient/ + gaia_client.c） | 7 个（无 gradient/，无 gaia_client.c） |
| CFLAGS | -O3 -std=c++17 -Wall -fopenmp -shared | -O3 -march=native -ffast-math -funroll-loops -Wall -std=c++17 -fopenmp |
| 链接库 | -lzstd -llz4 -lz -lm, -lastro_image_io | -lzstd -llz4 -lm, -lastro_image_io -lhealpix_io |
| include | ., gradient, aio/include, aio/src, gaia/src, eigen3 | ., aio/include, aio/src, healpix_io/include |
| 定义 | HAS_ZSTD, HAS_LZ4, AIO_ENABLE_HEALPIX | HAS_ZSTD, HAS_LZ4 |
| 输出 | healpix_stack.dll | healpix_stack.dll |

**差异**: Makefile 缺 gradient/ 源 + gaia_client.c；引用已合并的 healpix_io.dll；缺 AIO_ENABLE_HEALPIX + eigen3
**备注**: build.ps1 使用 Eigen3（C:\msys64\mingw64\include\eigen3）。源文件在模块根目录

### 3.11 healpix_browser_qt

| 项目 | CMakeLists.txt（权威） | Makefile（仅 core + tests） |
|------|------------------------|----------------------------|
| 构建系统 | CMake + Qt6 | g++ 直接编译 |
| 目标 | 3 个（core 静态库 + widgets 静态库 + app exe） | 1 个（core 静态库） |
| 源文件 | core(4) + widgets(2) + app(3) = 9 个 | core(4) |
| CXX 标准 | C++17, AUTOMOC/AUTORCC/AUTOUIC ON | C++17 |
| Qt6 组件 | Core, Gui, Widgets, OpenGLWidgets | 无 |
| 定义 | AIO_ENABLE_HEALPIX | AIO_ENABLE_HEALPIX |
| 输出 | libhealpix_browser_core.a + libhealpix_browser_qt_widgets.a + healpix_browser_qt.exe | libhealpix_browser_core.a |

**重要**: 任务描述说"qmake（Qt6）"，实际为 **CMake + Qt6**（find_package Qt6），无 .pro 文件。CMakeLists.txt 是完整 app 构建的权威源。Makefile 仅构建 core 静态库 + 单元测试（无 Qt）。另有 deploy.ps1 用于部署

### 3.12 orchestrator

| 项目 | 值 |
|------|----|
| 源文件 | src/main.cpp, orchestrator.cpp, dll_loader.cpp, checkpoint.cpp, logger.cpp, cli_repl.cpp, cli_command.cpp |
| CFLAGS | -O2 -std=c++17 -Wall -fopenmp |
| 链接库 | -lm（-static） |
| 输出 | orchestrator.exe |
| include | include + 9 个其他模块头文件路径 |
| 定义 | 无 |
**备注**: 输出为 EXE（非 DLL）。编译时不链接任何 DLL — 运行时通过 DllLoader 动态加载。包含 9 个模块的头文件

### 3.13 data_pipeline

| 项目 | 值 |
|------|----|
| 构建系统 | no_build |
| 源文件 | src/aio_pipeline.cpp, src/aio_pipeline_engine.cpp |
| 头文件 | include/aio_pipeline.h, include/aio_pipeline_engine.h |
**备注**: 无 build.ps1 或 Makefile。源文件由 astro_image_io 的 build.ps1 在 enable_pipeline=true 时编译进 astro_image_io.dll。非独立构建模块

## 4. 过时 Makefile 清单

| 模块 | 过时程度 | 差异说明 |
|------|----------|----------|
| astro_image_io | 中度 | 缺 healpix 源、条件定义、zstd/lz4 库 |
| calibration | 严重 | 输出名不同、1 源 vs 4 源、include 目录不同 |
| healpix_stack | 重度 | 引用已合并的 healpix_io.dll、缺 gradient 源 + gaia_client.c、缺 AIO_ENABLE_HEALPIX + eigen3 |

## 5. 一致模块清单

| 模块 | 一致性 |
|------|--------|
| snr_estimator | build.ps1 与 Makefile 完全一致 |
| photometric_calib | build.ps1 与 Makefile 一致 |
| plate_solve_ipv | 源文件一致，build.ps1 优化更激进 |

## 6. 外部依赖汇总

| 依赖 | 使用模块 |
|------|----------|
| GSL (-lgsl -lgslcblas) | star_detector |
| zlib (-lz) | gaia_xpsd_client, healpix_stack |
| zstd (-lzstd) | astro_image_io（条件）, healpix_stack |
| lz4 (-llz4) | astro_image_io（条件）, healpix_stack |
| Eigen3 | healpix_stack |
| Qt6 (Core/Gui/Widgets/OpenGLWidgets) | healpix_browser_qt |
| OpenGL | healpix_browser_qt |
| kernel32 (-lkernel32) | plate_solve_ipv |

## 7. 模块间依赖关系

```
astro_image_io        ← 基础 I/O 模块（无依赖）
calibration           ← 无依赖
dynamic_psf           ← 无依赖
gaia_xpsd_client      ← 无依赖（zlib）
star_detector         ← 无依赖（GSL）
snr_estimator         ← 无依赖
photometric_calib     → gaia_xpsd_client (DLL)
plate_solve_ipv       ← 无依赖
healpix_drizzle       → astro_image_io (DLL), healpix_stack (静态)
healpix_stack         → astro_image_io (DLL), gaia_xpsd_client (静态)
healpix_browser_qt    → astro_image_io (DLL)
orchestrator          → 9 个模块头文件（运行时 DLL 动态加载）
data_pipeline         → 编译进 astro_image_io (enable_pipeline)
```

## 8. 编译环境

- **编译器**: C:\msys64\mingw64\bin\g++.exe (MSYS2 MinGW64)
- **C++ 标准**: C++17（全部模块）
- **通用 CFLAGS**: -std=c++17 -fopenmp -Wall
- **通用库**: -lm
- **静态链接**: 多数模块使用 -static-libgcc -static-libstdc++ 或 -static

## 9. 输出文件

- `module_build_configs.json` — 完整机器可读配置（本文件的数据源）
- `module_build_configs.md` — 本摘要文件
