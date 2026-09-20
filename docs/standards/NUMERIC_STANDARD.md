# AstroCS Numeric Standard

## 每个科学 double/float 必须文档化

- 单位（ADU / e- / mag / deg / arcsec / 无单位比率）；
- 坐标系（pixel / WCS RA-Dec / HEALPix NESTED / tile+local xy）；
- normalization（除以曝光、中值、median 等）；
- precision requirement（FP32/FP64 边界；默认 science=FP64）；
- valid finite domain。

## MUST

- **NaN/Inf 契约（DOC-202 R34 订正，2026-09-20；依据 `GAP_AUDIT` §5.1 EXP-202 定案「掩膜」）**：
  输入校验返回显式 `INVALID_*` 状态。重采样 / 集成的 NaN 处置**唯一口径** =
  **rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`**（**唯一正本 = `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md`
  §2a 的 `invalid_handling` 块**，文字稿逐字采用
  `run/RELEASE-02/实验/E08-NaN处置/results/evidence_block_draft.md` §2/§4）：
  - **样本级掩膜 + 重归一**：不合格样本（`¬isfinite(x_j) ∨ ¬isfinite(V_j) ∨ V_j ≤ 0`）
    从该输出像素的**分子、分母、方差三项中一并剔除**并**重新归一**；
    **禁止**让单个不合格样本使整个输出像素变为 NaN；**禁止**保留被剔除样本的权重在分母里。
  - **覆盖级 NaN**：仅当**零合格样本**（`D_p = 0`）时输出 `signal = NaN` **且** `support ≤ 0`
    （两者互推）；**NaN 是无效的唯一表示**；**禁止**用 `0`、`±Inf` 或任意哨兵值冒充无效。
  - **强制计数（禁止静默）**：每个输出像素**必须**暴露被剔除样本计数
    **`n_rejected_nonfinite`**（值非有限 / 方差非有限 / 权重非正，分类计数）；
    计数为 0 与「字段缺失」**必须可区分**。
  - **禁止**把 NaN **传播**为合法产品。
    ⚠ 本行原为「禁止 NaN 传播为合法产品」；与之相反的
    `DISP-DRZ-004`「NaN 经 `F_p` 传播、不掩膜」曾于 2026-09-20 被标「已闭环」，
    该「已闭环」声明**已被三面实测推翻**（EXP-202）⇒ `DISP-DRZ-004` 已**改回
    TRACKED/OPEN**，见 `docs/standards/STANDARDS_REGISTRY.md` D.drizzle 偏差表与 §3 索引。
  - **本文件不复制第二套**：三处（本文件、DATA-002 §2a、STANDARDS_REGISTRY D.drizzle）
    必须逐字同口径；如有分歧以 DATA-002 §2a 的 `invalid_handling` 块为准。
- division by zero：显式守卫或状态。
- overflow：checked 尺寸运算；科学累积用 FP64/stable sums。
- epsilon 必须说明物理/数值来源，禁止裸 `1e-6` 无来源。
- FP32/FP64 boundary：fp32 路径与 fp64 等价性测试。

## 权重/逆方差（DOC-202 R33 订正，2026-09-20）

- **权重不是被存的东西**：HiPS 里只**存**「**帧级 SNR**」与「**稀疏的相对 SNR 比值**」
  （最高设计 §2.1）；**权重 = 阶段二在集成时，按某个天球像素对应的那组输入帧现场算出的
  派生量**，**由 SNR 计算**（`w = 1/σ² = SNR²/F_ref²`；最高设计 §4.3/§4.4）。
- **阶段一、阶段三不产生、也不消费任何权重**（最高设计 §2.1）。
- ⛔ **删除原表述**「ivar 优先（>0），否则 `1/uncertainty²` 回退（SCI-NOISE-014/015）」
  ——该「回退」是**权重档位/模式**语义，已按 §9.73 A44 整套作废（不存在「权重模式」）；
  `ivar` 与 `uncertainty` 是**数据对象**（各有正本定义，见 `docs/contracts/DATA_SEMANTICS.md`），
  **不是**两个可回退的权重来源档位。
- 权重必须**正有限**；全 0 / NaN / Inf 权重 → `ZERO_VALID_WEIGHT` / `INVALID_INPUT`。
- **禁止**用 `support` / `coverage` / `validity` / `mask` 冒充权重（四概念分离，最高设计 §4.4）。

## 关联

- docs/science/UNCERTAINTY_AND_COVARIANCE.md；docs/science/DRIZZLE.md；
- docs/standards/CODE_STANDARD.md；docs/standards/STANDARDS_REGISTRY.md。
