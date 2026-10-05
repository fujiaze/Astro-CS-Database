"""守恒映射算子（P3）单元测试集 —— 吸收 `T02-门禁退役与判据清单.md` §2.1 的 **S7 / S8 / S9**。

## 本文件的判别力来源

`expected` **不由 `drizzle_ref.py` 的输出反推**。每个期望值来自下列三者之一：

1. `docs/science/drizzle/DRIZZLE.md` 的闭式（逐字引用在每条用例的 `source` 里）；
2. 本文件内的独立解析推导（例如 `c_jp = a_jp/(A_pixel,j·D_p)` 由 §3.3 的定义直接算出）；
3. `astropy_healpix` 的独立 HEALPix 几何引擎（叶面积、四角弦多边形、像元归属、
   稠密采样覆盖认证）。

`drizzle_ref.py` 是按正本公式逐行转写的**被测口径**（第二实现），与上面的 oracle 互相独立。

## S7 的条件化结构（本单最关键的纪律点）

`T02-门禁退役与判据清单.md` §6.2 C5 逐字：「**通量守恒「严格不变量」已被对抗审核反例推翻**
……该恒等式只在 drop 几何闭合时成立；§8 的 NaN 样本掩膜路径会从 `F_p`/分母/方差中一并剔除
并重归一，故严格守恒在该路径下破裂」。`DRIZZLE.md` §3.7 自带「（几何闭合时）」限定。

⇒ 本文件**不写无条件守恒断言**。守恒被写成三层条件式：

```text
条件 A（几何闭合）  ∀j: Σ_p a_jp = A_drop,j            —— L1，判据 tolerances.DRIZZLE_L1_COMPLETENESS
条件 B（掩膜一致）  j 被剔除 ⟺ 对**所有**叶同时剔除     —— 样本级掩膜，不是逐叶掩膜
条件 C（子集守恒）  只有在 A ∧ B 下，「掩膜后合格子集 Ω」上才有
                    Σ_p Σ_{j∈Ω} x_j w_jp = Σ_{j∈Ω} x_j
                    —— 判据 tolerances.DRIZZLE_FLUX_SUM_REL
```

- 条件 A 由 `astropy_healpix` 独立认证（稠密采样确认 drop 整块落在同一叶内）
  + 被测实现实测 `Σ_p a_jp` 双重确认；
- 条件 B 由实现暴露的 `n_rejected_nonfinite` 逐叶计数一致性确认；
- 条件 C 的**反例方向**（A 或 B 不成立时守恒破裂）另有专门负例逐条实测：
  `drizzle.s7.neg-swallowed-leaf-breaks-closure`（A 破裂）、
  `drizzle.s7x9.conditional-conservation-under-mask`（C 的无条件形式破裂）。

`DRIZZLE.md` §5.1 逐字：「求和型守恒门只证明总量守恒……**通量守恒的判别力只能由逐叶判据
提供，求和型门只作辅助**」⇒ `drizzle.s7.neg-total-preserving-per-leaf-misallocation`
把这一条实测出来：同一注入下求和型读数精确为 0（绿），逐叶读数 `O(0.4)`（红）。

## `criteria` 标签约定

`S7` / `S8` / `S9` 是 `T02-门禁退役与判据清单.md` §2.1 的判据编号。
`G-*` 是 `DRIZZLE.md` §5.1「正确性判据」表的行名缩写，便于逐行回指正本：

| 标签 | `DRIZZLE.md` §5.1 行 |
|---|---|
| `G-CONS` | 通量守恒门 |
| `G-WEIGHT` | 权重和门 |
| `G-SB` | 常量面亮度门（累加器级） |
| `G-PROD` | 常量面亮度门（产品级） |
| `G-VAR` | 方差恒等判据 |
| `G-SCALE` | 联合缩放律门 |
| `G-COV` | 协方差门 |
| `G-L1` | 几何闭合 L1（构造级） |

## pixfrac 夹具

一律用 `tolerances.DRIZZLE_FIXTURE_PIXFRAC`（0.8），**不用任何生产默认**。
理由逐字照 `tolerances.PIXFRAC_FREEZE_NOTE`：`T02` §6.2 C4 指出 pixfrac 默认值在仓内有
`1.0`（治理裁决）/ `0.8`（科学正本 + `defaults.json`）/「pending_authority」（schema）
三方冲突；`pixfrac = 1` 时 S8 负例与正确实现同值、退化为恒绿。
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Callable, Dict, Mapping, Sequence, Tuple

import astropy.units as u
import astropy_healpix as healpix

from . import drizzle_ref as ref
from . import harness
from . import tolerances as tol

# ---------------------------------------------------------------------------
# 常量与夹具参数
# ---------------------------------------------------------------------------

NSIDE = 512                                   # `DRIZZLE.md` §4 参数表下限
ARCSEC = math.pi / (180.0 * 3600.0)
PIXFRAC = tol.DRIZZLE_FIXTURE_PIXFRAC.value   # 0.8，冻结理由见 PIXFRAC_FREEZE_NOTE

#: 单叶夹具的像元角尺度 [arcsec/px]。取 60″：`DRIZZLE.md` §5.2 的 δ 取值示例里
#: `60″/px` 是表内最大的一档，`DRIZZLE_GEOMETRY.md` §10 进一步给到 300″/px，
#: 故 60″ 落在正本自己讨论过的尺度域内，而 drop 仍远小于生产 `nside` 的叶。
THETA_ARCSEC = 60.0
#: 双样本叶夹具的像元角尺度 [arcsec/px]。取 30″ 只为给两个相邻 drop 留出间隙。
THETA_PAIR_ARCSEC = 30.0
#: 被剔除（非有限）样本的像元角尺度 [arcsec/px]。取 20″ 使其面积与合格样本明显不同，
#: 这样「被剔除样本的交叠面积不得进入 D_p / N_p」的读数没有歧义。
THETA_MASKED_ARCSEC = 20.0
#: 常量面亮度夹具的 `B₀` [ADU/sr]。取值使 `x_j = B₀·A_pixel,j` 落在
#: `tolerances.DRIZZLE_FLUX_SUM_REL` 声明的 `Σ_j x_j ~ 1e0–1e8 ADU` 量级域内。
B0 = 3.3e8
#: 逐像元方差 [ADU²]。
V0 = 100.0
#: 联合缩放律的缩放因子 α。
ALPHA_SCALE = 3.0

A_CELL = math.pi / (3.0 * NSIDE ** 2)          # `DRIZZLE.md` §3.5 闭式

#: 正例方位 [deg, deg]：全在赤道带内、与任何面界/极冠都无关。
LON, LAT = 45.0, 10.0
#: 跨叶夹具用的两个相邻叶（`astropy_healpix` 实测共边）。
LEAF_A, LEAF_B = 12300, 12294
#: 「无覆盖」对照叶：与本夹具几何无任何交叠（南天另一侧）。
LEAF_EMPTY = 979


# ---------------------------------------------------------------------------
# Oracle 侧：独立 HEALPix 几何
# ---------------------------------------------------------------------------

def cell_area_oracle(nside: int = NSIDE) -> float:
    """`astropy_healpix` 的叶面积 [sr]。与 `ref.cell_area` 的闭式互校。"""
    return float(healpix.nside_to_pixel_area(nside).to_value(u.sr))


def cell_corners(ipix: int, nside: int = NSIDE) -> ref.Polygon:
    """目标叶的四角弦多边形 —— 生产 `nside ≥ 256` 的边界取法
    （`DRIZZLE.md` §4.1「生产域内叶边界恒为四角弦」）。"""
    return tuple(ref.vec_normalise(healpix.healpix_to_xyz(ipix, nside, dx, dy, order="nested"))
                 for dx, dy in ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)))


def cell_centre(ipix: int, nside: int = NSIDE) -> ref.Vec:
    return ref.vec_normalise(healpix.healpix_to_xyz(ipix, nside, 0.5, 0.5, order="nested"))


def cell_of(vec: Sequence[float], nside: int = NSIDE) -> int:
    return int(healpix.xyz_to_healpix(float(vec[0]), float(vec[1]), float(vec[2]),
                                      nside, order="nested"))


def direction(lon_deg: float, lat_deg: float) -> ref.Vec:
    lo, la = math.radians(lon_deg), math.radians(lat_deg)
    return ref.vec_normalise((math.cos(la) * math.cos(lo),
                              math.cos(la) * math.sin(lo),
                              math.sin(la)))


def polygon_centre(polygon: ref.Polygon) -> ref.Vec:
    """球面多边形的 gnomonic 质心（独立于被测实现的面积例程）。"""
    east, north = ref.tangent_basis(polygon[0])
    xs = [ref.vec_dot(v, east) / ref.vec_dot(v, polygon[0]) for v in polygon]
    ys = [ref.vec_dot(v, north) / ref.vec_dot(v, polygon[0]) for v in polygon]
    return ref.gnomonic_offset(polygon[0], math.fsum(xs) / len(xs), math.fsum(ys) / len(ys))


def covered_cells(polygon: ref.Polygon, half_width: float, nside: int = NSIDE,
                  samples: int = 48) -> Tuple[int, ...]:
    """独立覆盖认证：在 drop 的**外扩包框**上稠密采样。

    这是几何闭合条件 A 的**独立证书**：它不经过被测实现的任何裁剪代码。
    """
    centre = polygon_centre(polygon)
    cells = {cell_of(v, nside) for v in polygon}
    for i in range(samples + 1):
        for j in range(samples + 1):
            cells.add(cell_of(ref.gnomonic_offset(centre,
                                                   -1.4 * half_width + 2.8 * half_width * i / samples,
                                                   -1.4 * half_width + 2.8 * half_width * j / samples),
                              nside))
    return tuple(sorted(cells))


def overlaps_of(pixel: ref.SourcePixel, leaves: Mapping[int, ref.Polygon]) -> Dict[int, float]:
    """`a_jp`：被测实现实测的交叠面积。右端 `A_drop,j` 同样由多边形实测
    （**不由 `A_drop/pixfrac²` 反推**），两侧相互独立 —— `DRIZZLE.md` §3.7 L1 的前提。"""
    return {ipix: ref.polygon_intersection_area(pixel.drop_vertices, poly, pixel.reference)
            for ipix, poly in leaves.items()}


def closed_form_sb(a_jp: Sequence[float], x_j: Sequence[float],
                   pixel_areas: Sequence[float]) -> float:
    """`S_p = Σ_j B_j a_jp / Σ_j a_jp`（`DRIZZLE.md` §3.5 逐字），`B_j = x_j / A_pixel,j`。"""
    num = math.fsum((x / ap) * a for x, ap, a in zip(x_j, pixel_areas, a_jp))
    return num / math.fsum(a_jp)


def closed_form_variance(raw: Sequence[Tuple[float, float, float, float, int]],
                         covered_area: float) -> float:
    """`variance_p = Σ_j c_jp² v_j`，`c_jp = a_jp/(A_pixel,j·D_p)`（§3.3 / §3.6）。"""
    return math.fsum((r[0] / (r[1] * covered_area)) ** 2 * r[3] for r in raw)


# ---------------------------------------------------------------------------
# 夹具
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Fixture:
    name: str
    pixels: Tuple[ref.SourcePixel, ...]
    leaves: Dict[int, ref.Polygon]
    certified_cells: Tuple[int, ...]


def _sample(index: int, value: float, variance: float, centre: ref.Vec,
           theta_arcsec: float) -> ref.SourcePixel:
    theta = theta_arcsec * ARCSEC
    return ref.SourcePixel.make(index=index, value=value, variance=variance,
                               pixel_vertices=ref.square_polygon(centre, 0.5 * theta),
                               drop_vertices=ref.square_polygon(centre, 0.5 * PIXFRAC * theta),
                               reference=centre)


def _pair_centres() -> Tuple[ref.Vec, ref.Vec]:
    """同一叶内两个相邻且不重叠的像元中心。偏置放在**像元**尺度上，
    于是两个像元本身不重叠、两个 drop 更不重叠。"""
    centre = direction(LON, LAT)
    a = 0.5 * THETA_PAIR_ARCSEC * ARCSEC
    return (ref.gnomonic_offset(centre, -1.05 * a, 0.0),
            ref.gnomonic_offset(centre, 1.05 * a, 0.0))


def _make_pair_fixture(value_of: Callable[[int, float], float]) -> Fixture:
    centres = _pair_centres()
    pixels = tuple(
        _sample(k, value_of(k, ref.polygon_area(ref.square_polygon(centres[k],
                                                                    0.5 * THETA_PAIR_ARCSEC * ARCSEC),
                                                 centres[k])),
                V0 * (1.0 + 0.5 * k), centres[k], THETA_PAIR_ARCSEC)
        for k in (0, 1))
    leaf = cell_of(polygon_centre(ref.square_polygon(
        direction(LON, LAT), 0.5 * PIXFRAC * THETA_PAIR_ARCSEC * ARCSEC)))
    cells = set()
    for p in pixels:
        cells.update(covered_cells(p.drop_vertices, 0.5 * PIXFRAC * THETA_PAIR_ARCSEC * ARCSEC))
    return Fixture("two-adjacent-drops-one-leaf", pixels, {leaf: cell_corners(leaf)},
                   tuple(sorted(cells)))


def _make_single_fixture(name: str, theta_arcsec: float, value_of) -> Fixture:
    centre = direction(LON, LAT)
    area = ref.polygon_area(ref.square_polygon(centre, 0.5 * theta_arcsec * ARCSEC), centre)
    pixel = _sample(0, value_of(area), V0, centre, theta_arcsec)
    leaf = cell_of(centre)
    return Fixture(name, (pixel,), {leaf: cell_corners(leaf)},
                   covered_cells(pixel.drop_vertices, 0.5 * PIXFRAC * theta_arcsec * ARCSEC))


def _make_straddle_fixture() -> Fixture:
    """drop 跨两个相邻叶：drop 中心取两叶共边中点方向 ⇒ 独立认证必然给出两个叶。"""
    corners = cell_corners(LEAF_A)
    shared = ref.vec_normalise(tuple(corners[0][i] + corners[1][i] for i in range(3)))
    pixel = _sample(0, 100.0, V0, shared, THETA_ARCSEC)
    leaves = {LEAF_B: cell_corners(LEAF_B), LEAF_A: cell_corners(LEAF_A)}
    return Fixture("straddling-two-leaves", (pixel,), leaves,
                   covered_cells(pixel.drop_vertices, 0.5 * PIXFRAC * THETA_ARCSEC * ARCSEC))


def _make_high_coverage_fixture(coverage: float) -> Fixture:
    """目标叶被 drop 覆盖 `coverage` 比例的夹具（产品级 8bit 量化层）。

    drop 取目标叶四角弦多边形关于叶心的内缩同位似形 ⇒ 严格内含 ⇒ `a_jp = A_drop,j`，
    几何闭合逐位为 0。本条判据的对象是**产品级量化层**，与 drop 的来源无关。
    """
    corners = cell_corners(LEAF_A)
    centre = cell_centre(LEAF_A)
    eps = math.sqrt(coverage)
    inner = tuple(ref.vec_normalise(tuple(centre[i] + eps * (corners[k][i] - centre[i])
                                         for i in range(3))) for k in range(4))
    pixel = ref.SourcePixel.make(index=0, value=1.0, variance=V0,
                                 pixel_vertices=inner, drop_vertices=inner, reference=centre)
    return Fixture(f"leaf-covered-{coverage:.4f}", (pixel,), {LEAF_A: cell_corners(LEAF_A)},
                   covered_cells(inner, ref.max_angular_radius(inner, centre)))


def _pair_sample(index: int, value: float, theta_arcsec: float = THETA_PAIR_ARCSEC
                 ) -> ref.SourcePixel:
    """FIXTURE_PAIR 几何下的第 `index` 个样本，只改值（与可选的像元角尺度）。

    `theta_arcsec` 可调是为了让**被剔除样本的面积**与合格样本的面积在数值上明显不同，
    这样「被剔除样本的交叠面积不得进入 `D_p`」这条断言的读数才没有歧义。
    """
    return _sample(index, value, V0 * (1.0 + 0.5 * index), _pair_centres()[index],
                   theta_arcsec)


#: 常量面亮度夹具：`x_j = B₀·A_pixel,j`。
FIXTURE_SINGLE = _make_single_fixture("single-leaf", THETA_ARCSEC,
                                      lambda area: B0 * area)
#: 亚阈值覆盖夹具：`D_p/A_cell < 1/510` ⇒ `q = 0` ⇒ 产品级 `signal = ±Inf`。
FIXTURE_TINY = _make_single_fixture("single-leaf-sub-threshold-coverage", 2.0,
                                    lambda area: 1.0e-5)
FIXTURE_PAIR = _make_pair_fixture(lambda _k, area: B0 * area)
#: 等值双样本夹具：两个样本通量相等，用于守恒缺口读数。
FIXTURE_PAIR_EQUAL = _make_pair_fixture(lambda _k, _area: 100.0)
FIXTURE_STRADDLE = _make_straddle_fixture()


def _run(fixture: Fixture, defects: ref.Defects = ref.NO_DEFECTS,
         flux_scale: float = 1.0, variance_scale: float = 1.0):
    acc = ref.accumulate(fixture.pixels, fixture.leaves, defects=defects,
                         flux_scale=flux_scale, variance_scale=variance_scale)
    return acc, {ipix: ref.finalise(a, nside=NSIDE, defects=defects)
                 for ipix, a in acc.items()}


def _flux_sum_rel(products: Mapping[int, ref.LeafProduct], input_total: float) -> float:
    """求和型通量守恒读数 `|Σ_p F_p − Σ_j x_j| / Σ_j|x_j|`。"""
    return abs(ref.frame_flux_total(products.values()) - input_total) / abs(input_total)


def _l1_completeness(acc: Mapping[int, ref.LeafAccumulator],
                     pixels: Sequence[ref.SourcePixel]) -> float:
    """逐像元完备性读数 `max_j |Σ_p a_jp / A_drop,j − 1|`。

    分母 `A_drop,j` 是 drop 多边形**本身**实测的值（`DRIZZLE.md` §3.7 L1 右端），
    分子是被测实现累加的 `D_p`。被吞掉的叶没有累加量，天然计入面积亏损。
    """
    survived = [a for a in acc.values() if a.n_contrib > 0]
    if not survived:
        return float("nan")
    total_area = math.fsum(a.sum_area for a in survived)
    return max(abs(total_area / p.drop_area - 1.0) for p in pixels)


def _assert_geometry_closed(fixture: Fixture) -> None:
    """条件 A 的独立证书：全部 drop 的认证叶集合必须落在候选叶集合里。"""
    harness.is_true(set(fixture.certified_cells) <= set(fixture.leaves),
                    f"{fixture.name}: 独立认证的叶 {fixture.certified_cells} "
                    f"必须全在候选集合 {sorted(fixture.leaves)} 内")


# ===========================================================================
# A. S7 —— 通量守恒，但只在它成立的条件内写成测试
# ===========================================================================

@harness.test(
    "drizzle.s7.geometry-closed-single-leaf",
    intent="条件 A（几何闭合）成立时，逐叶全域求和闭合 `Σ_p Σ_j x_j w_jp = Σ_j x_j`。"
           "条件 A 由 astropy_healpix 独立认证（稠密采样确认 drop 整块落在同一叶内）"
           "并由被测实现实测 `Σ_p a_jp` 双重确认。",
    inputs=f"单叶夹具 FIXTURE_SINGLE：θ_j = {THETA_ARCSEC}″/px，pixfrac = {PIXFRAC}，"
           f"nside = {NSIDE}，`x_j = B₀·A_pixel,j`（B₀ = {B0:.3g} ADU/sr ⇒ Σ_j x_j ≈ "
           f"{FIXTURE_SINGLE.pixels[0].value:.4g} ADU），drop 认证叶 "
           f"{FIXTURE_SINGLE.certified_cells}",
    expected="求和型守恒残差 ≤ tolerances.DRIZZLE_FLUX_SUM_REL = 8.2e-15；"
             "逐像元完备性 ≤ tolerances.DRIZZLE_L1_COMPLETENESS = 6.6e-12；"
             "同一叶内 `Σ_p w_jp − 1` 逐位为 0（精确档 tolerances.DRIZZLE_WEIGHT_SUM_SAME_LEAF）",
    source="docs/science/drizzle/DRIZZLE.md §5.1「通量守恒门」「权重和门」、§3.5、§3.7 L1；"
           "叶面积闭式 A_cell = π/(3·nside²) 由 astropy_healpix.nside_to_pixel_area 独立互校",
    criteria=("S7", "G-CONS", "G-WEIGHT", "G-L1"),
)
def test_s7_geometry_closed_single_leaf():
    with harness.evidence() as ev:
        pixel = FIXTURE_SINGLE.pixels[0]
        _assert_geometry_closed(FIXTURE_SINGLE)
        harness.exact(len(FIXTURE_SINGLE.certified_cells), 1,
                      "条件 A 的独立证书：drop 整块必须落在同一叶内")

        acc, products = _run(FIXTURE_SINGLE)
        rel = _flux_sum_rel(products, pixel.value)
        l1 = _l1_completeness(acc, (pixel,))
        leaf = next(iter(acc))
        weight_sum = acc[leaf].sum_area / pixel.drop_area

        ev.record("Σ_j x_j", pixel.value, unit="ADU")
        ev.record("Σ_p Σ_j x_j w_jp", ref.frame_flux_total(products.values()), unit="ADU")
        ev.record("求和型守恒残差 rel", rel, tol.DRIZZLE_FLUX_SUM_REL.value)
        ev.record("逐像元完备性 |Σ_p a_jp/A_drop,j − 1|", l1,
                  tol.DRIZZLE_L1_COMPLETENESS.value)
        ev.record("同一叶内 Σ_p w_jp − 1", weight_sum - 1.0,
                  tol.DRIZZLE_WEIGHT_SUM_SAME_LEAF.value, note="精确档")
        ev.record("A_drop,j / (pixfrac²·A_pixel,j)",
                  pixel.drop_area / (PIXFRAC ** 2 * pixel.pixel_area),
                  note="切平面分支下本应 ≡ 1（见报告：L2 判别力的第二个真空来源）")
        ev.record("A_cell（闭式 vs astropy_healpix）",
                  (A_CELL, cell_area_oracle()), note="两引擎逐位一致")

        harness.close(A_CELL, cell_area_oracle(), rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * A_CELL, scale=A_CELL,
                      what="叶面积闭式必须与 astropy_healpix 独立引擎一致")
        harness.less_equal(rel, tol.DRIZZLE_FLUX_SUM_REL.value, "逐叶全域求和闭合")
        harness.less_equal(l1, tol.DRIZZLE_L1_COMPLETENESS.value, "逐像元完备性（L1 构造级）")
        harness.exact(weight_sum, 1.0, "同一叶内 drop 分割的权重和")


@harness.test(
    "drizzle.s7.geometry-closed-straddling-drop",
    intent="条件 A 在**跨叶**构造下同样成立：drop 被两个叶精确分完，逐叶权重和为 1，"
           "总量守恒闭合。证明守恒不依赖「单叶」这个额外假设。",
    inputs=f"跨叶夹具 FIXTURE_STRADDLE：θ_j = {THETA_ARCSEC}″/px，pixfrac = {PIXFRAC}，"
           f"drop 中心取叶 {LEAF_A} 与 {LEAF_B} 的共边中点方向；x_j = 100 ADU；"
           f"独立认证覆盖叶 = {FIXTURE_STRADDLE.certified_cells}",
    expected="认证恰好两个叶；Σ_p a_jp = A_drop,j（残差 ≤ 6.6e-12）；"
             "求和型守恒残差 ≤ 8.2e-15",
    source="docs/science/drizzle/DRIZZLE.md §3.2「`Σ_p w_jp = 1`」、§3.7 L1、§5.1「通量守恒门」",
    criteria=("S7", "G-CONS", "G-L1"),
)
def test_s7_geometry_closed_straddling():
    with harness.evidence() as ev:
        pixel = FIXTURE_STRADDLE.pixels[0]
        _assert_geometry_closed(FIXTURE_STRADDLE)
        harness.exact(len(FIXTURE_STRADDLE.certified_cells), 2,
                      "独立认证：跨叶 drop 必须恰好压在两个叶上")

        acc, products = _run(FIXTURE_STRADDLE)
        rel = _flux_sum_rel(products, pixel.value)
        l1 = _l1_completeness(acc, (pixel,))
        split = {ipix: acc[ipix].sum_area / pixel.drop_area for ipix in sorted(acc)}

        ev.record("A_drop,j", pixel.drop_area, unit="sr")
        ev.record("逐叶份额 w_jp", split, note=f"和 = {math.fsum(split.values())!r}")
        ev.record("求和型守恒残差 rel", rel, tol.DRIZZLE_FLUX_SUM_REL.value)
        ev.record("逐像元完备性", l1, tol.DRIZZLE_L1_COMPLETENESS.value)

        harness.less_equal(l1, tol.DRIZZLE_L1_COMPLETENESS.value, "跨叶 drop 的 L1 闭合")
        harness.less_equal(rel, tol.DRIZZLE_FLUX_SUM_REL.value, "跨叶逐叶全域求和闭合")


@harness.test(
    "drizzle.s7.neg-swallowed-leaf-breaks-closure",
    intent="条件 A **不**成立时守恒必须破裂：drop 跨叶但其中一个叶被候选枚举漏掉"
           "（`DRIZZLE.md` §3.7 具名的「面积亏损：叶被吞掉」），总量与 L1 完备性同时判红。"
           "这是守恒判据**不是恒真**的第一份证据。",
    inputs=f"FIXTURE_STRADDLE + 缺陷 `Defects(swallow_leaves=({LEAF_A},))`："
           f"叶 {LEAF_A} 不参与候选枚举，其 `a_jp` 面积静默丢失",
    expected="求和型守恒残差 ≈ 被吞叶的份额 ≈ 0.5，远超 8.2e-15；"
             "逐像元完备性 ≈ 0.5，远超 6.6e-12；两者都判红",
    source="docs/science/drizzle/DRIZZLE.md §3.7 L1「面积**亏损**（如叶边界被吞掉、无效交叠"
           "被剔除、跨面 drop 被 fail-closed 丢弃）分别具名判红」、§5.1「通量守恒门」",
    criteria=("S7", "G-CONS", "G-L1"),
    kind=harness.NEGATIVE,
    inject="候选枚举漏掉一个叶 ⇒ 该叶的交叠面积从 F_p/D_p/N_p/方差四项中一并消失",
    defect_id="DZ-GEO-SWALLOW",
)
def test_s7_neg_swallowed_leaf():
    with harness.evidence() as ev:
        pixel = FIXTURE_STRADDLE.pixels[0]
        defects = ref.Defects(swallow_leaves=(LEAF_A,))

        clean_acc, clean_products = _run(FIXTURE_STRADDLE)
        clean_rel = _flux_sum_rel(clean_products, pixel.value)
        clean_l1 = _l1_completeness(clean_acc, (pixel,))

        acc, products = _run(FIXTURE_STRADDLE, defects)
        rel = _flux_sum_rel(products, pixel.value)
        l1 = _l1_completeness(acc, (pixel,))
        lost_share = clean_acc[LEAF_A].sum_area / pixel.drop_area

        ev.record("对照（无注入）求和型残差", clean_rel, tol.DRIZZLE_FLUX_SUM_REL.value,
                  note="撤销注入必须变绿")
        ev.record("注入后求和型残差 rel", rel, tol.DRIZZLE_FLUX_SUM_REL.value)
        ev.record("对照（无注入）L1 完备性", clean_l1, tol.DRIZZLE_L1_COMPLETENESS.value)
        ev.record("注入后 L1 完备性", l1, tol.DRIZZLE_L1_COMPLETENESS.value)
        ev.record("被吞叶份额 a_jp/A_drop,j", lost_share,
                  note="闭式：守恒亏损恰等于该份额")

        harness.less_equal(clean_rel, tol.DRIZZLE_FLUX_SUM_REL.value, "撤销注入后必须绿")
        harness.less_equal(clean_l1, tol.DRIZZLE_L1_COMPLETENESS.value, "撤销注入后必须绿")
        harness.is_true(rel > tol.DRIZZLE_FLUX_SUM_REL.value,
                        "面积亏损必须让求和型守恒判红")
        harness.is_true(l1 > tol.DRIZZLE_L1_COMPLETENESS.value,
                        "面积亏损必须让 L1 完备性判红")


@harness.test(
    "drizzle.s7.neg-total-preserving-per-leaf-misallocation",
    intent="本单最重要的一条负例：`DRIZZLE.md` §5.1 逐字「求和型守恒门只证明总量守恒，"
           "对『总量不变但逐叶错注入』的缺陷没有判别力」。注入一个在总量上**恰好抵消**的"
           "逐叶偏差，断言**求和型判据仍然通过（绿）而逐叶判据变红** —— 证明"
           "「只测求和型」是无效的。",
    inputs="FIXTURE_STRADDLE + 缺陷 `Defects(allocation_bias=((LEAF_B, LEAF_A, +0.4·a_B),))`："
           "把 `+0.4·a` 的交叠面积从叶 B 互补地移到叶 A，Σ_p a_jp 不变",
    expected="求和型残差 ≤ 8.2e-15（**绿**：总量不变）；逐叶相对偏差 ≈ 0.4，"
             "远超逐叶门 tolerances.DRIZZLE_SURFACE_BRIGHTNESS_REL = 1e-3（**红**）；"
             "两个读数同时记入 evidence",
    source="docs/science/drizzle/DRIZZLE.md §5.1 末段逐字「仓内实验在恰保总量注入的成对构造下"
           "测得求和型判据多例精确为 `0`（双精度求和地板），而逐叶判据为 `O(0.3–0.9)`，"
           "两者分离 `≥ 14.88` 个数量级。因此通量守恒的判别力只能由逐叶判据提供」",
    criteria=("S7", "G-CONS"),
    kind=harness.NEGATIVE,
    inject="逐叶分配按互补量互移：总量守恒逐位成立、逐叶面亮度错 0.4",
    defect_id="DZ-FLUX-PERLEAF-SWAP",
)
def test_s7_neg_total_preserving_misallocation():
    with harness.evidence() as ev:
        pixel = FIXTURE_STRADDLE.pixels[0]
        a_b = overlaps_of(pixel, FIXTURE_STRADDLE.leaves)[LEAF_B]
        defects = ref.Defects(allocation_bias=((LEAF_B, LEAF_A, 0.4 * a_b),))

        _ca, clean = _run(FIXTURE_STRADDLE)
        _ia, injected = _run(FIXTURE_STRADDLE, defects)

        sum_clean = _flux_sum_rel(clean, pixel.value)
        sum_inj = _flux_sum_rel(injected, pixel.value)
        per_leaf = {ipix: abs(injected[ipix].flux / clean[ipix].flux - 1.0)
                    for ipix in sorted(clean)}
        worst = max(per_leaf.values())
        separation = math.log10(worst / sum_inj) if sum_inj > 0 else math.inf

        ev.record("对照（无注入）求和型残差", sum_clean, tol.DRIZZLE_FLUX_SUM_REL.value)
        ev.record("注入后求和型残差 rel", sum_inj, tol.DRIZZLE_FLUX_SUM_REL.value,
                  note="求和型判据仍然绿 —— 这正是它没有判别力的证据")
        ev.record("注入后逐叶相对偏差 max|F'_p/F_p − 1|", worst,
                  tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value, note="逐叶判据红")
        ev.record("逐叶逐项读数", per_leaf)
        ev.record("两判据分离（数量级）", separation, note="正本记载 ≥ 14.88")

        harness.less_equal(sum_inj, tol.DRIZZLE_FLUX_SUM_REL.value,
                           "求和型判据必须仍然通过（否则这条负例的前提不成立）")
        harness.is_true(worst > tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value,
                        "逐叶判据必须判红（否则求和型门就是唯一判据、且是无效的）")


@harness.test(
    "drizzle.s7.flux-conservation-factor-is-one",
    intent="溯源因子 `provenance.flux_conservation_factor` 恒为 1，使"
           "「输出总通量等于输入总通量」成为可被外部复核的声明。",
    inputs="全部夹具走 `NO_DEFECTS`；pixfrac = 0.8；该字段属元数据，落 tolerances.EXACT 精确档",
    expected="`flux_conservation_factor` 逐位等于 1.0",
    source="docs/science/drizzle/DRIZZLE.md §4 参数表逐字「`flux_conservation_factor` 恒为 `1` ……"
           "必须作为溯源项落盘」、§5.1「权重一致性门」；"
           "eng/contracts/data/clause_registry.json#FZ-COND-FLUX-CONSERV 逐字"
           "「缺 flux_conservation_factor 或取值 != 1 却用于绝对通量/孔径 → REJECT」",
    criteria=("S7",),
)
def test_s7_flux_conservation_factor_is_one():
    with harness.evidence() as ev:
        for fixture in (FIXTURE_SINGLE, FIXTURE_STRADDLE, FIXTURE_PAIR):
            _acc, products = _run(fixture)
            for ipix, p in sorted(products.items()):
                ev.record(f"[{fixture.name}] leaf {ipix} flux_conservation_factor",
                          p.flux_conservation_factor, tol.EXACT, note="元数据精确档")
                harness.exact(p.flux_conservation_factor, 1.0, "溯源因子必须恒为 1")


@harness.test(
    "drizzle.s7.neg-flux-conservation-factor-pixfrac-squared",
    intent="把溯源因子写成 `pixfrac²`。合同条款 `FZ-COND-FLUX-CONSERV` 明写该情形 REJECT；"
           "在 `pixfrac = 0.8` 下取值 0.64 ≠ 1，必须判红。",
    inputs=f"缺陷 `Defects(flux_conservation_factor={PIXFRAC}**2)` = {PIXFRAC ** 2!r}；"
           f"夹具 FIXTURE_SINGLE。**必须取 < 1 的 pixfrac**：pixfrac = 1 时 pixfrac² = 1，"
           "该退化与正确实现同值，负例退化为恒绿（tolerances.PIXFRAC_FREEZE_NOTE 逐字理由）",
    expected="因子读数 0.64，偏离 1 恰 0.36；精确档门限 0 ⇒ 判红",
    source="docs/science/drizzle/DRIZZLE.md §5.1「通量守恒门」红侧逐字「或把 "
           "`flux_conservation_factor` 写成 `pixfrac²`」；"
           "eng/contracts/data/clause_registry.json#FZ-COND-FLUX-CONSERV#fail_closed",
    criteria=("S7",),
    kind=harness.NEGATIVE,
    inject="flux_conservation_factor := pixfrac² = 0.64",
    defect_id="DZ-FCF-PIXFRAC2",
)
def test_s7_neg_flux_factor_pixfrac_squared():
    with harness.evidence() as ev:
        defects = ref.Defects(flux_conservation_factor=PIXFRAC ** 2)
        _acc, products = _run(FIXTURE_SINGLE, defects)
        factor = next(iter(products.values())).flux_conservation_factor
        deviation = abs(factor - 1.0)

        ev.record("注入的 flux_conservation_factor", factor, tol.EXACT,
                  note="期望 1.0；精确档门限 0 ⇒ 余量 inf")
        ev.record("|factor − 1|", deviation, tol.EXACT)
        ev.record("pixfrac² 闭式", PIXFRAC ** 2, note="DRIZZLE.md §3.4 逐字「压到 0.64」")

        harness.is_true(factor != 1.0, "因子必须不再是 1（负例的前提）")
        harness.is_true(deviation > tol.EXACT, "精确档门限 0 ⇒ 任何偏离都判红")


# ===========================================================================
# B. S8 —— 禁用把核权重 / 归一分母换成别的口径
# ===========================================================================

@harness.test(
    "drizzle.s8.positive-parameterisation-equivalence",
    intent="S8 负例的对照基线：`DRIZZLE.md` §3.3 逐字「`w'_jp = pixfrac²·w_jp`，因此它与 "
           "`w_jp` 只对 `c_jp`、`S_p`、`variance_p` 等价，对**通量泛函**不等价」。"
           "本条证明这个等价域成立，使 S8 的负例有确定的判红位置。",
    inputs=f"FIXTURE_SINGLE 跑两次：`NO_DEFECTS` 与 `Defects(kernel_denominator=\"pixel_area\")`；"
           f"pixfrac = {PIXFRAC}",
    expected="两种参数化给出**同一个** `c_jp`（逐位）、`S_p`（f64 非归约档）、"
             "`variance_p`（f64 非归约档）；而 `F_p` 相差 `pixfrac²` 倍",
    source="docs/science/drizzle/DRIZZLE.md §3.3 末段与 §3.6 逐字「参数化换成 "
           "`a_jp / A_pixel,j` 后 `pixfrac²` 在分子分母相消，逐位给出同一个 `variance_p`」",
    criteria=("S8", "G-VAR"),
)
def test_s8_positive_parameterisation_equivalence():
    with harness.evidence() as ev:
        pixel = FIXTURE_SINGLE.pixels[0]
        acc_a, prod_a = _run(FIXTURE_SINGLE)
        acc_b, prod_b = _run(FIXTURE_SINGLE, ref.Defects(kernel_denominator="pixel_area"))
        ipix = next(iter(acc_a))

        def coefficient(acc, _ipix):
            a_jp, ap, _x, _v, _j = acc[_ipix].raw[0]
            return ref.sb_coefficient(a_jp, ap, acc[_ipix].sum_area)

        c_a = coefficient(acc_a, ipix)
        c_b = coefficient(acc_b, ipix)

        ev.record("c_jp（w 参数化）", c_a, unit="sr⁻¹")
        ev.record("c_jp（w' 参数化）− c_jp（w 参数化）", c_b - c_a, tol.EXACT,
                  note="组合系数是唯一真正决定输出的量，必须逐位相同")
        ev.record("S_p 差", prod_b[ipix].signal - prod_a[ipix].signal,
                  tol.F64_ATOL_PER_SCALE * prod_a[ipix].signal)
        ev.record("variance_p 差", prod_b[ipix].variance - prod_a[ipix].variance,
                  tol.F64_ATOL_PER_SCALE * prod_a[ipix].variance)
        ev.record("F_p 比值（w'/w）", prod_b[ipix].flux / prod_a[ipix].flux,
                  note="通量泛函不等价 —— S8 负例的判红位置")
        ev.record("Σ_j x_j", pixel.value, unit="ADU")

        harness.exact(c_b, c_a, "c_jp 必须逐位相同")
        for label, actual, expected in (("S_p", prod_b[ipix].signal, prod_a[ipix].signal),
                                        ("variance_p", prod_b[ipix].variance, prod_a[ipix].variance)):
            scale = max(abs(expected), abs(actual))
            harness.close(actual, expected, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * scale, scale=scale, what=label)
        harness.close(prod_b[ipix].flux / prod_a[ipix].flux, PIXFRAC ** 2, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * PIXFRAC ** 2, scale=PIXFRAC ** 2,
                      what="通量泛函之比必须等于 pixfrac²")


@harness.test(
    "drizzle.s8.neg-kernel-denominator-pixel-area",
    intent="S8 的核心负例：把核权重的正向分母从 `A_drop,j` 换成 `A_pixel,j`"
           "（`w'_jp = a_jp/A_pixel,j`，配同型分母 `N'_p = D_p`）。闭式可验："
           "`Φ_out = pixfrac²·Σ_j x_j`，`pixfrac = 0.8` 给出 0.64。",
    inputs=f"FIXTURE_SINGLE + 缺陷 `Defects(kernel_denominator=\"pixel_area\")`；"
           f"**显式固定 pixfrac = {PIXFRAC}（tolerances.DRIZZLE_FIXTURE_PIXFRAC），"
           "不用任何生产默认**。T02 §6.2 C4 指出 pixfrac 默认值在仓内有 1.0 / 0.8 / "
           "pending_authority 三方冲突；pixfrac = 1 时 pixfrac² = 1，本负例与正确实现同值、"
           "退化为恒绿",
    expected="`Φ_out/Σ_j x_j` 闭式等于 `pixfrac²` = 0.64；守恒残差 0.36 "
             "远超 tolerances.DRIZZLE_FLUX_SUM_REL = 8.2e-15",
    source="docs/science/drizzle/DRIZZLE.md §3.4 逐字「若改取 `k = 1`，即 "
           "`w_jp = a_jp / A_pixel,j`，则 `Σ_p w_jp = pixfrac²`，总通量泛函被压低 `pixfrac²` 倍，"
           "`pixfrac = 0.8` 时压到 `0.64`」、§5.1「通量守恒门」红侧逐字",
    criteria=("S8", "G-CONS"),
    kind=harness.NEGATIVE,
    inject="核权重 w_jp := a_jp/A_pixel,j（分母从 drop 面积换成像元面积）",
    defect_id="DZ-KERNEL-DENOM-APIX",
)
def test_s8_neg_kernel_denominator_pixel_area():
    with harness.evidence() as ev:
        pixel = FIXTURE_SINGLE.pixels[0]
        _ca, clean = _run(FIXTURE_SINGLE)
        _ia, injected = _run(FIXTURE_SINGLE, ref.Defects(kernel_denominator="pixel_area"))

        ratio = ref.frame_flux_total(injected.values()) / pixel.value
        rel = abs(ratio - 1.0)
        clean_rel = _flux_sum_rel(clean, pixel.value)

        ev.record("对照（无注入）|Φ_out/Σ_j x_j − 1|", clean_rel,
                  tol.DRIZZLE_FLUX_SUM_REL.value, note="撤销注入必须变绿")
        ev.record("注入后 Φ_out/Σ_j x_j", ratio, note="闭式期望 pixfrac²")
        ev.record("|Φ_out/Σ_j x_j − 1|", rel, tol.DRIZZLE_FLUX_SUM_REL.value)
        ev.record("闭式 pixfrac²", PIXFRAC ** 2)
        ev.record("等效星等偏置 −2.5·log10(pixfrac²)", -2.5 * math.log10(PIXFRAC ** 2),
                  unit="mag", note="DRIZZLE.md §3.4 逐字 0.4846 mag")

        harness.close(ratio, PIXFRAC ** 2, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * PIXFRAC ** 2, scale=PIXFRAC ** 2,
                      what="Φ_out/Σ_j x_j 必须闭式等于 pixfrac²")
        harness.is_true(rel > tol.DRIZZLE_FLUX_SUM_REL.value,
                        "守恒残差必须越界（0.36 ≫ 8.2e-15）")


@harness.test(
    "drizzle.s8.neg-normaliser-coverage-area",
    intent="S8 的第二个负例：保留 drop 归一核但把面亮度归一分母换成覆盖面积 "
           "`D_p = Σ_j a_jp`（`DRIZZLE_GEOMETRY.md` DISP-DRZ-009 明令禁用的一支）。"
           "闭式：常量面亮度场被放大 `1/pixfrac²` 倍。",
    inputs=f"FIXTURE_SINGLE + 缺陷 `Defects(normaliser=\"coverage_area\")`；pixfrac = {PIXFRAC}",
    expected="`S_p/B₀` 闭式等于 `1/pixfrac²` = 1.5625；偏离 1 为 0.5625，"
             "远超 tolerances.DRIZZLE_SURFACE_BRIGHTNESS_REL = 1e-3",
    source="docs/science/drizzle/DRIZZLE.md §3.4 逐字「若改取分母为覆盖面积 `D_p = Σ_j a_jp`"
           "（与 drop 归一核配对），则 `S_p = B₀ / pixfrac²`，常量场被放大 `1/pixfrac²` 倍"
           "（`pixfrac = 0.8` 时为 `+56.25%`）」；docs/science/algorithms/DRIZZLE_GEOMETRY.md "
           "DISP-DRZ-009",
    criteria=("S8", "G-SB"),
    kind=harness.NEGATIVE,
    inject="面亮度归一分母 N_p := D_p（覆盖面积），核仍按 drop 面积归一",
    defect_id="DZ-NORMALISER-COVERAGE",
)
def test_s8_neg_normaliser_coverage_area():
    with harness.evidence() as ev:
        _ca, clean = _run(FIXTURE_SINGLE)
        _ia, injected = _run(FIXTURE_SINGLE, ref.Defects(normaliser="coverage_area"))
        ipix = next(iter(injected))

        ratio = injected[ipix].signal / B0
        deviation = abs(ratio - 1.0)

        ev.record("对照（无注入）|S_p/B₀ − 1|", abs(clean[ipix].signal / B0 - 1.0),
                  tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value, note="撤销注入必须变绿")
        ev.record("注入后 S_p/B₀", ratio, note="闭式期望 1/pixfrac²")
        ev.record("|S_p/B₀ − 1|", deviation, tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value)
        ev.record("闭式 1/pixfrac²", 1.0 / PIXFRAC ** 2)

        harness.close(ratio, 1.0 / PIXFRAC ** 2, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * (1.0 / PIXFRAC ** 2),
                      scale=1.0 / PIXFRAC ** 2, what="S_p/B₀ 必须闭式等于 1/pixfrac²")
        harness.is_true(deviation > tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value,
                        "常量面亮度门必须判红")


# ===========================================================================
# C. S9 —— NaN 处理口径（与 S7 同批）
# ===========================================================================

@harness.test(
    "drizzle.s9.positive-sample-mask-renormalisation",
    intent="样本级掩膜 + 重归一：不合格样本从分子、分母、方差三项中**一并**剔除，"
           "叶上的 `S_p` 仍等于由合格样本单独算出的值 —— 证明 NaN 没有经 `F_p` 传播。",
    inputs=f"FIXTURE_PAIR：样本 0 取 `x = NaN`、样本 1 为有限值；pixfrac = {PIXFRAC}；"
           f"期望值由闭式 `Σ_j B_j a_jp / Σ_j a_jp` 在**合格子集**上单独算出",
    expected="`S_p` 与合格子集闭式在 f64 非归约档内一致；`D_p` 不含被剔除样本的面积；"
             "`n_contrib` 逐位 = 合格样本数；`n_rejected_nonfinite` 逐位 = 1（精确档）",
    source="docs/science/drizzle/DRIZZLE.md §5.2「非有限样本」逐字「不合格样本从分子、分母、"
           "方差三项中一并剔除并重新归一」、§3.5 重建式；"
           "docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md 处置规则 1 逐字",
    criteria=("S9", "G-SB"),
)
def test_s9_positive_sample_mask_renormalisation():
    with harness.evidence() as ev:
        leaf = next(iter(FIXTURE_PAIR.leaves))
        good = FIXTURE_PAIR.pixels[1]
        samples = (_pair_sample(0, float("nan"), THETA_MASKED_ARCSEC), good)
        acc = ref.accumulate(samples, FIXTURE_PAIR.leaves)
        product = ref.finalise(acc[leaf], nside=NSIDE)
        raw = acc[leaf]

        expected_s = closed_form_sb([good.drop_area], [good.value], [good.pixel_area])
        expected_d = good.drop_area

        ev.record("合格样本数 n_contrib", product.n_contrib, tol.EXACT, note="精确档")
        ev.record("n_rejected_nonfinite", product.n_rejected_nonfinite, tol.EXACT, note="精确档")
        ev.record("按原因分类计数（值非有限 / 方差非有限 / 权重非正）",
                  (raw.rejected_nonfinite_value, raw.rejected_nonfinite_variance,
                   raw.rejected_nonpositive_weight), tol.EXACT,
                  note="互斥可加；本夹具只有「值非有限」可达")
        ev.record("S_p [ADU/sr]", product.signal, tol.F64_ATOL_PER_SCALE * expected_s)
        ev.record("闭式 S_p（仅合格样本）", expected_s)
        ev.record("D_p [sr]", product.covered_area, tol.F64_ATOL_PER_SCALE * expected_d)
        ev.record("被剔除样本的面积（不得进入 D_p）", samples[0].drop_area, unit="sr")

        harness.exact(product.n_contrib, 1, "合格样本计数")
        harness.exact(product.n_rejected_nonfinite, 1, "被剔除样本计数（强制计数）")
        harness.exact(raw.rejected_nonfinite_value, 1, "原因分类：值非有限")
        harness.exact(raw.rejected_nonfinite_value + raw.rejected_nonfinite_variance
                      + raw.rejected_nonpositive_weight, product.n_rejected_nonfinite,
                      "三类原因计数互斥可加，必须等于总计数")
        harness.is_true(math.isfinite(product.signal), "S_p 必须有限（NaN 不得进 F_p）")
        harness.close(product.signal, expected_s, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * expected_s, scale=expected_s,
                      what="S_p 必须等于合格子集单独算出的值")
        harness.close(product.covered_area, expected_d, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * expected_d, scale=expected_d,
                      what="D_p 必须只含合格样本的交叠面积")


@harness.test(
    "drizzle.s9.neg-nan-propagated-through-flux",
    intent="S9 的牙齿：实现成「NaN 经 `F_p` 传播」（不剔除、直接把 NaN 混进分子）。"
           "`DRIZZLE_GEOMETRY.md` DISP-DRZ-004 逐字「**禁用**把非有限样本传播进 "
           "`F_p`/分母/方差——会污染整像素信号与几何支撑」。",
    inputs="与上一条同一夹具，注入 `Defects(sample_qualification=\"none\")`：完全不做样本级掩膜",
    expected="`S_p` 变成非有限（NaN），与合格子集闭式不可能在容差内一致 ⇒ 判红。"
             "`TEST.md` §4.4 逐字：非有限值「要么位置精确一致、要么判错，不存在"
             "『落在容差内通过』的中间态」",
    source="docs/science/drizzle/DRIZZLE.md §5.2；docs/science/algorithms/DRIZZLE_GEOMETRY.md "
           "DISP-DRZ-004「禁用」项；docs/engineering/testing/TEST.md §4.4",
    criteria=("S9", "G-SB"),
    kind=harness.NEGATIVE,
    inject="取消样本级掩膜，NaN 直接进入 F_p / N_p / 方差三项",
    defect_id="DZ-NAN-PROPAGATE",
)
def test_s9_neg_nan_propagated_through_flux():
    with harness.evidence() as ev:
        leaf = next(iter(FIXTURE_PAIR.leaves))
        samples = (_pair_sample(0, float("nan"), THETA_MASKED_ARCSEC), FIXTURE_PAIR.pixels[1])

        clean = ref.finalise(ref.accumulate(samples, FIXTURE_PAIR.leaves)[leaf], nside=NSIDE)
        defects = ref.Defects(sample_qualification="none")
        injected = ref.finalise(
            ref.accumulate(samples, FIXTURE_PAIR.leaves, defects=defects)[leaf], nside=NSIDE)

        ev.record("对照 S_p", clean.signal, note="撤销注入必须变绿")
        ev.record("注入后 S_p", injected.signal,
                  note="TEST.md §4.4：无「落在容差内通过」的中间态 ⇒ 直接判错")
        ev.record("对照 / 注入后 n_contrib", (clean.n_contrib, injected.n_contrib), tol.EXACT)
        ev.record("对照 / 注入后 n_rejected_nonfinite",
                  (clean.n_rejected_nonfinite, injected.n_rejected_nonfinite), tol.EXACT)

        harness.is_true(math.isfinite(clean.signal), "撤销注入后 S_p 必须有限")
        harness.is_true(math.isnan(injected.signal),
                        "注入后 S_p 必须是 NaN（负例成立的前提）")


@harness.test(
    "drizzle.s9.positive-nonfinite-input-masked-and-counted",
    intent="±Inf **输入**按不合格样本掩膜并强制计数。合格样本的判定是 "
           "`isfinite(x_j)`（`PHASE_PRODUCT_EXCHANGE.md` 逐字），因此 `±Inf` 与 `NaN` 同族剔除。",
    inputs="FIXTURE_PAIR：样本 0 取 `x = +Inf`、样本 1 为有限值；"
           "期望值由闭式在合格子集上算出",
    expected="`S_p` 有限且等于合格子集闭式；`n_contrib = 1`；`n_rejected_nonfinite = 1`（精确档）",
    source="docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md「合格样本」逐字「值有限"
           "（`isfinite(x_j)`）且几何上覆盖该输出像素（`w_jp > 0`）」与处置规则 1/3；"
           "docs/science/drizzle/DRIZZLE.md §5.2「源像元值非有限时按样本级掩膜处理」",
    criteria=("S9",),
)
def test_s9_positive_nonfinite_input_masked_and_counted():
    with harness.evidence() as ev:
        leaf = next(iter(FIXTURE_PAIR.leaves))
        good = FIXTURE_PAIR.pixels[1]
        samples = (_pair_sample(0, float("inf"), THETA_MASKED_ARCSEC), good)
        acc = ref.accumulate(samples, FIXTURE_PAIR.leaves)
        product = ref.finalise(acc[leaf], nside=NSIDE)
        expected_s = closed_form_sb([good.drop_area], [good.value], [good.pixel_area])

        ev.record("S_p", product.signal, tol.F64_ATOL_PER_SCALE * expected_s)
        ev.record("n_contrib", product.n_contrib, tol.EXACT)
        ev.record("n_rejected_nonfinite", product.n_rejected_nonfinite, tol.EXACT)

        harness.is_true(math.isfinite(product.signal), "±Inf 不得传播进 S_p")
        harness.exact(product.n_contrib, 1, "合格样本计数")
        harness.exact(product.n_rejected_nonfinite, 1, "±Inf 必须被强制计数")
        harness.close(product.signal, expected_s, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * expected_s, scale=expected_s,
                      what="S_p 必须等于合格子集闭式")


@harness.test(
    "drizzle.s9.neg-inf-not-masked",
    intent="把 `±Inf` 当作有效样本放行进聚合（只剔 `NaN`）。这条负例与上一条同构，"
           "证明「非有限样本掩膜」这条判据对 `±Inf` 也有牙齿，而不是只对 `NaN` 成立。",
    inputs="同一夹具，注入 `Defects(sample_qualification=\"nan_only\")`：`NaN` 被剔、`±Inf` 被保留",
    expected="`S_p` 变成 `+Inf`（非有限）⇒ 判红；`n_contrib` 从 1 变 2",
    source="docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md「合格样本」`isfinite(x_j)`；"
           "docs/science/drizzle/DRIZZLE.md §5.2",
    criteria=("S9",),
    kind=harness.NEGATIVE,
    inject="只对 NaN 做样本级掩膜，+Inf 被当作有效样本参与聚合",
    defect_id="DZ-INF-KEEP",
)
def test_s9_neg_inf_not_masked():
    with harness.evidence() as ev:
        leaf = next(iter(FIXTURE_PAIR.leaves))
        samples = (_pair_sample(0, float("inf"), THETA_MASKED_ARCSEC), FIXTURE_PAIR.pixels[1])

        clean = ref.finalise(ref.accumulate(samples, FIXTURE_PAIR.leaves)[leaf], nside=NSIDE)
        defects = ref.Defects(sample_qualification="nan_only")
        injected = ref.finalise(
            ref.accumulate(samples, FIXTURE_PAIR.leaves, defects=defects)[leaf], nside=NSIDE)

        ev.record("对照 S_p", clean.signal, note="撤销注入必须变绿")
        ev.record("注入后 S_p", injected.signal, note="TEST.md §4.4：非有限即判错")
        ev.record("对照 / 注入后 n_contrib", (clean.n_contrib, injected.n_contrib), tol.EXACT)

        harness.is_true(math.isfinite(clean.signal), "撤销注入后必须绿")
        harness.is_true(injected.signal == float("inf"),
                        "注入后 S_p 必须是 +Inf（负例成立的前提）")


@harness.test(
    "drizzle.s9.neg-output-zero-and-inf-treated-as-invalid",
    intent="`DRIZZLE.md` §5.2 逐字「`NaN` 是无效的唯一表示，`0` 与 `±Inf` 一律读作有效数值」。"
           "本条把这句话放在**输出侧**（它的适用域）做成负例：把 `0` / `±Inf` 的输出判为无效，"
           "必须判红。`±Inf` 状态不是人为捏造的：`D_p/A_cell < 1/510` 时 "
           "`q = lround(255·clamp(D_p/A_cell,0,1)) = 0`，`covered_area = 0` 使产品级 "
           "`signal = F_p·k/covered_area = ±Inf`（IEEE 754）。",
    inputs="三个产品状态：(1) FIXTURE_TINY 的亚阈值叶（q = 0 ⇒ signal = +Inf、support > 0）；"
           "(2) 零通量叶（x_j = 0 ⇒ signal = 0、support > 0）；"
           "(3) FIXTURE_SINGLE 的正常叶（signal 有限、support > 0）。"
           "注入 `Defects(validity_predicate=\"nonfinite_or_zero\")`",
    expected="正本判定下三个状态**全部有效**（无效只在 `NaN ∧ support ≤ 0`）；"
             "注入判定下 (1)(2) 被误判为无效 ⇒ 判红",
    source="docs/science/drizzle/DRIZZLE.md §5.2 逐字；"
           "docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md「无效输出 = `signal = NaN` **且** "
           "`support ≤ 0`；两者**必须同时**成立（互推）」与处置规则 2 逐字「NaN 是**无效的唯一"
           "表示**；`0`、`±Inf` 或任意哨兵值一律不表示无效」；docs/engineering/testing/TEST.md §4.4",
    criteria=("S9",),
    kind=harness.NEGATIVE,
    inject="有效性谓词把 0 / ±Inf 当作无效（正本只承认 NaN ∧ support≤0）",
    defect_id="DZ-VALIDITY-ZERO-INF",
)
def test_s9_neg_output_zero_and_inf_treated_as_invalid():
    with harness.evidence() as ev:
        zero_pixel = _sample(0, 0.0, V0, direction(LON, LAT), THETA_ARCSEC)
        zero_leaf = next(iter(FIXTURE_SINGLE.leaves))
        tiny_leaf = next(iter(FIXTURE_TINY.leaves))
        normal_leaf = next(iter(FIXTURE_SINGLE.leaves))

        tiny = ref.finalise(ref.accumulate(FIXTURE_TINY.pixels, FIXTURE_TINY.leaves)[tiny_leaf],
                            nside=NSIDE)
        zero = ref.finalise(ref.accumulate([zero_pixel], FIXTURE_SINGLE.leaves)[zero_leaf],
                            nside=NSIDE)
        normal = ref.finalise(ref.accumulate(FIXTURE_SINGLE.pixels,
                                             FIXTURE_SINGLE.leaves)[normal_leaf], nside=NSIDE)

        states = (("亚阈值覆盖叶 q=0", ref.product_signal(tiny), tiny.support),
                  ("零通量叶", ref.product_signal(zero), zero.support),
                  ("正常叶", ref.product_signal(normal), normal.support))
        defects = ref.Defects(validity_predicate="nonfinite_or_zero")
        wrongly_invalid = [n for n, s, p in states if ref.is_invalid_output(s, p, defects)]
        truly_invalid = [n for n, s, p in states if ref.is_invalid_output(s, p)]

        for name, signal, support in states:
            ev.record(f"[{name}] signal / support", (signal, support),
                      note=f"正本判定 无效={name in truly_invalid}；"
                           f"注入判定 无效={name in wrongly_invalid}")
        ev.record("亚阈值叶 q", tiny.q, note=f"D_p/A_cell = "
                                            f"{tiny.covered_area / A_CELL:.3e}")
        ev.record("被误判为无效的状态", wrongly_invalid)

        harness.exact(len(truly_invalid), 0, "正本判定下三个状态全部有效")
        harness.is_true(tiny.q == 0 and tiny.support > 0.0 and not math.isfinite(ref.product_signal(tiny)),
                        "±Inf 状态必须真的由 q=0 的覆盖面积产生（不是人为捏造）")
        harness.is_true(len(wrongly_invalid) >= 2, "注入判定必须把 0 / ±Inf 误判为无效")


@harness.test(
    "drizzle.s9.positive-zero-eligible-nan-and-support-zero",
    intent="零合格样本 ⇒ `NaN` 且 `support = 0`；并且被剔除样本的计数必须与 `support` "
           "**可区分**：零剔除的叶必须显式给出计数 0（「字段缺失」是第三态，"
           "不得由计数 0 或 `NaN` 兼表）。",
    inputs=f"两叶场景：叶 A 有一个候选样本且其为 `NaN`（有覆盖、零合格）；"
           f"叶 {LEAF_EMPTY} 没有任何交叠（无覆盖）。nside = {NSIDE}",
    expected="两叶 `S_p` 均 NaN、`support` 均 0；叶 A 的 `n_rejected_nonfinite` 逐位 = 1，"
             "叶 B 逐位 = 0 且字段在场（与 A 的 1 可区分）",
    source="docs/science/drizzle/DRIZZLE.md §5.2 逐字「仅当零合格样本时输出 `NaN` 且 "
           "`support = 0`」「每个输出像元必须暴露被剔除样本的计数，计数为 0 与『字段缺失』"
           "必须可区分」；docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md 处置规则 2/3 与"
           "「与两类输入的对应」表",
    criteria=("S9",),
)
def test_s9_positive_zero_eligible_nan_and_support_zero():
    with harness.evidence() as ev:
        covered_leaf = next(iter(FIXTURE_PAIR.leaves))
        leaves = dict(FIXTURE_PAIR.leaves)
        leaves[LEAF_EMPTY] = cell_corners(LEAF_EMPTY)
        samples = (_pair_sample(0, float("nan"), THETA_MASKED_ARCSEC),)

        acc = ref.accumulate(samples, leaves)
        zero_eligible = ref.finalise(acc[covered_leaf], nside=NSIDE)
        no_coverage = ref.finalise(acc[LEAF_EMPTY], nside=NSIDE)

        ev.record("零合格叶 signal / support", (zero_eligible.signal, zero_eligible.support),
                  tol.EXACT)
        ev.record("零合格叶 n_rejected_nonfinite", zero_eligible.n_rejected_nonfinite,
                  tol.DRIZZLE_NAN_SUPPORT_EXACT.value, note="精确档：期望 1")
        ev.record("无覆盖叶 signal / support", (no_coverage.signal, no_coverage.support), tol.EXACT)
        ev.record("无覆盖叶 n_rejected_nonfinite", no_coverage.n_rejected_nonfinite,
                  tol.DRIZZLE_NAN_SUPPORT_EXACT.value, note="精确档：期望 0")
        ev.record("无覆盖叶 D_p [sr]", no_coverage.covered_area)

        harness.is_true(math.isnan(zero_eligible.signal) and zero_eligible.support == 0.0,
                        "零合格样本必须 NaN ∧ support=0")
        harness.is_true(math.isnan(no_coverage.signal) and no_coverage.support == 0.0,
                        "无覆盖必须 NaN ∧ support=0")
        harness.exact(no_coverage.covered_area, 0.0, "无覆盖叶的 D_p 必须为 0")
        harness.exact(zero_eligible.n_rejected_nonfinite, 1, "零合格叶的被剔除样本计数（精确档）")
        harness.exact(no_coverage.n_rejected_nonfinite, 0, "无覆盖叶的被剔除样本计数（精确档）")
        harness.is_true(zero_eligible.n_rejected_nonfinite != no_coverage.n_rejected_nonfinite,
                        "「有覆盖但全坏」与「无覆盖」在诊断层必须可区分")


@harness.test(
    "drizzle.s9.neg-reject-count-field-suppressed-when-zero",
    intent="计数为 0 时不落 `n_rejected_nonfinite` 字段，使「0」与「字段缺失」混同 —— "
           "违反 `DRIZZLE.md` §5.2 与 `PHASE_PRODUCT_EXCHANGE.md` 处置规则 3 逐字"
           "「计数为 0 与**字段缺失**必须可区分」。",
    inputs="零剔除的叶（计数真值 = 0）+ 注入 `Defects(reject_count_suppressed=True)`",
    expected="正本：计数字段在场且逐位为 0；注入：字段变成缺失（None）⇒ 判红",
    source="docs/science/drizzle/DRIZZLE.md §5.2；"
           "docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md 处置规则 3 与缺陷名 "
           "`COUNT_FIELD_MISSING`",
    criteria=("S9",),
    kind=harness.NEGATIVE,
    inject="n_rejected_nonfinite == 0 时把该字段置为缺失",
    defect_id="DZ-NREJ-FIELD-MISSING",
)
def test_s9_neg_reject_count_field_suppressed():
    with harness.evidence() as ev:
        leaf = next(iter(FIXTURE_SINGLE.leaves))
        clean = ref.finalise(ref.accumulate(FIXTURE_SINGLE.pixels, FIXTURE_SINGLE.leaves)[leaf],
                             nside=NSIDE)
        defects = ref.Defects(reject_count_suppressed=True)
        injected = ref.finalise(
            ref.accumulate(FIXTURE_SINGLE.pixels, FIXTURE_SINGLE.leaves,
                           defects=defects)[leaf], nside=NSIDE)

        ev.record("对照 n_rejected_nonfinite", clean.n_rejected_nonfinite, tol.EXACT)
        ev.record("注入后 n_rejected_nonfinite", injected.n_rejected_nonfinite, tol.EXACT,
                  note="None = 字段缺失，与计数 0 不可区分")

        harness.exact(clean.n_rejected_nonfinite, 0, "正本：计数 0 必须显式落盘")
        harness.is_true(injected.n_rejected_nonfinite is None, "注入后字段必须缺失")


@harness.test(
    "drizzle.s7x9.conditional-conservation-under-mask",
    intent="S7 × S9 交互：证明守恒断言**只能是条件式**。在含 NaN 样本的夹具上，"
           "条件 C（掩膜后合格子集上的守恒）成立，而**无条件**的严格守恒"
           "（对整帧全部输入通量求和）被实测证伪。这是 `T02` §6.2 C5 的直接复现。",
    inputs="FIXTURE_PAIR_EQUAL：两个样本通量相等（各 100 ADU）、同落一个叶。"
           "条件 A 由 L1 读数给出；条件 B 由逐叶 `n_rejected_nonfinite` 逐位相等给出",
    expected="条件 C：`Σ_p Σ_{j∈Ω} x_j w_jp = Σ_{j∈Ω} x_j` 在 8.2e-15 内；"
             "无条件严格守恒（对整帧 `Σ_j x_j = 200 ADU`）的缺口 = 被剔除样本占比 = 0.5，"
             "远超 8.2e-15 ⇒ 无条件断言必然误红，守恒必须写成条件式",
    source="run/GOVERN-08/审核包-R2/T02-门禁退役与判据清单.md §6.2 C5 逐字「通量守恒"
           "『严格不变量』已被对抗审核反例推翻……该恒等式只在 drop 几何闭合时成立；"
           "§8 的 NaN 样本掩膜路径会从 `F_p`/分母/方差中一并剔除并重归一，故严格守恒在"
           "该路径下破裂」；docs/science/drizzle/DRIZZLE.md §3.7 自带「（几何闭合时）」限定",
    criteria=("S7", "S9", "G-CONS"),
)
def test_s7x9_conditional_conservation_under_mask():
    with harness.evidence() as ev:
        good = FIXTURE_PAIR_EQUAL.pixels[1]
        masked = _pair_sample(0, float("nan"), THETA_MASKED_ARCSEC)
        # 整帧物理输入通量：同一几何、两个样本都合格时的 Σ_j x_j
        _full_acc, full_products = _run(FIXTURE_PAIR_EQUAL)
        full_input = math.fsum(p.value for p in FIXTURE_PAIR_EQUAL.pixels)

        acc = ref.accumulate((masked, good), FIXTURE_PAIR_EQUAL.leaves)
        products = {ipix: ref.finalise(a, nside=NSIDE) for ipix, a in acc.items()}

        # 条件 A：几何闭合（只对合格样本核）
        l1 = _l1_completeness(acc, (good,))
        # 条件 B：掩膜是样本级而非逐叶
        counts = tuple(products[ipix].n_rejected_nonfinite for ipix in sorted(products))
        # 条件 C：掩膜后合格子集上的守恒
        qualified_total = math.fsum(p.value for p in (masked, good) if math.isfinite(p.value))
        conditional_rel = _flux_sum_rel(products, qualified_total)
        # 无条件严格守恒：拿整帧物理输入通量作分母，缺口 = 被剔除样本的占比
        strict_rel = abs(ref.frame_flux_total(products.values()) - full_input) / full_input
        full_rel = _flux_sum_rel(full_products, full_input)

        ev.record("整帧输入 Σ_j x_j（全合格时）", full_input, unit="ADU")
        ev.record("对照（无 NaN）守恒残差", full_rel, tol.DRIZZLE_FLUX_SUM_REL.value,
                  note="撤销注入必须变绿")
        ev.record("条件 A：L1 完备性（合格样本）", l1, tol.DRIZZLE_L1_COMPLETENESS.value)
        ev.record("条件 B：逐叶 n_rejected_nonfinite", counts, tol.EXACT)
        ev.record("条件 C：合格子集 Σ_{j∈Ω} x_j", qualified_total, unit="ADU")
        ev.record("条件 C：掩膜后子集守恒残差", conditional_rel, tol.DRIZZLE_FLUX_SUM_REL.value,
                  note="S7 的断言必须写成这一条")
        ev.record("无条件严格守恒残差（对整帧 Σ_j x_j）", strict_rel,
                  tol.DRIZZLE_FLUX_SUM_REL.value,
                  note="≡ 被剔除样本的通量占比 ⇒ 写成无条件断言就是必然误红的判据")

        harness.less_equal(full_rel, tol.DRIZZLE_FLUX_SUM_REL.value, "撤销注入后必须绿")
        harness.less_equal(l1, tol.DRIZZLE_L1_COMPLETENESS.value, "条件 A 必须成立")
        harness.is_true(len(set(counts)) == 1 and counts[0] == 1,
                        "条件 B 必须成立：掩膜是样本级的（各叶剔除计数相同且非零）")
        harness.less_equal(conditional_rel, tol.DRIZZLE_FLUX_SUM_REL.value,
                           "条件 C：掩膜后子集上的守恒必须闭合")
        harness.close(strict_rel, 0.5, rtol=tol.F64_RTOL, atol=tol.F64_ATOL_PER_SCALE * 0.5,
                      scale=0.5,
                      what="无条件严格守恒的缺口必须闭式等于被剔除样本的通量占比 0.5")


# ===========================================================================
# D. 辅助判据（常量面亮度 / 产品级量化 / 方差 / 联合缩放律 / 协方差）
# ===========================================================================

@harness.test(
    "drizzle.aux.positive-constant-surface-brightness",
    intent="不变量 2（`DRIZZLE.md` §1）：常数**面亮度**场 `B(Ω) = B₀` 的输入，输出恰为 `B₀`，"
           "与 `pixfrac` 无关。判在 `S_p = F_p/N_p` 上。",
    inputs=f"FIXTURE_SINGLE 与 FIXTURE_PAIR，构造 `x_j = B₀·A_pixel,j`（B₀ = {B0:.3g} ADU/sr）；"
           f"pixfrac = {PIXFRAC}；nside = {NSIDE}",
    expected="全部有覆盖叶 `|S_p/B₀ − 1| < 1e-3`"
             "（tolerances.DRIZZLE_SURFACE_BRIGHTNESS_REL）",
    source="docs/science/drizzle/DRIZZLE.md §1 不变量 2、§5.1「常量面亮度门（累加器级）」"
           "逐字「按 `x_j = B₀·A_pixel,j` 构造输入，断言 `|S_p/B₀ − 1| < 1e-3` 对全部"
           "有覆盖叶成立」",
    criteria=("S7", "G-SB"),
)
def test_aux_positive_constant_surface_brightness():
    with harness.evidence() as ev:
        for fixture in (FIXTURE_SINGLE, FIXTURE_PAIR):
            _acc, products = _run(fixture)
            for ipix, p in sorted(products.items()):
                ratio = p.signal / B0
                ev.record(f"[{fixture.name}] leaf {ipix} |S_p/B₀ − 1|", abs(ratio - 1.0),
                          tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value)
                harness.less_equal(abs(ratio - 1.0), tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value,
                                   f"{fixture.name} 叶 {ipix} 的常量面亮度门")


@harness.test(
    "drizzle.aux.neg-constant-adu-fixture",
    intent="按**每像元常量 ADU** 构造输入（而非常量面亮度），即把 `B₀` 当成通量直接写进 "
           "`x_j`。`DRIZZLE.md` §5.1 点名该构造为常量面亮度门的红侧。",
    inputs=f"与上一条同一几何，构造 `x_j = B₀ = {B0:.3g}`（单位 ADU，不乘 `A_pixel,j`）；"
           f"pixfrac = {PIXFRAC}",
    expected="`S_p/B₀` 闭式 = `1/A_pixel,j`（单像元叶），偏离 1 达 `1e7` 量级，"
             "远超 tolerances.DRIZZLE_SURFACE_BRIGHTNESS_REL = 1e-3",
    source="docs/science/drizzle/DRIZZLE.md §5.1「常量面亮度门（累加器级）」红侧逐字"
           "「红：按每像元常量 ADU 构造、或漏掉面亮度归一分母」；§3.5 逐字 "
           "`S_p = Σ_j B_j a_jp / Σ_j a_jp`",
    criteria=("S7", "G-SB"),
    kind=harness.NEGATIVE,
    inject="输入按每像元常量 ADU 构造（漏掉 x_j = B₀·A_pixel,j 的 A_pixel 因子）",
    defect_id="DZ-FIXTURE-CONSTANT-ADU",
)
def test_aux_neg_constant_adu_fixture():
    with harness.evidence() as ev:
        centre = direction(LON, LAT)
        adu_pixel = _sample(0, B0, V0, centre, THETA_ARCSEC)
        acc = ref.accumulate([adu_pixel], FIXTURE_SINGLE.leaves)
        product = ref.finalise(acc[next(iter(acc))], nside=NSIDE)
        expected_ratio = 1.0 / FIXTURE_SINGLE.pixels[0].pixel_area
        ratio = product.signal / B0
        deviation = abs(ratio - 1.0)

        ev.record("对照（常量面亮度构造）|S_p/B₀ − 1|", 0.0,
                  tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value, note="撤销构造错误必须变绿")
        ev.record("注入后 S_p/B₀", ratio, note="闭式 1/A_pixel,j")
        ev.record("闭式 1/A_pixel,j", expected_ratio, unit="sr⁻¹")
        ev.record("|S_p/B₀ − 1|", deviation, tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value)

        harness.close(ratio, expected_ratio, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * expected_ratio, scale=expected_ratio,
                      what="S_p/B₀ 必须闭式等于 1/A_pixel,j")
        harness.is_true(deviation > tol.DRIZZLE_SURFACE_BRIGHTNESS_REL.value,
                        "常量面亮度门必须判红")


@harness.test(
    "drizzle.aux.positive-product-signal-quantisation-budget",
    intent="产品级 signal 的容差由 8bit 覆盖面积量化预算决定："
           "`q = lround(255·clamp(D_p/A_cell,0,1))`、`covered_area = (q/255)·A_cell`，"
           "故 `|signal/S_p − 1| ≤ 0.5/q`。判在真实几何产生的两个覆盖档上。",
    inputs="两个夹具：(1) 覆盖 99.85% 的叶（`q = 255`，全覆盖档）；"
           "(2) FIXTURE_SINGLE 的单叶（`q = 3`，深欠覆盖档，容差随 q 放大到 0.5/q）",
    expected="两档的 `|signal/S_p − 1|` 都 ≤ `0.5/q`；全覆盖档 `0.5/255 = 1.96e-3` "
             "（tolerances.DRIZZLE_PRODUCT_SIGNAL_REL_FULL）",
    source="docs/science/drizzle/DRIZZLE.md §5.1「常量面亮度门（产品级）」逐字"
           "「`q = lround(255·clamp(D_p/A_cell,0,1))`、`covered_area = (q/255)·A_cell`，"
           "故 `|signal/S_p − 1| ≤ 0.5/q`，全覆盖叶 `q = 255` 时为 `1.96e-3`」",
    criteria=("S7", "G-PROD"),
)
def test_aux_positive_product_signal_budget():
    with harness.evidence() as ev:
        for fixture, label in ((_make_high_coverage_fixture(0.9985), "全覆盖档"),
                               (FIXTURE_SINGLE, "深欠覆盖档")):
            _acc, products = _run(fixture)
            for ipix, p in sorted(products.items()):
                signal = ref.product_signal(p)
                deviation = abs(signal / p.signal - 1.0)
                budget = 0.5 / p.q if p.q else math.inf
                ev.record(f"[{label}] 覆盖 D_p/A_cell", p.covered_area / A_CELL)
                ev.record(f"[{label}] q", p.q)
                ev.record(f"[{label}] |signal/S_p − 1|", deviation, budget)
                harness.less_equal(deviation, budget, f"{label} 的 0.5/q 预算")
                if p.q == 255:
                    harness.less_equal(deviation, tol.DRIZZLE_PRODUCT_SIGNAL_REL_FULL.value,
                                       "全覆盖叶的产品级量化预算（tolerances 冻结档）")


@harness.test(
    "drizzle.aux.neg-product-signal-gate-1em3",
    intent="用 `1e-3` 卡产品级 signal。`DRIZZLE.md` §5.1 逐字「红：任何以 `1e-3` 卡产品级 "
           "`signal` 的实现（即使全覆盖也吃满 `0.196%`，必然判红）」。本条**实测**该必然性："
           "取一个覆盖 99.85% 的叶（`q = 255`、量化中点邻域），其真实偏差落在 "
           "`(1e-3, 0.5/255]` 区间内。",
    inputs=f"夹具 `_make_high_coverage_fixture(0.9985)`（drop 严格内含于叶 {LEAF_A}，"
           f"`D_p/A_cell ≈ 0.9985`，`q = lround(255·0.9985) = 255`）；"
           f"对照门 = 1e-3，预算门 = tolerances.DRIZZLE_PRODUCT_SIGNAL_REL_FULL = 0.5/255",
    expected="实测 `|signal/S_p − 1| ≈ 1.5e-3`：`> 1e-3`（对照门必红）且 `≤ 0.5/255`"
             "（正本预算门必须绿）",
    source="docs/science/drizzle/DRIZZLE.md §5.1「常量面亮度门（产品级）」红侧逐字",
    criteria=("S7", "G-PROD"),
    kind=harness.NEGATIVE,
    inject="以 1e-3 作为产品级 signal 的验收门（无视 8bit 量化预算 0.5/q）",
    defect_id="DZ-PRODUCT-SIGNAL-1EM3",
)
def test_aux_neg_product_signal_gate_1em3():
    with harness.evidence() as ev:
        _acc, products = _run(_make_high_coverage_fixture(0.9985))
        product = next(iter(products.values()))
        deviation = abs(ref.product_signal(product) / product.signal - 1.0)
        illegal_gate = 1e-3
        budget = tol.DRIZZLE_PRODUCT_SIGNAL_REL_FULL.value

        ev.record("D_p/A_cell", product.covered_area / A_CELL)
        ev.record("q", product.q, note="全覆盖档")
        ev.record("|signal/S_p − 1|", deviation, illegal_gate, note="对照门 1e-3")
        ev.record("正本预算门 0.5/q", budget,
                  note=f"对照门 1e-3 超界 {harness.margin(deviation, illegal_gate):.3g}×")
        ev.record("超界倍数", harness.margin(deviation, illegal_gate))

        harness.is_true(product.q == 255, "必须落在全覆盖档 q = 255（闭式前提）")
        harness.is_true(deviation > illegal_gate, "1e-3 门必须判红（正本「必然判红」）")
        harness.less_equal(deviation, budget, "正本预算门必须仍然通过（否则夹具取错了档）")


@harness.test(
    "drizzle.aux.positive-variance-identity-from-raw",
    intent="方差恒等式 `variance_p = Σ_j c_jp² v_j`，且由 raw `(a_jp, A_pixel,j, D_p)` "
           "**重算**而非读取已计算量（`DRIZZLE.md` §7.3 逐字「避免同实现自证」）。",
    inputs=f"FIXTURE_PAIR（同一叶两个样本，`v_j` 取不同值）与 FIXTURE_STRADDLE（跨叶）；"
           f"期望值由闭式 `c_jp = a_jp/(A_pixel,j·D_p)` 在 oracle 侧独立算出",
    expected="被测实现的 `variance_p` 与闭式在 f64 非归约档内一致",
    source="docs/science/drizzle/DRIZZLE.md §3.6 逐字「`variance_p = Σ_j c_jp² v_j`」、"
           "§5.1「方差恒等判据」逐字「由 raw `(a_jp, A_pixel,j, D_p)` 重算而非读取已计算量」、"
           "§7.3 `reference_sb_coefficient`",
    criteria=("S7", "G-VAR"),
)
def test_aux_positive_variance_identity_from_raw():
    with harness.evidence() as ev:
        for fixture in (FIXTURE_PAIR, FIXTURE_STRADDLE):
            acc, products = _run(fixture)
            for ipix, p in sorted(products.items()):
                raw = acc[ipix].raw
                expected = closed_form_variance(raw, p.covered_area)
                ev.record(f"[{fixture.name}] leaf {ipix} variance_p", p.variance,
                          tol.F64_ATOL_PER_SCALE * expected, note="ADU²/sr²")
                ev.record(f"[{fixture.name}] leaf {ipix} 闭式 Σ c_jp² v_j", expected)
                harness.close(p.variance, expected, rtol=tol.F64_RTOL,
                              atol=tol.F64_ATOL_PER_SCALE * expected, scale=expected,
                              what=f"{fixture.name} 叶 {ipix} 的方差恒等式")


def _variance_negative(defects: ref.Defects) -> Tuple[ref.LeafProduct, ref.LeafProduct, int]:
    _ca, clean = _run(FIXTURE_SINGLE)
    _ia, injected = _run(FIXTURE_SINGLE, defects)
    ipix = next(iter(injected))
    return clean[ipix], injected[ipix], ipix


@harness.test(
    "drizzle.aux.neg-variance-missing-square",
    intent="方差项漏一次平方（`Σ c_jp v_j` 而非 `Σ c_jp² v_j`）。"
           "`DRIZZLE.md` §3.6 逐字「若方差项少一次平方……量纲与标度都不成立」。",
    inputs="FIXTURE_SINGLE + 缺陷 `Defects(variance_form=\"missing_square\")`",
    expected="注入后 `variance_p` 偏离闭式 `Σ c_jp² v_j`，偏离倍数由 `c_jp` 量级决定，"
             "远超 f64 非归约档",
    source="docs/science/drizzle/DRIZZLE.md §5.1「方差恒等判据」红侧逐字「红：漏平方」；§3.6",
    criteria=("S7", "G-VAR"),
    kind=harness.NEGATIVE,
    inject="variance_p := Σ_j c_jp·v_j（漏一次平方）",
    defect_id="DZ-VARIANCE-MISSING-SQUARE",
)
def test_aux_neg_variance_missing_square():
    with harness.evidence() as ev:
        clean, injected, ipix = _variance_negative(
            ref.Defects(variance_form="missing_square"))
        c = ref.sb_coefficient(FIXTURE_SINGLE.pixels[0].drop_area,
                               FIXTURE_SINGLE.pixels[0].pixel_area,
                               clean.covered_area)
        ratio = injected.variance / clean.variance
        ev.record("对照 variance_p", clean.variance, note="撤销注入必须变绿")
        ev.record("注入后 variance_p", injected.variance)
        ev.record("c_jp [sr⁻¹]", c, note="漏平方的比值闭式 ≡ 1/c_jp")
        ev.record("注入/闭式 比值", ratio, tol.F64_RTOL, note=f"闭式 1/c_jp = {1.0 / c!r}")
        ev.record("|比值 − 1|", abs(ratio - 1.0), tol.F64_RTOL, note="f64 非归约档")
        ev.record("超界倍数", harness.margin(abs(ratio - 1.0), tol.F64_RTOL))

        harness.close(ratio, 1.0 / c, rtol=tol.F64_RTOL, atol=tol.F64_ATOL_PER_SCALE / c,
                      scale=1.0 / c, what="比值必须闭式等于 1/c_jp")
        harness.is_true(abs(ratio - 1.0) > tol.F64_RTOL, "漏平方必须判红")
        harness.is_true(ipix in FIXTURE_SINGLE.leaves, "夹具叶必须存在")


@harness.test(
    "drizzle.aux.neg-variance-missing-normaliser",
    intent="方差项漏 `N_p²`（写成 `Σ_j v_j w_jp² / N_p`）。"
           "`DRIZZLE.md` §3.6 逐字「若方差项少一次平方或漏掉 `N_p²`，量纲与标度都不成立」。",
    inputs="FIXTURE_SINGLE + 缺陷 `Defects(variance_form=\"missing_normaliser\")`",
    expected="注入后 `variance_p` 与闭式的比值 ≡ `N_p`（一个 sr 量级的小数），"
             "相对偏离 ≈ 1，远超 f64 非归约档",
    source="docs/science/drizzle/DRIZZLE.md §3.6、§5.1「方差恒等判据」红侧逐字「漏 `N²`」",
    criteria=("S7", "G-VAR"),
    kind=harness.NEGATIVE,
    inject="variance_p := Σ_j v_j w_jp² / N_p（漏一次 N_p²）",
    defect_id="DZ-VARIANCE-MISSING-N",
)
def test_aux_neg_variance_missing_normaliser():
    with harness.evidence() as ev:
        clean, injected, _ipix = _variance_negative(
            ref.Defects(variance_form="missing_normaliser"))
        ratio = injected.variance / clean.variance
        ev.record("N_p [sr]", clean.normaliser)
        ev.record("对照 variance_p", clean.variance, note="撤销注入必须变绿")
        ev.record("注入后 variance_p", injected.variance)
        ev.record("注入/闭式 比值（闭式期望 N_p）", ratio, tol.F64_RTOL)
        ev.record("|比值 − 1|", abs(ratio - 1.0), tol.F64_RTOL)
        ev.record("超界倍数", harness.margin(abs(ratio - 1.0), tol.F64_RTOL))

        harness.close(ratio, clean.normaliser, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * clean.normaliser,
                      scale=clean.normaliser, what="比值必须闭式等于 N_p")
        harness.is_true(abs(ratio - 1.0) > tol.F64_RTOL, "漏 N_p² 必须判红")


@harness.test(
    "drizzle.aux.neg-variance-kernel-square-denominator",
    intent="把产品域方差误写成累加器域的通量方差 `Σ_j v_j w_jp² / D_p²`（漏发布因子 "
           "`k²`）。`DRIZZLE.md` §5.1 逐字红侧「或改用核权重的平方做分母」。",
    inputs=f"FIXTURE_SINGLE + 缺陷 `Defects(variance_form=\"kernel_square_denominator\")`；"
           f"pixfrac = {PIXFRAC}",
    expected=f"注入后 `variance_p` 与闭式的比值 ≡ `pixfrac⁻⁴` = {PIXFRAC ** -4:.6f}，"
             "相对偏离 ≈ 1.44 ≫ f64 非归约档",
    source="docs/science/drizzle/DRIZZLE.md §5.1「方差恒等判据」红侧逐字；§7.3 逐字"
           "「发布因子 `k = D_p/N_p`，signal 乘 `k`、variance 乘 `k²`」",
    criteria=("S7", "G-VAR"),
    kind=harness.NEGATIVE,
    inject="variance_p := Σ_j v_j w_jp² / D_p²（漏发布因子 k²）",
    defect_id="DZ-VARIANCE-KERNEL-DENOM",
)
def test_aux_neg_variance_kernel_square_denominator():
    with harness.evidence() as ev:
        clean, injected, _ipix = _variance_negative(
            ref.Defects(variance_form="kernel_square_denominator"))
        ratio = injected.variance / clean.variance
        ev.record("对照 variance_p", clean.variance, note="撤销注入必须变绿")
        ev.record("注入后 variance_p", injected.variance)
        ev.record("注入/闭式 比值（闭式期望 pixfrac⁻⁴）", ratio, tol.F64_RTOL,
                  note=f"闭式 {PIXFRAC ** -4!r}")
        ev.record("|比值 − 1|", abs(ratio - 1.0), tol.F64_RTOL)
        ev.record("超界倍数", harness.margin(abs(ratio - 1.0), tol.F64_RTOL))

        harness.close(ratio, PIXFRAC ** -4, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * PIXFRAC ** -4,
                      scale=PIXFRAC ** -4, what="比值必须闭式等于 pixfrac⁻⁴")
        harness.is_true(abs(ratio - 1.0) > tol.F64_RTOL, "该缺陷必须判红")


@harness.test(
    "drizzle.aux.positive-joint-scaling-law",
    intent="联合缩放律：对 `(x, v)` 作**联合**缩放 `(x, v) → (α·x, α²·v)` 时 "
           "`variance_p → α²·variance_p`、`ivar_p → ivar_p/α²`。"
           "`DRIZZLE.md` §3.6 逐字两条式子必须同步。",
    inputs=f"FIXTURE_PAIR，α = {ALPHA_SCALE}（缩放经 `accumulate(flux_scale=α, "
           f"variance_scale=α²)` 进入输入侧）",
    expected=f"`variance` 比值 = α² = {ALPHA_SCALE ** 2} 且 `ivar` 比值 = "
             f"α⁻² = {ALPHA_SCALE ** -2:.6f}，在 f64 非归约档内一致",
    source="docs/science/drizzle/DRIZZLE.md §3.6 逐字「对 `(x, v)` 作**联合**缩放 "
           "`(x, v) → (α·x, α²·v)` 时 `variance_p → α²·variance_p`、`ivar_p → ivar_p/α²`」；"
           "§5.1「联合缩放律门」",
    criteria=("S7", "G-SCALE"),
)
def test_aux_positive_joint_scaling_law():
    with harness.evidence() as ev:
        _ba, base = _run(FIXTURE_PAIR)
        _sa, scaled = _run(FIXTURE_PAIR, flux_scale=ALPHA_SCALE,
                           variance_scale=ALPHA_SCALE ** 2)
        for ipix in sorted(base):
            v_ratio = scaled[ipix].variance / base[ipix].variance
            ivar_ratio = scaled[ipix].ivar / base[ipix].ivar
            ev.record(f"[leaf {ipix}] variance 比值（期望 α²）", v_ratio,
                      tol.F64_ATOL_PER_SCALE * ALPHA_SCALE ** 2)
            ev.record(f"[leaf {ipix}] ivar 比值（期望 α⁻²）", ivar_ratio,
                      tol.F64_ATOL_PER_SCALE * ALPHA_SCALE ** -2)
            harness.close(v_ratio, ALPHA_SCALE ** 2, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * ALPHA_SCALE ** 2,
                          scale=ALPHA_SCALE ** 2, what="联合缩放律：variance → α²")
            harness.close(ivar_ratio, ALPHA_SCALE ** -2, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * ALPHA_SCALE ** -2,
                          scale=ALPHA_SCALE ** -2, what="联合缩放律：ivar → α⁻²")


@harness.test(
    "drizzle.aux.neg-joint-scaling-law-broken",
    intent="破坏联合缩放律（把方差当均值除以样本数，破坏对 `v_j` 的一次齐次性）。"
           "`DRIZZLE.md` §5.1 逐字「红：任何破坏线性性的注入」。",
    inputs=f"FIXTURE_PAIR + 缺陷 `Defects(variance_form=\"mean_over_samples\")`，α = {ALPHA_SCALE}",
    expected=f"注入后 `variance` 比值 ≡ `1`（对 α² 缩放不变），与闭式 `α²` 的相对偏离 "
             f"= `1 − α⁻²` = {1 - ALPHA_SCALE ** -2:.4f} ≫ f64 非归约档",
    source="docs/science/drizzle/DRIZZLE.md §5.1「联合缩放律门」红侧逐字；§3.6",
    criteria=("S7", "G-SCALE"),
    kind=harness.NEGATIVE,
    inject="variance_p := (Σ_j c_jp² v_j)/n_contrib（当成均值，破坏齐次性）",
    defect_id="DZ-VARIANCE-MEAN-OVER-SAMPLES",
)
def test_aux_neg_joint_scaling_law_broken():
    with harness.evidence() as ev:
        defects = ref.Defects(variance_form="mean_over_samples")
        _ba, base = _run(FIXTURE_PAIR)
        _sa, scaled = _run(FIXTURE_PAIR, defects, flux_scale=ALPHA_SCALE,
                           variance_scale=ALPHA_SCALE ** 2)
        ipix = next(iter(scaled))
        ratio = scaled[ipix].variance / base[ipix].variance
        deviation = abs(ratio - ALPHA_SCALE ** 2)

        ev.record("对照（无注入）variance 比值", ALPHA_SCALE ** 2, note="撤销注入必须变绿")
        ev.record("注入后 variance 比值（闭式期望 α²）", ratio, tol.F64_RTOL)
        ev.record("|比值 − α²|", deviation, tol.F64_RTOL)
        ev.record("n_contrib", scaled[ipix].n_contrib, tol.EXACT,
                  note="缺陷只在多样本叶上可见")

        harness.is_true(scaled[ipix].n_contrib >= 2, "负例必须在多样本叶上运行")
        harness.is_true(deviation > tol.F64_RTOL, "联合缩放律门必须判红")


@harness.test(
    "drizzle.aux.positive-aperture-exact-gt-diag",
    intent="协方差门的**条件**不变量：孔径精确方差不小于对角归约，且在 `a_p ≥ 0`、"
           "`c_jp ≥ 0` 且存在非对角贡献时**严格**大于。同时显式登记容差地板，"
           "证明本条不是一条在低量级上恒绿的门。",
    inputs=f"FIXTURE_STRADDLE（一个源样本、两个叶、`c_jp ≥ 0`、`a_p = [1, 1]`、"
           f"`v_j = {V0:.0f}` ADU²）；地板按正本公式 `tol = diag_rel_tol·max(|exact|,|diag|,1.0)`，"
           "`diag_rel_tol = 1e-11`",
    expected="`exact > diag` 且 `exact − diag` 闭式等于 `2·v·c_1·c_2`；"
             "`exact − diag` 相对容差地板的余量 ≫ 1（不在地板上判绿）",
    source="docs/science/drizzle/DRIZZLE.md §3.6 逐字「`Var( Σ_p a_p S_p ) = Σ_{p,q} a_p a_q "
           "Cov(S_p,S_q) = Σ_j v_j ( Σ_p a_p c_jp )²`」「只用对角元给出的 `Σ_p a_p² variance_p` "
           "是这个量的**下界，条件是孔径权重 `a_p ≥ 0` 且 `c_jp ≥ 0`**」、"
           "§5.1「协方差门」与「⚠ 协方差门的容差存在地板」",
    criteria=("S7", "G-COV"),
)
def test_aux_positive_aperture_exact_gt_diag():
    with harness.evidence() as ev:
        acc, _products = _run(FIXTURE_STRADDLE)
        aperture = {ipix: 1.0 for ipix in acc}
        exact, diag, deficit = ref.aperture_variance(acc, aperture)

        c = {ipix: ref.sb_coefficient(acc[ipix].raw[0][0], acc[ipix].raw[0][1],
                                      acc[ipix].sum_area) for ipix in acc}
        cov = ref.covariance(acc, LEAF_B, LEAF_A)
        closed_exact = V0 * (math.fsum(c.values()) ** 2)
        closed_gap = 2.0 * V0 * c[LEAF_B] * c[LEAF_A]
        floor = 1e-11 * max(abs(exact), abs(diag), 1.0)

        ev.record("c_jp", c, unit="sr⁻¹")
        ev.record("Cov(S_p,S_q) [ADU²/sr²]", cov, tol.F64_ATOL_PER_SCALE * abs(cov),
                  note="闭式 v·c_1·c_2；exact = variance_p + 2·Cov + … 的交叉项来源")
        ev.record("exact", exact, note="ADU²")
        ev.record("diag", diag, note="ADU²")
        ev.record("exact − diag", exact - diag)
        ev.record("闭式 2·v·c_1·c_2", closed_gap)
        ev.record("deficit = (exact−diag)/exact", deficit)
        ev.record("容差地板 diag_rel_tol·max(|exact|,|diag|,1)", floor)
        ev.record("(exact−diag)/地板（余量倍数）", (exact - diag) / floor)

        harness.close(cov, V0 * c[LEAF_B] * c[LEAF_A], rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * abs(cov), scale=abs(cov),
                      what="相邻目标像元的噪声协方差必须非零（正本 §3.6：p≠q 时一般非零）")
        harness.close(exact, closed_exact, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * closed_exact, scale=closed_exact,
                      what="孔径精确方差的闭式")
        harness.close(exact - diag, closed_gap, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * closed_gap, scale=closed_gap,
                      what="exact − diag 必须闭式等于 2·v·c_1·c_2")
        harness.is_true(all(v >= 0.0 for v in c.values()), "条件：c_jp ≥ 0")
        harness.is_true(all(v >= 0.0 for v in aperture.values()), "条件：a_p ≥ 0")
        harness.is_true(exact > diag, "存在非对角贡献时必须严格大于")
        harness.is_true((exact - diag) > floor,
                        "判读必须离开容差地板（否则这条门在低量级恒绿）")


@harness.test(
    "drizzle.aux.positive-signed-aperture-counterexample",
    intent="复现 `DRIZZLE.md` §3.6 **自带**的反例：带号孔径权重使 `exact < diag`。"
           "这条把「孔径精确方差不小于对角归约」这条**严格不变量推翻**的落法固定下来："
           "它只在下界方向成立，带号时反向。",
    inputs="正本逐字给出的反例数据：`a_p = [1, −1]`、`c_jp = [0.6, 0.4]`、`v_j = [1]`",
    expected="`exact = (0.6 − 0.4)² = 0.04`，`diag = 0.6² + 0.4² = 0.52`，`exact < diag`",
    source="docs/science/drizzle/DRIZZLE.md §3.6 逐字「展开式 `exact − diag = Σ_j v_j · 2 Σ_{p<q} "
           "a_p a_q c_jp c_jq` 的每个配对项符号由 `sign(a_p·a_q)` 决定，故带号的 `a_p` 会使该差"
           "为负（反例：`a_p = [1, −1]`、`c_jp = [0.6, 0.4]`、`v_j = [1]` 给出 "
           "`exact = 0.04 < diag = 0.52`）」",
    criteria=("S7", "G-COV"),
)
def test_aux_positive_signed_aperture_counterexample():
    with harness.evidence() as ev:
        a_p = (1.0, -1.0)
        c_jp = (0.6, 0.4)
        v_j = 1.0
        exact = v_j * (a_p[0] * c_jp[0] + a_p[1] * c_jp[1]) ** 2
        diag = v_j * sum((a_p[i] * c_jp[i]) ** 2 for i in (0, 1))

        ev.record("a_p", a_p)
        ev.record("c_jp", c_jp)
        ev.record("v_j", v_j)
        ev.record("exact", exact, note="正本反例读数 0.04")
        ev.record("diag", diag, note="正本反例读数 0.52")
        ev.record("exact − diag", exact - diag)

        harness.close(exact, 0.04, rtol=tol.F64_RTOL, atol=tol.F64_ATOL_PER_SCALE * 0.04,
                      scale=0.04, what="exact")
        harness.close(diag, 0.52, rtol=tol.F64_RTOL, atol=tol.F64_ATOL_PER_SCALE * 0.52,
                      scale=0.52, what="diag")
        harness.is_true(exact < diag, "带号孔径权重下「下界」方向反转 —— 严格不变量在此被推翻")


@harness.test(
    "drizzle.aux.neg-declare-diagonal-reduction-exact",
    intent="把对角归约 `Σ_p a_p² variance_p` 声明为孔径方差的**精确值**。"
           "`DRIZZLE.md` §5.1「协方差门」红侧逐字「红：声明对角归约为精确值」。",
    inputs="FIXTURE_STRADDLE + 缺陷 `Defects(aperture_variance_form=\"diagonal\")`",
    expected="注入后 `exact` 与真值的偏离 = 跨项亏损 `exact − diag`，远超容差地板",
    source="docs/science/drizzle/DRIZZLE.md §5.1「协方差门」红侧逐字「红：声明对角归约为精确值」；"
           "§3.6 逐字「粗化到父级网格时同理：父级方差的对角归约只能声明为下界……"
           "声明为精确值被拒」",
    criteria=("S7", "G-COV"),
    kind=harness.NEGATIVE,
    inject="aperture variance := 对角归约 Σ_p a_p² variance_p（声明为精确值）",
    defect_id="DZ-COVARIANCE-DIAGONAL-AS-EXACT",
)
def test_aux_neg_declare_diagonal_reduction_exact():
    with harness.evidence() as ev:
        acc, _products = _run(FIXTURE_STRADDLE)
        aperture = {ipix: 1.0 for ipix in acc}
        exact, diag, deficit = ref.aperture_variance(acc, aperture)
        defects = ref.Defects(aperture_variance_form="diagonal")
        inj_exact, inj_diag, inj_deficit = ref.aperture_variance(acc, aperture, defects)

        floor = 1e-11 * max(abs(exact), abs(diag), 1.0)
        deviation = abs(inj_exact - exact)

        ev.record("对照 exact", exact)
        ev.record("对照 diag", diag, note="真值下界")
        ev.record("注入后 'exact'（= 对角归约）", inj_exact)
        ev.record("|'exact' 声明值 − 真 exact|", deviation, floor)
        ev.record("对照 deficit", deficit)
        ev.record("注入后 deficit", inj_deficit, note="注入后亏损被声明为 0")
        ev.record("容差地板", floor)

        harness.is_true(exact > diag, "对照必须满足条件不变量")
        harness.is_true(deviation > floor, "声明对角归约为精确值必须判红")
        harness.exact(inj_deficit, 0.0, "注入后亏损量被抹平（亏损量随声明一起丢失）")
