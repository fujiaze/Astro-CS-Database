r"""P3 · 守恒映射算子的合成全链不变量（`AGENTS.md` §10 创新点三）。

正本：`docs/science/drizzle/DRIZZLE.md`（SCI-DRZ）。

## 这一层只测四条真空

`eng/tests/unit/` 与 `eng/tests/integration/` 已覆盖：几何闭合 L1、跨叶/跨面权重和门、
NaN/±Inf/0 三态掩膜、方差恒等（漏平方 / 漏 N² / 分母用核权重平方）、联合缩放律、协方差门
`exact>diag` 及带号反例、求和型守恒门无判别力的实测、零漏选 S10（用 `astropy_healpix`
重建 oracle）、S11 HEALPix 四件套。⇒ **那些一条都不在这里重写**。

本文件只写四条：

| 判据 | 正本 | 要点 |
|---|---|---|
| P3-a | §5.3 等面积 chart | 12 个 base-resolution chart 的 Jacobian 恒定 |
| P3-b | §3.7(:206-214) L2 面积比级 | `A_pixel,j` 必须由**未收缩四角**独立实测 |
| P3-c | §5.2(:287) 叶边界弦亏缺 | 极限相对亏缺 `2√2/π − 1` |
| P3-d | §3.8(:218-228) 控制点搬运 | 三条规则 + 点包含落格 |

## ⚠ 纪律一：Jacobian 必须**真算**

`实验/healpix-polar/route2/.../p3lib.py:82-84` 实测把 Jacobian 写成 `np.full_like(u, π/3)`，
判据零判别力（`实验/TAUTOLOGY_REGISTER.md:337` 登记）。⇒ 本文件的 P3-a 用**中心差分**
从 Górski et al. 2005（arXiv:astro-ph/0409513）§5.1 式 (4)(5)(8)(9) 的**闭式 chart 位置**
当场求导，被测对象是那条映射本身，参照量取自 `astropy_healpix` 的精确 `pixel_area`。

## ⚠ 纪律二：预期值不得来自被测实现自身

- chart Jacobian 的解析值 = `π/(3 nside²)`，由 §5.1 的**闭式**逐字推导给出
  （`|Δz Δφ| = Ω_pix` 是原文），不由本文件的实现反推；
- 叶面积用 `astropy_healpix`（HEALPix C++ 基库）的 `pixel_area` 逐位给出；
- 弦亏缺用**真边界求根器**：对每个方位角二分「该像元成员关系的终点」，取其**局部极大值**
  （拐点）作为四角，再算测地多边形的立体角。⚠ 用 `healpix_to_xyz(dx, dy)` 的
  `(dx,dy) ∈ {0,1}` 端点**不是**叶的四角（实测它们落在 θ=90° 的赤道上）——
  这是本层实测踩到并否决的写法。

## 夹具

HEALPix 网格参数 `nside ∈ {16, 32, 64, 256, 512, 1024}`（都是 2 的幂，
`astropy_healpix` 只接受 2 的幂）。`pixfrac = 0.8`（DRIZZLE.md §4 参数表的数值默认）。
"""

from __future__ import annotations

import math
from typing import Dict, List, Sequence, Tuple

import astropy.units as u
import numpy as np
from astropy_healpix import (HEALPix, healpix_to_xyz, lonlat_to_healpix,
                             nside_to_pixel_resolution, xyz_to_healpix)

from eng.tests.synthetic import _tol_chain_a as A
from eng.tests.synthetic import tolerances as tol
from eng.tests.unit import harness as H

F64_RTOL = tol.F64_RTOL

#: DRIZZLE.md §4 参数表逐字的数值默认。
PIXFRAC = 0.8
#: 本层的 nside 档（2 的幂；`astropy_healpix` 只接受 2 的幂）。
NSIDES_L2 = (16, 32, 64)
NSIDES_CHORD = (256, 512, 1024)
NSIDES_CONTRAST = (64,)
#: 中心差分步长（chart Jacobian 用）。
FD_STEP = 1.0e-5
#: chart Jacobian 的扫掠网格（polar 的环向指标 rho、指标 nu；equatorial 同名）。
POLAR_RHO_GRID = (1.0, 1.37, 2.5, 5.75, 13.0)
EQUAT_RHO_GRID = (16.0, 18.5, 22.0, 25.5, 31.0)
#: 像元角尺度 `hp_res = sqrt(pi/3)/nside`（DRIZZLE.md §4 参数表逐字）。
def hp_res(nside: int) -> float:
    return math.sqrt(math.pi / 3.0) / float(nside)


# ---------------------------------------------------------------------------
# §1 球面几何工具（纯 numpy；Van Oosterom & Strackee 的球面三角形公式）
# ---------------------------------------------------------------------------

def _unit(v: np.ndarray) -> np.ndarray:
    v = np.asarray(v, dtype=np.float64)
    return v / np.linalg.norm(v, axis=-1, keepdims=True)


def spherical_triangle_area(a: np.ndarray, b: np.ndarray, c: np.ndarray) -> float:
    """球面三角形立体角（Van Oosterom & Strackee 公式），单位 sr。"""
    a, b, c = _unit(a), _unit(b), _unit(c)
    num = float(np.linalg.norm(np.cross(a, b) + np.cross(b, c) + np.cross(c, a)))
    den = 1.0 + float(np.dot(a, b) + np.dot(b, c) + np.dot(c, a))
    return 2.0 * math.atan2(num, den)


def spherical_polygon_area(vertices: np.ndarray) -> float:
    """球面多边形立体角：自顶点 0 作扇形三角剖分（顶点必须按环序给出）。"""
    v = _unit(np.asarray(vertices, dtype=np.float64))
    n = v.shape[0]
    return float(sum(spherical_triangle_area(v[0], v[i], v[i + 1])
                     for i in range(1, n - 1)))


def slerp(a: np.ndarray, b: np.ndarray, t: float) -> np.ndarray:
    """沿大圆把 `a` 到 `b` 的弧长按比例 `t` 取点。"""
    a, b = _unit(a), _unit(b)
    om = max(-1.0, min(1.0, float(np.dot(a, b))))
    w = math.atan2(math.sqrt(max(0.0, 1.0 - om * om)), om)
    s = math.sin(w)
    if s < 1.0e-12:
        return a
    return _unit(math.sin((1.0 - t) * w) / s * a + math.sin(t * w) / s * b)


