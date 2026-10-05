"""守恒映射算子（P3）的**第二实现**——逐行转写自科学正本，作为单元层的被测口径。

## 它是什么

`docs/science/drizzle/DRIZZLE.md` 的公式到 Python 的**逐条转写**：核权重、面亮度归一分母、
组合系数、重建式、方差与协方差传播、几何闭合的球面裁剪与面积、以及 `NAN-SAMPLE-MASK-COVERAGE-NAN`
的样本级掩膜路径。每一条函数在 docstring 里标注它转写的正本条款。

它**不是**产品代码的绑定层，也不 import 任何第三方库：它与测试文件里的 oracle
（`astropy` / `astropy_healpix` / 解析闭式）**互相独立**，二者对拍才有证据资格。
`standards/05_INDEPENDENT_TEST_SUITE.md` §2 逐字「单元：函数与核（含 Oracle 对拍、归零负例）」。

## 它不是什么

- 它不是产品实现的替代品。本单元层不链接 `lib/`，`drizzle_ref.py` 测的是**正本口径本身**
  被写进代码后是否守恒；产品实现的回归由模块层与集成层承担。
- 它不是裁判。缺陷注入点（`Defects`）是**显式具名**的，每一项对应一条负例用例；
  默认 `NO_DEFECTS` 走全部正本分支。

## 缺陷注入的纪律

`Defects` 的每一个字段只允许在对应负例用例里被打开，且必须在 `inject` 字段写明注入的是什么。
不存在「测试为了通过而改被测实现」的路径：`NO_DEFECTS` 是唯一默认，负例把 `Defects` 显式传进去。

缺陷与正本条款的一一对应（括号内是负例用例的 `defect_id`）：

| `Defects` 字段 | 注入的缺陷 | 正本条款（它违反谁） |
|---|---|---|
| `kernel_denominator="pixel_area"` | 核权重分母从 drop 面积换成像元面积 | `DRIZZLE.md` §3.3 / §5.1「通量守恒门」红侧 |
| `normaliser="coverage_area"` | 面亮度归一分母从 `N_p` 换成覆盖面积 `D_p` | `DRIZZLE.md` §3.4 第二条反例、`DRIZZLE_GEOMETRY.md` DISP-DRZ-009 |
| `variance_form="missing_square"` | `Σ c_jp v_j`（漏一次平方） | `DRIZZLE.md` §3.6 |
| `variance_form="missing_normaliser"` | `Σ v_j w_jp²/N_p`（漏 `N_p²`） | `DRIZZLE.md` §3.6 |
| `variance_form="kernel_square_denominator"` | 用核权重平方除覆盖面积 `D_p²` | `DRIZZLE.md` §5.1「方差恒等判据」红侧 |
| `variance_form="mean_over_samples"` | 把方差当均值除以样本数 | `DRIZZLE.md` §3.6「联合缩放律」 |
| `sample_qualification="none"` | 不做样本级掩膜，NaN 直接进 `F_p` | `DRIZZLE.md` §5.2「非有限样本」 |
| `sample_qualification="nan_only"` | 只剔 NaN、不剔 `±Inf` | `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md` 合格样本定义（`isfinite(x_j)`） |
| `flux_conservation_factor` | 溯源因子写成 `pixfrac²` | `DRIZZLE.md` §4 参数表 / 合同 `FZ-COND-FLUX-CONSERV` |
| `swallow_leaves` | 候选枚举漏掉一个叶（面积亏损） | `DRIZZLE.md` §3.7 L1「面积亏损」 |
| `allocation_bias` | 逐叶分配按互补量互移（总量不变、逐叶错） | `DRIZZLE.md` §5.1「求和型守恒门只作辅助」 |
| `reject_count_suppressed` | 计数为 0 时不落该字段 | `DRIZZLE.md` §5.2「计数为 0 与字段缺失必须可区分」 |
| `validity_predicate="nonfinite_or_zero"` | 把 `0` / `±Inf` 读作无效 | `DRIZZLE.md` §5.2「`0` 与 `±Inf` 一律读作有效数值」 |
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

# ---------------------------------------------------------------------------
# 球面矢量与角度
# ---------------------------------------------------------------------------

Vec = Tuple[float, float, float]
Polygon = Tuple[Vec, ...]

#: `DRIZZLE.md` §4 参数表「微小多边形切平面分支阈值 `max_angle < 1e-3` rad」。
TANGENT_BRANCH_MAX_ANGLE = 1e-3


def vec_normalise(v: Sequence[float]) -> Vec:
    x, y, z = float(v[0]), float(v[1]), float(v[2])
    n = math.sqrt(x * x + y * y + z * z)
    if n == 0.0:
        raise ValueError("零向量不能归一化")
    return (x / n, y / n, z / n)


def vec_dot(a: Sequence[float], b: Sequence[float]) -> float:
    return float(a[0]) * float(b[0]) + float(a[1]) * float(b[1]) + float(a[2]) * float(b[2])


def vec_cross(a: Sequence[float], b: Sequence[float]) -> Vec:
    a0, a1, a2 = float(a[0]), float(a[1]), float(a[2])
    b0, b1, b2 = float(b[0]), float(b[1]), float(b[2])
    return (a1 * b2 - a2 * b1, a2 * b0 - a0 * b2, a0 * b1 - a1 * b0)


def angular_separation(a: Sequence[float], b: Sequence[float]) -> float:
    """球面角距 [rad]。用 `atan2(|a×b|, a·b)`，近退化时不发生相消。"""
    c = vec_cross(a, b)
    return math.atan2(math.sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]), vec_dot(a, b))


def tangent_basis(reference: Sequence[float]) -> Tuple[Vec, Vec]:
    """参考点处的切平面正交基 `(east, north)`。"""
    ref = vec_normalise(reference)
    up = (0.0, 0.0, 1.0) if abs(ref[2]) < 0.9 else (1.0, 0.0, 0.0)
    east = vec_cross(up, ref)
    east = vec_normalise(east)
    east = vec_normalise((east[0] - vec_dot(east, ref) * ref[0],
                          east[1] - vec_dot(east, ref) * ref[1],
                          east[2] - vec_dot(east, ref) * ref[2]))
    north = vec_cross(ref, east)
    return east, north


def gnomonic_offset(reference: Sequence[float], x: float, y: float) -> Vec:
    """切平面偏移 `(x, y)` [rad] → 单位球面矢量。

    正本：`DRIZZLE.md` §3.1「四角 = `pixelToSky((x ± half, y ± half))`」。夹具用它把
    「像元角尺度」这一物理量直接写进球面坐标，使 `A_pixel,j` / `A_drop,j` 有解析闭式可比。
    """
    ref, east, north = (vec_normalise(reference),) + tangent_basis(reference)
    return vec_normalise((ref[0] + x * east[0] + y * north[0],
                          ref[1] + x * east[1] + y * north[1],
                          ref[2] + x * east[2] + y * north[2]))


def square_polygon(reference: Sequence[float], half_width: float) -> Polygon:
    """以 `reference` 为心、gnomonic 半宽 `half_width` 的球面正方形四角（逆时针）。"""
    return tuple(gnomonic_offset(reference, sx * half_width, sy * half_width)
                 for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)))


def max_angular_radius(vertices: Iterable[Sequence[float]], reference: Sequence[float]) -> float:
    return max(angular_separation(reference, v) for v in vertices)


# ---------------------------------------------------------------------------
# 面积（`DRIZZLE.md` §3.7：两支，同一分支同一例程）
# ---------------------------------------------------------------------------

def _shoelace(points: Sequence[Tuple[float, float]]) -> float:
    n = len(points)
    acc = 0.0
    for i in range(n):
        x1, y1 = points[i]
        x2, y2 = points[(i + 1) % n]
        acc += x1 * y2 - x2 * y1
    return abs(0.5 * acc)


def _lhuilier(a: float, b: float, c: float) -> float:
    """球面三角立体角（L'Huilier）。比 `2·atan2(det, …)` 在近退化下稳定。"""
    s = 0.5 * (a + b + c)
    product = (math.tan(s / 2.0) * math.tan((s - a) / 2.0)
               * math.tan((s - b) / 2.0) * math.tan((s - c) / 2.0))
    return 4.0 * math.atan2(math.sqrt(product), 1.0)


