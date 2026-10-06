"""合成全链层容差冻结表（**唯一源**）。

**冻结发生在写任何用例之前**（`docs/engineering/testing/TEST.md` §3 逐字「浮点断言用容差
加容差来源说明；容差在写用例前冻结，取值见第 4 节」）。本文件由骨架单独占位落盘，
写用例的子代理只能**取用**这里的键，不得在用例里现编容差（`get()` 对未冻结 key 抛错）。

同构说明：本文件与 `eng/tests/unit/tolerances.py` **同构**——同一个 `Frozen` 载体
（`key / value / scale_domain / source / note`）、同一张 `FROZEN_TABLE`、同一个
`get()` 的 fail-closed 语义、同一套 §1 通用档。两处**不共享**同一份表对象：合成层
的门限有合成层自己的来源（统计口径、创新点占位），跨层复制会让「冻结发生在写用例前」
这条纪律的时点不可追。

## 0 档位来源（`docs/engineering/testing/TEST.md` §4，逐字）

| 量类 | 取值 |
|---|---|
| 元数据、掩膜、计数、索引、端口、选择结果 | 精确一致 |
| 双精度非归约 | `rtol = 1e-12`，`atol = 1e-13 × scale` |
| 单精度产品非归约 | `rtol = 5e-6`，`atol = 1e-6 × scale` |
| 归约 | `γ_n = n·u/(1−n·u)`，门限 `C·γ_n·Σ|terms| + atol`，`C ≤ 4` 事前冻结 |

`u` 取同一 dtype 的 unit roundoff（`TEST.md` §4.1）：f64 = `2⁻⁵³` = 1.1102230246251565e-16，
f32 = `2⁻²⁴` = 5.9604644775390625e-08。**混用 dtype 的 `u` 把门限放宽约 2²⁹ 倍，属判据失效。**

本层 §1 的七个字面常量与 `eng/tests/unit/tolerances.py` §1 **逐字同值**，`ulp()` /
`reduction_tolerance()` 两个函数体也逐字同构。这不是巧合而是被 `run_synthetic.py` 依赖的
不变量：本层的断言走 `eng.tests.unit.harness.close()`，该函数的可满足性前检
（`atol ≥ 1 ulp(scale)`）内部只用到 §1 的 `ulp()`。两处 `ulp()` 同值 ⇒ 前检在两层
给出同一结论。若哪天有人改了本层 `U_F64`，前检会与本层 `tolerances` 脱钩。

## 1 这里是「冻结」而不是「现算」

容差值在本文件里是**字面常量**：不从产品输出反推、不从被测模型跑出来的残差反推、
不从本层 `_kit.py` 的输出反推。三类来源互不混用，每条在 `source` 里写明是哪一类：

| 来源类 | 含义 | 本文件的写法 |
|---|---|---|
| **正本条款** | 仓内正本逐字给出的阈值或实测读数 | `source` 引 `文件 §节` 与逐字片段 |
| **科学推导** | 由解析式 + 冻结输入算得的闭式值 | `source` 写推导式本身，推导里每个输入都是字面常量 |
| **一手文献** | 可解析的外部论文/标准 | `source` 写书目或 DOI/arXiv 标识 |

## 2 统计口径（`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md` §2 的合成全链特有口径）

合成全链跑的是**定种子蒙特卡洛**（`TEST.md` §3「固定种子；同一输入重复运行逐字节同结果」）。
重复次数有限 ⇒ 每个统计量本身有散布，所以门限不能写成 `0`，必须写成
**(置信区间半宽, 重复次数 R, 统计量)** 三元组。

本层统一采用**分位数法**：把 R 次重复的统计量取出来，用分位数（非参数）给出 95%
区间，半宽 = `(q_{0.975} − q_{0.025}) / 2`。分位数法是 bootstrap 的百分位法所对应的
那一族估计量（`Efron & Tibshirani 1993, An Introduction to the Bootstrap`, Chapman & Hall/CRC,
1993，第 13 节）；在 R 足够大时它与正态近似的半宽 `z·σ/√R` 相差一个高阶项，
本层把 `z_{0.975}` 冻结为 `1.959963984540054` 并**由分位数法实现**（用例里用
`numpy.quantile`，不在本文件里预乘 `z`）。

⚠ 冻结的不是「跑出来的读数」，而是**推导**：`half_width = z·σ/√R`，其中 `σ` 由解析式
在冻结输入上算出、`R` 是写死的重复次数。推导里不出现任何程序输出。

σ ∝ 1/√N 的口径来自重复抽样本身：R 次独立重复的均值标准差是 `σ/√R`（`TEST.md` §4
归约档 `γ_n` 的同一前向误差模型，`u` 换成抽样标准差）。

## 3 创新点门限**占位**的纪律（AGENTS.md §10 五个创新点）

`05_INDEPENDENT_TEST_SUITE.md` §2 逐字「合成全链覆盖五个创新点的关键科学不变量」。
本文件对 P1–P5 **各留至少一个门限占位**，占位遵守三条：

1. **不得写成连正确实现都过不了的死数**：占位取「保守但可达到」的宽值；
2. **不得写成必然恒绿的 `0.0`**：`0.0` 只留给确实是构造级恒等、且**不涉浮点误差**的
   判据（见 `SYNTH_SKY_SHOT_NOISE_ONLY`），并在 `note` 里写明为什么；
3. **`source` 写「量级冻结」，不写假出处**：占位的真实来源由写用例的子代理在写实后回填，
   本文件已把「冻结的是量级与统计口径」这句话写进每条 `note`。

## 4 参考文献（`source` 里的一手文献类出处）

[1] B. Efron and R. J. Tibshirani. *An Introduction to the Bootstrap*. Chapman & Hall/CRC, 1993.
    ISBN 978-0-412-04263-9.（分位数法 = bootstrap 百分位法）
[2] N. J. Higham. *Accuracy and Stability of Numerical Algorithms*, 2nd ed. SIAM, 2002.
    ISBN 978-0-89871-521-0.（`γ_n` 前向误差模型；与 `TEST.md` [1] 同一书目）
[3] IEEE. IEEE Std 754-2019, IEEE Standard for Floating-Point Arithmetic. IEEE, 2019.
[4] J. R. Janesick. *Scientific Charge-Coupled Devices*. SPIE Press, 2001. PM83, Ch. 2.
    ISBN 978-0-8194-0448-3.（CCD 噪声模型：源/天光/暗流泊松 + 读出高斯 + 量化）
[4] G. Newberry. *Pixel Statistics: An Astrophysicist's Cookbook*. PASP 103, 122 (1991).
    https://doi.org/10.1086/132000 （泊松诊断交叉）
"""

