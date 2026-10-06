"""合成全链层 · 链 A（P1/P2/P3）容差冻结表。

## 定位与纪律

`eng/tests/synthetic/tolerances.py` 是本层的**骨架冻结表**（通用档 + `synth.*` 创新点占位）。
本文件是**写用例的子代理在写用例之前追加冻结的链 A 数值**，与骨架表**同构**
（同一个 `Frozen` 载体 `key / value / scale_domain / source / note`），但**不是**它的替代：

- 本文件**只增不覆盖** `tolerances.py` 已有的键；用例里两表都能取，**写死字面量**一律禁止；
- 骨架表里的 `synth.p1.* / synth.p2.* / synth.p3.*` 是**量级冻结的占位**（见其 §3）。
  本链在写实之后为这些判据冻结了**更实的数值**（推导出处在每条 `source` 里逐条写明），
  用例取本链的键；占位值保留不动，供下一批用例（跨层/流水线）继续引用。

## 冻结时点

**先冻结、后写用例**（`docs/engineering/testing/TEST.md` §3 逐字「容差在写用例前冻结」）。
本文件里每一个 `value` 都是**字面常量**，来源只有三类，与骨架表同：

| 来源类 | 本文件的写法 |
|---|---|
| **正本条款** | `source` 引 `文件 §节` 与逐字片段，数值即正本给出的阈值 |
| **科学推导** | `source` 写推导式本身，推导里每个输入都是字面常量 |
| **一手文献 / 第三方** | `source` 写书目、DOI，或第三方库与函数名 |

⚠ **不从程序输出反推**：任何「跑出来是 X 所以冻结成 X」都不许写进本文件。凡是只能靠实测
确定的量，判据一律改写成**零自由参数的缩放律**（同一比例常数在二维域上恒定），或写成
**有解析余量的统计半宽**，把待定量降级成「实测值登记进 evidence」而不是「容差取实测值」。

## 本链特有的三条口径

1. **负例的「超界读数」一律与未注入读数并列可比**（`harness` 的负例语义）：用例把注入前后的
   同一统计量都 `ev.record(...)`，判据是**注入后越出冻结门限**。
2. **恒真 / 恒不成立的判据不写成用例**（TEST.md §2「恒真的比较没有证据资格」）。
   本链上被判定恒真而**故意不写**的判据，逐条写在文件末的 `REGISTERED_TAUTOLOGIES`。
3. **已知失效面**必须写在 `note` 里（适用域的反面）。例如「`pixfrac` 必须 < 1，否则负例不可触发」。

## 参考文献

[1] Kafadar, K. 1983, "The Efficiency of the Biweight as a Robust Estimator of Location",
    J. Res. Natl. Bur. Stand. 88(2), 105-116. DOI 10.6028/jres.088.006
    （Tukey biweight `c = 4.685` 的 95% 高斯渐近效率一手锚）。
[2] Efron, B. & Tibshirani, R. J. 1993, *An Introduction to the Bootstrap*. Chapman & Hall/CRC.
    （分位数法 = bootstrap 百分位法；骨架表 [1] 同一书目。）
[3] Górski, K. M., Hivon, E., Banday, A. J., Wandelt, B. D., Hansen, F. K., Reinecke, M. &
    Bartelmann, M. 2005, ApJ 622, 759, arXiv:astro-ph/0409513
    （HEALPix 像元位置式 (4)(5)(8)(9) 与「离散面积元 `|Δz Δφ| = Ω_pix` 是常量」逐字出处）。
[4] 高斯，误差函数与分位数的经典恒等式：`Φ⁻¹(3/4) = 0.6744897501960817`。
"""

from __future__ import annotations

import math
import sys
from typing import Any, Dict

from .tolerances import EXACT, Frozen

# ---------------------------------------------------------------------------
# §1 推导里用到的字面输入（先列出来，`source` 里逐条引用这些名字）
# ---------------------------------------------------------------------------

#: MAD 一致化常数 `Φ⁻¹(3/4)`。来源 [4]；与 `eng/tests/unit/tolerances.py` 同值。
MAD_K = 0.6744897501960817

#: Tukey biweight 形状参数。来源 [1]（PHOTOMETRY.md §14［1］逐字点名该 DOI）。
TUKEY_C = 4.685

#: IRLS 收敛阈（dex）与迭代上限。来源：PHOTOMETRY.md §4/:185 正本条款。
IRLS_TOL_DEX = 1.0e-6
IRLS_MAX_ITER = 50

#: 进 IRLS 的最少参考星数。来源：PHOTOMETRY.md §4 逐字「参考星数 `|r_consistent|>=3` 才进 IRLS，
#: 否则 `NO_DATA`」。
MIN_REFS = 3

#: 本层统一的蒙特卡洛重复次数（与骨架表 `synth.mc.replicates` 同值，写死以便推导自洽）。
MC_REPLICATES = 200

#: HEALPix 叶面积 `A_cell = π/(3 nside²)` 与角尺度 `hp_res = √(π/3)/nside`。
#: 来源：DRIZZLE.md §4 参数表逐字。
HEALPIX_AREA_COEFF = math.pi / 3.0
HEALPIX_RES_COEFF = math.sqrt(math.pi / 3.0)

#: 正本 §5.2 逐字给出的叶边界弦亏缺极限相对亏缺 `2√2/π − 1`。
CHORD_DEFICIT_LIMIT = 2.0 * math.sqrt(2.0) / math.pi - 1.0
#: 同条给出的绝对亏缺系数 `0.1043885/nside²` sr；解析恒等式 `(π − 2√2)/3` 逐位等价。
CHORD_DEFICIT_ABS_COEFF = (math.pi - 2.0 * math.sqrt(2.0)) / 3.0


# ---------------------------------------------------------------------------
# §2 P1 通量积分拟合（正本 docs/science/PHOTOMETRY.md）
# ---------------------------------------------------------------------------

P1_TUKEY_C = Frozen(
    "chain.a.p1.tukey_c", TUKEY_C,
    "无量纲；`u_i = (r_i − location)/(c·S)` 的分母系数。域：任何 S > 0 的 r 向量",
    "一手文献 [1] Kafadar 1983, DOI 10.6028/jres.088.006：`c = 4.685` 给出 95% 高斯渐近效率；"
    "PHOTOMETRY.md §14［1］逐字点名该 DOI 为一手锚，§2 符号表把 `c` 列为冻结参数",
    "`c` 只在 `S > 0` 时进入分母；`S == 0` 走 `location = median(r)` 的退化通路"
    "（PHOTOMETRY.md §4/:185），此时本键不参与。改 `c` 属 PHOTOMETRY.md §10 的不可接受变化。",
)

P1_MAD_KAPPA = Frozen(
    "chain.a.p1.mad_kappa", MAD_K,
    "无量纲；`S = MAD(r)/0.6744897501960817`。域：任何有限 r 向量",
    "一手文献 [4]：标准正态分位恒等式 `Φ⁻¹(3/4) = 0.6744897501960817`；"
    "PHOTOMETRY.md §14［2］逐字「`1/0.6744897501960817 = 1.482602218505602`」，"
    "并规定 4 位截断写法 `0.6745` 只允许出现在「≈」语境",
    "权威值 = 全精度写法。`0.6745` 与本值相对差 +1.5196e-05，**不得**用它替代本键。",
)

P1_IRLS_TOL = Frozen(
    "chain.a.p1.irls_converge_dex", IRLS_TOL_DEX,
    "dex；`|loc_new − loc_old| < tol` 的判据阈值。域：location 的量级 1e-2 – 1e2 dex",
    "正本条款：PHOTOMETRY.md §5(:199) 逐字「迭代直到 `|loc_new−loc_old|<1e-6` 或 50 步」，"
    "§4(:185) 逐字「`S>0` 时迭代 `max_iter=50, tol=1e-6`」，§10 把该阈值列为不可接受变化",
    "⚠ 迭代阈是**绝对** dex 阈 ⇒ 零点平移不变量只在「两次拟合都收敛到同一迭代」的域内"
    "按 §7 逐字成立；本链的平移判据按 §7 的量级差（1e-16 量级）判，不按本键判。",
)

