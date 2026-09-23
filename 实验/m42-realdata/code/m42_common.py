# 实验/m42-realdata/code/m42_common.py
# -*- coding: utf-8 -*-
"""M42 真实数据端到端验证单元 —— 公共库。

只读 `run/RELEASE-05/vis/out/` 下的**现成产物**（normalize 两个 block + mosaic + export），
不重跑任何 astrocs 命令；除本单元与 `run/M42-REALDATA-01/` 外不写任何路径。

设计约束（照 `实验/` 既有单元体例）：
- 固定 seed：SEED_BASE = 20261003，随机性一律由 derive_rng(tag) 派生；
- 大产物一律**流式/分块**读（mosaic 32 GB、normalize 40 GB），绝不整块载入；
- HEALPix 公式逐字移植自生产权威 `lib/algorithms/shared/healpix/healpix_core.cpp:155-226`
  （hp_to_xyz，迁移自 astrometry.net healpix.c，BSD-3），并由 p2_samples.json 的
  33472 个控制点 (leaf_ipix -> ra/dec) 做**实测校验**（见 c4 的 SELFTEST 判据）。
"""
from __future__ import annotations

import glob
import hashlib
import json
import math
import os
from pathlib import Path

import numpy as np

SEED_BASE = 20261003

ROOT = Path(__file__).resolve().parents[3]
UNIT = Path(__file__).resolve().parents[1]
CODE = Path(__file__).resolve().parent
RESULTS = UNIT / "results"
DOCS = UNIT / "docs"

VIS = ROOT / "run" / "RELEASE-05" / "vis"
OUT = VIS / "out"
P1T2 = OUT / "m42_p1_t2"
P1T3 = OUT / "m42_p1_t3"
P2 = OUT / "m42_p2"
P3 = OUT / "m42_p3"
VISREPORT = VIS / "vis" / "m42" / "vis_report.json"
CONFIG = VIS / "configs" / "p1_m42.json"

# 冻结常量（逐条来源见 docs/CRITERIA.md）
NSIDE_LEAF = 262144            # 叶 nside（= 2^18）
HP_RES_ARCSEC = 0.8051921277697045   # p1_stack.json:hp_res_arcsec
TILE_W = 512
TILE_SPAN = TILE_W * TILE_W
A_CELL = 4.0 * math.pi / (12.0 * NSIDE_LEAF * NSIDE_LEAF)   # 叶立体角 [sr] = 1.5238e-11
K_MAD = 1.4826
ARCSEC = 180.0 * 3600.0 / math.pi


def derive_rng(tag: str) -> np.random.Generator:
    h = hashlib.sha256(("M42-REALDATA|%d|%s" % (SEED_BASE, tag)).encode("utf-8")).digest()
    return np.random.default_rng(int.from_bytes(h[:8], "little"))


# ---------------------------------------------------------------- 目录/清单
def read_json(p) -> dict:
    with open(p, "r", encoding="utf-8") as f:
        return json.load(f)


def p1_dir(block: str) -> Path:
    return P1T2 if block == "t2" else P1T3


def frame_ids(block: str):
    return [f["frame_key"] for f in read_json(p1_dir(block) / "p1_phot.json")["frames"]]


def hips_root(block: str, fid: str) -> Path:
    return p1_dir(block) / fid


def hips_tiles(block: str, fid: str, product: str = "signal", order: int = 9):
    d = hips_root(block, fid) / product / ("Norder%d" % order)
    return sorted(int(os.path.basename(f)[4:-5])
                  for f in glob.glob(str(d / "Dir*" / "Npix*.fits")))