def _project(vertices: Sequence[Sequence[float]], reference: Sequence[float]) -> List[Tuple[float, float]]:
    ref = vec_normalise(reference)
    east, north = tangent_basis(ref)
    out = []
    for v in vertices:
        d = vec_dot(v, ref)
        if d <= 0.0:
            raise ValueError("切平面投影要求全部顶点在参考点的同一半球内")
        out.append((vec_dot(v, east) / d, vec_dot(v, north) / d))
    return out


def polygon_area(vertices: Polygon, reference: Sequence[float]) -> float:
    """球面多边形面积 [sr]。

    `DRIZZLE.md` §3.7 逐字：「交叠面积按 drop 的最大角半径分两支：角跨度小于 `1e-3` rad 的
    极小多边形走切平面二维面积（避免球面立体角的三重积在近退化时相消），其余走球面立体角分支。
    **核分母 `A_drop,j` 与面亮度归一分母用的 `A_pixel,j` 走同一分支同一例程**」。

    分支按 `max_angular_radius(vertices, reference) < TANGENT_BRANCH_MAX_ANGLE` 选取，
    `reference` 由调用方给定（`A_pixel,j` 取像元中心方向，`A_drop,j` 取 drop 中心方向——
    两者同一像元同一点，满足「同一分支同一例程」）。
    """
    if len(vertices) < 3:
        return 0.0
    ref = vec_normalise(reference)
    if max_angular_radius(vertices, ref) < TANGENT_BRANCH_MAX_ANGLE:
        return _shoelace(_project(vertices, ref))
    acc = 0.0
    n = len(vertices)
    for i in range(n):
        a = vertices[i]
        b = vertices[(i + 1) % n]
        acc += _lhuilier(angular_separation(a, b),
                         angular_separation(b, ref),
                         angular_separation(ref, a))
    return abs(acc)