from __future__ import annotations

import math
import sys

# ---------------------------------------------------------------------------
# §1 通用档（来源：docs/engineering/testing/TEST.md §4，与 unit 层逐字同值）
# ---------------------------------------------------------------------------

#: `TEST.md` §4「双精度非归约」档
F64_RTOL = 1e-12
#: `TEST.md` §4「双精度非归约」档，atol = 1e-13 × scale
F64_ATOL_PER_SCALE = 1e-13

#: `TEST.md` §4「单精度产品非归约」档
F32_RTOL = 5e-6
#: `F32_ATOL_PER_SCALE = 1e-6`（`TEST.md` §4「单精度产品非归约」档）
F32_ATOL_PER_SCALE = 1e-6

#: `TEST.md` §4.1，IEEE 754 binary64 unit roundoff = 2⁻⁵³
U_F64 = 2.0 ** -53
#: `TEST.md` §4.1，IEEE 754 binary32 unit roundoff = 2⁻²⁴
U_F32 = 2.0 ** -24

#: `TEST.md` §4「`C ≤ 4` 事前冻结」。合成层的归约是「把整窗读数求成一个标量统计量」，
#: 项数 = 窗内像元数（≤ 1e4 量级），没有需要收紧放大预算的结构，故取约束上界。
REDUCTION_C = 4

#: `TEST.md` §4「元数据、掩膜、计数、索引、端口、选择结果」= 精确一致
EXACT = 0.0


def ulp(scale: float, u: float = U_F64) -> float:
    """`scale` 处的一个 ulp 上界 = `scale · u`（`TEST.md` §4.3 可满足性下限用）。

    `atol` 小于本值时该绝对容差**不可满足**（连下一个可表示数都跨不过去），
    调用点必须改用同值的相对形式（§4.3「只放宽不收紧」）。
    """
    return abs(scale) * u


def reduction_tolerance(n_terms: int, sum_abs_terms: float,
                        u: float = U_F64, c: int = REDUCTION_C,
                        atol: float = 0.0) -> float:
    """`TEST.md` §4「归约」档门限：`C·γ_n·Σ|terms| + atol`。

    `γ_n = n·u/(1−n·u)` 是 n 次运算的相对误差增长上界（Higham 2002 §4.2，本文件参考文献 [2]）。
    """
    if n_terms < 0:
        raise ValueError("n_terms 必须 ≥ 0")
    gamma_n = (n_terms * u) / (1.0 - n_terms * u)
    return c * gamma_n * abs(sum_abs_terms) + atol