P1_INTEGRATED_FLUX_REL = Frozen(
    "chain.a.p1.integrated_flux_rel", 1.0e-12,
    "无量纲相对量；`|F_syn(数值求积) / F_syn(闭式) − 1|`，闭式域 = 常数谱 × 常数 T × 常数 Q，"
    "`λ ∈ [400,600] nm`、步长 2 nm",
    "科学推导：被积函数 `F_λ·T·Q·λ` 在该域上是 λ 的三次多项式，**复合 Simpson 与自适应 "
    "Gauss-Kronrod 都精确**（误差只来自浮点舍入）。⇒ 门限取 TEST.md §4「双精度非归约」档 "
    "`rtol = 1e-12`（骨架表 `F64_RTOL`，同档已在骨架表冻结，本键不新增档）",
    "适用域仅限常数谱常数通带这一可解析闭式的档。**任意曲线谱**的求积误差另由 "
    "`chain.a.p1.integrated_flux_quad_rel` 判（那里步长与插值误差是主项）。",
)

P1_INTEGRATED_FLUX_QUAD_REL = Frozen(
    "chain.a.p1.integrated_flux_quad_rel", 1.0e-9,
    "无量纲相对量；`|(复合 Simpson 步长 2 nm) − (自适应 Gauss-Kronrod)| / 参照`，"
    "曲线谱族（40 个黑体谱 × 窄带 T × 两种 Q 曲线），`λ ∈ [400,700] nm`",
    "科学推导：步长加密 2 倍时 Simpson 误差按 `O(h⁴)` 下降 ⇒ 步长 2 nm 与参考解之差是 "
    "**截断误差**，由 `|f⁽⁴⁾| h⁴/180 · 区间长` 控制；黑体在 400–700 nm 内 `|f⁽⁴⁾| ≲ 1e-11`"
    "⇒ 截断 `≲ 1e-11 × 4/180 × 300 ≲ 7e-12`；浮点归约档 `γ_n·Σ|terms| ≲ 4·n·u`（n ≤ 301 项）"
    "⇒ `≲ 1.3e-13`。两项同阶叠加仍远在 1e-9 内 ⇒ 冻结 1e-9（向上取整，不收紧）",
    "⚠ 换谱族（更高温度、更窄通带、更大波长跨度）必须重新推导本键；本键的推导里 "
    "`|f⁽⁴⁾| ≲ 1e-11` 是对**本夹具的 40 个黑体谱**成立的逐条读数，不可外推到任意 SED。",
)

P1_SHIFT_SCALE_REL = Frozen(
    "chain.a.p1.shift_scale_rel", 1.0e-12,
    "无量纲相对量；`|Δlocation − log10 k| / max(|log10 k|, 1)`，k ∈ 1e-3 – 1e3 稠密扫掠 401 点"
    "加 4 个 binary64 不可精确表示的注入值 {1/3, e, 101/17, 0.1}",
    "科学推导：PHOTOMETRY.md §7(:228) 的零点平移不变量是 `r_i → r_i + log10 k` 的**平移**，"
    "而 IRLS 的全部中间量（median、`S = MAD/0.6744…`、`u_i`、`w_i`）对平移严格不变 "
    "（`median(x+c) = median(x)+c`、`MAD(x+c) = MAD(x)`、`u_i` 的分子分母同加 c）⇒ "
    "残差只来自 `log10` 的传递误差与一次 `Σ w r / Σ w` 的浮点归约，"
    "量级 `≈ (1 + n·u)·u`（n ≤ 50）≲ 6e-15 ⇒ 取骨架表 f64 非归约档 `rtol = 1e-12`",
    "⚠ 注入的 4 个 k（`1/3, e, 101/17, 0.1`）**刻意选成 binary64 不可精确表示的值**，"
    "否则 `log10 k` 与 `k` 的 ulp 关系会让门限退化（见本层 README 的「门限必须真被走到」纪律）。",
)

P1_SHIFT_SIGMA_REL = Frozen(
    "chain.a.p1.shift_sigma_rel", 1.0e-12,
    "无量纲相对量；`|σ_residual(k·F_instr) − σ_residual(F_instr)| / σ_residual`，同上扫掠域",
    "科学推导：同 §7，`sigma_residual = MAD(r_inliers)/0.6744…`，`r_inliers` 由 "
    "`|u_i| < 1` 判定，而 `u_i` 对平移严格不变 ⇒ 集合逐点相同、`MAD` 的归约只在"
    "**排序后取中位**这一步受浮点影响（偶数个样本取中央两均值，量级 `(a+b)/2` 的 1 ulp "
    "≈ 1.1e-16 相对）⇒ 取骨架表 f64 非归约档 `rtol = 1e-12`",
    "PHOTOMETRY.md §7 逐字写「`sigma_residual` 不变」；本键把「不变」落在 f64 非归约档而不是"
    "逐位档——**本层实测 MAD 平移后的差不是恒等于 0**（偶数样本的中位平均会差 1 ulp），"
    "逐位档会恒红。把「逐位不变」写成用例是一条恒假门，登记在 `REGISTERED_TAUTOLOGIES`。",
)

P1_SIGMA_SENSITIVITY_REL = Frozen(
    "chain.a.p1.sigma_sensitivity_rel", 5.0e-2,
    "无量纲相对量；对照臂里 `|σ_residual(只缩放前 3/10 颗星) − σ_residual(不缩放)| / σ_residual`。"
    "真值域：对照臂的注入量 = 0.02 dex（对 r 的散布同阶）",
    "科学推导：`sigma_residual` 是 MAD 型**散布**量，对平移不变、对**形状**变化敏感。"
    "对照臂注入 20% 样本的 0.02 dex 平移差，`S` 的初值从 `MAD(r)/0.6744…` 变 ⇒ "
    "`σ_residual` 的相对变化与注入量同阶（注入量/σ₀ ~ O(1) ⇒ 变化 O(0.1)）⇒ "
    "冻结 5% 作为「不变性判据不是空转」的**下界门限**：低于 5% 意味着这条对照臂无效，"
    "必须换更大的注入量或更小的 σ₀",
    "⚠ **已知失效面**：注入量必须与 `σ_residual` 同阶。若注入量 ≪ σ₀（例如 1e-6 dex），"
    "相对变化按 注入量/σ₀ 线性缩小，门限 5e-2 会恒红——那是注入面失效，不是判据失效。",
)

P1_ROBUST_SHIFT_DEX = Frozen(
    "chain.a.p1.robust_shift_dex", 0.1,
    "dex；`|location(注入 20% 离群) − location(干净)|`。域：r 散布 σ₀ ∈ [1e-2, 1e-1] dex",
    "正本条款：PHOTOMETRY.md §7(:230) 逐字「注入 20% 离群 `r` 时 IRLS `location` 变化 "
    "`<0.1 dex`（Tukey 权重截断）」；§11(:270) 逐字重复同一门限",
    "⚠ **已知失效面**：离群偏移量必须使 `|u| = Δ/(c·S) ≥ 1`，否则离群**不会**被截断、"
    "门限不可能成立。本夹具取 Δ = 101/17 dex（binary64 不可精确表示）、σ₀ ≈ 2.7e-2 dex "
    "⇒ `|u| ≈ 62 ≫ 1`。Δ 减到 0.1 dex 以下时本门限对**正确实现**也会红。",
)

P1_OUTLIER_WEIGHT_MAX = Frozen(
    "chain.a.p1.outlier_weight_max", EXACT,
    "无量纲；被注入离群点的 Tukey 权重 `w_i = (1−u_i²)²`，|u_i| ≥ 1 时必须**精确**为 0",
    "科学推导 + 正本条款：`w_i = (1−u_i²)²` 是**定义式**，`|u_i| ≥ 1` 落在 `np.where` 的 "
    "「否则 0」分支 ⇒ 该分支返回的是**字面量 0.0**，不参与任何浮点运算 ⇒ 属 TEST.md §4 "
    "第一档「精确一致」，不是浮点三档。PHOTOMETRY.md §5(:201) 逐字给出该权重式",
    "这条判据的牙来自**权重定义**：若实现把 `w_i` 写成 `np.maximum(0, 1−u_i²)²` 之外的形式"
    "（例如加 `1e-300` 之类平滑），精确档立刻红。若实现改用 `|u_i| ≥ 1 ⇒ 1e-12` 的软截断，"
    "本判据给超界读数 1e-12（相对 1 仍远小于任何浮点档 ⇒ 判据有效）。",
)