# ---------------------------------------------------------------------------
# 球面交叠（Sutherland–Hodgman 的球面推广，`DRIZZLE.md` §7.3）
# ---------------------------------------------------------------------------

def _clip_planar(poly: List[Tuple[float, float]], nx: float, ny: float, nc: float
                 ) -> List[Tuple[float, float]]:
    """平面半平面裁剪，内部量 `nx·x + ny·y + nc ≥ 0`。

    `nc = n·ref`：对参考点处 gnomonic 坐标为 `(x, y)` 的单位球面矢量 `v`，
    `n·v = (nc + nx·x + ny·y)/‖·‖`，所以判内必须用 **`+nc`**。
    """
    out: List[Tuple[float, float]] = []
    m = len(poly)
    for i in range(m):
        p = poly[i]
        q = poly[(i + 1) % m]
        dp = nx * p[0] + ny * p[1] + nc
        dq = nx * q[0] + ny * q[1] + nc
        if dp >= 0.0:
            out.append(p)
        if (dp > 0.0 > dq) or (dp < 0.0 < dq):
            t = dp / (dp - dq)
            out.append((p[0] + t * (q[0] - p[0]), p[1] + t * (q[1] - p[1])))
    return out


def _clip_spherical(poly: List[Vec], normal: Vec) -> List[Vec]:
    """球面逐边裁剪，内部量 `normal·x ≥ 0` 为内。交点取 `d_Q·P − d_P·Q`（按 `d_P` 选半球）。"""
    out: List[Vec] = []
    m = len(poly)
    for i in range(m):
        p = poly[i]
        q = poly[(i + 1) % m]
        dp = vec_dot(normal, p)
        dq = vec_dot(normal, q)
        if dp >= 0.0:
            out.append(p)
        if (dp > 0.0 > dq) or (dp < 0.0 < dq):
            y = (dq * p[0] - dp * q[0], dq * p[1] - dp * q[1], dq * p[2] - dp * q[2])
            if dp > 0.0:
                y = (-y[0], -y[1], -y[2])
            out.append(vec_normalise(y))
    return out


def _inward_normals(clip: Polygon, reference: Sequence[float]) -> List[Tuple[float, float, float]]:
    """裁剪多边形各边的内向法线，在参考点切平面基下给 `(nx, ny, nc)`，满足
    `nx·x + ny·y + nc ≥ 0` 为内（`nc = n·ref`）。"""
    ref = vec_normalise(reference)
    east, north = tangent_basis(ref)
    centroid = [0.0, 0.0, 0.0]
    for v in clip:
        centroid[0] += v[0]
        centroid[1] += v[1]
        centroid[2] += v[2]
    centroid = vec_normalise(centroid)
    ce = vec_dot(centroid, east)
    cn = vec_dot(centroid, north)
    out = []
    for i in range(len(clip)):
        n = vec_normalise(vec_cross(clip[i], clip[(i + 1) % len(clip)]))
        nx, ny, nc = vec_dot(n, east), vec_dot(n, north), vec_dot(n, ref)
        if nx * ce + ny * cn + nc < 0.0:
            nx, ny, nc = -nx, -ny, -nc
        out.append((nx, ny, nc))
    return out