def hips_tile_path(block: str, fid: str, product: str, ipix: int, order: int = 9) -> Path:
    # HiPS Dir 命名：Dir<floor(ipix/10000)*10000>（IVOA HiPS 1.0）
    return (hips_root(block, fid) / product / ("Norder%d" % order)
            / ("Dir%d" % ((ipix // 10000) * 10000)) / ("Npix%d.fits" % ipix))


def read_tile(path, dtype=np.float64):
    from astropy.io import fits
    with fits.open(str(path), memmap=False) as h:
        return np.asarray(h[0].data, dtype=dtype)


def read_tile_opt(block: str, fid: str, product: str, ipix: int):
    p = hips_tile_path(block, fid, product, ipix)
    return read_tile(p) if p.exists() else None


# ---------------------------------------------------------------- HEALPix NESTED
# 逐字移植 lib/algorithms/shared/healpix/healpix_core.cpp:155-226（hp_to_xyz）
def nest_to_xy(nest: int, order: int):
    x = 0
    y = 0
    for b in range(order):
        x |= ((nest >> (2 * b)) & 1) << b
        y |= ((nest >> (2 * b + 1)) & 1) << b
    return x, y


def hp_to_xyz(basehp: int, px: int, py: int, dx: float, dy: float, nside: int):
    ns = int(nside)
    x = float(px) + dx
    y = float(py) + dy
    equatorial = True
    zfactor = 1.0
    if basehp < 4 and (x + y) > ns:
        equatorial = False
        zfactor = 1.0
    if basehp >= 8 and (x + y) < ns:
        equatorial = False
        zfactor = -1.0
    if equatorial:
        chp = int(basehp)
        zoff = 0.0
        phioff = 0.0
        x /= ns
        y /= ns
        if chp <= 3:
            phioff = 1.0
        elif chp <= 7:
            zoff = -1.0
            chp -= 4
        else:
            phioff = 1.0
            zoff = -2.0
            chp -= 8
        z = (2.0 / 3.0) * (x + y + zoff)
        phi = math.pi / 4.0 * (x - y + phioff + 2.0 * chp)
        rad = math.sqrt(max(0.0, 1.0 - z * z))
        return rad * math.cos(phi), rad * math.sin(phi), z
    if zfactor == -1.0:
        x, y = y, x
        x = ns - x
        y = ns - y
    if y == ns and x == ns:
        phi_t = 0.0
    else:
        phi_t = math.pi * (ns - y) / (2.0 * ((ns - x) + (ns - y)))
    if phi_t < math.pi / 4.0:
        vv = abs(math.pi * (ns - x) / ((2.0 * phi_t - math.pi) * ns) / math.sqrt(3.0))
    else:
        vv = abs(math.pi * (ns - y) / (2.0 * phi_t * ns) / math.sqrt(3.0))
    z = (1.0 - vv) * (1.0 + vv)
    rad = math.sqrt(1.0 + z) * vv
    z *= zfactor
    phi = (math.pi / 2.0 * (int(basehp) - 8) + phi_t) if basehp >= 8 \
        else (math.pi / 2.0 * int(basehp) + phi_t)
    if phi < 0.0:
        phi += 2.0 * math.pi
    return rad * math.cos(phi), rad * math.sin(phi), z


def pix2ang(nside: int, ipix: int):
    """NESTED ipix -> (ra_deg, dec_deg)。"""
    order = int(round(math.log2(nside)))
    npface = nside * nside
    basehp = ipix // npface
    x, y = nest_to_xy(ipix % npface, order)
    rx, ry, rz = hp_to_xyz(basehp, x, y, 0.5, 0.5, nside)
    ra = math.degrees(math.atan2(ry, rx)) % 360.0
    dec = math.degrees(math.asin(max(-1.0, min(1.0, rz))))
    return ra, dec


def leaf_center_z(ipix: int):
    _, dec = pix2ang(NSIDE_LEAF, ipix)
    return math.sin(math.radians(dec))


def ang_sep_deg(ra1, dec1, ra2, dec2):
    r1, d1, r2, d2 = map(math.radians, (ra1, dec1, ra2, dec2))
    s = math.sin(d1) * math.sin(d2) + math.cos(d1) * math.cos(d2) * math.cos(r1 - r2)
    return math.degrees(math.acos(max(-1.0, min(1.0, s))))


# ---------------------------------------------------------------- 统计工具
def stats(v):
    v = np.asarray(v, dtype=float)
    v = v[np.isfinite(v)]
    if v.size == 0:
        return {"n": 0}
    return {"n": int(v.size), "mean": float(np.mean(v)), "median": float(np.median(v)),
            "std": float(np.std(v, ddof=1)) if v.size > 1 else 0.0,
            "min": float(np.min(v)), "max": float(np.max(v)),
            "p05": float(np.percentile(v, 5)), "p95": float(np.percentile(v, 95))}


def robust_scale(x):
    x = np.asarray(x, dtype=float)
    x = x[np.isfinite(x)]
    if x.size == 0:
        return float("nan")
    return float(K_MAD * np.median(np.abs(x - np.median(x))))


def json_dump(obj, name: str, sub: str = ""):
    d = RESULTS / sub if sub else RESULTS
    d.mkdir(parents=True, exist_ok=True)
    p = d / name
    p.write_text(json.dumps(obj, indent=2, ensure_ascii=False, default=_np_default),
                 encoding="utf-8")
    return p


def _np_default(o):
    if isinstance(o, np.floating):
        return float(o)
    if isinstance(o, np.integer):
        return int(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    if isinstance(o, np.bool_):
        return bool(o)
    raise TypeError(type(o))


class Gates:
    """判据收集器：每条判据记录来源（文档:行 / 已有实验单元）、实测值、红绿与证据等级。"""

    def __init__(self):
        self.rows = []

    def add(self, gid, desc, value, ok, source="", level="data", note=""):
        self.rows.append(dict(id=gid, desc=desc, value=value, ok=bool(ok),
                              source=source, level=level, note=note))
        return ok

    def summary(self):
        return dict(n=len(self.rows), n_pass=int(sum(r["ok"] for r in self.rows)),
                    n_fail=int(sum(not r["ok"] for r in self.rows)), rows=self.rows)
