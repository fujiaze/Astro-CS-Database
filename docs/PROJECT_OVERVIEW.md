# Astro CS Normalization Database — 顶层工程概述

> 创建日期：2026-07-17（GAP-011/012/013/016/017 修复后状态） | 更新：2026-07-18（归档 GRADIENT_2D，stage1 重排为 7 节点）
> 用途：项目最顶层工程描述，供快速理解整体设计；细节见 `ARCHITECTURE.md`（架构盘点）与 `PIPELINE_OVERVIEW.md`（流程概述）。
> 设计与实现割裂记录见 `DESIGN_IMPL_GAP.md`。

---

## 1. 项目目标

将原始 FITS CCD 图像经校准、解析、测光定标、SNR 建模、HEALPix 重采样，汇入球面分块的标准化星图档案（.hiss 单帧 / .hcsd 天球数据库），实现多帧、多波段、全天球可浏览、可查询、可叠加的标准化数据库。

核心产出：
- `.hiss`：单帧 HEALPix 存储（含图像 + 稀疏 SNR 控制点）
- `.hcsd`：天球数据库（含子叶块索引，浏览器按需加载）

---

## 2. 核心架构原则

| 原则 | 说明 |
|------|------|
| **C++ 核心算法 + Python 调试层** | 性能敏感算法 C++17 DLL；Python 仅调试，调试完毕后由 C++ 替代 |
| **内存管线（PipelineFrame）** | 模块间通过命名块容器（astro_image_io 提供）在内存中传递数据，避免临时文件落盘 |
| **分模块独立仓库** | 每个核心模块独立 GitHub 仓库，本地 `lib/` 组织；根目录非 git 仓库 |
| **两段流水线** | stage1（FITS→.hiss，7 节点）+ stage2（.hiss→.hcsd，2 节点），两个 CLI 命令调用 |
| **稀疏 SNR 控制点** | 不存储稠密 SNR 图，用稀疏控制点 + 球面 IDW 重建（节省存储 + 按需展开） |

---

## 3. 两段流水线 9 节点（修复后实际状态）

### 第一段：单帧预处理（`orchestrator stage1`，FITS → .hiss）

| Stage | 节点 | 模块 | 关键能力（修复后） |
|-------|------|------|--------------------|
| 0 | READ_FITS | astro_image_io | 读取 FITS/XISF，构造 PipelineFrame（data + header 命名块） |
| 1 | CALIBRATE | calibration | Bias/Dark/Flat 校准 + 坏点修复（注：当前 orchestrator 走退化路径传 nullptr，待 GAP-020 修复） |
| 2 | PLATESOLVE | plate_solve | WCS/SIP 解析（向量匹配 + Umeyama SVD），输出 star_det + gaia_cat 块 |
| 3 | PSF | dynamic_psf | PSF 拟合（高斯/Moffat），OpenMP 16 线程并行，输出 psf 块 [N,9] |
| 4 | PHOTOMETRIC | photometric_calib | **F_syn = ∫ S(λ)·T(λ)·Q(λ) dλ**（含 CCD QE）+ IRLS+Tukey biweight 稳健回归求全局 scale，输出 photo_stats 块（scale_factor / n_matched / sigma_residual） |
| 5 | SNR | snr_estimator | **调用 snr_extract_model 提取稀疏控制点**，输出 snr_model 块（AIO_BLOCK_RAW：n_points + 控制点数组 + snr_phot + median_snr + idw_power） |
| 6 | DRIZZLE | healpix_drizzle | **NSIDE 自适应**（从 CD 矩阵算 pixel_scale，1x_to_2x 策略；用户可 nside_override 指定）+ 读 snr_model 块用 SnrEvaluator 重建逐像素 SNR + Drizzle 重采样 + .hiss 落盘 |

### 第二段：多帧合并（`orchestrator stage2`，.hiss → .hcsd）

| Stage | 节点 | 模块 | 关键能力（修复后） |
|-------|------|------|--------------------|
| 7 | GRADIENT_SPHERE | healpix_stack | `hp_stack_gradient_corrected` 一次完成：球面 TPS 采样 → Gauss-Seidel 梯度拟合 → 校正叠加 → .hcsd 输出 |
| 8 | STACK | healpix_stack | **Winsorized sigma clip**（缩尾 5%/95% 分位数 + 稳健均值/标准差）+ SNR²加权叠加；当前为空骨架（.hcsd 已由 stage 7 生成） |

---

## 4. 数据流（命名块容器）

