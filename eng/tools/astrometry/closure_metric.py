#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""G-P1-WCS-CLOSURE v1 —— 天测精度外部闭环指标（冻结口径）的唯一可执行实现与门。

口径定义（唯一事实源，不得在别处重述数值）：
  docs/science/ASTROMETRY.md §11a（SCI-WCS-001）
  docs/algorithms/GATES_AND_TOLERANCES.md §3（门行 G-P1-WCS-CLOSURE /
  G-P1-WCS-CLOSURE-REPRO）与 §2（SNR_det 定义行）。

量的语义：检出星经 WCS 前向映射后的天球位置，与其在星表中 1-最近邻星的真实
大圆角距；在该样本上的 median（同时必须同报 n_matched / match_rate / p95 / max）。

独立性（ENGINEERING_SPEC §5.1「不调用生产实现的独立 Oracle」）：本工具不导入
AstroCS 任何代码，WCS 只用 astropy 从产物 JSON/FITS 头重建，星表取自外部
锥搜索产物；残差用真大圆角距，不用平面近似代替。

子命令（rc: 0=绿/通过, 1=红/检出, 2=用法或输入错误）：
  compute       --product DIR --catalog NPZ --flavor solved|header|both --out DIR
  check         A.json B.json [--median-tol-px X] [--json-out F]
  fault-inject  IN.json --mode MODE --out OUT.json
  selftest      合成场：同输入两跑必绿 + 全部负例注入必红

