"""HEALPix 核的**参考实现**（单元层被测口径）+ S10 穷举 oracle 的几何面。

## 这个文件是什么

`lib/algorithms/shared/healpix/healpix_core.{h,cpp}` 的**逐行转写**（Python），
外加 `lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp` 里
`query_candidate_pixels` 的转写（S10 判据的被测口径面）。

**转写不等于复制**：本文件是测试侧独立代码集（`AGENTS.md` §8「测试代码是独立的一套
代码集…不与产品代码混放」），逐函数保留产品源码的行号锚与分支结构，便于逐条核对。

## 为什么参考实现不等于 oracle（对拍如何才是真的）

| 面 | 算法路径 | 真值来源 |
|---|---|---|
| 本文件（被测口径） | **逐面（base face）解析坐标**：`xyz_to_hp` 反解 (face, ix, iy)，`hp_to_xyz` 正解中心向量；嵌套位交错 `xy_to_nest` / `nest_to_xy` | — |
| Oracle（`test_healpix_kernel.py`） | **第三方独立实现**（`astropy-healpix` BSD-3-Clause / `healpy` GPL-2.0）的 C/Fortran 核心；候选枚举完备性另有**解析闭式球面多边形面积** | 第三方 + 闭式解析 |

⇒ 两条路径的输入相同、算法族不同（解析逐面反解 vs 查表式第三方核心），
对拍才有意义。**本文件任何输出都不得被反过来当作 expected**。

## 缺陷注入面（负例专用）

`Defect` 收集的是「注入哪个缺陷」的开关，全部默认 0 / 正确分支；负例用
`inject(...)` 上下文管理器临时打开，退出即还原（逐条可消融自证）。
每个开关在产品源码里对应**一处具体写法的替换**，见各开关的注释。

## 第三方库在本环境的可用性（实测登记，`TEST.md` §13「外部求解器不可用时，
相关判据记为未执行并说明缺什么，不以跳过冒充通过」）

- `astropy-healpix` 2.0.1：**可用**（`lonlat_to_healpix` / `healpix_to_lonlat` /
  `boundaries_lonlat` / `nside_to_pixel_area`）。注意它的入参必须是
  `astropy.units.Quantity`，`lonlat_to_healpix` 返回 lon ∈ [−180,180] 且单位 rad。
- `healpy` 1.20.1：**部分可用**。`ang2pix` / `pix2ang` / `boundaries` /
  `query_disc` / `nside2pixarea` 可用；⚠ `query_polygon`（本用于 S10 的多边形
  交叠查询）在本环境**抛 `ValueError: The truth value of an array with more than
  one element is ambiguous`**，属 healpy 1.20.1 与 numpy 2.x 的不兼容，
  **不可用**。S10 因此按 `TEST.md` §13 的降级口径改用**闭式解析 oracle**
  （Van Oosterom-Strackee 球面立体角 + 大圆裁剪，见本文件 §S10），不静默跳过。
- `astropy.coordinates.polygon`（`SphericalPolygonCrossIntersection`）：
  **本环境缺失**（astropy 7.0.1 的系统包未安装 `astropy/coordinates/polygon.py`，
  目录实测只有 angles/attributes/…/transformations，无 polygon）。
  ⇒ 同样是降级到闭式解析 oracle。
"""

from __future__ import annotations

import contextlib
import math
from dataclasses import dataclass, replace
from typing import Iterable, List, Sequence, Tuple

# ---------------------------------------------------------------------------
# 常数（逐字取自 lib/algorithms/shared/healpix/healpix_core.cpp:26-30）
# ---------------------------------------------------------------------------

KPI = 3.14159265358979323846264338327950288
KTWOPI = 2.0 * KPI
KHALFPI = 0.5 * KPI
KTWOTHIRD = 2.0 / 3.0
KROOT3 = 1.73205080756887729352744634150587237

ARCSEC_TO_RAD = KPI / (180.0 * 3600.0)
RAD_TO_ARCSEC = 1.0 / ARCSEC_TO_RAD

Vec3 = Tuple[float, float, float]


# ---------------------------------------------------------------------------
# 失败面（HEALPIX_MAPPING.md「非法输入的显式失败面」逐字：不返哨兵、不静默改写）
# ---------------------------------------------------------------------------

class HealpixInvalidArgument(ValueError):
    """对应产品的 `std::invalid_argument`。"""


class HealpixOverflow(OverflowError):
    """对应产品的 `std::overflow_error`。"""


# ---------------------------------------------------------------------------
# 缺陷注入面（负例专用；全部默认为正确写法）
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Defect:
    """被测口径上的具名缺陷开关。字段名 = 缺陷代号（见 `test_healpix_kernel.py`）。

    每个开关对应产品源码里**一处写法**的替换：

    | 字段 | 产品源码位置 | 正确写法 | 注入写法 |
    |---|---|---|---|
    | `nest2ring_face_offset` | `healpix_core.cpp:257` | `basehp = ipix / npface` | `basehp = (ipix/npface + 1) mod 12`（基面判定偏一，模 12 回绕等价于 face 映射表错位一行） |
    | `polar_phi_index_offset` | `healpix_core.cpp:205` | `phi_t = π(ns−y)/(2((ns−x)+(ns−y)))` | `phi_t` 的分子少 1（即极冠内 phi 索引偏一） |
    | `deg_normalize_mode` | `healpix_core.cpp:81` | `phi_t = fmod(phi, π/2)` | `phi_t = phi / (π/2) mod 1` 之类的「简单除法」归一 |
    | `cell_area_factor12` | `healpix_core.cpp:329` | `4π/(12 nside²)` | `4π/nside²`（漏因子 12） |
    | `child_offset` | `healpix_core.h:57` | `child = 4·parent + k` | `4·parent + k + off` |
    | `tile_shift_override` | `aio_upm.cpp` / `upm.cpp` | `tile_shift = 9` | 改成别的位移 |
    | `order_max` | `healpix_core.h:68` | `order ≤ 29` 越界显式拒 | 夹逼 / 静默降级 |
    | `nside_round_up` | `healpix_core.h:60` | 非 2 的幂 → 抛 | 静默向上取整 |
    | `ipix_domain_guard` | `healpix_core.cpp:255` | `ipix ≥ 12·npface` → (0,0) | 去掉域校验 |
    | `child_overflow_guard` | `healpix_core.cpp:321` | 溢位抛 `overflow_error` | 去掉溢位校验，返回回绕值 |
    | `tile_to_leaf_overflow_guard` | `healpix_core.h:97` | 同上 | 同上 |
    | `query_buffer_factor` | `spherical_overlap.cpp:1567` | `3.0·hp_res` | 缩小到 `f·hp_res` |
    """

    nest2ring_face_offset: int = 0
    polar_phi_index_offset: int = 0
    deg_normalize_mode: str = "fmod"
    cell_area_factor12: bool = True
    child_offset: int = 0
    tile_shift_override: int = -1
    order_max: int = 29
    order_clamp_mode: str = "reject"     # 注入 DEF-S11-ORDER-CLAMP 时改成 "clamp"
    nside_round_up: bool = False
    ipix_domain_guard: bool = True
    child_overflow_guard: bool = True
    tile_to_leaf_overflow_guard: bool = True
    query_buffer_factor: float = 3.0


