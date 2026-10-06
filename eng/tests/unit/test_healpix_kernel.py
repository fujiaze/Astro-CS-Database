"""HEALPix 核单元测试集 —— 吸收 `审核包-R2/T02-门禁退役与判据清单.md` §2.1 的 **S10 / S11**。

## 这两条判据的来历（为什么是本单）

`T02-门禁退役与判据清单.md` §2.1 逐字：

- **S10**「欠采样重建候选枚举完备性 `false_negative=0`」——「判据是，**证据已失**
  （`healpix_drizzle/tests/` 目录不存在）」、「**穷举 oracle 须重建**；T09 不得当作已覆盖」；
- **S11**「HEALPix 四件套」——「部分（①②④ 实现在位；③ 无在位 oracle）」、「往返 oracle **须重建**」，
  且「③ 是**跨帧绝对可比性的几何前提**，丢了 P2 的『绝对 SNR』声明失去支撑」。

`T02` §8 待办第 2 条逐字：「**S10/S11 的证据资产已失**…判据仍在但 **oracle 需重建**」。
⇒ **本文件就是那个重建的 oracle。**

## Oracle 从哪来（`docs/engineering/testing/TEST.md` §13 逐字白名单）

| 面 | Oracle | 归属 |
|---|---|---|
| ang2pix / pix2ang | `astropy-healpix` 2.0.1（BSD-3-Clause）+ `healpy` 1.20.1（GPL-2.0，只读对照） | §13「HEALPix 几何的独立 Oracle」 |
| 候选枚举完备性 | **闭式解析**：Van Oosterom-Strackee 球面三角形立体角 + 球面 Sutherland–Hodgman 大圆裁剪；像元四角取自 `healpy.boundaries`（第三方，非本仓） | §13「外部求解器不可用时…降级到解析 oracle，不以跳过冒充通过」 |

⇒ **预期值没有任何一条由 `healpix_ref.py` 的输出反推**（`standards/05_INDEPENDENT_TEST_SUITE.md`
§1「不用空断言充数」+ `TEST.md` §2「恒真的比较没有证据资格」）。

## 取样域为什么不是被测方的网格（`TEST.md` §2 R3 + `GATES_AND_TOLERANCES.md` R3）

`HEALPIX_MAPPING.md`「Postconditions」与 `GATES_AND_TOLERANCES.md` 的 G-P1-WCS-RT 行都逐字点名
「7×7 网格上 <1e-6 px」是**自证门**（自网格 1.8e-12 px vs 离网格 3.10 px）。
本文件因此全部取样于 `healpix_ref.dense_sample_domain()` + `face_boundary_sample_domain()`：
固定种子（`TEST.md` §3）、全域随机 + 赤道带 + 极区 + 基面边界 + RA 跨界锚点，
**与被测实现的内部采样网格无关**。

## 负例语义（`harness.py` 逐字）

负例 = 注入具名缺陷后**断言独立判据能把它抓住**；注入后判据仍满足 ⇒ 判据恒绿 ⇒ 本用例判红。
每条负例把实测超界读数记进 `evidence`，由 `--verbose` 打印。

## 本文件登记的「未执行 / 降级 / 冲突」（`TEST.md` §13 与 §10 逐字要求如实登记）

1. **降级**：`healpy.query_polygon`（多边形交叠查询）在本环境抛
   `ValueError: The truth value of an array with more than one element is ambiguous`
   （healpy 1.20.1 与 numpy 2.x 不兼容）⇒ S10 的 oracle 改用闭式解析面积；
   `astropy.coordinates.polygon`（`SphericalPolygonCrossIntersection`）在本环境**未安装**
   ⇒ 同样降级。两者都**没有**用「跳过」冒充通过。
2. **产品自身的尺子不可用于 1e-12 度门**：`healpix_core.cpp:304 angular_distance_deg`
   用 `acos`，在 `c → 1` 处丢一半有效位，绝对误差上界 `√(2u)·180/π`。
   实测：**同一次**正确实现上的复合往返，该函数读出 `8.537736462515939e-07` 度
   （超 1e-12 度门 `8.54e5` 倍），而 `atan2(|u×v|, u·v)` 形式的
   `angular_distance_deg_stable` 读出 `0.0` 度 ⇒ 判门必须改用后者
   （证据见 `healpix.s11.roundtrip.floor_is_structural`）。
3. **正本与实现冲突 1（order 上限的落点）**：`HEALPIX_MAPPING.md`「Preconditions」写
   `order ≤ 29`，但「非法输入的显式失败面」清单里没有它的对应异常，且 `healpix_core.h:60`
   逐字只查 2 的幂 ⇒ 上限实际由调用方强制
   （`lib/algorithms/coverage/src/stage2_common.cpp:93`、`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1249`）。
   本参考实现把它作为核的显式失败面落实，使 S11④ 有可判红的被测口径（登记，不改产品）。
4. **正本与实现冲突 2（叶边界弦亏缺常数）**：`DRIZZLE.md` §5.2 登记逐叶面积主项
   「绝对亏缺 `0.1043885/nside²` sr … 随 `nside` 增大而减小」；实测**四角弦表示**的逐叶
   面积与 `A_cell` 的相对差是 `O(nside⁻²)`（极冠面 `−0.09447/nside²`、赤道面 `+0.18895/nside²`，
   二者严格 −2 倍），且四角弦四边形**铺满天球**（Σ 与 `4π` 之差 ≤ `2.1e-12` sr @ nside=64，`
   落在 `TEST.md` §4 归约档 `C·γ_n·Σ|terms|` 内），
   绝对量级为 `O(nside⁻⁴)` sr，与文档常数不吻合。复算入口见 `healpix_chord_tiling_note()`。
5. **正本 1e-12 度门的可测性边界**：同一物理量的两条独立实现（参考实现 vs astropy-healpix）
   的像元中心角差，在**全域**（含基面边界锚点）上实测随 `nside` 恶化：
   `nside=32: 3.43e-13`、`64: 3.43e-13`、`128: 3.44e-13`、`256: 1.35e-12`、
   `1024: 1.06e-11`、`4096: 2.13e-11` 度（超门）；只在随机子集上才稳定在 `1.4e-13`
   ⇒ **该跨实现角差门只在 `nside ≤ 16` 注册为门**（实测 ≤ `1.09e-13` 度，余量 ≥ 9.1×），
   更高阶读数只作证据登记。
"""

from __future__ import annotations

import functools
import math
from typing import Dict, List, Sequence, Tuple

import astropy.units as u
import astropy_healpix as ah
import healpy as hp
import numpy as np

from . import harness
from . import tolerances as tol
from . import healpix_ref as H

# ---------------------------------------------------------------------------
# 冻结容差（**直接取用 `tolerances.py`，本文件不新立容差**）
# ---------------------------------------------------------------------------

TOL_ROUNDTRIP_DEG = tol.HEALPIX_ROUNDTRIP_DEG.value          # 1e-12 度
TOL_MAX_ORDER = tol.HEALPIX_MAX_ORDER.value                   # 29
TOL_TILE_SHIFT = tol.HEALPIX_TILE_SHIFT.value                 # 9
TOL_TILE_MASK = tol.HEALPIX_TILE_MASK.value                   # (1<<18)-1
CELL_AREA_SR = tol.HEALPIX_CELL_AREA_SR                       # λ nside ↦ π/(3 nside²)

#: 正本 `HEALPIX_MAPPING.md`「Postconditions · 外部 Oracle 承接」表第 1 行逐字：
#: 「**硬门 mismatch == 0** 且往返角距 ≤ `1.2 × hp_res + 1e-9`（判据写在该测试源内）」。
#: 这不是本层新立的容差，是正本转录；本层不往 `tolerances.py` 增行（写域隔离）。
QUANT_BOUND_HP_RES = 1.2
QUANT_BOUND_FLOOR_ARCSEC = 1e-9

#: 跨实现角差门注册的 `nside` 域（更宽的域见本文件头 §「正本 1e-12 度门的可测性边界」）。
CROSS_ORACLE_MAX_NSIDE = 16

#: 主往返门注册的 `nside` 域（覆盖 order 0..6；更高阶由索引腿与证据登记覆盖）。
ROUNDTRIP_NSIDES = (1, 2, 4, 16, 64)
CROSS_ORACLE_NSIDES = (1, 2, 4, 8, 16)
EVIDENCE_NSIDES = (32, 64, 128, 256, 1024, 4096)


# ---------------------------------------------------------------------------
# 独立密集取样域（固定种子；模块级缓存，避免每条用例重建）
# ---------------------------------------------------------------------------

@functools.lru_cache(maxsize=1)
def dense_points() -> Tuple[Tuple[float, float], ...]:
    """独立密集域（**非并列点**）= 全域随机 + 赤道带 + 极区 + 基面边界邻域 + RA 跨界锚点
    + 解析面边界锚点。这是「硬门 mismatch == 0」的量测域。"""
    return (tuple(H.dense_sample_domain())
            + tuple(H.face_boundary_sample_domain(include_ties=False)))


@functools.lru_cache(maxsize=1)
def tie_points() -> Tuple[Tuple[float, float], ...]:
    """精确并列点（恰好落在 12 基面边界上）。只用于自洽往返加压 + 第三方 tie-break 取证。"""
    return tuple(H.face_boundary_tie_domain())


@functools.lru_cache(maxsize=1)
def all_points() -> Tuple[Tuple[float, float], ...]:
    """独立密集域 + 并列点（被测口径自洽往返的全域）。"""
    return dense_points() + tie_points()


#: 稳定性探测的扰动量（度）。1e-9 度 = 1.7e-11 rad，比 `nside=1024` 的像元
#: 角尺度（3.6e-3 度）小 6 个数量级 ⇒ 只有落在像元/基面边界 1e-9 度以内的点
#: 才会被扰动翻转。
STABILITY_DELTA_DEG = 1e-9


def stability_mask(nside: int, points, delta: float = STABILITY_DELTA_DEG):
    """「硬门 mismatch == 0」的量测域掩码。

    一个点被判为**稳定**当且仅当它在 `(ra, dec)`、`(ra±δ, dec)`、`(ra, dec±δ)`
    五处取值的像元归属完全一致（δ = `STABILITY_DELTA_DEG`）。

    理由（这是本次重建 oracle 的实测结论，不是先验假设）：HEALPix 的基面边界
    （`z = ±2/3`）与面内像元边界都是**非测地线曲线**，落在边界 1 ulp 邻域内的点，
    三个独立实现（参考实现 / astropy-healpix / healpy）各自的 FP 表达式会把它判到
    **不同的**像元上——这是并列的固有性质，不是任何一方的缺陷。实测（见
    `healpix.s11.roundtrip.dense_oracle`）在 `nside = 1…1024` 上全部第三方分歧点
    都落在不稳定域内，稳定域内 mismatch 恒为 0。

    判据仍然跑在**全域**上（自洽往返的索引腿与 1e-12 度角腿），只是
    「与第三方逐点对拍 mismatch == 0」这条**跨实现**判据限定在稳定域。
    """
    q0 = np.fromiter((H.ang2pix_nest(nside, ra, dec) for ra, dec in points),
                     np.int64, len(points))
    keep = q0
    for dra, ddec in ((delta, 0.0), (-delta, 0.0), (0.0, delta), (0.0, -delta)):
        q = np.fromiter(
            (H.ang2pix_nest(nside, ra + dra, dec + ddec) for ra, dec in points),
            np.int64, len(points))
        keep = np.where(q == q0, keep, np.int64(-1))
    return keep >= 0



# ---------------------------------------------------------------------------
# Oracle 门面（第三方独立实现；本文件任何 expected 都不由 healpix_ref 反推）
# ---------------------------------------------------------------------------

def oracle_ang2pix_astropy(nside: int, ra_deg, dec_deg) -> np.ndarray:
    """`astropy-healpix`（BSD-3-Clause）`lonlat_to_healpix`，NESTED。"""
    return np.asarray(ah.lonlat_to_healpix(
        np.asarray(ra_deg) * u.deg, np.asarray(dec_deg) * u.deg,
        nside, order="NESTED"), dtype=np.int64)


def oracle_pix2ang_astropy(nside: int, ipix) -> Tuple[np.ndarray, np.ndarray]:
    """`astropy-healpix` `healpix_to_lonlat`，ra 归一到 [0,360)。"""
    lon, lat = ah.healpix_to_lonlat(np.asarray(ipix, dtype=np.int64), nside,
                                    order="NESTED")
    return (np.asarray(lon.to_value(u.deg)) % 360.0,
            np.asarray(lat.to_value(u.deg)))


def oracle_ang2pix_healpy(nside: int, ra_deg, dec_deg) -> np.ndarray:
    """`healpy`（GPL-2.0，只读对照）`ang2pix`（入参是 theta=余纬、phi=赤经，弧度）。"""
    ra = np.asarray(ra_deg, dtype=float)
    dec = np.asarray(dec_deg, dtype=float)
    return np.asarray(hp.ang2pix(nside, np.deg2rad(90.0 - dec),
                                  np.deg2rad(ra), nest=True), dtype=np.int64)


def oracle_pix2ang_healpy(nside: int, ipix) -> Tuple[np.ndarray, np.ndarray]:
    """`healpy` `pix2ang` → (ra_deg, dec_deg)。"""
    theta, phi = hp.pix2ang(nside, np.asarray(ipix, dtype=np.int64), nest=True)
    return (np.degrees(phi) % 360.0, 90.0 - np.degrees(theta))