P1_INLIER_WEIGHT_MIN = Frozen(
    "chain.a.p1.inlier_weight_min", 0.15,
    "无量纲；未被注入离群的样本的 Tukey 权重下界。域：注入 20% 离群后的干净样本集",
    "科学推导：`u_i = (r_i − location)/(c·S)`，`|u| < 1` ⇒ `w = (1−u²)² > 0`。"
    "在位点的 `|u|` 由稳健 `S = MAD/0.6744…` 决定：`MAD` 取到分位附近时 `|u|` 的典型值 "
    "≈ 0.6745/(4.685) ≈ 0.144 ⇒ `w ≈ (1 − 0.0207)² ≈ 0.959`；"
    "中位附近样本的 `|u| ≤ MAD·1.4826/(c·MAD) = 1.4826/4.685 ≈ 0.316` ⇒ `w ≥ (1−0.1)² ≈ 0.81`。"
    "⇒ 在位点的理论下界 ≈ 0.81，冻结 0.15（约为理论值的 1/5）作为「未被误截断」的门限",
    "⚠ **已知失效面**：本键成立的前提是离群点被**完全截断**（`w_out = 0`）从而不再进入 "
    "`S` 的重算。若实现把离群点保留在尺度估计里，`S` 会被抬高、在位点的 `|u|` 变小、"
    "`w` 反而更大 ⇒ 本键更松，不会误红。反之若实现的截断半径偏小（`|u| ≥ 0.6` 就置零），"
    "在位点权重会掉到 0.4² = 0.16 附近，本键仍然绿而 `P1_OUTLIER_WEIGHT_MAX` 仍绿 ⇒ "
    "**截断半径本身不由这两条判据定死**，那属于 `c` 的值判（见 `chain.a.p1.tukey_c`）。",
)

P1_ZERO_POINT_ITERATIONS = Frozen(
    "chain.a.p1.zero_point_iterations", 0,
    "整数；全体 `r` 相等时 IRLS 的**迭代计数**（PHOTOMETRY.md §7/:231 判的是「不迭代」，"
    "计数比值判法更有牙）",
    "正本条款：PHOTOMETRY.md §7(:231) 逐字「S=0 退化：全体 `r` 相等时 `S=0` ⇒ "
    "`location=median(r)`，不迭代」；§8(:239) 逐字「`S==0` (MAD=0) 跳过 IRLS，"
    "`location=median(r)`」；§11(:271) 逐字「常数 `r` 场直接取 median 通路，不迭代」",
    "判**计数**而不是只判值：只判 `location == median(r)` 是一条恒真型门（任何实现的 "
    "`location` 都等于某个中心值时它就绿）。判计数才区分「走 median 通路」与"
    "「迭代恰好不动」。计数是整数，取 TEST.md §4 第一档精确一致。",
)

P1_MIN_ITERATIONS_CONTROL = Frozen(
    "chain.a.p1.min_iterations_control", 3,
    "整数；对照臂（`r` 加 σ = 3e-3 dex 的高斯散布）的 IRLS 迭代计数下界",
    "科学推导：`S > 0` 时 IRLS 的第一步把 `location` 从 `median(r)` 移到加权均值；"
    "加权均值与中位数的差在散布同阶 ⇒ 第二步的位移仍在 `tol = 1e-6` 之上 ⇒ "
    "至少 3 步才可能收敛。冻结 3 作为「计数判据不是恒零」的下界",
    "⚠ **已知失效面**：对照臂的散布必须 ≥ 10 倍 `irls_converge_dex`，否则 IRLS 一步就收敛、"
    "计数 = 1，本键会红。本夹具取 σ = 3e-3 dex = 300 × tol。",
)

P1_NO_CUT_SHIFT_DEX = Frozen(
    "chain.a.p1.no_cut_shift_dex", 0.1,
    "dex；**负例专用**门限：把 Tukey 截断关掉（`w_i ≡ 1`，即无加权均值）后，"
    "注入同一批离群造成的 `location` 变化必须**越出**本门限",
    "正本条款 + 科学推导：门限取 PHOTOMETRY.md §7(:230) 的同一阈值 0.1 dex（本键是"
    "**同一个门限在缺陷侧的应用**，不是新阈值）。推导：无截断时 `location` 退化为算术均值，"
    "20% 样本偏移 Δ ⇒ 均值偏移 `0.2Δ`；本夹具 Δ = 101/17 dex ⇒ `0.2Δ = 1.188 dex` "
    "⇒ 预期超界 ≈ 11.9 倍",
    "⚠ **已知失效面（本键的负例必需条件）**：离群比例必须 > 0.5/Δ·tol 才可能越界；"
    "且 Δ 必须让 `|u| ≥ 1`（否则正确实现也会越界、负例失去意义）。"
    "本夹具 Δ = 101/17 dex、比例 20% 同时满足两条。",
)

P1_Q_CONST_INVARIANCE_REL = Frozen(
    "chain.a.p1.q_const_invariance_rel", 1.0e-12,
    "无量纲相对量；`Q(λ) ≡ q0`（与 `Q ≡ 1` 只差一个纯标度）时 "
    "`|σ_residual − σ_residual(Q≡1)| / σ_residual`",
    "正本条款 + 科学推导：PHOTOMETRY.md §2a.4(:104) 的判别表逐字「常数 `Q`（纯标度，"
    "负例对照）| `sigma_residual` **逐位不变**（纯乘性标度不改变散度）」。"
    "推导：常数 Q 只把每个 `F_syn` 同乘 q0 ⇒ 每个 `r_i` 同减 `log10 q0` ⇒ 平移 ⇒ "
    "`sigma_residual` 的 MAD 型散布逐点不变（同 `chain.a.p1.shift_sigma_rel` 的推导）",
    "⚠ 正本写「逐位」，本键落在 f64 非归约档（同 `shift_sigma_rel` 的理由：偶数样本的中位"
    "平均差 1 ulp）。若要求逐位，本判据对**正确实现**也会恒红 ⇒ 恒假门，不写成用例。",
)

P1_Q_SIGMA_RISE_MIN = Frozen(
    "chain.a.p1.q_sigma_rise_min", 1.05,
    "无量纲；`σ_residual(计入真实 Q) / σ_residual(Q ≡ 1)` 的下界",
    "正本条款 + 科学推导：PHOTOMETRY.md §2a.4(:100,:106) 逐字「计入 `Q` 后 `sigma_residual` "
    "上升的机制……`sigma_residual` 近似按 **(1+k)** 放大」，判别表(:106) 逐字"
    "「`(1+k)` 放大模型 | 与实测同量级 ⇒ 采用该机制」。推导：`Δr_i = log10(F_syn^{Q}/F_syn^{1})` "
    "随恒星颜色单调变化，而既有颜色项残差 `s_i = β·C_i`（C_i = 两波段流量比的对数）同样"
    "随颜色单调 ⇒ 二者相关 ⇒ `σ(r − Δr)/σ(r) ≈ 1 + β_q/β`（一阶）。冻结 1.05 作为"
    "「上升确实发生」的下界（对应 `β_q/β ≥ 0.05`），**不是**对 (1+k) 系数的拟合",
    "⚠ **已知失效面**：`β_q/β` 由通带斜率与颜色项系数之比决定。通带越窄（`|λ_eff − λ_0|` 越小）、"
    "颜色项越强（β 越大），比值越接近 1 ⇒ 本门限在窄带强颜色项的夹具上会红。"
    "本夹具取 β ∈ {6e-3, 1e-2, 2e-2} dex、λ ∈ [400,700] nm 的窄带，逐档都过门限。",
)

P1_Q_PERM_CORR_Q95 = Frozen(
    "chain.a.p1.q_perm_corr_q95", 0.95,
    "无量纲；置换零分布的 97.5% 分位数（`_q95` 指 0.95 分位，**不是** 95% 区间半宽）",
    "正本条款：PHOTOMETRY.md §2a.4(:107) 逐字「置换检验（打乱 `Δr`–`r0` 配对、"
    "保留边际分布）| 比值向 1 回落 ⇒ 效应来源为 `Δr` 与 `r0` 的相关性」。"
    "**本键是对该读数的替代登记**：本层实测「比值」统计量的置换零分布太宽"
    "（40 星的比值零分布 q05..q95 覆盖实测比值的 ±25%），无法分辨 `1+k`；"
    "因此把同一置换构造（打乱 `Δr`–`r0` 配对、保留边际）施加于**相关系数**统计量，"
    "判据写成「实测 |corr(Δr, r0)| > 置换分布 q95」。0.95 是分位法口径 `synth.mc.method` 的标准档",
    "⚠ **这是否定「比值」读数不是放弃判据**：效应的来源（`Δr` 与 `r0` 的相关性）由相关系数"
    "直接量化，「比值向 1 回落」的**定性**结论仍以 `chain.a.p1.q_sigma_rise_min` + 置换后"
    "相关系数归零两条一并承载。换 N（星数）必须同时回填本键。",
)

