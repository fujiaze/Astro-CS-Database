r"""量化往返判据：S12（Gaia DR3SP 8-bit 光谱）与 S13（XPSD 本地编码）。

## 参考实现放在本文件而不是 `wcs_ref.py`

`wcs_ref.py` 的立约是「按 `lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp`
逐行转写」，源文件唯一。本文件的源文件是
`lib/infrastructure/gaia_xpsd_client/src/gaia_client.c`，把 Gaia 量化口径塞进
`wcs_ref.py` 会**模糊溯源**（两份正本混在一个模块里），违反 AGENTS §3
「同一主题只有一份正本」。故参考实现就地写在下面，各自标注行号锚点。

## S12 / S13 的分工（本文件的核心纪律）

| | S12 | S13 |
|---|---|---|
| 量 | Gaia DR3SP XP 光谱通量（W·m⁻²·nm⁻¹） | XPSD 位置（µas）/ dra（µas）/ 星等（mag） |
| 量化器 | 每星线性 8-bit：`F = byte·fluxMul + fluxMin` | 固定步长整数栅格：2 / 10 µas·LSB⁻¹、0.001 mag·LSB⁻¹ |
| 判据形状 | **统计量**（median + p95 相对残差） | **逐点解析界**（往返误差 ≤ Δ/2，可取等号） |
| 分布依赖 | 是（相对残差的分布取决于被量化量的分布） | 否（对称均匀量化的最坏界与分布无关） |

## 期望值与阈值的分离（正本读数不得当期望值）

`tolerances.GAIA_QUANT` = `{median: 0.0021, p95: 0.018}`。这两个数**只作阈值**，
来源是 `docs/science/algorithms/GAIA_QUERY.md` §2.9 第 1 条逐字：

> 「星等量化 0.001 mag 与 DR3SP 光谱 8-bit 量化（残差 median 0.21%/p95 1.8%，
> memory.md）」

其上游 `run/FINAL-07-e2e/bisect/src/lib/infrastructure/gaia_xpsd_client/memory.md`
的逐字记录是「**官方解码形状残差** median 0.21%/p95 1.8%」——这是**真实 DR3SP
星表样本上的实测读数**，而**该样本总体（星的通量分布）未在仓内任何正本中登记**。

⇒ 因此本文件把两层期望值严格分开：

| 量 | 来源 | 用途 |
|---|---|---|
| `expected`（逐点解码值、逐点解析界、闭式统计量） | `GAIA_QUERY.md` §2.6 闭式 + 对称均匀量化的解析结论 | 判正误 |
| `threshold`（0.0021 / 0.018） | `GAIA_QUERY.md` §2.9.1 的实测读数 | 只判是否超界 |

零原点（`fluxMin = 0`）是相对残差的**最坏情形**（分母无下界），本文件固定用它，
不为了凑读数而调 `fluxMin`。

## 统计量为什么用 median + p95 而不是 max

`tolerances.GAIA_STATISTIC_NOTE` 逐字：「8-bit 量化残差的分布长尾由个别离群样本
决定，max 随样本集变化而漂移，不构成可复现的门限」。本文件零处使用 max 作为门限；
需要「最大值」语义的地方（S13 逐点界、S12 端点夹具）用的是**解析可判的界**，
不是样本统计量。
"""

from __future__ import annotations

import math

import numpy as np

from . import harness
from . import tolerances as tol


# ===========================================================================
# §1 XPSD 本地编码量化参考实现（S13）
# ===========================================================================
#
# 逐字转写锚点（`lib/infrastructure/gaia_xpsd_client/src/gaia_client.c`）：
#   :1952  double inv_scale = 1.0 / (3600.0 * 1000.0 * 500.0);
#   :1953  double inv_dra   = 1.0 / (3600.0 * 1000.0 * 100.0);
#   :1962  double magG = mag_raw * 0.001 - 1.5;
#   :1965-1966  x = node->x0 + dx*inv_scale;  y = node->y0 + dy*inv_scale;
#   :1978  s_ra += dra_raw * inv_dra;
# 科学正本：`docs/science/algorithms/GAIA_QUERY.md` §2.2（星等）、§2.3（位置 + 步长推导）。

#: 每度的微秒数。1 deg = 3600″ × 1e6 µas/″ = 3.6e9 µas（GAIA_QUERY.md §2.3 逐字同式）。
UAS_PER_DEG = 3600.0 * 1.0e6

#: 产品实码的度/LSB 分母（`gaia_client.c:1952-1953` 逐字）。
INV_SCALE_DEG_PER_LSB = 1.0 / (3600.0 * 1000.0 * 500.0)   # = 1/1.8e9
INV_DRA_DEG_PER_LSB = 1.0 / (3600.0 * 1000.0 * 100.0)     # = 1/3.6e8

#: 星等零点与步长（`gaia_client.c:1962` 逐字 `mag_raw * 0.001 - 1.5`）。
MAG_STEP = 0.001
MAG_ZERO = -1.5
#: 星等记录的 uint16 码域上界（`gaia_client.c:1961` 逐字 `memcpy(&mag_raw, p+20, 2)`）。
MAG_CODE_MAX = 65535


def derive_steps() -> dict[str, float]:
    """**独立推导**三个步长——不查产品常量，只从「度/LSB 分母 × 单位换算」算出。

    这是 S13 的 Oracle：GAIA_QUERY.md §2.3 逐字给出的推导
    「1 deg = 3.6e9 µas，故 1 LSB = 3.6e9/1.8e9 = **2 µas**」、
    「1/3.6e8 deg = 10 µas/LSB」。
    """
    return {
        "pos_uas_per_lsb": INV_SCALE_DEG_PER_LSB * UAS_PER_DEG,
        "dra_uas_per_lsb": INV_DRA_DEG_PER_LSB * UAS_PER_DEG,
        "mag_per_lsb": MAG_STEP,
    }


def quantize(value: float, step: float, *, mode: str = "nearest") -> int:
    """对称均匀量化器的编码侧（产品的逆，格式定义给出）。

    | mode | 定义 | 说明 |
    |---|---|---|
    | `nearest` | `floor(v/Δ + 0.5)` | 对称均匀量化的最近邻（正解） |
    | `trunc` | `trunc(v/Δ)` | 截断，负数不对称（负例注入） |
    | `half_down` | `ceil(v/Δ − 0.5)` | 取整方向取反：整级样本被推到下一格（负例注入） |
    """
    t = value / step
    if mode == "nearest":
        return math.floor(t + 0.5)
    if mode == "trunc":
        return math.trunc(t)
    if mode == "half_down":
        return math.ceil(t - 0.5)
    raise ValueError(f"未知取整模式 {mode!r}")


def dequantize(code: int, step: float, *, grid: str = "mid_tread") -> float:
    """重建。`grid='mid_rise'` 模拟「重建网格偏移半个 LSB」（负例注入）。"""
    return (code + (0.5 if grid == "mid_rise" else 0.0)) * step


def decode_mag(mag_raw: int) -> float:
    """`gaia_client.c:1962` 逐字：`magG = mag_raw * 0.001 - 1.5`。"""
    return mag_raw * 0.001 + MAG_ZERO


def decode_position_uas(node_uas: float, dx_raw: int, step: float) -> float:
    """`gaia_client.c:1965-1966` 的位置反投影（赤道带 Equirectangular 树）。

    `node_uas` 是节点原点 `x0` 的微秒值；`dx_raw` 是记录里的 uint32 LSB 计数。
    """
    return node_uas + dx_raw * step


def decode_dra_uas(s_ra_uas: float, dra_raw: int, step: float) -> float:
    """`gaia_client.c:1978` 的 dra 修正（记录偏移 +26 的 int16）。"""
    return s_ra_uas + dra_raw * step


