"""单元层容差冻结表（唯一源）。

**冻结发生在写任何用例之前**（`docs/engineering/testing/TEST.md` §3「浮点断言用容差
加容差来源说明；容差在写用例前冻结，取值见第 4 节」）。每一条容差都必须能回答两问：

1. **来源**：它出自哪一份正本的哪一档（通用档 / 领域判据 / 标定读数）；
2. **适用量级域 `scale`**：`TEST.md` §4.3「任何绝对容差只有在被比较量的量级 `scale`
   满足 `atol ≥ 1 ulp(scale)` 时才可判。模块冻结绝对容差时必须同时声明适用量级域」。

## 0 档位来源（`docs/engineering/testing/TEST.md` §4，逐字）

| 量类 | 取值 |
|---|---|
| 元数据、掩膜、计数、索引、端口、选择结果 | 精确一致 |
| 双精度非归约 | `rtol = 1e-12`，`atol = 1e-13 × scale` |
| 单精度产品非归约 | `rtol = 5e-6`，`atol = 1e-6 × scale` |
| 归约 | `γ_n = n·u/(1−n·u)`，门限 `C·γ_n·Σ|terms| + atol`，`C ≤ 4` 事前冻结 |

`u` 取同一 dtype 的 unit roundoff（`TEST.md` §4.1）：f64 = `2⁻⁵³` = 1.1102230246251565e-16，
f32 = `2⁻²⁴` = 5.9604644775390625e-08。**混用 dtype 的 `u` 把门限放宽约 2²⁹ 倍，属判据失效。**

## 1 为什么这里是「冻结」而不是「现算」

容差值在本文件里是**字面常量**，不从产品输出反推、不从被测模型跑出来的残差反推。
领域判据（S7/S11/S12/S13）另有正本来源的数值，逐条标在 `SOURCE` 里；
通用档（f64/f32/归约/精确）直接引 `TEST.md` §4 的表，两条来源互不混用。

## 2 `C` 的冻结值

`TEST.md` §4 把 `C` 定义为「前向误差放大预算」，约束 `C ≤ 4`，并明说「`C ≤ 4` 的取值
由前向误差放大预算决定，属项目冻结值而非文献值」。本层**冻结 C = 4**（取约束上界，
即最宽门限），理由：单元层的被测量是核函数的小规模归约（项数 ≤ 64），
没有需要收紧放大预算的结构；取上界保证不会因冻结了一个偏紧的 C 而把合格实现判红。
凡本层用 `C` 的地方都在调用点注明项数 `n` 与 `Σ|terms|`，门限可当场复算。
"""

from __future__ import annotations

import math
import sys

# ---------------------------------------------------------------------------
# §1 通用档（来源：docs/engineering/testing/TEST.md §4）
# ---------------------------------------------------------------------------

#: `TEST.md` §4「双精度非归约」档
F64_RTOL = 1e-12
#: `TEST.md` §4「双精度非归约」档，atol = 1e-13 × scale
F64_ATOL_PER_SCALE = 1e-13

#: `TEST.md` §4「单精度产品非归约」档
F32_RTOL = 5e-6
F32_ATOL_PER_SCALE = 1e-6

#: `TEST.md` §4.1，IEEE 754 binary64 unit roundoff = 2⁻⁵³
U_F64 = 2.0 ** -53
#: `TEST.md` §4.1，IEEE 754 binary32 unit roundoff = 2⁻²⁴
U_F32 = 2.0 ** -24

