# SNR 模块设计 + §12 梯度校正设计调整 + §13 PSF 统一调整

> **任务类型**: 工程设计文档更新（不写实现代码）
> **状态**: 待用户确认
> **日期**: 2026-07-15

## 任务目标

更新 `PROJECT_ARCHITECTURE.md`，落实 iterative-discussion 阶段确认的所有设计决策。本次**只更新工程设计文档**，不写实现代码（后续逐步实现，每个模块独立 spec）。

## 已确认的设计决策

### 决策 1: SNR 估算模块（新增 §14）

**模块定位**: `lib/snr_estimator/` 独立 C++ 模块（符合"分模块开发"规则）

**管线阶段**: 新增 `STAGE_SNR`（PHOTOMETRIC 后、DRIZZLE 前）

**算法**: 乘法模型（已严谨数学推导确认）

```
SNR(pixel) = SNR_phot × (SNR_psf(pixel) / median(SNR_psf))

其中:
  SNR_phot = 1 / (ln10 × sigma_residual)                    # 帧级基线, 来自测光
  sigma_residual = MAD(log10(F_instr/F_syn)) / 0.6745        # star_matcher 已算未输出
  SNR_psf(pixel) = IDW(PSF星位置 (A-B)/mad)                  # 局部, 来自 PSF
  median(SNR_psf) = 中位数归一化                              # 空间修正因子
```

**数学推导要点**:
- `log10(F_instr/F_syn) ≈ (F_instr-F_syn)/(F_syn×ln10)` → `sigma_F_rel ≈ ln10 × sigma_residual`
- `SNR_phot = 1/sigma_F_rel` 为帧平均测光信噪比（无量纲）
- 噪声分解 `noise(pixel)=noise_global×alpha(pixel)` → 乘法模型自然导出
- 乘积无量纲，可跨光学系统/天区/帧通用

**输入**:
- `psf` 块（需扩展 A/B/mad 列，见决策 4）
- `photo_stats`（需扩展 sigma_residual 输出，见决策 8）

**输出**: `snr` 块（FLOAT32[H,W]）

**边界处理**:
- `sigma_residual → 0`（完美校准）: `SNR_phot_max = 1000` 上限兜底
- 无 PSF 星区域: `SNR_psf(pixel)` 退化到 `median(SNR_psf)=1.0`，`SNR(pixel)=SNR_phot`（用帧平均）
- `N_matched < 10`: sigma_residual 不可靠 → 兜底 `SNR=1.0`

**IDW 插值参数**: 6 邻居，power=2

### 决策 2: §12 新增 §12.15 SNR 耦合与隔离设计

> **状态: 已确认 — SNR 单次计算，与梯度严格解耦**
>
> 2026-07-15 讨论确认：SNR 与梯度在物理上严格独立，不需要外挂/迭代/重拟合 PSF。

**SNR 不受梯度影响的严格推导**（2026-07-15 讨论结论）:

1. **测光不确定度（sigma_residual）不受梯度影响**:
   - 孔径测光的环形背景邻域拟合 `local_sky + local_gradient` → 梯度作为局部背景被扣除
   - `F_instr = star_flux`（纯星点流量，无梯度）
   - `sigma_residual = MAD(log10(F_instr/F_syn))` 基于纯 star_flux → 不受梯度影响

2. **PSF SNR 不受梯度影响**:
   - Moffat 拟合 `f = A×(1+(r/R)²)^(-β) + B`，B 拟合了 `local_sky + local_gradient`
   - A（振幅）= 扣除 B 后的星点峰值 → 不含梯度
   - mad（残差 MAD）= 纯噪声 → 不含梯度
   - `snr_i = (A-B)/mad` 或 `A/mad`（取决于 convention）→ 不含梯度

3. **结论**: SNR_phot 和 SNR_psf 都不受梯度影响 → **SNR 与梯度严格独立，解耦不是近似而是精确的**

**两个 SNR 概念区分**（已确认）:
- **SNR-A（梯度拟合权重）**: 来自样本表 `snr` 字段，建议 `median/(1.4826×MAD)`（与星等解耦）
- **SNR-B（叠加权重）**: 来自 `DrizzlePixel.weight`（drizzle 阶段算，基于 per-pixel SNR 模型）

