# Rejection / Outlier Science (SCI-REJ)

> 上游：ASTROCS_DESIGN.md §5.5（逐像素排异）

> ID: SCI-REJ-001  范围: SCI-REJ-001..008  状态: FROZEN  上游: SCI-SCOPE-001  下游 ALG: ALG-REJ-001..008  模块: phase2 (rejection)

## 1 目的与非目标

- **目的**：对每像素的候选栈（candidate stack，经 UPM 校准后的多帧样本）识别并排除异常候选（卫星线、宇宙线、云、坏帧污染），使加权积分稳健。
- **非目标**：不保证分离真实瞬变（同轨卫星）与剔除目标的语义区分；不处理单帧无重叠区的排异（单帧无排异）；不提供像素外结构重建。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `values[i]` | 第 i 候选的校准后信号 | `P2RejectionInput` |
| `support[i]` | 覆盖度 [0,1] | 同上 |
| `weights[i]` | 科学权重 | 同上 |
| `frame_id[i]` | 样本归属帧 | 同上 |
| `n` | `nominal contributors` 几何可贡献数（一次解析） | `p2_reject_plan_resolve` |
| `n_eff` | 资格后有效候选数 | 资格层 |
| `method` | `None/Sigma/Winsorized/AveragedSigma/LinearFit/GeneralizedESD/RCR` | `P2RejectionMethod` |
| `profile` | `astrocs_adaptive_pixel`（**生产默认，AstroCS 自研**）/ `wbpp_2_9_1`（对照档）/ `wbpp_current`(alias) / `astrocs_adaptive`（可调档） | `plan` |
| `large_scale` | 结构生长开关及参数 | `P2RejectionLargeScaleConfig` |
| `P2_REASON_*` | `ACCEPTED/REJECTED_LOW/REJECTED_HIGH/UNDERDETERMINED` | `rejection.h:76` |
| `P2_STATUS_*` | `OK/MIN_SAMPLES/ALL_REJECTED/INVALID_INPUT/UNDERDETERMINED/...` | `rejection.h:83-86` |

## 3 物理量和单位

- `values, S`: **面亮度 ADU·sr⁻¹**（与 UPM 校准后同一标度；量纲依据=上游 Phase1 HiPS
  signal 层写盘 BUNIT 冻结集 {ADU/sr, ADU^2/sr^2, sr^2/ADU^2}，裸 ADU 判红，
  `lib/infrastructure/aio/src/hiss_writer.cpp:335-365`；实测闭环见 ALG-P2-SMP-001 §2）；
  `support`: 无量纲 [0,1]；`weights`: **(ADU·sr⁻¹)⁻²**；`frame_id`: uint64；
  `sigma, threshold`: **面亮度 ADU·sr⁻¹**；`alpha`: 无量纲 (ESD)；迭代 `max_iterations`:
  无量纲；`radius`: pixel。
- **适用域（正向约束）**：`sigma`/`threshold` 与 `values` 同标度是本层全部阈值判据的
  前提；`percentile` 档的 `scale=|median|`（§8a）同样以 `values` 标度为量纲——
  换标度（ADU ↔ ADU·sr⁻¹）只按比例缩放 `median`，**不改变** §8a 的塌缩条件
  （塌缩判据是 `median → 0` 的相对条件，与标度无关）。

## 4 输入有效域