#: `TEST.md` §4「`C ≤ 4` 事前冻结」；本层取上界，见本文件 §2
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

    `γ_n = n·u/(1−n·u)` 是 n 次运算的相对误差增长上界（Higham 2002 §4.2，
    本文件参考文献同 `TEST.md` [1]）。
    """
    if n_terms < 0:
        raise ValueError("n_terms 必须 ≥ 0")
    gamma_n = (n_terms * u) / (1.0 - n_terms * u)
    return c * gamma_n * abs(sum_abs_terms) + atol


# ---------------------------------------------------------------------------
# §2 领域冻结容差（来源：各自正本，逐条标注）
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


# --- Drizzle（S7 / S8 / S9）-------------------------------------------------

#: 同一叶内 drop 分割的权重和：`Σ_p w_jp = 1`。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §5.1「权重和门」逐字「**同一叶内**逐位为 `0`」。
#: 该行是几何构造级的恒等（drop 面积被其覆盖的输出像元精确分完），不涉浮点归约误差，
#: 故取精确档而非 `TEST.md` §4 的浮点档。
DRIZZLE_WEIGHT_SUM_SAME_LEAF = Frozen(
    "drizzle.weight_sum_same_leaf", 0.0,
    "无量纲；Σ_p w_jp，w_jp = a_jp/A_drop,j",
    "docs/science/drizzle/DRIZZLE.md §5.1「权重和门」：同一叶内逐位为 0",
    "适用域 = 同一叶内的 drop 分割。跨叶 / 跨面不在此域（见跨叶/跨面两条）。",
)

#: 逐像元完备性 `max|Σ_p a_jp/A_drop,j − 1|`。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §5.1「通量守恒门」逐字读数 `6.6e-12`。
DRIZZLE_L1_COMPLETENESS = Frozen(
    "drizzle.l1_completeness", 6.6e-12,
    "无量纲；max over j of |Σ_p a_jp/A_drop,j − 1|",
    "docs/science/drizzle/DRIZZLE.md §5.1「通量守恒门」实测读数 6.6e-12",
    "正本给的是**实测读数**不是可满足性上限；本层取同一数值作门限并同时报实测值，"
    "使门限与读数之比可被审查。",
)

#: 逐叶全域求和闭合 `Σ_p Σ_j x_j w_jp − Σ_j x_j`（相对）。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §5.1「通量守恒门」逐字读数 `8.2e-15`。
DRIZZLE_FLUX_SUM_REL = Frozen(
    "drizzle.flux_sum_rel", 8.2e-15,
    "无量纲；(Σ_p Σ_j x_j w_jp − Σ_j x_j)/Σ_j|x_j|，Σ_j x_j 标度 1e0–1e8 ADU",
    "docs/science/drizzle/DRIZZLE.md §5.1「通量守恒门」实测读数 8.2e-15",
    "⚠ 正本同节明写：求和型守恒门**只作辅助**，对「总量不变但逐叶错注入」的缺陷无判别力。"
    "本层据此不把它当主判据，只作辅助登记。",
)

#: 常量面亮度门 `|S_p/B₀ − 1|`，累加器级。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §5.1 逐字阈值 `< 1e-3`。
DRIZZLE_SURFACE_BRIGHTNESS_REL = Frozen(
    "drizzle.surface_brightness_rel", 1e-3,
    "无量纲；S_p/B₀，B₀ = 1e0–1e10 ADU/sr",
    "docs/science/drizzle/DRIZZLE.md §5.1「常量面亮度门（累加器级）」阈值 < 1e-3",
)

#: 产品级 signal 覆盖面积量化容差 `0.5/q`，全覆盖叶 `q = 255` 时 `1.96e-3`。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §5.1 逐字推导 `q = lround(255·clamp(D_p/A_cell,0,1))`。
DRIZZLE_PRODUCT_SIGNAL_REL_FULL = Frozen(
    "drizzle.product_signal_rel_full_coverage", 0.5 / 255,
    "无量纲；|signal/S_p − 1|，全覆盖叶 q = 255",
    "docs/science/drizzle/DRIZZLE.md §5.1「常量面亮度门（产品级）」：≤ 0.5/q，q=255 ⇒ 1.96e-3",
    "该行正本同时**禁用**任何以 1e-3 卡产品级 signal 的实现（即使全覆盖也吃满 0.196%）。",
)

#: 面积比级残差 δ 的闭式 `(1 − pixfrac²)·θ_j²/4`。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §3.7「几何闭合 L2」与 §5.2「面积比残差 δ」。
DRIZZLE_AREA_RATIO_DELTA = Frozen(
    "drizzle.area_ratio_delta", lambda pixfrac, theta_rad: (1.0 - pixfrac ** 2) * theta_rad ** 2 / 4.0,
    "δ 无量纲；θ_j = 像元角尺度 rad，生产域 1e-5–1e-2 rad",
    "docs/science/drizzle/DRIZZLE.md §3.7 闭式 δ = (1 − pixfrac²)·θ_j²/4 + O(θ_j⁴)",
    "⚠ 该级**只在 A_pixel,j 由未收缩四角独立实测时有判别力**；由 A_drop,j/pixfrac² 反推时"
    "是代数真空（δ 恒 0）。见 FZ-COND-FLUX-CONSERV 与 §3.7 的反例。",
)

#: NaN 路径的覆盖计数语义：`support` 与被剔除样本计数必须可区分且精确一致。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §5.2「非有限样本」。
DRIZZLE_NAN_SUPPORT_EXACT = Frozen(
    "drizzle.nan_support", EXACT,
    "整数；support（合格样本数）与 masked_count（被剔除样本数）",
    "docs/science/drizzle/DRIZZLE.md §5.2：每个输出像元必须暴露被剔除样本的计数，"
    "计数为 0 与「字段缺失」必须可区分",
    "取 `TEST.md` §4「计数、索引、掩膜 = 精确一致」档。",
)

# --- pixfrac 夹具冻结 -------------------------------------------------------

#: S8 负例夹具用的 pixfrac。正本数值 0.8。
#: 理由见报告 §容差冻结表 与本文件下方长注。
DRIZZLE_FIXTURE_PIXFRAC = Frozen(
    "drizzle.fixture_pixfrac", 0.8,
    "无量纲；(0, 1]",
    "docs/science/drizzle/DRIZZLE.md §4 参数表「数值默认 0.8」；"
    "eng/packaging/config/defaults.json#drizzle.pixfrac = 0.8（authority_status = owner_adjudicated）",
)

PIXFRAC_FREEZE_NOTE = """\
pixfrac 夹具冻结理由（不依赖任何有争议的生产默认）：

