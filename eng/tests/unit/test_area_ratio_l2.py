"""面积比级闭合 L2 —— 一条**已知无判别力**的门，它的「无判别力」本身被写成测试。

`docs/science/drizzle/DRIZZLE.md` §3.7 把几何闭合成两级：

- **L1 构造级**：`Σ_p a_jp = A_drop,j`（主判据，构造性，见 `test_drizzle_conservation.py`）；
- **L2 面积比级**：`δ ≡ A_drop,j / (pixfrac²·A_pixel,j) − 1`，闭式 `δ = (1−pixfrac²)·θ_j²/4 + O(θ_j⁴)`。

正本 §3.7 明写 L2 的限定：「**当 `A_pixel,j` 是由 `drop_area / pixfrac²` 反推而来时，
L2 是代数真空、没有证据资格**：此时 `pixfrac²·A_pixel,j` 按定义恒等于 `drop_area`，
比值恒为 `1`，δ 恒等于 `0`，该级门永远无法发现 `A_drop,j ≠ pixfrac²·A_pixel,j` 的那一支」。

⚠️ **本文件要验的不是「L2 通过」，而是「L2 在什么条件下有判别力、在什么条件下没有」**。
把一条恒绿的门写成正例会制造假证据（`TEST.md` §2 逐字「恒真的比较没有证据资格」），
所以这里写成三条：

1. **正例**：在 `A_pixel,j` 由**未收缩四角独立实测**的路径上，实测 δ 与闭式一致；
2. **负例（真空来源一）**：切到**反推路径**（`A_pixel := A_drop/pixfrac²`），
   δ 必须**恒等于 0** ⇒ 判据给不出任何非零读数 ⇒ 判别力归零（这是缺陷形态，必须判红）；
3. **负例（真空来源二 · 本轮新查出）**：即便在独立实测路径上，**切平面面积分支**
   使 `A_drop = 4(pixfrac·a)²`、`A_pixel = 4a²` 对**任何** θ_j 都成立 ⇒ δ 仍恒为 0。
   正本 §3.7 只警告了反推那一支，**没有**警告这一支。

⇒ 第 3 条是本文件的**主要产出**：它把「L2 在生产实现下**整条无判别力**」这件事
从口头判断变成可复算的读数，交给裁决（见报告 §11 第 3 条）。
"""

from __future__ import annotations

import math

from . import drizzle_ref as ref
from . import harness, tolerances as tol

#: 本文件专用的相对残差判据：δ 的实测值与闭式值的差。
#: ⚠ 门限**取自冻结表** `tolerances.DRIZZLE_AREA_RATIO_DELTA` 的同一档位口径
#: （`TEST.md` §4 双精度非归约档 `rtol = 1e-12`），不另立。
_RTOL = tol.F64_RTOL
_ATOL = tol.F64_ATOL_PER_SCALE

ARCSEC = math.pi / (180.0 * 3600.0)
_PIXFRACS = (0.4, 0.6, 0.8, 1.0)

#: `drizzle_ref.polygon_area` 的分支阈值（`DRIZZLE.md` §3.7 逐字 `max_angle < 1e-3` rad）。
#: 正方形的外接半径 = 半宽 · √2 ⇒ 进入**球面立体角分支**需要半宽 ≥ 1e-3/√2，
#: 即像元角尺度 θ ≳ 2.83e-4 rad ≈ **292 ″/px**。低于该尺度一律走切平面二维面积。
BRANCH_MAX_ANGLE_RAD = 1e-3
THETA_SPHERICAL_ARCSEC = 292.0   # 刚过分支界
THETA_TANGENT_ARCSEC = (7.0, 20.0, 60.0, 200.0)   # 一律落在切平面分支


def _centre(lon_deg: float = 0.0, lat_deg: float = 0.3) -> ref.Vec:
    """单位方向向量（与 `test_drizzle_conservation.py:138` 的 `direction` 同一构造，
    在本文件内独立实现，不从测试文件互相 import）。"""
    lon, lat = math.radians(lon_deg), math.radians(lat_deg)
    return (math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon),
            math.sin(lat))


def _tangent_plane_area(theta_arcsec: float, pixfrac: float, centre):
    """**强制**走切平面二维面积分支时的像元足迹面积与 drop 足迹面积。

    与 `drizzle_ref.polygon_area` 的区别：后者按 `max_angular_radius < 1e-3` **自动选支**，
    在本文件用到的尺度上它自己就落到切平面支 —— 那不构成对照。
    这里取纯二维面积：把以 `centre` 为中心、半宽 `half` 的正方形投到切平面上，
    它的面积恒为 `(2·half)²`（gnomonic 投影把平面上的直线映成大圆，
    反投影回来就是原来的正方形）。于是

        A_drop = (2·pf·h)² = pf²·(2h)² = pf²·A_pixel   ⇒ δ ≡ 0

    **对任何 θ_j 精确成立** ⇒ L2 在该分支下没有任何判别力（真空来源二）。
    """
    del centre                      # 面积与中心方向无关
    half_pixel = 0.5 * theta_arcsec * ARCSEC
    return (2.0 * half_pixel) ** 2, (2.0 * pixfrac * half_pixel) ** 2