def closed_hp_res_rad(nside: int) -> float:
    """闭式叶尺度 `hp_res = √(π/3)/nside` rad（`DRIZZLE.md` §4 参数表逐字）。

    **闭式**，不是 `healpix_ref` 的输出，也不是任何被测方的输出。
    """
    return math.sqrt(math.pi / 3.0) / float(nside)


def closed_hp_res_arcsec(nside: int) -> float:
    """闭式叶尺度（角秒）。"""
    return closed_hp_res_rad(nside) * 180.0 * 3600.0 / math.pi


# ---------------------------------------------------------------------------
# 往返统计
# ---------------------------------------------------------------------------

def _max_stable_ang(a: Sequence[Tuple[float, float]],
                    b: Sequence[Tuple[float, float]]) -> float:
    return max(H.angular_distance_deg_stable(p[0], p[1], q[0], q[1])
               for p, q in zip(a, b))


def roundtrip_stats(nside: int, points: Sequence[Tuple[float, float]]) -> Dict[str, float]:
    """在给定点集上跑 `ang2pix → pix2ang → ang2pix → pix2ang` 复合往返。

    返回四组统计量：

    - `idx_mismatch`：正向两次 `ang2pix` 的索引不一致数（精确档，判据 = 0）；
    - `ang_residual_deg`：正向两次 `pix2ang` 输出的最大球面角距（`TOL_ROUNDTRIP_DEG`）；
    - `rev_idx_mismatch`：反向（索引域起手）两次 `ang2pix` 的索引不一致数；
    - `rev_ang_residual_deg`：反向两次 `pix2ang` 输出的最大球面角距。
    """
    q1 = [H.ang2pix_nest(nside, ra, dec) for ra, dec in points]
    c1 = [H.pix2ang_nest(nside, q) for q in q1]
    q2 = [H.ang2pix_nest(nside, c[0], c[1]) for c in c1]
    c2 = [H.pix2ang_nest(nside, q) for q in q2]
    idx_mm = sum(1 for a, b in zip(q1, q2) if a != b)
    rev_mm, rev_ang = _reverse_leg_stats(nside, points)
    return {
        "idx_mismatch": idx_mm,
        "ang_residual_deg": _max_stable_ang(c1, c2),
        "rev_idx_mismatch": rev_mm,
        "rev_ang_residual_deg": rev_ang,
        "n_points": len(points),
    }


def _reverse_leg_stats(nside: int, points: Sequence[Tuple[float, float]]
                       ) -> Tuple[int, float]:
    """反向腿 `pix2ang → ang2pix → pix2ang`：起手点是**索引域**上固定种子抽样。"""
    total = H.npix(nside)
    idxs = H.pixel_center_domain(nside, min(len(points), 2000))
    c1 = [H.pix2ang_nest(nside, q) for q in idxs]
    q2 = [H.ang2pix_nest(nside, c[0], c[1]) for c in c1]
    c2 = [H.pix2ang_nest(nside, q) for q in q2]
    del total
    return (sum(1 for a, b in zip(idxs, q2) if a != b),
            _max_stable_ang(c1, c2))


# ---------------------------------------------------------------------------
# S10 oracle 辅助
# ---------------------------------------------------------------------------

@functools.lru_cache(maxsize=8)
def pixel_corner_vectors(nside: int, ipix: int) -> Tuple[Tuple[float, float, float], ...]:
    """像元四角单位向量，来源 = **`healpy.boundaries`**（第三方），不是本仓实现。

    `hp.boundaries(nside, ipix, nest=True)` 返回 `(3, 4)`，三行分别是 x / y / z 分量。
    """
    b = hp.boundaries(nside, np.int64(ipix), nest=True)
    return tuple((float(b[0][k]), float(b[1][k]), float(b[2][k])) for k in range(4))


def oracle_overlap_superset(nside: int, center: Tuple[float, float, float],
                            radius_rad: float) -> set:
    """候选叶的**穷举取样超集**：第三方 `healpy.query_disc(inclusive=True, fact=32)`
    ∪ 其 8 邻域。

    取 `fact=32` 是为了让「像元与圆盘相交」的判定在 `32·nside` 的分辨率上做，
    再并上 8 邻域闭包，覆盖「像元只是擦到圆盘边缘」的情形。取样集的正确性另有
    一条正例与**全域穷举**逐点比对（`healpix.s10.oracle_is_exhaustive`）。
    """
    disc = hp.query_disc(nside, np.asarray(center, dtype=float), radius_rad,
                         nest=True, inclusive=True, fact=32)
    out = {int(v) for v in disc}
    if out:
        nb = hp.get_all_neighbours(nside, np.array(sorted(out), dtype=np.int64),
                                   nest=True)
        for row in np.atleast_2d(nb):
            out.update(int(v) for v in row if v >= 0)
    return out


def oracle_true_overlaps(corners, nside: int, candidate_ids) -> Dict[int, float]:
    """**精确**交叠面积集合（闭式解析）：`{ipix: a_jp}`，`a_jp > 0` 即真交叠。

    `a_jp` 由球面 Sutherland–Hodgman 大圆裁剪 + Van Oosterom–Strackee 立体角闭式算出，
    像元四角取自 `healpy.boundaries`（第三方），与被测实现无共享代码路径。
    """
    out: Dict[int, float] = {}
    for ipix in sorted(candidate_ids):
        area = H.spherical_intersection_area(
            corners, pixel_corner_vectors(nside, ipix))
        if area > 0.0:
            out[ipix] = area
    return out


def s10_drop_geometries(nside: int, count: int, seed_offset: int = 0
                        ) -> List[Tuple[float, float, float, float, float]]:
    """S10 用的 drop 几何（固定种子）：`(ra, dec, pa, scale_rad, pixfrac)`。

    覆盖：全域随机 + 压在赤道带/极冠分界面 `|z| = 2/3` 附近（该处叶外接半径最坏，
    `DRIZZLE.md` §4.2 的穷举序列落在 `|z| = 2/3` 的边界叶上）。
    """
    rng = np.random.default_rng(H.SAMPLE_SEED + 101 + seed_offset)
    hp_res = closed_hp_res_rad(nside)
    out = []
    for i in range(count):
        if i % 3 == 0:
            z = float(np.sign(rng.uniform(-1.0, 1.0)) * 2.0 / 3.0
                      + rng.uniform(-0.05, 0.05))
            z = max(-1.0, min(1.0, z))
            dec = math.degrees(math.asin(z))
        else:
            dec = math.degrees(math.asin(float(rng.uniform(-1.0, 1.0))))
        out.append((
            float(rng.uniform(0.0, 360.0)), dec, float(rng.uniform(0.0, 180.0)),
            float(rng.choice([0.5, 1.0, 1.5, 2.0])) * hp_res,
            float(rng.choice([0.6, 0.8, 1.0])),
        ))
    return out


def s10_measure(nside: int, drops, buffer_factor: float = 3.0):
    """对一批 drop 统计「保守候选枚举 vs 精确交叠集合」的漏选数。

    返回 `(n_drops, n_true, false_negative, missed_ids, candidate_total)`。
    """
    false_neg = 0
    n_true = 0
    cand_total = 0
    missed: List[int] = []
    for ra, dec, pa, scale, pixfrac in drops:
        corners = H.gnomonic_drop_corners(ra, dec, pa, scale, pixfrac)
        center, max_angle = H.drop_bounding_circle(corners)
        sup = oracle_overlap_superset(nside, center,
                                      max_angle + 2.0 * closed_hp_res_rad(nside))
        truth = oracle_true_overlaps(corners, nside, sup)
        n_true += len(truth)
        with H.inject(query_buffer_factor=buffer_factor):
            cand = set(H.query_candidate_pixels(corners, nside))
        cand_total += len(cand)
        gap = sorted(set(truth) - cand)
        false_neg += len(gap)
        if gap:
            missed.append((nside, ra, dec, pa, scale, pixfrac, gap[0],
                           truth[gap[0]]))
    return len(drops), n_true, false_neg, missed, cand_total


# ===========================================================================
# S11 ① 叶面积  A = 4π/(12·nside²) = π/(3·nside²)
# ===========================================================================

@harness.test(
    "healpix.s11.cell_area.closed_form_conservation",
    intent="叶面积闭式 π/(3·nside²) 与天球守恒式 12·nside²·A_cell = 4π 精确成立；"
           "角尺度 √A_cell 与 hp_res 一致。",
    inputs="nside ∈ {1,2,4,8,16,32,64,128,256}（order 0..8），闭式 `math.pi/(3·nside²)`。",
    expected="守恒残差 `(12·nside²·A_cell − 4π)/(4π)` 逐 nside 精确为 0（精确档，闭式恒等）；"
             "`pixel_resolution_arcsec` 与闭式 `√A_cell·(180·3600/π)` 落 f64 非归约档。",
    source="docs/science/drizzle/DRIZZLE.md §4 参数表「叶面积 A_cell = π/(3·nside²)」；"
           "docs/science/algorithms/HEALPIX_MAPPING.md「Postconditions」；"
           "eng/tests/unit/tolerances.py#healpix.cell_area_sr（已冻结）。"
           "守恒式 4π = 12·nside²·Ω_pix 的文献锚：Górski et al. 2005, ApJ 622, 759，"
           "「A HEALPix map has Npix = 12Nside² pixels of the same area Ωpix = π/(3Nside²)」。",
    criteria=["S11"],
)
def _s11_cell_area_closed_form():
    nsides = (1, 2, 4, 8, 16, 32, 64, 128, 256)
    with harness.evidence() as ev:
        worst_cons = 0.0
        worst_res = 0.0
        for nside in nsides:
            a_cell = CELL_AREA_SR.value(nside)                 # 闭式
            harness.close(a_cell, math.pi / (3.0 * nside ** 2),
                          rtol=0.0, atol=0.0,
                          what=f"nside={nside} 冻结容差表给出值与闭式不一致")
            # 守恒式：像元数 × 单叶面积 = 天球总面积（闭式恒等，精确档）
            cons = (H.npix(nside) * a_cell - 4.0 * math.pi) / (4.0 * math.pi)
            worst_cons = max(worst_cons, abs(cons))
            # 角尺度：角分辨率 = √(单叶面积)
            res = H.pixel_resolution_arcsec(nside)
            res_closed = math.sqrt(a_cell) * 180.0 * 3600.0 / math.pi
            harness.close(res, res_closed, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * max(abs(res), 1.0),
                          scale=max(abs(res), 1.0),
                          what=f"nside={nside} pixel_resolution_arcsec")
            worst_res = max(worst_res, abs(res - res_closed) / res_closed)
            ev.record(f"A_cell(nside={nside})", a_cell, unit="sr")
        ev.record("worst_conservation_rel", worst_cons, 0.0,
                  note="12·nside²·A_cell − 4π 的相对残差（闭式恒等，应精确为 0）")
        ev.record("worst_resolution_rel", worst_res, tol.F64_RTOL)
        harness.less_equal(worst_cons, 0.0, what="天球守恒残差")
        harness.less_equal(worst_res, tol.F64_RTOL, what="角尺度相对残差")


@harness.test(
    "healpix.s11.cell_area.thirdparty_oracle",
    intent="叶面积与两个第三方独立实现（healpy GPL-2.0、astropy-healpix BSD-3-Clause）"
           "对拍，mismatch 必须为 0 —— 预期值不来自本仓任何实现。",
    inputs="nside ∈ {1,2,4,8,16,32,64,128,256}。",
    expected="`hp.nside2pixarea(nside)` 与 `ah.nside_to_pixel_area(nside)` 各自与闭式 "
             "`π/(3·nside²)` 在 rtol=1e-12 上一致；两者互差的相对值同样 ≤ 1e-12。",
    source="docs/engineering/testing/TEST.md §13 表逐字：「astropy-healpix = HEALPix 几何的"
           "独立 Oracle（测试侧，不进产品）」；上游正本 "
           "docs/science/drizzle/DRIZZLE.md §4 与 Górski et al. 2005, ApJ 622, 759。"
           "healpy 仅作对照（GPL-2.0，只读不复制）。",
    criteria=["S11"],
)
def _s11_cell_area_thirdparty():
    nsides = (1, 2, 4, 8, 16, 32, 64, 128, 256)
    with harness.evidence() as ev:
        worst_hp = 0.0
        worst_ah = 0.0
        for nside in nsides:
            closed = CELL_AREA_SR.value(nside)
            a_hp = float(hp.nside2pixarea(nside))
            a_ah = float(ah.nside_to_pixel_area(nside).to_value(u.sr))
            harness.close(a_hp, closed, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * closed, scale=closed,
                          what=f"healpy nside2pixarea(nside={nside})")
            harness.close(a_ah, closed, rtol=tol.F64_RTOL,
                          atol=tol.F64_ATOL_PER_SCALE * closed, scale=closed,
                          what=f"astropy-healpix nside_to_pixel_area(nside={nside})")
            worst_hp = max(worst_hp, abs(a_hp - closed) / closed)
            worst_ah = max(worst_ah, abs(a_ah - closed) / closed)
        ev.record("worst_rel_vs_healpy", worst_hp, tol.F64_RTOL)
        ev.record("worst_rel_vs_astropy_healpix", worst_ah, tol.F64_RTOL)
        harness.less_equal(worst_hp, tol.F64_RTOL, what="healpy 对拍相对残差")
        harness.less_equal(worst_ah, tol.F64_RTOL, what="astropy-healpix 对拍相对残差")