def polygon_intersection_area(subject: Polygon, clip: Polygon, reference: Sequence[float]) -> float:
    """`a_jp`：drop 多边形与目标叶多边形的球面交叠面积 [sr]。

    分支选取与 `polygon_area` 同一判据（drop 的最大角半径）。切平面支把交叠**整体**放在
    平面里做：gnomonic 投影把大圆映成直线、把半球映成半平面，故球面裁剪与平面裁剪在
    该分支上逐点等价，drop 分割的构造级闭合因此是逐位恒等而非近似。
    """
    if len(subject) < 3 or len(clip) < 3:
        return 0.0
    ref = vec_normalise(reference)
    if max_angular_radius(subject, ref) < TANGENT_BRANCH_MAX_ANGLE:
        poly = _project(subject, ref)
        for nx, ny, nc in _inward_normals(clip, ref):
            poly = _clip_planar(poly, nx, ny, nc)
            if len(poly) < 3:
                return 0.0
        return _shoelace(poly)
    poly = list(subject)
    centroid = [0.0, 0.0, 0.0]
    for v in clip:
        centroid[0] += v[0]
        centroid[1] += v[1]
        centroid[2] += v[2]
    centroid = vec_normalise(centroid)
    for i in range(len(clip)):
        n = vec_normalise(vec_cross(clip[i], clip[(i + 1) % len(clip)]))
        if vec_dot(n, centroid) < 0.0:
            n = (-n[0], -n[1], -n[2])
        poly = _clip_spherical(poly, n)
        if len(poly) < 3:
            return 0.0
    return polygon_area(tuple(poly), ref)


# ---------------------------------------------------------------------------
# 缺陷注入面
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Defects:
    """具名缺陷开关。默认全零 = 全正本分支（`NO_DEFECTS`）。"""

    #: "drop_area"（正本 §3.2）| "pixel_area"（负例：通量泛函退化为 `pixfrac²·Σx_j`）
    kernel_denominator: str = "drop_area"
    #: "surface_brightness"（正本 §3.3）| "coverage_area"（负例：`S_p` 偏 `1/pixfrac²`）
    normaliser: str = "surface_brightness"
    #: "canonical" | "missing_square" | "missing_normaliser" | "kernel_square_denominator"
    #: | "mean_over_samples"
    variance_form: str = "canonical"
    #: "finite"（正本：`isfinite(x_j)`）| "nan_only" | "none"（不剔除，NaN 直接进 `F_p`）
    sample_qualification: str = "finite"
    #: 正本恒为 1.0；负例写 `pixfrac²`。
    flux_conservation_factor: float = 1.0
    #: 候选枚举漏掉的叶（面积亏损面）。
    swallow_leaves: Tuple[int, ...] = ()
    #: 逐叶分配偏置 `((from_ipix, to_ipix, eps_area), …)`，总量不变、逐叶错。
    allocation_bias: Tuple[Tuple[int, int, float], ...] = ()
    #: 计数为 0 时不落 `n_rejected_nonfinite` 字段（与「字段缺失」混同）。
    reject_count_suppressed: bool = False
    #: "nan_or_support_le_0"（正本）| "nonfinite_or_zero"（把 `0` / `±Inf` 读作无效）
    validity_predicate: str = "nan_or_support_le_0"
    #: "exact_quadratic"（正本）| "diagonal"（把对角归约声明为精确值）
    aperture_variance_form: str = "exact_quadratic"


NO_DEFECTS = Defects()


# ---------------------------------------------------------------------------
# 算子
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class SourcePixel:
    """源像元样本。

    `pixel_area` / `drop_area` 由**多边形实测**（`polygon_area`）给出，不允许由
    `A_drop / pixfrac²` 反推——这是 `DRIZZLE.md` §3.7 给 L2 判别力设的前提。
    """

    index: int
    value: float                    # x_j [ADU]
    variance: float                 # v_j [ADU²]
    pixel_area: float               # A_pixel,j [sr]
    drop_area: float                # A_drop,j [sr]
    drop_vertices: Polygon          # drop 四角（单位球面矢量）
    reference: Vec                  # 像元中心方向（面积与交叠共用的切平面参考点）

    @classmethod
    def make(cls, *, index: int, value: float, variance: float,
             pixel_vertices: Polygon, drop_vertices: Polygon, reference: Vec) -> "SourcePixel":
        return cls(index=index, value=value, variance=variance,
                   pixel_area=polygon_area(pixel_vertices, reference),
                   drop_area=polygon_area(drop_vertices, reference),
                   drop_vertices=drop_vertices, reference=vec_normalise(reference))


