# Astro CS Normalization Database - 项目架构文档

> 本文档归纳截至 2026-07-16 的项目真实架构状态，基于实际代码盘点编写。
> 历史架构文档已归档至 `docs/archive/`（PROJECT_ARCHITECTURE_2026-07-16.md / UI_ARCHITECTURE_2026-07-16.md）。
> 设计与实施割裂问题详见 `docs/DESIGN_IMPL_GAP.md`。
> 浏览器/UI 架构细节见各模块 `memory.md`（§10 文档职责分离）。

---

## 1. 项目概述

本项目是一套**天文图像处理管线**，将 FITS 原始 CCD 图像经校准、解析、PSF 拟合、流量标定归一化、梯度校正、SNR 估算、Drizzle 重投影等阶段，汇入基于 HEALPix 球面分块的标准化数据库（.hiss 单帧 / .hcsd 天球数据库），实现多帧、多波段、全天球的可浏览、可查询、可叠加的标准化星图档案。

**核心设计原则**：

- **C++ 核心算法 + Python 调试层**：性能敏感算法用 C++17 实现，编译为 DLL；Python 定位为**调试层**（非永久胶水层），调试完毕后由 C++ 逐步替代，最终转化为纯 C++ 架构。
- **内存管线（PipelineFrame）**：模块间通过命名块容器在内存中传递数据，避免临时 FITS 文件落盘开销。
- **分模块独立仓库**：每个核心模块独立 GitHub 仓库，本地通过 `lib/` 目录组织，按需 clone 依赖模块。根目录**非 git 仓库**，各模块独立 git 管理。
- **两段流水线架构**：第一段单帧预处理（FITS→.hiss, stage 0-6，2026-07-18 归档 GRADIENT_2D 后 7 节点）+ 第二段多帧合并（.hiss→.hcsd, stage 7-8），通过 `orchestrator stage1` / `orchestrator stage2` 两个 CLI 命令调用。

**端到端流程**：

```
原始 FITS → 读取 → 校准 → 解析(WCS/SIP) → PSF 拟合 → 流量标定(应用 scale 到图像) → SNR 估算
        → Drizzle 到 HEALPix → .hiss 单帧输出
        → 球面梯度校正 → Winsorized sigma clip + SNR²加权叠加 → .hcsd 天球数据库
```

---

## 2. 两段流水线 9 节点架构（2026-07-16，2026-07-18 归档 GRADIENT_2D）

### 第一段：单帧预处理（`orchestrator stage1`，FITS → .hiss）

| Stage | 名称 | 模块 | 说明 |
|-------|------|------|------|
| 0 | READ_FITS | astro_image_io | 读取 FITS/XISF，构造 PipelineFrame（data + header） |
| 1 | CALIBRATE | calibration | dark/flat/bias 校准 + 坏点修复 |
| 2 | PLATESOLVE | plate_solve | WCS/SIP 解析（向量匹配法 + Umeyama SVD），star_det + gaia_cat 注入 |
| 3 | PSF | dynamic_psf | PSF 拟合（高斯/Moffat），OpenMP 16 线程并行，输出 [N,9] |
| 4 | PHOTOMETRIC | photometric_calib | F_syn=∫S(λ)·T(λ)·Q(λ)dλ 积分 + IRLS+Tukey 稳健回归求全局 scale 并应用到图像（测光坐标系校准），输出 photo_stats（scale_factor / n_matched / sigma_residual） |
| 5 | SNR | snr_estimator | 乘法模型 SNR = SNR_phot × (SNR_psf/median) |
| 6 | DRIZZLE | healpix_drizzle | Drizzle 到 HEALPix，输出 .hiss（含 snr 通道） |

> 2026-07-18 归档：原 stage 5 GRADIENT_2D（乘性梯度曲面拟合 + 图像校正）已归档至 `lib/photometric_calib/archive/gradient_2d/`。stage1 不做曲面拟合和图像亮度修正（由 stage2 球面梯度校准承担），PSF 后只做测光坐标系校准（PHOTOMETRIC 已应用 scale 到图像）然后直接算 SNR。stage 序号重排：SNR 6→5, DRIZZLE 7→6。

### 第二段：多帧合并（`orchestrator stage2`，.hiss → .hcsd）