DEFECT = Defect()


@contextlib.contextmanager
def inject(**kwargs):
    """临时注入具名缺陷；退出上下文后逐项还原（负例的消融自证靠这一条）。"""
    global DEFECT
    old = DEFECT
    DEFECT = replace(old, **kwargs)
    try:
        yield DEFECT
    finally:
        DEFECT = old


# ---------------------------------------------------------------------------
# C++ 语义助手
# ---------------------------------------------------------------------------

def _cxx_round(x: float) -> int:
    """C++ `std::round`（半数远离零）——Python 内建 `round` 是银行家舍入，不同。"""
    return int(math.floor(x + 0.5)) if x >= 0.0 else -int(math.floor(-x + 0.5))


def _fmod(a: float, b: float) -> float:
    """C++ `std::fmod` 的截断取余（不是 Python `%` 的地板取余）。"""
    return math.fmod(a, b)


# ---------------------------------------------------------------------------
# nside 校验（healpix_core.h:60-76）
# ---------------------------------------------------------------------------

def require_valid_nside(nside: int) -> int:
    """healpix_core.h:60 `require_valid_nside`：0 或非 2 的幂 → `invalid_argument`。

    返回值是**生效的** nside。注入 `nside_round_up=True` 时不抛错，而是静默向上取整
    （产品明文禁止的口径，`healpix_core.h:60` 逐字「禁静默向上取整」）。
    """
    if nside <= 0:
        raise HealpixInvalidArgument(
            f"healpix: nside must be a power of two (got {nside})")
    if DEFECT.nside_round_up:
        return 1 << (nside - 1).bit_length()      # 注入 DEF-S11-NSIDE-ROUNDUP
    if (nside & (nside - 1)) != 0:
        raise HealpixInvalidArgument(
            f"healpix: nside must be a power of two (got {nside})")
    return nside


def nside_to_order(nside: int) -> int:
    """healpix_core.h:68 `nside_to_order`。"""
    nside = require_valid_nside(nside)
    k = 0
    while (1 << k) < nside:
        k += 1
    if DEFECT.order_clamp_mode == "reject":
        if k > DEFECT.order_max:
            # 正本：order ≤ 29（HEALPIX_MAPPING「Preconditions」）。产品核
            # `healpix_core.h:60` 本身只查 2 的幂，该上限由调用方强制
            # （`lib/algorithms/coverage/src/stage2_common.cpp:93` 的
            #  `target_order > 29`、`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1249`
            #  的 `nside > (1u << 29)`）；本参考实现把它作为**核的显式失败面**落实，
            # 使 S11④ 有可判红的被测口径（见 test_healpix_kernel.py 的
            # HEALPIX_ORDER_CEILING_REGISTER 记录）。
            raise HealpixInvalidArgument(
                f"healpix: order {k} exceeds max order {DEFECT.order_max}")
    else:
        # 注入 DEF-S11-ORDER-CLAMP：越界被夹逼到上限（静默降级）
        k = min(k, DEFECT.order_max)
    return k


def order_to_nside(order: int) -> int:
    """healpix_core.h:76 `order_to_nside`。"""
    return 1 << order


# ---------------------------------------------------------------------------
# 位交错（healpix_core.cpp:38-55）
# ---------------------------------------------------------------------------

def xy_to_nest(ix: int, iy: int, bits: int) -> int:
    """healpix_core.cpp:38 `xy_to_nest`：x 偶数位、y 奇数位。"""
    ip = 0
    for i in range(bits):
        ip |= ((ix >> i) & 1) << (2 * i)
        ip |= ((iy >> i) & 1) << (2 * i + 1)
    return ip


def nest_to_xy(ip_low: int, bits: int) -> Tuple[int, int]:
    """healpix_core.cpp:48 `nest_to_xy`：`xy_to_nest` 的逆。"""
    ix = 0
    iy = 0
    for i in range(bits):
        ix |= ((ip_low >> (2 * i)) & 1) << i
        iy |= ((ip_low >> (2 * i + 1)) & 1) << i
    return ix, iy


# ---------------------------------------------------------------------------
# 球面向量（healpix_core.cpp:58-72）
# ---------------------------------------------------------------------------

def radec_to_xyz(ra_rad: float, dec_rad: float) -> Vec3:
    """healpix_core.cpp:58 `radec_to_xyz`。"""
    cd = math.cos(dec_rad)
    return (cd * math.cos(ra_rad), cd * math.sin(ra_rad), math.sin(dec_rad))