#: 95% 双侧区间用的标准正态分位数。**正本条款**：`TEST.md` §4 把容差分档冻结为事前
#: 取值，本条把「分位数法在 R 足够大时收敛到正态近似」这一件事所用的 `z` 冻结成字面量，
#: 使推导式 `z·σ/√R` 可当场复算。
Z95 = 1.959963984540054


def ci_half_width(sigma: float, n_replicates: int, z: float = Z95) -> float:
    """分位数法 95% 区间半宽的**推导式**：`z·σ/√R`。

    ⚠ 这不是测量：本文件冻结的是**推导式与它的输入**（`z`、`σ`、`R` 全是字面常量），
    用例里必须用 `numpy.quantile` 的分位数法实算区间半宽，然后与本函数对照。
    直接调用本函数当期望值，等于跳过了分位数法这一步——那是另一条代码路径。
    """
    if n_replicates <= 0:
        raise ValueError("n_replicates 必须 ≥ 1")
    return z * sigma / math.sqrt(n_replicates)


# ---------------------------------------------------------------------------
# §2 冻结载体（与 eng/tests/unit/tolerances.py 同构）
# ---------------------------------------------------------------------------


class Frozen:
    """一条冻结容差：值 + 适用量级域 + 来源。"""

    __slots__ = ("key", "value", "scale_domain", "source", "note")

    def __init__(self, key: str, value, scale_domain: str, source: str,
                 note: str = "") -> None:
        self.key = key
        self.value = value
        self.scale_domain = scale_domain
        self.source = source
        self.note = note

    def __repr__(self) -> str:  # pragma: no cover - 诊断输出
        return f"<Frozen {self.key}={self.value!r}>"


# ---------------------------------------------------------------------------
# §3 统计口径冻结（分位数法；σ ∝ 1/√N）
# ---------------------------------------------------------------------------

SYNTH_MC_METHOD = Frozen(
    "synth.mc.method", "quantile_percentile_95",
    "无量纲；口径标签，不是数值",
    "正本条款：04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md §2「真值完全已知的构造数据」"
    "+ TEST.md §3「固定种子；同一输入重复运行逐字节同结果」；"
    "口径 = 分位数法（非参数 bootstrap 百分位法，Efron & Tibshirani 1993 第 13 节）",
    "分位数法 ≠ 正态近似：本层用 `numpy.quantile` 在 R 个重复上取 q_{0.025}/q_{0.975}，"
    "半宽 = (q975 − q025)/2；正态近似只用来**推导**门限的量级（Z95·σ/√R），"
    "不在实现里乘 Z95。用例不得用 `mean ± Z95·σ` 代替分位数法。",
)

#: 本层统一的重复次数 R。**正本条款**：`TEST.md` §3 要求固定种子可复跑；R 是冻结的
#: 设计参数（写用例时照抄，不许每条用例各写一个 R——那样门限之间不可比）。
#: 本层统一的重复次数 R。**正本条款**：`TEST.md` §3 要求固定种子可复跑；R 是冻结的
#: 设计参数（写用例时照抄，不许每条用例各写一个 R——那样门限之间不可比）。
SYNTH_MC_REPLICATES = Frozen(
    "synth.mc.replicates", 200,
    "无量纲；每个统计量的独立重复次数 R",
    "科学推导：门限随 R 按 1/√R 收缩（R=200 ⇒ 1/√R = 7.07e-2）。取 200 是"
    "「单条蒙特卡洛判据在秒级跑完」与「半宽进入个位数百分点档」之间的折中；"
    "提高 R 只能让门限变紧，不改变判据的口径。",
    "⚠ R 是**冻结的设计参数**，不是实测算出来的。写用例时若换 R，必须同时回填对应门限。",
)