@dataclass
class LeafAccumulator:
    """目标叶累加器。四个累加量对应 `DRIZZLE_GEOMETRY.md` §1 的
    `Σ_j x_j w_jp` / `Σ_j a_jp` / `Σ_j w_jp A_pixel,j` / `Σ_j v_j w_jp²`。"""

    ipix: int
    sum_flux: float = 0.0           # Σ_j x_j w_jp                       [ADU]
    sum_area: float = 0.0           # Σ_j a_jp = D_p                     [sr]
    sum_norm: float = 0.0           # Σ_j w_jp A_pixel,j = N_p           [sr]
    sum_var_num: float = 0.0        # Σ_j v_j w_jp²（累加器域通量方差分子）[ADU²]
    n_contrib: int = 0              # 合格样本计数
    n_rejected_nonfinite: Optional[int] = 0   # 强制计数，None = 字段缺失
    rejected_nonfinite_value: int = 0
    rejected_nonfinite_variance: int = 0
    rejected_nonpositive_weight: int = 0
    #: 逐 `(a_jp, A_pixel,j, x_j, v_j, j)` 的 raw 记账。方差与协方差项只从这里重算，
    #: 不读已计算量（`DRIZZLE.md` §7.3 `reference_sb_coefficient` 逐字「各门只接受 raw
    #: 输入并重算，不读取任何已计算量，避免同实现自证」）。
    raw: List[Tuple[float, float, float, float, int]] = field(default_factory=list)


def kernel_weight(overlap_area: float, pixel: SourcePixel, defects: Defects = NO_DEFECTS) -> float:
    """核权重 `w_jp`（无量纲）。

    正本 `DRIZZLE.md` §3.2 逐字「`w_jp = a_jp / A_drop,j`」。
    """
    if defects.kernel_denominator == "pixel_area":
        return overlap_area / pixel.pixel_area          # 负例：分母换成像元面积
    return overlap_area / pixel.drop_area              # 正本


def normaliser_term(overlap_area: float, pixel: SourcePixel, defects: Defects = NO_DEFECTS) -> float:
    """面亮度归一分母的单项 `w_jp·A_pixel,j`（sr）。

    正本 `DRIZZLE.md` §3.3 逐字「`N_p = Σ_j w_jp · A_pixel,j`」，实现侧写法
    `overlap_area·(pixel_area/drop_area)`（`DRIZZLE_GEOMETRY.md` §1）。

    分母项与核权重**共用同一个 `w_jp`**：正本 §3.6 逐字「**信号与方差必须用同一个
    `w_jp`**」。因此 `kernel_denominator="pixel_area"` 这一参数化变更自动把分母带成
    `Σ_j w'_jp A_pixel,j = D_p`（正本 §3.3 的「同型分母 `N'_p`」），二者同步，
    `c_jp`、`S_p`、`variance_p` 逐位不变，只有通量泛函变。
    """
    if defects.normaliser == "coverage_area":
        return overlap_area                            # 负例：drop 归一核配 `D_p` 作分母
    return kernel_weight(overlap_area, pixel, defects) * pixel.pixel_area


def sb_coefficient(overlap_area: float, pixel_area: float, covered_area: float) -> float:
    """组合系数 `c_jp = a_jp/(A_pixel,j·D_p)`（sr⁻¹）。

    正本 `DRIZZLE.md` §3.3 逐字「`c_jp = w_jp / N_p = a_jp / (A_pixel,j · D_p)`」；
    §7.3 要求「用 raw `(a_jp, A_pixel,j, D_p)` 重算，不读取任何已计算量」。
    """
    return overlap_area / (pixel_area * covered_area)