- `n` 为 planning 层 `nominal contributors`，一次解析，禁止 per-pixel effective 路由（`docs/science/REJECTION.md:16`）。
- 方法合法且 `method != AUTO` 才进 kernel；`n <= underdetermined_n(=2)` 或 `n < minimum_n` ⇒ `UNDERDETERMINED`（`rejection.h:153-154`）。
- **全拒容错域（SC-005 登记）**：`n = 4` ∧ 方法核全拒 ⇒ 降级 `UNDERDETERMINED` 全接受
  （`rejection.cpp:1860-1874`）。该容错的**可达域恰为 n=4**：n≤2 已被白名单截走；
  奇数 n 的百分位带必含中位样本（不可全拒）；n≥5 全拒仍 `ALL_REJECTED`。**可达域三条件**：① 该输出像素**几何 `n = 4`**（`1 ≤ n ≤ 3` 由内核闸 `underdetermined_n = 3` 判 `UNDERDETERMINED`、永不进方法核——**不得**用 `plan.method` 断言「N≤3 ⇒ none」，该档 `plan.method` 仍解析为 `percentile`）；② 路由把 `n = 4` 分派给 `percentile`（本文件 §5 两个 profile 均命中）；③ 百分位带**退化**（近零天光 `median → 0` ∧ `scale = |median|` ⇒ 带宽 → 0，§8a），否则带非空、不可能全拒。**电平依赖**：小 N 段优劣**由电平决定、不由 N 决定**——低/中电平（≲2700 e⁻/pix，含真实数据 NGC1727 1110 ADU、LDN43 2664 ADU）下 `N=3` 强制 percentile **有损**（`ρ−1` = 0.3%–27% ≫ `τ_ρ` = 0.31%）、`N=2` **不可用**（83.5% 像素无输出）；高电平（≳3400 e⁻/pix）对抗轮 R5 实测 `N=3` 占优，翻转边界 ≈3000–3400 e⁻/pix（**超出实验网格上界 1734 e⁻/pix，属外延**）⇒ 生产默认取保守读法 `1 ≤ N ≤ 3 → none`。
- `support/weights` 有限性在资格层校验，非有限 ⇒ `INVALID_INPUT` hard fail。
- `profile` 合法集 = {`astrocs_adaptive_pixel`（**生产默认，AstroCS 自研**）, `wbpp_2_9_1`（**对照档**，路由阈值表采纳自 WBPP 2.9.1 `bestRejectionMethod`）, `wbpp_current`（历史 alias，解析为 `wbpp_2_9_1`）, `astrocs_adaptive`（可调档，与对照档同阈）}；其余值 ⇒ `rc=1` + 显式错误（配置非法）。

## 5 连续定义

```text
逐像素候选栈经 planning 方法过滤，7 种方法 (SCI-REJ):

  None / Sigma / Winsorized / AveragedSigma / LinearFit / GeneralizedESD / RCR

生产默认 auto + profile = astrocs_adaptive_pixel (AstroCS 自研，按逐输出像素几何 n):
  1 ≤ n ≤ 3      → none（不排异 + 直接逆方差加权积分；provenance 记 underdetermined_no_rejection。**偏离代价**：不排异的代价 = 污染**泄漏**——注入实验实测 `N=2` 泄漏 **1/2**、`N=3` 泄漏 **1/3**；收益 = **不误剔真信号**——低/中电平（≲2700 e⁻/pix）下强制 percentile 使 `N=3` 精度损失 `ρ−1` = **0.3%–27%**（≫ `τ_ρ` = 0.31%）、`N=2` **83.5% 像素无输出**）
  4 ≤ n ≤ 5      → percentile (low 0.2 / high 0.1, scale=|median|)
  6 ≤ n ≤ 15     → winsorized_sigma (lower 4.0 / upper 3.0 / 8 iter)
  n ≥ 16         → linear_fit (lower 5.0 / upper 3.5 / 8 iter)
  与 WBPP 档界的差异（一手实测 `BPP-FrameGroup.js:1304-1312`/`:1229-1293`、`BPP-engine.js:2695-2719`）：WBPP 的档界为 6 / 16 两处（即 percentile 上限 5、winsorized 上限 15）；本表把 `1 ≤ N ≤ 3` 改为 none、percentile 收窄到 `4 ≤ N ≤ 5`。差异依据：低电平强制 percentile 有损（`N=3` 的 `ρ−1` = 0.3%–27% ≫ `τ_ρ` = 0.31%）、`N=2` 时 83.5% 像素无输出 ⇒ 小 N 段不排异更接近真值；`N ≥ 4` 起 percentile 偏差进入容差内。
  显式指定算法时 `16≤n<20` linear_fit 由调用方发 WARN。extreme_value_clip_prior_sigma 为显式 opt-in，永不参与 AUTO 路由。**电平依赖**：翻转边界 ≈3000–3400 e⁻/pix——判据**由电平决定、不由 N 决定**；该区间**超出实验网格上界 1734 e⁻/pix，属外延**。
对照档 wbpp_2_9_1 (WBPP 2.9.1 bestRejectionMethod；仅对照/回归基线):
  n < 6          → percentile (low 0.2 / high 0.1, scale=|median|)
  6 ≤ n ≤ 15     → winsorized_sigma (lower 4.0 / upper 3.0 / 8 iter)
  n > 15         → linear_fit (lower 5.0 / upper 3.5 / 8 iter)
  astrocs_adaptive (tunable): 同阈但可配置 large_scale 等

适用域（astrocs_adaptive_pixel）：
  像素候选栈域、逐输出像素几何 n；n ≥ 4 才声明排异能力（n ≤ 3 显式不声明，recall=0）；
  空间生长仅 large_scale 结构（trail），compact cosmic 不生长；
  阈值逐档继承本节冻结锚点，不新增阈值。

阈值冻结锚点 (SCI-REJ / ALG-REJ-001..008, rejection.cpp:1):
  sigma/winsorized/averaged: 4.0/3.0/8
  linear_fit: 5.0/3.5/8
  ESD: alpha 0.05 / max_outliers 10
  percentile: low 0.2 / high 0.1
  minmax: reject_low 1 / reject_high 1 / min_kept 4
  large_scale 默认关闭: enabled=0, min_structure=8, low/high grow radius=2

eligibility 分层:
  invalid_finite / invalid_support / explicit reason → INVALID_* hard fail
  UNDERDETERMINED (n ≤2) → 不做猜测，全接受 (P2_REASON_UNDERDETERMINED)
  normal → 方法核按阈过滤
  全拒容错（域写死 n=4）→ UNDERDETERMINED 全接受 (rejection.cpp:1860-1874)
  n ≥5 全拒 → ALL_REJECTED（不做猜测，不降级）

large_scale 结构生长:
  仅扩展结构生长 (trail)，compact cosmic 不生长 (rejection.cpp:1501-1592 trail 分支)
```