def _delta(a_drop: float, a_pixel: float, pixfrac: float) -> float:
    """`δ ≡ A_drop / (pixfrac²·A_pixel) − 1`。"""
    return a_drop / (pixfrac * pixfrac * a_pixel) - 1.0


def _closed_form(pixfrac: float, theta_arcsec: float) -> float:
    """闭式 `(1 − pixfrac²)·θ_j²/4 + O(θ_j⁴)`（`DRIZZLE.md` §3.7 / §5.2）。"""
    return tol.DRIZZLE_AREA_RATIO_DELTA.value(pixfrac, theta_arcsec * ARCSEC)


# ---------------------------------------------------------------------------
# 1. 「正例」侧**没有写成通过** —— 如实登记闭式复算不成立
# ---------------------------------------------------------------------------
# 本层原打算写一条正例：在球面分支上断言 `δ_measured == (1−pixfrac²)·θ²/4`。
# **复算不成立**，逐点读数如下（在库 `drizzle_ref.polygon_area` 的球面立体角分支上实测）：
#
#   θ=292″/px  pixfrac=0.8 : 实测 5.010204e-07   闭式 1.803674e-07   相对差 1.78×
#   θ=1000″/px pixfrac=0.8 : 实测 2.115387e-06   闭式 2.115399e-06   相对差 6e-06  ✓
#   θ=4000″/px pixfrac=0.8 : 实测 3.384347e-05   闭式 3.384638e-05   相对差 9e-05  ✓
#
# ⇒ 闭式在 θ ≥ 1000″ 上吻合（O(θ⁴) 余项），但在**刚跨过分支界**的 θ=292″ 上差 1.78 倍。
# 另外正本 §5.2 自己的三个示例（θ=2″ ⇒ 8.46e-12、10″ ⇒ 2.12e-10、60″ ⇒ 7.62e-09）
# 与同节的闭式 `(1−pixfrac²)·θ_j²/4` **对不上**（反解出的 θ 与给出的 θ 差 √2 量级）。
#
# ⇒ **处置**：不写「闭式吻合」的正例。写了就是迁就一个我复算不上的式子。
#    本文件因此只有两条负例 —— 它们要守的性质是**「L2 在两种路径下都恒零、
#    没有判别力」**，这个性质是可复算的，且已被实测证实。
#    闭式本身的复核问题登记在报告 §11 待裁决表，不在本层强行判红也不强行判绿。

# ---------------------------------------------------------------------------
# 2. 负例 · 真空来源一：由 drop_area/pixfrac² 反推 A_pixel（正本点名的路径）
# ---------------------------------------------------------------------------

@harness.test(
    "l2.negative.back_derived_pixel_area_is_algebraic_vacuum",
    intent="L2 负例：`A_pixel := A_drop/pixfrac²` 的反推路径必须被判为**无判别力**",
    inputs="同一批几何，改走反推路径求 A_pixel，再算 δ",
    expected="δ 在全部 θ_j 与 pixfrac 上**逐位为 0**（判别力归零）；"
             "反推路径与非反推路径的 δ 之差必须非零（否则这条负例无效）",
    source="docs/science/drizzle/DRIZZLE.md §3.7 逐字「当 `A_pixel,j` 是由 "
           "`drop_area / pixfrac²` 反推而来时，L2 是代数真空、没有证据资格……"
           "该级门永远无法发现 `A_drop,j ≠ pixfrac²·A_pixel,j` 的那一支」；"
           "同节另逐字「生产门路径当前正处于这种反推形态，因此那条门实际只在执行 L1」",
    criteria=("S8",),
    kind=harness.NEGATIVE,
    inject="DEF-L2-BACKDERIVED-A_PIXEL：把 A_pixel,j 改由 A_drop,j/pixfrac² 反推",
    defect_id="DZ-L2-BACKDERIVED",
)
def test_l2_negative_back_derived_pixel_area_is_algebraic_vacuum():
    centre = _centre()
    measured_independent, measured_backderived = [], []
    for theta in (THETA_SPHERICAL_ARCSEC, 1000.0, 4000.0):
        for pf in _PIXFRACS:
            a_pixel = ref.polygon_area(ref.square_polygon(centre, 0.5 * theta * ARCSEC),
                                       centre)
            a_drop = ref.polygon_area(ref.square_polygon(centre, 0.5 * pf * theta * ARCSEC),
                                      centre)
            measured_independent.append(_delta(a_drop, a_pixel, pf))
            # ← 注入：反推路径
            a_pixel_back = a_drop / (pf * pf)
            measured_backderived.append(_delta(a_drop, a_pixel_back, pf))
    with harness.evidence() as ev:
        ev.record("反推路径 δ 的最大绝对值（须 ≤ 1e-12）",
                  max(abs(d) for d in measured_backderived))
        ev.record("独立实测路径 δ 的最大绝对值（必须 > 0）",
                  max(abs(d) for d in measured_independent))
        ev.record("两组 δ 的最大差（证明注入确实改变了结论）",
                  max(abs(a - b) for a, b in
                      zip(measured_independent, measured_backderived)))
    # 反推路径是**代数恒等**：δ 的残差只可能是浮点舍入，用 f64 档判而不是精确档
    for d in measured_backderived:
        harness.close(d, 0.0, rtol=0.0, atol=tol.F64_RTOL,
                      what="反推路径下 δ 必须是浮点零（代数真空）")
    harness.is_true(max(abs(d) for d in measured_independent) > 0.0,
                    "独立实测路径的 δ 也恒为 0 ⇒ 两条路径无差别，注入无效")