- **必须 < 1**：`docs/science/drizzle/DRIZZLE.md` §5.1「通量守恒门」的红侧逐字是
  「把核权重写回 `a_jp/A_pixel,j`（求和退化为 `pixfrac²·Σ_j x_j`）」。`pixfrac = 1` 时
  `pixfrac² = 1`，该退化与正确实现**同值**⇒ 负例在默认配置下不可触发、恒绿。
- **取 0.8 而不是 1.0**：`eng/packaging/config/defaults.json#drizzle.pixfrac` 当前值 0.8，
  `authority_status = owner_adjudicated`；`docs/science/drizzle/DRIZZLE.md` §4 参数表同值。
- **本层不断言生产默认**：仓内 `pixfrac` 数值默认存在**四处互相冲突的表述**
  （defaults.json = 0.8「owner_adjudicated」/ DRIZZLE.md §4 = 0.8 / CONFIG.md §2 表 = 1、
  同页说明列 = 0.8 / phase_config_normalize.schema.json = 「数值默认未冻结…
  defaults.json 为 pending_authority」，而 defaults.json 实为 owner_adjudicated）。
  单元层因此把 pixfrac 当**参数**扫掠，只在夹具上冻结具体值，不断言生产默认值。
"""

# --- HEALPix（S11）----------------------------------------------------------

#: 球面角 ↔ NESTED 像元往返角距上限。
#: 来源 `docs/science/algorithms/HEALPIX_MAPPING.md`「Postconditions」逐字
#: 「round-trip 误差 ≤ 1e-12 度（FP64）」。
HEALPIX_ROUNDTRIP_DEG = Frozen(
    "healpix.roundtrip_deg", 1e-12,
    "度；往返球面角距，dec ∈ [−90, 90]",
    "docs/science/algorithms/HEALPIX_MAPPING.md「Postconditions」：≤ 1e-12 度（FP64）",
    "⚠ 该正本同时指出既有「7×7 网格 <1e-6 px」是**自证门**（自网格 1.8e-12 px vs "
    "离网格 3.10 px）。本层因此在**独立密集域**上取样，不在被测方自己的采样网格上取样。",
)

#: nside 的合法 order 上限。
#: 来源 `docs/science/algorithms/HEALPIX_MAPPING.md` 输入表逐字 `2^k，k ≤ 29`。
HEALPIX_MAX_ORDER = Frozen(
    "healpix.max_order", 29,
    "无量纲；nside = 2^k 的 k",
    "docs/science/algorithms/HEALPIX_MAPPING.md 输入表 + Preconditions「order ≤ 29」",
)

#: tile 层位移与掩码。
#: 来源 `docs/science/algorithms/HEALPIX_MAPPING.md`「Invariants」逐字
#: 「tile_shift=9、mask=(1<<18)-1」。
HEALPIX_TILE_SHIFT = Frozen("healpix.tile_shift", 9, "bit", "HEALPIX_MAPPING.md「Invariants」")
HEALPIX_TILE_MASK = Frozen(
    "healpix.tile_mask", (1 << 18) - 1, "无量纲；2^18 − 1",
    "HEALPIX_MAPPING.md「Invariants」",
)

#: 叶面积 `A = 4π/(12·nside²)`。
#: 来源 `docs/science/drizzle/DRIZZLE.md` §4 参数表逐字 `A_cell = π/(3·nside²)`，
#: 与 HEALPix 构造 `4π/(12·nside²)` 同值。
HEALPIX_CELL_AREA_SR = Frozen(
    "healpix.cell_area_sr", lambda nside: math.pi / (3.0 * nside ** 2), "sr；单叶面积",
    "docs/science/drizzle/DRIZZLE.md §4「叶面积 A_cell = π/(3·nside²)」= 4π/(12·nside²)",
)

# --- 量化往返（S12 / S13）---------------------------------------------------

#: Gaia DR3SP 8-bit 量化残差统计量与门限。
#: 统计量选 median + p95 双统计量（不用 max：max 会被离群点绑架，见本文件下方注）。
GAIA_QUANT = Frozen(
    "gaia.quant_rel", {"median": 0.0021, "p95": 0.018},
    "无量纲相对残差；被量化量 1e0–1e6 量级（XP 系数 / BP-RP 通带采样）",
    "审核包-R2/T02 判据 S12 引 STANDARDS_REGISTRY.md:206「median 0.21% / p95 1.8%」；"
    "上游正本 docs/science/algorithms/GAIA_QUERY.md",
)

GAIA_STATISTIC_NOTE = """\
为什么用 median + p95 而不用 max：8-bit 量化残差的分布长尾由个别离群样本决定，
max 随样本集变化而漂移，不构成可复现的门限。正本给的就是 median/p95 两个统计量，
`TEST.md` §4 的 R4 精神（统计量必须显式、阈值与统计量同写）要求两者同写。
"""

#: XPSD 本地编码量化步长。
XPSD_QUANT = Frozen(
    "xpsd.quant_step", {"pos_uas_per_lsb": 2.0, "dra_uas_per_lsb": 10.0,
                        "mag_per_lsb": 0.001},
    "位置 µas/LSB；dra µas/LSB；星等 mag/LSB",
    "审核包-R2/T02 判据 S13 引 STANDARDS_REGISTRY.md:205；上游正本 docs/science/algorithms/GAIA_QUERY.md",
)

#: 量化往返误差上限 = 半个 LSB（对称量化器的最大往返误差）。
#: 来源：对称均匀量化器的解析结论 `|x − Q(Q⁻¹(x))| ≤ Δ/2`；
#: 见本文件参考文献 Rousseeuw & Croux 1993, JASA 88, 1273 之后的量化理论；
#: 与 `docs/science/algorithms/GATES_AND_TOLERANCES.md` 参考文献表同一书目族。
QUANT_ROUNDTRIP_HALF_LSB = Frozen(
    "quant.roundtrip_half_lsb", lambda step: 0.5 * step,
    "与 LSB 同单位",
    "对称均匀量化的解析往返界：最坏情形恰在两个量化级中点，误差 = Δ/2",
)

# --- 排异路由（S1 / S2 / S4）-----------------------------------------------

#: 路由返回值是**枚举整数**，属 `TEST.md` §4「索引、选择结果」= 精确一致档。
REJECTION_ROUTING_EXACT = Frozen(
    "rejection.routing", EXACT,
    "无量纲；方法枚举整数",
    "docs/engineering/testing/TEST.md §4「元数据、…、索引、端口、选择结果」= 精确一致",
)

#: 归一化枚举默认与耦合不变量同属精确档。
REJECTION_NORMALIZATION_EXACT = Frozen(
    "rejection.normalization", EXACT,
    "无量纲；归一化枚举整数",
    "docs/engineering/testing/TEST.md §4 精确一致档",
)

# --- 资源判据（P / E 类）---------------------------------------------------

#: 利用率是**无量纲比率**，域 [0,1]。
RESOURCE_UTIL_RATIO = Frozen(
    "resource.utilization", EXACT,
    "无量纲比率；域 [0, 1]",
    "eng/contracts/resource_gate_v1.json#compute 的 percent 字段换算；比率本身按精确档判",
)

# --- 冻结表自检 -------------------------------------------------------------

FROZEN_TABLE = {f.key: f for f in (
    DRIZZLE_WEIGHT_SUM_SAME_LEAF, DRIZZLE_L1_COMPLETENESS, DRIZZLE_FLUX_SUM_REL,
    DRIZZLE_SURFACE_BRIGHTNESS_REL, DRIZZLE_PRODUCT_SIGNAL_REL_FULL,
    DRIZZLE_AREA_RATIO_DELTA, DRIZZLE_NAN_SUPPORT_EXACT, DRIZZLE_FIXTURE_PIXFRAC,
    HEALPIX_ROUNDTRIP_DEG, HEALPIX_MAX_ORDER, HEALPIX_TILE_SHIFT, HEALPIX_TILE_MASK,
    HEALPIX_CELL_AREA_SR, GAIA_QUANT, XPSD_QUANT, QUANT_ROUNDTRIP_HALF_LSB,
    REJECTION_ROUTING_EXACT, REJECTION_NORMALIZATION_EXACT, RESOURCE_UTIL_RATIO,
)}


def get(key: str) -> Frozen:
    """按 key 取冻结容差。未知 key 直接抛错——不许在用例里现编容差。"""
    try:
        return FROZEN_TABLE[key]
    except KeyError:
        raise KeyError(
            f"未冻结的容差 key={key!r}；先在 tolerances.py 登记来源与适用量级域"
        ) from None


if __name__ == "__main__":  # pragma: no cover - 人工查阅入口
    for _k, _f in FROZEN_TABLE.items():
        print(f"{_k:42s} = {_f.value!r}\n    域: {_f.scale_domain}\n    源: {_f.source}")
    print(f"\n通用档: f64 rtol={F64_RTOL} atol={F64_ATOL_PER_SCALE}·scale; "
          f"f32 rtol={F32_RTOL} atol={F32_ATOL_PER_SCALE}·scale; C={REDUCTION_C}")
    print(f"u_f64={U_F64!r} u_f32={U_F32!r}")
    print(f"\n{PIXFRAC_FREEZE_NOTE}")
    print(f"{GAIA_STATISTIC_NOTE}")
    sys.stdout.flush()