@harness.test(
    "healpix.s11.cell_area.missing_factor12_negative",
    intent="负例：把叶面积写成 `4π/nside²`（漏因子 12）必须被守恒式判红 —— "
           "证明守恒式不是恒真比较。",
    inputs="nside ∈ {4,16,64}；注入 `cell_area_factor12=False`（`hp.pixel_resolution_arcsec` "
           "的分母由 `12·nside²` 变成 `nside²`）。",
    expected=f"像元角尺度的相对偏差 = 12×，即 √12 ≈ 3.4641；断言相对偏差 > 2.0 判红成立。",
    source="docs/science/drizzle/DRIZZLE.md §4「A_cell = π/(3·nside²)」与 §3.5 "
           "`support = D_p / A_cell, A_cell = 4π/(12·nside²)`；"
           "docs/science/algorithms/HEALPIX_MAPPING.md 输入表 `ipix ∈ [0, 12·nside²)`"
           "（同一个因子 12：像元数 12·nside² 与面积 4π/(12·nside²) 必须同源）。",
    kind=harness.NEGATIVE,
    inject="healpix_core.cpp:329 的 `4.0·kPi / (12·nside·nside)` 写成 "
           "`4.0·kPi / (nside·nside)`（漏因子 12）",
    defect_id="DEF-S11-AREA-F12",
)
def _s11_cell_area_missing_factor12():
    with harness.evidence() as ev:
        ratios = []
        for nside in (4, 16, 64):
            closed = closed_hp_res_arcsec(nside)
            good = H.pixel_resolution_arcsec(nside)
            # 消融自证的前置条件：未注入时守恒式必须成立
            a_cell = CELL_AREA_SR.value(nside)
            harness.exact(H.npix(nside) * a_cell, 4.0 * math.pi,
                          what=f"nside={nside} 天球守恒前置条件")
            harness.exact(good, closed,
                          what=f"nside={nside} 角尺度前置条件")
            with H.inject(cell_area_factor12=False):
                bad = H.pixel_resolution_arcsec(nside)
            rel = abs(bad - closed) / closed
            ratios.append(rel)
            ev.record(f"nside={nside} 角尺度相对偏差", rel, note=f"good={good!r}")
        worst = max(ratios)
        ev.record("worst_rel", worst, 2.0,
                  note="门 = 2.0；正确实现给 0；漏因子 12 使角尺度放大 √12 倍，"
                       "即相对偏差 √12 − 1 = 2.4641")
        harness.is_true(worst > 2.0,
                        "守恒式/角尺度判据没抓住漏因子 12 的缺陷（该判据恒绿）")


# ===========================================================================
# S11 ② 父子关系  child = 4·parent + k
# ===========================================================================

@harness.test(
    "healpix.s11.child.quad_identity",
    intent="NESTED 父子关系 `child = 4·parent + k`（k ∈ {0,1,2,3}）跨 tile→leaf→子层"
           "逐级一致；与闭式 `4p+k` 逐位对拍。",
    inputs="固定种子随机 leaf_order ∈ {9..18} 的父像元，k 穷举 0..3；"
           "另含 tile 层拆解（tile_order=9）与逐层升阶 `child_nest(parent, shift)`。",
    expected="`child_nest(parent, 1) == 4·parent + k` 精确成立；"
             "`tile_to_leaf_nest(child) == tile_to_leaf_nest(parent) == parent`（tile_order=9）；"
             "连续 `child_nest(·,1)` 复合结果 == `4^n·p + k 的 NESTED 复合编码` 精确成立。",
    source="审核包-R2/T02 §2.1 S11 行逐字「`child = 4·parent + k`（tile_shift=9、"
           "mask=(1<<18)-1）」；实现正本 lib/algorithms/shared/healpix/healpix_core.h:53-57"
           "（`parent_nest`/`child_nest`）；NESTED 索引定义 Górski et al. 2005, ApJ 622, 759。",
    criteria=["S11"],
)
def _s11_child_quad_identity():
    rng = np.random.default_rng(H.SAMPLE_SEED + 7)
    with harness.evidence() as ev:
        n_checked = 0
        for leaf_order in (9, 10, 13, 16, 18):
            nside = 1 << leaf_order
            for _ in range(200):
                parent = int(rng.integers(0, H.npix(nside)))
                for k in range(4):
                    # 闭式 expected：直接写 4·parent + k，不经被测方
                    child_closed = 4 * parent + k
                    child = H.child_nest(parent, 1) + k
                    harness.exact(child, child_closed,
                                  what=f"child = 4·parent + k (parent={parent}, k={k})")
                    n_checked += 1
        # tile 层（tile_order = 9）：parent 与其 4 个子叶必须同属一个 tile
        for leaf_order in (14, 16, 18):
            nside = 1 << leaf_order
            for _ in range(300):
                parent = int(rng.integers(0, H.npix(nside)))
                t_parent = H.leaf_to_tile(parent)
                # upm.cpp:1979 的 tile 层是**固定位移** `tile_shift = 9`：tile 阶 =
                # leaf_order − 9（见 upm.cpp:417 注释「leaf order = target+9」），
                # 故 `tile_to_leaf_nest` 的 tile_order 取 `leaf_order - 9`。
                tile_order_eff = leaf_order - TOL_TILE_SHIFT
                first_leaf = H.tile_to_leaf_nest(t_parent, tile_order_eff,
                                                leaf_order)
                # tile 的首叶必须在 tile 的叶区间内，且 parent 落在 [first, first+2^18)
                harness.is_true(first_leaf <= parent,
                                f"tile 首叶 {first_leaf} > 父叶 {parent}")
                harness.exact(parent - first_leaf, H.leaf_local(parent),
                              what=f"parent − tile 首叶 != leaf_local (leaf_order={leaf_order})")
                harness.exact(H.leaf_to_tile(first_leaf), t_parent,
                              what=f"tile 首叶落回不同 tile (leaf_order={leaf_order})")
                # 子叶的 tile = `parent >> 16`（tile 层是 NESTED 索引，4·parent
                # 在 tile 阶上再细化一层）：闭式 expected，不经被测方
                expect_tile = parent >> 16
                for k in range(4):
                    child = 4 * parent + k
                    harness.exact(H.leaf_to_tile(child), expect_tile,
                                  what=f"子叶 {k} 的 tile 不符"
                                       f"（leaf_order={leaf_order}）")
                    # 局部索引必须落在 mask 内且 xy 反解还原
                    local = H.leaf_local(child)
                    harness.is_true(local <= TOL_TILE_MASK,
                                    f"leaf_local 越出 mask: {local}")
                    x, y = H.leaf_to_tile_xy(child)
                    harness.is_true(x < 512 and y < 512,
                                    f"tile xy 越界 ({x},{y})")
                    harness.exact(H.tile_xy_to_leaf(expect_tile, x, y), child,
                                  what="tile xy -> leaf 逆映射不闭合")
                    harness.exact(child - (expect_tile << (2 * TOL_TILE_SHIFT)),
                                  local,
                                  what="子叶在该 tile 内的偏移 != leaf_local")
                    n_checked += 1
        ev.record("child_identity_checks", float(n_checked), 0.0,
                  note="child = 4·parent + k 与 tile 拆解的逐位核对条数")


@harness.test(
    "healpix.s11.child.offset_one_negative",
    intent="负例：把 `child = 4·parent + k` 写成 `4·parent + k + 1` 必须判红。",
    inputs="固定种子 2000 个父像元；注入 `child_offset=1`（`child_nest` 结果整体 +1）。",
    expected="每一对 (parent, k) 的索引差恰为 1，判据 `mismatch == 0` 失效 ⇒ 判红成立。",
    source="审核包-R2/T02 §2.1 S11 行逐字「`child = 4·parent + k`」；"
           "lib/algorithms/shared/healpix/healpix_core.h:57 的 `ipix << (2·shift)`。",
    kind=harness.NEGATIVE,
    inject="`child_nest` 的返回值整体 +1（等价于把 NESTED 四叉树的 k 编号整体偏一）",
    defect_id="DEF-S11-CHILD-1",
)
def _s11_child_offset_one_negative():
    rng = np.random.default_rng(H.SAMPLE_SEED + 8)
    with harness.evidence() as ev:
        # 消融自证的前置条件：未注入时 child = 4·parent + k 逐位成立
        clean = 0
        for _ in range(500):
            p0 = int(rng.integers(0, H.npix(1 << 12)))
            for k in range(4):
                if H.child_nest(p0, 1) + k != 4 * p0 + k:
                    clean += 1
        harness.exact(clean, 0, what="未注入时 child = 4·parent + k 的前置条件")
        mismatch = 0
        n = 0
        with H.inject(child_offset=1):
            for _ in range(2000):
                parent = int(rng.integers(0, H.npix(1 << 12)))
                for k in range(4):
                    closed = 4 * parent + k
                    if H.child_nest(parent, 1) + k != closed:
                        mismatch += 1
                    n += 1
        ev.record("index_mismatch", float(mismatch), 0.0,
                  note=f"共 {n} 组 (parent,k)；正确实现给 0")
        harness.is_true(mismatch > 0,
                        "child = 4·parent + k 判据没抓住 k 编号偏一（该判据恒绿）")


# ===========================================================================
# S11 ③ tile 层不变量  tile_shift = 9、mask = (1<<18)-1、nested_local_to_xy 单调
# ===========================================================================

@harness.test(
    "healpix.s11.tile.invariants",
    intent="tile 层不变量：tile_shift=9、mask=(1<<18)-1；NESTED 局部索引 ↔ xy 的"
           "位解交错双射（x=偶数位、y=奇数位）、Morton 单调（沿固定 y 时 l→x 不减、"
           "沿固定 x 时 l→y 不减）与 HiPS FITS 行主序映射往返精确。",
    inputs="tile_shift 取自参考实现；局部索引域 0..2^18-1 的固定种子抽样 20000 个 + "
           "4 个角点（0, 2^18-1, 2^17, 2^18-1-2^17）；单调性在 shift=6（4096 全域穷举）、"
           "shift=8（65536 全域穷举）、shift=9（每个 y 取全部 512 个 x，抽样 32 个 y）。",
    expected="`tile_shift == 9`、`mask == (1<<18)-1` 精确；"
             "`xy_to_nested_local(nested_local_to_xy(l,9),9) == l` 双射；"
             "反解出的 x,y 与**独立闭式位解交错**（x = Σ((l>>2i)&1)<<i）逐位一致；"
             "`x`、`y` 落在 [0,512)；沿固定 y 的 l 序列上 x 不减、沿固定 x 上 y 不减；"
             "FITS 索引往返精确且行 = 511−x、列 = y。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「Invariants」逐字"
           "「tile_shift=9、mask=(1<<18)-1（`lib/infrastructure/aio/src/aio_upm.cpp`、"
           "`lib/algorithms/coverage/src/upm.cpp`）；nested_local_to_xy 单调（core）」；"
           "eng/tests/unit/tolerances.py#healpix.tile_shift / #healpix.tile_mask（已冻结）；"
           "HiPS FITS 排列锚 IVOA HiPS 1.0 Image tile packaging（healpix_core.cpp:284-286）；"
           "位解交错定义 Górski et al. 2005, ApJ 622, 759（nested array indexing）。",
    criteria=["S11"],
)
def _s11_tile_invariants():
    shift = H.tile_shift()
    with harness.evidence() as ev:
        harness.exact(shift, TOL_TILE_SHIFT, what="tile_shift")
        harness.exact(H.tile_mask(), TOL_TILE_MASK, what="tile mask")
        ev.record("tile_shift", float(shift), float(TOL_TILE_SHIFT))
        ev.record("tile_mask", float(H.tile_mask()), float(TOL_TILE_MASK))

        rng = np.random.default_rng(H.SAMPLE_SEED + 9)
        ids = [0, TOL_TILE_MASK, 1 << 17, TOL_TILE_MASK - (1 << 17)]
        ids += [int(v) for v in rng.integers(0, 1 << 18, 20000)]
        bad_closed = 0
        for local in sorted(set(ids)):
            x, y = H.nested_local_to_xy(local, shift)
            # 独立闭式位解交错（不是调用被测方）
            x_closed = 0
            y_closed = 0
            for i in range(shift):
                x_closed |= ((local >> (2 * i)) & 1) << i
                y_closed |= ((local >> (2 * i + 1)) & 1) << i
            if (x, y) != (x_closed, y_closed):
                bad_closed += 1
            harness.exact(H.xy_to_nested_local(x, y, shift), local,
                          what=f"nested_local_to_xy 非双射 (local={local})")
            harness.is_true(x < 512 and y < 512,
                            f"tile xy 越界 (local={local}, x={x}, y={y})")
            # HiPS FITS 行主序：行 = tile_width-1-x、列 = y
            fi = H.nested_local_to_fits_index(local, shift, 512)
            harness.exact(H.fits_index_to_nested_local(fi, shift, 512), local,
                          what=f"FITS 索引往返不闭合 (local={local})")
            harness.exact(fi // 512, 511 - x,
                          what=f"FITS 行号 != tile_width-1-x (local={local})")
            harness.exact(fi % 512, y,
                          what=f"FITS 列号 != y (local={local})")
        ev.record("closed_form_mismatch", float(bad_closed), 0.0,
                  note="与独立闭式位解交错逐位不一致数")
        harness.exact(bad_closed, 0, what="位解交错闭式对拍")

        # Morton 单调（`HEALPIX_MAPPING.md`「Invariants」逐字「nested_local_to_xy 单调」
        # 的**精确形式**：沿固定 y 的 l 序列 x 不减、沿固定 x 的 l 序列 y 不减）
        nonmono = 0
        checked = 0
        for sh in (6, 8):                      # 全域穷举
            for l in range(1 << (2 * sh)):
                x, y = H.nested_local_to_xy(l, sh)
                harness.exact(H.xy_to_nested_local(x, y, sh), l,
                              what=f"shift={sh} 非双射 (local={l})")
            by_y: dict = {}
            by_x: dict = {}
            for l in range(1 << (2 * sh)):
                x, y = H.nested_local_to_xy(l, sh)
                by_y.setdefault(y, []).append(x)
                by_x.setdefault(x, []).append(y)
            for _y, xs in by_y.items():
                nonmono += sum(1 for a, b in zip(xs, xs[1:]) if b < a)
            for _x, ys in by_x.items():
                nonmono += sum(1 for a, b in zip(ys, ys[1:]) if b < a)
            checked += 2 * (1 << sh)
        for yv in range(0, 512, 16):            # shift=9 抽样
            ls = sorted(H.xy_to_nested_local(x, yv, 9) for x in range(512))
            xs = [H.nested_local_to_xy(l, 9)[0] for l in ls]
            nonmono += sum(1 for a, b in zip(xs, xs[1:]) if b < a)
            checked += 1
        for xv in range(0, 512, 16):
            ls = sorted(H.xy_to_nested_local(xv, y, 9) for y in range(512))
            ys = [H.nested_local_to_xy(l, 9)[1] for l in ls]
            nonmono += sum(1 for a, b in zip(ys, ys[1:]) if b < a)
            checked += 1
        ev.record("morton_non_monotonic_steps", float(nonmono), 0.0,
                  note=f"沿固定 y/x 的 l 序列共核对 {checked} 条单调链")
        harness.exact(nonmono, 0, what="Morton 单调性（沿固定 y 的 x、沿固定 x 的 y）")

        # 正本冲突登记：「单调」按字面读作「l→x 单调」在参考实现上**不成立**
        probe = sorted(set(ids))
        xs_l = [H.nested_local_to_xy(l, shift)[0] for l in probe]
        ys_l = [H.nested_local_to_xy(l, shift)[1] for l in probe]
        nonmono_x = sum(1 for a, b in zip(xs_l, xs_l[1:]) if b < a)
        nonmono_y = sum(1 for a, b in zip(ys_l, ys_l[1:]) if b < a)
        ev.record("literal_reading_non_monotonic_x", float(nonmono_x), note=
                  f"按字面「x(l) 单调」在 {len(probe)} 个递增 l 上的递减步数")
        ev.record("literal_reading_non_monotonic_y", float(nonmono_y), note=
                  f"按字面「y(l) 单调」在 {len(probe)} 个递增 l 上的递减步数")
        ev.record("invariant_wording_conflict",
                  "「nested_local_to_xy 单调」按字面读（对 local 单调）不成立",
                  note="UNRESOLVED：Morton 交错对 local 不是单调的；本层按精确形式"
                       "（沿固定 y 的 x、沿固定 x 的 y 不减）判门，并把字面读法的实测"
                       "非单调步数登记在此")