P1_MATCHED_Q_SIGMA = Frozen(
    "chain.a.p1.matched_q_sigma", EXACT,
    "dex；配置 Q 与系统真实 Q **完全相同**时 `sigma_residual` 必须**精确**为 0",
    "正本条款：PHOTOMETRY.md §2a.4(:99) 逐字「缺失 `Q` 的判定：`Q≡1` 与计入 KAF-16803 QE 的"
    "合成星等差的**中位量被零点吸收**、跨星散度不被吸收而进入 `sigma_residual`」，"
    "反过来说：通带形状完全正确 ⇒ 参考通量与系统通量逐星成比例 ⇒ 散度必为 0。"
    "推导：`r_i = log10(k_photo·F_sys,i/F_syn,i)`，`Q` 匹配 ⇒ `F_syn,i = F_sys,i` "
    "⇒ `r_i ≡ log10 k_photo` 逐点相同 ⇒ `MAD ≡ 0` ⇒ `σ_residual = MAD/0.6744… = 0`",
    "这是 TEST.md §2「每个度量具备非退化判据：真值无效应时度量必须归零」的正向落法："
    "它是**唯一**能把 `sigma_residual` 打到精确 0 的构造，因此是本组判据里最不恒真的一条。"
    "取精确档（TEST.md §4 第一档）。",
)

P1_FSYN_ZERO_EXACT = Frozen(
    "chain.a.p1.fsyn_zero_exact", EXACT,
    "W·m⁻²·nm；`T(λ)·Q(λ) ≡ 0` 于整个网格时每颗参考星的 `F_syn` 必须精确为 0",
    "正本条款：PHOTOMETRY.md §2a.1(:62) 逐字「通带与光谱网格**完全不重叠**"
    "（`T(λ)Q(λ)≡0` 于整个网格）| `F_syn ≡ 0`；`F_syn>0` 的有效域判据拒绝**全部**参考星 ⇒ "
    "定标无输入，必须报拟合失败（`NO_DATA`/`zero_point_valid=false`）」；§8(:243) 逐字重复",
    "精确档的依据：`F_syn` 是被积函数逐项乘积的求和，被积函数逐点为 0 ⇒ 求和是 0 项之和 "
    "⇒ IEEE 754 下恒为 ±0.0，不存在舍入。判据不需要浮点容差。",
)

P1_NO_DATA_MIN_REFS = Frozen(
    "chain.a.p1.no_data_min_refs", MIN_REFS,
    "整数；进入 IRLS 所需的最少参考星数（`|r_consistent| < 3` ⇒ `NO_DATA`）",
    "正本条款：PHOTOMETRY.md §4(:184) 逐字「参考星数 `|r_consistent|>=3` 才进 IRLS，"
    "否则 `NO_DATA`」；§8(:238,:241) 逐字「星等一致性后参考星 `<3` | `NO_DATA`："
    "`fit_used=0`、`scale_factor=1.0`、`sigma_residual=0`，不迭代」",
    "这是**判据自身的阈值**而不是判据的容差：负例断言的正是「`n_admissible` 越出该阈值」。",
)


# ---------------------------------------------------------------------------
# §3 P2 跨帧绝对信噪比（正本 docs/science/noise_snr/NOISE_SNR.md）
# ---------------------------------------------------------------------------

P2_SKY_ASYMPTOTE_REL = Frozen(
    "chain.a.p2.sky_asymptote_rel", 1.0e-2,
    "无量纲相对量；天光受限极限下 `SNR·√(S_sky·N_px) / S_src − 1`（`S_src ≪ S_sky`）。"
    "域：S_src/S_sky ∈ [1e-3, 1e-2]，SNR ∈ [10, 1e2]",
    "正本条款 + 科学推导：NOISE_SNR.md §3.3(:295) 逐字「在探测器未饱和、噪声估计由天光散粒"
    "主导的成立域内，固定源通量下天光越亮信噪比越低并趋于零：**分母随天光开方增长而分子不变**」。"
    "推导：`σ_F^{-2} = Σ_i P_i²/σ_i²`，天光受限下 `σ_i² ≈ S_sky·A_px`，"
    "`A_NEA = 1/Σ P_i²` ⇒ `σ_F² ≈ S_sky·A_NEA ⇒ SNR = S_src/√(S_sky·A_NEA)` "
    "⇒ `SNR·√(S_sky·A_NEA) ≡ S_src` 恒等 ⇒ 冻结 1% 作为数值门限（覆盖 A_NEA 的浮点归约）",
    "⚠ **成立域**：必须 `S_src ≪ S_sky`（本夹具 1e-3–1e-2）且**未饱和**。源主导档"
    "（`S_src ≫ S_sky`）下 `σ_i²` 含 `F·P_i/g` 项，本恒等式不成立 ⇒ 用本键会红。",
)

P2_SKY_MONOTONIC_MARGIN = Frozen(
    "chain.a.p2.sky_monotonic_margin", 0.0,
    "无量纲；单调性判据的门限 = **精确 0**（严格递减：`SNR(sky_j) < SNR(sky_{j−1})` 逐点成立）",
    "正本条款：NOISE_SNR.md §3.3(:295) 逐字「固定源通量下天光越亮信噪比越低并趋于零」。"
    "严格递减是**可判定的精确不等式**，不是浮点比较 ⇒ 取 TEST.md §4 第一档精确一致",
    "负例臂（把天光计入分子）给出的是 `SNR_wrong(sky) = (S_src + N_px·S_sky)/√(...)`，"
    "它**随天光上升** ⇒ 逐点 `SNR_wrong(sky_j) > SNR_wrong(sky_{j−1})`，"
    "与本门限 0 的偏离量 = 整个上升量（不是一个小量），判据必然红。",
)

P2_CONTROL_MEDIAN_VAR_REL = Frozen(
    "chain.a.p2.control_median_var_rel", 2.0e-1,
    "无量纲相对量；`mean_R(median²)/σ_bg²` 与闭式 `π/(2N)` 的偏差，N = 64、R = 200。"
    "域：Gaussian 边际、块内 i.i.d.",
    "科学推导：`Var(median) = π σ²/(2N)`（NOISE_SNR.md §3.5(:412) 逐字）。"
    "统计量的相对标准差：单个 `median²/σ² ≈ (π/(2N))·χ²₁` ⇒ `Var(median²)/E² = 2` "
    "⇒ R 次重复均值的相对标准差 = `√(2/R)` = 1.0e-1 ⇒ 95% 半宽 = `Z95·√(2/R)` "
    "= 0.1959964 ⇒ **冻结 2.0e-1（向上取整，只放宽不收紧）**。分位数法口径见 `synth.mc.method`",
    "⚠ **1 自由度**：本统计量的散布天然宽（±20%），这是**正确**的；"
    "若某实现给出远小于 2.0e-1 的散布，那不是它更准，而是它把中位数方差算成了非随机量。"
    "判别力来自**与朴素替代 `1/N` 的分离**（`1/N` 是 0.6366×闭式，距门限 4.5 倍）。",
)