与 `lib/algorithms/coverage/src/rejection.cpp:1-11,1051-1092,1501-1592,1859` 及 `lib/algorithms/coverage/include/astro/phase2/rejection.h:76-154` 一致。

## 6 假设

- 每像素候选独立；噪声近似对称可用稳健中位数尺度；自动选择以 `nominal n` 为唯一路由依据，不以局部有效数重选。

## 7 独立不变量

- **阈值不变量**：同 `n` 的 `method` 选择确定性一致（阈值表驱动，逐档继承 §5 冻结锚点），`auto` 路由不依赖 per-pixel `n_eff`；生产默认档 = `astrocs_adaptive_pixel`（自研）。
- **状态分离不变量**：`P2_REASON` (per-sample) 与 `P2_STATUS` (stack-level) 分离，`INVALID_*` → hard fail 非可继续集合。
- **UNDERDETERMINED 单调性**：`n ≤2` 恒 `UNDERDETERMINED`，不做剔除（recall=0 显式）。
- **全拒容错域（n=4，SC-005）**：`n=4` 且方法核全拒 ⇒ `UNDERDETERMINED` 全接受
  （accepted=n，rej_low=rej_high=0）；`n ≥5` 全拒 ⇒ `ALL_REJECTED`（无 n 相关例外）。
- **流量中性**：单帧无排异（`n=1` 不进核），多帧无离群时不拒真值（NIST ESR 对照）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `n ≤2` 或 `n < minimum_n` | `UNDERDETERMINED` 全接受 | `rejection.h:86` |
| `n=4` 且方法核全拒 | `UNDERDETERMINED` 全接受（容错域写死 n=4） | `rejection.cpp:1860-1874` |
| `n ≥5` 且方法核全拒 | `ALL_REJECTED`（不降级） | 同上 `else` 分支 |
| 非有限 `weights/support` | `INVALID_INPUT` hard fail | `rejection.cpp` 资格层 |
| 全拒 | `ALL_REJECTED` | `rejection.h:84` |
| 无候选 | `NO_CANDIDATES` | 同上 |
| 配置非法 (method/profile) | `INVALID_CONFIGURATION/INVALID_METHOD` | `rejection.h:87-88` |
| 大结构 vs 紧凑 | trail 扩张，compact 不生长 | `rejection.cpp:1501-1592` |

### 8a 根因登记：percentile 的 `scale=|median|` 在近零天光上塌缩（未修复，SC-005）