#: 帧级绝对信噪比估计的相对偏差 —— 95% 分位数区间半宽。
#: σ 的推导：散粒噪声主导、源稀疏域内 SNR² 服从 Poisson 计数，则
#: `σ(SNR)/SNR = (1/2)·σ(N)/N = 1/(2·SNR)`；代入 SNR = 10 得 σ = 5.0e-2；
#: 半宽 = `Z95·5.0e-2/√200 = 6.9295e-3`，**冻结取 7.0e-3（向上取整，只放宽不收紧）**。
SYNTH_MC_ABS_SNR = Frozen(
    "synth.mc.abs_snr_rel_ci95", 7.0e-3,
    "无量纲相对量；被比较量 = |ŝnr/snr_true − 1|，域 1e-4–1e-1；SNR 档 10（域 1–1000）",
    "科学推导（σ 部分）+ 正本条款（SNR 逐像素模型）："
    "σ = 1/(2·SNR) 来自 SNR² 的泊松计数方差（NOISE_SNR.md §2.2「源光子散粒 … 方差 F·P/g」）；"
    "分位数法口径见 synth.mc.method",
    "统计量 = 帧级绝对信噪比估计的相对偏差在 R=200 次定种子重复上的分布。"
    "⚠ 本层**不**在这里冻结 SNR 的定义式——定义式由被测口径给出，"
    "本条只冻结「该统计量在 R 次重复下的散布」。",
)

#: 背景扣除后孔径通量的相对残差 —— 95% 分位数区间半宽。
#: σ 的推导（冻结输入：源积分 `F = 1e4 e⁻`、孔径像元数 `N = 100`、
#: 每像元天光 `B = 250 e⁻`、读噪 `RN = 5 e⁻`）：
#: `Var(F̂) = F + N·B + N·RN² = 1e4 + 2.5e4 + 2.5e3 = 3.75e4 e⁻²`
#: ⇒ `σ = √3.75e4 / 1e4 = 1.9364917e-2`；半宽 = `Z95·1.9364917e-2/√200 = 2.6838e-3`，
#: **冻结取 2.7e-3（向上取整）**。
SYNTH_MC_APERTURE_FLUX = Frozen(
    "synth.mc.aperture_flux_rel_ci95", 2.7e-3,
    "无量纲相对量；被比较量 = |F̂/F − 1|，源积分域 1e2–1e7 e⁻；本冻结对应 F=1e4 e⁻、"
    "N=100 像元、B=250 e⁻/px、RN=5 e⁻",
    "科学推导：Var = F + N·B + N·RN² 是背景扣除孔径积分的泊松 + 读出噪声解析方差"
    "（NOISE_SNR.md §2.2 随机方差项表：源光子散粒 / 天光光子散粒 / 读出噪声三项的逐像素求和）；"
    "分位数法口径见 synth.mc.method",
    "冻结的四个输入是**夹具参数**，写用例时照抄；换输入必须回填本门限。",
)

#: 实测方差 / 解析方差 之比 —— 95% 分位数区间半宽。
#: σ 的推导：单自由度（两点差分）样本方差的相对标准差是 `√(2/(n−1)) = √2`；
#: 半宽 = `Z95·1.4142136/√200 = 0.1959964`，**冻结取 2.0e-1（向上取整）**。
SYNTH_MC_VAR_RATIO = Frozen(
    "synth.mc.var_ratio_ci95", 2.0e-1,
    "无量纲比值；被比较量 = Var(实测)/Var(解析)，真值 1.0，域 0.2–5；单自由度估计",
    "科学推导：n 点样本方差的相对标准差 √(2/(n−1))（标准抽样分布，"
    "与 TEST.md [2] 同族的前向误差论证）；分位数法口径见 synth.mc.method",
    "这条专门用来判「散粒噪声项的量级对不对」。⚠ 单自由度下散布很大（±20%）是**正确**的："
    "若某条实现给出远小于 2.0e-1 的散布，那不是它更准，而是它把方差算成了非随机的量。",
)

#: 稀疏控制点重建稠密场的**重复间散布** —— 95% 分位数区间半宽。
#: σ 的推导：K 个独立控制点的注入估计，其相对标准差 `1/√K`，冻结 `K = 64` ⇒ σ = 0.125；
#: 半宽 = `Z95·0.125/√200 = 1.73238e-2`，**冻结取 1.8e-2（向上取整）**。
SYNTH_MC_DENSE_FIELD = Frozen(
    "synth.mc.dense_field_ci95", 1.8e-2,
    "无量纲相对量；被比较量 = 重建值在重复间的相对散布，域 1e-3–1e-1；K = 64 个控制点",
    "科学推导：K 个独立注入的相对标准差 = 1/√K（独立项标准差的平方可加，"
    "TEST.md §4 归约档的前向误差模型同形）；分位数法口径见 synth.mc.method",
    "统计量是**散布**不是偏差：重建算子的偏置由 `synth.p4.dense_snr_rel` 单独判，"
    "两者不合并，避免一条判据同时测两件事而无法归因。",
)