```
READ_FITS      →  data(FLOAT32[H,W]) + header(KV: WCS/SIP)
CALIBRATE      →  data(校准后) + cal_stats(KV)
PLATESOLVE     →  header(WCS/SIP 更新) + star_det(FLOAT32[N,4]) + gaia_cat(FLOAT64[N,3])
PSF            →  psf(FLOAT64[N,9]: status,B,flux,cx,cy,fwhm,A,mad,eccentricity)
PHOTOMETRIC    →  data(标定后) + photo_stats(KV: scale_factor,n_matched,sigma_residual)
SNR            →  snr_model(RAW: 稀疏控制点序列化)  [修复后: 不再写 snr 稠密块]
DRIZZLE        →  读 snr_model 块 → SnrEvaluator IDW 重建逐像素 SNR → .hiss 落盘 + 清空所有块
GRADIENT_SPHERE→  读 N 个 .hiss → 球面梯度校准
STACK          →  Winsorized sigma clip + SNR²加权叠加 → .hcsd
```

**snr_model 块序列化格式**（与 hp_drizzle_api.cpp 期望一致）：
```
[n_points: uint32]
[points: n_points × 20B]  // SnrControlPoint: ra(double 8B) + dec(double 8B) + snr_psf(float 4B)
[snr_phot: double 8B]     // 1/(ln10×sigma_residual) 全局标量
[median_snr: double 8B]   // median(snr_psf) 归一化基准
[idw_power: double 8B]    // IDW 幂次（默认 2.0）
```

---

## 5. SNR 乘法模型（修复后端到端链路）

```
SNR(ra,dec) = SNR_phot × ( IDW_球面(控制点, query) / median_snr )
```

- **SNR_phot = 1/(ln10 × sigma_residual)**：帧级全局标量，来自 PHOTOMETRIC 阶段的 IRLS 稳健回归残差
- **稀疏控制点**：PSF 星位置 (cx,cy) 经 WCS 转球面 (ra,dec)，附带 snr_psf = (A-B)/mad
- **IDW 评估**：SnrEvaluator 用 nanoflann KD-tree K 近邻（K=16）+ 球面大圆距离 γ + 权重 w_i = 1/γ_i^idw_power

链路：
1. SNR 阶段：snr_extract_model 提取稀疏控制点 → 写 snr_model 块
2. DRIZZLE 阶段：读 snr_model 块 → SnrEvaluator.build() → 逐像素 evaluateBatch() 重建 SNR → Drizzle 时按 SNR²加权
3. STACK 阶段：Winsorized sigma clip + SNR²加权叠加

---

## 6. 模块清单（修复后活跃模块）

| 模块 | 职责 | GitHub 仓库 |
|------|------|------------|
| astro_image_io | FITS/XISF I/O + PipelineFrame 命名块容器 + 管线引擎 + zstd/lz4 压缩 | Astro-Image-IO-C |
| calibration | Bias/Dark/Flat 校准 + 坏点修复 + 主帧生成 | Astro-Calibration-Cpp |
| plate_solve | WCS/SIP 解析（向量匹配 + Umeyama SVD） | PlateSolve-IPV-Cpp |
| dynamic_psf | PSF 拟合（高斯/Moffat），OpenMP 并行 | 待建独立仓库 |
| photometric_calib | **F_syn 积分含 Q(λ)** + **IRLS+Tukey 稳健回归** + 全局 scale | Flux-calibration |
| photometric_calib/archive/gradient_2d | 【已归档 2026-07-18】原 step4 C++化曲面拟合实现，stage1 不再做曲面拟合和图像亮度修正（那是 stage2 马赛克阶段的事）；代码保留供 stage2 设计参考 | 随 photometric_calib |
| snr_estimator | **snr_extract_model 稀疏控制点提取**（snr_estimate 保留测试用） | Snr-Estimator-Cpp-Python |
| healpix_drizzle | Drizzle + NSIDE 自适应 + SnrEvaluator 重建 SNR → .hiss | Healpix-Drizzle-Cpp |
| healpix_stack | 球面梯度校准 + **Winsorized sigma clip** + SNR²加权叠加 → .hcsd | Healpix-Mosaic-Cpp |
| healpix_browser_qt | Qt6 + OpenGL 3.3 Core 浏览器（core/widgets/app 三层） | Healpix-Database |
| gaia_xpsd_client | Gaia DR3SP 光谱数据库客户端 | 待建独立仓库 |
| star_detector | 星点检测（plate_solve 调用） | 待建独立仓库 |
| orchestrator | 两段流水线编排引擎（10 节点 C++ CLI + Python 调试层） | Orchestrator-Cpp-Python |
| data_pipeline | 数据总线（拆分中间态，与 astro_image_io 同 API，待 GAP-006 处理） | Astro-Data-Pipeline |