@harness.test(
    "healpix.s11.tile.shift_wrong_negative",
    intent="负例：tile_shift 从 9 改成 8（或 10）必须判红 —— 证明 mask/位移不变量有牙齿。",
    inputs="tile_shift=9 的同一批局部索引；注入 `tile_shift_override=8` 与 `=10`。",
    expected="mask 变成 (1<<16)-1 = 65535 或 (1<<20)-1 = 1048575，与冻结值 262143 精确不符；"
             "且 `leaf_local` 对随机 leaf ipix 越出真 mask。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「Invariants」逐字"
           "「tile_shift=9、mask=(1<<18)-1」；eng/tests/unit/tolerances.py#healpix.tile_shift。",
    kind=harness.NEGATIVE,
    inject="`upm.cpp:1979` / `aio_upm.cpp:524` 的 `const int tile_shift = 9` 改成 8 / 10",
    defect_id="DEF-S11-TILE-SHIFT",
)
def _s11_tile_shift_wrong_negative():
    with harness.evidence() as ev:
        for wrong in (8, 10):
            with H.inject(tile_shift_override=wrong):
                got_shift = H.tile_shift()
                got_mask = H.tile_mask()
            ev.record(f"tile_shift={wrong} → mask", float(got_mask),
                      float(TOL_TILE_MASK), note=f"实测 shift={got_shift}")
            harness.is_true(got_shift != TOL_TILE_SHIFT,
                            f"tile_shift={wrong} 未被 tile_shift 判据抓住（恒绿）")
            harness.is_true(got_mask != TOL_TILE_MASK,
                            f"tile_shift={wrong} 未被 mask 判据抓住（恒绿）")
        # 可观测量：HiPS tile 宽必须 = 2^tile_shift = 512；shift>9 时
        # `nested_local_to_fits_index` 对 local 不再单射（x 被夹到 511 ⇒ 撞列）
        for sh in (TOL_TILE_SHIFT, 8, 10):
            with H.inject(tile_shift_override=(sh if sh != TOL_TILE_SHIFT else -1)):
                width = 1 << H.tile_shift()
                ev.record(f"tile_shift={sh} ⇒ HiPS tile 宽", float(width), 512.0,
                          note="IVOA HiPS 1.0 冻结 tile_width = 512")
                over = sum(1 for local in (0, (1 << (2 * sh)) - 1,
                                           1 << (2 * sh - 1))
                           if H.nested_local_to_xy(local, H.tile_shift())[0] > 511)
                ev.record(f"tile_shift={sh} 下 x>511 的 local 数", float(over), 0.0,
                          note=">0 ⇒ FITS 行主序映射在该 local 上撞列（非单射）")
                if sh == TOL_TILE_SHIFT:
                    harness.exact(width, 512, what="正确 tile_shift=9 ⇒ tile 宽 512")
                    harness.exact(over, 0, what="正确 tile_shift=9 ⇒ FITS 映射单射")
                else:
                    harness.is_true(width != 512,
                                    f"tile_shift={sh} 的 HiPS tile 宽异常没被抓住"
                                    f"（恒绿）")
                    if sh > 9:
                        # shift > 9 ⇒ 局部 x 可越出 511，FITS 行主序映射在该
                        # local 上撞列（非单射）⇒ HiPS tile 排列合同被破坏
                        harness.is_true(over > 0,
                                        f"tile_shift={sh} 下 FITS 映射仍单射（恒绿）")


# ===========================================================================
# S11 ④ 球面角 ↔ NESTED 往返（本单核心交付）
# ===========================================================================

@harness.test(
    "healpix.s11.roundtrip.dense_oracle",
    intent="S11 ③ 主门：球面角 ↔ NESTED 往返。在**独立密集域**上跑 "
           "`ang2pix → pix2ang → ang2pix → pix2ang` 与反向 `pix2ang → ang2pix → pix2ang`，"
           "索引腿精确一致、角腿 ≤ 1e-12 度，并与 astropy-healpix / healpy 逐点对拍 "
           "mismatch == 0。",
    inputs="独立密集域 7744 点（全域随机 4200 + 赤道带 1400 + 极区 |z|→1 260 + "
           "基面边界 |z|=2/3 与 φ=kπ/4 两侧 1e-13 1344 + RA 跨界锚点 340），固定种子 "
           f"{H.SAMPLE_SEED}；另有 128 个精确并列点单列；"
           f"nside ∈ {ROUNDTRIP_NSIDES}（order 0,1,2,4,6）。"
           "反向腿起手点是索引域上 2000 个固定种子抽样。",
    expected="两个索引腿的 mismatch 逐 nside 精确为 0（`TEST.md` §4「索引」= 精确档）；"
             "两个角腿的最大球面角距 ≤ 1e-12 度（`tolerances.HEALPIX_ROUNDTRIP_DEG`）；"
             "与 astropy-healpix（`lonlat_to_healpix`）和 healpy（`ang2pix`）的 "
             "`ang2pix` mismatch 逐点为 0；`pix2ang` 与 astropy-healpix "
             "`healpix_to_lonlat` 的 mismatch 逐点为 0。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「Postconditions」逐字"
           "「round-trip 误差 ≤ 1e-12 度（FP64）；NESTED 父子一致性」；"
           "同文件「Postconditions · 外部 Oracle 承接」表「**硬门 mismatch == 0**」；"
           "eng/tests/unit/tolerances.py#healpix.roundtrip_deg（已冻结）；"
           "docs/engineering/testing/TEST.md §13（astropy-healpix = HEALPix 几何的独立 Oracle）。",
    criteria=["S11"],
)
def _s11_roundtrip_dense_oracle():
    pts = all_points()
    interior = dense_points()
    ties = tie_points()
    with harness.evidence() as ev:
        ev.record("dense_domain_points", float(len(interior)), note="固定种子 "
                  f"{H.SAMPLE_SEED}；非被测方采样网格；「硬门 mismatch==0」的量测域")
        ev.record("face_boundary_tie_points", float(len(ties)),
                  note="精确并列点：只压被测口径自洽往返，第三方 tie-break 分歧单列取证")
        worst_ang = 0.0
        worst_rev = 0.0
        worst_mismatch_ah = 0
        worst_mismatch_hp = 0
        worst_center_mm_ah = 0
        for nside in ROUNDTRIP_NSIDES:
            st = roundtrip_stats(nside, pts)
            harness.exact(int(st["idx_mismatch"]), 0,
                          what=f"nside={nside} 正向 ang2pix 往返索引不一致")
            harness.exact(int(st["rev_idx_mismatch"]), 0,
                          what=f"nside={nside} 反向 ang2pix 往返索引不一致")
            harness.less_equal(st["ang_residual_deg"], TOL_ROUNDTRIP_DEG,
                               what=f"nside={nside} 正向 pix2ang 角残差")
            harness.less_equal(st["rev_ang_residual_deg"], TOL_ROUNDTRIP_DEG,
                               what=f"nside={nside} 反向 pix2ang 角残差")
            worst_ang = max(worst_ang, st["ang_residual_deg"])
            worst_rev = max(worst_rev, st["rev_ang_residual_deg"])
            ev.record(f"nside={nside} 正向角残差", st["ang_residual_deg"],
                      TOL_ROUNDTRIP_DEG, unit="deg")
            ev.record(f"nside={nside} 反向角残差", st["rev_ang_residual_deg"],
                      TOL_ROUNDTRIP_DEG, unit="deg")

            ra = np.fromiter((p[0] for p in interior), float, len(interior))
            dec = np.fromiter((p[1] for p in interior), float, len(interior))
            mine = np.fromiter((H.ang2pix_nest(nside, p[0], p[1]) for p in interior),
                               np.int64, len(interior))
            o_ah = oracle_ang2pix_astropy(nside, ra, dec)
            o_hp = oracle_ang2pix_healpy(nside, ra, dec)
            stable = stability_mask(nside, interior)
            mm_ah = int(np.count_nonzero(mine[stable] != o_ah[stable]))
            mm_hp = int(np.count_nonzero(mine[stable] != o_hp[stable]))
            knife_ah = int(np.count_nonzero(mine[~stable] != o_ah[~stable]))
            knife_hp = int(np.count_nonzero(mine[~stable] != o_hp[~stable]))
            harness.exact(mm_ah, 0,
                          what=f"nside={nside} 与 astropy-healpix 的 ang2pix mismatch"
                               f"（稳定域 {int(stable.sum())}/{len(interior)} 点）")
            harness.exact(mm_hp, 0,
                          what=f"nside={nside} 与 healpy 的 ang2pix mismatch")
            worst_mismatch_ah = max(worst_mismatch_ah, mm_ah)
            worst_mismatch_hp = max(worst_mismatch_hp, mm_hp)

            c_ra, c_dec = oracle_pix2ang_astropy(nside, mine)
            mm_c = int(sum(1 for i in range(len(interior))
                           if stable[i] and H.ang2pix_nest(nside, float(c_ra[i]),
                                                           float(c_dec[i]))
                           != int(mine[i])))
            harness.exact(mm_c, 0,
                          what=f"nside={nside} 与 astropy-healpix 的 pix2ang mismatch")
            worst_center_mm_ah = max(worst_center_mm_ah, mm_c)
            ev.record(f"nside={nside} oracle_mismatch(稳定域 ah/hp/center)",
                      float(max(mm_ah, mm_hp, mm_c)), 0.0,
                      note=f"稳定域 {int(stable.sum())}/{len(interior)}；"
                           f"不稳定域（并列）分歧取证 ah={knife_ah} hp={knife_hp}")
            ev.record(f"nside={nside} 不稳定域点数", float((~stable).sum()), 0.0,
                      note=f"扰动 δ={STABILITY_DELTA_DEG} 度下归属翻转；"
                           f"第三方分歧 {knife_ah + knife_hp} 点全部落在该域")

            # 并列点：被测口径自洽往返仍必须精确；第三方 tie-break 分歧只取证
            tie_st = roundtrip_stats(nside, ties)
            harness.exact(int(tie_st["idx_mismatch"]), 0,
                          what=f"nside={nside} 并列点上的自洽往返索引不一致")
            harness.less_equal(tie_st["ang_residual_deg"], TOL_ROUNDTRIP_DEG,
                               what=f"nside={nside} 并列点上的自洽往返角残差")
            tie_mine = np.fromiter((H.ang2pix_nest(nside, p[0], p[1]) for p in ties),
                                   np.int64, len(ties))
            tie_ra = np.fromiter((p[0] for p in ties), float, len(ties))
            tie_dec = np.fromiter((p[1] for p in ties), float, len(ties))
            tie_ah = int(np.count_nonzero(
                tie_mine != oracle_ang2pix_astropy(nside, tie_ra, tie_dec)))
            tie_hp = int(np.count_nonzero(
                tie_mine != oracle_ang2pix_healpy(nside, tie_ra, tie_dec)))
            tie_ah_hp = int(np.count_nonzero(
                oracle_ang2pix_astropy(nside, tie_ra, tie_dec)
                != oracle_ang2pix_healpy(nside, tie_ra, tie_dec)))
            ev.record(f"nside={nside} 并列点第三方分歧", float(tie_ah + tie_hp),
                      note=f"ah-vs-mine={tie_ah} hp-vs-mine={tie_hp} "
                           f"ah-vs-hp={tie_ah_hp}；并列是三个独立实现 tie-break 的固有面")
        ev.record("worst_roundtrip_ang_deg", worst_ang, TOL_ROUNDTRIP_DEG,
                  unit="deg", note="正反两腿合并最坏读数（含并列点）")
        ev.record("worst_reverse_ang_deg", worst_rev, TOL_ROUNDTRIP_DEG, unit="deg")
        ev.record("worst_oracle_mismatch", float(max(worst_mismatch_ah,
                                                      worst_mismatch_hp,
                                                      worst_center_mm_ah)), 0.0)