# ===========================================================================
# §2 Gaia DR3SP 8-bit 光谱量化参考实现（S12）
# ===========================================================================
#
# 逐字转写锚点（`lib/infrastructure/gaia_xpsd_client/src/gaia_client.c`）：
#   :1990-1997
#       float flux_min = 0.0f, flux_mul = 0.0f;
#       if (xf->has_spectrum) {
#           /* PCL GaiaDatabaseFile::EncodedStarSPData:
#            *   32B EncodedStarData | float fluxMin | float fluxMul | uint8 flux[...] */
#           memcpy(&flux_min, p + 32, 4);
#           memcpy(&flux_mul, p + 36, 4);
#           spectrum = p + 40;
#       }
# 科学正本：`docs/science/algorithms/GAIA_QUERY.md` §2.6 逐字
#   「F(λ_j) = byte_j × flux_mul + flux_min  单位 W·m⁻²·nm⁻¹」。

#: 每条光谱的样本数（`GAIA_QUERY.md` §2.6 逐字 `uint8 flux[343]`）。
SPECTRUM_SAMPLES = 343

#: 码宽（8 bit ⇒ 码级 0..255）。`GAIA_QUERY.md` §2.6 逐字 `uint8`。
CODE_BITS = 8
CODE_MAX = (1 << CODE_BITS) - 1


def spectrum_encode(flux, flux_mul: float, flux_min: float = 0.0, *,
                    mode: str = "nearest", max_code: int = CODE_MAX,
                    clamp: bool = True):
    """8-bit 编码侧（`GAIA_QUERY.md` §2.6 闭式的逆）：`byte = ⌊(F−flux_min)/flux_mul+½⌋`。

    可表示域 = `[flux_min, flux_min + 255·flux_mul]`；域外样本必须被夹取。
    `clamp=False` 模拟漏掉端点夹取 ⇒ uint8 环绕（负例注入）。
    """
    t = (flux - flux_min) / flux_mul
    q = np.rint(t) if mode == "nearest" else np.floor(t)
    if clamp:
        q = np.clip(q, 0, max_code)
    return q.astype(np.int64)


def spectrum_decode(code, flux_mul: float, flux_min: float = 0.0):
    """`GAIA_QUERY.md` §2.6 闭式：`F(λ_j) = byte_j × flux_mul + flux_min`。"""
    return code * flux_mul + flux_min


def uint8_wrap(code):
    """C 侧把越界值写进 `uint8` 后的环绕（模 256），缺夹取时的实际后果。"""
    return (code.astype(np.int64) % (1 << CODE_BITS))


#: S12 夹具：零原点 = 相对残差的**最坏情形**（分母无下界）。
#: 4096 条 × 343 样本 = 1,404,928 个残差；分位数在该样本量下的渐近标准误
#: median ≈ 8.4e-4 相对、p95 ≈ 3.7e-4 绝对（见 `_order_statistic_sd`）。
SPECTRUM_SEED = 20260906
SPECTRUM_N_STARS = 4096
FLUX_MUL = 1.0e-12          # W·m⁻²·nm⁻¹，一个任意但固定的物理量级
FLUX_MIN_ZERO_ORIGIN = 0.0  # 最坏情形；正本未约束 fluxMin 取值


def _spectral_flux(n_stars: int = SPECTRUM_N_STARS) -> "np.ndarray":
    """确定子集：在整条 8-bit 码空间 `[0, 255·Δ]` 上均匀取样（固定种子）。"""
    rng = np.random.default_rng(SPECTRUM_SEED)
    return rng.random((n_stars, SPECTRUM_SAMPLES)) * (CODE_MAX * FLUX_MUL)


def _order_statistic_sd(q: float, n: int, density: float) -> float:
    """经验分位数的渐近标准误 `sqrt(q(1−q)/n)/f(x_q)`（Higham 2002 同族的统计口径）。

    本文件用它**冻结**「实测统计量 vs 闭式解析值」的容差，而不是随手写个数。
    """
    return math.sqrt(q * (1.0 - q) / n) / density


# ---------------------------------------------------------------------------
# 零原点 8-bit 码的**闭式**相对残差分布（不是实测值，是推导值）
# ---------------------------------------------------------------------------
#
# 记 `s = F/Δ`（码空间坐标，均匀取样时 `s ~ U[0, 255]`）与归一化舍入误差
# `u = round(s) − s ~ U[−1/2, 1/2]`，两者独立。相对残差 `r = |u| / s`。
# 于是对 `x ≥ 0`：
#   P(r ≤ x) = E_s[ min(2·x·s, 1) ]
#   · `x ≤ 1/510`  时上界不起作用：P = 255·x
#   · `x > 1/510` 时积分分段：  P = 255 + 0 − 1/(4x) 化简为 `1 − 1/(1020·x)`
# 由此：median = 1/510 ≈ 0.1961%、p95 = 1/51 ≈ 1.9608%。
#
# ⚠ 这两个闭式值**大于**正本实测读数 0.21% / 1.8%（p95 尤甚）：正本读数取自真实
# DR3SP 星表，`fluxMin > 0` 且通量分布非均匀，两者都压低相对残差。见 §「期望值与
# 阈值的分离」。

_S12_MEDIAN_CLOSED_FORM = 1.0 / 510.0
_S12_P95_CLOSED_FORM = 1.0 / 51.0
# 零原点残差 CDF 的密度（用于冻结分位数容差）：
#   median 处：dP/dx = 255
#   p95   处：dP/dx = 1/(1020·x²)，在 x = 1/51 处等于 255/100 = 2.55
_S12_MEDIAN_DENSITY = 255.0
_S12_P95_DENSITY = CODE_MAX / 100.0


def _s12_statistic(flux, *, flux_mul: float = FLUX_MUL, flux_min: float = 0.0,
                   mode: str = "nearest", max_code: int = CODE_MAX,
                   clamp: bool = True) -> tuple[float, float]:
    """返回 (median, p95) 相对残差。"""
    code = spectrum_encode(flux, flux_mul, flux_min, mode=mode,
                           max_code=max_code, clamp=clamp)
    decoded = spectrum_decode(code, flux_mul, flux_min)
    rel = np.abs(decoded - flux) / np.abs(flux)
    return float(np.median(rel)), float(np.percentile(rel, 95))


# ===========================================================================
# §3 S13 —— 逐点解析往返界
# ===========================================================================

_SRC_STEPS = ("docs/science/algorithms/GAIA_QUERY.md §2.3（位置步长闭式推导）"
              " + §2.2（星等 0.001 mag/LSB、零点 −1.5）")
#: S13 步长 Δ 的代码锚（`gaia_client.c:1952-1953` 的度/LSB 分母、`:1962` 的星等式）。
_SRC_STEPS_CODE = ("lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:1952-1953,1962"
                   "（inv_scale=1/(3600·1000·500)、inv_dra=1/(3600·1000·100)、"
                   "magG=mag_raw*0.001-1.5）")
#: S13 往返判据的完整来源。`AGENTS.md` §6 逐字「实质性证据的形式：……『我觉得』
#: 『惯例如此』不算证据」——只写「对称均匀量化」这一**解析结论**不构成期望值来源，
#: 必须同时锚到仓内正本与实码行号，判据才可被逐字核对（`test_criterion_meta.py`
#: 的 E5/E13 落法）。
_SRC_RT_BOUND = ("docs/science/algorithms/GAIA_QUERY.md §2.3 逐字位置步长闭式"
                 "（+ §2.2 星等 0.001 mag/LSB、零点 −1.5）；"
                 + _SRC_STEPS_CODE
                 + "；对称均匀量化在该 Δ 上的解析往返界 |x − Q(Q⁻¹(x))| ≤ Δ/2，"
                   "最坏情形恰在两个量化级中点（tolerances.QUANT_ROUNDTRIP_HALF_LSB）")