def ring_frame(center: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """给单位向量建一个右手正交标架 `(c, e1, e2)`（`e1` 选在切平面内）。"""
    c = _unit(center)
    tmp = np.array([0.0, 0.0, 1.0]) if abs(c[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    e1 = _unit(np.cross(c, tmp))
    return c, e1, np.cross(c, e1)


def _radius(nside: int, ipix: int, c: np.ndarray, e1: np.ndarray, e2: np.ndarray,
            psi: np.ndarray) -> np.ndarray:
    """从像元中心沿方位角 `psi` 二分「成员关系终点」距离（rad），向量版。"""
    psi = np.atleast_1d(np.asarray(psi, dtype=np.float64))
    d = e1[None, :] * np.cos(psi)[:, None] + e2[None, :] * np.sin(psi)[:, None]
    hi0 = 2.0 * float(nside_to_pixel_resolution(nside).to_value(u.rad))
    lo = np.full(psi.shape, 1.0e-12)
    hi = np.full(psi.shape, hi0)
    for _ in range(56):
        mid = 0.5 * (lo + hi)
        t = mid[:, None]
        ct = np.cos(t)
        v = c[None, :] * ct + np.cross(d, c[None, :]) * np.sin(t) \
            + d * np.sum(c[None, :] * d, axis=-1)[:, None] * (1.0 - ct)
        same = (xyz_to_healpix(v[:, 0], v[:, 1], v[:, 2], nside, order='RING') == ipix)
        lo = np.where(same, mid, lo)
        hi = np.where(same, hi, mid)
    return 0.5 * (lo + hi)


def cell_corners(nside: int, ipix: int, n_psi: int = 128
                 ) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """HEALPix 像元（**RING** 序）的四个真角点。

    算法：以像元中心为极点、扫方位角 `psi`，二分出成员关系的终点 `R(psi)`；`R` 的
    **局部极大值**（一阶差分变号处）就是叶边界两段的交点（四角）。逐角点做黄金分割细化。

    ⚠ 这是**真边界求根**：不用 `(dx,dy)` 端点（实测那不是叶的四角）。
    """
    c = _unit(np.array(healpix_to_xyz(ipix, nside, order='RING'), dtype=np.float64))
    c, e1, e2 = ring_frame(c)
    psi = np.linspace(0.0, 2.0 * math.pi, n_psi, endpoint=False)
    r = _radius(nside, ipix, c, e1, e2, psi)
    dr = np.diff(np.concatenate([r, r[:1]]))
    gr = (math.sqrt(5.0) - 1.0) / 2.0
    out: List[float] = []
    for i in range(n_psi):
        if not (dr[i] > 0.0 and dr[(i + 1) % n_psi] <= 0.0):
            continue
        j = (i + 1) % n_psi
        a, b = float(psi[i]), float(psi[j]) + (2.0 * math.pi if j < i else 0.0)
        x1, x2 = b - gr * (b - a), a + gr * (b - a)
        f1 = float(_radius(nside, ipix, c, e1, e2, x1)[0])
        f2 = float(_radius(nside, ipix, c, e1, e2, x2)[0])
        for _ in range(60):
            if f1 < f2:
                a, x1, f1 = x1, x2, f2
                x2 = a + gr * (b - a)
                f2 = float(_radius(nside, ipix, c, e1, e2, x2)[0])
            else:
                b, x2, f2 = x2, x1, f1
                x1 = b - gr * (b - a)
                f1 = float(_radius(nside, ipix, c, e1, e2, x1)[0])
        out.append(0.5 * (a + b) % (2.0 * math.pi))
    ang = np.array(sorted(out), dtype=np.float64)
    rad = _radius(nside, ipix, c, e1, e2, ang)
    axis = e1[None, :] * np.cos(ang)[:, None] + e2[None, :] * np.sin(ang)[:, None]
    t = rad[:, None]
    corners = _unit(c[None, :] * np.cos(t) + np.cross(axis, c[None, :]) * np.sin(t)
                    + axis * np.sum(c[None, :] * axis, axis=-1)[:, None] * (1.0 - np.cos(t)))
    return c, ang, corners


def pixel_area(nside: int) -> float:
    """`astropy_healpix`（HEALPix C++ 基库）给出的像元面积，精确值。"""
    return float(HEALPix(nside=nside, order='RING').pixel_area.to_value(u.sr))


def polar_cell_index(nside: int, south: bool) -> int:
    """含极叶的 **RING** 序像元号：北极 0，南极 `12 nside² − 1`。"""
    return (12 * nside * nside - 1) if south else 0


def equatorial_cell_index(nside: int, offset: int) -> int:
    """赤道带里一个像元的 RING 序号（`offset` 相对赤道带起点）。"""
    n_eq = 4 * nside
    first = nside * (nside - 1)      # 极冠像元总数（北 + 南各 nside(nside−1)）
    return first + offset


# ---------------------------------------------------------------------------
# §2 HEALPix 的 12 个等面积 chart（Górski et al. 2005, arXiv:astro-ph/0409513 §5.1）
# ---------------------------------------------------------------------------

def _dir_from_zphi(z: np.ndarray, phi: np.ndarray) -> np.ndarray:
    z = np.asarray(z, dtype=np.float64)
    phi = np.asarray(phi, dtype=np.float64)
    s = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    return _unit(np.stack([s * np.cos(phi), s * np.sin(phi), z], axis=-1))


def polar_chart(nside: int, rho, nu):
    """北**极** chart 的闭式位置（式 (4)(5)）：`z = 1 − rho²/(3N²)`、`φ = π(ν−1/2)/(2ρ)`。"""
    rho = np.asarray(rho, dtype=np.float64)
    nu = np.asarray(nu, dtype=np.float64)
    return 1.0 - rho ** 2 / (3.0 * nside * nside), math.pi * (nu - 0.5) / (2.0 * rho)


def equatorial_chart(nside: int, rho, nu):
    """**赤道** chart 的闭式位置（式 (8)(9)）：`z = 4/3 − 2rho/(3N)`、`φ = π(ν−s/2)/(2N)`。"""
    rho = np.asarray(rho, dtype=np.float64)
    nu = np.asarray(nu, dtype=np.float64)
    return 4.0 / 3.0 - 2.0 * rho / (3.0 * nside), math.pi * nu / (2.0 * nside)


def polar_chart_equidistant(nside: int, rho, nu):
    """**缺陷注入**：把极 chart 的纬向映射从等面积 `z = 1 − ρ²/(3N²)` 换成**等距**
    `z = 1 − ρ/(3N²)`。Górski et al. 2005 §4 逐字把 Equidistant Cylindrical Projection
    列为「satisfies points 1 and 3, but **by construction fails with point 2**」的方案。"""
    rho = np.asarray(rho, dtype=np.float64)
    nu = np.asarray(nu, dtype=np.float64)
    return 1.0 - rho / (3.0 * nside * nside), math.pi * (nu - 0.5) / (2.0 * rho)


CHART_FAMILIES = {
    "polar_0": lambda n, r, v: polar_chart(n, r, v),
    "polar_1": lambda n, r, v: polar_chart(n, r, v),
    "polar_2": lambda n, r, v: polar_chart(n, r, v),
    "polar_3": lambda n, r, v: polar_chart(n, r, v),
    "polar_4": lambda n, r, v: polar_chart(n, r, v),
    "polar_5": lambda n, r, v: polar_chart(n, r, v),
    "polar_6": lambda n, r, v: polar_chart(n, r, v),
    "polar_7": lambda n, r, v: polar_chart(n, r, v),
    "equat_0": lambda n, r, v: equatorial_chart(n, r, v),
    "equat_1": lambda n, r, v: equatorial_chart(n, r, v),
    "equat_2": lambda n, r, v: equatorial_chart(n, r, v),
    "equat_3": lambda n, r, v: equatorial_chart(n, r, v),
}


def chart_jacobian(fn, nside: int, rho: float, nu: float, h: float = FD_STEP) -> float:
    """`(rho, nu) → (z, phi)` 映射的**中心差分** Jacobian 的行列式绝对值（sr / 单位指标²）。

    `dΩ = sinθ dθ dφ = |dz dφ|`，而 `z` 恒是 `cosθ` ⇒ **`dΩ` 就是该 2×2 行列式**。
    ⚠ 两条本层实测踩到的写法：
      ① 不得写成 `|n·(∂p/∂ρ × ∂p/∂ν)|`——那会多出一个 `sinθ` 因子；
      ② 不得用**前向**差分——`z(ρ)` 对 `ρ` 是二次的，前向差分给 `z' + ½z''h`，
         相对误差 `= h/(2ρ)`（实测 rel = 5e-6·h，**随 h 线性发散**）；
         中心差分把它降到 `O(h²)`（实测 h = 1e-5 处 ≤ 1e-10）。
    """
    z0, p0 = fn(nside, rho, nu)
    zap, _ = fn(nside, rho + h, nu)
    zam, _ = fn(nside, rho - h, nu)
    _, pbp = fn(nside, rho, nu + h)
    _, pbm = fn(nside, rho, nu - h)
    dz_dr = (zap - zam) / (2.0 * h)
    dph_dn = (pbp - pbm) / (2.0 * h)
    return abs(float(dz_dr * dph_dn))


# ---------------------------------------------------------------------------
# §3 控制点搬运（DRIZZLE.md §3.8）
# ---------------------------------------------------------------------------

class ControlPoint(dict):
    """帧内稀疏信噪比控制点（目录式样本，DRIZZLE.md §3.8:220 逐字列的字段）。"""

    def __getattr__(self, name: str):
        try:
            return self[name]
        except KeyError as exc:  # pragma: no cover
            raise AttributeError(name) from exc


def make_control_points() -> List[ControlPoint]:
    """一组合成的控制点夹具：位置取成**落在 tile 内部、与边界相距甚远**的字面坐标。

    ⚠ HEALPix 的边界点在浮点上有归属歧义（primer「Finite precision and cross-platform
    reproducibility」一节逐字说明 `ang2pix` 在边界 ~1e-15 rad 内可能落到相邻像元）
    ⇒ 位置一律取固定的、与 tile 边界距离 ≫ 1e-9 rad 的字面值。
    """
    # 位置是**冻结字面量**，由 nside = 64 的 tile 中心按定种子序列（seed = 777）挑出：
    # 要求在 nside ∈ {8, 64} 两档下都满足「±0.2·hp_res 的 8 方位扰动不改变 tile」，
    # 且 nside = 8 的 tile 互不相同。多数点就落在 tile 中心（偏移 0），一个偏移 0.15·hp_res。
    raw = [
        # (star_id, lon_deg, lat_deg, snr, fit_quality, photom_state)
        ("HD-1001", -42.890625, 24.624318, 128.5, "A", "PSF_OK"),
        ("HD-1002", -156.250000, -63.448284, 64.25, "B", "PSF_OK"),
        ("HD-1003", -78.750000, 37.921651, 31.125, "A", "PSF_OK"),
        ("HD-1004", -138.913043, 73.126882, 7.5, "C", "NEAR_SATURATED"),
        ("HD-1005", -148.263181, -5.841737, 256.0, "A", "PSF_OK"),
        ("HD-1006", -139.218750, -29.313199, 3.25, "D", "PSF_BAD"),
    ]
    return [ControlPoint(star_id=s, lon_deg=lo, lat_deg=la, snr=sn,
                         fit_quality=fq, photom_state=ps)
            for s, lo, la, sn, fq, ps in raw]


def tile_of(lon_deg: float, lat_deg: float, nside: int) -> int:
    """点包含落格：`ang2pix_NESTED` 在 tile 阶的像元号。

    ⚠ tile 序号的**索引算术**委托给 `astropy_healpix`（TEST.md §13 白名单的第三方实现）
    —— 重新推导 HEALPix 的面/环/辐条算术超出本层的裁决范围。被测的科学口径是
    DRIZZLE.md §3.8 的**三条搬运规则**与「恰落一个 tile」这个**点包含**性质，
    后者由 `transport_control_points` 里的**包含性复核**独立承担（见 `p3-d`）。
    """
    return int(lonlat_to_healpix(float(lon_deg) * u.deg, float(lat_deg) * u.deg,
                                nside=nside, order='NESTED'))


def tile_hit_count(lon_deg: float, lat_deg: float, nside: int) -> int:
    """点包含判定：走**两个独立入口**（`lonlat_to_healpix` 与 `xyz_to_healpix`，"
    "后者是 C++ 基库的向量化入口）分别求 tile，返回两者给出的**不同 tile 数**。

    必须**恰为 1**：一个控制点恰落在一个 tile 中（DRIZZLE.md §3.8:220 逐字）。
    ⚠ 本层实测否决的一种写法：拿一个半径 `margin·hp_res` 的小球做「整块包含」复核——
    该叶的内切半径**逐叶不同**（极冠附近可小到 0.05·hp_res 以下），夹具位置会随机踩到
    边界而使复核读数在 0 与 1 之间跳。⇒ 改用「两条独立代码路径给出同一 tile」。
    """
    lon = math.radians(float(lon_deg))
    lat = math.radians(float(lat_deg))
    v = np.array([math.cos(lat) * math.cos(lon), math.cos(lat) * math.sin(lon),
                  math.sin(lat)])
    a = int(lonlat_to_healpix(float(lon_deg) * u.deg, float(lat_deg) * u.deg,
                             nside=nside, order='NESTED'))
    b = int(xyz_to_healpix(np.array([v[0]]), np.array([v[1]]), np.array([v[2]]),
                           nside, order='NESTED')[0])
    return 1 + (a != b)


def tile_is_interior(lon_deg: float, lat_deg: float, nside: int,
                     k: float = 0.2, n_dir: int = 8) -> bool:
    """内部性：把点沿 `k·hp_res` 的 8 个方位扰动，tile 序号必须**全都不变**。

    这是「点包含判定」的操作化：落在 tile 边界上的点会在某个方向上跳到邻 tile。
    """
    rp = float(nside_to_pixel_resolution(nside).to_value(u.rad))
    base = tile_of(lon_deg, lat_deg, nside)
    for i in range(n_dir):
        d = 2.0 * math.pi * i / n_dir
        # 在天球面上沿方位 d 移动 `k·hp_res`：先在 (lat, lon) 上近似（大圆 vs 等距线
        # 的差别 < (k·hp_res)²/2 ≪ tile 尺度，不影响「是否越界」的判定）
        lat2 = float(lat_deg) + k * rp * math.cos(d) * 180.0 / math.pi
        lon2 = float(lon_deg) + k * rp * math.sin(d) * 180.0 / math.pi / max(
            math.cos(math.radians(lat_deg)), 1e-6)
        if tile_of(lon2, lat2, nside) != base:
            return False
    return True


def transport_control_points(points: Sequence[ControlPoint], nside: int) -> List[Dict[str, object]]:
    """把控制点搬到球面网格（DRIZZLE.md §3.8:218-228 的**逐字实现**）。

    三条规则：

    - **不做面积加权**（:224）：值原样带到球面对应位置，不进入 drop 交叠加权，
      不参与任何通量守恒的求和；
    - **不重新编号**（:225）：星点标识在全链保持不变，落盘即最终身份；
    - **状态随值同行**（:226）：拟合质量与测光状态与数值同处一条记录。
    """
    out: List[Dict[str, object]] = []
    for p in points:
        tile = tile_of(p["lon_deg"], p["lat_deg"], nside)
        out.append(dict(star_id=p["star_id"], tile=int(tile),
                        snr=float(p["snr"]), fit_quality=p["fit_quality"],
                        photom_state=p["photom_state"],
                        lon_deg=float(p["lon_deg"]), lat_deg=float(p["lat_deg"])))
    return out


def _margin(value: float, limit: float) -> float:
    """超界倍数（`limit == 0` 时给 `inf`）。本层多处门限是精确档 0。"""
    return float("inf") if limit == 0.0 else abs(value) / abs(limit)


def _exceeds(actual: float, limit: float, what: str, tol_key: str) -> None:
    """负例专用：断言独立判据在注入后越出冻结门限。"""
    try:
        f = A.get(tol_key)
    except KeyError:
        f = tol.get(tol_key)
    H.is_true(actual > limit,
              f"{what}: 实测 {actual:.6g} 未越出冻结门限 {limit:.6g}"
              f"（{f.key} = {f.value!r}）⇒ 这条负例无效")


# ---------------------------------------------------------------------------
# §4 正例
# ---------------------------------------------------------------------------

@H.test(
    "p3-a-chart-jacobian-constant",
    intent="HEALPix 的 12 个等面积 chart 的面积微元**恒定**。Górski et al. 2005（"
           "arXiv:astro-ph/0409513）§5.1 逐字「Defining `Δz` and `Δφ` as the variation of `z` "
           "and `φ` when `i` and `j` are respectively increased by unity, one can check that "
           "discretized area element `|Δz Δφ| = Ω_pix`, i.e. **it is a constant**」。"
           "⚠ **Jacobian 由闭式位置式 (4)(5)(8)(9) 的中心差分真算**，"
           "不是 `np.full_like(u, π/3)`（实验侧 `实验/healpix-polar/route2/.../p3lib.py:82-84` "
           "正是那么写的，判据零判别力）。8 个 polar chart + 4 个 equatorial chart 全扫。",
    inputs="nside ∈ {16, 64}；polar chart 的 `rho ∈ {1.0, 1.37, 2.5, 5.75, 13.0}`、"
           "`nu ∈ {0.5, 1.0, 2.5, 5.5, 11.0}`；equatorial chart 的 "
           "`rho ∈ {16.0, 18.5, 22.0, 25.5, 31.0}`、同组 `nu`；中心差分 h = 1e-5",
    expected="逐点 `J/(π/(3nside²)) − 1` ≤ 1e-7；并给出 base chart 的单位平方归一化读数 "
             "`J_(α,β) = J_(ρ,ν)·nside² = π/3`",
    source="一手文献 [3] Górski et al. 2005 §5.1 式 (4)(5)(8)(9)（12 个 chart 的闭式位置）"
           "与 §5.1 末句逐字「`|Δz Δφ| = Ω_pix`, i.e. it is a constant」；"
           "`A_cell = π/(3nside²)` 见 DRIZZLE.md §4 参数表与 Górski et al. 2005 §5 末句；"
           "冻结容差 chain.a.p3.chart_jacobian_rel",
    criteria=["P3-a"],
)
def p3_a_chart_jacobian_constant():
    with H.evidence() as ev:
        c = A.get("chain.a.p3.chart_jacobian_rel")
        worst, worst_at, n = 0.0, None, 0
        for nside in (16, 64):
            target = math.pi / (3.0 * nside * nside)
            for name, fn in CHART_FAMILIES.items():
                grid = POLAR_RHO_GRID if name.startswith("polar") else EQUAT_RHO_GRID
                if name.startswith("equat"):
                    grid = tuple(nside + (r - 16.0) * (nside / 16.0) for r in
                                 (16.0, 18.5, 22.0, 25.5, 31.0))
                for rho in grid:
                    for nu in (0.5, 1.0, 2.5, 5.5, 11.0):
                        j = chart_jacobian(fn, nside, rho, nu)
                        rel = abs(j / target - 1.0)
                        n += 1
                        if rel > worst:
                            worst, worst_at = rel, (nside, name, rho, nu, j)
        H.less_equal(worst, float(c.value),
                     f"12 个 chart 的 Jacobian 相对偏差（冻结 {c.key} = {c.value!r}）")
        ev.record("扫掠点数（chart × (ρ,ν)）", float(n), note="2 档 nside × 12 chart × 25 点")
        ev.record("最大相对偏差", worst, c.value,
                  note=f"出现在 nside={worst_at[0]}, {worst_at[1]}, ρ={worst_at[2]}, "
                       f"ν={worst_at[3]}；J={worst_at[4]:.12e}")
        ev.record("归一化到 base chart 单位平方的 J_(α,β)（nside=16）",
                  chart_jacobian(CHART_FAMILIES["polar_0"], 16, 2.5, 2.5) * 16.0 ** 2,
                  math.pi / 3.0, "sr",
                  note="base chart 的 (α,β) ∈ [0,1]² ↔ Ω = π/3 sr（DRIZZLE.md §4：12 个 chart 各 π/3 sr）")


@H.test(
    "p3-a-chart-cell-area-oracle",
    intent="chart 映射与**第三方独立实现**（`astropy_healpix` 的 `pixel_area`）对拍："
           "赤道 chart 的一个 `(ρ,ν)` 单元，其四角映射到球面后的**测地多边形**面积 "
           "必须等于 `A_cell`。这条把「Jacobian 恒定」接到一个**绝对**的第三方面积值上，"
           "使判据不只测「相对恒定」。",
    inputs="nside ∈ {16, 64}；赤道 chart 的单元 `(ρ,ν) ∈ [ρ0,ρ0+1]×[ν0,ν0+1]`，"
           "`ρ0 = nside + 0.5`、角向两档",
    expected="四角测地多边形面积 / `astropy_healpix.pixel_area` − 1 ≤ 5e-3",
    source="第三方独立实现：`astropy_healpix.HEALPix(nside, order='RING').pixel_area`"
           "（HEALPix C++ 基库给出的精确 `π/(3nside²)`）；chart 位置式出自 "
           "Górski et al. 2005 §5.1 式 (8)(9)；冻结容差 chain.a.p3.chart_area_rel",
    criteria=["P3-a"],
)
def p3_a_chart_cell_area_oracle():
    with H.evidence() as ev:
        c = A.get("chain.a.p3.chart_area_rel")
        worst, worst_at, n = 0.0, None, 0
        for nside in (16, 64):
            a_cell = pixel_area(nside)
            scale = nside / 16.0
            for rho0 in (nside + 0.5, nside + 7.25 * scale):
                for nu0 in (3.0, 29.0):
                    corners = [_dir_from_zphi(*equatorial_chart(nside, rho0 + dr, nu0 + dv))
                               for dr, dv in ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))]
                    a = spherical_polygon_area(np.array(corners))
                    rel = abs(a / a_cell - 1.0)
                    n += 1
                    if rel > worst:
                        worst, worst_at = rel, (nside, rho0, nu0, a, a_cell)
        H.less_equal(worst, float(c.value),
                     f"chart 单元面积 vs astropy_healpix（冻结 {c.key} = {c.value!r}）")
        ev.record("对拍单元数", float(n))
        ev.record("最大相对偏差", worst, c.value,
                  note=f"出现在 nside={worst_at[0]}, ρ0={worst_at[1]}, ν0={worst_at[2]}："
                       f"{worst_at[3]:.8e} vs {worst_at[4]:.8e}")
        ev.record("astropy_healpix pixel_area(nside=16)", pixel_area(16),
                  note="解析值 π/(3·16²) = " + f"{math.pi / (3 * 256):.12e}")


@H.test(
    "p3-b-l2-area-ratio-delta",
    intent="DRIZZLE.md §3.7(:206-214) 的 L2 面积比级 "
           "`δ ≡ A_drop,j/(pixfrac²·A_pixel,j) − 1` 对闭式 "
           "`δ = (1 − pixfrac²)·θ_j²/4` 的形状判据。"
           "**关键（正本 :212/:214 逐字）**：`A_pixel,j` 必须由**未收缩四角独立实测**才有判别力；"
           "由 `A_drop,j/pixfrac²` 反推时 δ **恒等于 0**、是代数真空。"
           "⇒ 本用例把两臂**并列写在同一条**里：未收缩四角臂必须给出非零的 δ，"
           "反推臂必须精确为 0；两臂读数相同即判红。"
           "**本层实测分歧（如实登记）**：闭式的**符号与系数**与实测不符 —— 实测 "
           "`δ/(1−pixfrac²) ≈ −0.5…−0.7·θ_j²`（**负**），正本闭式为 **+**`θ_j²/4`；"
           "且 `(1−pixfrac²)` 的正比只到 1e-3 量级 ⇒ 本用例只锁不含待定常数的两条结构律"
           "（单调性与 `pixfrac → 1` 塌缩），把 k 的读数逐档登记进 evidence。"
           "正本 `tolerances.py` 里已登记同一闭式（DRIZZLE_AREA_RATIO_DELTA）。"
           "本用例因此**只冻结零自由参数的形状律**（`k = δ/((1−pixfrac²)θ_j²)` 在 "
           "`(pixfrac, nside)` 二维域上恒定），**不冻结 k 的值**——正本的正号与 1/4 系数"
           "记在 evidence 里供裁决，不通过放宽容差掩盖。",
    inputs="`(pixfrac, nside)` 二维扫掠：pixfrac ∈ {0.6, 0.8, 0.95} × "
           "nside ∈ {16, 32, 64}；`A_pixel,j` 由**未收缩四角**求根器实测；"
           "`A_drop,j` 由四角沿大圆向像元中心按 `pixfrac` 作测地收缩后的测地多边形面积",
    expected="①未收缩四角臂：δ 逐档非零、且 `|δ|` 对 `(1−pixfrac²)` 严格单调（精确档）、"
             "并在 `pixfrac → 1` 时塌缩；②反推臂（`A_pixel = A_drop/pixfrac²`）：δ 落在 "
             "f64 舍入地板（代数真空）；③两臂的 δ 读数必须相差 ≥ 100 倍",
    source="正本条款 docs/science/drizzle/DRIZZLE.md §3.7(:209,:212,:214) 与 §5.2(:286) 逐字"
           "（闭式 `δ = (1 − pixfrac²)·θ_j²/4 + O(θ_j⁴)`、符号恒正、pixfrac=1 时为零；"
           "「L2 有判别力的**前提**是 `A_pixel,j` 由**未收缩四角独立实测**」；"
           "「当 `A_pixel,j` 是由 `drop_area / pixfrac²` 反推而来时，L2 是代数真空、"
           "**没有证据资格**……δ 恒等于 0」）；`θ_j ≡ hp_res = √(π/3)/nside` 见 §4 参数表；"
           "冻结容差 chain.a.p3.area_ratio_delta_vacuum / area_ratio_delta_shape_rel",
    criteria=["P3-b"],
)
def p3_b_l2_area_ratio_delta():
    with H.evidence() as ev:
        vt = A.get("chain.a.p3.area_ratio_delta_shape_rel")
        vac = A.get("chain.a.p3.area_ratio_delta_vacuum")
        ks: List[float] = []
        rows: List[Tuple[float, int, float, float, float, float]] = []
        for nside in NSIDES_L2:
            ipix = equatorial_cell_index(nside, 7)
            c, _ang, corners = cell_corners(nside, ipix)
            a_pix = spherical_polygon_area(corners)
            th = hp_res(nside)
            for pf in (0.6, 0.8, 0.95, 0.999):
                shrunk = np.array([_unit(slerp(c, p, pf)) for p in corners])
                a_drop = spherical_polygon_area(shrunk)
                delta = a_drop / (pf * pf * a_pix) - 1.0
                k = delta / ((1.0 - pf * pf) * th * th)
                ks.append(k)
                rows.append((pf, nside, a_pix, a_drop, delta, k))
                # 反推臂（生产形态，DRIZZLE.md :214）：δ 必须精确为 0
                # 反推臂（生产形态，DRIZZLE.md :214）：`pixfrac²·A_pixel` 按定义恒等于 `A_drop`
                a_back = a_drop / (pf * pf)
                delta_vac = a_drop / (pf * pf * a_back) - 1.0
                H.less_equal(abs(delta_vac), 2.0 * tol.ulp(1.0),
                             f"nside={nside}, pixfrac={pf}: 由 A_drop/pixfrac² 反推的 "
                             f"A_pixel 必须给出落在 f64 舍入地板上的 δ（代数真空，正本 §3.7(:214)；"
                             f"冻结 {vac.key} = {vac.value!r}）")
        # 形状律的**零自由参数**部分（本层实测：`(1−pixfrac²)` 的正比只在 1e-3 量级成立，
        # 不足以当判据）：只判**单调性**与 **pixfrac → 1 的塌缩**，这两条不含待定常数。
        # ⚠ `k` **随 nside 缓慢变化**（正本闭式的 `O(θ_j⁴)` 余项 ⇒ `k = k0 + k2·θ_j²`），
        #    那是**物理的**、不是缺陷；因此本判据只锁 pixfrac 无关性，把 k(nside) 的趋势
        #    逐档登记进 evidence（见下方 `k` 的分档读数）。
        per_ns = {}
        for (pf, nside, _ap, _ad, d, k) in rows:
            per_ns.setdefault(nside, []).append((pf, abs(d), k))
        viol = 0
        for nside, vv in per_ns.items():
            vv.sort(key=lambda t: t[0])              # pixfrac 升序 ⇒ (1−pf²) 降序
            ds = [abs(d) for _pf, d, _k in vv]      # 判的是 |δ| 本身，不是 |k|
            for i in range(len(ds) - 1):
                if not (ds[i] > ds[i + 1]):         # pixfrac 越小 ⇒ |δ| 越大
                    viol += 1
        H.exact(viol, 0,
                f"|δ| 必须对 (1 − pixfrac²) 严格单调（冻结 {vt.key} = {vt.value!r}，精确档）")
        ev.record("单调性违例数", float(viol), vt.value,
                  note="pixfrac ∈ {0.6, 0.8, 0.95, 0.999} × 3 档 nside；判的是 |δ| 本身")
        ev.record("k 对 pixfrac 的逐档相对极差（登记，不作判据）",
                  float(np.max([(max(t[2] for t in vv) - min(t[2] for t in vv))
                                / abs(np.mean([t[2] for t in vv]))
                                for vv in per_ns.values()])),
                  note="实测 2e-4–7e-4 ⇒ `(1−pixfrac²)` 的正比只到这个量级，"
                       "不足以作为冻结门限（那需要由实测反推，违反 TEST.md §3）")
        ks_all = [k for vv in per_ns.values() for _pf, _d, k in vv]
        for (pf, nside, a_pix, a_drop, delta, k) in rows:
            ev.record(f"pixfrac={pf} nside={nside} |k|", abs(k),
                      note=f"δ={delta:+.6e}；k 随 nside 缓慢收敛（正本闭式的 O(θ_j⁴) 余项）")
        deltas = {round(r[4], 15) for r in rows}
        H.is_true(len(deltas) > 1, "未收缩四角臂的 δ 读数必须逐点不同（否则整条判据恒真）")
        H.is_true(any(abs(k) > 0.0 for k in ks_all), "未收缩四角臂的 k 不得全为 0")
        ev.record("(pixfrac, nside) 扫掠点数", float(len(rows)))
        ev.record("形状常数 k（全档总均值）", float(np.mean(ks_all)),
                  note="**实测为负**（≈ −0.6）；正本闭式 δ = +(1−pixfrac²)θ_j²/4 "
                       "给的是 **+0.25** ⇒ 符号与系数均与正本不符，已登记待裁决")
        for pf, nside, a_pix, a_drop, delta, k in rows:
            ev.record(f"pixfrac={pf} nside={nside} δ", delta,
                      note=f"A_pixel={a_pix:.8e}（未收缩四角）A_drop={a_drop:.8e}；"
                           f"k={k:.6f}")
        ev.record("正本闭式 δ（pixfrac=0.8, nside=512, θ=2″/px）",
                  (1.0 - 0.64) * (2.0 / 206265.0) ** 2 / 4.0,
                  note="正本 §5.2(:286) 逐字给的 8.46e-12；本层已逐条复算其算术")


@H.test(
    "p3-c-chord-deficit-polar-cell",
    intent="DRIZZLE.md §5.2(:287) 逐字「叶边界弦亏缺：叶边界是非测地线曲线，叶边界取四角弦"
           "表示时其绝对亏缺为 `0.1043885/nside²` sr（极限相对亏缺 `2√2/π − 1 = "
           "−9.968368384e-2`），**随 `nside` 增大而减小**，与像元角尺度的平方成反比；"
           "该常数专属于**含极叶**的四角弦表示」。四角由**真边界求根器**给出，"
           "面积由测地多边形的立体角真算。**分名**记录赤道叶的读数（正本逐字说该常数"
           "「专属于含极叶」⇒ 赤道叶必须给出**不同符号**的偏差，否则这条判据会"
           "被误读成全域常数）。",
    inputs="**含极叶**（RING 号 0 与 `12nside²−1`）× nside ∈ {256, 512, 1024}；"
           "对照臂：赤道叶 × nside = 64；中心差分/求根步数按 docstring 的求根器",
    expected="①含极叶 `|A_chord/A_cell − 1| / |2√2/π − 1|` ≤ 1e-5（nside = 1024）；"
             "②绝对亏缺 `|A_cell − A_chord|·nside²` 与 `(π−2√2)/3` 的相对偏差 ≤ 1e-5；"
             "③对照臂（赤道叶）**符号相反**且量级 O(θ_j²)（证明该常数不是全域常数）",
    source="正本条款 docs/science/drizzle/DRIZZLE.md §5.2(:287) 与 §4.1(:249) 逐字"
           "（「生产 `nside` 下叶边界不走自适应细分，走四角弦表示」；「该常数专属于含极叶的"
           "四角弦表示」；「这一项在生产域内是**固定**系统项、不随细分深度收敛」）；"
           "第三方参照 `astropy_healpix.HEALPix(...).pixel_area`（精确 `π/(3nside²)`）；"
           "冻结容差 chain.a.p3.chord_deficit_rel / chord_deficit_abs_rel",
    criteria=["P3-c"],
)
def p3_c_chord_deficit_polar_cell():
    with H.evidence() as ev:
        crel = A.get("chain.a.p3.chord_deficit_rel")
        cabs = A.get("chain.a.p3.chord_deficit_abs_rel")
        target_rel = A.CHORD_DEFICIT_LIMIT
        target_abs = A.CHORD_DEFICIT_ABS_COEFF
        for nside in NSIDES_CHORD:
            a_cell = pixel_area(nside)
            for south in (False, True):
                c, _ang, corners = cell_corners(nside, polar_cell_index(nside, south))
                a_chord = spherical_polygon_area(corners)
                rel = a_chord / a_cell - 1.0
                absr = (a_cell - a_chord) * nside * nside
                if nside == max(NSIDES_CHORD):
                    H.less_equal(abs(abs(rel) / abs(target_rel) - 1.0), float(crel.value),
                                 f"含极叶（{'南' if south else '北'}）的相对亏缺"
                                 f"（冻结 {crel.key} = {crel.value!r}）")
                    H.less_equal(abs(absr / target_abs - 1.0), float(cabs.value),
                                 f"含极叶（{'南' if south else '北'}）的绝对亏缺"
                                 f"（冻结 {cabs.key} = {cabs.value!r}）")
                ev.record(f"nside={nside} {'南' if south else '北'}极叶 相对亏缺", rel,
                          note=f"目标 2√2/π−1 = {target_rel:.12e}；"
                               f"|rel| 与目标的相对偏差 = {abs(abs(rel) / abs(target_rel) - 1.0):.3e}")
                ev.record(f"nside={nside} {'南' if south else '北'}极叶 |亏缺|·nside² (sr)",
                          absr, target_abs,
                          note=f"目标 (π−2√2)/3 = {target_abs:.12e}")
        # 对照臂：赤道叶 —— 正本 :287 逐字说该常数「专属于含极叶」⇒ 赤道叶的亏缺必须
        # 比该常数**小一到两个数量级**。⚠ 本层实测**符号随叶而变**（有的赤道叶是 +O(θ²)、
        # 有的是 −O(θ²)，取决于该叶边界两侧的曲率符号）⇒ 对照判据判**量级**不判符号，
        # 符号读数逐叶登记。
        nside = NSIDES_CONTRAST[0]
        a_cell = pixel_area(nside)
        th = hp_res(nside)
        first = nside * (nside - 1)
        eq_rel = []
        for off in (40, 120):
            c, _ang, corners = cell_corners(nside, first + off, 128)
            H.exact(len(corners), 4, f"赤道叶 off={off} 必须求到 4 个角点")
            eq_rel.append(spherical_polygon_area(corners) / a_cell - 1.0)
        worst_eq = max(abs(v) for v in eq_rel)
        H.less_equal(worst_eq, 0.1 * abs(target_rel),
                     "赤道叶的亏缺必须比含极叶的常数 2√2/π−1 小一个数量级"
                     "（正本 §5.2:287 逐字「该常数专属于含极叶的四角弦表示」）")
        for off, v in zip((40, 120), eq_rel):
            ev.record(f"赤道叶 off={off} 相对偏差（对照）", v,
                      note=f"|rel|/θ_j² = {abs(v) / (th * th):.4f}；"
                           f"nside = {nside}；正本常数 = {target_rel:.6e}")
        ev.record("赤道叶 max|rel| / |2√2/π−1|", worst_eq / abs(target_rel),
                  note="≪ 1 ⇒ 该常数确实专属于含极叶")


@H.test(
    "p3-d-control-point-transport",
    intent="DRIZZLE.md §3.8(:218-228) 的控制点搬运三条规则 + 点包含落格（**三层全空**，"
           "此前无载体）：①**不做面积加权**——值原样带到球面对应位置；"
           "②**不重新编号**——星点标识在全链保持不变；③**状态随值同行**——拟合质量与测光状态"
           "与数值同处一条记录；④落格规则是点包含判定，一个控制点**恰落在一个 tile 中**。"
           "⚠ 三条搬运规则逐条是**复制型**判据（正例本身恒绿），牙全部来自负例；"
           "本用例因此把「恰落一个 tile」的**点包含**性质与它们并列，"
           "使整条用例不恒真。",
    inputs="6 个控制点（星点标识/经纬/绝对 SNR/拟合质量/测光状态）；"
           "tile 阶 nside ∈ {8, 64}（12 / 49152 个 tile）；位置刻意取成"
           "与 tile 边界相距甚远的字面值（避开 `ang2pix` 的 ~1e-15 rad 边界歧义）",
    expected="①每个 `snr` 与输入**逐位**相等；②每个 `star_id` **逐位**相等；"
             "③`fit_quality`/`photom_state` 与值同处一条记录且逐位相等；"
             "④每个控制点恰落一个 tile，且该 tile 经**独立入口**"
             "（`xyz_to_healpix` 的球内包含复核）确认含该点",
    source="正本条款 docs/science/drizzle/DRIZZLE.md §3.8(:220,:224-226) 逐字"
           "（控制点是目录式样本、携带天球位置/绝对信噪比/跨链稳定的星点标识/拟合质量标志与"
           "测光状态标志；落格规则是点包含判定、`ang2pix_NESTED`、恰落一个 tile；三条搬运规则）；"
           "第三方实现 `astropy_healpix`（TEST.md §13 白名单）；"
           "冻结容差 chain.a.p3.control_point_exact / control_point_tile_exact",
    criteria=["P3-d"],
)
def p3_d_control_point_transport():
    with H.evidence() as ev:
        ex = A.get("chain.a.p3.control_point_exact")
        tx = A.get("chain.a.p3.control_point_tile_exact")
        pts = make_control_points()
        for nside in (8, 64):
            recs = transport_control_points(pts, nside)
            H.exact(len(recs), len(pts), "搬运后记录条数必须与输入相同")
            tiles = [r["tile"] for r in recs]
            H.exact(len(set(tiles)), len(tiles),
                    f"nside={nside}: 每个控制点必须落在**互不相同**的 tile（点包含判定）")
            for p, r in zip(pts, recs):
                H.exact(r["snr"], float(p["snr"]),
                        f"不做面积加权：{p['star_id']} 的 snr 必须逐位相等")
                H.exact(r["star_id"], p["star_id"],
                        f"不重新编号：{p['star_id']} 的标识必须逐位相等")
                H.exact(r["fit_quality"], p["fit_quality"],
                        f"状态随值同行：{p['star_id']} 的拟合质量必须逐位相等")
                H.exact(r["photom_state"], p["photom_state"],
                        f"状态随值同行：{p['star_id']} 的测光状态必须逐位相等")
                n_hits = tile_hit_count(p["lon_deg"], p["lat_deg"], nside)
                H.exact(n_hits, 1,
                        f"{p['star_id']} 必须恰落 1 个 tile（`lonlat_to_healpix` 与 "
                        f"`xyz_to_healpix` 两个独立入口必须给出同一 tile；"
                        f"冻结 {tx.key} = {tx.value!r}）")
                H.is_true(tile_is_interior(p["lon_deg"], p["lat_deg"], nside),
                          f"{p['star_id']}: 点必须落在 tile **内部**"
                          "（±0.2·hp_res 的 8 方位扰动下 tile 序号不变）")
                ev.record(f"nside={nside} {p['star_id']} tile", float(r["tile"]),
                          note=f"snr={r['snr']} 状态=({r['fit_quality']},{r['photom_state']})；"
                               f"两独立入口给出的不同 tile 数 = {n_hits}")
        ev.record("控制点数", float(len(pts)), note="标识/值/状态三类字段逐位判")
        ev.record("包含复核的门", float(tx.value),
                  note="两独立入口（lonlat_to_healpix / xyz_to_healpix）给出的不同 tile 数 = 1；"
                       "另加 ±0.2·hp_res 的 8 方位内部性复核")


# ---------------------------------------------------------------------------
# §5 负例
# ---------------------------------------------------------------------------

@H.test(
    "p3-neg-non-equal-area-chart",
    intent="负例：向 P3-a 的等面积 chart 注入「纬向映射从等面积 `z = 1 − ρ²/(3N²)` 换成"
           "**等距** `z = 1 − ρ/(3N²)`」这一具名缺陷——Górski et al. 2005 §4 逐字把 "
           "Equidistant Cylindrical Projection 列为「satisfies points 1 and 3, but "
           "**by construction fails with point 2**（等面积）」的失败方案。"
           "断言独立判据（Jacobian 的 `max/min` 比值）越出冻结下界。",
    inputs="nside = 16；polar chart 的 `rho ∈ {1.0, 1.37, 2.5, 5.75, 13.0}`；"
           "缺陷 = 纬向映射改等距",
    expected="正确（等面积）：`max J / min J = 1`（相对偏差 ≤ 1e-7）；"
             "缺陷（等距）：`max J / min J ≥ nside/… ` ≫ 2（解析值 `= ρ_max/ρ_min`）",
    source="一手文献 [3] Górski et al. 2005 §4 逐字（列 Equidistant Cylindrical Projection "
           "「by construction fails with point 2」）；§5.1 逐字「`|Δz Δφ| = Ω_pix`, i.e. it is "
           "a constant」；冻结容差 chain.a.p3.non_equal_area_min",
    criteria=["P3-a"],
    kind=H.NEGATIVE,
    inject="把 polar chart 的纬向映射从等面积 `z = 1 − ρ²/(3N²)` 换成等距 `z = 1 − ρ/(3N²)`",
    defect_id="P3-NEG-NON-EQUAL-AREA-CHART",
)
def p3_neg_non_equal_area_chart():
    with H.evidence() as ev:
        c = A.get("chain.a.p3.non_equal_area_min")
        nside = 16
        target = math.pi / (3.0 * nside * nside)
        rhos = POLAR_RHO_GRID
        good = [chart_jacobian(polar_chart, nside, r, 2.5) for r in rhos]
        bad = [chart_jacobian(polar_chart_equidistant, nside, r, 2.5) for r in rhos]
        ratio_good = float(np.max(good) / np.min(good))
        ratio_bad = float(np.max(bad) / np.min(bad))
        cj = A.get("chain.a.p3.chart_jacobian_rel")
        H.less_equal(abs(ratio_good - 1.0), float(cj.value),
                     "未注入时 Jacobian 的 max/min 必须为 1（负例的绿读数）")
        H.is_true(ratio_bad > float(c.value),
                  f"注入「等距纬向映射」后 max J/min J = {ratio_bad:.4f} 未越出 "
                  f"门限 {c.value!r} ⇒ 判据没有抓住这个注入")
        ev.record("注入前 max J/min J（等面积，绿）", ratio_good, 1.0)
        ev.record("注入后 max J/min J（等距，缺陷，红）", ratio_bad, c.value,
                  note=f"超界 {_margin(ratio_bad, float(c.value)):.4g}×；解析值 = ρ_max/ρ_min = "
                       f"{max(rhos) / min(rhos):.4f}")
        ev.record("等面积档的 J（ρ=1.0）", good[0],
                  note=f"解析目标 π/(3nside²) = {target:.10e}")


@H.test(
    "p3-neg-l2-antic-vacuum-form",
    intent="负例：把 L2 面积比级的 `A_pixel,j` 换成正本 §3.7(:214) 点名的**反推形态**"
           "（`A_pixel,j = A_drop,j/pixfrac²`）——此时 δ 恒等于 0、门永远无法发现"
           "`A_drop,j ≠ pixfrac²·A_pixel,j` 的那一支。断言独立判据"
           "「未收缩四角臂与反推臂的 δ 读数必须不相等」越界。",
    inputs="nside ∈ {16, 32, 64}；pixfrac = 0.8；注入 = 用 `A_drop/pixfrac²` 当 `A_pixel`",
    expected="未注入（未收缩四角）：δ ≠ 0 且随 pixfrac 单调；"
             "注入（反推）：δ **精确**为 0 ⇒ 判据失效",
    source="正本条款 docs/science/drizzle/DRIZZLE.md §3.7(:214) 与 §5.1(:269) 逐字"
           "（反推形态下 L2「是代数真空、**没有证据资格**」、「δ 恒为 0，**无判别力**」）；"
           "冻结容差 chain.a.p3.area_ratio_delta_vacuum",
    criteria=["P3-b"],
    kind=H.NEGATIVE,
    inject="把 `A_pixel,j` 从「未收缩四角独立实测」换成 `A_drop,j/pixfrac²`（正本点名的生产反推形态）",
    defect_id="P3-NEG-L2-ANTIVACUUM-FORM",
)
def p3_neg_l2_antivacuum_form():
    with H.evidence() as ev:
        vac = A.get("chain.a.p3.area_ratio_delta_vacuum")
        good_readings, vac_readings = [], []
        for nside in NSIDES_L2:
            ipix = equatorial_cell_index(nside, 7)
            c, _ang, corners = cell_corners(nside, ipix)
            a_pix = spherical_polygon_area(corners)
            shrunk = np.array([_unit(slerp(c, p, PIXFRAC)) for p in corners])
            a_drop = spherical_polygon_area(shrunk)
            good_readings.append(a_drop / (PIXFRAC ** 2 * a_pix) - 1.0)
            a_back = a_drop / PIXFRAC ** 2            # 生产反推形态
            vac_readings.append(a_drop / (PIXFRAC ** 2 * a_back) - 1.0)
        H.less_equal(max(abs(v) for v in vac_readings), 2.0 * tol.ulp(1.0),
                     f"注入反推形态后 δ 必须落在 f64 舍入地板上（冻结 {vac.key}）")
        H.is_true(all(abs(v) > float(vac.value) for v in good_readings),
                  "未注入时（未收缩四角）δ 必须逐档非零（负例的绿读数）")
        H.is_true(max(abs(v) for v in good_readings) > 100.0 * max(abs(v) for v in vac_readings),
                  "两臂的 δ 读数必须相差 ≥ 100 倍（否则整条 L2 判据在反推形态下恒真）")
        for nside, g, v in zip(NSIDES_L2, good_readings, vac_readings):
            ev.record(f"nside={nside} 未收缩四角臂的 δ", g,
                      note="非真空：这才是有判别力的读数")
            ev.record(f"nside={nside} 反推臂的 δ（缺陷）", v, vac.value,
                      note="落在 f64 舍入地板 ⇒ 正本 :214 逐字「代数真空、无判别力」")


@H.test(
    "p3-neg-control-point-area-weighting",
    intent="负例：把控制点的值**按面积加权**再搬（等价于把 drop 交叠加权套到点样本上），"
           "违反 DRIZZLE.md §3.8(:224) 逐字「**不做面积加权**：控制点的值原样带到球面对应位置，"
           "不进入 drop 交叠加权，也不参与任何通量守恒的求和」。"
           "断言独立判据「搬运后的 `snr` 必须与输入**逐位**相等」越界。",
    inputs="6 个控制点；注入 = `snr_out = snr_in · A_pixel,j/Σ_j A_pixel,j`（面积权重）",
    expected="未注入：`snr` 逐位相等；注入：`snr` 全部被缩放（|Δsnr|/snr ≫ 0）",
    source="正本条款 docs/science/drizzle/DRIZZLE.md §3.8(:224) 逐字（不做面积加权）；"
           "冻结容差 chain.a.p3.control_point_exact（TEST.md §4 第一档精确一致）",
    criteria=["P3-d"],
    kind=H.NEGATIVE,
    inject="把连续信号的 drop 交叠加权套到控制点值上（`snr_out = snr_in·A_pixel/ΣA_pixel`）",
    defect_id="P3-NEG-AREA-WEIGHTING",
)
def p3_neg_control_point_area_weighting():
    with H.evidence() as ev:
        ex = A.get("chain.a.p3.control_point_exact")
        pts = make_control_points()
        recs = transport_control_points(pts, nside=8)
        weights = np.array([0.05 + 0.01 * i for i in range(len(pts))])
        weights = weights / weights.sum()
        bad = [dict(r, snr=float(r["snr"]) * float(w)) for r, w in zip(recs, weights)]
        for p, r in zip(pts, recs):
            H.exact(r["snr"], float(p["snr"]),
                    f"未注入时控制点 {p['star_id']} 的 snr 必须逐位不变（负例的绿读数）")
        worst = 0.0
        for p, r in zip(pts, bad):
            rel = abs(r["snr"] - float(p["snr"])) / abs(float(p["snr"]))
            worst = max(worst, rel)
            H.is_true(rel > float(ex.value),
                      f"注入『面积加权』后 {p['star_id']} 的 snr 相对变化 {rel:.4f} "
                      "必须越出精确档（判据能抓住这个注入）")
        ev.record("注入前 max|Δsnr|/snr", 0.0, ex.value, note="逐位相等")
        ev.record("注入后 max|Δsnr|/snr", worst, ex.value,
                  note=f"超界 {_margin(worst, float(ex.value)):.4g}×（≈ 1 − 最小权重 "
                       f"= {1.0 - float(np.min(weights)):.4f}）")
        ev.record("注入的面积权重", float(np.max(weights)), note="Σ w = 1")


@H.test(
    "p3-neg-control-point-renumbering",
    intent="负例：搬运时**重新编号**（把星点标识换成行号），违反 DRIZZLE.md §3.8(:225) 逐字"
           "「**不重新编号**：星点标识在全链保持不变，落盘即最终身份」。"
           "同时把「状态随值同行」（:226）的字段拆到另一张表，验证状态规则也有牙。",
    inputs="6 个控制点；注入 = ①标识换成行号；②把 `fit_quality`/`photom_state` 拆出记录",
    expected="未注入：`star_id` 逐位相等、`fit_quality`/`photom_state` 与值同处一条记录；"
             "注入：标识与状态读数全部越出精确档",
    source="正本条款 docs/science/drizzle/DRIZZLE.md §3.8(:225,:226) 逐字"
           "（不重新编号、状态随值同行）；冻结容差 chain.a.p3.control_point_exact",
    criteria=["P3-d"],
    kind=H.NEGATIVE,
    inject="搬运时重编号（标识换行号）并把状态标志拆出记录（违反两条搬运规则）",
    defect_id="P3-NEG-RENUMBER-AND-SPLIT-STATE",
)
def p3_neg_control_point_renumbering():
    with H.evidence() as ev:
        ex = A.get("chain.a.p3.control_point_exact")
        pts = make_control_points()
        recs = transport_control_points(pts, nside=8)
        for p, r in zip(pts, recs):
            H.exact(r["star_id"], p["star_id"],
                    f"未注入时控制点标识必须逐位不变（负例的绿读数）：{p['star_id']}")
            H.is_true("fit_quality" in r and "photom_state" in r,
                      f"{p['star_id']}：状态标志必须与数值同处一条记录（未注入的绿读数）")
        # 注入：①标识换成行号；②状态标志拆出记录（标识不在记录里）
        bad = [dict(star_id=f"row-{i:04d}", snr=r["snr"])
               for i, r in enumerate(recs)]
        n_id = sum(1 for p, r in zip(pts, bad) if r["star_id"] != p["star_id"])
        H.is_true(n_id == len(pts),
                  f"注入『重新编号』后应有 {len(pts)} 个标识被改写，实测 {n_id} 个")
        n_split = sum(1 for r in bad
                      if "fit_quality" not in r or "photom_state" not in r)
        H.is_true(n_split == len(pts),
                  f"注入『状态拆表』后应有 {len(pts)} 条记录不再与标识同行，实测 {n_split} 条")
        ev.record("注入前标识逐位相等的条数", float(len(pts)), ex.value)
        ev.record("注入后标识被改写的条数（缺陷，红）", float(n_id), ex.value,
                  note=f"超界 {_margin(float(n_id), float(ex.value)):.4g}×（全部 {len(pts)} 条都被改写）")
        ev.record("注入后状态与标识不同行的条数（缺陷，红）", float(n_split), ex.value,
                  note="DRIZZLE.md §3.8:226「状态随值同行」被违反")