def surface_brightness_variance(acc: LeafAccumulator, defects: Defects = NO_DEFECTS) -> float:
    """`variance_p`（ADU²/sr²），由 raw 重算。

    正本 `DRIZZLE.md` §3.6 逐字「`variance_p = Σ_j c_jp² v_j = Σ_j v_j w_jp² / N_p²`」。
    四个缺陷分支都由同一条 `Σ v c²` 改写，`c_jp = w_jp/N_p`：

    | 缺陷 | 表达式 | 相对 `Σ v c²` 的因子 |
    |---|---|---|
    | `missing_square` | `Σ v c_jp` | `1/c_jp` |
    | `missing_normaliser` | `Σ v c_jp² N_p` | `N_p` |
    | `kernel_square_denominator` | `Σ v c_jp² (N_p/D_p)²` | `(N_p/D_p)² = pixfrac⁻⁴` |
    | `mean_over_samples` | `(Σ v c_jp²)/n_contrib` | `1/n_contrib`（破坏联合缩放律） |
    """
    d_p = acc.sum_area
    n_p = acc.sum_norm
    if acc.n_contrib == 0 or d_p <= 0.0:
        return float("nan")
    coefficients = [(sb_coefficient(a, ap, d_p), v) for a, ap, _x, v, _j in acc.raw]
    if defects.variance_form == "missing_square":
        return math.fsum(c * v for c, v in coefficients)
    if defects.variance_form == "missing_normaliser":
        return math.fsum(v * c * c * n_p for c, v in coefficients)
    if defects.variance_form == "kernel_square_denominator":
        return math.fsum(v * c * c * (n_p / d_p) ** 2 for c, v in coefficients)
    if defects.variance_form == "mean_over_samples":
        return math.fsum(v * c * c for c, v in coefficients) / acc.n_contrib
    return math.fsum(v * c * c for c, v in coefficients)


def _coefficients_by_sample(accumulators: Mapping[int, LeafAccumulator],
                            defects: Defects = NO_DEFECTS
                            ) -> Dict[int, Dict[int, Tuple[float, float]]]:
    """`{ipix: {j: (c_jp, v_j)}}`，全部由 raw 重算。"""
    out: Dict[int, Dict[int, Tuple[float, float]]] = {}
    for ipix, acc in accumulators.items():
        d_p = acc.sum_area
        out[ipix] = {j: (sb_coefficient(a, ap, d_p), v) for a, ap, _x, v, j in acc.raw}
    return out


def covariance(accumulators: Mapping[int, LeafAccumulator], ipix_p: int, ipix_q: int,
               defects: Defects = NO_DEFECTS) -> float:
    """`Cov(S_p, S_q) = Σ_j v_j c_jp c_jq`（ADU²/sr²），按样本序号对齐 raw。

    正本 `DRIZZLE.md` §3.6 逐字「`Cov(S_p, S_q) = Σ_j v_j w_jp w_jq / (N_p N_q)`
    （`p ≠ q` 时一般非零）」。
    """
    coeffs = _coefficients_by_sample({ipix_p: accumulators[ipix_p],
                                      ipix_q: accumulators[ipix_q]}, defects)
    total = 0.0
    for j, (c_p, v_p) in coeffs[ipix_p].items():
        if j in coeffs[ipix_q]:
            c_q, v_q = coeffs[ipix_q][j]
            total += c_p * c_q * v_p
    return total


def aperture_variance(accumulators: Mapping[int, LeafAccumulator],
                      aperture_weights: Mapping[int, float],
                      defects: Defects = NO_DEFECTS) -> Tuple[float, float, float]:
    """孔径（块平均）方差：返回 `(exact, diag, deficit)`。

    正本 `DRIZZLE.md` §3.6 逐字
    「`Var( Σ_p a_p S_p ) = Σ_{p,q} a_p a_q Cov(S_p,S_q) = Σ_j v_j ( Σ_p a_p c_jp )²`」
    与「只用对角元给出的 `Σ_p a_p² variance_p` 是这个量的**下界，条件是孔径权重
    `a_p ≥ 0` 且 `c_jp ≥ 0`**」。

    `deficit = (exact − diag)/exact`（正本同节要求与算子摘要一起给出，声明为精确值被拒）。
    缺陷 `aperture_variance_form="diagonal"` 把对角归约声明为精确值。
    """
    coeffs = _coefficients_by_sample(accumulators, defects)
    per_sample: Dict[int, float] = {}
    variances: Dict[int, float] = {}
    for ipix, acc in accumulators.items():
        a_p = aperture_weights[ipix]
        for j, (c_jp, v_j) in coeffs[ipix].items():
            per_sample[j] = per_sample.get(j, 0.0) + a_p * c_jp
            variances[j] = v_j
    exact = math.fsum(variances[j] * per_sample[j] ** 2 for j in per_sample)
    diag = math.fsum(a_p * a_p * surface_brightness_variance(accumulators[ipix], defects)
                     for ipix, a_p in aperture_weights.items())
    if defects.aperture_variance_form == "diagonal":
        return diag, diag, 0.0
    deficit = (exact - diag) / exact if exact != 0.0 else float("nan")
    return exact, diag, deficit