_SRC_S12_DECODE = ("docs/science/algorithms/GAIA_QUERY.md §2.6 逐字闭式 "
                   "F(λ_j) = byte_j × flux_mul + flux_min")
_SRC_S12_STAT = ("docs/science/algorithms/GAIA_QUERY.md §2.9 第 1 条逐字实测读数 "
                 "「残差 median 0.21%/p95 1.8%」")


@harness.test(
    "quantization.s13.step_derivation_from_source",
    intent="S13 的三个步长必须由**独立推导**（度/LSB 分母 × 单位换算）复现，"
           "而不是从产品常量抄来。GAIA_QUERY.md §2.3 逐字「1 deg = 3.6e9 µas，"
           "故 1 LSB = 3.6e9/1.8e9 = 2 µas」。",
    inputs="gaia_client.c:1952-1953 的度/LSB 分母 1.8e9 与 3.6e8；"
           "§2.2 的星等步长 0.001 mag/LSB",
    expected="独立推导出的 (pos, dra, mag) = (2.0 µas/LSB, 10.0 µas/LSB, "
             "0.001 mag/LSB)，与 tolerances.XPSD_QUANT 冻结值精确一致",
    source=_SRC_STEPS,
    criteria=("S13",),
)
def _case_s13_steps() -> None:
    frozen = tol.get("xpsd.quant_step").value
    derived = derive_steps()
    with harness.evidence() as ev:
        for key in ("pos_uas_per_lsb", "dra_uas_per_lsb", "mag_per_lsb"):
            # 步长是整数域记录格式常量（GAIA_QUERY.md §2.2 明写「两个常数都是
            # XPSD 记录格式常量」），判精确一致档。
            harness.exact(derived[key], frozen[key],
                          f"{key}: 独立推导值与冻结值不一致")
            ev.record(f"推导 {key}", derived[key], frozen[key],
                      unit="LSB^-1", note="1 deg = 3.6e9 µas 的闭式换算")
        # 交叉印证：1.8e9 LSB/deg ⇒ 1 LSB = 2 µas（GAIA_QUERY.md §2.3 逐字同式）
        lsb_per_deg = 1.0 / INV_SCALE_DEG_PER_LSB
        harness.exact(lsb_per_deg, 1.8e9, "位置码密度必须逐字为 1.8e9 LSB/deg")
        ev.record("位置码密度", lsb_per_deg, 1.8e9, unit="LSB/deg")
        lsb_per_deg_dra = 1.0 / INV_DRA_DEG_PER_LSB
        harness.exact(lsb_per_deg_dra, 3.6e8, "dra 码密度必须逐字为 3.6e8 LSB/deg")
        ev.record("dra 码密度", lsb_per_deg_dra, 3.6e8, unit="LSB/deg")


@harness.test(
    "quantization.s13.roundtrip_bound_position",
    intent="S13 位置维（µas）：对称均匀量化的往返误差解析上界 = Δ/2，"
           "且**恰在量化级中点取等号**；量化级上的样本误差必须逐位为 0。",
    inputs=f"pos 步长 Δ = {tol.get('xpsd.quant_step').value['pos_uas_per_lsb']} µas/LSB；"
           "k ∈ [−1000, 1000] 的整级样本与 (k+0.5) 中点样本",
    expected="整级样本误差 = 0；中点样本误差上界 = Δ/2 = 1.0 µas（µas 量纲，"
             "不与 mag 的门限混用）",
    source=_SRC_RT_BOUND + "；步长源 " + _SRC_STEPS,
    criteria=("S13",),
)
def _case_s13_position() -> None:
    step = tol.get("xpsd.quant_step").value["pos_uas_per_lsb"]
    half = tol.get("quant.roundtrip_half_lsb").value(step)
    worst_level = 0.0
    worst_mid = 0.0
    worst_all = 0.0
    with harness.evidence() as ev:
        for k in range(-1000, 1001):
            level = k * step
            mid = (k + 0.5) * step
            e_level = abs(dequantize(quantize(level, step), step) - level)
            e_mid = abs(dequantize(quantize(mid, step), step) - mid)
            worst_level = max(worst_level, e_level)
            worst_mid = max(worst_mid, e_mid)
            worst_all = max(worst_all, e_level, e_mid)
        harness.exact(worst_level, 0.0, "量化级上的往返误差必须逐位为 0")
        # µas 维的中点界：解析界恰取等号（落 f64 非归约档量化的判定）
        harness.close(worst_mid, half, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * abs(half),
                      what="pos 中点最坏往返误差", scale=abs(half))
        harness.less_equal(worst_all, half, "pos 全部样本的最坏往返误差")
        ev.record("pos 整级样本最坏误差", worst_level, 0.0, unit="uas")
        ev.record("pos 中点样本最坏误差", worst_mid, half, unit="uas",
                  note="解析界恰取等号")
        ev.record("pos 步长", step, unit="uas/LSB")
        # 量纲隔离：pos 的门限是 µas，不得被 mag 的门限替换后仍成立
        ev.record("mag 门限（不同量纲，不得用于 pos）",
                  tol.get("quant.roundtrip_half_lsb").value(MAG_STEP),
                  unit="mag")


@harness.test(
    "quantization.s13.roundtrip_bound_dra",
    intent="S13 dra 维（µas）：dra 是 int16 记录偏移 +26 的整数修正量，"
           "步长 10 µas/LSB，往返误差上界 = Δ/2 = 5 µas。**与 pos 维分开判**"
           "（两者都是 µas 但步长不同，门限不得互换）。",
    inputs="dra 步长 Δ = 10 µas/LSB；k ∈ [−32768, 32767]（int16 全域）的整级与中点样本",
    expected="整级样本误差 = 0；中点样本误差上界 = 5.0 µas；"
             "int16 全域上无饱和 ⇒ 无额外偏差",
    source=_SRC_RT_BOUND + "；步长源 " + _SRC_STEPS
             + "（gaia_client.c:1975-1978，dra_raw 为 int16）",
    criteria=("S13",),
)
def _case_s13_dra() -> None:
    step = tol.get("xpsd.quant_step").value["dra_uas_per_lsb"]
    half = tol.get("quant.roundtrip_half_lsb").value(step)
    worst_level = 0.0
    worst_mid = 0.0
    with harness.evidence() as ev:
        for k in (-32768, -32767, -1000, -1, 0, 1, 999, 1000, 32766, 32767):
            for frac in (0.0, 0.5):
                x = (k + frac) * step
                err = abs(dequantize(quantize(x, step), step) - x)
                if frac == 0.0:
                    worst_level = max(worst_level, err)
                else:
                    worst_mid = max(worst_mid, err)
        harness.exact(worst_level, 0.0, "dra 整级样本误差必须逐位为 0")
        harness.close(worst_mid, half, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * abs(half),
                      what="dra 中点最坏往返误差", scale=abs(half))
        ev.record("dra 整级样本最坏误差", worst_level, 0.0, unit="uas")
        ev.record("dra 中点样本最坏误差", worst_mid, half, unit="uas")
        ev.record("dra 步长", step, unit="uas/LSB")
        # int16 全域可达，不存在饱和 ⇒ 界在整个记录域上成立
        code_min = quantize(-32768 * step, step)
        code_max = quantize(32767 * step, step)
        harness.is_true(code_min >= -32768 and code_max <= 32767,
                        "dra 码必须在 int16 全域内（无饱和）")
        ev.record("dra 码域", (code_min, code_max), (-32768, 32767))