# ---------------------------------------------------------------------------
# §4 合成层的构造级恒等（`0.0` 的合法使用点，逐条说明为什么不涉浮点误差）
# ---------------------------------------------------------------------------

#: 天光只经散粒噪声进入信噪比 —— 结构判据，取精确档。
#: `note` 必须回答「为什么 0.0 在这里合法」：被比较的对象是**公式的分解结构**
#: （信噪比分子由哪些项构成），比较的是「哪些项出现在分子里」这个集合，
#: 不是两个浮点数之差。集合相等是离散判定，没有舍入误差可积累。
SYNTH_SKY_SHOT_NOISE_ONLY = Frozen(
    "synth.sky.shot_noise_only", EXACT,
    "无量纲；信噪比分子的项集合（源项在、天光项不在），集合基数 ≤ 16",
    "正本条款：04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md §2.2 逐字「天光对信噪比的影响"
    "通过散粒噪声体现」；上游正本 docs/science/noise_snr/NOISE_SNR.md §4 逐字"
    "「**分子只有源**：天光只经散粒噪声进入分母。把天光电平或观测电平计入分子，"
    "就得到一个随天光上升的量，与信噪比的定义相反」",
    "为什么这里是合法的 0.0：被比较的是**项集合**，不是浮点量。⇒ 不属 "
    "TEST.md §4 的浮点三档，也就不存在「落在容差内通过」的中间态"
    "（同 §4.4 的非有限值语义）。若判据退化成比较两个浮点数，本条立刻失效。",
)

#: Poisson 采样出的电子数是**整数**，计数类比较一律精确一致。
SYNTH_POISSON_COUNT_EXACT = Frozen(
    "synth.poisson.count_exact", EXACT,
    "整数；逐像元电子数、暗计数、饱和像元计数、量化后数字量",
    "正本条款：TEST.md §4 第一档「元数据、掩膜、计数、索引、端口、选择结果 = 精确一致」；"
    "电子域 Poisson 采样的定义见 04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md §2.1",
    "⚠ 计数精确一致**不含** `N_e` 与 `λ` 的关系：那是抽样误差，由 "
    "`synth.mc.*` 的统计口径判，不在这里判。",
)

#: 量化往返的最坏误差 = 半个 LSB（对称均匀量化器）。
SYNTH_QUANT_HALF_LSB = Frozen(
    "synth.quant.half_lsb", lambda step: 0.5 * step,
    "与 LSB 同单位；LSB = 满量程 / 2^n_bits",
    "科学推导：对称均匀量化器的解析往返界 |x − Q(Q⁻¹(x))| ≤ Δ/2，"
    "最坏情形恰在两个量化级中点（与 eng/tests/unit/tolerances.py 的同名条同源）；"
    "量化器定义见 04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md §2.1「电子经增益、饱和与量化转 ADU」",
    "8-bit 档 Δ = 满量程/256，16-bit 档 Δ = 满量程/65536。LSB 与满量程都是字面常量，"
    "写在 `_kit.py` 的常量区并标出处。",
)

# ---------------------------------------------------------------------------
# §5 五个创新点的门限占位（AGENTS.md §10；05 §2「合成全链覆盖五个创新点的关键科学不变量」）
# ---------------------------------------------------------------------------

#: P1 通量积分拟合 —— 线性标度把图像校准到统一测光星等坐标系后的标度相对残差。
SYNTH_P1_LINEAR_SCALE = Frozen(
    "synth.p1.linear_scale_rel", 5.0e-2,
    "无量纲相对量；|ĉ − c|/c，ĉ = 拟合得到的线性标度；域 1e-4–1",
    "量级冻结（**占位**）：科学推导面 = XP 光谱 × CCD QE × 滤镜透过率的正向积分 vs "
    "注入真值的线性标度；上游正本 docs/science/PHOTOMETRY.md §「绝对刻度的适用域（C5）」"
    "明确该量只在窄带定量",
    "该值将由写用例的子代理填实来源；这里先冻结量级与统计口径。5% 是「窄带绝对刻度残差"
    "在个位数百分点档」这一量级，**保守但非恒真**：一个把通量拟合做对、能落到几个百分点"
    "的实现必然通过，一个拿错标度或漏掉色项的实现必然超界。",
)