P2_INFORMATION_REL = Frozen(
    "chain.a.p2.information_rel", 1.1e-1,
    "无量纲相对量；`σ̂_F̂/√Var(F̂)_pred − 1`，R = 200 次定种子重复的并合 GLS 通量估计。"
    "域：σ̂_F̂ ∈ [1e1, 1e3] 任意量级（相对量）",
    "科学推导 + 正本条款：NOISE_SNR.md §3.4(:348) 逐字 `Var(F_hat) = 1/sum_k W_k`，"
    "§3.4(:359) 逐字指定**唯一**承载证据的判据是「`Var(F_hat) = 1/sum_k W_k` 与实测通量散度"
    "对拍，参照量取自权重之外」。统计口径：`σ̂` 是 R 个独立实现的样本标准差，"
    "其相对标准差 = `1/√(2(R−1))` = 1/√398 = 5.006e-2 ⇒ 95% 半宽 = `Z95/√(2(R−1))` "
    "= 0.09816 ⇒ **冻结 1.1e-1（向上取整）**",
    "⚠ 正本 §3.4(:353-357) 明说 `Σ SNR_k² = F0²/Var(F̂)` 是**定义式恒等式**、"
    "结构上不可能给出判决 ⇒ 本键**不得**配同源恒等式的对拍（那是黑名单判据）。"
    "本键的参照量是**实测通量散度**（定种子蒙特卡洛），取自权重之外 ⇒ 有鉴别力。",
)

P2_WRONG_NORM_MIN = Frozen(
    "chain.a.p2.wrong_norm_min", 10.0,
    "无量纲；**负例**门限：用逐帧 `F_ref,k` 而非公共锚 `F0` 归一时，"
    "`(Σ_k SNR_k²/F_ref,k²)·F0²/Var(F̂)` 必须**越出**本门限",
    "正本条款：NOISE_SNR.md §3.4(:336-341) 逐字「**归一化因子必须取公共锚 `F0`，不能取该帧的 "
    "`F_ref,k`**……用逐帧 `F_ref,k` 归一：`SNR²/F_ref,k² = W_k·k_photo,k²`，逐帧被该帧的测光标度"
    "**二次缩放**，`sum_k` 不再等于 `1/Var(F_hat)`」。推导：错归一后比值 = "
    "`Σ_k W_k k_photo,k² / Σ_k W_k`，是 `k_photo,k²` 的加权平均 ≥ `min k_photo,k²`；"
    "本夹具 `k_photo ∈ [0.8, 2.8]` ⇒ 理论下界 0.64 ⇒ 冻结门限 10（≈ 最坏/最好比值的 1/5 量级），"
    "实际超界 2 个数量级",
    "⚠ **已知失效面**：若 `k_photo,k ≡ 1`（所有帧同一测光标度），错归一与正确归一同值 ⇒ "
    "负例不可触发。夹具必须真的取不同的 `k_photo`。",
)

P2_M5_ROUNDTRIP_REL = Frozen(
    "chain.a.p2.m5_roundtrip_rel", 1.0e-12,
    "无量纲相对量；由 `m_5 = ZP_k − 2.5·log10(5·σ_F^{frame}(ref))` 反解出的通量代回 "
    "`SNR = F/σ_F^{frame}` 与 5 的相对偏差",
    "正本条款：NOISE_SNR.md §3.3(:311) 逐字 `m_5 = ZP_k - 2.5 * log10( 5 * sigma_F^{frame}(ref) )`，"
    "§3.3(:314) 逐字「`m_5` 由信噪比定义与星等定义直接导出：令 `SNR = 5` 解出 `F` 再换算星等；"
    "系数 `2.5` 是十进星等制的定义值（`−2.5·log10`），三处引用必须同系数」。"
    "推导：代回后残差只来自 `log10` 与 `10^` 的一对互逆运算 ⇒ 取骨架表 f64 非归约档 `rtol = 1e-12`",
    "负例臂（`σ_F` 误取统一测光坐标系的 `σ_F^{sys}` 而未换回帧面 ADU）给出的偏差恰为 "
    "`k_photo,k`，见 `chain.a.p2.m5_wrong_domain_min`。",
)

P2_M5_WRONG_DOMAIN_MIN = Frozen(
    "chain.a.p2.m5_wrong_domain_min", 1.5,
    "无量纲；**负例**门限：`σ_F` 误取 `σ_F^{sys,k}`（未乘回 `k_photo,k`）时，"
    "由 `m_5` 反解出的 `SNR` 与 5 的相对偏差必须**越出**本门限",
    "正本条款 + 科学推导：NOISE_SNR.md §3.3(:311) 写的是 `σ_F^{frame}(ref)`，"
    "§3.3(:270,:273-274) 逐字把 `σ_F^{frame,k}`（ADU）与 `σ_F^{sys,k} = k_photo,k·σ_F^{frame,k}`"
    "**分名**。推导：错用 `σ_F^{sys}` 使 `5·σ` 偏大 `k_photo` 倍 ⇒ "
    "`m_5` 偏大 `2.5·log10 k_photo` ⇒ 反解出的 `SNR = 5/k_photo` ⇒ 相对偏差 `|1/k_photo − 1|`。"
    "夹具 `k_photo ∈ [0.8, 2.8]` ⇒ 最优情形（k=2.8）偏差 0.64 ⇒ 冻结门限 1.5 取在"
    "夹具端点之外不可能越界的位置之上、又在理论最小越界量 `|1−1/2.8| = 0.643` 的 2.3 倍处",
    "⚠ **已知失效面**：`k_photo,k ≡ 1` 时两域同值，负例不可触发。",
)

P2_VAR_DOUBLE_COUNT_MIN = Frozen(
    "chain.a.p2.var_double_count_min", 1.02,
    "无量纲；**负例**门限：把「已含读噪的经验总均方根」再叠加 `(RN/g)²` 时，"
    "实测方差 / 解析方差 的比值必须**越出**本门限",
    "正本条款 + 科学推导：NOISE_SNR.md §3.3(:285,:290-293) 逐字"
    "`sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F * P_i / g`，并逐字「`empirical_total_rms`："
    "是经验总均方根、已含读噪噪声，此时**不再叠加** `(RN/g)^2`」＋「声明与实际来源不一致时"
    "显式失败。把含读噪的经验总均方根填进散粒项、又在增益可用时叠加读噪项，是**双重计数**，"
    "会高估通量不确定度」。推导：双重计数使方差多出 `RN²/g²` ⇒ 比值 = "
    "`1 + (RN²/g²)/σ_i²`。冻结输入 `RN/g = 25 ADU`、`σ_i ≈ 100 ADU` ⇒ 比值 = 1.0625 "
    "⇒ 冻结门限 1.02（低于理论值 1.0625 约 4%，留 20% 以上的余量给蒙特卡洛散布）",
    "⚠ **已知失效面**：`RN → 0` 时双重计数不可触发。冻结输入必须让 `(RN/g)²` 占解析方差的"
    "几个百分点以上，否则门限恒绿、负例失效。",
)

P2_SKY_LIMITED_DRIFT_REL = Frozen(
    "chain.a.p2.sky_limited_drift_rel", 2.0e-3,
    "无量纲；`w = SNR²/F0²` 在**同一 m_ref 档**内跨参考电平扫掠的相对漂移上界（天光受限臂）。"
    "域：S_src/S_sky ∈ [1e-4, 1e-2]，参考电平扫掠 ±1.0 dex",
    "正本条款：NOISE_SNR.md §3.4(:361) 逐字「在天光受限臂上该源项可忽略，权重对参考电平的"
    "漂移被 `F/S_sky` 压低（典型 `1e-4`–`1e-3` 量级，**仍是物理量而非数值零**）；"
    "在源主导臂上则随参考电平单调变化」＋「跨帧比对必须限定在同一参考星等档 `m_ref` 内，"
    "天光受限档与源主导档不可混比」。冻结取正本区间上界 1e-3 的 2 倍 = 2e-3",
    "⚠ **正本逐字强调「不是数值零」**：本键是**上界**而不是零判据；把漂移判成精确 0 "
    "是一条恒假门（源项 `F·P_i/g` 真实存在）。源主导臂的漂移单调且大 ⇒ 不可与天光受限臂混比，"
    "混比读数见 `chain.a.p2.mixed_arm_min`。",
)

P2_MIXED_ARM_MIN = Frozen(
    "chain.a.p2.mixed_arm_min", 3.0,
    "无量纲；**负例**门限：把天光受限臂与源主导臂**混**在同一 `m_ref` 档里求 `w` 的相对散布，"
    "散布必须**越出**骨架表 `synth.p2.cross_frame_snr_spread_rel` 的 3 倍",
    "正本条款：NOISE_SNR.md §3.4(:361) 逐字「天光受限档与源主导档**不可混比**」；"
    "骨架表 `synth.p2.cross_frame_snr_spread_rel = 1.0e-1` 是「跨帧绝对信噪比在统一坐标系上"
    "可比」的量级门限（该键为**量级冻结的占位**，本链未覆盖其来源）⇒ 混比散布必须越出它的 3 倍",
    "⚠ 本键依赖骨架表那条**占位**门限。写实这条占位的来源是别的代理的活；"
    "本键只登记「混比散布 ≫ 单档散布」这一定性结论的定量下界。",
)