fail-closed（ENGINEERING_SPEC §8）：输入缺失/键缺失/样本为空/口径参数不符一律
判红，不静默通过。
"""
from __future__ import annotations

import argparse
import glob
import hashlib
import json
import math
import os
import shutil
import sys
import tempfile
from pathlib import Path

import numpy as np

SPEC_ID = "astrocs.astrometry.closure-metric/v1"
STATISTIC = "median"
MAX_SOURCES_DEFAULT = 20000
SCAN_MAX_ARCSEC = 5.0
FLAVORS = ("solved_cd_sip", "frame_header")

# 冻结口径参数（唯一数值源；与 docs/science/ASTROMETRY.md §11a 表逐行对应）
FROZEN_PARAMS = {
    "catalog_mag_max": 18.0,
    "detection_snr_min": 20.0,
    "match_radius_arcsec": 1.0,
    "statistic": STATISTIC,
    "scan_max_arcsec": SCAN_MAX_ARCSEC,
    "max_sources": MAX_SOURCES_DEFAULT,
}
# 复现容差：实测漂移 0（同输入同口径全链产物逐字节相等）⇒ 阈值 0 px。
# 1e-9 px 只是 JSON 浮点往返护栏，不是物理余量（不能当科学容差用）。
SERIALIZATION_EPS_PX = 1e-9


class Red(Exception):
    """判红：门不通过（fail-closed）。"""


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def sky_sep_arcsec(ra1, dec1, ra2, dec2):
    r1, d1, r2, d2 = (np.radians(np.asarray(x, float)) for x in (ra1, dec1, ra2, dec2))
    cos = np.sin(d1) * np.sin(d2) + np.cos(d1) * np.cos(d2) * np.cos(r1 - r2)
    return np.degrees(np.arccos(np.clip(cos, -1.0, 1.0))) * 3600.0


# ── 样本筛选（冻结口径第 1/2 条） ──────────────────────────────────────────

def load_sample(product: Path, snr_min: float, max_sources: int):
    """返回 (frame_meta, xy[N,2], n_total, n_snr_pass, capped)。

    筛选：x,y 有限 ∧ snr > snr_min；再按 flux 降序截断到 max_sources（运行时间
    上界）。截断是否生效必须随记录报告（sample_capped），否则样本不可复现。
    """
    try:
        with open(product / "p1_sources.json", encoding="utf-8") as f:
            doc = json.load(f)
    except OSError as e:
        raise Red("p1_sources.json 不可读: %s（fail-closed）" % e)
    except json.JSONDecodeError as e:
        raise Red("p1_sources.json 不可解析: %s（fail-closed）" % e)
    if not isinstance(doc, dict) or not isinstance(doc.get("frames"), list) or not doc["frames"]:
        raise Red("p1_sources.json 缺 frames 数组（fail-closed）")
    fr = doc["frames"][0]
    raw = fr.get("sources")
    if not isinstance(raw, list):
        raise Red("p1_sources.json frames[0] 缺 sources 数组（fail-closed）")
    keep = []
    for s in raw:
        x, y = s.get("x"), s.get("y")
        if x is None or y is None or not (np.isfinite(x) and np.isfinite(y)):
            continue
        keep.append(s)
    if not keep:
        raise Red("检出星样本为空（fail-closed）")
    snr = np.array([float(s.get("snr", float("nan"))) for s in keep], float)
    flux = np.array([float(s.get("flux", float("nan"))) for s in keep], float)
    pass_snr = np.isfinite(snr) & (snr > snr_min)
    n_pass = int(pass_snr.sum())
    if n_pass == 0:
        raise Red("SNR>%g 的检出星为 0（样本不可判，fail-closed）" % snr_min)
    idx = np.flatnonzero(pass_snr)
    if n_pass > max_sources:
        order = np.argsort(-flux[idx], kind="stable")
        idx = idx[order[:max_sources]]
    xy = np.array([[float(keep[i]["x"]), float(keep[i]["y"])] for i in idx], float)
    capped = bool(n_pass > max_sources)
    return fr, xy, len(keep), n_pass, capped


def load_catalog(path: Path, mag_max: float):
    """外部锥星表（npz: ra/dec/mag 单位 deg）。缺失/空 ⇒ 判红。"""
    if not path.is_file():
        raise Red("星表文件不存在: %s（fail-closed）" % path)
    try:
        z = np.load(path)
    except (OSError, ValueError) as e:
        raise Red("星表不可读: %s（fail-closed）" % e)
    for key in ("ra", "dec", "mag"):
        if key not in z:
            raise Red("星表缺键 %s（fail-closed）" % key)
    ra, dec, mag = (np.asarray(z[k], float).ravel() for k in ("ra", "dec", "mag"))
    if not (len(ra) == len(dec) == len(mag)):
        raise Red("星表 ra/dec/mag 长度不一致（fail-closed）")
    ok = np.isfinite(ra) & np.isfinite(dec) & np.isfinite(mag) & (mag < mag_max)
    ra, dec = ra[ok], dec[ok]
    if len(ra) == 0:
        raise Red("星表在 mag<%g 下的锥内星为 0（fail-closed）" % mag_max)
    return ra, dec, int(ok.sum())


# ── WCS 重建（口径第 7/8 条：solved 与 header 分别重建，禁止合并） ──────────

def build_solved_wcs(product: Path):
    from astropy.wcs import WCS, Sip
    with open(product / "p1_wcs.json", encoding="utf-8") as f:
        wj = json.load(f)
    w = wj.get("wcs")
    if not isinstance(w, dict):
        raise Red("p1_wcs.json 缺 wcs 对象（fail-closed）")
    out = WCS(naxis=2)
    out.wcs.crpix = [w["crpix1"], w["crpix2"]]
    out.wcs.crval = [w["crval1"], w["crval2"]]
    out.wcs.cd = [[w["cd11"], w["cd12"]], [w["cd21"], w["cd22"]]]
    out.wcs.ctype = [w["ctype1"], w["ctype2"]]
    sip = w.get("sip")
    if sip:
        n = int(round(len(sip["ap"]) ** 0.5))
        if n * n != len(sip["ap"]):
            raise Red("SIP 系数长度非满方阵（fail-closed）")
        mat = lambda v: np.array(v, float).reshape(n, n)
        out.sip = Sip(mat(sip["a"]), mat(sip["b"]), mat(sip["ap"]), mat(sip["bp"]),
                      out.wcs.crpix)
    return out


def build_header_wcs(product: Path):
    from astropy.io import fits
    from astropy.wcs import WCS
    cands = sorted(glob.glob(str(product / "calibrated_*.fts")))
    if not cands:
        return None
    hdr = fits.getheader(cands[0])
    return WCS(hdr, naxis=2)


def footprint(w):
    """视场外接半径：用 1-based FITS 角点（W,H 由像素范围定义）。"""
    corners = np.array([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]])
    sk = w.all_pix2world(corners, 0)
    return sk


# ── 指标计算（口径第 3/4/5/9 条） ────────────────────────────────────────

def compute_record(product: Path, cat_path: Path, flavor: str, params: dict) -> dict:
    if flavor not in FLAVORS:
        raise Red("未知 wcs_flavor: %s" % flavor)
    from scipy.spatial import cKDTree

    fr, xy, n_total, n_pass, capped = load_sample(
        product, params["detection_snr_min"], int(params["max_sources"]))
    w_solved = build_solved_wcs(product)
    w = w_solved if flavor == "solved_cd_sip" else build_header_wcs(product)
    if w is None:
        raise Red("frame_header 口径不可用：无 calibrated_*.fts（fail-closed）")

    # 视场中心/半径与像素尺度：一律取 solved WCS 的线性 CD（口径第 6 条）
    import astropy.io.fits as _fits  # noqa: F401  (保持 astropy 依赖显式)
    hdr_paths = sorted(glob.glob(str(product / "calibrated_*.fts")))
    if hdr_paths:
        from astropy.io import fits
        H = int(fits.getheader(hdr_paths[0])["NAXIS2"])
        Wd = int(fits.getheader(hdr_paths[0])["NAXIS1"])
    else:
        H = int(xy[:, 1].max() + 1)
        Wd = int(xy[:, 0].max() + 1)
    corners = np.array([[0, 0], [Wd - 1, 0], [Wd - 1, H - 1], [0, H - 1]], float)
    skc = w_solved.all_pix2world(corners, 0)
    ra0 = float(np.mean(skc[:, 0]) % 360.0)
    dec0 = float(np.mean(skc[:, 1]))
    rad = max(math.hypot((a - ra0) * math.cos(math.radians(dec0)), b - dec0)
              for a, b in skc)
    s0 = 3600.0 * math.sqrt(abs(float(np.linalg.det(w_solved.wcs.cd))))

    gra, gdec, n_cone = load_catalog(cat_path, params["catalog_mag_max"])
    cosd = math.cos(math.radians(dec0))
    gx = (gra - ra0) * cosd * 3600.0
    gy = (gdec - dec0) * 3600.0
    tree = cKDTree(np.column_stack([gx, gy]))
    sky = w.all_pix2world(xy, 0)
    qx = (sky[:, 0] - ra0) * cosd * 3600.0
    qy = (sky[:, 1] - dec0) * 3600.0
    _, idx = tree.query(np.column_stack([qx, qy]), k=1)
    sep = sky_sep_arcsec(sky[:, 0], sky[:, 1], gra[idx], gdec[idx])
    scan = float(params["scan_max_arcsec"])
    res = np.sort(sep[sep <= scan])

    rec = {
        "spec": SPEC_ID,
        "generated_by": "eng/tools/astrometry/closure_metric.py",
        "params": dict(params),
        "wcs_flavor": flavor,
        "product": str(product),
        "frame_file": fr.get("file"),
        "inputs": {
            "p1_sources_sha256": sha256_file(product / "p1_sources.json"),
            "catalog": str(cat_path),
            "catalog_sha256": sha256_file(cat_path),
            "catalog_resolution_source": "external cone catalog (npz: ra/dec/mag deg)",
        },
        "n_detected_total": int(n_total),
        "n_det_snr_pass": int(n_pass),
        "n_sample": int(len(xy)),
        "sample_capped": bool(capped),
        "n_gaia_cone": int(n_cone),
        "field_center_deg": [ra0, dec0],
        "field_radius_deg": float(rad),
        "s0_arcsec_per_px": float(s0),
        "residuals_arcsec": [float(v) for v in res],
    }
    rec["metric"] = derive_metric(rec)
    return rec


def derive_metric(rec: dict) -> dict:
    """从记录自身的残差向量与冻结参数重新导出全部指标（自证据一致性）。"""
    res = np.asarray(rec.get("residuals_arcsec", []), float)
    if res.size == 0:
        raise Red("残差向量为空（fail-closed）")
    if not np.all(np.diff(res) >= 0):
        raise Red("残差向量未排序（fail-closed）")
    p = rec["params"]
    r = float(p["match_radius_arcsec"])
    scan = float(p["scan_max_arcsec"])
    if r > scan:
        raise Red("match_radius(%g) > scan_max(%g)：证据不足（fail-closed）" % (r, scan))
    sub = res[res <= r]
    n_matched = int(sub.size)
    if n_matched == 0:
        raise Red("匹配半径 %g\" 内 0 颗（指标不可判，fail-closed）" % r)
    n_sample = int(rec["n_sample"])
    if n_sample <= 0:
        raise Red("n_sample<=0（fail-closed）")
    s0 = float(rec["s0_arcsec_per_px"])
    med_as = float(np.median(sub))
    return {
        "n_matched": n_matched,
        "match_rate": n_matched / float(n_sample),
        "median_arcsec": med_as,
        "median_px": med_as / s0,
        "p95_arcsec": float(np.percentile(sub, 95)),
        "max_arcsec": float(sub.max()),
    }


# ── 门：同数据同口径两次运行一致性（G-P1-WCS-CLOSURE-REPRO） ──────────────

def _close(a: float, b: float, tol: float) -> bool:
    return abs(a - b) <= tol


def check_records(a: dict, b: dict, median_tol_px: float = 0.0):
    """返回 (ok, lines)。任何结构性缺口 ⇒ 红（fail-closed）。"""
    lines = []
    fails = []

    def need(rec, key, tag):
        cur = rec
        for k in key.split("."):
            if not isinstance(cur, dict) or k not in cur:
                fails.append("C5 %s 缺键 %s（fail-closed）" % (tag, key))
                return None
            cur = cur[k]
        return cur

    for tag, rec in (("A", a), ("B", b)):
        if not isinstance(rec, dict):
            fails.append("C5 %s 不是对象（fail-closed）" % tag)
            continue
        if rec.get("spec") != SPEC_ID:
            fails.append("C1 %s spec 不符: %r ≠ %s" % (tag, rec.get("spec"), SPEC_ID))
        par = rec.get("params")
        if not isinstance(par, dict):
            fails.append("C1 %s 无 params（旧口径记录不可判，fail-closed）" % tag)
        else:
            for k, v in FROZEN_PARAMS.items():
                if k not in par:
                    fails.append("C1 %s params 缺 %s（fail-closed）" % (tag, k))
                elif par[k] != v:
                    fails.append("C1 %s params[%s]=%r ≠ 冻结值 %r（改口径必须走 claim）"
                                 % (tag, k, par[k], v))
        flav = rec.get("wcs_flavor")
        if flav not in FLAVORS:
            fails.append("C3 %s wcs_flavor 非法/缺失: %r（fail-closed）" % (tag, flav))

    if fails:
        return False, fails

    if a["wcs_flavor"] != b["wcs_flavor"]:
        fails.append("C3 口径混用: A=%s B=%s（solved 与 header 必须分别报告，禁止合并比较）"
                     % (a["wcs_flavor"], b["wcs_flavor"]))

    for tag, rec in (("A", a), ("B", b)):
        try:
            d = derive_metric(rec)
        except Red as e:
            fails.append("C5 %s 残差向量不可导出指标: %s" % (tag, e))
            continue
        m = rec.get("metric")
        if not isinstance(m, dict):
            fails.append("C5 %s 缺 metric 块（fail-closed）" % tag)
            continue
        if int(m.get("n_matched", -1)) != d["n_matched"]:
            fails.append("C2 %s n_matched 自证据不符: 声明 %s ≠ 按声明半径 %.4g\" 从残差重导 %d"
                         "（半径改大而不记录即在此判红）"
                         % (tag, m.get("n_matched"), rec["params"]["match_radius_arcsec"],
                            d["n_matched"]))
        for key in ("median_arcsec", "median_px", "p95_arcsec", "max_arcsec"):
            if m.get(key) is None or not _close(float(m[key]), d[key],
                                                1e-12 * max(1.0, abs(d[key]))):
                fails.append("C2 %s %s 自证据不符: 声明 %r ≠ 重导 %.12g"
                             % (tag, key, m.get(key), d[key]))
        if m.get("match_rate") is None or not _close(float(m["match_rate"]), d["match_rate"],
                                                     1e-12):
            fails.append("C2 %s match_rate 自证据不符: 声明 %r ≠ 重导 %.12g"
                         % (tag, m.get("match_rate"), d["match_rate"]))
        lines.append("C2 %s 自证据一致: n_matched=%d match_rate=%.4f median=%.4f\"(%.4f px)"
                     % (tag, d["n_matched"], d["match_rate"], d["median_arcsec"], d["median_px"]))

    if not fails:
        ma, mb = float(a["metric"]["median_px"]), float(b["metric"]["median_px"])
        na, nb = int(a["metric"]["n_matched"]), int(b["metric"]["n_matched"])
        tol = median_tol_px + SERIALIZATION_EPS_PX
        if na != nb:
            fails.append("C4 两次运行 n_matched 不等: A=%d B=%d（样本/口径/输入哈希有未记录变化）"
                         % (na, nb))
        if not _close(ma, mb, tol):
            fails.append("C4 两次运行 median 超容差: |%.6f − %.6f| = %.6g px > %.3g px"
                         % (ma, mb, abs(ma - mb), tol))
        else:
            lines.append("C4 两跑 median 一致: %.6f px vs %.6f px（容差 %.3g px，实测漂移 0）"
                         % (ma, mb, tol))
        if a["inputs"]["catalog_sha256"] != b["inputs"]["catalog_sha256"]:
            lines.append("C6 提示: 两跑星表哈希不同（A=%s B=%s）——非同输入，比较仅供参考"
                         % (a["inputs"]["catalog_sha256"][:12], b["inputs"]["catalog_sha256"][:12]))
    return (len(fails) == 0), (fails if fails else lines)


# ── 负例注入（门能红；ENGINEERING_SPEC §8） ───────────────────────────────

INJECTIONS = {
    "radius-unrecorded": "按 5\" 重算但声明仍写 1\"（半径改大而不记录）",
    "radius-recorded": "声明半径改成 5\"（记录了但与冻结口径不符）",
    "median-drift": "median 直接乘 1.14（篡改统计量）",
    "count-drift": "n_matched 加 1（篡改匹配数）",
    "flavor-swap": "把 B 的口径标成另一个（solved ↔ header 混用）",
    "residuals-truncated": "截断残差向量（证据不足以支撑声明的匹配数）",
    "drop-params": "删除 params（旧口径记录，无冻结参数）",
}


def inject(rec: dict, mode: str) -> dict:
    out = json.loads(json.dumps(rec))
    if mode == "radius-unrecorded":
        # 用 5" 重算全部指标，但 params 仍声明 1" ⇒ C2 必红
        tmp = json.loads(json.dumps(rec))
        tmp["params"]["match_radius_arcsec"] = 5.0
        out["metric"] = derive_metric(tmp)
        out["params"]["match_radius_arcsec"] = 1.0
    elif mode == "radius-recorded":
        out["params"]["match_radius_arcsec"] = 5.0
        out["metric"] = derive_metric(out)
    elif mode == "median-drift":
        for k in ("median_arcsec", "median_px", "p95_arcsec"):
            out["metric"][k] = float(out["metric"][k]) * 1.14
    elif mode == "count-drift":
        out["metric"]["n_matched"] = int(out["metric"]["n_matched"]) + 1
    elif mode == "flavor-swap":
        out["wcs_flavor"] = ("frame_header" if out["wcs_flavor"] == "solved_cd_sip"
                             else "solved_cd_sip")
    elif mode == "residuals-truncated":
        out["residuals_arcsec"] = out["residuals_arcsec"][:max(1, len(out["residuals_arcsec"]) // 2)]
    elif mode == "drop-params":
        out.pop("params", None)
    else:
        raise Red("未知注入模式: %s" % mode)
    return out


# ── 合成场自检（确定性；无外部数据、无生产代码） ───────────────────────────

def _synthetic_workspace(root: Path, seed: int = 20260917):
    from astropy.io import fits
    from astropy.wcs import WCS, Sip

    rng = np.random.default_rng(seed)
    W = H = 512
    s0_deg = 1.0 / 3600.0                      # 1.0 "/px
    crval = (120.0, 30.0)
    crpix = (W / 2 + 0.5, H / 2 + 0.5)
    ang = math.radians(20.0)
    cd = [[s0_deg * math.cos(ang), -s0_deg * math.sin(ang)],
          [s0_deg * math.sin(ang), s0_deg * math.cos(ang)]]
    order = 2
    n = order + 1
    a = np.zeros((n, n)); b = np.zeros((n, n))
    a[1, 1] = 2.0e-6; a[2, 0] = 1.0e-7; b[1, 1] = -1.5e-6; b[0, 2] = 8.0e-8
    ap = -a.copy(); bp = -b.copy()             # 一阶近似逆（前向不使用）

    def mk_wcs():
        w = WCS(naxis=2)
        w.wcs.crpix = list(crpix); w.wcs.crval = list(crval)
        w.wcs.cd = [list(cd[0]), list(cd[1])]
        w.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
        w.sip = Sip(a, b, ap, bp, w.wcs.crpix)
        return w

    w_solved = mk_wcs()
    w_shift = mk_wcs()                          # header 口径：CRVAL 偏 0.3"
    w_shift.wcs.crval = [crval[0] + 0.3 / 3600.0, crval[1]]

    # 真星：星表侧（sky）与检出侧（pixel + 噪声）
    nstar = 400
    truth_pix = np.column_stack([rng.uniform(20, W - 20, nstar),
                                 rng.uniform(20, H - 20, nstar)])
    truth_sky = w_solved.all_pix2world(truth_pix, 0)
    mag = rng.uniform(12.0, 19.5, nstar)

    srcs = []
    for i in range(nstar):
        if mag[i] >= 18.0:
            continue                             # 星表 mag<18 之外 ⇒ 不应被匹配
        # 检出噪声：0.25 px 高斯 + 20% 强扰（错配长尾来源）
        sig = 0.25 if i % 5 else 1.5
        x, y = truth_pix[i] + rng.normal(0, sig, 2)
        srcs.append({"id": "src-%d" % i, "x": float(x), "y": float(y),
                     "flux": float(1e5 / (1 + i)), "snr": 120.0, "quality": 0})
    # 掺入无对应体的假源：snr 低（被 SNR>20 剔除）与 snr 高（制造错配长尾）
    for k in range(120):
        x, y = rng.uniform(5, W - 5, 2)
        srcs.append({"id": "noise-%d" % k, "x": float(x), "y": float(y),
                     "flux": 1.0, "snr": 5.0 if k % 2 else 400.0, "quality": 0})
    (root / "p1_sources.json").write_text(json.dumps(
        {"frames": [{"file": "synthetic.fts", "n_detected": len(srcs),
                     "noise_sigma": 10.0, "sources": srcs}]}), encoding="utf-8")

    wj = {"rms_arcsec": 0.2, "rms_px": 0.2, "n_pairs": 50, "wcs_source": "ipv",
          "wcs": {"crpix1": crpix[0], "crpix2": crpix[1], "crval1": crval[0],
                  "crval2": crval[1], "cd11": cd[0][0], "cd12": cd[0][1],
                  "cd21": cd[1][0], "cd22": cd[1][1],
                  "ctype1": "RA---TAN-SIP", "ctype2": "DEC--TAN-SIP",
                  "sip": {"order": order, "ap_order": order,
                          "a": a.ravel().tolist(), "b": b.ravel().tolist(),
                          "ap": ap.ravel().tolist(), "bp": bp.ravel().tolist()}}}
    (root / "p1_wcs.json").write_text(json.dumps(wj), encoding="utf-8")

    hdr = w_shift.to_header(relax=True)
    hdr["NAXIS"] = 2; hdr["NAXIS1"] = W; hdr["NAXIS2"] = H
    fits.PrimaryHDU(np.zeros((2, 2), np.float32), header=hdr).writeto(
        root / "calibrated_synthetic.fts", overwrite=True)

    np.savez(root / "catalog.npz", ra=truth_sky[:, 0], dec=truth_sky[:, 1], mag=mag)
    return root / "catalog.npz"


def selftest(verbose: bool = True) -> int:
    tmp = Path(tempfile.mkdtemp(prefix="closure_metric_selftest_"))
    bad = 0
    try:
        cat = _synthetic_workspace(tmp)
        recs = {}
        for flav in FLAVORS:
            a = compute_record(tmp, cat, flav, FROZEN_PARAMS)
            b = compute_record(tmp, cat, flav, FROZEN_PARAMS)
            ok, lines = check_records(a, b)
            print("  [%s] 合成场同输入两跑一致 (%s): median=%.4f px n_matched=%d"
                  % ("PASS" if ok else "FAIL", flav,
                     a["metric"]["median_px"], a["metric"]["n_matched"]))
            if not ok:
                bad += 1
                print("        " + "; ".join(lines))
            recs[flav] = a
        # 样本筛选自检：SNR>20 必须真的剔除低 SNR 假源
        n_snr_fail = sum(1 for s in json.loads((tmp / "p1_sources.json").read_text())
                         ["frames"][0]["sources"] if s["snr"] <= 20.0)
        if n_snr_fail == 0 or recs["solved_cd_sip"]["n_det_snr_pass"] >= \
                recs["solved_cd_sip"]["n_detected_total"]:
            bad += 1
            print("  [FAIL] SNR 筛选无效（应剔除低 SNR 检出星）")
        else:
            print("  [PASS] SNR>20 筛选生效：%d/%d 剔除"
                  % (recs["solved_cd_sip"]["n_detected_total"]
                     - recs["solved_cd_sip"]["n_det_snr_pass"],
                     recs["solved_cd_sip"]["n_detected_total"]))
        # header 口径必须与 solved 分开且确实不同（合成场 CRVAL 偏 0.3"）
        if abs(recs["solved_cd_sip"]["metric"]["median_px"]
               - recs["frame_header"]["metric"]["median_px"]) < 1e-6:
            bad += 1
            print("  [FAIL] 两种 WCS 口径在合成场未分离（口径必须分别报告）")
        else:
            print("  [PASS] solved 与 header 口径分别报告且数值可区分")

        base = recs["solved_cd_sip"]
        other = recs["frame_header"]
        for mode in INJECTIONS:
            try:
                if mode == "flavor-swap":
                    detected = not check_records(base, inject(other, mode))[0]
                else:
                    detected = not check_records(inject(base, mode), base)[0]
            except Red:
                detected = True
            print("  [%s] 注入必红: %-20s %s" % ("PASS" if detected else "FAIL", mode,
                                                INJECTIONS[mode]))
            if not detected:
                bad += 1
        # 缺文件 / 空输入 ⇒ fail-closed 必红
        for name, fn in (("缺 p1_sources.json", lambda: compute_record(
                             tmp / "nope", cat, "solved_cd_sip", FROZEN_PARAMS)),
                         ("缺星表", lambda: compute_record(
                             tmp, tmp / "nope.npz", "solved_cd_sip", FROZEN_PARAMS))):
            try:
                fn(); detected = False
            except Red:
                detected = True
            print("  [%s] fail-closed 必红: %s" % ("PASS" if detected else "FAIL", name))
            if not detected:
                bad += 1
        print("SELFTEST %s (%d 未检出)" % ("PASS" if bad == 0 else "FAIL", bad))
        return 0 if bad == 0 else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ── CLI ──────────────────────────────────────────────────────────────────

def main() -> int:
    ap = argparse.ArgumentParser(description="G-P1-WCS-CLOSURE v1 冻结口径门")
    sub = ap.add_subparsers(dest="cmd", required=True)

    pc = sub.add_parser("compute")
    pc.add_argument("--product", required=True)
    pc.add_argument("--catalog", required=True, help="锥星表 npz (ra/dec/mag, deg)")
    pc.add_argument("--flavor", default="both", choices=("solved", "header", "both"))
    pc.add_argument("--out", required=True, help="输出目录（每口径一个 JSON）")
    pc.add_argument("--tag", default="")

    pk = sub.add_parser("check")
    pk.add_argument("records", nargs=2)
    pk.add_argument("--median-tol-px", type=float, default=0.0)
    pk.add_argument("--json-out", default=None)

    pi = sub.add_parser("fault-inject")
    pi.add_argument("record")
    pi.add_argument("--mode", required=True, choices=sorted(INJECTIONS))
    pi.add_argument("--out", required=True)

    sub.add_parser("selftest")
    args = ap.parse_args()

    if args.cmd == "selftest":
        return selftest()

    if args.cmd == "compute":
        product = Path(args.product).resolve()
        cat = Path(args.catalog).resolve()
        outdir = Path(args.out).resolve()
        outdir.mkdir(parents=True, exist_ok=True)
        flavs = FLAVORS if args.flavor == "both" else (
            ("solved_cd_sip",) if args.flavor == "solved" else ("frame_header",))
        try:
            for flav in flavs:
                rec = compute_record(product, cat, flav, FROZEN_PARAMS)
                name = "closure_%s%s.json" % (flav, ("_" + args.tag) if args.tag else "")
                p = outdir / name
                p.write_text(json.dumps(rec, indent=2, ensure_ascii=False), encoding="utf-8")
                m = rec["metric"]
                print("RECORD %s → %s" % (flav, p))
                print("  n_det=%d n_snr_pass=%d n_sample=%d capped=%s n_gaia=%d s0=%.4f\"/px"
                      % (rec["n_detected_total"], rec["n_det_snr_pass"], rec["n_sample"],
                         rec["sample_capped"], rec["n_gaia_cone"], rec["s0_arcsec_per_px"]))
                print("  matched=%d match_rate=%.4f median=%.4f\"=%.4f px p95=%.4f\" max=%.4f\""
                      % (m["n_matched"], m["match_rate"], m["median_arcsec"], m["median_px"],
                         m["p95_arcsec"], m["max_arcsec"]))
            return 0
        except Red as e:
            print("CLOSURE_METRIC_RED: %s" % e)
            return 1

    if args.cmd == "check":
        try:
            a = json.loads(Path(args.records[0]).read_text(encoding="utf-8"))
            b = json.loads(Path(args.records[1]).read_text(encoding="utf-8"))
        except OSError as e:
            print("CLOSURE_METRIC_RED: 记录不可读 %s（fail-closed）" % e)
            return 1
        except json.JSONDecodeError as e:
            print("CLOSURE_METRIC_RED: 记录不可解析 %s（fail-closed）" % e)
            return 1
        ok, lines = check_records(a, b, args.median_tol_px)
        for ln in lines:
            print(("PASS " if ok else "FAIL ") + ln)
        print("CLOSURE_REPRO_GATE %s (A=%s flavor=%s, B=%s flavor=%s)"
              % ("PASS" if ok else "FAIL", Path(args.records[0]).name, a.get("wcs_flavor"),
                 Path(args.records[1]).name, b.get("wcs_flavor")))
        if args.json_out:
            Path(args.json_out).write_text(json.dumps(
                {"verdict": "PASS" if ok else "FAIL", "lines": lines,
                 "a": args.records[0], "b": args.records[1],
                 "median_tol_px": args.median_tol_px}, indent=2, ensure_ascii=False),
                encoding="utf-8")
        return 0 if ok else 1

    if args.cmd == "fault-inject":
        rec = json.loads(Path(args.record).read_text(encoding="utf-8"))
        out = inject(rec, args.mode)
        Path(args.out).write_text(json.dumps(out, indent=2, ensure_ascii=False), encoding="utf-8")
        print("INJECTED %s → %s" % (args.mode, args.out))
        return 0

    return 2


if __name__ == "__main__":
    sys.exit(main())