| Stage | 名称 | 模块 | 说明 |
|-------|------|------|------|
| 7 | GRADIENT_SPHERE | healpix_stack | 球面梯度校准（hp_stack_gradient_corrected） |
| 8 | STACK | healpix_stack | Winsorized sigma clip + SNR²加权叠加，输出 .hcsd |

---

## 3. 模块清单（lib/ 实际结构盘点）

### 3.1 活跃模块（主管线使用）

| 序号 | 模块路径 | 模块名 | 职责 | GitHub 仓库 | 实施状态 |
|------|----------|--------|------|-------------|----------|
| 1 | `lib/astro_image_io/` | astro_image_io | FITS/XISF I/O + PipelineFrame + 管线引擎 + zstd/lz4 压缩 | Astro-Image-IO-C | 活跃 |
| 2 | `lib/calibration/` | calibration | dark/flat/bias 校准 + 坏点修复 + 主帧生成 | Astro-Calibration-Cpp | 活跃 |
| 3 | `lib/plate_solve/` | plate_solve | WCS/SIP 解析（向量匹配 + Umeyama SVD） | PlateSolve-IPV-Cpp | 活跃（2026-07-16 .git 已重建，commit 308f209） |
| 4 | `lib/dynamic_psf/` | dynamic_psf | PSF 拟合（高斯/Moffat），OpenMP 并行 | 待建立独立仓库 | 活跃 |
| 5 | `lib/photometric_calib/` | photometric_calib | 流量标定归一化（DR3SP 光谱积分 + 全局 scale） | Flux-calibration | 活跃 |
| 5a | `lib/photometric_calib/archive/gradient_2d/` | gradient_2d（已归档） | step4 C++化：乘性梯度曲面拟合 + 图像校正（2026-07-18 归档，stage1 不做曲面拟合，由 stage2 球面梯度校正承担） | 随 photometric_calib | 已归档（2026-07-18） |
| 6 | `lib/healpix_db/healpix_drizzle/` | healpix_drizzle | Drizzle 到 HEALPix（输出 .hiss） | Healpix-Drizzle-Cpp | 活跃（独立仓库，.gitignore 忽略） |
| 7 | `lib/healpix_db/healpix_stack/` | healpix_stack | 多帧 sigma-clip 堆叠 + 球面梯度校正（输出 .hcsd） | Healpix-Mosaic-Cpp | 活跃（独立仓库，.gitignore 忽略） |
| 8 | `lib/healpix_db/healpix_io/` | healpix_io | .hiss / .hcsd 存储格式读写（已归档，API 并入 astro_image_io） | Healpix-Database | 已归档（2026-07-16） |
| 9 | `lib/healpix_db/healpix_browser_qt/` | healpix_browser_qt | **当前浏览器**：Qt6 + OpenGL 3.3 Core 三层架构 | Healpix-Database | 活跃（依赖 lib/astro_image_io/） |
| 10 | `lib/gaia_xpsd_client/` | gaia_xpsd_client | Gaia DR3SP 光谱数据库客户端（C API） | 待建立独立仓库 | 活跃 |
| 11 | `lib/orchestrator/` | orchestrator | 管线编排引擎（两段流水线 10 节点 C++ CLI + Python 调试层） | Orchestrator-Cpp-Python | 活跃 |
| 12 | `lib/snr_estimator/` | snr_estimator | SNR 估算（乘法模型） | Snr-Estimator-Cpp-Python | 设计/实施阶段 |
| 13 | `lib/star_detector/` | star_detector | 星点检测（被 plate_solve 调用） | 待建立独立仓库 | 活跃 |
| 14 | `lib/data_pipeline/` | data_pipeline | **数据总线**：PipelineFrame + PipelineEngine + 命名块容器（从 astro_image_io 拆分，为各模块提供底层数据管线支撑） | Astro-Data-Pipeline | 活跃（拆分中间态：astro_image_io 仍保留副本） |
| 15 | `lib/integration_test/` | integration_test | 全链路整合测试（已归档） | — | 已归档（orchestrator/archive/scripts/ 有完整副本） |

### 3.2 已废弃/归档模块（代码保留供参考）