@harness.test(
    "quantization.s13.roundtrip_bound_magnitude",
    intent="S13 星等维（**mag**）：步长 0.001 mag/LSB、零点 −1.5，"
           "往返误差上界 = Δ/2 = 5e-4 mag。**mag 与 µas 是不同量纲，"
           "分开断言，不混进同一个容差**。",
    inputs="mag 步长 Δ = 0.001；k = 0..65535（uint16 全域）的整级与中点样本",
    expected="整级样本往返后 mag 逐位等于 k·0.001 − 1.5；"
             "中点样本误差上界 = 5e-4 mag",
    source=_SRC_RT_BOUND + "；解码闭式 " + _SRC_STEPS
             + "（gaia_client.c:1962 `mag_raw * 0.001 - 1.5`）",
    criteria=("S13",),
)
def _case_s13_magnitude() -> None:
    step = tol.get("xpsd.quant_step").value["mag_per_lsb"]
    half = tol.get("quant.roundtrip_half_lsb").value(step)
    worst_level = 0.0
    worst_mid = 0.0
    with harness.evidence() as ev:
        for k in range(0, MAG_CODE_MAX + 1):
            level = k * step
            code = quantize(level, step)
            worst_level = max(worst_level, abs(decode_mag(code) - (level + MAG_ZERO)))
            mid = (k + 0.5) * step
            code_mid = quantize(mid, step)
            worst_mid = max(worst_mid,
                            abs(decode_mag(code_mid) - (mid + MAG_ZERO)))
        harness.exact(worst_level, 0.0, "星等整级样本往返误差必须逐位为 0")
        # `decode_mag` 是 `raw·0.001 − 1.5` 的**带零点减法**，其可达精度下限由
        # 样本量级 `k·Δ − 1.5` 决定（TEST.md §4.3「atol ≥ 1 ulp(scale) 才可判」）。
        # 本域样本量级达 67 mag ⇒ ulp ≈ 7.4e-15，该下限高于 Δ/2 本身。
        mag_scale = MAG_CODE_MAX * step + abs(MAG_ZERO)
        harness.close(worst_mid, half, rtol=tol.F64_RTOL, atol=tol.ulp(mag_scale),
                      what="mag 中点最坏往返误差", scale=mag_scale)
        ev.record("mag 样本量级 scale", mag_scale, unit="mag",
                  note=f"atol 下限 = 1 ulp(scale) = {tol.ulp(mag_scale):.3e}")
        ev.record("mag 整级样本最坏误差", worst_level, 0.0, unit="mag")
        ev.record("mag 中点样本最坏误差", worst_mid, half, unit="mag",
                  note="解析界 Δ/2 恰取等号（差值即零点减法的浮点下限）")
        ev.record("mag 适用域", f"m ∈ [{MAG_ZERO}, {MAG_ZERO + MAG_CODE_MAX * step}]",
                  unit="mag", note="GAIA_QUERY.md §2.2 逐字")
        # 量纲隔离证据：pos 的门限 1.0 µas 与 mag 的 5e-4 mag 不可互换
        ev.record("pos 门限", tol.get("quant.roundtrip_half_lsb").value(
            tol.get("xpsd.quant_step").value["pos_uas_per_lsb"]), unit="uas",
            note="≠ mag 门限，不同量纲")


@harness.test(
    "quantization.s13.decode_closed_form",
    intent="XPSD 解码必须逐字等于 `GAIA_QUERY.md` §2.2/§2.3 的闭式"
           "（`mag_raw*0.001−1.5`、`x0 + dx·1/1.8e9`、`+ dra_raw·1/3.6e8`），"
           "且 dra 的符号（int16 可负）不得丢失。",
    inputs="星等 raw=0/1/32768/65535；dx raw=0/1/2^32−1；dra raw=−32768/−1/0/1/32767",
    expected="逐点与闭式一致（闭式由度/弧度常量独立算出，不调用产品符号）",
    source="docs/science/algorithms/GAIA_QUERY.md §2.2,§2.3（闭式）"
           " + gaia_client.c:1962,1965-1966,1978（实现锚点）",
    criteria=("S13",),
)
def _case_s13_decode() -> None:
    steps = derive_steps()
    node_uas = 12.345678e9        # 节点原点（µas），任意但固定
    with harness.evidence() as ev:
        for raw in (0, 1, 32768, 65535):
            expect = raw * 0.001 + MAG_ZERO
            harness.close(decode_mag(raw), expect, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * abs(expect),
                          what=f"mag raw={raw}", scale=abs(expect))
            ev.record(f"mag raw={raw}", decode_mag(raw), expect, unit="mag")
        for raw in (0, 1, 2 ** 32 - 1):
            got = decode_position_uas(node_uas, raw, steps["pos_uas_per_lsb"])
            # 闭式：x = x0 + dx/(3600·1000·500) 度，闭式换算到 µas
            expect = node_uas + raw * (1.0 / (3600.0 * 1000.0 * 500.0)
                                       * 3600.0 * 1.0e6)
            harness.close(got, expect, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * abs(expect),
                          what=f"pos dx raw={raw}", scale=abs(expect))
            ev.record(f"pos dx raw={raw}", got, expect, unit="uas")
        for raw in (-32768, -1, 0, 1, 32767):
            got = decode_dra_uas(0.0, raw, steps["dra_uas_per_lsb"])
            expect = raw * (1.0 / (3600.0 * 1000.0 * 100.0) * 3600.0 * 1.0e6)
            harness.close(got, expect, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * abs(expect),
                          what=f"dra raw={raw}", scale=abs(expect))
            if raw < 0:
                harness.is_true(got < 0.0,
                                f"dra raw={raw} 为负时解码必须保号，实际 {got!r}")
            elif raw > 0:
                harness.is_true(got > 0.0,
                                f"dra raw={raw} 为正时解码必须保号，实际 {got!r}")
            ev.record(f"dra raw={raw}", got, expect, unit="uas",
                      note="int16 有符号，负修正必须保号")


# ===========================================================================
# §4 S12 —— Gaia DR3SP 8-bit 量化
# ===========================================================================

@harness.test(
    "quantization.s12.decode_closed_form",
    intent="**期望值层**：S12 的解码必须逐点等于 `GAIA_QUERY.md` §2.6 的闭式 "
           "`F(λ_j) = byte_j × flux_mul + flux_min`（独立闭式，"
           "期望值不来自被测实现自身的输出）。",
    inputs="byte ∈ {0, 1, 127, 128, 254, 255} × flux_mul ∈ {1e-12, 3.7e-13} × "
           "flux_min ∈ {0, 8·flux_mul}",
    expected="逐点精确等于 `byte·flux_mul + flux_min`（浮点档 f64 非归约）",
    source=_SRC_S12_DECODE + "（期望值来源）",
    criteria=("S12",),
)
def _case_s12_decode() -> None:
    with harness.evidence() as ev:
        for flux_mul in (1.0e-12, 3.7e-13):
            for n_zero in (0, 8):
                flux_min = n_zero * flux_mul
                for byte in (0, 1, 127, 128, 254, 255):
                    got = spectrum_decode(np.int64(byte), flux_mul, flux_min)
                    expect = byte * flux_mul + flux_min
                    harness.close(got, expect, rtol=tol.F64_RTOL,
                                  atol=tol.F64_ATOL_PER_SCALE * abs(expect),
                                  what=f"byte={byte} mul={flux_mul} min={flux_min}",
                                  scale=abs(expect))
                    ev.record(f"decode[{byte}/mul={flux_mul:g}/zero={n_zero}L]",
                              got, expect, unit="W·m^-2·nm^-1",
                              note="GAIA_QUERY.md §2.6 闭式")