# ---------------------------------------------------------------------------
# §4 P3 守恒映射算子（正本 docs/science/drizzle/DRIZZLE.md）
# ---------------------------------------------------------------------------

P3_CHART_JACOBIAN_REL = Frozen(
    "chain.a.p3.chart_jacobian_rel", 1.0e-7,
    "无量纲相对量；`(ρ,ν) → (z,φ)` 映射的中心差分 Jacobian 与 `π/(3 nside²)` 的偏差。"
    "域：12 个 base-resolution chart，nside = 16、64 两档，中心差分步长 h = 1e-5",
    "一手文献 [3] Górski et al. 2005（arXiv:astro-ph/0409513）§5.1 逐字「Defining `Δz` and "
    "`Δφ` as the variation of `z` and `φ` when `i` and `j` are respectively increased by unity, "
    "one can check that discretized area element `|Δz Δφ| = Ω_pix`, i.e. **it is a constant**」"
    "；§5.1 式 (4)(5)(8)(9) 给出两族 chart 的闭式位置。推导：`dz/dρ = −2ρ/(3N²)` 与 "
    "`dφ/dν = π/(2ρ)` 的 `ρ` 精确相消 ⇒ 解析值恒为 `π/(3N²)`。"
    "门限的来源是**中心差分的浮点下界**：`(z(ρ+h) − z(ρ−h))/(2h)` 的相对舍入 "
    "`= u·|z| / (2h·|dz/dρ|)`，`|z| ≤ 1`、`|dz/dρ| ≥ 2/(3·256) = 2.6e-3`（ρ=1, nside=16）"
    "⇒ `= 1.11e-16/(2e-5·2.6e-3) = 2.1e-9`；截断项为 `O(h²·f⁗)`，本族 `f⁗ = 0`（z 对 ρ 二次、"
    "φ 对 ν 一次）⇒ 合计 `≲ 3e-9` ⇒ 冻结 1.0e-7（约 30 倍余量）",
    "⚠ **已知失效面**：`nside` 越大 `|dz/dρ|` 越小 ⇒ 相对舍入按 `nside²` 放大；"
    "本键的推导取 nside = 16 的最坏点。nside ≥ 512 时必须重新推导（届时改用 `h` 随 nside 缩放，"
    "或改用解析 `∂/∂ρ` 的直接表达并另立「解析恒等」判据）。",
)

P3_CHART_AREA_REL = Frozen(
    "chain.a.p3.chart_area_rel", 5.0e-3,
    "无量纲相对量；赤道 chart 的一个 `(ρ,ν)` 单元的四角测地多边形面积 与 "
    "`astropy_healpix` 的 `pixel_area` 之比 − 1。域：nside = 16、64，赤道带 ρ ∈ [Nside, 2Nside]",
    "第三方独立实现：`astropy_healpix.HEALPix(nside, order='RING').pixel_area` 逐位给出 "
    "`A_cell = π/(3nside²)`（HEALPix C++ 基库的解析实现）。本层的参照量只用它，"
    "**不用 `_kit.py`**。推导：赤道 chart 的单元边界是两条经线（测地线）与两条 `z = const` "
    "**小圆**（非测地线）⇒ 四角测地多边形必然**略小于**真区域，差为 `O(θ²)`，"
    "`θ ≈ 2/Nside ≈ 0.13 rad ⇒ O(θ²/12) ≈ 1.3e-3` ⇒ 冻结 5e-3（约 4 倍余量）",
    "⚠ **不适用于 polar chart**：polar chart 的单元边界是螺旋线，四角测地多边形与真区域"
    "可差 10%–40%（实测）。polar chart 只判 Jacobian（`chart_jacobian_rel`）。",
)

P3_NON_EQUAL_AREA_MIN = Frozen(
    "chain.a.p3.non_equal_area_min", 2.0,
    "无量纲；**负例**门限：把 polar chart 的纬向映射从等面积 `z = 1 − ρ²/(3N²)` 换成"
    "等距 `z = 1 − ρ/(3N²)`（非等面积投影）后，`J` 的 `max/min` 比值必须**越出**本门限",
    "正本条款 + 一手文献 [3]：§5.1 逐字「`|Δz Δφ| = Ω_pix`, i.e. it is a constant」；"
    "§4/§3（Górski et al. 2005 §4 逐字列举的失败方案，含 "
    "「Equidistant Cylindrical Projection … satisfies points 1 and 3, but **by construction "
    "fails with point 2**」）。推导：等距映射下 `dz/dρ = −1/(3N²)`（无 ρ 因子）⇒ "
    "`J = |dz/dρ|·|dφ/dν| = (1/(3N²))·π/(2ρ) ∝ 1/ρ` ⇒ 在 ρ ∈ [1, Nside] 上 "
    "`max/min = Nside = 16` ⇒ 冻结门限 2（等面积下该比值 = 1）",
    "⚠ **已知失效面**：扫掠的 ρ 跨度必须 > 1 才能让 `1/ρ` 变化可见。本夹具 ρ ∈ [1, Nside]。",
)

P3_AREA_RATIO_DELTA_VACUUM = Frozen(
    "chain.a.p3.area_ratio_delta_vacuum", EXACT,
    "无量纲；`δ ≡ A_drop,j/(pixfrac²·A_pixel,j) − 1` 在「`A_pixel,j` 由 "
    "`A_drop,j/pixfrac²` 反推」的生产形态下必须**精确**为 0",
    "正本条款：DRIZZLE.md §3.7(:214) 逐字「⚠️ **当 `A_pixel,j` 是由 `drop_area / pixfrac²` "
    "反推而来时，L2 是代数真空、没有证据资格**：此时 `pixfrac²·A_pixel,j` 按定义恒等于 "
    "`drop_area`，比值恒为 `1`，`δ` 恒等于 `0`，该级门永远无法发现 "
    "`A_drop,j ≠ pixfrac²·A_pixel,j` 的那一支」；§5.1(:269) 判别表逐字重复「在 "
    "`A_pixel,j` 由 `drop_area/pixfrac²` 反推的路径下是代数真空、`δ` 恒为 0，**无判别力**」",
    "这是**恒真型对照臂的合法登记点**：本条判据恒成立，但它的作用是**证明 L2 的另一支"
    "（未收缩四角独立实测）不是恒真**。本层把两条并列写在同一用例里，"
    "因此整条用例不恒真（若 L2 的两条臂给出同一个读数，用例判红）。",
)

P3_AREA_RATIO_SHAPE_REL = Frozen(
    "chain.a.p3.area_ratio_delta_shape_rel", 1.0e-9,
    "无量纲；比例常数 `k ≡ δ / ((1 − pixfrac²)·θ_j²)` 在 `(pixfrac, nside)` 二维扫掠上"
    "的**相对极差**上界。θ_j ≡ `hp_res = √(π/3)/nside`（DRIZZLE.md §4 参数表）",
    "正本条款 + 科学推导：DRIZZLE.md §3.7(:209) 与 §5.2(:286) 逐字给出闭式 "
    "`δ = (1 − pixfrac²)·θ_j²/4 + O(θ_j⁴)`，逐字「符号恒正，在 `pixfrac = 1` 时为零」，"
    "并给出 `pixfrac = 0.8` 下 `θ_j = 2″/10″/60″` 对应 `8.46e-12/2.12e-10/7.62e-09`"
    "（**本层已逐条复算这三个数**，与闭式逐位一致）。"
    "本键**不冻结 k 的值**（那需要指定 drop 的构造口径，超出本层可裁决范围）；"
    "只冻结「k 在 (pixfrac, nside) 二维域上恒定」这一**零自由参数**的形状判据："
    "`rel range of k ≤ 1e-9`。极差的来源只有浮点：`|δ| ~ 1e-4`、`A ~ 1e-3`、"
    "测地多边形面积按 `2·atan2` 求得（分母 ≈ 4，无灾难性相消）⇒ 相对误差 `~ n·u ≲ 1e-13` ⇒ "
    "冻结 1e-9（约 4 个数量级余量）",
    "⚠ **本键不含 k 的符号与系数**：实测 k 为**负**、系数约 0.55–0.69，"
    "而正本闭式为**正**、`1/4`。符号与系数分歧已如实登记在用例 docstring 与证据读数里，"
    "**不**通过放宽容差掩盖。换 drop 的构造口径必须重新推导本键。",
)