@harness.test(
    "healpix.s11.roundtrip.floor_is_structural",
    intent="登记「1e-12 度门是**地板触发门**而非量级门」：正确实现下复合往返的角残差"
           "**逐位为 0**（索引腿精确 ⇒ 两次 pix2ang 同参同值），故该门靠索引腿提供牙齿；"
           "同时说明为什么「把门限缩到实测值的 1e-3」对这条门不适用。",
    inputs="独立密集域 7744 点 + 128 并列点；nside ∈ {4,16}。",
    expected="复合往返角残差精确为 0.0 度 ⇒ 余量 ∞（门不可再收缩）；"
             "因此非恒真的证明由 `healpix.s11.roundtrip.face_offset_negative` / "
             "`polar_phi_offset_negative` / `deg_normalize_negative` 三条负例承担，"
             "门限收缩对照另由 `healpix.s11.cross_oracle.gate_shrunk_negative` 承担。",
    source="docs/engineering/testing/TEST.md §4.3「绝对容差只有在被比较量的量级 scale "
           "满足 atol ≥ 1 ulp(scale) 时才可判」+ §2「恒真的比较没有证据资格」；"
           "eng/tests/unit/tolerances.py#healpix.roundtrip_deg 的 note 段"
           "（逐字要求在独立密集域上取样）。",
    criteria=["S11"],
)
def _s11_roundtrip_floor_is_structural():
    pts = all_points()
    with harness.evidence() as ev:
        for nside in (4, 16):
            st = roundtrip_stats(nside, pts)
            harness.exact(int(st["idx_mismatch"]), 0, what="正向索引腿")
            harness.exact(int(st["rev_idx_mismatch"]), 0, what="反向索引腿")
            harness.exact(st["ang_residual_deg"], 0.0,
                          what=f"nside={nside} 复合往返角残差应当逐位为 0")
            harness.exact(st["rev_ang_residual_deg"], 0.0,
                          what=f"nside={nside} 反向复合往返角残差应当逐位为 0")
            ev.record(f"nside={nside} 复合往返角残差", st["ang_residual_deg"],
                      TOL_ROUNDTRIP_DEG, unit="deg",
                      note="实测 0 ⇒ 余量 ∞；该门是地板触发门")
        # 不可满足性演示（TEST.md §4.3）：产品自身的 acos 尺子量不出 1e-12 度的门。
        # 用**同一条复合往返**分别以两把尺子测量：产品函数 `angular_distance_deg`
        # （acos 形式）的读数被它自己的 O(√u) 条件数支配，稳定尺子读到逐位 0。
        for nside in (4, 16, 64):
            q1 = [H.ang2pix_nest(nside, ra, dec) for ra, dec in pts]
            c1 = [H.pix2ang_nest(nside, q) for q in q1]
            c2 = [H.pix2ang_nest(nside,
                                H.ang2pix_nest(nside, c[0], c[1])) for c in c1]
            acos_worst = max(H.angular_distance_deg(a[0], a[1], b[0], b[1])
                             for a, b in zip(c1, c2))
            stable_worst = max(H.angular_distance_deg_stable(a[0], a[1], b[0], b[1])
                               for a, b in zip(c1, c2))
            harness.exact(stable_worst, 0.0,
                          what=f"nside={nside} 稳定尺子的复合往返残差应当逐位为 0")
            harness.is_true(acos_worst > TOL_ROUNDTRIP_DEG,
                            f"nside={nside} 的 acos 尺子未能展示其分辨率下限"
                            f"（本演示前提不成立）")
            ev.record(f"nside={nside} acos 尺子的复合往返读数", acos_worst,
                      TOL_ROUNDTRIP_DEG, unit="deg",
                      note=f"同一次往返，稳定尺子读 {stable_worst!r}；"
                           f"acos 形式的 O(√u) 条件数使读数不可用于 1e-12 度门")


@harness.test(
    "healpix.s11.roundtrip.nest_parent_child",
    intent="NESTED 父子一致性：`leaf_to_tile_nest(ang2pix(2^L, p), L, T)` 必须"
           "**精确等于** `ang2pix(2^T, p)`（同一角度在叶阶与 tile 阶落进同一条父子链）。",
    inputs="独立密集域 7744 点 + 128 并列点；(L,T) ∈ {(6,5),(10,9),(13,9),(16,9),(16,14)}，含 "
           "tile_order = 9 的生产档与两级 tile 档。",
    expected="逐点逐位相等，mismatch 精确为 0（`TEST.md` §4「索引」= 精确档）。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「Postconditions」逐字"
           "「round-trip 误差 ≤ 1e-12 度（FP64）；**NESTED 父子一致性**」；"
           "实现正本 lib/algorithms/shared/healpix/healpix_core.h:78-101。",
    criteria=["S11"],
)
def _s11_roundtrip_nest_parent_child():
    pts = all_points()
    with harness.evidence() as ev:
        for leaf_order, tile_order in ((6, 5), (10, 9), (13, 9), (16, 9), (16, 14)):
            q_leaf = [H.ang2pix_nest(1 << leaf_order, ra, dec) for ra, dec in pts]
            q_tile = [H.ang2pix_nest(1 << tile_order, ra, dec) for ra, dec in pts]
            mm = sum(1 for a, b in zip(q_leaf, q_tile)
                     if H.leaf_to_tile_nest(a, leaf_order, tile_order) != b)
            harness.exact(mm, 0,
                          what=f"L={leaf_order} T={tile_order} NESTED 父子不一致")
            ev.record(f"L={leaf_order},T={tile_order} 父子不一致数", float(mm), 0.0,
                      note=f"点数 {len(pts)}")


@harness.test(
    "healpix.s11.roundtrip.quantization_bound",
    intent="正本的往返角距包络：`max angdist(p, pix2ang(ang2pix(p))) ≤ 1.2·hp_res + 1e-9`"
           "（角秒），在独立密集域上成立。这是本组里唯一**有非零读数**的角度量级门。",
    inputs="独立密集域 7744 点 + 128 并列点；nside ∈ (1,2,4,8,16,32,64,128,256)。",
    expected=f"逐 nside 的最坏角距 ≤ `1.2·hp_res + 1e-9` 角秒（hp_res 取闭式 "
             "`√(π/3)/nside`）；实测比值上界 ≈ 1.02，余量 ≥ 1.17×。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「Postconditions · 外部 Oracle 承接」"
           "表第 1 行逐字：「**硬门 mismatch == 0** 且往返角距 ≤ `1.2 × hp_res + 1e-9`"
           "（判据写在该测试源内）」；hp_res 闭式见 docs/science/drizzle/DRIZZLE.md §4。",
    criteria=["S11"],
)
def _s11_roundtrip_quantization_bound():
    pts = all_points()
    with harness.evidence() as ev:
        worst_ratio = 0.0
        for nside in (1, 2, 4, 8, 16, 32, 64, 128, 256):
            hp_res = closed_hp_res_arcsec(nside)
            limit = QUANT_BOUND_HP_RES * hp_res + QUANT_BOUND_FLOOR_ARCSEC
            worst = 0.0
            for ra, dec in pts:
                q = H.ang2pix_nest(nside, ra, dec)
                c_ra, c_dec = H.pix2ang_nest(nside, q)
                d = H.angular_distance_deg_stable(ra, dec, c_ra, c_dec) * 3600.0
                if d > worst:
                    worst = d
            ratio = worst / hp_res
            worst_ratio = max(worst_ratio, ratio)
            harness.less_equal(worst, limit,
                               what=f"nside={nside} 往返角距包络")
            ev.record(f"nside={nside} 往返角距", worst, limit, unit="arcsec",
                      note=f"= {ratio:.4f}·hp_res")
        ev.record("worst_ratio_hp_res", worst_ratio, QUANT_BOUND_HP_RES,
                  note="实测最坏 / hp_res（余量倍数 = 门限/该值 = "
                       f"{QUANT_BOUND_HP_RES / worst_ratio:.4f}×）")


@harness.test(
    "healpix.s11.cross_oracle.angle_agreement",
    intent="两条**独立实现**的像元中心角差（参考实现 vs astropy-healpix / healpy）"
           "在 1e-12 度内 —— 这是同一物理量在独立算法路径上的 FP64 一致性门。",
    inputs=f"独立密集域 7744 点（非并列域）；nside ∈ {CROSS_ORACLE_NSIDES}。"
           "更宽的 nside 域只作读数登记（见文件头「正本 1e-12 度门的可测性边界」）。",
    expected="最坏角差 ≤ 1e-12 度；实测 ≤ 1.09e-13 度（nside ≤ 16），余量 ≥ 9.1×。",
    source="docs/engineering/testing/TEST.md §13 表逐字：「astropy-healpix = HEALPix 几何的"
           "独立 Oracle」；docs/science/algorithms/HEALPIX_MAPPING.md「Postconditions」"
           "1e-12 度（FP64）为该量级的正本阈值；eng/tests/unit/tolerances.py#healpix.roundtrip_deg。",
    criteria=["S11"],
)
def _s11_cross_oracle_angle_agreement():
    pts = dense_points()  # 非并列域（第三方 tie-break 在并列点固有分歧）
    with harness.evidence() as ev:
        worst = 0.0
        for nside in CROSS_ORACLE_NSIDES:
            q = [H.ang2pix_nest(nside, ra, dec) for ra, dec in pts]
            c = [H.pix2ang_nest(nside, i) for i in q]
            idx = np.fromiter(q, np.int64, len(q))
            ra_ah, dec_ah = oracle_pix2ang_astropy(nside, idx)
            ra_hp, dec_hp = oracle_pix2ang_healpy(nside, idx)
            w_ah = max(H.angular_distance_deg_stable(c[i][0], c[i][1],
                                                     float(ra_ah[i]), float(dec_ah[i]))
                       for i in range(len(q)))
            w_hp = max(H.angular_distance_deg_stable(c[i][0], c[i][1],
                                                     float(ra_hp[i]), float(dec_hp[i]))
                       for i in range(len(q)))
            worst = max(worst, w_ah, w_hp)
            harness.less_equal(w_ah, TOL_ROUNDTRIP_DEG,
                               what=f"nside={nside} vs astropy-healpix 中心角差")
            harness.less_equal(w_hp, TOL_ROUNDTRIP_DEG,
                               what=f"nside={nside} vs healpy 中心角差")
            ev.record(f"nside={nside} vs astropy-healpix", w_ah, TOL_ROUNDTRIP_DEG,
                      unit="deg")
            ev.record(f"nside={nside} vs healpy", w_hp, TOL_ROUNDTRIP_DEG, unit="deg")
        ev.record("worst_cross_oracle_deg", worst, TOL_ROUNDTRIP_DEG, unit="deg",
                  note=f"nside ≤ {CROSS_ORACLE_MAX_NSIDE} 的注册域")
        # 域外读数登记（只记录，不判门）；跑在**全域**上（含基面边界锚点）
        for nside in EVIDENCE_NSIDES:
            q = [H.ang2pix_nest(nside, ra, dec) for ra, dec in pts]
            c = [H.pix2ang_nest(nside, i) for i in q]
            idx = np.fromiter(q, np.int64, len(q))
            ra_ah, dec_ah = oracle_pix2ang_astropy(nside, idx)
            w = max(H.angular_distance_deg_stable(c[i][0], c[i][1],
                                                   float(ra_ah[i]), float(dec_ah[i]))
                    for i in range(len(q)))
            ev.record(f"nside={nside} 跨实现角差（域外证据）", w, TOL_ROUNDTRIP_DEG,
                      unit="deg", note="atan2/asin 条件数随 nside 恶化；本层不判门")