#: P1 配套 —— 正向合成的期望测光对解析积分的相对残差（数值积分面，不是标度面）。
SYNTH_P1_INTEGRATED_FLUX = Frozen(
    "synth.p1.integrated_flux_rel", 1.0e-3,
    "无量纲相对量；|∫XP·QE·T dλ − F_true|/F_true，域 1e-4–1e0",
    "量级冻结（**占位**）：科学推导面 = 数值积分的收敛性（步长加密 2 倍时残差按阶数下降）；"
    "上游正本 AGENTS.md §10 P1「Gaia XP 光谱 × CCD QE × 滤镜透过率积分」",
    "该值将由写用例的子代理填实来源；这里先冻结量级与统计口径。1e-3 是「求积权重误差 + "
    "插值误差」的量级，明显宽于 f64 归约档（1e-12），因为被比较量跨越光谱采样网格。",
)

#: P2 跨帧绝对信噪比 —— 同一控制点在统一坐标系上跨帧的相对散布。
SYNTH_P2_CROSS_FRAME_SPREAD = Frozen(
    "synth.p2.cross_frame_snr_spread_rel", 1.0e-1,
    "无量纲相对量；std/mean，跨 n_frames 帧、同一控制点、同一参考星等档；域 1e-3–1",
    "量级冻结（**占位**）：科学推导面 = 逐帧散粒噪声给出的 1/√(N_px·F) 相对散布，"
    "在不同透明度/天光/视宁度下取值范围约 3%–30%；上游正本 "
    "docs/science/noise_snr/NOISE_SNR.md §「跨帧可比性的硬约束」逐字「跨帧比对必须限定在"
    "同一参考星等档 m_ref 内，天光受限档与源主导档不可混比」",
    "该值将由写用例的子代理填实来源；这里先冻结量级与统计口径。10% 是「跨帧绝对信噪比"
    "在统一坐标系上可比」的量级：它比单帧统计误差宽（后者见 synth.mc.abs_snr_rel_ci95），"
    "因此**判的是跨帧可比性本身**，不是单帧精度。",
)

#: P3 守恒映射算子 —— 平面到 HEALPix 的通量绝对守恒残差（合成全链面）。
SYNTH_P3_FLUX_CONSERVATION = Frozen(
    "synth.p3.flux_conservation_rel", 1.0e-6,
    "无量纲相对量；|Σ_out F_out − Σ_in F_in| / Σ_in|F_in|，域 1e-9–1e-2",
    "量级冻结（**占位**）：科学推导面 = 逐叶完备性 + 求和闭合的闭式残差；"
    "上游正本 docs/science/drizzle/DRIZZLE.md §5.1「通量守恒门」",
    "该值将由写用例的子代理填实来源；这里先冻结量级与统计口径。⚠ 本层取 1e-6，"
    "比单元层已冻结的 `drizzle.l1_completeness = 6.6e-12` **宽六个数量级**，"
    "因为合成全链多了采样噪声、量化与不完整覆盖；两者不可互换引用。"
    "⚠ 同 §5.1 逐字提醒：求和型守恒门只作辅助，对「总量不变但逐叶错注入」无判别力。",
)

#: P4 重建稠密信噪比 —— 稀疏控制点重建出的稠密场相对真值的偏差。
SYNTH_P4_DENSE_SNR = Frozen(
    "synth.p4.dense_snr_rel", 2.5e-1,
    "无量纲相对量；|ŝnr_dense − snr_true|/snr_true，域 1e-2–1",
    "量级冻结（**占位**）：科学推导面 = 控制点间距 / PSF 尺度之比决定的插值误差；"
    "上游正本 AGENTS.md §10 P4「据噪声信号模型把稀疏控制点重建为稠密信噪比场，现场按像素求值」",
    "该值将由写用例的子代理填实来源；这里先冻结量级与统计口径。25% 是「控制点足够密时"
    "重建可达」的量级，**保守但非恒真**：控制点密度不足的实现必然超界。"
    "⚠ 本条判**偏差**；重复间的**散布**由 `synth.mc.dense_field_ci95` 单独判，不合并。",
)

#: P5 加性天光去除 —— 叠加后跨接缝的相对残差 / 梯度连续性。
SYNTH_P5_SEAM_RESIDUAL = Frozen(
    "synth.p5.seam_residual_rel", 5.0e-2,
    "无量纲相对量；接缝两侧控制点之差的相对量，域 1e-4–1",
    "量级冻结（**占位**）：科学推导面 = 「多退少补」的残差上界与背景估计噪声同阶；"
    "上游正本 AGENTS.md §10 P5「信噪比加权去除加性天光、保留公共背景，叠加天然无接缝」",
    "该值将由写用例的子代理填实来源；这里先冻结量级与统计口径。"
    "⚠ 本条必须配「真值无效应 ⇒ 归零」的对照臂（TEST.md §2「恒真的比较没有证据资格」）。",
)