def xyz_to_radec(vx: float, vy: float, vz: float) -> Tuple[float, float]:
    """healpix_core.cpp:65 `xyz_to_radec`：ra 归一到 [0, 2π)，z 夹到 [−1,1]。"""
    ra_rad = math.atan2(vy, vx)
    if ra_rad < 0.0:
        ra_rad += KTWOPI
    z = vz
    if z > 1.0:
        z = 1.0
    if z < -1.0:
        z = -1.0
    return ra_rad, math.asin(z)


def vec_to_radec(v: Vec3) -> Tuple[float, float]:
    """球面向量 → (ra_deg, dec_deg)。`spherical_overlap.cpp` 的 `vec_to_radec` 同义。"""
    ra_rad, dec_rad = xyz_to_radec(v[0], v[1], v[2])
    return ra_rad * 180.0 / KPI, dec_rad * 180.0 / KPI


def normalize(v: Vec3) -> Vec3:
    """`spherical_overlap.cpp` 的 `normalize`（归一化到单位球面）。"""
    n = math.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
    return (v[0] / n, v[1] / n, v[2] / n)


def angular_distance(v1: Vec3, v2: Vec3) -> float:
    """`spherical_overlap.cpp` 的 `angular_distance<T>`（弧度）。"""
    c = v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2]
    if c > 1.0:
        c = 1.0
    if c < -1.0:
        c = -1.0
    return math.acos(c)


# ---------------------------------------------------------------------------
# base face 区域（healpix_core.cpp:33-34）
# ---------------------------------------------------------------------------

def is_north_polar(base: int) -> bool:
    """healpix_core.cpp:33。"""
    return 0 <= base <= 3


def is_south_polar(base: int) -> bool:
    """healpix_core.cpp:34。"""
    return 8 <= base <= 11


# ---------------------------------------------------------------------------
# xyz -> (face, ix, iy)（healpix_core.cpp:77-149，逐行转写）
# ---------------------------------------------------------------------------

def xyz_to_hp(vx: float, vy: float, vz: float, nside: int) -> Tuple[int, int, int]:
    """healpix_core.cpp:77 `xyz_to_hp`（迁移自 astrometry.net，BSD-3）。"""
    phi = math.atan2(vy, vx)
    if phi < 0.0:
        phi += KTWOPI
    if DEFECT.deg_normalize_mode == "fmod":
        phi_t = _fmod(phi, KHALFPI)
    else:
        # 注入 DEF-S11-DEG-NORM：把「取余归一」写成简单除法截断（phi_t 被放大到 ≥1）
        phi_t = phi / KHALFPI

    ns = nside
    base = 0
    ix = 0
    iy = 0

    if vz >= KTWOTHIRD or vz <= -KTWOTHIRD:
        north = vz >= KTWOTHIRD
        zz = vz if north else -vz
        coz = math.sqrt(vx * vx + vy * vy)
        kx = (coz / math.sqrt(1.0 + zz)) * KROOT3 * abs(ns * (2.0 * phi_t - KPI) / KPI)
        ky = (coz / math.sqrt(1.0 + zz)) * KROOT3 * ns * 2.0 * phi_t / KPI
        if north:
            xx = ns - kx
            yy = ns - ky
        else:
            xx = ky
            yy = kx
        ix = int(min(float(ns - 1), math.floor(xx)))
        iy = int(min(float(ns - 1), math.floor(yy)))
        if ix < 0:
            ix = 0
        if iy < 0:
            iy = 0
        sector = (phi - phi_t) / KHALFPI
        offset = _cxx_round(sector) % 4
        base = offset if north else (8 + offset)
    else:
        zunits = (vz + KTWOTHIRD) / (4.0 / 3.0)
        phiunits = phi_t / KHALFPI
        u1 = zunits + phiunits
        u2 = zunits - phiunits + 1.0
        xx = u1 * ns
        yy = u2 * ns
        sector = (phi - phi_t) / KHALFPI
        offset = _cxx_round(sector) % 4
        if xx >= ns:
            xx -= ns
            if yy >= ns:
                yy -= ns
                base = offset
            else:
                base = ((offset + 1) % 4) + 4
        else:
            if yy >= ns:
                yy -= ns
                base = offset + 4
            else:
                base = 8 + offset
        ix = int(math.floor(xx))
        iy = int(math.floor(yy))
        if ix < 0:
            ix = 0
        if iy < 0:
            iy = 0
        if ix >= ns:
            ix = ns - 1
        if iy >= ns:
            iy = ns - 1
    return base, ix, iy


# ---------------------------------------------------------------------------
# (face, ix, iy) -> xyz（healpix_core.cpp:155-226，逐行转写）
# ---------------------------------------------------------------------------

def hp_to_xyz(basehp: int, px: int, py: int, dx: float, dy: float,
              nside: int) -> Vec3:
    """healpix_core.cpp:155 `hp_to_xyz`（像素内分数位置 dx=dy=0.5 即中心）。"""
    ns = nside
    chp0 = basehp
    x = float(px) + dx
    y = float(py) + dy
    equatorial = True
    zfactor = 1.0

    if is_north_polar(chp0) and (x + y) > ns:
        equatorial = False
        zfactor = 1.0
    if is_south_polar(chp0) and (x + y) < ns:
        equatorial = False
        zfactor = -1.0

    if equatorial:
        chp = chp0
        zoff = 0.0
        phioff = 0.0
        x /= float(ns)
        y /= float(ns)
        if chp <= 3:
            phioff = 1.0
        elif chp <= 7:
            zoff = -1.0
            chp -= 4
        else:
            phioff = 1.0
            zoff = -2.0
            chp -= 8
        z = KTWOTHIRD * (x + y + zoff)
        phi = KPI / 4.0 * (x - y + phioff + 2.0 * chp)
        rad = math.sqrt(1.0 - z * z)
        return (rad * math.cos(phi), rad * math.sin(phi), z)

    if zfactor == -1.0:
        x, y = y, x
        x = ns - x
        y = ns - y
    if y == ns and x == ns:
        phi_t = 0.0
    else:
        nn = DEFECT.polar_phi_index_offset
        phi_t = KPI * (ns - y + nn) / (2.0 * ((ns - x) + (ns - y)))
    if phi_t < KPI / 4.0:
        vv = abs(KPI * (ns - x) / ((2.0 * phi_t - KPI) * ns) / KROOT3)
    else:
        vv = abs(KPI * (ns - y) / (2.0 * phi_t * ns) / KROOT3)
    z = (1.0 - vv) * (1.0 + vv)
    rad = math.sqrt(1.0 + z) * vv
    z *= zfactor
    if is_south_polar(chp0):
        phi = KHALFPI * (chp0 - 8) + phi_t
    else:
        phi = KHALFPI * chp0 + phi_t
    if phi < 0.0:
        phi += KTWOPI
    return (rad * math.cos(phi), rad * math.sin(phi), z)