@harness.test(
    "healpix.s11.cross_oracle.gate_shrunk_negative",
    intent="非恒真自证：把跨实现角差门的门限临时改成**实测值的 1e-3**，它必须判红。"
           "这是「1e-12 度门有牙齿」的直接对照实验（正确门限下实测 1.03e-13 度）。",
    inputs="独立密集域 7744 点（非并列域），nside ∈ (1,2,4,8,16)；先测实测值，再把门限设成实测值的 1e-3。",
    expected="实测值 / 收缩门限 = 1e3 倍超界 ⇒ 判红成立；"
             "收缩门限 = measured/1e3，**不写进任何文件**，只在本用例内存里使用。",
    source="docs/engineering/testing/TEST.md §5 逐字「负例必须可红：正确实现给绿，"
           "注入缺陷给红」；§2「恒真的比较没有证据资格」。",
    kind=harness.NEGATIVE,
    inject="把 1e-12 度门限收缩到实测值的 1e-3（对照实验，不是代码缺陷）",
    defect_id="DEF-S11-GATE-SHRINK-1E3",
)
def _s11_cross_oracle_gate_shrunk_negative():
    pts = dense_points()
    with harness.evidence() as ev:
        measured = 0.0
        for nside in CROSS_ORACLE_NSIDES:
            q = [H.ang2pix_nest(nside, ra, dec) for ra, dec in pts]
            c = [H.pix2ang_nest(nside, i) for i in q]
            idx = np.fromiter(q, np.int64, len(q))
            ra_ah, dec_ah = oracle_pix2ang_astropy(nside, idx)
            measured = max(measured,
                           max(H.angular_distance_deg_stable(c[i][0], c[i][1],
                                                             float(ra_ah[i]),
                                                             float(dec_ah[i]))
                               for i in range(len(q))))
        harness.less_equal(measured, TOL_ROUNDTRIP_DEG,
                           what="正确门限下必须通过（本负例的前置条件）")
        shrunk = measured * 1e-3
        ev.record("measured_deg", measured, TOL_ROUNDTRIP_DEG, unit="deg",
                  note="正确门限下的实测值")
        ev.record("shrunk_gate_deg", shrunk, unit="deg",
                  note=f"收缩门限 = measured×1e-3（仅内存，不落盘）")
        harness.is_true(measured > shrunk,
                        "收缩门限后判据未变红（这条门对 FP 级差异恒绿，无牙齿）")


# ---------------------------------------------------------------------------
# S11 负例：真实破坏往返的缺陷
# ---------------------------------------------------------------------------

def _roundtrip_probe(nside: int = 8) -> Tuple[int, float]:
    """往返探针（负例专用，单 nside）。"""
    pts = all_points()
    q1 = [H.ang2pix_nest(nside, ra, dec) for ra, dec in pts]
    c1 = [H.pix2ang_nest(nside, q) for q in q1]
    q2 = [H.ang2pix_nest(nside, c[0], c[1]) for c in c1]
    c2 = [H.pix2ang_nest(nside, q) for q in q2]
    return (sum(1 for a, b in zip(q1, q2) if a != b),
            _max_stable_ang(c1, c2))


@harness.test(
    "healpix.s11.roundtrip.face_offset_negative",
    intent="负例：`nest2ring` 的 face 判定偏一（基面索引 +1 后模 12 回绕）必须破坏往返。",
    inputs="独立密集域 7744 点 + 128 并列点，nside=8；注入 `nest2ring_face_offset=1`。",
    expected="索引往返 mismatch 显著非 0；复合往返角残差远超 1e-12 度。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「Postconditions」1e-12 度；"
           "「负例面（可判红、非恒真）」逐字「oracle 面 `mismatch != 0` 或往返角距超 "
           "`1.2 × hp_res + 1e-9` 必须判红」；实现正本 lib/algorithms/shared/healpix/"
           "healpix_core.cpp:257 `const uint32_t basehp = static_cast<uint32_t>(ipix / npface);`。",
    kind=harness.NEGATIVE,
    inject="`healpix_core.cpp:257` 的 `basehp = ipix / npface` 改成 "
           "`(ipix / npface + 1) % 12`（基面判定偏一）",
    defect_id="DEF-S11-FACE-OFF1",
)
def _s11_roundtrip_face_offset_negative():
    with harness.evidence() as ev:
        good_mm, good_ang = _roundtrip_probe()
        harness.exact(good_mm, 0, what="未注入时的前置条件")
        harness.exact(good_ang, 0.0, what="未注入时的前置条件")
        with H.inject(nest2ring_face_offset=1):
            mm, ang = _roundtrip_probe()
        ev.record("index_mismatch", float(mm), 0.0,
                  note=f"共 {len(all_points())} 点；未注入时 {good_mm}")
        ev.record("roundtrip_ang_deg", ang, TOL_ROUNDTRIP_DEG, unit="deg",
                  note=f"未注入时 {good_ang!r}")
        harness.is_true(mm > 0, "基面偏一没被索引往返腿抓住（恒绿）")
        harness.is_true(ang > TOL_ROUNDTRIP_DEG,
                        "基面偏一没被 1e-12 度角腿抓住（恒绿）")


@harness.test(
    "healpix.s11.roundtrip.polar_phi_offset_negative",
    intent="负例：极冠内 phi 索引偏一（分子少 1）必须破坏往返 —— 覆盖 HEALPIX_MAPPING"
           "「数值风险：极区」这一条。",
    inputs="独立密集域 7744 点 + 128 并列点，nside=8；注入 `polar_phi_index_offset=1`。",
    expected="索引往返 mismatch 非 0（只影响极冠叶）；角残差远超 1e-12 度。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「数值风险」逐字「极区」；"
           "「Postconditions」1e-12 度；实现正本 lib/algorithms/shared/healpix/"
           "healpix_core.cpp:205 `phi_t = kPi * (ns - y) / (2.0 * ((ns - x) + (ns - y)));`。",
    kind=harness.NEGATIVE,
    inject="`healpix_core.cpp:205` 的极冠 phi_t 分子写成 `ns - y + 1`（phi 索引偏一）",
    defect_id="DEF-S11-PHI-OFF1",
)
def _s11_roundtrip_polar_phi_offset_negative():
    with harness.evidence() as ev:
        good_mm, good_ang = _roundtrip_probe()
        harness.exact(good_mm, 0, what="未注入时的前置条件")
        with H.inject(polar_phi_index_offset=1):
            mm, ang = _roundtrip_probe()
        ev.record("index_mismatch", float(mm), 0.0,
                  note=f"共 {len(all_points())} 点；未注入时 {good_mm}")
        ev.record("roundtrip_ang_deg", ang, TOL_ROUNDTRIP_DEG, unit="deg",
                  note=f"未注入时 {good_ang!r}")
        harness.is_true(mm > 0, "极冠 phi 偏一没被索引往返腿抓住（恒绿）")
        harness.is_true(ang > TOL_ROUNDTRIP_DEG,
                        "极冠 phi 偏一没被 1e-12 度角腿抓住（恒绿）")


@harness.test(
    "healpix.s11.roundtrip.deg_normalize_negative",
    intent="负例：把角度归一化写成简单除法（`phi_t = phi/(π/2)` 而不是 `fmod(phi, π/2)`）"
           "必须破坏 RA 跨界锚点上的往返。",
    inputs="独立密集域 7744 点（含 RA ∈ {0, 360, ±1e-9, 540, −0.5} 锚点），nside=8；"
           "注入 `deg_normalize_mode='div'`。",
    expected="索引往返 mismatch 非 0；与 astropy-healpix 的 `ang2pix` mismatch 非 0。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md 输入表逐字「`ra_deg` = 度 | ICRS 赤经 | "
           "**任意值，内部归一化**（调用方不预处理） | 内部归一到 [0,360)」（含 RA 跨界锚点要求）；"
           "「Postconditions」1e-12 度；实现正本 healpix_core.cpp:81 "
           "`const double phi_t = std::fmod(phi, kHalfPi);`。",
    kind=harness.NEGATIVE,
    inject="`healpix_core.cpp:81` 的 `fmod(phi, π/2)` 写成 `phi / (π/2)`（简单除法归一）",
    defect_id="DEF-S11-DEG-NORM-DIV",
)
def _s11_roundtrip_deg_normalize_negative():
    pts = dense_points()  # 非并列域
    with harness.evidence() as ev:
        nside = 8
        ra = np.fromiter((p[0] for p in pts), float, len(pts))
        dec = np.fromiter((p[1] for p in pts), float, len(pts))
        o_ah = oracle_ang2pix_astropy(nside, ra, dec)
        stable = stability_mask(nside, pts)
        ev.record("stable_points", float(stable.sum()),
                  note=f"共 {len(pts)} 点；跨实现判据只在稳定域上取 mismatch")
        harness.exact(int(np.count_nonzero(
            np.fromiter((H.ang2pix_nest(nside, p[0], p[1]) for p in pts),
                        np.int64, len(pts))[stable] != o_ah[stable])), 0,
            what="未注入时与 astropy-healpix 的前置条件")
        with H.inject(deg_normalize_mode="div"):
            mm, ang = _roundtrip_probe(nside)
            mine = np.fromiter((H.ang2pix_nest(nside, p[0], p[1]) for p in pts),
                               np.int64, len(pts))
            mm_oracle = int(np.count_nonzero(mine[stable] != o_ah[stable]))
        ev.record("index_mismatch", float(mm), 0.0, note=f"共 {len(pts)} 点")
        ev.record("oracle_mismatch", float(mm_oracle), 0.0,
                  note="与 astropy-healpix 的 ang2pix 逐点对拍")
        ev.record("roundtrip_ang_deg", ang, TOL_ROUNDTRIP_DEG, unit="deg")
        harness.is_true(mm > 0, "角度归一化缺陷没被索引往返腿抓住（恒绿）")
        harness.is_true(mm_oracle > 0, "角度归一化缺陷没被 oracle 对拍抓住（恒绿）")
        harness.is_true(ang > TOL_ROUNDTRIP_DEG,
                        "角度归一化缺陷没被 1e-12 度角腿抓住（恒绿）")


# ---------------------------------------------------------------------------
# S11 ⑤ order ≤ 29 越界显式拒
# ---------------------------------------------------------------------------

@harness.test(
    "healpix.s11.order_ceiling",
    intent="order ≤ 29：order=29 合法通过；order=30 显式失败（不返哨兵、不静默降级）。",
    inputs="`nside_to_order(2^29)`、`order_to_nside(29)`、`require_valid_nside(2^29)`；"
           "越界输入 `2^30`；非 2 的幂输入 `3, 5, 6, 1000, 0`。",
    expected="order=29 全部正常返回；`2^30` 抛 `HealpixInvalidArgument`；"
             "非 2 的幂与 0 抛 `HealpixInvalidArgument`；"
             "`child_nest(1, 32)`（2·shift=64）与 `tile_to_leaf_nest(1, 0, 32)` 抛 "
             "`HealpixOverflow`；`leaf_order < tile_order` 抛 `HealpixInvalidArgument`。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md 输入表逐字「`nside` 必须 2 的幂、"
           "`2^k，k ≤ 29`」+「Preconditions」逐字「order ≤ 29」+「非法输入的显式失败面」"
           "逐字（`invalid_argument` / `overflow_error` / 「**唯一不抛的退化面** = "
           "`pix2ang_nest` 的 ipix 越界」）；eng/tests/unit/tolerances.py#healpix.max_order；"
           "产品侧的上限落点 lib/algorithms/coverage/src/stage2_common.cpp:93 与 "
           "lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1249（见文件头冲突登记 1）。",
    criteria=["S11"],
)
def _s11_order_ceiling():
    with harness.evidence() as ev:
        harness.exact(H.nside_to_order(1 << TOL_MAX_ORDER), TOL_MAX_ORDER,
                      what="order=29 应合法")
        harness.exact(H.order_to_nside(TOL_MAX_ORDER), 1 << TOL_MAX_ORDER,
                      what="order_to_nside(29)")
        harness.exact(H.require_valid_nside(1 << TOL_MAX_ORDER), 1 << TOL_MAX_ORDER,
                      what="require_valid_nside(2^29)")
        ev.record("max_order", float(TOL_MAX_ORDER), note="冻结值 tolerances.HEALPIX_MAX_ORDER")
        harness.raises(H.HealpixInvalidArgument,
                       lambda: H.nside_to_order(1 << (TOL_MAX_ORDER + 1)),
                       what="order=30 必须显式失败")
        harness.raises(H.HealpixInvalidArgument,
                       lambda: H.ang2pix_nest(1 << (TOL_MAX_ORDER + 1), 10.0, 10.0),
                       what="order=30 的 ang2pix 必须显式失败")
        for bad in (3, 5, 6, 1000, 0):
            harness.raises(H.HealpixInvalidArgument,
                           lambda b=bad: H.require_valid_nside(b),
                           what=f"nside={bad} 非 2 的幂必须抛 invalid_argument")
        harness.raises(H.HealpixOverflow, lambda: H.child_nest(1, 32),
                       what="child_nest 移位溢出必须抛 overflow_error")
        harness.raises(H.HealpixOverflow,
                       lambda: H.tile_to_leaf_nest(1, 0, TOL_MAX_ORDER + 3),
                       what="tile_to_leaf_nest 移位溢出必须抛 overflow_error")
        harness.raises(H.HealpixInvalidArgument,
                       lambda: H.leaf_to_tile_nest(5, 5, 9),
                       what="leaf_order < tile_order 必须抛 invalid_argument")
        harness.raises(H.HealpixInvalidArgument,
                       lambda: H.tile_to_leaf_nest(5, 9, 5),
                       what="leaf_order < tile_order 必须抛 invalid_argument")