| 模块路径 | 状态 | 说明 |
|----------|------|------|
| `lib/healpix_db/archive/healpix_browser_cpp/` | 已归档（2026-07-13） | C++ HTTP 后端 + WebGL 前端，被 healpix_browser_qt 替代 |
| `lib/healpix_db/archive/healpix_browser_web/` | 已归档（2026-07-13） | WebGL 前端，被 healpix_browser_qt 替代 |
| `lib/healpix_db/archive/legacy/healpix_browser_python/` | 已归档（2026-07-16） | PyQt5+vispy 浏览器，被 healpix_browser_qt 替代 |
| `lib/healpix_db/archive/legacy/healpix_lod/` | 已归档（2026-07-16） | LOD 金字塔，被 healpix_browser_qt 内存 ud_grade 替代 |
| `lib/healpix_db/archive/legacy/tests/` | 已归档（2026-07-16） | 端到端集成测试，依赖已删除模块 |
| `lib/astro_image_io/src/ahpx/` | 已废弃 | 由 healpix_io .hiss 替代（healpix_io 已并入 astro_image_io） |
| `lib/plate_solve/archive/blind_solving/` | 已归档 | 含 astrometry_net 旧版盲解析（已废弃，仅用初始解析） |

### 3.3 模块内历史归档

各活跃模块内部含 `archive/` 子目录，存放开发过程中的历史脚本/日志：
- `lib/plate_solve/archive/`：blind_solving/、historical_logs/、historical_scripts/、debug_2026-07/
- `lib/orchestrator/archive/scripts/`：旧版 step1-4 脚本
- `lib/photometric_calib/`：old_gradient_tools/、old_monolithic/、old_python_photometric/、flux_calibrator/、shared/、spectrum_integrator/（含 ARCHIVED.md）
- `lib/dynamic_psf/archive/debug_2026-07/`：本次归档的 PSF 诊断脚本
- `lib/healpix_db/healpix_io/archive/`：healpix_io 源码归档（API 已并入 astro_image_io，healpix_browser_qt 依赖已迁移至 lib/astro_image_io/）

---

## 4. 数据流与格式体系

### 4.1 PipelineFrame 命名块

| 阶段 | 输入块 | 输出块 |
|------|--------|--------|
| READ_FITS | — | `data` (FLOAT32[H,W]), `header` (KV) |
| CALIBRATE | data, header | data ← 校准后, `cal_stats` (KV) |
| PLATESOLVE | data, header | header ← WCS/SIP, `star_det` (FLOAT32[N,4]), `gaia_cat` (FLOAT64[N,3]) |
| PSF | data, star_det | `psf` (FLOAT64[N,9])：x,y,fwhm,背景,A,mad,eccentricity 等 |
| PHOTOMETRIC | data, header, psf, gaia_cat | data ← 应用 scale 后（测光坐标系校准）, `photo_stats` (KV)：scale_factor, n_matched, sigma_residual |
| SNR | data, psf, photo_stats | `snr` (FLOAT32[H,W]) |
| DRIZZLE | data, header, snr | 输出 .hiss（含 snr 通道），清空所有块 |
| GRADIENT_SPHERE | N 个 .hiss | — |
| STACK | — | 输出 .hcsd |

### 4.2 格式体系（自 2026-07-13 统一）

- `.hiss`（HEALPix Storage System）：单帧存储，含 snr 通道
- `.hcsd`（HEALPix CS Database）：天球数据库，含子叶块索引（nside=64）支持浏览器按需加载
- 废弃格式：`.ahpx` / `.ahps` / `.ahpl`（由 healpix_io 替代）

---

## 5. 模块依赖关系

```
                            ┌─────────────────────┐
                            │  astro_image_io     │
                            │  (FITS/XISF)        │
                            │  PipelineFrame      │
                            └──────────┬──────────┘
                                       ▼
                            ┌─────────────────────┐
                            │     calibrate       │
                            └──────────┬──────────┘
                                       ▼
                            ┌─────────────────────┐
            gaia_xpsd ───►  │    plate_solve      │  ◄── star_detector
                            │  WCS/SIP 解析       │
                            └──────────┬──────────┘
                                       ▼
                            ┌─────────────────────┐
                            │   dynamic_psf       │
                            └──────────┬──────────┘
                                       ▼
       gaia_xpsd  ───►  ┌─────────────────────┐  ◄── spectrum_integrator
       (DR3SP 光谱)      │  photometric_calib  │      (F_syn 积分)
                         └──────────┬──────────┘
                                    ▼
                         ┌─────────────────────┐
                         │   snr_estimator     │
                         └──────────┬──────────┘
                                    ▼
                         ┌─────────────────────┐
                         │  healpix_drizzle    │ → .hiss
                         └──────────┬──────────┘
                                    ▼
                         ┌─────────────────────┐
                         │  healpix_stack      │ → .hcsd
                         └──────────┬──────────┘
                                    ▼
                         ┌─────────────────────┐
                         │ healpix_browser_qt  │ （当前浏览器）
                         └─────────────────────┘

                  ┌─────────────────────────────────┐
                  │         orchestrator            │
                  │  Python + C++ CLI 编排引擎      │
                  └─────────────────────────────────┘
```