**执行方案: SNR 单次计算 + 梯度用固定 SNR 权重**:
```
STAGE_SNR（前置，photometric后，计算一次）
  ├─ SNR_phot = 1/(ln10×sigma_residual)        ← 帧级，不受梯度影响
  ├─ SNR_psf(pixel) = IDW(PSF A/mad)            ← 局部，不受梯度影响
  └─ SNR(pixel) = SNR_phot × (SNR_psf/median)   ← 固定值，写入 snr 块
     ↓ drizzle 写入 .hiss snr 通道（固定值）

STACK 阶段（固定 SNR，不迭代重算）
  ├─ 阶段1: 采样 → SNR-A（固定权重）
  ├─ 阶段2: 梯度拟合（SNR-A 加权，Gauss-Seidel 迭代）→ 收敛得 g_i
  └─ 阶段3: 校正叠加（扣除 g_i，SNR-B 加权）→ .hcsd
```

**核心优势**:
- SNR 计算一次，简单稳定
- 梯度拟合用固定权重 → Gauss-Seidel 确定性收敛（不会因 SNR 变化震荡）
- 帧间梯度耦合通过固定权重解耦
- SNR 模块与梯度校正模块独立，代码复杂度低

**叠加阶段 SNR² 最优加权**（2026-07-15 讨论确认）:

**理论依据**: SNR² 加权 = inverse-variance weighting = 最大似然估计（MLE）的最小方差无偏估计（MVUE）
```
假设: x_i = s + noise_i, noise_i ~ N(0, σ_i²)
MLE: ŝ = Σ(x_i/σ_i²) / Σ(1/σ_i²) = Σ(w_i × x_i) / Σ(w_i), w_i = 1/σ_i²
SNR_i = s/σ_i → σ_i² = s²/SNR_i² → w_i = SNR_i²/s²
归一化后: w_i ∝ SNR_i²   ← SNR² 加权
```

**双层 SNR 保护**（解决"低 SNR 污染高 SNR"问题）:
1. **阶段2 梯度拟合用 SNR-A 加权** → 低 SNR 控制点权重小，不污染梯度曲面 g_i
2. **阶段3 叠加用 SNR-B² 加权** → 低 SNR 帧叠加权重小，不抬高叠加结果

**叠加执行顺序**（关键: 梯度迭代与 sigma-clip 串行，不嵌套）:
```
阶段2: 梯度迭代拟合（Gauss-Seidel，SNR-A 加权）
  → 收敛后得到稳定的 g_i（梯度曲面）

阶段3: 校正叠加（用收敛的 g_i，一次性执行）
  for each frame:
    corrected = pixel - g_i(ra, dec)         ← 梯度校正（改变像素值，这是期望行为）
    → sigma-clip（基于校正后像素值，剔除流星/卫星等瞬态离群）
    → 保留像素 SNR-B² 加权平均
  result = Σ(SNR_B_i² × corrected_i) / Σ(SNR_B_i²)
```

**为什么梯度校正与 SNR² 加权不耦合**:
1. SNR 是固定值（不受梯度影响，前面已确认）→ 权重 SNR² 不随梯度变化
2. 梯度 g_i 收敛后是稳定值 → 校正量确定
3. sigma-clip 用收敛后 g_i 一次性判定 → 不需要迭代
4. 梯度校正改变像素值是**期望行为**（校正目的就是扣除梯度对叠加结果的贡献）
5. 梯度拟合用 SNR-A 加权（控制点级），叠加用 SNR-B² 加权（像素级），两者独立

**验证标准补强**: §12.14 "低 SNR 帧不抬高高 SNR 帧光度"通过双层保证：
1. 阶段 2: SNR-A 加权 TPS 拟合，低 SNR 帧梯度估计噪声不污染高 SNR 帧
2. 阶段 3: SNR-B² 加权叠加，低 SNR 帧叠加权重小

### 决策 3: §12.13 文件名修正

**现状问题**: §12.13 列的 `existing/sigma_clip_stack.h/.cpp` 不存在，当前实际是 `stack_engine.h/.cpp` + `hp_stack_hiss.h/.cpp`

**修正**:
- `existing/sigma_clip_stack` → `existing/stack_engine` + `existing/hp_stack_hiss`

**gradient/ 子目录 5 文件确认**（全部待创建）:
- `gradient_sampler.h/.cpp`（阶段1 采样）
- `spherical_tps.h/.cpp`（球面 TPS）
- `gradient_fitter.h/.cpp`（阶段2 拟合）
- `gaia_rejector.h/.cpp`（Gaia 星拒绝）
- `corrected_stacker.h/.cpp`（阶段3 校正叠加）

### 决策 4: §13 PSF 统一调整

**保留**: platesolve 不动（稳定工作，用 star_detector 检测星点）

**psf 块生命周期延长**:
- 当前: PHOTOMETRIC 阶段清理 psf 块
- 调整: STAGE_SNR 后清理（SNR 模块需要 psf 的 A/B/mad）