@harness.test(
    "quantization.s12.roundtrip_bound_pointwise",
    intent="**期望值层（分布无关）**：8-bit 对称均匀量化的逐点往返界是 "
           "`|F̂ − F| ≤ flux_mul/2`，且恰在两个码级中点取等号。"
           "这是与统计量无关的解析结论，任何分布都成立。",
    inputs="零原点码空间的整级与中点样本，k = 0..255；另加越界一步的样本"
           "（−0.5Δ 与 255.5Δ）检验夹取饱和",
    expected="域内样本误差 ≤ Δ/2 且中点样本恰取等号；越界一步的样本误差 ≤ Δ"
             "（饱和界），**不得**出现 ≥ Δ 的环绕跳变",
    source="对称均匀量化器的逐点界 |F̂ − F| ≤ Δ/2；"
           "码宽 8 bit 与 343 样本数来自 " + _SRC_S12_DECODE,
    criteria=("S12",),
)
def _case_s12_pointwise() -> None:
    delta = FLUX_MUL
    half = tol.get("quant.roundtrip_half_lsb").value(delta)
    with harness.evidence() as ev:
        # 整级：误差必须逐位为 0
        worst_level = max(
            abs(spectrum_decode(spectrum_encode(np.float64(k * delta), delta),
                                delta) - k * delta) for k in range(CODE_MAX + 1))
        harness.exact(worst_level, 0.0, "码级上的往返误差必须逐位为 0")
        ev.record("整级样本最坏相对误差", worst_level, 0.0)

        # 中点：解析界恰取等号
        worst_mid = max(
            abs(spectrum_decode(spectrum_encode(np.float64((k + 0.5) * delta),
                                                delta), delta) - (k + 0.5) * delta)
            for k in range(CODE_MAX))
        harness.close(worst_mid, half, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * abs(half),
                      what="8-bit 中点最坏往返误差", scale=abs(half))
        ev.record("中点样本最坏往返误差", worst_mid, half,
                  unit="W·m^-2·nm^-1", note="Δ/2 的解析界恰取等号")

        # 越界一步：夹取饱和界 Δ（不是 Δ/2），且不得环绕
        out = np.array([-0.5 * delta, 255.5 * delta, 256.0 * delta])
        code = spectrum_encode(out, delta)
        harness.is_true(code.min() >= 0 and code.max() <= CODE_MAX,
                        "越界一步的样本必须被夹取到 0..255，不得越界")
        worst_sat = float(np.abs(spectrum_decode(code, delta) - out).max())
        harness.less_equal(worst_sat, delta, "夹取饱和的最坏误差上界 = Δ")
        ev.record("越界一步的最坏饱和误差", worst_sat, delta,
                  unit="W·m^-2·nm^-1", note="饱和界 Δ（码宽一步）")


@harness.test(
    "quantization.s12.statistic_vs_closed_form",
    intent="**期望值层（统计量）**：实测 median/p95 必须落在**闭式解析值**附近——"
           "零原点码空间上相对残差的 CDF 是 `P(r≤x)=255x`（x≤1/510）与 "
           "`1−1/(1020x)`（x>1/510），故 median = 1/510、p95 = 1/51。"
           "容差由经验分位数的渐近标准误**事前冻结**，不是随手写的数。",
    inputs=f"固定种子 {SPECTRUM_SEED}；{SPECTRUM_N_STARS} 条 × "
           f"{SPECTRUM_SAMPLES} 样本 = {SPECTRUM_N_STARS * SPECTRUM_SAMPLES} 残差；"
           "fluxMin=0（相对残差最坏情形）",
    expected=f"median ≈ {1 / 510:.6e}（=1/510）、p95 ≈ {1 / 51:.6e}（=1/51），"
             "偏差在冻结的分位数标准误内",
    source="闭式 CDF 推导（见本文件 §「零原点 8-bit 码的闭式相对残差分布」）；"
           "夹具常量取自 " + _SRC_S12_DECODE,
    criteria=("S12",),
)
def _case_s12_vs_closed_form() -> None:
    flux = _spectral_flux()
    n = flux.size
    median, p95 = _s12_statistic(flux, flux_min=FLUX_MIN_ZERO_ORIGIN)
    # 容差 = 3 倍渐近标准误（3σ），事前冻结
    tol_median = 3.0 * _order_statistic_sd(0.5, n, _S12_MEDIAN_DENSITY)
    tol_p95 = 3.0 * _order_statistic_sd(0.95, n, _S12_P95_DENSITY)
    with harness.evidence() as ev:
        harness.less_equal(abs(median - _S12_MEDIAN_CLOSED_FORM), tol_median,
                           "median 与闭式 1/510 的偏差超出 3σ 分位数容差")
        harness.less_equal(abs(p95 - _S12_P95_CLOSED_FORM), tol_p95,
                           "p95 与闭式 1/51 的偏差超出 3σ 分位数容差")
        ev.record("median 实测", median, _S12_MEDIAN_CLOSED_FORM,
                  note=f"闭式 1/510；3σ 容差 {tol_median:.3e}")
        ev.record("p95 实测", p95, _S12_P95_CLOSED_FORM,
                  note=f"闭式 1/51；3σ 容差 {tol_p95:.3e}")
        ev.record("median 3σ 容差", tol_median, note="来自 sqrt(0.25/n)/255")
        ev.record("p95 3σ 容差", tol_p95, note="来自 sqrt(0.95·0.05/n)/2.55")


@harness.test(
    "quantization.s12.statistic_vs_spec_threshold",
    intent="**阈值层**：`GAIA_QUERY.md` §2.9.1 的实测读数 median 0.21% / p95 1.8% "
           "作为**门限**（不是期望值）逐项判。零原点是相对残差的最坏情形，"
           "因此这是该门限可被检验的最严格一侧。",
    inputs="同一夹具（零原点、固定种子、4096×343 残差）",
    expected="median ≤ 0.0021 且 p95 ≤ 0.018（tolerances.GAIA_QUANT 冻结门限）",
    source=_SRC_S12_STAT + "；统计量选型依据 tolerances.GAIA_STATISTIC_NOTE",
    criteria=("S12",),
)
def _case_s12_vs_threshold() -> None:
    frozen = tol.get("gaia.quant_rel").value
    flux = _spectral_flux()
    median, p95 = _s12_statistic(flux, flux_min=FLUX_MIN_ZERO_ORIGIN)
    with harness.evidence() as ev:
        ev.record("median 实测", median, frozen["median"],
                  note="门限 = GAIA_QUERY.md §2.9.1 实测读数 0.21%")
        ev.record("p95 实测", p95, frozen["p95"],
                  note="门限 = GAIA_QUERY.md §2.9.1 实测读数 1.8%")
        harness.less_equal(median, frozen["median"],
                           "median 超正本门限 0.21%（零原点最坏情形）")
        harness.less_equal(p95, frozen["p95"],
                           "p95 超正本门限 1.8%（零原点最坏情形）")


# ===========================================================================
# §5 S12 负例
# ===========================================================================