#: P5 配套 —— 去除后的天光残差相对量（判「加性天光确实被去掉」而不是「接缝好看」）。
SYNTH_P5_SKY_RESIDUAL = Frozen(
    "synth.p5.sky_residual_rel", 1.0e-2,
    "无量纲相对量；|S_residual − S_true|/S_true，S 为天光面；域 1e-4–1",
    "量级冻结（**占位**）：科学推导面 = 信噪比加权估计的背景权重残差 ~ 1/SNR_cp，"
    "控制点 SNR 档 10–100 ⇒ 1%–10%；上游正本 AGENTS.md §10 P5",
    "该值将由写用例的子代理填实来源；这里先冻结量级与统计口径。1% 是「控制点 SNR ≈ 100」"
    "这一档的量级；SNR 更低时该门限必须按 1/SNR_cp 重算，不得直接套用。",
)

# ---------------------------------------------------------------------------
# §6 夹具冻结（是**夹具**，不是容差；同 unit 层 DRIZZLE_FIXTURE_PIXFRAC 的先例）
# ---------------------------------------------------------------------------

SYNTH_FIXTURE_M16_BAND = Frozen(
    "synth.fixture.m16_band", "f502n",
    "无量纲；本仓 testdata/HST_M16 的三个窄带之一（f502n / f657n / f673n）",
    "正本条款：04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md §2.1 逐字「以 HST 真实数据"
    "（本仓 M16，带 PHOTFLAM 定标）作为纯信号模板」；"
    "docs/science/PHOTOMETRY.md §「绝对刻度的适用域（C5）」点名窄带为 F657N/F673N/F502N",
    "三带都在仓内；本层默认取 f502n 是因为它是三带里天光最暗的一带，"
    "模板窗口的信噪比最高、最适合当**纯信号模板**。换带只改夹具，不改任何门限。",
)

#: 模板子窗口（`[y0, y1, x0, x1]`，左闭右开，DRZ 全幅 8000×8400）。
#: ⚠ 这四个数是**夹具选择**，读数是「该窗口内 DRZ 值无 NaN、中位值落在 F502N 窄带的合理电平域」，
#: 不是从任何程序输出反推的容差。
SYNTH_FIXTURE_M16_WINDOW = Frozen(
    "synth.fixture.m16_window", [4000, 4064, 4000, 4064],
    "像元；DRZ 全幅 (8000, 8400)，窗口 64×64 = 4096 像元",
    "正本条款 + 夹具选择：窗口位置取全幅中心附近的一个 64×64 子窗口，"
    "只做**按窗口读取**（268917120 字节 = 8000×8400×4 的 float32 全幅，整读进内存是 "
    "269 MB；本层禁止整读）。位置是夹具，不是容差，不参与任何通过/失败判定。",
    "换窗口位置只改夹具。但窗口若落在无结构区（纯天光），模板就失去「纯信号模板」的"
    "意义——换窗口时必须同时报该窗口的统计读数。",
)

SYNTH_FIXTURE_M16_WINDOW_PX = Frozen(
    "synth.fixture.m16_window_px", 64,
    "像元；窗口边长",
    "夹具选择：64 使单窗 Poisson 采样（4096 像元 × R 次重复）在秒级完成；"
    "窗口太大时 R 次重复会超秒级，R 的冻结值 200 就不可执行。",
)

SYNTH_INSTRUMENT_SOURCE_NOTE = """\
仪器参数的来源类（三类分开写，不混用）：

1. **从 FITS 头读的**（本层 `_kit.load_m16_template` 每次打开现读，不缓存成字面量）：
   - `PHOTFLAM` —— 零点通量密度（Jy），逐字见 04 §2.1「带 PHOTFLAM 定标」。
     **实测**：三个窄带都存在（f502n 5.2676009e-18 / f657n 2.2290223e-18 /
     f673n 2.23971949999999e-18），同节 `PHOTZPT = -21.1`。
   - `EXPTIME` —— 曝光秒数（f502n 16000 / f657n 9600 / f673n 14400）。
   - `BUNIT` —— **实测 `ELECTRONS/S`**，即 DRZ 像元值是**电子率**不是 ADU。
     `_kit` 据此把窗口值乘 `EXPTIME` 还原成「每像元电子数」；`BUNIT` 不含
     `ELECTRON` 时**显式抛错**，不静默按别的单位读。
   - `CCDGAIN` —— **实测 1.5**，三带同值，单位电子/ADU。
   - **`RDNOISE` 实测不存在**：三个头的键集里没有 `RDNOISE`、也没有任何
     `*NOISE*` 键。因此读噪**不是**从 FITS 头读的，而是 `_kit` 里一条**外部一手文献**
     常量（见 `HST_READ_NOISE_E` 的 docstring）。这一条必须如实登记，
     不得写成「从 FITS 头读到的读噪」。
2. **科学推导**：见本文件各 `Frozen.source` 里标「科学推导」的条。
3. **一手文献**：见本文件 §4 参考文献与 `_kit` 常量区逐条标注。
"""