---

## 7. 模块依赖关系

```
            ┌─────────────────────┐
            │  astro_image_io     │  ← 数据总线（PipelineFrame + 命名块）
            │  (FITS/XISF I/O)    │
            └──────────┬──────────┘
                       ▼
            ┌─────────────────────┐
            │     calibrate       │
            └──────────┬──────────┘
                       ▼
            ┌─────────────────────┐
gaia_xpsd ─►│    plate_solve      │◄─ star_detector
            │  WCS/SIP 解析       │
            └──────────┬──────────┘
                       ▼
            ┌─────────────────────┐
            │   dynamic_psf       │
            └──────────┬──────────┘
                       ▼
gaia_xpsd ─►┌─────────────────────┐
(DR3SP 光谱)│  photometric_calib  │
            └──────────┬──────────┘
                       ▼
            ┌─────────────────────┐
            │   snr_estimator     │  ← snr_extract_model 输出稀疏控制点
            └──────────┬──────────┘
                       ▼
            ┌─────────────────────┐
            │  healpix_drizzle    │  → .hiss（含 snr_model 块）
            └──────────┬──────────┘
                       ▼
            ┌─────────────────────┐
            │  healpix_stack      │  → .hcsd（Winsorized + SNR²加权）
            └──────────┬──────────┘
                       ▼
            ┌─────────────────────┐
            │ healpix_browser_qt  │
            └─────────────────────┘

       ┌─────────────────────────────┐
       │       orchestrator          │  ← Python + C++ CLI 编排引擎
       └─────────────────────────────┘
```

---

## 8. 配置文件（修复后）

### stage1_config.json 关键字段
- `frame.qe_curve`：CCD QE 曲线名称（如 "GSENSE2020BSI"），orchestrator 加载 qe_curves.json 传给 pc_calibrate_simple_with_gaia
- `drizzle.nside_strategy`：NSIDE 自适应策略（"1x_to_2x_drizzle" 默认 / "1x" / "2x" / "fixed"）
- `drizzle.nside_override`：用户指定 NSIDE（>0 优先于自适应，规整到 2 的幂次方，范围 [64, 131072]）

### stage2_config.json 关键字段
- `stack.sigma_clip_method`：离群值剔除方法（"standard" 默认 / "winsorized"）
- `stack.sigma_clip_sigma`：sigma 阈值（默认 3.0）
- `stack.sigma_clip_max_iter`：最大迭代次数（默认 5）
- `stack.winsorize_low_pct`：Winsorized 下分位数（默认 0.05）
- `stack.winsorize_high_pct`：Winsorized 上分位数（默认 0.95）

---

## 9. 性能基线（单帧 4096×4096，16 线程）

| 阶段 | 耗时 |
|------|------|
| PLATESOLVE | 4.43 s |
| PSF | 0.44 s |
| PHOTOMETRIC | 0.88 s |
| DRIZZLE | 26.32 s |
| **总计** | **60.62 s** |

> 修复后 PHOTOMETRIC 因加入 IRLS+Tukey 迭代 + CCD QE 积分，预计耗时略增（待实测）。
> SNR 阶段改为稀疏控制点提取，比稠密 SNR 图生成更快。

---

## 10. 待办（未处理的 GAP）

| GAP | 描述 | 优先级 |
|-----|------|--------|
| GAP-006 | data_pipeline 从 astro_image_io 拆分未完成 | 高 |
| GAP-008 | 4 处断层：drizzle 落盘 / hiss 格式 / Python 绑定 / stack 加权 | 高-中 |
| GAP-014 | stage1 节点拆分文档与实际不符（PIPELINE_OVERVIEW.md 待更新；部分修复：GRADIENT_2D 已归档 2026-07-18） | 中 |
| GAP-015 | stage2 4 步合并为 1 函数 + STACK 空骨架（文档待更新） | 中 |
| GAP-018 | 区域 SNR 拟合 vs IDW 评估（术语差异） | 低 |
| GAP-019 | data_pipeline 数据总线未被 orchestrator 使用 | 低 |
| GAP-020 | 基础校准退化路径（orchestrator 传 nullptr） | 低 |

---

## 11. 开发环境

- OS：Windows
- 编译：MinGW64（C:\msys64\mingw64\bin）
- Qt6：C:\msys64\mingw64\share\Qt6\plugins
- CPU：16 线程，64GB 内存
- Shell：PowerShell 7（强制 UTF-8 编码）
- 各模块独立 git 仓库，根目录非 git 仓库