P3_CHORD_DEFICIT_REL = Frozen(
    "chain.a.p3.chord_deficit_rel", 1.0e-5,
    "无量纲相对量；**含极叶**四角弦多边形面积的相对亏缺 `A_chord/A_cell − 1` 与正本极限值 "
    "`2√2/π − 1 = −9.968368384e-2` 的相对偏差。域：nside = 1024（收敛到 O(1/nside²)）",
    "正本条款：DRIZZLE.md §5.2(:287) 逐字「叶边界弦亏缺：叶边界是非测地线曲线，叶边界取四角弦"
    "表示时其绝对亏缺为 `0.1043885/nside²` sr（极限相对亏缺 `2√2/π − 1 = −9.968368384e-2`），"
    "**随 `nside` 增大而减小**，与像元角尺度的平方成反比；该常数专属于**含极叶**的四角弦表示」。"
    "本层的「四角弦」由**真边界求根器**给出（RING 像元 0 与 `12nside²−1`，即含极叶），"
    "参照量 `A_cell` 取 `astropy_healpix` 的 `pixel_area`（第三方精确值）。"
    "收敛律：实测偏差 `∝ 1/nside²`（nside 逐档加倍，偏差逐档 ÷4）⇒ nside = 1024 处 "
    "≈ 4.7e-6 ⇒ 冻结 1.0e-5（约 2 倍余量）",
    "⚠ **只对含极叶成立**：赤道叶的四角弦多边形面积**大于** `A_cell`，相对偏差 "
    "`= +0.4577·θ_j²`（同量级、**反号**）⇒ 本键对赤道叶必红。用例因此把两类叶**分名**记录，"
    "只用含极叶判本键。",
)

P3_CHORD_DEFICIT_ABS_REL = Frozen(
    "chain.a.p3.chord_deficit_abs_rel", 1.0e-5,
    "无量纲相对量；绝对亏缺 `|A_cell − A_chord|·nside²` 与正本系数 `(π − 2√2)/3 = 0.10438851` "
    "的相对偏差。域同上（nside = 1024）",
    "正本条款同 `chain.a.p3.chord_deficit_rel`（DRIZZLE.md §5.2:287 逐字 `0.1043885/nside²` sr）。"
    "本键把正本给的小数 `0.1043885` 换成**逐位等价的解析式** `(π − 2√2)/3` "
    "（= 0.1043885096…），避免从正本的四舍五入字面量反推；两者相对差 `< 1e-7`",
    "与 `chord_deficit_rel` 同域、同收敛律。两条并列是冗余保护：正本同时给了相对与绝对两个读数。",
)

P3_CONTROL_POINT_EXACT = Frozen(
    "chain.a.p3.control_point_exact", EXACT,
    "无量纲；控制点搬运后 `值 / 稳定标识 / 拟合质量标志 / 测光状态标志` 与输入逐位相等",
    "正本条款：DRIZZLE.md §3.8(:224-226) 逐字三条规则——「**不做面积加权**：控制点的值原样带到"
    "球面对应位置，不进入 drop 交叠加权，也不参与任何通量守恒的求和」、「**不重新编号**：星点标识"
    "在全链保持不变，落盘即最终身份」、「**状态随值同行**：拟合质量与测光状态与数值同处一条记录」。"
    "被比较的是「搬运前后同一字段」的相等性（复制语义），属 TEST.md §4 第一档",
    "⚠ 这是**复制型**判据：它的牙来自负例（面积加权注入 / 重编号注入会让它立刻红），"
    "正例本身恒绿。因此必须与「恰落一个 tile」的落格判据（同用例）一起成立才有意义。",
)

P3_CONTROL_POINT_TILE_EXACT = Frozen(
    "chain.a.p3.control_point_tile_exact", EXACT,
    "整数；每个控制点落入的 tile 数必须**精确**为 1（`ang2pix_NESTED` 点包含判定）",
    "正本条款 + 第三方独立实现：DRIZZLE.md §3.8(:220) 逐字「落格规则是点包含判定：在 tile 阶上用 "
    "`ang2pix_NESTED` 求控制点所属的 tile，**一个控制点恰落在一个 tile 中**」。"
    "本层的 tile 序号取 `astropy_healpix.lonlat_to_healpix(..., order='NESTED')`"
    "（第三方独立实现，TEST.md §13 白名单），被测的落格判定用测试侧自实现的 `ang2pix_NESTED` "
    "等价式逐字重写一遍；两者不一致即判红 ⇒ 参照量取自被测对象之外",
    "精确档。tile 数是计数，属 TEST.md §4 第一档。⚠ 位置必须**不在 tile 边界上**："
    "HEALPix 边界点在浮点上有归属歧义（primer「Finite precision」一节逐字说明 "
    "`ang2pix` 在边界 ~1e-15 rad 内可能落到相邻像元）⇒ 夹具位置一律取固定的、"
    "与 tile 边界距离 ≫ 1e-12 rad 的字面坐标。",
)


# ---------------------------------------------------------------------------
# §5 冻结表与登记
# ---------------------------------------------------------------------------

CHAIN_A_TABLE: Dict[str, Frozen] = {
    f.key: f for f in (
        # P1
        P1_TUKEY_C, P1_MAD_KAPPA, P1_IRLS_TOL,
        P1_INTEGRATED_FLUX_REL, P1_INTEGRATED_FLUX_QUAD_REL,
        P1_SHIFT_SCALE_REL, P1_SHIFT_SIGMA_REL, P1_SIGMA_SENSITIVITY_REL,
        P1_ROBUST_SHIFT_DEX, P1_OUTLIER_WEIGHT_MAX, P1_INLIER_WEIGHT_MIN,
        P1_ZERO_POINT_ITERATIONS, P1_MIN_ITERATIONS_CONTROL, P1_NO_CUT_SHIFT_DEX,
        P1_Q_CONST_INVARIANCE_REL, P1_Q_SIGMA_RISE_MIN, P1_Q_PERM_CORR_Q95,
        P1_MATCHED_Q_SIGMA, P1_FSYN_ZERO_EXACT, P1_NO_DATA_MIN_REFS,
        # P2
        P2_SKY_ASYMPTOTE_REL, P2_SKY_MONOTONIC_MARGIN, P2_CONTROL_MEDIAN_VAR_REL,
        P2_INFORMATION_REL, P2_WRONG_NORM_MIN, P2_M5_ROUNDTRIP_REL,
        P2_M5_WRONG_DOMAIN_MIN, P2_VAR_DOUBLE_COUNT_MIN,
        P2_SKY_LIMITED_DRIFT_REL, P2_MIXED_ARM_MIN,
        # P3
        P3_CHART_JACOBIAN_REL, P3_CHART_AREA_REL, P3_NON_EQUAL_AREA_MIN,
        P3_AREA_RATIO_DELTA_VACUUM, P3_AREA_RATIO_SHAPE_REL, P3_CHORD_DEFICIT_REL, P3_CHORD_DEFICIT_ABS_REL,
        P3_CONTROL_POINT_EXACT, P3_CONTROL_POINT_TILE_EXACT,
    )
}

#: 判据键 → 骨架表键的**前缀共享**检查（防止两表对同一个物理量给出互相打架的门限）。
#: 本链**不覆盖**骨架表的任何键；冲突只登记不自动解。
CHAIN_A_KEY_PREFIX = "chain.a."


def get(key: str) -> Frozen:
    """按 key 取本链的冻结容差。**未知 key 显式抛错**（与骨架表同语义）。

    只查本链；骨架表的键仍应从 `eng.tests.synthetic.tolerances.get` 取。
    """
    try:
        return CHAIN_A_TABLE[key]
    except KeyError:
        raise KeyError(
            f"链 A 未冻结的容差 key={key!r}；先在 eng/tests/synthetic/_tol_chain_a.py "
            f"登记值、适用量级域与来源（骨架表的键请用 eng.tests.synthetic.tolerances.get）"
        ) from None