# ---------------------------------------------------------------------------
# §7 冻结表自检
# ---------------------------------------------------------------------------

FROZEN_TABLE = {f.key: f for f in (
    SYNTH_MC_METHOD, SYNTH_MC_REPLICATES, SYNTH_MC_ABS_SNR, SYNTH_MC_APERTURE_FLUX,
    SYNTH_MC_VAR_RATIO, SYNTH_MC_DENSE_FIELD,
    SYNTH_SKY_SHOT_NOISE_ONLY, SYNTH_POISSON_COUNT_EXACT, SYNTH_QUANT_HALF_LSB,
    SYNTH_P1_LINEAR_SCALE, SYNTH_P1_INTEGRATED_FLUX, SYNTH_P2_CROSS_FRAME_SPREAD,
    SYNTH_P3_FLUX_CONSERVATION, SYNTH_P4_DENSE_SNR, SYNTH_P5_SEAM_RESIDUAL,
    SYNTH_P5_SKY_RESIDUAL,
    SYNTH_FIXTURE_M16_BAND, SYNTH_FIXTURE_M16_WINDOW, SYNTH_FIXTURE_M16_WINDOW_PX,
)}

#: 五个创新点各至少一个门限占位。本文件被自己守卫：少一条就不是「覆盖五个创新点」。
P_PLACEHOLDER_KEYS = {
    "P1": ("synth.p1.linear_scale_rel", "synth.p1.integrated_flux_rel"),
    "P2": ("synth.p2.cross_frame_snr_spread_rel",),
    "P3": ("synth.p3.flux_conservation_rel",),
    "P4": ("synth.p4.dense_snr_rel",),
    "P5": ("synth.p5.seam_residual_rel", "synth.p5.sky_residual_rel"),
}


def get(key: str) -> Frozen:
    """按 key 取冻结容差。未知 key 直接抛错——不许在用例里现编容差。"""
    try:
        return FROZEN_TABLE[key]
    except KeyError:
        raise KeyError(
            f"未冻结的容差 key={key!r}；先在 eng/tests/synthetic/tolerances.py "
            f"登记值、适用量级域与来源"
        ) from None


if __name__ == "__main__":  # pragma: no cover - 人工查阅入口
    print("=" * 78)
    print("ACSD 合成全链层容差冻结表（唯一源）")
    print("=" * 78)
    print("通用档（来源 TEST.md §4，与 unit 层逐字同值）:")
    print(f"  f64 非归约: rtol={F64_RTOL}  atol={F64_ATOL_PER_SCALE}·scale")
    print(f"  f32 非归约: rtol={F32_RTOL}  atol={F32_ATOL_PER_SCALE}·scale")
    print(f"  归约      : C={REDUCTION_C}, γ_n = n·u/(1−n·u)")
    print(f"  精确一致  : EXACT={EXACT}")
    print(f"  u_f64={U_F64!r}   u_f32={U_F32!r}")
    print(f"  z95 (分位数法推导用)={Z95!r}")
    print()
    for _k, _f in FROZEN_TABLE.items():
        print(f"{_k:44s} = {_f.value!r}")
        print(f"    域: {_f.scale_domain}")
        print(f"    源: {_f.source}")
        if _f.note:
            print(f"    注: {_f.note}")
        print()
    missing = [f"{p}:{k}" for p, ks in P_PLACEHOLDER_KEYS.items() for k in ks
               if k not in FROZEN_TABLE]
    if missing:
        print(f"❌ 创新点门限占位缺失: {missing}")
    else:
        print("✅ 五个创新点各有门限占位：")
        for p, ks in P_PLACEHOLDER_KEYS.items():
            print(f"   {p}: {', '.join(ks)}")
    print()
    print(SYNTH_INSTRUMENT_SOURCE_NOTE)
    sys.stdout.flush()