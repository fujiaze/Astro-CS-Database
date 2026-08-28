# Rejection / Outlier Science (SCI-REJ)

> ID: SCI-REJ-001  范围: SCI-REJ-001..008 (legacy RJ-001..008)  状态: FROZEN (T107 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-REJ-001..008  模块: phase2 (rejection)

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
| `profile` | `wbpp_2_9_1 / wbpp_current(alias)` | `plan` |
| `large_scale` | 结构生长开关及参数 | `P2RejectionLargeScaleConfig` |
| `P2_REASON_*` | `ACCEPTED/REJECTED_LOW/REJECTED_HIGH/UNDERDETERMINED` | `rejection.h:76` |
| `P2_STATUS_*` | `OK/MIN_SAMPLES/ALL_REJECTED/INVALID_INPUT/UNDERDETERMINED/...` | `rejection.h:83-86` |

## 3 物理量和单位

- `values, S`: ADU；`support`: 无量纲 [0,1]；`weights`: ADU⁻²；`frame_id`: uint64；`sigma, threshold`: ADU；`alpha`: 无量纲 (ESD)；迭代 `max_iterations`: 无量纲；`radius`: pixel。

## 4 输入有效域

- `n` 为 planning 层 `nominal contributors`，一次解析，禁止 per-pixel effective 路由（`docs/science/REJECTION.md:16`）。
- 方法合法且 `method != AUTO` 才进 kernel；`n <= underdetermined_n(=2)` 或 `n < minimum_n` ⇒ `UNDERDETERMINED`（`rejection.h:153-154`）。
- `support/weights` 有限性在资格层校验，非有限 ⇒ `INVALID_INPUT` hard fail。
- `profile` 仅 `wbpp_2_9_1`（及 `wbpp_current` alias）合法，否则 `INVALID_CONFIGURATION`。

## 5 连续定义

```text
逐像素候选栈经 planning 方法过滤，7 种方法 (SCI-REJ):

  None / Sigma / Winsorized / AveragedSigma / LinearFit / GeneralizedESD / RCR

生产默认 auto + profile:
  wbpp_2_9_1 (WBPP 2.9.1 bestRejectionMethod):
    n < 6          → percentile (low 0.2 / high 0.1, scale=|median|)
    6 ≤ n ≤ 15     → winsorized_sigma (lower 4.0 / upper 3.0 / 8 iter)
    n > 15         → linear_fit (lower 5.0 / upper 3.5 / 8 iter)
  astrocs_adaptive (tunable):
    同阈但可配置 large_scale 等

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

large_scale 结构生长:
  仅扩展结构生长 (trail)，compact cosmic 不生长 (rejection.cpp:1501-1592 trail 分支)
```

与 `lib/phase2/src/rejection.cpp:1-11,1051-1092,1501-1592,1859` 及 `lib/phase2/include/astro/phase2/rejection.h:76-154` 一致。

## 6 假设

- 每像素候选独立；噪声近似对称可用稳健中位数尺度；自动选择以 `nominal n` 为唯一路由依据，不以局部有效数重选。

## 7 独立不变量

- **阈值不变量**：同 `n` 的 `method` 选择确定性一致（WBPP 表驱动），`auto` 路由不依赖 per-pixel `n_eff`。
- **状态分离不变量**：`P2_REASON` (per-sample) 与 `P2_STATUS` (stack-level) 分离，`INVALID_*` → hard fail 非可继续集合。
- **UNDERDETERMINED 单调性**：`n ≤2` 恒 `UNDERDETERMINED`，不做剔除（recall=0 显式）。
- **流量中性**：单帧无排异（`n=1` 不进核），多帧无离群时不拒真值（NIST ESR 对照）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `n ≤2` 或 `n < minimum_n` | `UNDERDETERMINED` 全接受 | `rejection.h:85` |
| 非有限 `weights/support` | `INVALID_INPUT` hard fail | `rejection.cpp` 资格层 |
| 全拒 | `ALL_REJECTED` | `rejection.h:83` |
| 无候选 | `NO_CANDIDATES` | 同上 |
| 配置非法 (method/profile) | `INVALID_CONFIGURATION/INVALID_METHOD` | `rejection.h:85-86` |
| 大结构 vs 紧凑 | trail 扩张，compact 不生长 | `rejection.cpp:1501-1592` |

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
- 实现: `lib/phase2/src/rejection.cpp` (1,1051-1092,1501-1592,1859), `lib/phase2/include/astro/phase2/rejection.h` (76-154), `lib/phase2/src/integrate.cpp` (状态消费)
- 公开 API: `p2_reject_plan_resolve, p2_reject, p2_large_scale_apply`
- 测试: `synthetic_gate` (74/74)、`rejection_oracle_compare` (NIST)、`satellite_gate_build/controlled_rejection_truth` (注入门)

## 3a 坐标 frame

排异在**像素候选栈域**逐像素独立进行；每候选绑定 `frame_id`（DATA_SEMANTICS §5），排异决策不跨像素共享状态；无 WCS 参与（空间邻域仅 `large_scale` 结构半径，pixel 域，默认关闭）。

## 9a 专属问题回答（SCI-006 指定问题逐项）

- **统计假设**：Sigma/Winsorized/AveragedSigma=对称噪声+稳健 median/MAD 尺度；LinearFit=栈内随 n 的线性趋势+残差对称；GeneralizedESD=近似正态多离群检验（`alpha=0.05, max_outliers=10`）；percentile=小 N 无尺度估计时分位拒（`low 0.2/high 0.1, scale=|median|`）；minmax=极值拒（`min_kept=4`）；RCR=reject-clean-refine 迭代（官方 RCR 2.4.7 oracle 锚定）。
- **阈值**：表驱动冻结锚点（`rejection.cpp:1`：sigma/winsorized/averaged 4.0/3.0/8；linear_fit 5.0/3.5/8；ESD alpha 0.05/max 10；percentile 0.2/0.1；minmax 1/1/4；large_scale 默认关闭）——**禁止用"效果好"定义**。
- **自动选择可判定性**：`auto` 以 `nominal n` 唯一路由（n<6→percentile；6≤n≤15→winsorized；n>15→linear_fit，wbpp_2_9_1 profile），不依赖 per-pixel `n_eff` 重选（§7 阈值不变量）。
- **small-N**：n<6 无稳健尺度意义 → percentile；单帧无排异（§1 非目标）。
- **frame identity**：每候选携带 `frame_id[i]`；排异只置 `accepted[i]` 掩膜不合并样本，identity 全程保持（integration 侧可追溯，SCI-INT §9a）。

## 14 Primary literature（引用定位声明）

1. Generalized ESD：Rosner, B. 1983, Technometrics 25, 165——文章级定位（bibcode 1983Techno..25..165R，未逐页核验），alpha/max_outliers 语义为 Project-defined 采纳。
2. winsorization 概念：Hoaglin, Mosteller & Tukey 1983, *Understanding Robust and Exploratory Data Analysis*——书籍级，未逐页核验。
3. `wbpp_2_9_1` auto 路由与阈值表：**采纳自 WBPP 2.9.1 `bestRejectionMethod` 源码**（非学术文献；对齐记录见 docs/development/CONFIG_SCHEMA.md#51 与 docs/science/REJECTION.md#40）。
4. RCR：官方 RCR 2.4.7 软件参考（项目 oracle 锚定 `official rcr 2.4.7`，见 tools/assemble_v17_review_pkg.py 文献映射），非论文引用。

## 15 Acceptance

- §11 Oracle 全过（以 §11 列门为准：各方法已知 inlier/outlier 注入的 reject set 解析一致）；
- §7 阈值不变量/路由确定性门全过；
- `tools/science_contract_lint.py` PASS；
- 解析不变量→SYN-006 转换：卫星线/宇宙线/坏帧注入、small-N 分位、frame identity 保持、reject set 与 identity 解析可验用例登记 SYN-006。