---

## 6. 性能基线（单帧 4096×4096，16 线程，2026-07-13 实测）

| 阶段 | 耗时 |
|------|------|
| PLATESOLVE | 4.43 s |
| PSF | 0.44 s |
| PHOTOMETRIC | 0.88 s |
| DRIZZLE | 26.32 s |
| **总计** | **60.62 s** |

> 历史基线：PLATESOLVE 2.17 s / PSF 0.26 s / PHOTOMETRIC 0.03 s / DRIZZLE 18.44 s / 总计 20.90 s。
> PHOTOMETRIC 在 Galaxy_Center 测试帧上曾因循环内 fprintf + 滤光片重复预处理 + 锥形搜索返回过多星退化到 354.7 s，经 P0/P1/P2 三重优化恢复到 0.881 s（详见各模块 memory.md）。

---

## 7. 浏览器架构（当前版本：healpix_browser_qt）

> 详细 UI 架构见归档的 `docs/archive/UI_ARCHITECTURE_2026-07-16.md` §1.7-1.8 及 `lib/healpix_db/healpix_browser_qt/memory.md`。

**三层架构**：
```
core/   (无 Qt 依赖, 纯 C++17 + OpenGL 3.3, 可嵌入任何 C++ 项目)
  ├── browser_backend.h/.cpp  - .hiss/.hcsd 加载, 按需子叶, ud_grade 降采样
  ├── healpix_math.h/.cpp     - pix2ang_nest/ang2pix_nest/query_disc (NESTED, 三区域分块)
  ├── stf_engine.h/.cpp       - MTF + 4预设 + MAD自动拉伸 + GPU uniform 转换
  └── gl_renderer.h/.cpp      - OpenGL 3.3 Core 渲染 (wglGetProcAddress 加载 1.2+ 函数)

widgets/ (Qt6 QOpenGLWidget, 依赖 core)
  ├── abstract_view.h/.cpp    - QOpenGLWidget 基类
  ├── sphere_view.h/.cpp      - .hiss/.hcsd 统一球面 3D 渲染 (赤道仪相机导航 + LOD 金字塔)
  └── archive/single_frame_view.h/.cpp - 已废弃的 2D 切面投影 (大数据集卡死, 改用 SphereView)

app/     (demo exe, 依赖 widgets + core)
  ├── main.cpp                - QApplication 入口 (QCommandLineParser)
  ├── main_window.h/.cpp      - QMainWindow
  └── stf_panel.h/.cpp        - STF 控制面板 (4滑块 + 4预设 + 自动拉伸)
```

**启动方式**：`run_healpix.bat`（位于 `lib/healpix_db/healpix_browser_qt/`）配置 PATH/QT_PLUGIN_PATH 后启动 exe。

**关键约束**：
- core/ 零 Qt 依赖
- fact2 系数 = 1.0/(3*npface)（非 4.0/(3*npface)）
- nside=8192 需 uint64_t 避免 npface 溢出
- 相机：yaw 绕极轴 Z（赤经轴），pitch 绕赤纬轴 east，up 始终 north-up 重算（不携带不 roll）
- look_at_matrix column-major 存储：col0=(side.x, up.x, -fwd.x), col1=(side.y, up.y, -fwd.y), col2=(side.z, up.z, -fwd.z)
- MAX_FOV=50°（限制球面 yaw 畸变）
- nside_target 随 FOV 动态调整：fov=15.73° 用 nside=4096（HEALPix 像素 0.0143° < 屏幕像素 0.0211° 防止欠采样）

---

## 8. 开发环境

- OS: Windows
- 编译: MinGW64 (C:\msys64\mingw64\bin)
- Qt6: C:\msys64\mingw64\share\Qt6\plugins (QT_PLUGIN_PATH)
- CPU: 16 线程，64GB 内存
- Shell: PowerShell 7（强制 UTF-8 编码）
- 各模块独立 git 仓库，根目录非 git 仓库