**psf 块格式扩展**:
- 当前: `FLOAT64[N,6]`（status, B, flux, cx, cy, fwhm）
- 扩展: `FLOAT64[N,9]`（加 A, mad, eccentricity 供 SNR 用）
- 注: B 已有，新增 A/mad/eccentricity

**PSF 统一核心**: SNR 复用 PSF 的 A/B/mad 字段（已有调研确认 PSF 已是"一次拟合多处复用"模式）

### 决策 5: 管线阶段调整

**新增 STAGE_SNR**:
```
CALIBRATE=0 → PLATESOLVE=1 → PHOTOMETRIC=2 → SNR=3 → DRIZZLE=4 → STACK=5
```

**块生命周期调整**:
- `psf` 块: PHOTOMETRIC 清理 → STAGE_SNR 后清理
- `snr` 块: STAGE_SNR 生成 → DRIZZLE 后清理
- `photo_stats`: 需扩展 sigma_residual 字段

### 决策 6: .hiss 格式扩展

**新增 SNR 通道**:
- 当前: `ipix(uint64) + brightness(float32)`
- 扩展: `ipix(uint64) + brightness(float32) + snr(float32)`
- 可选: `+ weight(float32)`（供 stack 加权用，待 stack 实现时确认）

**SNR 计算公式**（drizzle 写出时）:
```
snr = sqrt(sumSnrSq / sumWeight)   # 加权 RMS SNR（当前公式，已知多帧不提高问题）
```

**FORMAT_SPEC.md 更新**: 新增 snr 通道说明

### 决策 7: drizzle 累加公式问题（记录为已知问题）

**问题**: `SNR=sqrt(sum_snr_sq/sum_weight)` 在 N 帧同 SNR=S, weight=1 时给出 `sqrt(N×S²/N)=S`，不提高（物理应为 `sqrt(N)×S`）

**正确公式**（待 stack 实现时修正）:
```
SNR_stacked = Σ signal_i / sqrt(Σ (signal_i/SNR_i)²)
```

**本次处理**: 仅在文档中记录为已知问题，不修正（待 stack 阶段实现时一并修正）

### 决策 8: photometric_calib 接口扩展

**当前**: `pc_calibrate_simple` 输出 `out_pixels, out_n_matched, out_scale_factor`

**扩展**: 新增输出 `out_sigma_residual`（double*）
- star_matcher 内部已计算 `sigma = MAD/0.6745`（用于清洗），只需暴露
- 供 SNR 模块计算 `SNR_phot = 1/(ln10×sigma_residual)`

### 决策 9: hp_stack_hiss 前置依赖

**当前**: `hp_stack_hiss.cpp` 是无权重 sigma-clip（纯 mean+std）

**需扩展**: SNR-B² 加权 sigma-clip（用 .hiss 的 snr 通道）
- 这是 §12 梯度校正集成的前置依赖
- §12.10 "校正叠加集成"需基于此扩展

### 决策 10: SNR² 加权贯通 4 处断层修复（记录为待实现项）

**现状**: SNR² 最优加权在 .hiss → .hcsd 新路径上完全未贯通，存在 4 处断层:

| 断层 | 位置 | 问题 | 修复 |
|------|------|------|------|
| 1 | drizzle_engine.cpp writeHis | 只输出 sumFlux，丢弃 sumWeight/sumSnrSq | 输出 snr/weight 数组 |
| 2 | healpix_io.h/.cpp hiss_write/read | FORMAT_SPEC 已设计 has_snr 字段，但 C API/Python 绑定未实现 | 实现 snr 通道读写 |
| 3 | healpix_io.py Python 绑定 | 只绑 ipix/pixel | 同步扩展 snr 参数 |
| 4 | hp_stack_hiss.cpp | 等权 mean+std sigma-clip，无 weightedMedian/weightedMAD | 改为 SNR-B² 加权 sigma-clip |

**复用机会**: 旧版 `stack_engine.cpp` 已有 weightedMedian + weightedMAD 实现，可移植到 hp_stack_hiss

**DrizzlePixel 结构体已有字段**（stack_engine.h）:
```cpp
struct DrizzlePixel {
    uint64_t healpixPix;
    float    value;
    float    snr;      // 已定义
    float    weight;   // 已定义（已含 SNR）
};
```

**本次处理**: 仅在文档中记录为待实现项，不在本次文档更新中实现代码

### 决策 11: PROJECT_ARCHITECTURE.md 整体重构

**重构动机**: 用户指出项目架构文档包含不应出现的内容（性能记录、归档文件记录、UI 设计），应聚焦数据流/后端

**重构原则**: PROJECT_ARCHITECTURE.md 是**数据流 + 后端架构**文档，非项目记忆或模块记忆

**移出内容**:

