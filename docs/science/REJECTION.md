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
| `profile` | `astrocs_adaptive_pixel`（**生产默认，Astro Celestial Sphere Database（ACSD） 自研**）/ `wbpp_2_9_1`（对照档）/ `wbpp_current`(alias) / `astrocs_adaptive`（可调档） | `plan` |
| `large_scale` | 结构生长开关及参数 | `P2RejectionLargeScaleConfig` |
| `P2_REASON_*` | `ACCEPTED/REJECTED_LOW/REJECTED_HIGH/UNDERDETERMINED` | `rejection.h:94-99` |
| `P2_STATUS_*` | `OK/MIN_SAMPLES/ALL_REJECTED/INVALID_INPUT/UNDERDETERMINED/...` | `rejection.h:102-111` |

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

- `n` 为 planning 层 `nominal contributors`，一次解析；本层路由只由该值决定（`docs/science/REJECTION.md:16`）。
- 方法合法且 `method != AUTO` 才进 kernel；`n <= underdetermined_n` 或 `n < minimum_n` ⇒ `UNDERDETERMINED`
  （`rejection.cpp:2064-2073`）。**`underdetermined_n` 的默认值由 profile 与 request 决定**
  （`rejection.cpp:1218-1225`，实测）：`astrocs_adaptive_pixel` ∧ `request=AUTO` ⇒ **3**（生产默认档）；
  `astrocs_adaptive_pixel` ∧ `request=extreme_value_clip_prior_sigma` ⇒ 1；其余 profile
  （`wbpp_2_9_1`/`wbpp_current`/`astrocs_adaptive`）⇒ 2。调用方显式传 `underdetermined_n>0` 时以显式值为准。
- **全拒容错域**：`n = 4` ∧ 方法核全拒 ⇒ 降级 `UNDERDETERMINED` 全接受
  （`rejection.cpp:1860-1874`）。该容错的**可达域恰为 n=4**：n≤2 已被白名单截走；
  奇数 n 的百分位带必含中位样本（不可全拒）；n≥5 全拒仍 `ALL_REJECTED`。**可达域三条件**：① 该输出像素**几何 `n = 4`**（`1 ≤ n ≤ 3` 由内核闸 `underdetermined_n = 3` 判 `UNDERDETERMINED`、永不进方法核——**`plan.method` 对 `N ≤ 3` 的取值随 profile 而定**（`rejection.cpp:1148-1158/:1254-1268`，实测）：生产档 `astrocs_adaptive_pixel` 解析为 `none`（method=0），`wbpp_2_9_1`/`wbpp_current`/`astrocs_adaptive` 解析为 `percentile`（method=7）；故「N≤3 ⇒ 不排异」在生产档由**路由**保证、在对照档由**内核闸**保证，两档的排异能力判定 = 路由与内核闸两面的合取；`plan.method` 取值随 profile 而定）；② 路由把 `n = 4` 分派给 `percentile`（本文件 §5 两个 profile 均命中）；③ 百分位带**退化**（近零天光 `median → 0` ∧ `scale = |median|` ⇒ 带宽 → 0，§8a），否则带非空、不可能全拒。**电平依赖**：小 N 段优劣**由电平决定、不由 N 决定**——低/中电平（≲2700 e⁻/pix，含真实数据 NGC1727 1110 ADU、LDN43 2664 ADU）下 `N=3` 强制 percentile **有损**（`ρ−1` = 0.3%–27% ≫ `τ_ρ` = 0.31%）、`N=2` **不可用**（83.5% 像素无输出）；高电平（≳3400 e⁻/pix）对抗轮 R5 实测 `N=3` 占优，翻转边界 ≈3000–3400 e⁻/pix（**超出实验网格上界 1734 e⁻/pix，属外延**）⇒ 生产默认取保守读法 `1 ≤ N ≤ 3 → none`。
- `support/weights` 有限性在资格层校验，非有限 ⇒ `INVALID_INPUT` hard fail。
- `profile` 合法集 = {`astrocs_adaptive_pixel`（**生产默认，ACSD 自研**）, `wbpp_2_9_1`（**对照档**，本仓解析表 `rejection.cpp:1265-1267`；档界取自 WBPP **2.5.9** `bestRejectionMethod`，其 `n>15` 档本仓取 `linear_fit` 与该版不符，见 §5）, `wbpp_current`（历史 alias，解析为 `wbpp_2_9_1`）, `astrocs_adaptive`（可调档，与对照档同阈）}；其余值 ⇒ `rc=1` + 显式错误（配置非法）。

## 5 连续定义