def _expect_statistic_tooth(what: str, faithful: tuple[float, float],
                            injected: tuple[float, float]) -> None:
    """统计量负例的双向断言。

    基准**不用**正本门限（`GAIA_QUANT`）——该门限的可复现性本身正在
    `quantization.s12.statistic_vs_spec_threshold` 上登记争议，零原点最坏情形下
    未注入的 p95 已越界，拿它当基准会让每条负例都「无法归因」。

    基准改用**闭式解析值**（median = 1/510、p95 = 1/51，见本文件 §「零原点 8-bit
    码的闭式相对残差分布」）：未注入必须落在闭式值附近，注入后必须显著偏离闭式值
    （≥ 1.5 倍）——否则该缺陷落在 median/p95 的盲区里，这条负例无效。
    """
    f_med, f_p95 = faithful
    i_med, i_p95 = injected
    pairs = (("median", f_med, i_med, _S12_MEDIAN_CLOSED_FORM),
             ("p95", f_p95, i_p95, _S12_P95_CLOSED_FORM))
    for name, base, inj, closed in pairs:
        if not (0.5 * closed <= base <= 1.5 * closed):
            raise harness.CheckFailure(
                f"{what}: 未注入时 {name}={base!r} 偏离闭式值 {closed!r} 超过 1.5 倍，"
                "这条负例无法归因")
        ratio = inj / closed
        if ratio < 1.5:
            raise harness.CheckFailure(
                f"{what}: 注入缺陷后 {name}={inj!r} 仅为闭式值 {closed!r} 的 "
                f"{ratio:.3f} 倍（< 1.5×）⇒ **判据没有牙齿**，"
                "该缺陷落在 median/p95 的盲区里")


@harness.test(
    "quantization.s12.negative.truncation",
    intent="把 8-bit 编码的 `round` 换成 `floor`/`trunc`（截断而非四舍五入）。"
           "截断的误差范围是 [0, Δ) 而非 [−Δ/2, Δ/2]，相对残差的 median 与 p95 "
           "都翻倍，直接越过正本门限。",
    inputs="同一夹具（零原点、4096×343）；缺陷 = `np.rint` → `np.floor`",
    expected="正本：median ≈ 1/510、p95 ≈ 1/51；注入后 median ≈ 2/510、"
             "p95 ≈ 2/51，两项都越过 0.0021 / 0.018 门限",
    source="截断量化器的解析误差范围 [0, Δ) vs 最近邻的 [−Δ/2, Δ/2]；"
           "门限 " + _SRC_S12_STAT,
    kind=harness.NEGATIVE,
    inject="编码侧 `round` → `floor`（截断；对负值不等价于向零截断，此处等价）",
    defect_id="GAIA-N-TRUNC",
    criteria=("S12",),
)
def _case_s12_neg_truncation() -> None:
    frozen = tol.get("gaia.quant_rel").value
    flux = _spectral_flux()
    faithful = _s12_statistic(flux, mode="nearest")
    injected = _s12_statistic(flux, mode="floor")
    with harness.evidence() as ev:
        _expect_statistic_tooth("截断量化", faithful, injected)
        ev.record("截断后 median", injected[0], frozen["median"],
                  note="正本门限 0.0021")
        ev.record("截断后 p95", injected[1], frozen["p95"],
                  note="正本门限 0.018")
        ev.record("截断后 median / 闭式 1/510", injected[0] / _S12_MEDIAN_CLOSED_FORM,
                  note="截断把误差范围从 Δ/2 扩到 Δ ⇒ 统计量翻倍")
        ev.record("截断后 p95 / 闭式 1/51", injected[1] / _S12_P95_CLOSED_FORM,
                  note="同上")
        ev.record("未注入 median", faithful[0], frozen["median"])
        ev.record("未注入 p95", faithful[1], frozen["p95"])


@harness.test(
    "quantization.s12.negative.wrong_code_level",
    intent="把 8-bit 码空间写错档（码级上限 127，即退化成 7-bit）。"
           "码宽不变而可用码级减半 ⇒ 高通量端全部饱和，残差暴涨。",
    inputs="同一夹具（零原点）；缺陷 = 码级上限 255 → 127",
    expected="正本：median ≤ 0.0021、p95 ≤ 0.018；注入后两项都远超门限",
    source="码宽 8 bit（码级 0..255）来自 " + _SRC_S12_DECODE + "；门限 " + _SRC_S12_STAT,
    kind=harness.NEGATIVE,
    inject="码级上限 255 → 127（8-bit 写成 7-bit）",
    defect_id="GAIA-N-LEVEL",
    criteria=("S12",),
)
def _case_s12_neg_level() -> None:
    frozen = tol.get("gaia.quant_rel").value
    flux = _spectral_flux()
    faithful = _s12_statistic(flux)
    injected = _s12_statistic(flux, max_code=127)
    with harness.evidence() as ev:
        _expect_statistic_tooth("码宽写错档", faithful, injected)
        ev.record("错档后 median", injected[0], frozen["median"], note="门限 0.0021")
        ev.record("错档后 p95", injected[1], frozen["p95"], note="门限 0.018")
        ev.record("错档后 median / 闭式 1/510",
                  injected[0] / _S12_MEDIAN_CLOSED_FORM, note="超界倍率")
        ev.record("错档后 p95 / 闭式 1/51",
                  injected[1] / _S12_P95_CLOSED_FORM, note="超界倍率")


@harness.test(
    "quantization.s12.negative.missing_high_clamp",
    intent="端点处理错：漏掉上端夹取。码级越界后写进 `uint8` 会**模 256 环绕**，"
           "高通量端被搬到码空间另一端（解码值跳变 256 个码级），"
           "而不是饱和在 255。这是本条负例要求的「端点处理错」家族。",
    inputs="越界一步的样本集（−0.5Δ、255.5Δ、256Δ）叠加零原点域内样本；"
           "缺陷 = 去掉 `clip` 并按 uint8 环绕",
    expected="正本：域内误差 ≤ Δ/2、越界一步饱和误差 ≤ Δ；"
             "注入后出现 ≥ 256Δ 量级的环绕跳变 ⇒ 超出饱和界两个数量级",
    source="uint8 记录宽度与 `GAIA_QUERY.md` §2.6 的 `uint8 flux[343]` 布局；"
           "环绕是 C 侧写 uint8 的定义行为",
    kind=harness.NEGATIVE,
    inject="去掉上/下端夹取（`clamp=False`）并按 uint8 模 256 环绕",
    defect_id="GAIA-N-NOCLAMP",
    criteria=("S12",),
)
def _case_s12_neg_noclamp() -> None:
    delta = FLUX_MUL
    half = tol.get("quant.roundtrip_half_lsb").value(delta)
    domain = _spectral_flux(n_stars=8).ravel()
    out = np.array([-0.5 * delta, 255.5 * delta, 256.0 * delta])
    fixture = np.concatenate([domain, out])

    faithful_code = spectrum_encode(fixture, delta)
    harness.is_true(faithful_code.min() >= 0 and faithful_code.max() <= CODE_MAX,
                    "未注入时码级必须在 0..255 内")
    faithful_err = float(np.abs(spectrum_decode(faithful_code, delta)
                                - fixture).max())
    harness.less_equal(faithful_err, delta,
                       "未注入时最坏误差（含一步越界的饱和）必须 ≤ Δ")

    injected_code = uint8_wrap(spectrum_encode(fixture, delta, clamp=False))
    injected_err = float(np.abs(spectrum_decode(injected_code, delta)
                                - fixture).max())
    with harness.evidence() as ev:
        ev.record("未注入最坏误差", faithful_err, delta,
                  unit="W·m^-2·nm^-1", note="含一步越界的饱和")
        ev.record("缺夹取后最坏误差", injected_err, delta,
                  unit="W·m^-2·nm^-1", note="uint8 环绕跳变")
        ev.record("环绕跳变 / 饱和界 Δ", injected_err / delta, note="超界倍率")
        if injected_err <= delta:
            raise harness.CheckFailure(
                f"缺夹取后最坏误差 {injected_err!r} 仍在饱和界 Δ={delta!r} 内 ⇒ "
                "**判据没有牙齿**（夹取缺陷在本夹具上不改变读数）")
        harness.is_true(injected_err >= 256.0 * delta - delta,
                        "环绕应造成约 256 个码级的跳变量级")