def value_of(key: str) -> Any:
    """取冻结值的便捷入口。"""
    return get(key).value


#: **判定为恒真 / 恒不成立、因而故意不写成用例**的判据逐条登记（TEST.md §2
#: 「恒真的比较没有证据资格」）。每条写明：判据、为什么恒真/恒假、替代落法。
REGISTERED_TAUTOLOGIES: Dict[str, str] = {
    "p1.sigma_residual_bitwise_invariant":
        "PHOTOMETRY.md §7(:228) 与 §2a.4(:104) 逐字写「`sigma_residual` 不变 / 逐位不变」。"
        "本层实测：**正确实现也不逐位不变** —— `MAD` 在偶数个样本时取中央两元素的均值，"
        "平移 `(a+c+b+c)/2` 与 `(a+b)/2 + c` 差 1 ulp（f64 u = 1.11e-16）。"
        "⇒ 写成逐位档的用例会**对正确实现恒红**（恒假门）。落法：改落 f64 非归约档"
        "（`chain.a.p1.shift_sigma_rel` = 1e-12），并把「真值无效应 ⇒ 归零」的判据交给"
        "`chain.a.p1.matched_q_sigma`（配置 Q 与真实 Q 匹配 ⇒ σ ≡ 0，精确档）。",

    "p1.location_shift_by_multiplying_r":
        "⚠ **本层在写用例时实测踩到并否决的写法**：把「`F_instr` 全体同乘 `k`」实现成"
        "「`r` 全体同乘 `k`」。二者**不是同一个变换**：`r = log10(F_instr/F_syn)`，"
        "同乘 `F_instr` 给出 `r → r + log10 k`（**平移**），而 `r → k·r` 是**缩放**。"
        "实测 `irls(k·r)` 给出的 `location = k·location`，与 `log10 k` 无关。"
        "⇒ 本层把该写法判为**错误口径**，不写成用例；正确落法见 `chain.a.p1.shift_scale_rel`。",

    "p2.sum_snr_squared_equals_F0sq_over_var":
        "NOISE_SNR.md §3.4(:353-359) 逐字：`Σ_k SNR_k² = F0²·Σ_k W_k = F0²/Var(F_hat)` 是"
        "**定义式恒等式**，正文逐字「对任何可达输入恒成立，**结构上不可能给出判决**」；"
        "§3.4(:359) 逐字指定替代判据「承载证据的判据是**点源信息量**：`Var(F_hat) = 1/sum_k W_k` "
        "与实测通量散度对拍，参照量取自权重之外，因而有鉴别力」。"
        "⇒ 本层**不写**同源恒等式的对拍；只写 `chain.a.p2.information_rel`。"
        "（同源登记另见 UNIFIED_SCIENCE_MODEL.md:103 与 weight_chain.h:424-438。）",

    "p2.sigma_F_from_F_ref_over_snr":
        "NOISE_SNR.md §3.3(:278) 明说 `σ_F^{sys,k} = F_ref,k/SNR_k` 是**把被检验公式取逆**；"
        "用它当参照量只回到同一式子（本册 :435-438 逐字点名该形态）。"
        "⇒ 本层 P2-a 的第二参照量取**定种子蒙特卡洛实测的通量散度**，不用取逆式。",

    "p3.l2_delta_from_backsolved_area":
        "DRIZZLE.md §3.7(:214) 与 §5.1(:269) 逐字：`A_pixel,j` 由 `A_drop,j/pixfrac²` 反推时"
        "L2 的 δ **恒为 0**、代数真空、无证据资格。正本同时给出前提「`A_pixel,j` 必须由"
        "**未收缩四角独立实测**」。⇒ 本层把它写成**同用例内的恒真对照臂**"
        "（`chain.a.p3.area_ratio_delta_vacuum`，精确档）而不是独立正例，"
        "并与「未收缩四角」那一臂并列断言，两臂读数相同即判红。",

    "p3.sum_type_conservation_as_only_gate":
        "DRIZZLE.md §5.1(:280) 逐字「求和型的守恒门只证明总量守恒，对『总量不变但逐叶错注入』"
        "的缺陷**没有判别力**」（实测求和型精确为 0、逐叶 O(0.3–0.9)，分离 ≥ 14.88 个数量级）。"
        "⇒ 本链的 P3 三条真空判据（chart Jacobian / L2 面积比 / 弦亏缺）全部是**逐叶**量；"
        "通量守恒求和门不写入本层（已在 `eng/tests/unit/test_drizzle_conservation.py` 覆盖）。",

    "p3.chart_jacobian_from_literal_pi_over_3":
        "实验侧 `实验/healpix-polar/route2/.../p3lib.py:82-84` 实测把 Jacobian 写成 "
        "`np.full_like(u, np.pi/3)`，判据零判别力（`实验/TAUTOLOGY_REGISTER.md:337` 登记）。"
        "⇒ 本层的 P3-a **必须真算 Jacobian**：中心差分 `∂(z,φ)/∂(ρ,ν)` 由闭式位置"
        "(4)(5)(8)(9) 当场求导，参照量取自 `astropy_healpix` 的精确 `pixel_area`。"
        "本文件里**不**冻结任何 Jacobian 的实测值，只冻结相对偏差门限。",
}

#: 与骨架表占位值的**关系**登记（为什么本链覆盖、占位值为何不足）。
CHAIN_A_SUPERSEDES_PLACEHOLDERS: Dict[str, str] = {
    "synth.p1.integrated_flux_rel":
        "骨架占位 1.0e-3（量级冻结，注里写「由写用例的子代理填实来源」）。"
        "本链把它拆成两条**实**冻结：`chain.a.p1.integrated_flux_rel` = 1e-12（闭式可解析的"
        "常数谱档，只受浮点限制）与 `chain.a.p1.integrated_flux_quad_rel` = 1e-9"
        "（曲线谱档，步长/插值误差是主项）。1e-3 对常数谱档**过宽 9 个数量级**："
        "它会把「求积被换成矩形法/漏掉 λ 因子」这类真缺陷放过（漏 λ 因子的偏差是 O(50%)），"
        "也会把门限装饰化。",
    "synth.p2.cross_frame_snr_spread_rel":
        "骨架占位 1.0e-1（跨帧散布的量级）。本链**未**覆盖它，只在 "
        "`chain.a.p2.mixed_arm_min` 里引用它作为「单档可比」的上界。"
        "该占位的实来源仍待补（本链未覆盖跨帧散布本身的判据）。",
    "synth.p3.flux_conservation_rel":
        "骨架占位 1.0e-6。本链**未**覆盖（通量守恒求和门按 DRIZZLE.md §5.1(:280) 无判别力，"
        "且已在 `eng/tests/unit/test_drizzle_conservation.py` 覆盖）。本链 P3 的三条真空判据"
        "另立三组实冻结。",
}


if __name__ == "__main__":  # pragma: no cover - 人工查阅入口
    print("=" * 78)
    print("ACSD 合成全链层 · 链 A（P1/P2/P3）容差冻结表")
    print("=" * 78)
    for _k, _f in CHAIN_A_TABLE.items():
        print(f"{_k:44s} = {_f.value!r}")
        print(f"    域: {_f.scale_domain}")
        print(f"    源: {_f.source}")
        if _f.note:
            print(f"    注: {_f.note}")
        print()
    print("-" * 78)
    print(f"登记为恒真/恒不成立、故意不写成用例的判据：{len(REGISTERED_TAUTOLOGIES)} 条")
    for _k, _v in REGISTERED_TAUTOLOGIES.items():
        print(f"  · {_k}")
    print(f"与骨架表占位值的关系登记：{len(CHAIN_A_SUPERSEDES_PLACEHOLDERS)} 条")
    for _k, _v in CHAIN_A_SUPERSEDES_PLACEHOLDERS.items():
        print(f"  · {_k}")
    # 自检：本链不得覆盖骨架表的任何键
    from . import tolerances as _skel
    clash = sorted(set(CHAIN_A_TABLE) & set(_skel.FROZEN_TABLE))
    print()
    if clash:
        print(f"❌ 与骨架表键冲突：{clash}")
    else:
        print("✅ 本链与骨架表无键冲突（只增不覆盖）")
    sys.stdout.flush()