- 判据带 = `[median − 0.2·|median|, median + 0.1·|median|]`（`rejection.cpp:1599-1611`，
  工作域 = `v − median`，`scale = |median|`；单位 = 面亮度 ADU·sr⁻¹，与 `values` 同标度）。
- **塌缩的可判定条件（正向表述）**：带半宽 = `0.2·|median|`，全宽 = `0.3·|median|`。
  设该像素栈的稳健尺度为 `s`（如 MAD×1.4826），则「带非空且含中位样本」要求
  `0.3·|median| > 0`；当 `|median| → 0`（近零天光）而样本离散度 `s` 不随之为 0 时，
  带在 `s` 的尺度上退化为 0 宽 ⇒ **全部非中位样本被拒**。判据是**相对条件**
  （`|median|/s → 0`），与标度无关；`s` 在低电平档可由 `max(|median|, MAD)` 提供，
  本条只登记条件与影响面，改尺度属重标定事项（§10 禁改）。
- 在近零天光像素上 `median → 0` ⇒ 带宽塌缩为 0 ⇒ **全部非中位样本被拒**。独立 MC
  （20 万次随机高斯栈）实测：显式 `percentile` 在 n=6 / n=8 的全拒率 **73.9% / 70.1%**；
  奇数 n（3/5/7）为 0（中位样本必在带内）。
- 生产 `auto` 路由把 `n ≥6` 交给 `winsorized_sigma`（实测全拒 0/200k），因此该塌缩
  在当前 auto 路径上被掩盖；只有 `n=4` 恰好落在 percentile 分支且可全拒（§4 容错域）。
- **这是真实缺陷，不由 §4 容错掩盖**：是否把尺度改为 `max(|median|, MAD)`（WBPP 对齐面）
  需单独评估与重标定；本文件只登记事实与影响面。
- 证据：`reports/PROJECT-GOVERNANCE-01/research/R-2_phase2权重与UPM语义.md` §3.6 /
  **SC-005 可达域条件见文末 §16（复核补充）**。

## 9 精度策略

- FP64 全链路；ESD/RCR 参照 NIST 独立实现验证；双 sqrt 已修 RJ-004，NONE NaN 已修 RJ-002；归一化 `astrocs_median_center_v1` 默认。

## 10 不可接受变化

- 改变 7 种方法阈值/迭代/ESD alpha 而无 SCI 变更；
- 将 `nominal n` 改为 per-pixel `n_eff` 路由；
- 将 `INVALID_*` 改为可继续集合；
- 使 compact cosmic 被 large_scale 生长误扩。

## 11 验证 Oracle

- **NIST/SCI 交叉**：`GeneralizedESD` 对 NIST 数据集 `120/120` 通过（`rejection_oracle_compare`）。
- **卫星线注入门**：结构注入 recall=1.0，`n≤2` 时 `UNDERDETERMINED` 不宣称可剔。
- **阈值不变量**：同 `n` 的 `plan.resolve` 输出 `method` 确定性一致（`synthetic_gate`）。
- **确定性门**：输入顺序/分块不改 `decision`（`reproducibility` 门）。
- **Python 参考**：SciPy `stats` 对同 candidate 栈的 ESD/RCR 复算 `decision`。

## 12 关联 ALG ID

- `ALG-REJ-001` None 基准
- `ALG-REJ-002..008` Sigma/Winsorized/AveragedSigma/LinearFit/ESD/RCR/Percentile/Minmax + large_scale

## 13 追溯与测试

- 权威文件: `docs/science/REJECTION.md` (SCI-REJ-001..008)
- 实现: `lib/algorithms/coverage/src/rejection.cpp` (1,1051-1092,1501-1592,1859), `lib/algorithms/coverage/include/astro/phase2/rejection.h` (76-154), `lib/algorithms/coverage/src/integrate.cpp` (状态消费)
- 公开 API: `p2_reject_plan_resolve, p2_reject, p2_large_scale_apply`
- 测试: `synthetic_gate` (74/74)、`rejection_oracle_compare` (NIST)、`satellite_gate_build/controlled_rejection_truth` (注入门)

## 3a 坐标 frame