def cell_area(nside: int) -> float:
    """叶面积 `A_cell = π/(3·nside²)` sr（HEALPix 构造性质）。正本 `DRIZZLE.md` §3.5 / §4。"""
    return math.pi / (3.0 * nside * nside)


def _sample_is_qualified(value: float, defects: Defects) -> bool:
    """合格样本判定。正本 `DRIZZLE.md` §5.2「源像元值非有限时按样本级掩膜处理」。"""
    if defects.sample_qualification == "none":
        return True
    if defects.sample_qualification == "nan_only":
        return not math.isnan(value)      # 负例：漏剔 `±Inf`
    return math.isfinite(value)


def accumulate(samples: Sequence[SourcePixel],
               leaves: Mapping[int, Polygon],
               *,
               defects: Defects = NO_DEFECTS,
               flux_scale: float = 1.0,
               variance_scale: float = 1.0) -> Dict[int, LeafAccumulator]:
    """按叶累加。`leaves` 是 `ipix → 叶多边形`，由调用方给出候选集
    （生产路径的候选枚举不在本层，见 `DRIZZLE.md` §5.1「零漏选门」）。

    `flux_scale` / `variance_scale` 实现**联合缩放律**的输入侧：正本 §3.6 要求
    `(x, v) → (α·x, α²·v)`；调用方传 `α` 与 `α²`。
    """
    acc: Dict[int, LeafAccumulator] = {}
    swallowed = set(defects.swallow_leaves)
    bias: Dict[int, float] = {}
    for frm, to, eps in defects.allocation_bias:
        bias[frm] = bias.get(frm, 0.0) + eps
        bias[to] = bias.get(to, 0.0) - eps

    for ipix in leaves:
        acc[ipix] = LeafAccumulator(ipix=ipix)

    for s in samples:
        x = s.value * flux_scale
        v = s.variance * variance_scale
        qualified = _sample_is_qualified(x, defects)
        if not math.isfinite(v):
            qualified = False                  # 方差面损坏按不合格样本剔除
        reason_value = not math.isfinite(x)
        reason_variance = not math.isfinite(v)
        for ipix, clip_poly in leaves.items():
            if ipix in swallowed:
                continue                                  # 缺陷：叶被吞（面积亏损）
            a_jp = polygon_intersection_area(s.drop_vertices, clip_poly, s.reference)
            a_jp += bias.get(ipix, 0.0)                   # 缺陷：逐叶互补互移（总量守恒）
            if a_jp <= 0.0:
                # 无几何覆盖 ⇒ 该源样本**不是本输出像元的候选样本**，不进入合格性判定，
                # 强制计数保持 0（`PHASE_PRODUCT_EXCHANGE.md`「与两类输入的对应」表
                # 逐字「无覆盖（帧足迹外、drop 未触及）| 无候选样本 | `NaN / support=0`，
                # `n_rejected=0`」）。
                continue
            a = acc[ipix]
            if not qualified:
                # 样本级掩膜：不合格样本从 F_p / 分母 / 方差三项中一并剔除并强制计数。
                if a.n_rejected_nonfinite is not None:
                    a.n_rejected_nonfinite += 1
                if reason_value:
                    a.rejected_nonfinite_value += 1
                if reason_variance:
                    a.rejected_nonfinite_variance += 1
                continue
            a.sum_flux += x * kernel_weight(a_jp, s, defects)
            a.sum_area += a_jp
            a.sum_norm += normaliser_term(a_jp, s, defects)
            a.sum_var_num += v * kernel_weight(a_jp, s, defects) ** 2
            a.n_contrib += 1
            a.raw.append((a_jp, s.pixel_area, x, v, s.index))

    if defects.reject_count_suppressed:
        for a in acc.values():
            if a.n_rejected_nonfinite == 0:
                a.n_rejected_nonfinite = None
    return acc