# ---------------------------------------------------------------------------
# 3. 负例 · 真空来源二：切平面分支本身使 δ ≡ 0（本轮新查出，正本未登记）
# ---------------------------------------------------------------------------

@harness.test(
    "l2.negative.tangent_plane_branch_is_a_second_vacuum_source",
    intent="L2 负例：切平面二维面积分支给 A_drop=(pf·a)²·4、A_pixel=a²·4 ⇒ δ ≡ 0 ⇒ "
           "L2 在该分支下整条无判别力；并测出进入球面分支所需的 θ_j 下限",
    inputs="θ_j ∈ {7, 20, 60, 200, 292}″/px 跨越 `max_angle < 1e-3` rad 的分支界",
    expected="θ_j < 292″/px（切平面分支）时 δ 恒为数值零 ⇒ 无判别力；"
             "θ_j ≥ 292″/px（球面分支）时 δ 非零 ⇒ 判别力只存在于这一侧",
    source="docs/science/drizzle/DRIZZLE.md §3.7 逐字「交叠面积按 drop 的最大角半径分两支："
           "角跨度小于 `10⁻³` rad 的极小多边形走切平面二维面积……其余走球面立体角分支」，"
           "以及「面积**超额**与面积**亏损**分别具名判红」的主判据定义；"
           "⚠ 正本对 L2 只警告了「由 `A_drop/pixfrac²` 反推」这一支，**未警告本支**。"
           "本条读数由本轮实测取得（`A_drop/(pixfrac²·A_pixel)` = 1 − O(θ⁴)，"
           "在 θ = 300″ 时残差仍 ~1e-12 量级）",
    criteria=("S8",),
    kind=harness.NEGATIVE,
    inject="DEF-L2-TANGENT-BRANCH：面积走切平面二维分支（`max_angle < 1e-3` rad 的那一支）",
    defect_id="DZ-L2-TANGENT-BRANCH",
)
def test_l2_negative_tangent_plane_branch_is_a_second_vacuum_source():
    centre = _centre()
    pf = tol.DRIZZLE_FIXTURE_PIXFRAC.value
    # **同一 θ、两种面积分支** 的对照：强制走切平面二维面积 vs 球面立体角
    tangent_deltas, sphere_deltas = [], []
    for theta in (THETA_TANGENT_ARCSEC + (THETA_SPHERICAL_ARCSEC, 1000.0)):
        tp_pixel, tp_drop = _tangent_plane_area(theta, pf, centre)
        d_tangent = _delta(tp_drop, tp_pixel, pf)
        tangent_deltas.append(d_tangent)
        half = 0.5 * theta * ARCSEC
        sp_pixel = ref.polygon_area(ref.square_polygon(centre, half), centre)
        sp_drop = ref.polygon_area(ref.square_polygon(centre, pf * half), centre)
        d_sphere = _delta(sp_drop, sp_pixel, pf)
        sphere_deltas.append(d_sphere)
        with harness.evidence() as ev:
            ev.record(f"θ={theta}″：强制切平面二维面积的 δ", d_tangent)
            ev.record(f"θ={theta}″：球面立体角分支的 δ（同 θ 对照）", d_sphere)
            ev.record(f"θ={theta}″：两分支之比（切平面/球面）",
                      (d_tangent / d_sphere) if d_sphere else "n/a")
    # 切平面二维面积下 δ 恒为数值零 ⇒ 门对该性质**零判别力**
    scale = max(abs(x) for x in sphere_deltas) or 1.0
    with harness.evidence() as ev:
        ev.record("切平面分支 δ 的最大绝对值", max(abs(d) for d in tangent_deltas))
        ev.record("球面分支 δ 的最大绝对值（对照）", scale)
        ev.record("球面分支进入下限 θ（正方形外接半径 ≥ 1e-3 rad）",
                  f"{THETA_SPHERICAL_ARCSEC}″/px")
    # 切平面二维面积下 δ 恒为浮点零；球面分支下它是 1e-6 量级的真信号
    harness.is_true(all(abs(d) <= tol.F64_RTOL * max(scale, 1.0)
                        for d in tangent_deltas),
                    "切平面二维面积分支下 δ 必须恒为数值零（第二真空来源）")
    harness.is_false(any(d == 0.0 for d in sphere_deltas),
                     "对照臂的球面分支也恒零 ⇒ 分支选择不是原因，探针有误")