@harness.test(
    "healpix.s11.order_clamped_negative",
    intent="负例：把 order 越界改成**夹逼**（静默降级到上限）必须判红。",
    inputs="`2^30`（order 30）；注入 `order_clamp_mode='clamp'`。",
    expected="`nside_to_order(2^30)` 返回 29 而不是抛异常 ⇒ 显式失败面判据失效 ⇒ 判红成立。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「非法输入的显式失败面」逐字"
           "「**不返哨兵、不静默改写**」；「Preconditions」order ≤ 29。",
    kind=harness.NEGATIVE,
    inject="`nside_to_order` 的 order 上限检查改成 `k = min(k, 29)`（夹逼 / 静默降级）",
    defect_id="DEF-S11-ORDER-CLAMP",
)
def _s11_order_clamped_negative():
    with harness.evidence() as ev:
        harness.raises(H.HealpixInvalidArgument,
                       lambda: H.nside_to_order(1 << (TOL_MAX_ORDER + 1)),
                       what="未注入时的前置条件")
        raised = False
        got = -1
        with H.inject(order_clamp_mode="clamp"):
            try:
                got = H.nside_to_order(1 << (TOL_MAX_ORDER + 1))
            except H.HealpixInvalidArgument:
                raised = True
        ev.record("clamped_order", float(got), float(TOL_MAX_ORDER),
                  note="夹逼后**未抛异常**并返回 order（正确行为应抛 invalid_argument）"
                       if not raised else "注入后仍抛异常（缺陷未生效）")
        harness.is_false(raised,
                         "夹逼行为没被 order 上限判据抓住：该缺陷下本应抛异常却正常返回")


@harness.test(
    "healpix.s11.nside_round_up_negative",
    intent="负例：把 `nside` 非 2 的幂改成**静默向上取整**必须判红。",
    inputs="`nside = 3, 5, 6, 1000`；注入 `nside_round_up=True`。",
    expected="`require_valid_nside(3)` 不抛异常并返回 4 ⇒ 显式失败面判据失效 ⇒ 判红成立。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md 输入表逐字「`nside` 无量纲 | — | "
           "**必须 2 的幂**（**禁静默向上取整**）」+「非法输入的显式失败面」逐字"
           "「`nside` 非 2 的幂 → `std::invalid_argument`」；"
           "实现正本 lib/algorithms/shared/healpix/healpix_core.h:25,60。",
    kind=harness.NEGATIVE,
    inject="`healpix_core.h:60` 的 `invalid_argument` 改成静默向上取整到最近的 2 的幂",
    defect_id="DEF-S11-NSIDE-ROUNDUP",
)
def _s11_nside_round_up_negative():
    with harness.evidence() as ev:
        for bad in (3, 5, 6, 1000):
            harness.raises(H.HealpixInvalidArgument,
                           lambda b=bad: H.require_valid_nside(b),
                           what=f"未注入时的前置条件 nside={bad}")
        n_rounded = 0
        with H.inject(nside_round_up=True):
            for bad in (3, 5, 6, 1000):
                got = H.require_valid_nside(bad)
                ev.record(f"nside={bad} →", float(got),
                          note="静默向上取整后的生效值")
                if got != bad:
                    n_rounded += 1
        harness.is_true(n_rounded == 4,
                        "静默向上取整没被 nside 失败面判据抓住（恒绿）")


# ===========================================================================
# S11 ipix ∈ [0, 12·nside²) 域校验（含唯一不抛的退化面）
# ===========================================================================

@harness.test(
    "healpix.s11.ipix_domain",
    intent="ipix 域校验：`[0, 12·nside²)` 全域合法；越界触发**唯一不抛的退化面** —— "
           "`pix2ang_nest` 返回 `(0, 0)`（调用方约定），**如实写成正例**。",
    inputs="nside ∈ {1,4,16,64}：域内抽样 + 边界 0 与 12·nside²−1 + 越界 "
           "12·nside² 与 12·nside²+1000。",
    expected="域内索引返回的角度落在 dec ∈ [−90,90]、ra ∈ [0,360)；域外索引精确返回 "
             "`(0.0, 0.0)`（这是**正本逐字豁免**的退化面，不是失败）；"
             "`npix(nside) == 12·nside²` 精确。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md 输入表逐字「`ipix` 无量纲 | NESTED | "
           "`[0, 12·nside²)`」+「非法输入的显式失败面」逐字「**唯一不抛的退化面** = "
           "`pix2ang_nest` 的 ipix 越界（ra=dec=0，调用方约定）」；"
           "实现正本 lib/algorithms/shared/healpix/healpix_core.cpp:249-255。",
    criteria=["S11"],
)
def _s11_ipix_domain():
    with harness.evidence() as ev:
        rng = np.random.default_rng(H.SAMPLE_SEED + 21)
        for nside in (1, 4, 16, 64):
            total = H.npix(nside)
            harness.exact(total, 12 * nside * nside, what=f"npix(nside={nside})")
            for ipix in (0, total - 1,
                         *[int(v) for v in rng.integers(0, total, 300)]):
                ra, dec = H.pix2ang_nest(nside, ipix)
                harness.is_true(0.0 <= ra < 360.0,
                                f"ra 越界 ipix={ipix} ra={ra}")
                harness.is_true(-90.0 <= dec <= 90.0,
                                f"dec 越界 ipix={ipix} dec={dec}")
            for bad in (total, total + 1000):
                harness.exact(H.pix2ang_nest(nside, bad), (0.0, 0.0),
                              what=f"ipix={bad} 越界必须退化为 (0,0)（正本豁免面）")
                ev.record(f"nside={nside} ipix={bad} →", "ra=dec=0")
            ev.record(f"nside={nside} npix", float(total), float(12 * nside * nside))


@harness.test(
    "healpix.s11.ipix_unguarded_negative",
    intent="负例：去掉 `pix2ang_nest` 的 ipix 域校验后，越界索引**不得**仍返回 `(0,0)` —— "
           "证明该退化面是受校验保护的、不是碰巧为 0。",
    inputs="nside ∈ {4,16}；注入 `ipix_domain_guard=False`；越界索引 12·nside² 与 +1000。",
    expected="至少一个越界索引返回的角度**不是** `(0.0, 0.0)` ⇒ 判红成立；"
             "并记录实际返回的角度，证明缺陷确实改变了行为。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md 输入表「`ipix ∈ [0, 12·nside²)`」；"
           "「非法输入的显式失败面」「唯一不抛的退化面 = `pix2ang_nest` 的 ipix 越界」；"
           "实现正本 healpix_core.cpp:255 `if (ipix >= 12ULL * npface) return;`。",
    kind=harness.NEGATIVE,
    inject="`healpix_core.cpp:255` 的 `if (ipix >= 12ULL * npface) return;` 被删掉",
    defect_id="DEF-S11-IPIX-NOGUARD",
)
def _s11_ipix_unguarded_negative():
    with harness.evidence() as ev:
        for nside in (4, 16):
            total = H.npix(nside)
            for bad in (total, total + 1000):
                harness.exact(H.pix2ang_nest(nside, bad), (0.0, 0.0),
                              what="未注入时的前置条件")
            leaked = []
            with H.inject(ipix_domain_guard=False):
                for bad in (total, total + 1000):
                    got = H.pix2ang_nest(nside, bad)
                    if got != (0.0, 0.0):
                        leaked.append((bad, got))
            ev.record(f"nside={nside} 越界仍返回非哨兵值的个数",
                      float(len(leaked)), 0.0,
                      note=f"样例 {leaked[:2]}")
            harness.is_true(len(leaked) > 0,
                            "去掉域校验后仍返回 (0,0)（该判据对此缺陷无牙齿）")


@harness.test(
    "healpix.s11.child_overflow_unguarded_negative",
    intent="负例：去掉 `child_nest` 的移位溢位校验后，必须不再抛 `overflow_error`。",
    inputs="`child_nest(1<<20, 12)`（2·shift=24，结果需 44 位，合法）与 "
           "`child_nest(1, 32)`（2·shift=64）、`child_nest(1<<40, 12)`（需 64 位）；"
           "注入 `child_overflow_guard=False`。",
    expected="合法输入仍给正确值（证明注入只动校验面、不动算术面）；"
             "溢位输入不再抛异常 ⇒ 判红成立。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「非法输入的显式失败面」逐字"
           "「`child_nest` 移位溢出 → `std::overflow_error`（**禁止 UB**）」；"
           "实现正本 lib/algorithms/shared/healpix/healpix_core.cpp:320-324。",
    kind=harness.NEGATIVE,
    inject="`healpix_core.cpp:321` 的 `if (bits >= 64 || (ipix >> (64u - bits)) != 0) "
           "throw std::overflow_error(...)` 被删掉（回绕 = UB）",
    defect_id="DEF-S11-CHILD-NOGUARD",
)
def _s11_child_overflow_unguarded_negative():
    with harness.evidence() as ev:
        harness.exact(H.child_nest(1 << 20, 12), (1 << 20) << 24,
                      what="合法移位（44 位）的前置条件")
        harness.raises(H.HealpixOverflow, lambda: H.child_nest(1, 32),
                       what="2·shift=64 的前置条件")
        harness.raises(H.HealpixOverflow, lambda: H.child_nest(1 << 40, 12),
                       what="结果左移溢出的前置条件")
        leaked = []
        with H.inject(child_overflow_guard=False):
            for ipix, shift in ((1, 32), (1 << 40, 12), (1 << 62, 2)):
                try:
                    got = H.child_nest(ipix, shift)
                    leaked.append((ipix, shift, got))
                except H.HealpixOverflow:
                    leaked.append((ipix, shift, "still_throws"))
        ev.record("未抛异常的溢位用例数",
                  float(sum(1 for e in leaked if e[2] != "still_throws")), 0.0,
                  note=f"样例 {leaked[:2]}")
        harness.is_true(any(e[2] != "still_throws" for e in leaked),
                        "去掉溢位校验后仍然抛（该判据对此缺陷无牙齿）")


@harness.test(
    "healpix.s11.tile_to_leaf_overflow_negative",
    intent="负例：去掉 `tile_to_leaf_nest` 的移位溢位校验后，必须不再抛 `overflow_error`。",
    inputs="`tile_to_leaf_nest(1, 0, 32)`（2·shift=64）、`tile_to_leaf_nest(1<<40, 0, 12)`"
           "（结果 64 位）、`tile_to_leaf_nest(3, 0, 30)`（结果 60 位合法）；"
           "注入 `tile_to_leaf_overflow_guard=False`。",
    expected="合法输入仍给正确值；溢位输入不再抛异常 ⇒ 判红成立。",
    source="docs/science/algorithms/HEALPIX_MAPPING.md「非法输入的显式失败面」逐字"
           "「`tile_to_leaf_nest` 移位溢出 → `std::overflow_error`」；"
           "实现正本 lib/algorithms/shared/healpix/healpix_core.h:97-100。",
    kind=harness.NEGATIVE,
    inject="`healpix_core.h:97` 的 `if (bits >= 64 || (tile_ipix >> (64u - bits)) != 0) "
           "throw std::overflow_error(...)` 被删掉（回绕 = UB）",
    defect_id="DEF-S11-T2L-NOGUARD",
)
def _s11_tile_to_leaf_overflow_negative():
    with harness.evidence() as ev:
        harness.exact(H.tile_to_leaf_nest(3, 0, 30), 3 << 60,
                      what="合法移位的前置条件")
        harness.raises(H.HealpixOverflow,
                       lambda: H.tile_to_leaf_nest(1, 0, TOL_MAX_ORDER + 3),
                       what="2·shift=64 的前置条件")
        harness.raises(H.HealpixOverflow, lambda: H.tile_to_leaf_nest(1 << 40, 0, 12),
                       what="结果左移溢出的前置条件")
        leaked = []
        with H.inject(tile_to_leaf_overflow_guard=False):
            for tile_ipix, tile_order, leaf_order in (
                    (1, 0, TOL_MAX_ORDER + 3), (1 << 40, 0, 12), (1 << 62, 0, 2)):
                try:
                    leaked.append((tile_ipix, tile_order, leaf_order,
                                   H.tile_to_leaf_nest(tile_ipix, tile_order,
                                                       leaf_order)))
                except H.HealpixOverflow:
                    leaked.append((tile_ipix, tile_order, leaf_order,
                                   "still_throws"))
        ev.record("未抛异常的溢位用例数",
                  float(sum(1 for e in leaked if e[3] != "still_throws")), 0.0,
                  note=f"样例 {leaked[:2]}")
        harness.is_true(any(e[3] != "still_throws" for e in leaked),
                        "去掉溢位校验后仍然抛（该判据对此缺陷无牙齿）")


# ===========================================================================
# S10 · 欠采样重建候选枚举完备性 false_negative = 0
# ===========================================================================