@dataclass(frozen=True)
class LeafProduct:
    """目标叶产品面。正本 `DRIZZLE.md` §2.1 输出语义表 + §5.1 产品级量化。"""

    ipix: int
    flux: float                     # F_p [ADU]
    normaliser: float               # N_p [sr]
    covered_area: float             # D_p [sr]
    signal: float                   # S_p = F_p/N_p [ADU/sr]
    variance: float                 # [ADU²/sr²]
    ivar: float
    support: float                  # D_p/A_cell
    n_contrib: int
    n_rejected_nonfinite: Optional[int]
    flux_conservation_factor: float
    quantised_covered_area: float   # (q/255)·A_cell [sr]
    q: int


def finalise(acc: LeafAccumulator, *, nside: int,
             defects: Defects = NO_DEFECTS) -> LeafProduct:
    """累加器 → 产品。正本 `DRIZZLE.md` §3.5 重建式 + §5.2 覆盖级 NaN + §5.1 8bit 量化。"""
    a_cell = cell_area(nside)
    f_p, n_p, d_p = acc.sum_flux, acc.sum_norm, acc.sum_area

    if acc.n_contrib == 0:
        # 「仅当零合格样本时输出 `NaN` 且 `support = 0`」
        signal = float("nan")
        variance = float("nan")
        support = 0.0
    else:
        signal = f_p / n_p
        variance = surface_brightness_variance(acc, defects)
        support = d_p / a_cell

    ivar = (1.0 / variance) if math.isfinite(variance) and variance != 0.0 else float("nan")

    # 产品级 8bit 覆盖面积量化：`q = lround(255·clamp(D_p/A_cell, 0, 1))`，
    # `covered_area = (q/255)·A_cell`（`DRIZZLE.md` §5.1 逐字）。
    ratio = d_p / a_cell
    clamped = min(1.0, max(0.0, ratio))
    q = int(math.floor(255.0 * clamped + 0.5))
    quantised = (q / 255.0) * a_cell

    return LeafProduct(ipix=acc.ipix, flux=f_p, normaliser=n_p, covered_area=d_p,
                       signal=signal, variance=variance, ivar=ivar, support=support,
                       n_contrib=acc.n_contrib,
                       n_rejected_nonfinite=acc.n_rejected_nonfinite,
                       flux_conservation_factor=defects.flux_conservation_factor,
                       quantised_covered_area=quantised, q=q)


def product_signal(product: LeafProduct) -> float:
    """产品级 `signal`：发布因子 `k = D_p/N_p`，`signal × k`，分母是量化后的覆盖面积。

    正本 `DRIZZLE.md` §7.3 逐字「发布因子 `k = D_p/N_p`，signal 乘 `k`、variance 乘 `k²`；
    同处把 `covered_area` 按 `q = lround(255·clamp(D_p/A_cell,0,1))` 量化」。
    """
    if product.n_contrib == 0:
        return float("nan")
    k = product.covered_area / product.normaliser
    num = product.flux * k
    den = product.quantised_covered_area
    if den == 0.0:
        # IEEE 754：`x/±0` 是 ±Inf，`0/0` 是 NaN。`±Inf` 是**有效数值**，
        # 不按 `DRIZZLE.md` §5.2 的无效表示处理（见 `is_invalid_output`）。
        if num == 0.0:
            return float("nan")
        return math.copysign(math.inf, num)
    return num / den


def is_invalid_output(signal: float, support: float, defects: Defects = NO_DEFECTS) -> bool:
    """输出有效性判定。

    正本 `docs/engineering/data/PHASE_PRODUCT_EXCHANGE.md`「无效输出 = `signal = NaN` **且**
    `support ≤ 0`；两者**必须同时**成立」；`DRIZZLE.md` §5.2 逐字「`NaN` 是无效的唯一表示，
    `0` 与 `±Inf` 一律读作有效数值」。
    """
    if defects.validity_predicate == "nonfinite_or_zero":
        return (not math.isfinite(signal)) or signal == 0.0 or support <= 0.0
    return math.isnan(signal) and support <= 0.0


def frame_flux_total(products: Iterable[LeafProduct]) -> float:
    """`Σ_p F_p`：逐叶通量之和（通量守恒门的被测量）。"""
    return math.fsum(p.flux for p in products)