@harness.test(
    "quantization.s12.negative.zero_point_dropped",
    intent="端点处理错（零点丢失）：解码写成 `byte·flux_mul` 而漏掉 "
           "`+ flux_min` 零点项（`GAIA_QUERY.md` §2.6 闭式的第二项）。"
           "零原点夹具上该缺陷与正确实现**同值**（fluxMin=0），"
           "故本条按正本格式约束另取一个非零零点夹具，否则这条负例恒绿。",
    inputs=f"fluxMin = 8·fluxMul 的码空间样本；缺陷 = 解码漏掉 + flux_min",
    expected="正本：逐点往返误差 ≤ Δ/2；注入后误差恒为 fluxMin = 8Δ = "
             "16 倍于 Δ/2 的界",
    source=_SRC_S12_DECODE + "（零点项 flux_min 是闭式的组成部分）"
           "；正本未约束 fluxMin 取值，故非零零点为合法记录",
    kind=harness.NEGATIVE,
    inject="解码漏掉 `+ flux_min` 零点项",
    defect_id="GAIA-N-ZEROPOINT",
    criteria=("S12",),
)
def _case_s12_neg_zeropoint() -> None:
    delta = FLUX_MUL
    half = tol.get("quant.roundtrip_half_lsb").value(delta)
    n_zero = 8
    flux_min = n_zero * delta
    flux = flux_min + _spectral_flux(n_stars=8).ravel()
    harness.is_true(float(flux.min()) >= flux_min
                    and float(flux.max()) <= flux_min + CODE_MAX * delta,
                    "夹具必须落在可表示域 [flux_min, flux_min+255Δ] 内，否则"
                    "测到的是饱和而不是零点缺陷")

    faithful = spectrum_decode(spectrum_encode(flux, delta, flux_min),
                               delta, flux_min)
    faithful_err = float(np.abs(faithful - flux).max())
    injected = spectrum_decode(spectrum_encode(flux, delta, flux_min), delta, 0.0)
    injected_err = float(np.abs(injected - flux).max())

    with harness.evidence() as ev:
        harness.less_equal(faithful_err, half, "未注入时逐点误差必须 ≤ Δ/2")
        ev.record("未注入最坏误差", faithful_err, half, unit="W·m^-2·nm^-1")
        ev.record("漏零点后最坏误差", injected_err, half, unit="W·m^-2·nm^-1",
                  note=f"等于 fluxMin = {n_zero}Δ")
        ev.record("漏零点后 / Δ/2 界", injected_err / half, note="超界倍率")
        if injected_err <= half:
            raise harness.CheckFailure(
                f"漏零点后最坏误差 {injected_err!r} 仍在 Δ/2={half!r} 内 ⇒ "
                "**判据没有牙齿**")
        harness.less_equal(injected_err, flux_min + half,
                           "漏零点后误差必须被夹在 [fluxMin−Δ/2, fluxMin+Δ/2] 内")
        harness.less_equal(flux_min - half, injected_err,
                           "漏零点后误差的下界也是 fluxMin−Δ/2")
        # 零原点上该缺陷不可见 —— 这就是必须换夹具的理由
        zero_flux = _spectral_flux(n_stars=8).ravel()
        zero_origin_err = float(np.abs(
            spectrum_decode(spectrum_encode(zero_flux, delta), delta, 0.0)
            - zero_flux).max())
        ev.record("零原点夹具上的同一缺陷", "不可见",
                  note=f"fluxMin=0 时漏零点与正确实现同值（误差上界 "
                       f"{zero_origin_err:.3e}）⇒ 本负例必须用非零零点夹具")


# ===========================================================================
# §6 S13 负例
# ===========================================================================

@harness.test(
    "quantization.s13.negative.truncation",
    intent="把 XPSD 本地编码的取整从最近邻换成截断（`floor`/`int()`，负数不对称）。"
           "截断的误差范围是 [0, Δ) ⇒ 最坏往返误差 = Δ = **2 倍**于解析界 Δ/2。",
    inputs=f"pos 步长 Δ = 2 µas；k ∈ [−1000, 1000] 的整级样本与"
           "「下一级前 1e-9 步」的样本（截断的最坏点）",
    expected="正本：最坏往返误差 ≤ Δ/2 = 1 µas；注入后 = Δ = 2 µas，"
             "超界恰 2 倍",
    source=_SRC_RT_BOUND + "；截断量化器的误差范围 [0, Δ)（最近邻为 [−Δ/2, Δ/2]）",
    kind=harness.NEGATIVE,
    inject="编码侧 `round` → `trunc`（截断取整，负值向零不对称）",
    defect_id="XPSD-N-TRUNC",
    criteria=("S13",),
)
def _case_s13_neg_truncation() -> None:
    step = tol.get("xpsd.quant_step").value["pos_uas_per_lsb"]
    half = tol.get("quant.roundtrip_half_lsb").value(step)
    with harness.evidence() as ev:
        faithful = max(
            abs(dequantize(quantize(x, step), step) - x)
            for k in range(-1000, 1001)
            for x in (k * step, (k + 0.5) * step, (k + 1) * step - step * 1e-9))
        injected = max(
            abs(dequantize(quantize(x, step, mode="trunc"), step) - x)
            for k in range(-1000, 1001)
            for x in (k * step, (k + 0.5) * step, (k + 1) * step - step * 1e-9))
        harness.less_equal(faithful, half, "未注入时最坏往返误差必须 ≤ Δ/2")
        ev.record("未注入最坏往返误差", faithful, half, unit="uas")
        ev.record("截断后最坏往返误差", injected, half, unit="uas",
                  note="截断的最坏点 = 下一级前一步")
        ev.record("截断后 / Δ/2 界", injected / half, note="超界倍率")
        if injected <= half:
            raise harness.CheckFailure(
                f"截断后最坏误差 {injected!r} 仍在 Δ/2={half!r} 内 ⇒ "
                "**判据没有牙齿**")
        harness.less_equal(injected, delta_upper(step), "截断的最坏界 = Δ")


def delta_upper(step: float) -> float:
    """截断量化器的解析最坏界 = Δ（供上条断言夹住上界，避免无限放行）。"""
    return step


@harness.test(
    "quantization.s13.negative.grid_shift_half_lsb",
    intent="取整方向取反（等价形态：重建网格从 mid-tread 偏移到 mid-rise）。"
           "中点样本的误差由 Δ/2 变成 `Δ/2 + 半个 LSB` = Δ，"
           "整级样本由 0 变成 Δ/2。",
    inputs=f"pos 步长 Δ = 2 µas；k ∈ [−1000, 1000] 的整级与中点样本；"
           "缺陷 = 重建 `(k+0.5)·Δ`",
    expected="正本：中点误差 = Δ/2 = 1 µas；注入后中点误差 = Δ = 2 µas，"
             "超界恰 2 倍",
    source=(_SRC_RT_BOUND + "；重建网格 mid-rise 与 mid-tread 相差半个 LSB"
              "（正本未另立口径，该形态即『取整方向取反』的可执行等价物）"),
    kind=harness.NEGATIVE,
    inject="重建网格偏移半个 LSB（`grid='mid_rise'`），等价于取整方向取反",
    defect_id="XPSD-N-GRIDSHIFT",
    criteria=("S13",),
)
def _case_s13_neg_gridshift() -> None:
    step = tol.get("xpsd.quant_step").value["pos_uas_per_lsb"]
    half = tol.get("quant.roundtrip_half_lsb").value(step)
    with harness.evidence() as ev:
        faithful_mid = max(
            abs(dequantize(quantize((k + 0.5) * step, step), step) - (k + 0.5) * step)
            for k in range(-1000, 1001))
        injected_mid = max(
            abs(dequantize(quantize((k + 0.5) * step, step), step,
                           grid="mid_rise") - (k + 0.5) * step)
            for k in range(-1000, 1001))
        faithful_lvl = max(
            abs(dequantize(quantize(k * step, step), step) - k * step)
            for k in range(-1000, 1001))
        injected_lvl = max(
            abs(dequantize(quantize(k * step, step), step,
                           grid="mid_rise") - k * step)
            for k in range(-1000, 1001))
        harness.close(faithful_mid, half, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * abs(half),
                      what="未注入的中点误差", scale=abs(half))
        harness.exact(faithful_lvl, 0.0, "未注入的整级误差必须为 0")
        ev.record("未注入中点误差", faithful_mid, half, unit="uas")
        ev.record("网格偏移后中点误差", injected_mid, half, unit="uas",
                  note="= Δ/2 + 半个 LSB")
        ev.record("网格偏移后中点误差 / Δ/2 界", injected_mid / half,
                  note="超界倍率")
        ev.record("未注入整级误差", faithful_lvl, 0.0, unit="uas")
        ev.record("网格偏移后整级误差", injected_lvl, 0.0, unit="uas",
                  note="整级样本也被挪走半格")
        if injected_mid <= half:
            raise harness.CheckFailure(
                f"网格偏移后中点误差 {injected_mid!r} 仍在 Δ/2={half!r} 内 ⇒ "
                "**判据没有牙齿**")