1. **性能记录** → 移到对应模块 memory.md
   - §1 典型性能表（PLATESOLVE/PSF/PHOTOMETRIC/DRIZZLE 耗时）→ `lib/orchestrator/memory.md`
   - §11 性能优化记录（P0/P1/P2、drizzle 缝隙修复、浏览器性能修复）→ 各模块 memory.md
   - §11.4 Drizzle 黑色缝隙修复 → `lib/healpix_db/healpix_drizzle/memory.md`
   - §11.5 浏览器性能/视觉修复 → `lib/healpix_db/healpix_browser_qt/memory.md`

2. **UI 设计相关** → 移到 UI 架构文档（新建根目录 `UI_ARCHITECTURE.md`）
   - §10 浏览器架构全部（§10.1-§10.8）→ `UI_ARCHITECTURE.md`
   - 包括: Qt6+OpenGL 架构、core/widgets/app 三层、STF 引擎、LOD 动态 nside、双击启动部署等

3. **归档/废弃模块详细记录** → 移到 `lib/healpix_db/memory.md`
   - §2.1 模块清单中已废弃/归档模块的详细信息（healpix_lod/healpix_browser_cpp/healpix_browser_web/healpix_browser/ahpx_io）
   - PROJECT_ARCHITECTURE.md 只保留一句"已废弃/归档，详见 lib/healpix_db/memory.md"

**保留内容**（PROJECT_ARCHITECTURE.md 聚焦）:
- §1 项目概述 + 端到端流程
- §2 模块清单（活跃模块）+ 依赖关系图
- §3 数据流图
- §4 PipelineFrame 命名块容器规范
- §5-§8 各模块接口规范（数据流相关）
- §9 格式体系（.hiss/.hcsd）
- §10 → 重编号为"管线阶段定义"（CALIBRATE→PLATESOLVE→PSF→PHOTOMETRIC→SNR→DRIZZLE→STACK）
- §11 → 重编号为"梯度校正架构"（原 §12）
- §12 → 重编号为"PSF 流程"（原 §13）
- §13 → 重编号为"SNR 模块"（原 §14 新增）
- 附录: 目录结构（仅活跃模块）

**新增内容**（本次讨论确认）:
- 新增章节: SNR 模块设计（乘法模型、STAGE_SNR、解耦推导）
- 新增章节: 梯度校正与 SNR 耦合设计（双层 SNR 保护、SNR² 最优加权、4处断层）
- 新增章节: SNR² 加权贯通 4 处断层修复计划

**文档整体检查维度**（"传递之类的"优化）:

在重构 PROJECT_ARCHITECTURE.md 时，需对数据流做以下 4 类检查，发现问题记录在文档中标注"待优化":

1. **传递冗余**: 模块间不必要的重复传递（如同一块被多阶段重复传递但中间未使用）
2. **传递缺失**: 该传的没传（如某模块需要某数据但管线未传递，导致重复计算或无法工作）
3. **反复计算**: 多模块重复计算同一内容（如 star_detector 算了 per-star SNR 但未导出，SNR 模块又要重算）
4. **计算未导出**: 需要的内容内部计算了却未暴露（如 star_matcher 内部算了 sigma_residual 用于清洗但未输出，photometric_calib 需要但拿不到）

**已知问题**（本次调研发现，需在文档中标注）:
- star_detector 算了 per-star SNR 未导出 → SNR 模块需从 PSF 重新计算
- star_matcher 内部算了 sigma_residual 未暴露 → photometric_calib 需扩展接口（决策8）
- drizzle 内部累加了 sumSnrSq 但落盘丢弃 → 4处断层之一（决策10）
- .hiss 格式设计了 has_snr 但未实现 → 4处断层之一（决策10）

## 不包含（本次文档更新不涉及）

- 实际代码实现（后续逐步实现，每个模块独立 spec）
- drizzle 累加公式修正（待 stack 实现时）
- §12 梯度校正代码实现（待工程设计确认后）
- SNR 模块代码实现（待文档确认后）
- 4 处断层代码修复（待后续实现）

## 验证标准

1. PROJECT_ARCHITECTURE.md 章节完整性（新增 SNR 章节、梯度校正耦合设计，修正 §12.13、§13）
2. 设计一致性（SNR-A/SNR-B 区分明确、模块间接口清晰、双层 SNR 保护、SNR² 最优加权）
3. 与现有代码现状对齐（4 处断层记录、DrizzlePixel 字段、FORMAT_SPEC 设计）
4. 文档职责清晰（PROJECT_ARCHITECTURE.md 聚焦数据流/后端，UI 移到 UI_ARCHITECTURE.md，性能/归档移到模块 memory.md）
5. memory.md 更新（记录本次设计决策 + 文档重构）