```text
逐像素候选栈经 planning 方法过滤，7 种方法 (SCI-REJ):

  None / Sigma / Winsorized / AveragedSigma / LinearFit / GeneralizedESD / RCR

生产默认 auto + profile = astrocs_adaptive_pixel (ACSD 自研，按逐输出像素几何 n):
  1 ≤ n ≤ 3      → none（不排异 + 直接逆方差加权积分；provenance 记 underdetermined_no_rejection。**偏离代价**：不排异的代价 = 污染**泄漏**——注入实验实测 `N=2` 泄漏 **1/2**、`N=3` 泄漏 **1/3**；收益 = **不误剔真信号**——低/中电平（≲2700 e⁻/pix）下强制 percentile 使 `N=3` 精度损失 `ρ−1` = **0.3%–27%**（≫ `τ_ρ` = 0.31%）、`N=2` **83.5% 像素无输出**）
  4 ≤ n ≤ 5      → percentile (low 0.2 / high 0.1, scale=|median|)
  6 ≤ n ≤ 15     → winsorized_sigma (lower 4.0 / upper 3.0 / 8 iter)
  n ≥ 16         → linear_fit (lower 5.0 / upper 3.5 / 8 iter)
  档界来源（**可核验形式**：版本 + 包 sha1 + 真实 file:line）：
  WBPP **2.5.9**（官方更新包 https://pixinsight.com/update/1.8.9-1/20230203-script.zip ，
  sha1 `712cc7c3fdb523643ad0e685104592d511996f82`，`product-info.txt` 自述版本 2.5.9）
  `WeightedBatchPreprocessing-engine.js:1421-1429` `bestRejectionMethod()`：
  `n = activeFrames().length`；`n < 6 → PercentileClip`；`n ≤ 15 或 BIAS/DARK → WinsorizedSigmaClip`；
  `否则 → Rejection_ESD`；合法性窗口 `rejectionIsGood()` = `:1349-1412`。
  **本表采纳 WBPP 的档界（6 / 15 两处），但不采纳其 `n > 15` 档的算法类型**：
  本表 `N ≥ 16 → linear_fit` 对应的是 **WBPP ≤ 2.3.x 的旧表**
  （1.4.6 `:190-203`：`n<8` percentile / `≤10` averaged / `<20` winsorized /
  `<25 或 ESD 未定义` LinearFit / 否则 ESD；2.4.0 起改为 `n>15 → ESD`，2.4.2 `:984-992` 实测）——
  即**档界取自 2.4.0+、该档算法取自 ≤2.3.x**，两者不是同一个版本的表。
  **逐像素按几何 `N` 路由是本项目自定扩展**：WBPP 按**帧组活动帧数**路由，没有逐像素行为；
  WBPP 只提供**档界与算法类型**的参考，本项目按逐像素 `n` 自定扩展。
  本表把 `1 ≤ N ≤ 3` 改为 none、percentile 收窄到 `4 ≤ N ≤ 5`。差异依据：低电平强制 percentile 有损（`N=3` 的 `ρ−1` = 0.3%–27% ≫ `τ_ρ` = 0.31%）、`N=2` 时 83.5% 像素无输出 ⇒ 小 N 段不排异更接近真值；`N ≥ 4` 起 percentile 偏差进入容差内。
  显式指定算法时 `16≤n<20` linear_fit 由调用方发 WARN。extreme_value_clip_prior_sigma 为显式 opt-in，永不参与 AUTO 路由。**电平依赖**：翻转边界 ≈3000–3400 e⁻/pix——判据**由电平决定、不由 N 决定**；该区间**超出实验网格上界 1734 e⁻/pix，属外延**。
对照档 wbpp_2_9_1 (仅对照/回归基线；解析表实测 rejection.cpp:1265-1267):
  n < 6          → percentile (low 0.2 / high 0.1, scale=|median|)
  6 ≤ n ≤ 15     → winsorized_sigma (lower 4.0 / upper 3.0 / 8 iter)
  n > 15         → linear_fit (lower 5.0 / upper 3.5 / 8 iter)
  注（**归属订正**）：WBPP 2.5.9 实测的 n > 15 分支是 Rejection_ESD
  （engine.js:1421-1429，sha1 712cc7c3…），本对照档该档取 linear_fit ⇒ **该档与 WBPP 2.4.0+ 不符**，
  对应的是 WBPP ≤2.3.x 旧表。本行只描述**本仓解析表**，不是对 WBPP 行为的转述。
  WBPP ≥2.6 源码随商业安装分发、公开不可核验，故以 **2.5.9** 为可核验基准。
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

与 `lib/algorithms/coverage/src/rejection.cpp:1-12,1182-1288,1809-1830,1996-2201` 及 `lib/algorithms/coverage/include/astro/phase2/rejection.h:45-62,94-118,203-239` 一致（行号实测，逐符号锚见 `docs/algorithms/PHASE2_REJECTION.md` §3）。

## 6 假设

- 每像素候选独立；噪声近似对称可用稳健中位数尺度；自动选择以 `nominal n` 为唯一路由依据，不以局部有效数重选。

## 7 独立不变量

- **阈值不变量**：同 `n` 的 `method` 选择确定性一致（阈值表驱动，逐档继承 §5 冻结锚点），`auto` 路由不依赖 per-pixel `n_eff`；生产默认档 = `astrocs_adaptive_pixel`（自研）。
- **状态分离不变量**：`P2_REASON` (per-sample) 与 `P2_STATUS` (stack-level) 分离，`INVALID_*` → hard fail 非可继续集合。
- **UNDERDETERMINED 单调性**：`n ≤2` 恒 `UNDERDETERMINED`，不做剔除（recall=0 显式）。
- **全拒容错域（n=4）**：`n=4` 且方法核全拒 ⇒ `UNDERDETERMINED` 全接受
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
| 无候选（`count==0`） | `MIN_SAMPLES`（值 1；本层八态无 `NO_CANDIDATES`——该名属积分域 `P2IntegrateStatus`） | `rejection.cpp:2018` |
| 配置非法 (method/profile) | `INVALID_CONFIGURATION/INVALID_METHOD` | `rejection.h:108-109`；`rejection.cpp:2036-2062/:2000-2013` |
| 大结构 vs 紧凑 | trail 扩张，compact 不生长 | `rejection.cpp:2342-2433` |

### 8a percentile 判据的适用域：阈值是天光电平的固定分数，不是噪声尺度

- **判据（本层定义）**：`v ∉ [median − plow·|median|, median + phigh·|median|]` ⇒ 判拒
  （`rejection.cpp:1809-1830`；`plow=|low_fraction|=0.2`、`phigh=|high_fraction|=0.1`）。
  工作域 = `v − median`，`normalization=MEDIAN_CENTER` 由内核强制
  （`rejection.cpp:2038-2045`）。**单位**：`median`、`plow·|median|`、`phigh·|median|` 与
  `values` 同标度 = 面亮度 ADU·sr⁻¹。判据式的**次生参考实现对拍**（不是语义来源）：
  Siril 1.4.3 `src/stacking/rejection_float.c:31-44`（`percentile_clipping`：`median - pixel > median*plow`
  ⇒ 低拒；`pixel - median > median*phigh` ⇒ 高拒），本层与其逐式等价（该参考只用于掩码逐元素对拍）
  （`v < median(1−plow)` ⇔ `v − median < −plow·|median|`，`median>0`）。
- **同名不同义（避免跨实现误比，必读）**：`percentile` 在参考实现里有两个**互不相同**的含义，本层只取其一：
  - **① 本层含义（乘性带；与 Siril 1.4.3 `percentile_clipping` 逐式等价）**：判据带随**天光电平**缩放，
    `v ∉ [median − plow·|median|, median + phigh·|median|] ⇒ 判拒`（本节首条）。
  - **② PixInsight/WBPP 与 IRAF `pclip` 含义（按秩裁固定比例）**：把候选栈**按值排序**后，低端裁掉
    `low_fraction` 比例、高端裁掉 `high_fraction` 比例的**样本序号**（IRAF `pclip=-0.5` 即取中位数与最低值的中点作下界），
    与样本的**绝对偏离**无关、与天光电平无关。
  - **本仓取 ①**，依据：① 与生产档的 `normalization=MEDIAN_CENTER` 工作域自洽（判据与归一化同域，见本节首条）；
    ② 逐式等价有可执行 oracle（次生参考实现掩码逐元素对拍，§11）；③ ② 的秩比例语义随 `n` 改变实际裁掉的比例、
    不能表达为固定阈值，与本层「阈值表驱动、同 `n` 确定性一致」的**阈值不变量**（§7）不相容。
  - **代价（如实登记）**：① 的等效显著性随 `|median|/s` 漂移（本节下文三段适用域），即它**不是**噪声尺度判据；
    改用 ② 属判据类型变更，走 SCI 变更流程（§10）。
- **正向约束（判据的等效显著性）**：设该像素栈的稳健尺度为 `s`（如 `1.482602218505602·MAD`），则判据在
  σ 单位下的等效阈值为 `z_low = plow·|median|/s`（低侧）、`z_high = phigh·|median|/s`（高侧）。
  **阈值随天光电平与噪声之比线性漂移**，`|median|/s` 是无量纲比 ⇒ 换标度
  （ADU ↔ ADU·sr⁻¹）不改变任何结论。
- **三段适用域（`rejection.cpp` 实测：每格点 2×10⁴ 个**干净**高斯栈，**不注入任何离群**；
  `s = 1` 固定、`median = 比值·s`，故比值扫描与绝对标度无关；n = 4/6/8）**：
  - `|median|/s ≳ 33`（`z_high ≳ 3.3`）：判据**惰性**——干净栈零拒率 ≥ 99.4%（比值 100/1000 时 100%）。
    真实天光帧即处此域：M42 真帧中央 512² 天光区实测 `|median|/s = 48.8–51.4` ⇒
    `z_low = 9.8–10.3`、`z_high = 4.9–5.1`（即只剔除偏离天光 20%（低）/ 10%（高）以上的样本）。
  - `|median|/s ≈ 3.3–10`：**对干净数据过拒**——至少剔掉一个干净样本的栈占比
    n=6：**96.65%**（比值 3.33）→ 84.5%（2.0）→ 67.5%（10）；n=8：**99.06%** → 77.95%。
    **该区间不是「有判别力」，而是判据的等效阈值落到了噪声宽度之内**——
    干净样本本身就越过 `z_high = 0.1·|median|/s`（比值 3.33 时 `z_high ≈ 0.33`）。
  - `|median|/s ≲ 2`：**全拒**；`|median|/s → 0`（近零天光）时带在 `s` 尺度上退化为 0 宽
    ⇒ 全拒率 n=6：74.4%（比值 0）→ 15.4%（2.0）；n=8：70.0% → 7.3%；
    n=4 的 full-reject 恒为 0，因为内核先全拒、再由 `n ≤ 4` 容错降级（比值 0 时 **79.13%** 走该分支）。
  **结论（正向约束）**：`percentile` 在**任何**天光电平上都不是一个噪声尺度判据——
  比值 ≳ 33 惰性、≲ 10 过拒、≲ 2 全拒/降级，**不存在**等效显著性落在合理区间（如 3–5σ）的稳定工作点。
- **适用域边界（不能的条件）**：`percentile` **不能**在缺少「天光电平远大于噪声」这一前提时被当作
  排异判据使用：真实天光电平（`|median|/s ≈ 50`）下退化为**惰性**，`|median|/s ≲ 10` 时退化为**过拒**，
  `≲ 2` 时退化为**全拒**。**真实数据上的主导形态是惰性而非塌缩**：M42 真帧 64² patch
  实测 `|median|/s` 中位 73.6–77.9、`≥33` 占 **89.3%–89.7%**、`≤3.3` 仅 **0.10%–0.17%**。
- **`n = 4` 档（生产档把 `4 ≤ N ≤ 5` 路由给 `percentile`，`rejection.cpp:1148-1158`）的三种结局
  都不产生噪声尺度上的排异**：`|median|/s ≈ 50` ⇒ 惰性（零拒率 99.68%）；
  `|median|/s ≈ 3.3–10` ⇒ 误剔干净样本（比值 3.33 时 **85.3%** 的干净栈至少剔一个样本）；
  `|median|/s ≲ 2` ⇒ 内核先全拒、再由 §4 容错降级为 `UNDERDETERMINED` 全接受
  （`n ≤ 4`，`rejection.cpp:2185-2195`；`|median|/s = 0` 实测 **79.13%** 走该分支、
  `= 2` 时 29.39%）。该档的排异能力以 §8a 的等效阈值为准，
  **该档的表述面 = 全拒 + 容错降级链**，能力锚在 §8a 的等效阈值，与噪声尺度排异无关。
- **判据带退化（`|median| = 0`）时的本层行为**：带退化为单点 `{median}`，除恰等于中位的样本外
  全部判拒；`n ≤ 4` 由 §4 容错域降级为全接受，`n ≥ 5` 为 `ALL_REJECTED`
  （`rejection.cpp:1818-1830/:2185-2195`）。参考实现 Siril 1.4.3 在同条件下**提前返回不排异**
  （`src/stacking/rejection_float.c:163-171`：`if (median == 0.0) return 0;`）——本层**不采用**
  该分支，理由是该分支会把 `n ≥ 5` 的 `ALL_REJECTED` 改写为 `OK`，属 SCI 语义变更，
  须走变更流程（§10）。**在该变更落地前，`|median| → 0` 的像素栈一律按上句处置。**
- **改尺度（`max(|median|, MAD)`，WBPP 对齐面）属重标定事项**（§10 禁改）：本条只登记条件、
  影响面与等效阈值，不改 `low_fraction/high_fraction` 冻结值、不改判据带定义。

## 9 精度策略

- FP64 全链路；ESD/RCR 参照 NIST 独立实现验证；归一化默认 `astrocs_median_center_v1`（`normalization=MEDIAN_CENTER`，`rejection.cpp:1226-1228`）。
- **正向约束**：ESD 的样本标准差用**单次** `sqrt`（`rejection.cpp:1730-1733`）；`NONE` 方法对非有限候选的回读值 = `INVALID_INPUT`（`rejection.cpp:2025-2034`）。

## 10 不可接受变化

- 改变 7 种方法阈值/迭代/ESD alpha 而无 SCI 变更；
- 将 `nominal n` 改为 per-pixel `n_eff` 路由；
- 将 `INVALID_*` 改为可继续集合；
- 使 compact cosmic 被 large_scale 生长误扩。

## 11 验证 Oracle

- **NIST 交叉**：`GeneralizedESD` 对 NIST Rosner 54 点集判出**恰 3 个**离群，且回看准则选出的 `k_out` 与独立两阶段 NIST 实现（SciPy t 分布、独立数值栈）一致（`lib/algorithms/coverage/tools/rejection_oracle_compare.py:193-215` 断言 `n_rej == 3 and k_ref == 3`）。
- **卫星线注入门**：结构注入 recall=1.0；生产档 `n ≤ 3` 路由 `none`、对照档 `n ≤ 2` 由内核闸判 `UNDERDETERMINED` ⇒ 该域不宣称可剔。
- **阈值不变量**：同 `n` 的 `plan.resolve` 输出 `method` 确定性一致（`synthetic_gate`）。
- **确定性门**：输入顺序/分块不改 `decision`（`reproducibility` 门）。
- **Python 参考**：SciPy `stats` 对同 candidate 栈的 ESD/RCR 复算 `decision`。

## 12 关联 ALG ID

- `ALG-REJ-001` None 基准
- `ALG-REJ-002..008` Sigma/Winsorized/AveragedSigma/LinearFit/ESD/RCR/Percentile/Minmax + large_scale
- **方法枚举共 11 项 + AUTO**：`NONE=0 / SIGMA=1 / WINSORIZED_SIGMA=2 / AVERAGED_SIGMA=3 / LINEAR_FIT=4 /
  GENERALIZED_ESD=5 / RCR=6 / PERCENTILE=7 / MEDIAN_SIGMA=8 / MINMAX=9 / AUTO=10 /
  EXTREME_VALUE_PRIOR_SIGMA=11`（`rejection.h:45-62`）；`AUTO` 只在规划层解析、永不进方法核，
  `EXTREME_VALUE_PRIOR_SIGMA` 为显式 opt-in、不参与任何 AUTO 路由。

## 13 追溯与测试

- 权威文件: 本文件（SCI-REJ-001..008 集合的唯一权威页）
- 实现: `lib/algorithms/coverage/src/rejection.cpp` (1-12 冻结头、1182-1288 规划、1996-2201 生产 kernel、2342-2433 large_scale), `lib/algorithms/coverage/include/astro/phase2/rejection.h` (45-62 方法枚举、94-118 reason/status/normalization、203-239 plan/request), `lib/algorithms/coverage/src/integrate.cpp` (状态消费)
- 公开 API: `p2_reject_plan_resolve, p2_reject, p2_large_scale_apply`
- 测试: `synthetic_gate`（排异面合成门，逐 TEST 用例；通过数以该文件与 ctest 实测为准，**不引用固定计数**）、
- `rejection_oracle_compare`（NIST ESD + Siril 1.4.3 harness 逐位对照）、
- `satellite_gate_build` / `controlled_rejection_truth`（注入门与受控真值）。

## 3a 坐标 frame

排异在**像素候选栈域**逐像素独立进行；每候选绑定 `frame_id`（DATA_SEMANTICS §5），排异决策不跨像素共享状态；无 WCS 参与（空间邻域仅 `large_scale` 结构半径，pixel 域，默认关闭）。

## 9a 专属问题回答（SCI-006 指定问题逐项）

- **统计假设**：Sigma/Winsorized/AveragedSigma=对称噪声 + 稳健 `median`/`MAD` 尺度；
  LinearFit=栈内随 n 的线性趋势 + 残差对称（残差尺度 = **平均绝对残差**，`rejection.cpp:1665-1668`）；
  GeneralizedESD=近似正态多离群检验（`alpha=0.05, max_outliers=10`，`rejection.cpp:1236`）；
  percentile=**天光电平的固定分数带**（`low 0.2 / high 0.1`，`scale=|median|`，`rejection.cpp:1237/:1817-1818`），
  **不是**小 N 下的无尺度分位判据——其等效显著性随 `|median|/s` 漂移（§8a）；
  minmax=固定秩极值拒（`min_kept=4`，`rejection.cpp:1240-1241`）；
  RCR=固定 3-pass 链（Median+DoubleLine → Median+68th → Mean+StdDev，`rejection.cpp:1785-1806`）
  的序贯 Chauvenet 迭代，经验修正因子表锚 Siril 之外的官方 RCR 实现；
  EXTREME_VALUE_PRIOR_SIGMA=已知先验 σ 的极值检验（显式 opt-in，`rejection.cpp:1915-1965`）。
- **阈值**：表驱动冻结锚点（冻结头注释 `rejection.cpp:1-12`；规划层 typed 默认值 `rejection.cpp:1226-1251`：sigma/winsorized/averaged/median_sigma 4.0/3.0/8；linear_fit 5.0/3.5/8；ESD alpha 0.05/max 10；percentile 0.2/0.1；minmax 1/1/4；large_scale 默认关闭）——**阈值定义只取自该表**。
- **自动选择可判定性**：`auto` 以 `nominal n` 唯一路由，不依赖 per-pixel `n_eff` 重选（§7 阈值不变量）；生产默认档 `astrocs_adaptive_pixel`（自研）的内置映射见 §5（`1≤n≤3`→none；`4≤n≤5`→percentile；`6≤n≤15`→winsorized_sigma；`n≥16`→linear_fit；`rejection.cpp:1148-1158` 实测）。
- **small-N**：`n ≤ 3` 不声明排异能力（生产档路由 `none`、对照档由内核闸判 `UNDERDETERMINED`）；`4 ≤ n ≤ 5` → percentile（其等效阈值见 §8a）；单帧无排异（§1 非目标）。
- **frame identity**：每候选携带 `frame_id[i]`；排异只置 `accepted[i]` 掩膜不合并样本，identity 全程保持（integration 侧可追溯，SCI-INT §9a）。

## 14 Primary literature（引用定位声明）

1. **Generalized ESD**：Rosner, B. 1983, *Percentage Points for a Generalized ESD Many-Outlier Procedure*, Technometrics **25**, 165-172（DOI [10.1080/00401706.1983.10487848](https://doi.org/10.1080/00401706.1983.10487848)）。
   **① 引用存在且逐字匹配**：Crossref 元数据核验题名/作者/卷/页一致。
   **② 该引用支持本层主张**：论文给出「多离群、按回退准则定 k_out」的广义 ESD 过程与临界值表，正是本层 F8 的判据来源。
   `alpha=0.05`/`max_outliers=10` 为 Project-defined 采纳值，**文献不提供**这两个门值。
2. **winsorization 概念**：Hoaglin, Mosteller & Tukey (eds.) 1983, *Understanding Robust and Exploratory Data Analysis*, Wiley（ISBN 0-471-09777-2）——书籍级定位；本层 `winsorized_sigma` 的**语义来源 = PixInsight ImageIntegration 官方文档式[18]/[19]**（±1.5σ winsorize、常数 1.134、迭代限 5e-4；概念出处 = Huber & Ronchetti 2009, *Robust Statistics* 2nd ed.，即官方文档式[4] 所指）；Siril 1.4.3 为**次生参考实现**（只用于掩码逐元素对拍，见 §14a），本书只作概念背景。
3. **`astrocs_adaptive_pixel`（生产默认，ACSD 自研）**：内置映射与低 n 保守档为项目自定（阈值逐档继承 §5 冻结锚点，不新增阈值）；**档界**采纳自 WBPP（可核验版本 = **2.5.9**，见 §5 与 §14a），**逐像素按几何 n 的粒度为本项目自定扩展**；`n ≥ 16` 档取 `linear_fit` 对应 WBPP ≤2.3.x 的旧表，与 WBPP 2.4.0+（该档为 ESD）**不符**。**对照档** `wbpp_2_9_1` 的 auto 路由与阈值表为本仓解析表（`rejection.cpp:1265-1267`），其 `n > 15` 档同样取 `linear_fit`（非学术软件来源；`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`）。
4. **RCR**：方法论文 = Maples, M. P., Reichart, D. E., Konz, N. C., et al. 2018, ApJS **238**, 2（DOI [10.3847/1538-4365/aad23d](https://doi.org/10.3847/1538-4365/aad23d)；[arXiv:1807.05276](https://arxiv.org/abs/1807.05276)）；
   **① 引用存在且逐字匹配**：Crossref 与 arXiv 元数据核验题名/作者/卷/页一致。
   **② 该引用支持本层主张**：论文提出序贯更换集中趋势测度的 RCR 过程（本层 3-pass 链）与**经验确定的**拒绝 σ 修正因子（本层查找表来源）。
   实现对照 = 官方 RCR 2.4.7（软件，非论文）。

## 14a 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动 §5 阈值表与 §10 禁改清单。

- **Generalized ESD**：Rosner, B. 1983, Technometrics 25, 165-172（DOI 10.1080/00401706.1983.10487848）；独立可执行实现与临界值表见 NIST/SEMATECH e-Handbook of Statistical Methods §1.3.5.17/§7.1.6。
- **方法族命名来源（参数名已订正）**：IRAF `combine`/`imcombine` 共用的 `ccdred/src/combine/` 引擎，其参数文件
  `noao/imred/ccdred/combine.par`（iraf-community/iraf@main，IRAF/NOAO 许可，非 OSI）的
  `reject` 值域为 `none|minmax|ccdclip|crreject|sigclip|avsigclip|pclip`，另有
  `combine=average|median`、`nlow=1`/`nhigh=1`、`nkeep=1`、`mclip=yes`、`lsigma=3.`/`hsigma=3.`、
  `pclip=-0.5`、`sigscale=0.1`、`rdnoise=0.`/`gain=1.`/`snoise=0.`、`grow=0`、`lthreshold`/`hthreshold=INDEF`。
  **该引擎没有 `lfitclip` 也没有 `winsorize` 参数**（本仓旧引文中的这两个名字在 IRAF 里不存在，已删去）；
  与 `linear_fit` 对应的 IRAF 方法族是 `ccdclip`（CCD 噪声模型裁切），与 `winsorized_sigma` **无对应项**
  （`sigclip`/`avsigclip` 是 σ 裁切，不是 winsorize）。**命名只作方法族命名来源，核语义一律以本层为准**
  （尤其 `pclip`：IRAF 的 `pclip` 是**按秩裁固定比例样本**，与本层 `percentile` 的**乘性判据带同名不同义**，
  两种含义并列见 §8a）。可执行独立对照 = ccdproc.combine（BSD-3-Clause）与 astropy `SigmaClip`（BSD-3-Clause）。
- **来源归属（订正后，取代旧表述）**：生产默认档 `astrocs_adaptive_pixel` 的**档界**采纳自 WBPP
  （可核验版本/file:line 见 §5、§14a），**逐像素按几何 n 的粒度**与低 n 保守档为项目自定；
  **方法核的语义来源**：`linear_fit` = PixInsight ImageIntegration 官方式[21]/式[22]（横轴 = 排序秩 `0…N−1`、
  带升序约束）+ *Numerical Recipes* 3rd ed. **§15.7.3**（`Fitmed`，L1 稳健拟合；本项目以加权最小二乘实现，
  偏离已在 `docs/algorithms/PHASE2_REJECTION.md` F7 登记）；`winsorized_sigma` = 官方式[18]/[19]（Huber 体系）；
  `percentile` = 本层定义（§5/§8a）+ IRAF `pclip` 命名。语义注册表 `rejection.cpp:1082-1098` 的 `*_SIRIL` id
  与核注释 `rejection.cpp:1514/1614` 是**次生参考实现对拍用的冻结标识**（oracle 用未修改的 Siril 1.4.3
  官方源码做**掩码逐元素对拍**），**不是核语义的归属**；语义 ID 文本改名属 SCI 变更（§10），本层本轮不改。
- **预测残差方差阈值/最优检验（若采用）**：Zackay, B., Ofek, E. O. & Gal-Yam, A. 2016, ApJ 830, 27（DOI 10.3847/0004-637X/830/1/27）。
- **RCR（Robust Chauvenet Rejection）**：方法论文 = Maples, M. P., Reichart, D. E., Konz, N. C., et al. 2018, ApJS 238, 2（DOI 10.3847/1538-4365/aad23d；arXiv:1807.05276）；后续方法学 = Konz, N. & Reichart, D. E. 2023, arXiv:2301.07838。实现对照 = 官方 RCR 2.4.7（软件，非论文）。
- **winsorization 与稳健尺度**：Hoaglin, Mosteller & Tukey (eds.) 1983, Understanding Robust and Exploratory Data Analysis, Wiley（ISBN 0-471-09777-2）。
- **Tukey biweight/bisquare**：Beaton, A. E. & Tukey, J. W. 1974, *The Fitting of Power Series, Meaning Polynomials, Illustrated on Band-Spectroscopic Data*, Technometrics **16**, 147-185（DOI [10.1080/00401706.1974.10489171](https://doi.org/10.1080/00401706.1974.10489171)，Crossref 元数据核验一致）。
- **该引用不支持本层任何方法的核语义**：本层 11 个方法均不使用 biweight/bisquare 核，此处只作稳健估计背景登记。
- **clipped-mean 叠加与 PSF 差异伪影**：Gruen, D., Seitz, S. & Bernstein, G. M. 2014, PASP 126, 158。
- **次生参考实现（只用于掩码逐元素对拍，不是核语义来源）**：Siril 1.4.3（GPL-3.0，https://gitlab.com/free-astro/siril）
  `src/stacking/rejection_float.c:31-44`（`percentile_clipping` 判据带）、`:163-171`（`median == 0.0` 提前返回，本层不采用）；
  本层 `percentile` 判据与 `:31-44` 逐式等价（§8a）。**行为/数值对照，不复制 GPL 代码**。
- **档界对照（可核验形式）**：PixInsight WBPP **2.5.9** `WeightedBatchPreprocessing-engine.js:1421-1429`
  `bestRejectionMethod()`（官方更新包 sha1 `712cc7c3fdb523643ad0e685104592d511996f82`；非学术软件来源；`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`）。
  **仓内旧引文 `BPP-FrameGroup.js:1304-1312` 已作废**：该文件名不存在于任何官方 WBPP 包，
  且其正文所称的 `n > 15 → LinearFit` 在 1.4.2–2.5.9 的任何版本都不成立（2.4.0+ 为 ESD，≤2.3.x 为 `n<25 → LinearFit`）。
- **percentile 判据的适用域**：等效显著性随 `|median|/s` 线性漂移（§8a），真实天光电平下退化为惰性、近零天光下退化为过拒；判据带定义与冻结阈值以 §5/§8a 为准。

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

## 16 全拒容错域的可达条件与小 N 档位取舍

> 本节与 §4/§5/§7/§8a 使用同一套公式、阈值、门与冻结锚点，不引入新判据。

- **「全拒容错」的可达条件**：§4 的降级分支被触发需**同时**满足
  ① 该输出像素**几何 `n = 4`**（`1 ≤ n ≤ 3` 由内核闸 `underdetermined_n` 判 `UNDERDETERMINED`，永不进方法核；
  `plan.method` 对 `n ≤ 3` 的取值随 profile 而定——生产档 `none`、对照档 `percentile`（§4），**两档取值各自具名**）；
  ② 路由把 `n = 4` 分派给 `percentile`（§5 两个 profile 均命中）；
  ③ 判据带退化到全拒：`w[i] < −0.2·|median| ∨ w[i] > 0.1·|median|` 对所有候选成立（§5 F10），
  即要求 `|median|/s ≲ 3.3`（§8a 实测：`|median|/s = 0` 时 n=4 有 79.1% 像素落入本分支）。
  **反向域**：`|median|/s ≳ 33` 时同一判据对噪声离群**从不拒绝**（§8a 实测 zero-reject ≥99.4%）——
  两个域都不产生「按噪声尺度拒绝」的能力，差异只在是否触发降级。
- **小 N 档位取舍的依据**：
  低/中电平（≲2700 e⁻/pix，含真实数据 NGC1727 1110 ADU、LDN43 2664 ADU）下 `N=3` 强制 percentile 精度损失
  `ρ−1` = **0.3%–27%**（≫ `τ_ρ` = 0.31%）、`N=2` **83.5% 像素无输出** ⇒ 生产档取 `1 ≤ N ≤ 3 → none`
  （保守读法）；高电平（≳3400 e⁻/pix）实测显示 `N=3` 反而占优，
  翻转边界 ≈3000–3400 e⁻/pix（**超出实验网格上界 1734 e⁻/pix，属外延**）。
  **§8a 的 percentile 适用域（阈值随 `|median|/s` 漂移）与上述档位取舍是两件事**，各自独立表述。
- **档位表归属**：逐像素冻结映射表见 `docs/plugins/algorithms_phase2/12_rejection.md` §9（`1≤N≤3` none / `4≤N≤5` percentile /
  `6≤N≤15` winsorized / `N≥16` linear fit）；本节 §5 的 `astrocs_adaptive_pixel` 表为**生产 profile 解析面**，两者以 §5 冻结阈值为共同锚。

## 17 真实数据读数（M42 沿线；口径显式）

> 本节只登记**实测算得的读数与分母口径**，不改本节上游任何公式、阈值、路由与冻结锚点。

- **两种 z 的区分（读数前必读）**：
  - `z_trail` = 卫星帧的**留一**稳健 z：排除该帧后，用其余候选帧的 `median` 与 `1.4826·MAD` 计算该帧的偏离；
  - **显著点** = `z_trail > 5` 的沿线取样点（即"卫星线确实在该像素留下可判异常"的点）。
- **沿线取样点读数**（M42 生产产品 `run/M42-E2E-02/out/full_p2_v2`；4 条真实卫星线、每 2 帧取样共 **5785** 点。
  逐点判定由**生产 kernel**（`build/lib/algorithms/coverage/rejection_cli` → `p2_reject_plan_resolve` +
  `p2_reject_stack_ex`）复现，与产品掩码 **5744/5744 一致**）：

  | 口径 | 分母 | 漏排 | 漏检率 | 检出率 |
  |---|---|---|---|---|
  | 全部取样点 | 5785 | 287 | 4.96% | 95.04% |
  | **显著点（正确口径）** | **5611** | **220** | **3.92%** | **96.08%** |
  | **真·单异常点**（栈内恰好 1 帧 `z>+5` 且无 `z<−5`） | **4556** | **30** | **0.66%** | **99.34%** |
  | 多异常点（≥2 帧 `z>+5`） | 451 | 50 | 11.09% | 88.91% |
  | ↳ 其中 `n = 6..15`（winsorized 档） | 3956 | 60 | 1.52% | 98.48% |
  | ↳ 其中 `n ≥ 16`（linear_fit 档） | 1655 | 160 | 9.67% | 90.33% |

- **分母口径说明（这是被订正的那个数）**：5785 点里有 **174 点**没有 `z_trail > 5` 的异常（卫星线太暗），
  把这类点计入分母会把漏检率抬到 **4.96%**——该读法**已作废**；正确读数是**显著点口径 3.92%（220/5611）**，
  而**真·单异常口径的检出率 = 99.34%（4526/4556）**。两者必须连同分母一起引用。
- **漏排成因（订正）**：漏排点的主导形态**不是**「同一条卫星线在同一像素产生 2 个同侧离群」——
  实测 5785 点中 **87.6%（5067 点）栈内只有 1 帧 `z>+5`**，没有任何一例是同一颗卫星在同一像素两次成像。
  主因是「**1 个高异常 + 1 个低异常**」两端同时拉动秩轴拟合（79/220），低异常主要来自单帧的线性暗伪影；
  「同侧多离群导致检出率随幅度下降」这一说法**已收回**。
- **复现**：`run/REJECT-DOCFIX-01/scripts/m3_m5_eval.py`（生产 kernel 三臂）+ `m3_m5_metrics.py`（读数）；
  证据 `run/REJECT-DOCFIX-01/evidence/m3_m5_eval.json`、`m3_m5_metrics.json`。