@harness.test(
    "quantization.s13.positive_disallowed_lsb_7p2_rejected_by_value",
    intent="`GAIA_QUERY.md` §2.3 逐字「**禁用 7.2 µas/LSB 这一取值**」——本条核对"
           "冻结步长取值不落在禁用集合内（判据设计自身的正确性）",
    inputs="独立推导的 pos 步长 vs 冻结值 2.0 µas/LSB，与正本点名禁用的 7.2",
    expected="冻结步长 = 2.0；7.2 属禁用集合且与冻结值相差 3.6 倍",
    source="docs/science/algorithms/GAIA_QUERY.md §2.3 逐字禁用条款；"
           "冻结值 tolerances.XPSD_QUANT",
    kind=harness.POSITIVE,
    inject="（无代码注入 —— 对抗复核 R1 指出初版把字面量比较标成了负例；已改正）",
    defect_id="XPSD-N-LSB72",
    criteria=("S13",),
)
def _case_s13_neg_lsb72() -> None:
    frozen = tol.get("xpsd.quant_step").value["pos_uas_per_lsb"]
    wrong = 7.2
    derived = derive_steps()["pos_uas_per_lsb"]
    with harness.evidence() as ev:
        harness.exact(derived, frozen, "独立推导的 pos 步长必须等于冻结值")
        ev.record("推导步长", derived, frozen, unit="uas/LSB")
        ev.record("禁用取值 7.2 / 冻结值", wrong / frozen, unit="倍",
                  note="GAIA_QUERY.md §2.3 点名禁用")
        if wrong <= frozen:
            raise harness.CheckFailure(
                f"7.2 µas/LSB 未超冻结步长 {frozen!r} ⇒ **判据没有牙齿**")
        # 7.2 µas/LSB 对应 1/5e8 deg/LSB 的分母，与实码 1.8e9 不符
        implied_lsb_per_deg = UAS_PER_DEG / wrong
        harness.is_true(implied_lsb_per_deg != 1.0 / INV_SCALE_DEG_PER_LSB,
                        "7.2 µas/LSB 必须对应不同的码密度，否则正本的禁用条款无意义")
        ev.record("7.2 µas/LSB 隐含的码密度", implied_lsb_per_deg, unit="LSB/deg",
                  note="GAIA_QUERY.md §2.3：对应 1/5e8 deg，与实码 1.8e9 不符")


@harness.test(
    "quantization.s13.negative.dimension_mixup",
    intent="量纲混用：位置量化器误取星等表的步长（0.001）当作 µas 步长。"
           "µas/LSB 与 mag/LSB 是**两个不可通约的量**，任何把两者放进同一个"
           "无量纲容差的比较都是错的。本条同时暴露一个判据盲区：**步长混细"
           "（偏小）在任何误差界判据下都是 fail-open 的**——它把位置精度虚报得"
           "过细而误差恒在界内，只能靠**步长取值本身**被抓住。",
    inputs="位置往返：正本步长 2.0 µas/LSB vs 混入的 0.001（当作 µas）；"
           "中点样本与整级样本各一",
    expected="正本：步长逐位 = 2.0 µas/LSB，误差界 = 1.0 µas；"
             "注入后步长 = 0.001，与冻结值差 2000 倍 ⇒ 步长判据判红；"
             "同时如实记录误差界判据在注入后**不红**（fail-open 盲区）",
    source="docs/science/algorithms/GAIA_QUERY.md §2.2/§2.3（两套步长分属不同量纲）"
           "；tolerances.XPSD_QUANT 的 scale_domain 逐字「位置 µas/LSB；"
           "dra µas/LSB；星等 mag/LSB」",
    kind=harness.NEGATIVE,
    inject="位置量化器误用 mag 步长 0.001 作为 µas 步长（量纲混用，步长偏细 2000 倍）",
    defect_id="XPSD-N-DIMMIX",
    criteria=("S13",),
)
def _case_s13_neg_dimmix() -> None:
    steps = derive_steps()
    pos_step = steps["pos_uas_per_lsb"]
    mag_step = steps["mag_per_lsb"]
    frozen = tol.get("xpsd.quant_step").value["pos_uas_per_lsb"]
    half = tol.get("quant.roundtrip_half_lsb").value(pos_step)

    samples = (1000.0 * pos_step,                 # 整级样本（µas）
               1000.0 * pos_step + pos_step / 2.0)  # 中点样本（µas）
    correct_err = max(abs(dequantize(quantize(x, pos_step), pos_step) - x)
                      for x in samples)
    mixed_err = max(abs(dequantize(quantize(x, mag_step), mag_step) - x)
                    for x in samples)

    with harness.evidence() as ev:
        # 正本：步长取值 + 误差界双双成立
        harness.exact(pos_step, frozen, "独立推导的 pos 步长必须逐位等于冻结值")
        harness.close(correct_err, half, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * abs(half),
                      what="未混用时的最坏往返误差", scale=abs(half))
        ev.record("未混用的最坏往返误差", correct_err, half, unit="uas")
        ev.record("未混用的 pos 步长", pos_step, frozen, unit="uas/LSB")

        # 注入：步长取值判据必须判红（这是唯一能抓住它的判据）
        ev.record("混入后的 pos 步长", mag_step, frozen, unit="uas/LSB",
                  note="把 mag 步长当成了 µas 步长")
        ev.record("步长偏差倍率", mag_step / frozen, unit="倍",
                  note="2000 倍；方向是偏细 ⇒ 误差界判据 fail-open")
        if mag_step == frozen:
            raise harness.CheckFailure(
                "混入后的步长与冻结值相同 ⇒ **步长判据没有牙齿**")

        # 盲区如实登记：偏细的步长在误差界判据下不红
        ev.record("混入后的最坏往返误差", mixed_err, half, unit="uas",
                  note=f"仍在界内 ⇒ 误差界判据对本缺陷 fail-open"
                       f"（判定：{'超界' if mixed_err > half else '未超界'}）")

        # 正例侧证据：mag 的门限只在 mag 面生效，与 µas 门限不可互换
        ev.record("mag 门限（只在 mag 面生效）",
                  tol.get("quant.roundtrip_half_lsb").value(mag_step), unit="mag")
        ev.record("pos 门限（只在 µas 面生效）", half, unit="uas")
        ev.record("两门限之比（不同量纲，比值无物理意义）", half
                  / tol.get("quant.roundtrip_half_lsb").value(mag_step),
                  unit="uas/mag", note="仅登记，说明不存在无量纲的统一容差")