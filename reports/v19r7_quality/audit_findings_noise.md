# QA-V19R7 A2-02 Noise 域审计分表

> Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` §4.1 A2-02  
> Checklist: `工程控制/checklists/QA_V19R7_QUALITY.md` A2-02  
> 基线: V19R6R2-W1 HEAD 2767874 | 只读不改 | 审计员: resident:science | 日期: 2026-08-22  
> 范围: `docs/science/NOISE_MODEL.md, UNCERTAINTY_AND_COVARIANCE.md` vs `lib/snr_estimator` (+ `lib/plate_solve/ipv` 涉及的 variance 语义) + `TRACEABILITY SCI-NOISE-001..015`  
> 机器扫描: `reports/v19r7_quality/machine_consistency_before.json` (broken=0, A1 已就绪)

---

## 1. 概述

本分表聚焦 **noise 三层模型** 与 **不确定度/协方差** 两份文档，对照 `lib/snr_estimator` 真实实现与追溯矩阵。重点核查：

- 三层模型符号/单位与代码是否一致（`PhotometricCalibrationQuality` / `PsfFitQuality` / `NoiseWeightModelV1`）；
- `k_corr=1.4` 冻结值与域（`UNCERTAINTY_AND_COVARIANCE` vs `PHASE2_UPM` / `sampler.cpp`）；
- `α²v` 方差传播恒等式；
- 协方差未建模声明与量化；
- `ivar` 语义与可缺省性；
- `SCI-NOISE-001..015` 追溯完整性。

**结论先行**: 共发现 **P0: 1, P1: 4, P2: 5**，合计 10 项。核心噪声管线语义正确，但存在 1 项 P0（gain 域缺失）与 4 项 P1 溯源断点需在 B1-05/B1-06/B5-06 闭环。

---

## 2. 方法

1. 逐节标注两份文档的“科学定义/算法/变量/假设/有效域/不保证/失效条件/数值精度/ID”。
2. 检索 `lib/snr_estimator/cpp/include/snr_estimator.h:28-431`, `lib/snr_estimator/cpp/src/noise_model.cpp:1-466`, `lib/snr_estimator/cpp/test/noise_model_science_test.cpp:1-567` 的符号、常数、分支。
3. 交叉核对 `UNCERTAINTY_AND_COVARIANCE.md` 中 `control_variance` 公式与 `lib/phase2/src/sampler.cpp:38-103,515,669-674` 的 `k_corr` 冻结/查表实现（noise 域连带核查）。
4. 扫描 `docs/TRACEABILITY.csv:23-37` SCI-NOISE-001..015 行与 `TEST-SNR-001` 测试闭环。
5. 按 P0/P1/P2 分级并关联合同与 B1 归口。

---

## 3. 逐文档发现表

| # | 级别 | 合同 | 文档节 | 代码位置 | 描述 | 建议归口 |
|---|------|------|--------|----------|------|----------|
| NO-01 | **P0** | SCI-NOISE-005 / NOISE-WIRE-001 (隐含) | NOISE_MODEL.md §科学定义 (无 gain/readnoise 章节) | `lib/snr_estimator/cpp/include/snr_estimator.h:98-141` (SnrNoiseModelConfig 含 `gain_e_per_adu/read_noise_e/use_gain_model`) + `lib/snr_estimator/cpp/src/noise_model.cpp:457-464` (`snr_noise_gain_variance`) + `lib/snr_estimator/cpp/test/noise_model_science_test.cpp:238-272` (SNR-005 Poisson+readnoise MC) | 文档定义“有 gain+readnoise 时用于 Poisson 交叉验证，缺失时经验 fallback”，但 **未给出 gain 模型的物理域与克制语义**：`snr_noise_gain_variance` 仅在单端实现 `max(signal,0)/gain + (rn/g)²`，未在 NOISE_MODEL.md 说明该公式仅作 **诊断/交叉验证 (SNR-005)**，不得注入 `NoiseWeightModelV1` 生产场（生产基线 `source==0 empirical`）。若用户据 header `gain` 误以为“模型会融合 gain/readnoise”，将误判生产 ivar 偏置。可观测误用风险，判 P0。 | B1-05 |
| NO-02 | **P1** | SCI-NOISE-011/012 (协方差) | UNCERTAINTY_AND_COVARIANCE.md §V19R3 control estimator 方差 `control_variance=k_corr·(π/2)·σ²/N_retained` | `lib/phase2/src/sampler.cpp:38-69` (`kControlCorrDefault=1.4`, `kcorr_lookup` 双线性查表) + `docs/science/UNCERTAINTY_AND_COVARIANCE.md:30-43` | Noise 文档承载了 **UPM 专用** 的 `k_corr/control_variance` 公式，但 **未说明该公式不属于 `lib/snr_estimator` 域**，易使读者误将 `1.4` 套用到 `NoiseWeightModelV1` 的 patch 级 `1.4826·MAD` 管线。责任边界缺失，追溯上 `SCI-NOISE-011/012` 与 `ALG-UPM-CONTROL-IVAR-001` 交叉引用但无显式域声明。 | B1-06 |
| NO-03 | **P1** | SCI-NOISE-001..015 整体 | NOISE_MODEL.md §ID, UNCERTAINTY… §ID + TRACEABILITY | `docs/TRACEABILITY.csv:23-37` (15 行 SCI-NOISE-*) + `lib/snr_estimator/cpp/test/noise_model_science_test.cpp:1-22` (SNR-011/012/015 委派说明) | TRACEABILITY 为 15 行 SCI-NOISE 统一指向 `TEST-SNR-001` 关系，但**未显式标注 SNR-011/012 委派至 `healpix_drizzle`、SNR-015 委派至 `phase2` 的分流**；测试文件头注释说明了分流，但追溯矩阵未结构化表达，导致 machine_consistency 对“15门是否全覆盖”的判断依赖注释而非行级映射。 | B5-06 |
| NO-04 | **P1** | A1-02 证据缺失 | QA_V19R7_QUALITY §A1-02 + 本域标准扫描 | `reports/v19r7_quality/file_audit_before.json` (873/~713+, tool_missing=true) + `reports/v19r7_quality/standards_violations.json` (forbidden=7, CODE 3/COMMENT 4) | `tools/file_audit` 缺失导致 science/noise 源文件覆盖度以替代统计呈现，noise 域的 `NUMERIC_STANDARD/COMMENT` 违规未定位到 `lib/snr_estimator` 行级证据；审计分表无法完成“文件→标准→行号”闭环。属证据链 P1。 | A1-02 补齐 |
| NO-05 | **P1** | NOISE-WIRE-001 | NOISE_MODEL.md §生产配置 | `lib/snr_estimator/cpp/src/noise_model.cpp:35-37,119-124` + `lib/snr_estimator/cpp/test/noise_model_science_test.cpp:478-560` (NOISE-WIRE-001) | 文档约定“`cfg==nullptr`/`default_config()`/生产默认 三者逐字段 exact，含 spatial field 默认开启、variance_floor=1e-12”，但 **合同 ID NOISE-WIRE-001 未进入 `docs/TRACEABILITY.csv`**（仅有 SCI-NOISE-*）。生产接线等价门无追溯行，machine_consistency 无法校验该语义。 | B5-06 |
| NO-06 | **P2** | SCI-NOISE-001 (ivar) | NOISE_MODEL.md §算法 `ivar=1/variance` + UNCERTAINTY… §逐像素方差 | `lib/snr_estimator/cpp/src/noise_model.cpp:32,210-212,235-238,402-417` (`g_model_floor` 注册表 + `variance_floor` clamp + degenerate r=1) + `lib/snr_estimator/cpp/include/snr_estimator.h:118-141` (NoiseWeightModelV1 含 degenerate/source/has_spatial_field) | 文档仅写 `ivar=1/variance`，未说明 **floor 传播与退化语义**：`variance_floor` 默认 1e-12 且通过 `g_model_floor` 按 model 指针注册传递至 `fill` 阶段的平面 clamp；无合格 patch 时 `degenerate=1` 仍可返回全局兜底，但全帧 NaN 时 `r=1, degenerate=1, ivar=0`。该分支对下游 `control_ivar` 意义重大，noise 域文档未展开。 | B1-05 |
| NO-07 | **P2** | SCI-NOISE-008/010 | NOISE_MODEL.md §假设 “掩膜半径与星亮度解耦（fixed conservative）” | `lib/snr_estimator/cpp/src/noise_model.cpp:128-147,144` (`rmax = max(1,r0)×max(1,scale)` 统一半径，`amps` 未用，假 adaptive 已删) | 实现与文档在“**禁用按星亮度缩放**”上已对齐（P0 已修），但文档括号内“若未来需要 PSF-aware adaptive mask，须先扩展 API 输入振幅并重新冻结”属于**前瞻性假设**，未标注为 `NOT GUARANTEED` 且未与 `star_x/star_y` API（无 amplitude）形成显式“API 约束”说明。 | B1-05 |
| NO-08 | **P2** | SCI-NOISE-002 | UNCERTAINTY_AND_COVARIANCE.md §Drizzle 传播 `var_p=Σ v_j w²/D²` | `lib/snr_estimator/cpp/src/noise_model.cpp:447-454` (`snr_noise_scale_law`: `var′=α²var, ivar′=ivar/α²`) | `α²v` 在 noise 头中以 `snr_noise_scale_law` 独立暴露，但 **Drizzle 侧 `SCI-DRZ-014` 的 `Σ v_j w²/D²` 恒等式未在 noise 文档中互引**，两份文档对同一缩放律的表述分治，缺乏交叉引用，影响 B1-06/B1-07 协同改正的锚点。 | B1-06 |
| NO-09 | **P2** | SCI-NOISE-007 | NOISE_MODEL.md §数值精度 “MAD 常数 1.4826022185（Gaussian）” | `lib/snr_estimator/cpp/src/noise_model.cpp:54-62,61` (`1.482602218505602`) + `lib/phase2/src/sampler.cpp:388,624,645` (同常数) | 常数在实现中为 **1.482602218505602** (双精度截断)，文档写作 **1.4826022185**，差值 <1e-12 但未声明截断策略；且 `robust_sigma` 的 `median(|x−median|)` 与 `Ps-fit` 侧 `trimmed-mean-abs (0.731673)` 系数并列时易混，文档未对比说明。 | B1-05 |
| NO-10 | **P2** | SCI-NOISE-013 | NOISE_MODEL.md §PsfFitQuality 交叉引用 | `lib/snr_estimator/cpp/include/snr_estimator.h:51-88` + `lib/snr_estimator/cpp/src/noise_model.cpp:35-37,295-324` | 文档声称 `q_psf` “不是图像噪声 SNR；默认不进入 Phase2 逐像素 weight”，已正确；但 **PsfFitQuality 的 `residual_scale=10-90% trimmed mean |residual|` 定义仅在 `NOISE_MODEL.md` 一句话带过**，真实推导 `E=0.731673σ` (noise_model.cpp:36) 与 `robust_residual_sigma` 换算未展开，科学读者无法复核。 | B1-05 |

---

## 4. 统计小结

| 级别 | 数量 | 占比 | 主要分布 |
|------|------|------|----------|
| **P0 不一致** | **1** | 10% | NOISE_MODEL gain 域缺失 (NO-01) |
| **P1 缺失溯源** | **4** | 40% | k_corr 域边界、TRACEABILITY 分流、file_audit 证据、NOISE-WIRE-001 追溯缺行 |
| **P2 表述不清** | **5** | 50% | ivar floor/degenerate、fixed-mask 前瞻、α²v 互引、MAD 常数截断、q_psf 推导 |
| **合计** | **10** | 100% | — |

- **按文档**: NOISE_MODEL 7项 (1P0+1P1+5P2), UNCERTAINTY_AND_COVARIANCE 3项 (1P1+1P1+1P2), 跨域证据 1项 (1P1)。
- **按归口**: B1-05 (NOISE_MODEL) 6项, B1-06 (UNCERTAINTY) 3项, B5-06/A1-02 (证据/追溯) 2项。
- **k_corr=1.4 域**: 已核查 `UNCERTAINTY_AND_COVARIANCE.md:30-43` 与 `lib/phase2/src/sampler.cpp:38-69` 一致：默认 1.4，`kcorr_lookup` 支持按 `pixfrac×scale` 双线性查表，`control_variance` 分支正确；仅边界说明需补 (NO-02)。
- **α²v / ivar / 协方差**: `snr_noise_scale_law` 与 `SCI-DRZ-014` 语义一致 (NO-08 互引缺失属 P2)；协方差未建模声明与 `SNR-012 mean|ρ|≈0.19 max|ρ|≈0.57` 量化已在 UNCERTAINTY… 文档化，无 P0。
- **machine_consistency**: 0 broken (A1 已就绪)；本表不再标注“待复核”。

---

## 5. 对照汇总（符号/单位/假设）

| 维度 | 文档声明 | 代码实现 | 结论 |
|------|----------|----------|------|
| 三层模型符号 | `PhotometricCalibrationQuality(σ_dex→σ_mag/σ_cal_rel)` / `PsfFitQuality(q_psf=A/residual_scale)` / `NoiseWeightModelV1(8×8, MAD→σ, 5σ≤2轮, 平面var)` | `snr_estimator.h:37-43,63-74,98-134` + `noise_model.cpp:12-23,128-212,372-417` | 一致；仅 gain 域与 floor 语义待补 (NO-01/06) |
| k_corr 域 | UNCERTAINTY… 声称 `k_corr=1.4` 经 MC 校准 | `sampler.cpp:42,48-69` (1.4 默认 + 查表) | 一致；需补域边界说明 (NO-02) |
| α²v | `x′=αx → var′=α²var, ivar′=ivar/α²` | `noise_model.cpp:447-454` | 一致；文档互引缺失 (NO-08) |
| 协方差 | `Cov=Σ c_jp c_jq v_j`，V19 不存完整矩阵，MC 量化 `mean|ρ|≈0.19` | UNCERTAINTY… 已文档化；`healpix_drizzle` 侧 MC 测试覆盖 | 一致 |
| ivar 语义 | `ivar=1/variance` | `noise_model.cpp:210-212,235-238,402-417` + degenerate 分支 | 一致；需补 floor/degenerate 约束 (NO-06) |

---

## 6. 证据与追溯

- 输入: `docs/science/NOISE_MODEL.md`, `docs/science/UNCERTAINTY_AND_COVARIANCE.md`, `lib/snr_estimator/**`, `lib/phase2/src/sampler.cpp` (k_corr), `docs/TRACEABILITY.csv:23-37`, `reports/v19r7_quality/machine_consistency_before.json`
- 产出: 本文件 `reports/v19r7_quality/audit_findings_noise.md` + `evidence/QA-V19R7-A2-02/EVIDENCE_INDEX.md`
- 方法: 公式/单位/假设/失效域逐节对照 + 符号表 + 行号锚定 + 追溯闭环