排异在**像素候选栈域**逐像素独立进行；每候选绑定 `frame_id`（DATA_SEMANTICS §5），排异决策不跨像素共享状态；无 WCS 参与（空间邻域仅 `large_scale` 结构半径，pixel 域，默认关闭）。

## 9a 专属问题回答（SCI-006 指定问题逐项）

- **统计假设**：Sigma/Winsorized/AveragedSigma=对称噪声+稳健 median/MAD 尺度；LinearFit=栈内随 n 的线性趋势+残差对称；GeneralizedESD=近似正态多离群检验（`alpha=0.05, max_outliers=10`）；percentile=小 N 无尺度估计时分位拒（`low 0.2/high 0.1, scale=|median|`）；minmax=极值拒（`min_kept=4`）；RCR=reject-clean-refine 迭代（官方 RCR 2.4.7 oracle 锚定）。
- **阈值**：表驱动冻结锚点（`rejection.cpp:1`：sigma/winsorized/averaged 4.0/3.0/8；linear_fit 5.0/3.5/8；ESD alpha 0.05/max 10；percentile 0.2/0.1；minmax 1/1/4；large_scale 默认关闭）——**禁止用"效果好"定义**。
- **自动选择可判定性**：`auto` 以 `nominal n` 唯一路由，不依赖 per-pixel `n_eff` 重选（§7 阈值不变量）；生产默认档 `astrocs_adaptive_pixel`（自研）的内置映射见 §5（n≤3→none；4..7→percentile；8..15→winsorized；≥16→linear_fit）。
- **small-N**：n<6 无稳健尺度意义 → percentile；单帧无排异（§1 非目标）。
- **frame identity**：每候选携带 `frame_id[i]`；排异只置 `accepted[i]` 掩膜不合并样本，identity 全程保持（integration 侧可追溯，SCI-INT §9a）。

## 14 Primary literature（引用定位声明）

1. Generalized ESD：Rosner, B. 1983, Technometrics 25, 165——文章级定位（bibcode 1983Techno..25..165R，未逐页核验），alpha/max_outliers 语义为 Project-defined 采纳。
2. winsorization 概念：Hoaglin, Mosteller & Tukey 1983, *Understanding Robust and Exploratory Data Analysis*——书籍级，未逐页核验。
3. `astrocs_adaptive_pixel`（**生产默认，AstroCS 自研**）：内置映射与低 n 保守档为项目自定（阈值逐档继承 §5 冻结锚点，不新增阈值）；其**对照档** `wbpp_2_9_1` 的 auto 路由与阈值表**采纳自 WBPP 2.9.1 `bestRejectionMethod` 源码**（非学术文献；对齐记录见 docs/development/CONFIG_SCHEMA.md#51）。
4. RCR：官方 RCR 2.4.7 软件参考（项目 oracle 锚定 `official rcr 2.4.7`，见 eng/tools/assemble_v17_review_pkg.py 文献映射），非论文引用。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5 阈值表与 §10 禁改清单。