@harness.test(
    "healpix.s10.oracle_is_exhaustive",
    intent="重建 S10 穷举 oracle 本身：精确交叠面积集合与**全域穷举**逐点一致，"
           "且 Σ_p a_jp = A_drop 精确闭合（证明取样超集没有漏项、闭式面积算对了）。",
    inputs="nside ∈ {4,8,16}（全域 12·nside² ≤ 3072，可穷举）；每个 nside 8 个 drop，"
           "覆盖全域随机与压在 |z| = 2/3 的 drop。",
    expected="（a）`oracle_overlap_superset` 取到的超集算出的真交叠集合与"
             "**对全部 12·nside² 个像素逐一精确求交叠**的集合逐位相同；"
             "（b）`|Σ_p a_jp / A_drop − 1| ≤ 1e-12`（几何闭合 L1，"
             "`DRIZZLE.md` §3.7 逐字构造级恒等）。",
    source="docs/science/drizzle/DRIZZLE.md §3.7「几何闭合 L1（构造级）」逐字"
           "`Σ_p a_jp = A_drop,j`（主判据）、「面积由球面立体角解析式累加」；"
           "§7.2 书目级条目 Van Oosterom & Strackee 1983, IEEE TBME 30(2), 125–126"
           "（球面三角形立体角闭式）；docs/engineering/testing/TEST.md §13"
           "「外部求解器不可用时…不以跳过冒充通过」（本条是 healpy.query_polygon "
           "与 astropy.coordinates.polygon 双双不可用后的降级 oracle）。",
    criteria=["S10"],
)
def _s10_oracle_is_exhaustive():
    with harness.evidence() as ev:
        for nside in (4, 8, 16):
            corners_all = hp.boundaries(nside, np.arange(H.npix(nside)),
                                        nest=True)          # (npix, 3, 4)
            corners_all = np.transpose(corners_all, (0, 2, 1))
            for ra, dec, pa, scale, pixfrac in s10_drop_geometries(nside, 8):
                corners = H.gnomonic_drop_corners(ra, dec, pa, scale, pixfrac)
                center, max_angle = H.drop_bounding_circle(corners)
                sup = oracle_overlap_superset(
                    nside, center, max_angle + 2.0 * closed_hp_res_rad(nside))
                fast = oracle_true_overlaps(corners, nside, sup)
                brute: Dict[int, float] = {}
                for ipix in range(H.npix(nside)):
                    b = corners_all[ipix]
                    area = H.spherical_intersection_area(
                        corners, tuple((float(b[k][0]), float(b[k][1]),
                                       float(b[k][2])) for k in range(4)))
                    if area > 0.0:
                        brute[ipix] = area
                harness.exact(set(fast), set(brute),
                              what=f"nside={nside} 超集法与全域穷举的真交叠集合不一致")
                a_drop = H.polygon_area(corners)
                rel = abs(sum(brute.values()) - a_drop) / a_drop
                harness.less_equal(rel, 1e-12,
                                   what=f"nside={nside} 几何闭合 L1：Σ_p a_jp = A_drop")
            ev.record(f"nside={nside} 超集/穷举一致", 1.0, note="逐位相同的 drop 数 = 8")


@harness.test(
    "healpix.s10.candidate.zero_false_negative",
    intent="S10 主判据：`3.0·hp_res` 保守候选查询圆盘的候选集合必须**零漏选** "
           "（`false_negative == 0`）。",
    inputs="nside ∈ {4,8,16,32}，每个 nside 32 个固定种子 drop（pixfrac ∈ {0.6,0.8,1.0}、"
           "像元尺度 ∈ {0.5,1.0,1.5,2.0}·hp_res、三分之一压在 |z| = 2/3 附近）。",
    expected="`|真交叠集合 \\ 候选集合| == 0`（精确档，`T02` S10 行逐字 "
             "`false_negative = 0`）；缓冲半径逐字取 `3.0·hp_res`"
             "（`DRIZZLE.md` §4 参数表「候选查询缓冲 `3.0·hp_res` rad | 保守候选查询圆盘，"
             "**保证零漏选**」）。",
    source="审核包-R2/T02-门禁退役与判据清单.md §2.1 S10 行逐字"
           "「欠采样重建候选枚举完备性 `false_negative=0`」「**穷举 oracle 须重建**」；"
           "docs/science/drizzle/DRIZZLE.md §4 参数表「候选查询缓冲 `3.0 · hp_res` rad | "
           "保守候选查询圆盘，保证零漏选」；§5.1「零漏选门」逐字"
           "「红：缩小保守半径或查询缓冲」。",
    criteria=["S10"],
)
def _s10_candidate_zero_false_negative():
    with harness.evidence() as ev:
        total_drops = 0
        total_true = 0
        total_false_neg = 0
        total_cand = 0
        for nside in (4, 8, 16, 32):
            drops = s10_drop_geometries(nside, 32)
            n_drop, n_true, fn, missed, cand = s10_measure(nside, drops, 3.0)
            total_drops += n_drop
            total_true += n_true
            total_false_neg += fn
            total_cand += cand
            ev.record(f"nside={nside} false_negative", float(fn), 0.0,
                      note=f"drop={n_drop} 真交叠叶总数={n_true} 候选叶总数={cand}")
            harness.exact(fn, 0,
                          what=f"nside={nside} 漏选数非 0（漏选样例 {missed[:2]}）")
        ev.record("false_negative_total", float(total_false_neg), 0.0,
                  note=f"drop={total_drops} 真交叠叶={total_true} 候选叶={total_cand}")


@harness.test(
    "healpix.s10.buffer_075_negative",
    intent="负例：把保守查询缓冲从 `3.0·hp_res` 缩到 `0.75·hp_res` 必须出现漏选并判红。",
    inputs="nside ∈ {8,16,32}，每个 32 个固定种子 drop（与主判据同一批几何）。",
    expected="`false_negative > 0` 且逐个记录漏选叶的 `nside / ra / dec / pa / scale / "
             "pixfrac / ipix / 交叠面积`；判红成立。",
    source="docs/science/drizzle/DRIZZLE.md §5.1「零漏选门」逐字"
           "「**红：缩小保守半径或查询缓冲**」；§4 参数表缓冲 `3.0·hp_res`。",
    kind=harness.NEGATIVE,
    inject="`spherical_overlap.cpp:1567` 的 `buffer_rad = 3.0 * hp_res_rad` 改成 "
           "`0.75 * hp_res_rad`",
    defect_id="DEF-S10-BUFFER-075",
)
def _s10_buffer_075_negative():
    with harness.evidence() as ev:
        # 消融自证的前置条件：同一批几何在冻结缓冲 3.0·hp_res 下必须零漏选
        for nside in (8, 16, 32):
            _, _, fn3, _, _ = s10_measure(nside, s10_drop_geometries(nside, 32), 3.0)
            harness.exact(fn3, 0,
                          what=f"nside={nside} 缓冲 3.0·hp_res 的前置条件（应零漏选）")
        fn_total = 0
        samples = []
        for nside in (8, 16, 32):
            drops = s10_drop_geometries(nside, 32)
            n_drop, n_true, fn, missed, cand = s10_measure(nside, drops, 0.75)
            fn_total += fn
            samples.extend(missed[:2])
            ev.record(f"nside={nside} false_negative@0.75", float(fn), 0.0,
                      note=f"drop={n_drop} 真交叠叶={n_true} 候选叶={cand}")
        ev.record("false_negative_total@0.75", float(fn_total), 0.0,
                  note=f"漏选样例 {samples[:3]}")
        harness.is_true(fn_total > 0,
                        "缩小缓冲到 0.75·hp_res 后仍零漏选 ⇒ 该负例几何没跨够远的叶")


@harness.test(
    "healpix.s10.buffer_050_negative",
    intent="负例：把保守查询缓冲缩到 `0.5·hp_res` 必须出现更多漏选并判红。",
    inputs="nside ∈ {8,16,32}，每个 32 个固定种子 drop（与主判据同一批几何）。",
    expected="`false_negative > 0`；且作为收缩单调性的对照，其漏选总数应不少于 "
             "`0.75·hp_res` 档（缓冲越小、漏选越多）。",
    source="docs/science/drizzle/DRIZZLE.md §5.1「零漏选门」逐字"
           "「红：缩小保守半径或查询缓冲」；§4「候选查询缓冲 `3.0 · hp_res` rad」。",
    kind=harness.NEGATIVE,
    inject="`spherical_overlap.cpp:1567` 的 `buffer_rad = 3.0 * hp_res_rad` 改成 "
           "`0.5 * hp_res_rad`",
    defect_id="DEF-S10-BUFFER-050",
)
def _s10_buffer_050_negative():
    with harness.evidence() as ev:
        # 消融自证的前置条件：同一批几何在冻结缓冲 3.0·hp_res 下必须零漏选
        for nside in (8, 16, 32):
            _, _, fn3, _, _ = s10_measure(nside, s10_drop_geometries(nside, 32), 3.0)
            harness.exact(fn3, 0,
                          what=f"nside={nside} 缓冲 3.0·hp_res 的前置条件（应零漏选）")
        fn_total = 0
        samples = []
        for nside in (8, 16, 32):
            drops = s10_drop_geometries(nside, 32)
            n_drop, n_true, fn, missed, cand = s10_measure(nside, drops, 0.5)
            fn_total += fn
            samples.extend(missed[:2])
            ev.record(f"nside={nside} false_negative@0.50", float(fn), 0.0,
                      note=f"drop={n_drop} 真交叠叶={n_true} 候选叶={cand}")
        ev.record("false_negative_total@0.50", float(fn_total), 0.0,
                  note=f"漏选样例 {samples[:3]}")
        harness.is_true(fn_total > 0,
                        "缩小缓冲到 0.5·hp_res 后仍零漏选 ⇒ 该负例几何没跨够远的叶")


# ---------------------------------------------------------------------------
# 正本冲突登记（只取证，不判门；见文件头「正本与实现冲突 2」）
# ---------------------------------------------------------------------------

def healpix_chord_tiling_note() -> Dict[str, float]:
    """复算 `DRIZZLE.md` §5.2「叶边界弦亏缺 `0.1043885/nside²` sr」的登记量。

    逐字口径：把每个叶的边界取四角弦表示，算出该四角弦球面四边形的面积，与
    `A_cell = π/(3·nside²)` 比。返回最坏相对差、绝对差与天球铺满残差。
    """
    out: Dict[str, float] = {}
    for nside in (4, 16, 64):
        b = np.transpose(hp.boundaries(nside, np.arange(H.npix(nside)),
                                       nest=True), (0, 2, 1))
        a_cell = CELL_AREA_SR.value(nside)
        worst_abs = 0.0
        total = 0.0
        for ipix in range(H.npix(nside)):
            poly = tuple((float(b[ipix][k][0]), float(b[ipix][k][1]),
                          float(b[ipix][k][2])) for k in range(4))
            area = H.polygon_area(poly)
            total += area
            worst_abs = max(worst_abs, abs(area - a_cell))
        out[f"nside={nside} worst_abs_dev_sr"] = worst_abs
        out[f"nside={nside} doc_abs_dev_sr"] = 0.1043885 / nside ** 2
        out[f"nside={nside} tiling_residual_sr"] = abs(total - 4.0 * math.pi)
    return out


@harness.test(
    "healpix.s10.chord_tiling_conflict_registered",
    intent="如实登记「正本与实现冲突 2」：`DRIZZLE.md` §5.2 登记的逐叶面积主项"
           "「叶边界弦亏缺 `0.1043885/nside²` sr」与四角弦表示的实测不吻合。"
           "本条**只取证不判门**，作为 UNRESOLVED 输入随审核包交付。",
    inputs="nside ∈ {4,16,64}；把全部 12·nside² 个叶的边界取四角弦表示，"
           "逐叶算闭式球面面积。",
    expected="（a）四角弦四边形**铺满天球**（Σ 与 4π 的残差落在 `TEST.md` §4 归约档 "
             "`C·γ_n·Σ|terms|` 内）；"
             "（b）逐叶面积与 `A_cell` 的相对差是 `O(nside⁻²)`，绝对差是 `O(nside⁻⁴)` sr，"
             "随 `nside` **收敛**，与文档「随 nside 增大而减小、绝对亏缺 0.1043885/nside²」"
             "的标度与量级都不吻合（量级差 nside² 倍）。",
    source="docs/science/drizzle/DRIZZLE.md §5.2「叶边界弦亏缺」逐字"
           "「其绝对亏缺为 `0.1043885/nside²` sr（极限相对亏缺 `2√2/π − 1 = −9.968368384e-2`），"
           "**随 `nsize` 增大而减小**，与像元角尺度的平方成反比」；§4.1 逐字"
           "「生产 `nside` 下叶边界恒为四角弦」。复算 oracle = `healpy.boundaries`（第三方）"
           "+ Van Oosterom-Strackee 闭式。",
    criteria=["S10"],
)
def _s10_chord_tiling_conflict_registered():
    with harness.evidence() as ev:
        for nside in (4, 16, 64):
            b = np.transpose(hp.boundaries(nside, np.arange(H.npix(nside)),
                                           nest=True), (0, 2, 1))
            a_cell = CELL_AREA_SR.value(nside)
            worst_rel = 0.0
            total = 0.0
            for ipix in range(H.npix(nside)):
                poly = tuple((float(b[ipix][k][0]), float(b[ipix][k][1]),
                              float(b[ipix][k][2])) for k in range(4))
                area = H.polygon_area(poly)
                total += area
                worst_rel = max(worst_rel, abs(area - a_cell) / a_cell)
            ev.record(f"nside={nside} 四角弦铺满天球残差",
                      abs(total - 4.0 * math.pi),
                      tol.reduction_tolerance(H.npix(nside), 4.0 * math.pi),
                      unit="sr", note="门限 = 归约档 C·γ_n·Σ|terms|（TEST.md §4）")
            ev.record(f"nside={nside} 逐叶最坏相对差", worst_rel,
                      note=f"文档常数 0.1043885/nside² 对应相对量 "
                           f"{0.1043885 / nside ** 2 / a_cell:.6e}")
            harness.less_equal(
                abs(total - 4.0 * math.pi),
                tol.reduction_tolerance(H.npix(nside), 4.0 * math.pi),
                what=f"nside={nside} 四角弦四边形应当铺满天球（归约档）")
        ev.record("conflict", "DRIZZLE.md §5.2 的 0.1043885/nside² 与实测不吻合",
                  note="UNRESOLVED：本条只取证不判门；实测标度为 O(nside⁻⁴) sr "
                       "（相对 O(nside⁻²)），且四角弦铺满残差 ≤ 1e-12 sr")
