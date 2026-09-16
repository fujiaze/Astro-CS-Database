> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# Phase3 算法实施规格（ALG-P3-001）

> 文档 ID：`ALG-P3-001`（本目录为 W3 任务 ALG-P3-001 交付物）
> 状态：`ALG_PROPOSED`（算法层规格；正式冻结由 W4 `CONTRACT-FREEZE-001` 写入 `docs/algorithms/v6/frozen/`）
> 任务：ALG-P3-001（wave 3，`depends_on: SCI-ADJ-001`）
> 上位：`ASTROCS-PROJECT-SPEC-002` §6/§7/§8/§9 → `DESIGN-P3-001` §1–§7 → `ASTROCS-SCIENCE-MODEL-001` / `SCI-PSFW-001` → 专项 SCI/ALG
> 冻结：`docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md`（`reports/v6/science-adjudication/adjudications.json` 为机器源）
> W1 复核：`docs/science/v6/phase3/PHASE3_PROPAGATION_REVIEW.md`（SCI-P3-001-REVIEW, C-P3-PROP-1..16）
> 控制器裁决：`工程控制/旧 V6 控制包（ROOT-007 已删除）/CONTROLLER_LOG.md` C-004（不得推翻）
> 机器可读规格与证据：`run/v6/alg-p3/`（gitignore 工作区；人读正文在本目录）

## 0. 范围与纪律

- 本目录**只写算法层**：三模式采样、`C_y = R C_x Rᵀ` 传播、Q/W 输出帧重算、effective PSF 传播、
  流式原子 FITS、采样核 registry 与独立 Oracle 规格。**不写生产源码、不改冻结门/容差/负责人裁决**。
- 本任务 write_scope = `docs/algorithms/v6/phase3/`；证据与命令日志写 `run/v6/alg-p3/`（工作区）。
- 不改写 `docs/science/*.md`、`docs/owner/**`、`docs/design/**`、`docs/references/**` 与生产源码；
  不 commit/push；不派生子代理。F1（工作树≠HEAD）与 AR-033（根构建面 owner）**只登记不裁决**。

## 1. 文件索引

| 文件 | 内容 | 关键条款 |
|---|---|---|
| `ALG-P3-001_SPEC.md` | 三模式采样、`C_y=R C_x Rᵀ`、Q/W 输出帧重算、effective PSF、单位/BUNIT 二次律、流式原子 FITS、12 条 fail-closed | ALG-P3-001..013、FZ-P3-* |
| `ALG-P3-001_KERNEL_REGISTRY.md` | 采样核 registry 规格；`bilinear_4quad` 注册前置独立 Oracle + 误差界 + 边界定义 | ALG-P3-003、FZ-P3-KERNEL-REGISTRY |
| `ALG-P3-001_VERIFICATION.md` | 验证门、独立 Oracle、负向 mutation 矩阵、命令与 rc、证据索引、局限 | ALG-P3-011、FZ-P3-FAILCLOSED |

## 2. 冻结口径继承（原样，不重定义）

- **Phase2 三模式**（`FZ-MODE-PRODUCTION`）：`point_information`（`Q=aPᵀC⁻¹d`、`W=a²PᵀC⁻¹P`、`F̂=Q/W`、`Var=1/W`）、
  `surface_gls`（`AᵀC⁻¹A` 权威；像素 ivar 仅条件近似且有门）、`psfsw_robust`（四分量 S/Conc/N/B、组内 median=1、
  无量纲、**不是 QA-only**，但**不得写成 ivar/Fisher**）。`psf_snr_power` 本包 **DEFERRED** 不进生产（C-004.1）。
- **最终 covariance 一律从实际组合系数传播** `C_out=R C_in Rᵀ`；`psfsw` 禁 `1/W`（`FZ-FORMULA-COV-PROP`、C-004、RULINGS #5）。
- **effective PSF 必输**（`FZ-GATE-PSFSW-EPSF`）；median source SNR / support / coverage / FWHM / residual
  **不得单独冒充任何权重**（`FZ-GATE-MEDIAN-SNR`、`FZ-GATE-SUPPORT-COVERAGE`）。
- **面亮度保持归一** `S_p=Σ_j B_j a_jp/Σ_j a_jp`（`FZ-FORMULA-DRIZZLE-SB`）；通量守恒降为**条件不变量**并写 provenance
  （`FZ-COND-FLUX-CONSERV`、`flux_conservation_factor`）。
- **单位表**以 FREEZE_LIST 为准：`signal_sb=ADU/px²`、`sb_variance_out=ADU²/px⁴`、`W_info=ADU⁻²`、`psfsw=1`；
  Phase3 `var_out` BUNIT = (主 HDU signal BUNIT)²（`FZ-P3-BUNIT-QUADRATIC`）。

## 3. 本任务拥有的 Phase3 冻结条目

`FZ-P3-MODES`、`FZ-P3-QW-RECOMPUTE`、`FZ-P3-FAILCLOSED`、`FZ-P3-KERNEL-REGISTRY`、`FZ-P3-BUNIT-QUADRATIC`。

## 4. 移交（W1 findings → 本任务处置）

F3-01 相关核强制输出 → 实现 IMPL-P3-RSMP-001；F3-02 对角公式适用域 → 本规格 ALG-P3-005；
F3-03 逐像素立体角 → 本规格 ALG-P3-002；F3-04 三模式/epsf/QW → 实现 W5/W7；
F3-05（FROZEN SCI 正文修正）与 F3-06（F1 回退）**只登记不裁决**。

## 5. 独立 Oracle 与负向门（摘要，详见 VERIFICATION）

- 正向：`python3 run/v6/alg-p3/tools/alg_p3_oracle.py run` → **rc=0（10/10）**；
  `python3 run/v6/alg-p3/tools/alg_p3_gate.py positive` → **rc=0（3/3 模式 ACCEPT）**；
  `python3 run/v6/alg-p3/tools/alg_p3_spec_check.py check` → **rc=0**。
- 负向：oracle 12/12、gate 28/28、spec 16/16 mutation 全部检出；`mutate-all` rc=0，逐条单跑 rc=1。
- Oracle **不调用任何生产实现**（不 import/链接/执行 `lib/phase3_*`）；真值来自解析式/固定种子 Monte Carlo，
  投影面积元用自实现球面盈余并与 Paper II 解析式交叉。

## 6. 状态

- 本任务建议状态：**PASS**（见 `ALG-P3-001_VERIFICATION.md` §7）。
- 不宣布发布；未 commit/push；未越界写；未派生子代理。