- **Generalized ESD**：Rosner, B. 1983, Technometrics 25, 165-172（DOI 10.1080/00401706.1983.10487848）；独立可执行实现与临界值表见 NIST/SEMATECH e-Handbook of Statistical Methods §1.3.5.17/§7.1.6。
- **六类排异核（sigma/winsorized/averaged-sigma/linear-fit/percentile/minmax）的祖先**：IRAF imcombine（reject=sigclip|avsigclip|pclip|lfitclip|minmax、winsorize 参数；IRAF/NOAO 许可，非 OSI）；可执行独立对照 ccdproc.combine（BSD-3-Clause，clip_extrema=IRAF-like minmax、sigma_clip_low/high_thresh）与 astropy SigmaClip（BSD-3-Clause）。**来源归属**：生产默认档 `astrocs_adaptive_pixel` 的路由与低 n 保守档为项目自定；方法核的语义注册表 `rejection.cpp:1016-1021` 为 `*_SIRIL`、注释 `:1298/1398` 与测试 `:2903` 均锚 Siril 1.4.3；WBPP 2.9.1 提供档界对照（`BPP-FrameGroup.js`）。WBPP/Siril 仅作对照来源，不作为生产算法归属。
- **预测残差方差阈值/最优检验（若采用）**：Zackay, B., Ofek, E. O. & Gal-Yam, A. 2016, ApJ 830, 27（DOI 10.3847/0004-637X/830/1/27）。
- **RCR（Robust Chauvenet Rejection）**：**论文出处补齐**——Maples, M. P., Reichart, D. E., Konz, N. C., et al. 2018, ApJS 238, 2（DOI 10.3847/1538-4365/aad23d；arXiv:1807.05276）；后续方法学 Konz, N. & Reichart, D. E. 2023, arXiv:2301.07838。现行 §14 第 4 条只登记“官方 RCR 2.4.7 软件参考”，缺该论文引用。
- **winsorization 与稳健尺度**：Hoaglin, Mosteller & Tukey (eds.) 1983, Understanding Robust and Exploratory Data Analysis, Wiley（ISBN 0-471-09777-2）。
- **Tukey biweight/bisquare**：Beaton & Tukey 1974, Technometrics 16, 147（DOI 10.1080/00401706.1974.10489171）。
- **clipped-mean 叠加与 PSF 差异伪影**：Gruen, D., Seitz, S. & Bernstein, G. M. 2014, PASP 126, 158。
- **开源对照**：Siril（GPL-3.0，https://gitlab.com/free-astro/siril）stacking/rejection 文档；PixInsight WBPP bestRejectionMethod 与 ImageIntegration 参考文档（非学术软件来源，AstroCS 自认）。
- **已知缺陷（SC-005，未修复）**：percentile 的 scale=|median| 在近零天光上塌缩（§8a），属真实缺陷，不由 §4 容错掩盖；本任务只登记事实与影响面。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- WCSLIB（LGPL-3.0）/ CFITSIO（宽松许可，NASA/HEASARC）：WCS 与 FITS 独立读取器。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

## 15 Acceptance

- §11 Oracle 全过（以 §11 列门为准：各方法已知 inlier/outlier 注入的 reject set 解析一致）；
- §7 阈值不变量/路由确定性门全过；
- `eng/tools/science_contract_lint.py` PASS；
- 解析不变量→SYN-006 转换：卫星线/宇宙线/坏帧注入、small-N 分位、frame identity 保持、reject set 与 identity 解析可验用例登记 SYN-006。

## 16 复核补充：SC-005 可达域条件与小 N 档位

> 本节为**登记补充**，不改动 §4/§5/§7/§8a 的任何公式、阈值、门与冻结锚点。

- **SC-005 可达域条件**：§4 的「全拒容错」可被触发需**同时**满足
  ① 该输出像素**几何 `n = 4`**（`1 ≤ n ≤ 3` 由内核闸 `underdetermined_n = 3` 判 `UNDERDETERMINED`，永不进方法核；
  这与「`plan.method` 对 `n ≤ 3` 解析为 `percentile`」并不矛盾——**不得**用 `plan.method` 断言「N≤3 ⇒ none」）；
  ② 路由把 `n = 4` 分派给 `percentile`（§5 两个 profile 均命中）；
  ③ 百分位带**退化**（近零天光 `median → 0` ∧ `scale = |median|` ⇒ 带宽 → 0，§8a），否则带非空、不可能全拒。
- **依据**：
  低/中电平（≲2700 e⁻/pix，含真实数据 NGC1727 1110 ADU、LDN43 2664 ADU）下 `N=3` 强制 percentile 精度损失
  `ρ−1` = **0.3%–27%**（≫ `τ_ρ` = 0.31%）、`N=2` **83.5% 像素无输出** ⇒ 生产默认取 `1 ≤ N ≤ 3 → none`
  （保守读法）；高电平（≳3400 e⁻/pix）实测显示 `N=3` 反而占优，
  翻转边界 ≈3000–3400 e⁻/pix（**超出实验网格上界 1734 e⁻/pix，属外延**）。
  **§8a 的尺度塌缩缺陷与上述档位取舍是两件事**，不得互相掩盖。
- **档位表归属**：逐像素冻结映射表见 `docs/plugins/algorithms_phase2/12_rejection.md` §9（`1≤N≤3` none / `4≤N≤5` percentile /
  `6≤N≤15` winsorized / `N≥16` linear fit）；本节 §5 的 `astrocs_adaptive_pixel` 表为**生产 profile 解析面**，两者以 §5 冻结阈值为共同锚。