# ---------------------------------------------------------------------------
# 公开 API（healpix_core.h:26-107）
# ---------------------------------------------------------------------------

def ang2pix_nest(nside: int, ra_deg: float, dec_deg: float) -> int:
    """healpix_core.cpp:233 `ang2pix_nest` → NESTED ipix。"""
    order = nside_to_order(nside)
    ns = 1 << order
    vx, vy, vz = radec_to_xyz(ra_deg * KPI / 180.0, dec_deg * KPI / 180.0)
    basehp, x, y = xyz_to_hp(vx, vy, vz, ns)
    npface = ns * ns
    return basehp * npface + xy_to_nest(x, y, order)


def pix2ang_nest(nside: int, ipix: int) -> Tuple[float, float]:
    """healpix_core.cpp:248 `pix2ang_nest` → (ra_deg, dec_deg)。

    ⚠ **唯一不抛的退化面**（HEALPIX_MAPPING「非法输入的显式失败面」逐字）：
    `ipix` 越界时返回 `(0.0, 0.0)`，与调用方约定一致。
    """
    ra_deg = 0.0
    dec_deg = 0.0
    order = nside_to_order(nside)
    ns = 1 << order
    npface = ns * ns
    if DEFECT.ipix_domain_guard and ipix >= 12 * npface:
        return ra_deg, dec_deg
    basehp = (ipix // npface + DEFECT.nest2ring_face_offset) % 12
    x, y = nest_to_xy(ipix % npface, order)
    rx, ry, rz = hp_to_xyz(basehp, x, y, 0.5, 0.5, ns)
    ra_rad, dec_rad = xyz_to_radec(rx, ry, rz)
    return ra_rad * 180.0 / KPI, dec_rad * 180.0 / KPI


def angular_distance_deg(ra1: float, dec1: float, ra2: float, dec2: float) -> float:
    """healpix_core.cpp:304 `angular_distance_deg`（度，**逐字转写**：`acos` 形式）。

    ⚠ **本函数在 1e-12 度量级上没有分辨力**：`acos` 在 `c → 1` 处丢掉一半有效位，
    绝对误差上界 `√(2u)·180/π ≈ 8.5e-7` 度（比 `HEALPIX_MAPPING.md`「Postconditions」
    冻结的 `1e-12` 度往返门粗 8 个数量级）；实测对本就重合的两点返回 `0.0`，
    而 `atan2` 形式能读出真实的 `6.36e-15` 度。
    ⇒ **判 1e-12 度往返门不能用本函数当尺子**，必须用
    `angular_distance_deg_stable`（`atan2(|u×v|, u·v)` 形式）。本函数保留是为了
    逐行核对产品实现（且产品自己也用它），不是判门工具。
    """
    d1 = dec1 * KPI / 180.0
    d2 = dec2 * KPI / 180.0
    dra = (ra2 - ra1) * KPI / 180.0
    c = math.sin(d1) * math.sin(d2) + math.cos(d1) * math.cos(d2) * math.cos(dra)
    if c > 1.0:
        c = 1.0
    if c < -1.0:
        c = -1.0
    return math.acos(c) * 180.0 / KPI


def angular_distance_deg_stable(ra1: float, dec1: float,
                                ra2: float, dec2: float) -> float:
    """大圆角距的**数值稳定**形式 `atan2(|u×v|, u·v)`（度）。

    与 `angular_distance_deg` 物理量完全相同（同一个闭式大圆角距），但条件数：
    `acos` 在 `θ → 0` 时误差按 `√` 放大（地板 `8.5e-7` 度），`atan2` 形式保持
    `~u·θ` 的相对精度，因此在 `θ ~ 1e-16` 度量级上仍可分辨。

    这不是「另立容差」：`TEST.md` §4.3 逐字「绝对容差只有在……才可判」——
    门的数值来自 `tolerances.HEALPIX_ROUNDTRIP_DEG.value`，本函数只决定**用什么尺子
    去测它**。用 `acos` 尺子测 1e-12 度的门属「不可满足的绝对容差」（§4.3）。
    """
    v1 = radec_to_xyz(ra1 * KPI / 180.0, dec1 * KPI / 180.0)
    v2 = radec_to_xyz(ra2 * KPI / 180.0, dec2 * KPI / 180.0)
    cross = _cross(v1, v2)
    s = math.sqrt(cross[0] ** 2 + cross[1] ** 2 + cross[2] ** 2)
    return math.atan2(s, _dot(v1, v2)) * 180.0 / KPI


def nested_local_to_xy(local: int, shift: int) -> Tuple[int, int]:
    """healpix_core.cpp:274 `nested_local_to_xy`。"""
    if shift >= 32:
        shift = 31
    return nest_to_xy(local, shift)


def xy_to_nested_local(x: int, y: int, shift: int) -> int:
    """healpix_core.cpp:279 `xy_to_nested_local`。"""
    if shift >= 32:
        shift = 31
    return xy_to_nest(x, y, shift)


def nested_local_to_fits_index(local: int, shift: int, tile_width: int) -> int:
    """healpix_core.cpp:288 `nested_local_to_fits_index`。"""
    x, y = nested_local_to_xy(local, shift)
    maxv = (tile_width - 1) if tile_width > 0 else 0
    return (maxv - x if x <= maxv else 0) * tile_width + y


def fits_index_to_nested_local(fits_index: int, shift: int, tile_width: int) -> int:
    """healpix_core.cpp:295 `fits_index_to_nested_local`。"""
    if tile_width == 0:
        return 0
    row = fits_index // tile_width
    col = fits_index % tile_width
    maxv = tile_width - 1
    x = (maxv - row) if row <= maxv else 0
    return xy_to_nested_local(x, col, shift)


def parent_nest(ipix: int, shift: int) -> int:
    """healpix_core.h:53 `parent_nest`：`ipix >> (2·shift)`。"""
    bits = 2 * shift
    if bits >= 64:
        # C++ 侧此为 UB；参考实现显式化，不静默回绕。
        raise HealpixOverflow(f"healpix: parent_nest shift {bits} overflows uint64")
    return ipix >> bits


def child_nest(ipix: int, shift: int) -> int:
    """healpix_core.h:57 / healpix_core.cpp:318 `child_nest`：`ipix << (2·shift)`。

    层级语义：`shift = 1` 时 `child = 4·parent + k`（`k ∈ {0,1,2,3}`）。
    """
    bits = 2 * shift
    if bits == 0:
        return ipix + DEFECT.child_offset
    if DEFECT.child_overflow_guard:
        if bits >= 64 or (ipix >> (64 - bits)) != 0:
            raise HealpixOverflow("healpix: child_nest shift overflow")
    return (ipix << bits) + DEFECT.child_offset


def leaf_to_tile_nest(leaf_ipix: int, leaf_order: int, tile_order: int) -> int:
    """healpix_core.h:80 `leaf_to_tile_nest`（层级倒挂 → `invalid_argument`）。"""
    if leaf_order < tile_order:
        raise HealpixInvalidArgument(
            "healpix: leaf_order < tile_order in leaf_to_tile_nest")
    return leaf_ipix >> (2 * (leaf_order - tile_order))


def tile_to_leaf_nest(tile_ipix: int, tile_order: int, leaf_order: int) -> int:
    """healpix_core.h:90 `tile_to_leaf_nest`（层级倒挂 → `invalid_argument`，
    移位溢出 → `overflow_error`）。"""
    if leaf_order < tile_order:
        raise HealpixInvalidArgument(
            "healpix: leaf_order < tile_order in tile_to_leaf_nest")
    bits = 2 * (leaf_order - tile_order)
    if bits == 0:
        return tile_ipix
    if DEFECT.tile_to_leaf_overflow_guard:
        if bits >= 64 or (tile_ipix >> (64 - bits)) != 0:
            raise HealpixOverflow("healpix: tile_to_leaf_nest shift overflow")
    return tile_ipix << bits


def pixel_resolution_arcsec(nside: int) -> float:
    """healpix_core.cpp:327 `pixel_resolution_arcsec` = √(4π/(12 nside²)) 弧秒。"""
    if nside == 0:
        return 0.0
    denom = 12.0 * float(nside) * float(nside)
    if not DEFECT.cell_area_factor12:
        denom = float(nside) * float(nside)          # 注入 DEF-S11-AREA-F12：漏因子 12
    return math.sqrt(4.0 * KPI / denom) * (180.0 * 3600.0 / KPI)


def hp_res_rad(nside: int) -> float:
    """`DRIZZLE.md` §4 叶尺度 `hp_res = √(π/3)/nside`（rad）。"""
    return math.sqrt(KPI / 3.0) / float(nside)


def npix(nside: int) -> int:
    """healpix_core.cpp:333 `npix` = `12·nside²`。"""
    return 12 * nside * nside


# ---------------------------------------------------------------------------
# tile 层拆解（`lib/algorithms/coverage/src/upm.cpp:1979-1987` 与
# `lib/infrastructure/aio/src/aio_upm.cpp:524-532` 的逐行转写）
#
# 正本 `HEALPIX_MAPPING.md`「Invariants」逐字：tile_shift=9、mask=(1<<18)-1。
# HiPS 标准 tile 宽 `2^shift = 512`（IVOA HiPS 1.0 Image tile packaging）。
# ---------------------------------------------------------------------------

def tile_shift() -> int:
    """`upm.cpp:1979` / `aio_upm.cpp:524` 的 `tile_shift`。

    正本冻结值 9。注入 DEF-S11-TILE-SHIFT 时改成别的位移（产品两处都是**字面常量**，
    不是可配置项 ⇒ 改错就是改错）。
    """
    return DEFECT.tile_shift_override if DEFECT.tile_shift_override >= 0 else 9


def tile_mask() -> int:
    """`upm.cpp:1980` / `aio_upm.cpp:525` 的 `mask` = `(1ULL << (2·tile_shift)) − 1`。"""
    return (1 << (2 * tile_shift())) - 1


def leaf_to_tile(leaf_ipix: int) -> int:
    """`upm.cpp:1982-1983`：tile ipix = `leaf_ipix >> (2·tile_shift)`。"""
    return leaf_ipix >> (2 * tile_shift())


def leaf_local(leaf_ipix: int) -> int:
    """`upm.cpp:1984`：tile 内 NESTED 局部索引 = `leaf_ipix & mask`。"""
    return leaf_ipix & tile_mask()


def leaf_to_tile_xy(leaf_ipix: int) -> Tuple[int, int]:
    """`upm.cpp:1986-1987`：`nested_local_to_xy(leaf_local(ipix), tile_shift)`。"""
    return nested_local_to_xy(leaf_local(leaf_ipix), tile_shift())


def tile_xy_to_leaf(tile_ipix: int, x: int, y: int) -> int:
    """逆映射（`aio_upm.cpp` 写出面的同一算子）：`(tile << 2·shift) | local(x,y)`。"""
    shift = tile_shift()
    return (tile_ipix << (2 * shift)) | xy_to_nested_local(x, y, shift)


# ---------------------------------------------------------------------------
# query_disc（healpix_core.cpp:426-512，官方四叉树下钻）
# ---------------------------------------------------------------------------

def _max_pixrad_order(order: int) -> float:
    """healpix_core.cpp:426 `max_pixrad_order`（官方 healpix_base.cc 的安全余量）。"""
    nside = float(1 << order)
    za = KTWOTHIRD
    phi_a = KPI / (4.0 * nside)
    t1 = 1.0 - 1.0 / nside
    zb = 1.0 - t1 * t1 / 3.0
    dot = za * zb + math.sqrt((1.0 - za * za) * (1.0 - zb * zb)) * math.cos(phi_a)
    return math.acos(min(1.0, max(-1.0, dot)))


def query_disc(nside: int, ra_deg: float, dec_deg: float,
               radius_arcsec: float) -> List[int]:
    """healpix_core.cpp:438 `query_disc`。

    语义 = **像素中心**落在盘内（与官方 `query_disc` 一致，见该函数注释）。
    输出已排序去重。
    """
    require_valid_nside(nside)
    order = nside_to_order(nside)
    radius_rad = radius_arcsec * KPI / (180.0 * 3600.0)
    dec_r = dec_deg * KPI / 180.0
    ra_r = ra_deg * KPI / 180.0
    cz = math.sin(dec_r)
    result: List[int] = []
    if radius_rad <= 0.0:
        result.append(ang2pix_nest(nside, ra_deg, dec_deg))
        return result
    if radius_rad >= KPI:
        total = 12 * (1 << order) * (1 << order)
        return list(range(total))

    cosrad = math.cos(radius_rad)
    crpdr = [(-1.0 if radius_rad + _max_pixrad_order(o) > KPI
              else math.cos(radius_rad + _max_pixrad_order(o))) for o in range(order + 1)]
    crmdr = [(1.0 if radius_rad - _max_pixrad_order(o) < 0.0
              else math.cos(radius_rad - _max_pixrad_order(o))) for o in range(order + 1)]

    stk = [(11 - i, 0) for i in range(12)]
    while stk:
        pix, o = stk.pop()
        on = 1 << o
        ra_c, dec_c = pix2ang_nest(on, pix)
        z = math.sin(dec_c * KPI / 180.0)
        phi = ra_c * KPI / 180.0
        cangdist = z * cz + math.sqrt(max(0.0, (1.0 - z * z) * (1.0 - cz * cz))) \
            * math.cos(phi - ra_r)
        if cangdist > 1.0:
            cangdist = 1.0
        if cangdist < -1.0:
            cangdist = -1.0
        if cangdist <= crpdr[o]:
            continue
        zone = 1 if cangdist < cosrad else (2 if cangdist <= crmdr[o] else 3)
        if o < order:
            if zone >= 3:
                sdist = 2 * (order - o)
                result.extend(range(pix << sdist, (pix + 1) << sdist))
            else:
                for i in range(4):
                    stk.append((4 * pix + (3 - i), o + 1))
        elif zone >= 2:
            result.append(pix)

    result.sort()
    return sorted(set(result))


# ---------------------------------------------------------------------------
# §S10 · 候选枚举口径（spherical_overlap.cpp:1535-1589 的转写）
# ---------------------------------------------------------------------------

def drop_bounding_circle(corners: Sequence[Vec3]) -> Tuple[Vec3, float]:
    """`spherical_overlap.cpp:1544-1557`：中心 = 顶点均值归一化，半径 = 最大顶点角距。"""
    cx = cy = cz = 0.0
    for v in corners:
        cx += v[0]
        cy += v[1]
        cz += v[2]
    center = normalize((cx, cy, cz))
    max_angle = 0.0
    for v in corners:
        ang = angular_distance(v, center)
        if ang > max_angle:
            max_angle = ang
    return center, max_angle


def query_candidate_pixels(corners: Sequence[Vec3], nside: int) -> List[int]:
    """`spherical_overlap.cpp:1535 query_candidate_pixels` 的转写。

    保守球冠半径 = `max_angle + buffer_factor·hp_res`；`buffer_factor` 默认 **3.0**
    （`DRIZZLE.md` §4 参数表逐字「候选查询缓冲 `3.0·hp_res` rad，保守候选查询圆盘，
    保证零漏选」）。负例 `DEF-S10-BUFFER` 通过 `inject(query_buffer_factor=…)` 缩小它。

    返回 NESTED ipix 列表（已排序去重）。
    """
    corners = list(corners)
    if not corners:
        return []
    center, max_angle = drop_bounding_circle(corners)
    buffer_rad = DEFECT.query_buffer_factor * hp_res_rad(nside)
    query_radius_rad = max_angle + buffer_rad
    ra_c, dec_c = vec_to_radec(center)
    return query_disc(nside, ra_c, dec_c, query_radius_rad * RAD_TO_ARCSEC)


# ---------------------------------------------------------------------------
# §S10 · 闭式解析 oracle：球面多边形交叠面积
#
# 真值来源（不依赖任何被测实现）：
#   - 像元四角：`healpy.boundaries(nside, ipix, nest=True)` 给出的单位球面角点
#     （第三方 GPL-2.0 实现，独立于本文件与产品）；
#   - 面积：Van Oosterom & Strackee 1983, IEEE TBME 30(2), 125–126 的球面三角形
#     立体角闭式 `Ω = 2·atan2(det[a,b,c], 1 + a·b + b·c + c·a)`（`DRIZZLE.md` §7.2 书目）；
#   - 裁剪：球面上的 Sutherland–Hodgman（逐边大圆平面裁剪）。
# ⚠ 像元区域取**四角弦表示**——这正是生产域内叶边界的取法
# （`DRIZZLE.md` §4.1：生产 `nside` 下叶边界恒为四角弦），故本 oracle 与下游
# 精确交叠计算用的是同一定义；弦亏缺由 `DRIZZLE.md` §5.2 给出闭式 `0.1043885/nside²` sr。
# ---------------------------------------------------------------------------

def _cross(a: Vec3, b: Vec3) -> Vec3:
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def _dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _det3(a: Vec3, b: Vec3, c: Vec3) -> float:
    return _dot(a, _cross(b, c))


def solid_angle(poly: Sequence[Vec3]) -> float:
    """球面多边形立体角（弧度制 sr），Van Oosterom–Strackee 扇形三角求和。

    有向：正向（从球外看逆时针）为正。
    """
    if len(poly) < 3:
        return 0.0
    a0 = poly[0]
    total = 0.0
    for i in range(1, len(poly) - 1):
        a = poly[i]
        b = poly[i + 1]
        num = _det3(a0, a, b)
        den = 1.0 + _dot(a0, a) + _dot(a, b) + _dot(b, a0)
        total += 2.0 * math.atan2(num, den)
    return total


def clip_to_great_circle(poly: Sequence[Vec3], normal: Vec3) -> List[Vec3]:
    """球面 Sutherland–Hodgman：保留 `normal·p ≥ 0` 的半空间。"""
    out: List[Vec3] = []
    m = len(poly)
    if m == 0:
        return out
    for i in range(m):
        s = poly[i]
        e = poly[(i + 1) % m]
        ds = _dot(normal, s)
        de = _dot(normal, e)
        if ds >= 0.0:
            out.append(s)
        if (ds > 0.0) != (de > 0.0):
            t = ds / (ds - de)
            out.append(normalize((s[0] + t * (e[0] - s[0]),
                                  s[1] + t * (e[1] - s[1]),
                                  s[2] + t * (e[2] - s[2]))))
    return out


def spherical_intersection_area(a: Sequence[Vec3], b: Sequence[Vec3]) -> float:
    """两个球面多边形的交叠面积（sr）；用四角弦表示的定义。"""
    if len(a) < 3 or len(b) < 3:
        return 0.0
    b = list(b)
    if solid_angle(b) < 0.0:
        b.reverse()                       # 统一成「从球外看逆时针」= 内法向朝内
    poly: List[Vec3] = list(a)
    for i in range(len(b)):
        normal = _cross(b[i], b[(i + 1) % len(b)])
        poly = clip_to_great_circle(poly, normal)
        if len(poly) < 3:
            return 0.0
    return abs(solid_angle(poly))


def polygon_area(poly: Sequence[Vec3]) -> float:
    """球面多边形面积（sr），无条件取正值。"""
    return abs(solid_angle(poly))


# ---------------------------------------------------------------------------
# §S10 · 构造：gnomonic drop 四角（DRIZZLE.md §3.1 的四角语义）
# ---------------------------------------------------------------------------

def gnomonic_drop_corners(ra_deg: float, dec_deg: float, pa_deg: float,
                          scale_rad: float, pixfrac: float) -> List[Vec3]:
    """在切点 `(ra_deg, dec_deg)` 处造一个 gnomonic（切平面投影）drop 四角。

    `DRIZZLE.md` §3.1 逐字：四角 = `pixelToSky((x ± half, y ± half))`，
    `half = 0.5·pixfrac`。本函数实现的就是这个映射（切点 + 像元尺度 `scale_rad`
    + 位置角 `pa_deg` + `pixfrac`），角点以单位向量给出。
    """
    a0 = math.radians(ra_deg)
    d0 = math.radians(dec_deg)
    p0 = (math.cos(d0) * math.cos(a0), math.cos(d0) * math.sin(a0), math.sin(d0))
    e_xi = (-math.sin(a0), math.cos(a0), 0.0)
    e_eta = (-math.sin(d0) * math.cos(a0), -math.sin(d0) * math.sin(a0), math.cos(d0))
    pa = math.radians(pa_deg)
    c = math.cos(pa)
    s = math.sin(pa)
    half = 0.5 * pixfrac * scale_rad
    corners: List[Vec3] = []
    for xi, eta in ((-half, -half), (half, -half), (half, half), (-half, half)):
        x = xi * c - eta * s
        y = xi * s + eta * c
        corners.append(normalize((p0[0] + x * e_xi[0] + y * e_eta[0],
                                  p0[1] + x * e_xi[1] + y * e_eta[1],
                                  p0[2] + x * e_xi[2] + y * e_eta[2])))
    return corners


# ---------------------------------------------------------------------------
# 密集取样域（`TEST.md` §3 固定种子；`HEALPIX_MAPPING.md`「Postconditions」与
# `GATES_AND_TOLERANCES.md` G-P1-WCS-RT 行都逐字警告自证门）
# ---------------------------------------------------------------------------

#: 固定种子（`TEST.md` §3「固定种子；同一输入重复运行逐字节同结果」）。
SAMPLE_SEED = 20260906


def dense_sample_domain(n_random: int = 4200, n_band: int = 1400,
                        n_polar: int = 260, n_face_edge: int = 320,
                        n_ra_seam: int = 220, seed: int = SAMPLE_SEED,
                        ) -> List[Tuple[float, float]]:
    """**独立密集取样域**：全域随机 + 赤道带 + 极区 + 基面边界 + RA 跨界锚点。

    刻意**不是**被测方的采样网格，也不是任何第三方实现的像素网格：
    全部由固定种子的 `numpy` 随机流与解析构造生成，与 `healpix_ref` 的内部结构无关。

    域的构成（每块都对应 `HEALPIX_MAPPING.md`「数值风险：极区」与任务书点名的覆盖项）：

    - **全域随机** `n_random` 点：`z ~ U(−1,1)`、`ra ~ U(0,360)`（球面均匀）；
    - **赤道带** `n_band` 点：`|z| ≤ 2/3`（赤道带/极冠分界面的内部）；
    - **极区** `n_polar` 点：`1 − |z| ≤ 1e-9`（逼近南北极）；
    - **基面边界** `n_face_edge` 点：`|z| = 2/3` 两侧 `±1e-12` 与 `φ = k·π/4`
      两侧 `±1e-12`，直接压在 12 基面的边界上；
    - **RA 跨界锚点** `n_ra_seam` 点：`ra ∈ {−1e-9, 0, 360, 360+1e-9, …}` 与
      其 ±`1e-9` 邻域（考验内部归一化到 [0,360)）。

    本函数**不含精确并列点**（恰好落在 `z = ±2/3` 或 `φ = kπ/4` 上）——
    那些点单列在 `face_boundary_tie_domain()`，因为并列点是三个独立实现 tie-break
    分歧的固有面，不属「硬门 mismatch == 0」的量测域。
    """
    import numpy as np

    rng = np.random.default_rng(seed)
    out: List[Tuple[float, float]] = []

    z = rng.uniform(-1.0, 1.0, n_random)
    ra = rng.uniform(0.0, 360.0, n_random)
    out.extend(zip(ra.tolist(), np.degrees(np.arcsin(np.clip(z, -1.0, 1.0))).tolist()))

    z = rng.uniform(-KTWOTHIRD, KTWOTHIRD, n_band)
    ra = rng.uniform(0.0, 360.0, n_band)
    out.extend(zip(ra.tolist(), np.degrees(np.arcsin(z)).tolist()))

    eps = 1e-9
    for i in range(n_polar):
        s = 1.0 if i % 2 == 0 else -1.0
        zz = s * (1.0 - rng.uniform(0.0, eps))
        out.append((float(rng.uniform(0.0, 360.0)),
                    float(np.degrees(np.arcsin(max(-1.0, min(1.0, zz)))))))

    # 基面边界：z = ±2/3 与 φ = k·π/4 两条边界的两侧邻域
    for _ in range(n_face_edge):
        s = 1.0 if rng.random() < 0.5 else -1.0
        zz = s * KTWOTHIRD + float(rng.uniform(-1e-12, 1e-12))
        zz = max(-1.0, min(1.0, zz))
        phi = float(rng.integers(0, 8)) * (KPI / 4.0) + float(rng.uniform(-1e-12, 1e-12))
        ra = (math.degrees(phi)) % 360.0
        out.append((ra, float(np.degrees(np.arcsin(zz)))))

    # RA 跨界锚点：0/360 两侧与跨 360 的等价点
    seam_base = [0.0, 360.0, -1e-9, 1e-9, 359.999999999, 360.000000001,
                 180.0, -180.0, 540.0, -0.5]
    for i in range(n_ra_seam):
        base = seam_base[i % len(seam_base)]
        out.append((float(base + rng.uniform(-eps, eps)),
                    float(rng.uniform(-89.999999, 89.999999))))
    return out


def face_boundary_sample_domain(include_ties: bool = False,
                                seed: int = SAMPLE_SEED) -> List[Tuple[float, float]]:
    """基面边界的**解析**锚点（不依赖随机流，可逐点复算）。

    12 基面的边界由两类曲线构成：赤道带/极冠分界面 `z = ±2/3`，以及面内的
    `φ = k·π/4`（`k = 0..7`）。本函数在每条边界上取解析点与其 `±1e-13` 邻点。

    `include_ties=False`（默认）时**剔除**恰好落在边界上的点（`z = ±2/3`、`φ = kπ/4`
    精确值）。理由：这类点属于三个独立实现在**并列打破（tie-break）**上的固有分歧
    面——同一物理点在 astropy-healpix / healpy / 参考实现上会落到不同像元（实测见
    `test_healpix_kernel.py::healpix.s11.roundtrip.dense_oracle` 的 tie 证据）。
    正本 `HEALPIX_MAPPING.md`「Postconditions · 外部 Oracle 承接」表第 1 行要求的
    「硬门 mismatch == 0」对应的正是**非并列**点集（产品自己的 1,000,000 全天随机点
    也不可能落在精确边界上）。
    """
    out: List[Tuple[float, float]] = []
    for k in range(8):
        phi = k * (KPI / 4.0)
        for sgn in (1.0, -1.0):
            zz = sgn * KTWOTHIRD
            for d in ((0.0,) if include_ties else (-1e-13, 1e-13)):
                z2 = max(-1.0, min(1.0, zz + d))
                ra = (math.degrees(phi) + 360.0) % 360.0
                out.append((ra, math.degrees(math.asin(z2))))
            for m in range(9):
                ph = phi + m * (KPI / 4.0) / 8.0
                out.append(((math.degrees(ph)) % 360.0,
                            math.degrees(math.asin(sgn * KTWOTHIRD))))
    for sgn in (1.0, -1.0):
        for m in range(73):
            zz = sgn * (KTWOTHIRD + (1.0 - KTWOTHIRD) * m / 72.0)
            for k in range(8):
                ph = k * (KPI / 4.0) + (m + 0.5) * (KPI / 4.0) / 72.0
                out.append(((math.degrees(ph)) % 360.0, math.degrees(math.asin(zz))))
    return out


def face_boundary_tie_domain() -> List[Tuple[float, float]]:
    """**精确并列点**：恰好落在 12 基面边界（`z = ±2/3`、`φ = k·π/4`）上的解析点。

    三个独立实现（参考实现 / astropy-healpix / healpy）在这类点上必然出现
    tie-break 分歧（同一物理点落到不同像元），这是**并列的固有性质**而不是缺陷。
    本域只用于给被测口径的**自洽往返**加压，并对第三方 tie-break 分歧取证，
    不参与「硬门 mismatch == 0」。
    """
    return face_boundary_sample_domain(include_ties=True)[:16 * 8]


def pixel_center_domain(nside: int, count: int, seed: int = SAMPLE_SEED,
                        ) -> List[int]:
    """像元索引域（往返回归的输入侧）：`[0, 12·nside²)` 上的固定种子抽样。

    索引的**约定**是 HEALPix 标准（Górski et al. 2005），第三方实现与产品实现共用，
    故此处不是「自网格」：`ang2pix` 面（独立密集角域）另取，见 `dense_sample_domain`。
    """
    import numpy as np

    rng = np.random.default_rng(seed + nside)
    total = npix(nside)
    return [int(v) for v in rng.integers(0, total, count)]


def exhaustive_pixel_indices(nside: int) -> Iterable[int]:
    """全域穷举索引。"""
    return range(npix(nside))
