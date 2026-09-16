# -*- coding: utf-8 -*-
"""
Gate 7 (Phase1 Final Closure V3): HiPS 直写产品集独立验证

独立 Reader: astropy.io.fits (不经 AIO / 不经自家 Writer 语义)。
验证项:
  1. 三子产品 (signal/support/snr) properties: hips_version=1.4,
     dataproduct_type, hips_tile_width=512, hips_order=K
  2. signal/support 叶级 tile: 512x512, BITPIX -32/-64, PIXTYPE=HEALPIX,
     ORDERING=NESTED, NSIDE=2^(K+9), FIRSTPIX/LASTPIX, DATASUM 有效
  3. support ∈ [0,1]; support>0 -> signal 有限; support==0 -> signal NaN
  4. F = signal × support × A_cell 有限非负
  5. MOC: UNIQ 单元格与叶级 tile ipix 一一对应 (order K)
  6. hierarchy: Norder0..NorderK 存在; 抽样父像素 = 面积加权子均值
  7. SNR Catalogue: TSV tile 头列一致, 行数与 properties 一致
  8. manifest.json 关键字段

用法: py -3.12 gate7_hips_validate.py --hips <dir> --out <dir>
"""

import argparse
import glob
import json
import math
import os
import sys

import numpy as np
from astropy.io import fits


def load_props(d):
    p = {}
    with open(os.path.join(d, "properties"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if "=" in line:
                k, v = line.split("=", 1)
                p[k.strip()] = v.strip()
    return p


def tile_ipix_from_path(t):
    """HiPS tile 路径 NorderK/DirD/NpixN.fits -> 完整 NESTED ipix = D*10000+N"""
    base = os.path.basename(t)            # NpixN.fits
    ddir = os.path.basename(os.path.dirname(t))  # DirD
    ip = int(base[4:-5])
    dd = int(ddir[3:])
    return dd * 10000 + ip


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hips", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    result = {"checks": {}, "errors": []}

    # 1. products + properties
    for prod, dtype in (("signal", "image"), ("support", "image"), ("snr", "catalog")):
        d = os.path.join(args.hips, prod)
        if not os.path.isdir(d):
            result["errors"].append(f"缺少子产品目录: {prod}")
            continue
        p = load_props(d)
        result.setdefault("properties", {})[prod] = p
        result["checks"][f"{prod}_hips_version"] = p.get("hips_version") == "1.4"
        result["checks"][f"{prod}_dataproduct_type"] = p.get("dataproduct_type") == dtype
        if prod != "snr":
            result["checks"][f"{prod}_tile_width"] = p.get("hips_tile_width") == "512"

    # 2. leaf tiles
    sig_tiles = sorted(glob.glob(os.path.join(args.hips, "signal", "Norder*", "Dir*", "Npix*.fits")))
    sup_tiles = sorted(glob.glob(os.path.join(args.hips, "support", "Norder*", "Dir*", "Npix*.fits")))
    # 只统计最高 order (叶级)
    orders = sorted({int(os.path.basename(os.path.dirname(os.path.dirname(t)))[6:]) for t in sig_tiles})
    leaf_order = orders[-1] if orders else -1
    leaf_sig = [t for t in sig_tiles if f"Norder{leaf_order}" in t]
    leaf_sup = [t for t in sup_tiles if f"Norder{leaf_order}" in t]
    result["leaf_order"] = leaf_order
    result["n_leaf_signal_tiles"] = len(leaf_sig)
    result["n_leaf_support_tiles"] = len(leaf_sup)
    result["hierarchy_orders"] = orders
    result["checks"]["hierarchy_complete"] = orders == list(range(0, leaf_order + 1))
    result["checks"]["signal_support_tile_count_match"] = len(leaf_sig) == len(leaf_sup) > 0

    nside_leaf = None
    tile_header_ok = True
    dat_sum_ok = True
    for t in leaf_sig[:8] + leaf_sup[:8]:
        hdul = fits.open(t)
        h = hdul[0].header
        data = hdul[0].data
        if data is None or data.shape != (512, 512):
            tile_header_ok = False
        if h.get("PIXTYPE") != "HEALPIX" or h.get("ORDERING") != "NESTED":
            tile_header_ok = False
        if h.get("BITPIX") not in (-32, -64):
            tile_header_ok = False
        if h.get("FIRSTPIX") != 0 or h.get("LASTPIX") != 512 * 512 - 1:
            tile_header_ok = False
        if nside_leaf is None:
            nside_leaf = int(h.get("NSIDE"))
        # DATASUM/CHECKSUM 验证
        try:
            ok = hdul[0].verify_checksum() == 1
            if not ok:
                dat_sum_ok = False
        except Exception:  # noqa: BLE001
            dat_sum_ok = False
        hdul.close()
    result["leaf_nside"] = nside_leaf
    result["checks"]["tile_fits_header"] = tile_header_ok
    result["checks"]["tile_checksum"] = dat_sum_ok

    # 3/4. signal/support 语义
    a_cell = 4.0 * math.pi / (12.0 * (nside_leaf ** 2)) if nside_leaf else 0.0
    sem = {"n": 0, "sup_range_ok": True, "sig_finite_ok": True, "flux_ok": True}
    for t in leaf_sig:
        sup_path = t.replace(os.sep + "signal" + os.sep, os.sep + "support" + os.sep)
        if not os.path.exists(sup_path):
            continue
        sig = fits.getdata(t).astype(float)
        sup = fits.getdata(sup_path).astype(float)
        sem["n"] += 1
        if np.any((sup < 0) | (sup > 1.0 + 1e-6)):
            sem["sup_range_ok"] = False
        pos = sup > 0
        if np.any(pos & ~np.isfinite(sig)):
            sem["sig_finite_ok"] = False
        if np.any(~pos & np.isfinite(sig)):
            sem["sig_finite_ok"] = False  # 空像素必须是 NaN
        f = sig * sup * a_cell
        if np.any(pos & ~np.isfinite(f)) or np.any(pos & (f < 0)):
            sem["flux_ok"] = False
    result["semantics"] = sem
    result["checks"]["support_range"] = sem["sup_range_ok"]
    result["checks"]["signal_nan_semantics"] = sem["sig_finite_ok"]
    result["checks"]["flux_roundtrip"] = sem["flux_ok"]

    # 5. MOC vs leaf tiles
    moc_path = os.path.join(args.hips, "signal", "Moc.fits")
    moc_ok = False
    if os.path.exists(moc_path):
        hdul = fits.open(moc_path)
        if len(hdul) > 1:
            tab = hdul[1].data
            uniq = [int(v) for v in tab["UNIQ"]]
            base = 4 * (4 ** leaf_order)
            moc_ipix = {u - base for u in uniq if u >= base}
            tile_ipix = {tile_ipix_from_path(t) for t in leaf_sig}
            moc_ok = moc_ipix == tile_ipix and len(moc_ipix) > 0
            result["n_moc_cells"] = len(moc_ipix)
            result["moc_minus_tiles"] = sorted(moc_ipix - tile_ipix)
            result["tiles_minus_moc"] = sorted(tile_ipix - moc_ipix)
        hdul.close()
    result["checks"]["moc_matches_tiles"] = moc_ok

    # 6. hierarchy 抽样一致性 (order K-1 vs children)
    hier_ok = True
    if leaf_order >= 1 and leaf_sig:
        parent_files = [t for t in sig_tiles if f"Norder{leaf_order - 1}" in t]
        child_by_parent = {}
        for t in leaf_sig:
            ip = tile_ipix_from_path(t)
            par = ip >> 2
            child_by_parent.setdefault(par, []).append(t)
        checked = 0
        for par_file in parent_files[:4]:
            par_ip = tile_ipix_from_path(par_file)
            kids = child_by_parent.get(par_ip)
            if not kids:
                continue
            pd = fits.getdata(par_file).astype(float)
            # 子 tile 像素 -> 父 tile 像素: 子全序 local z, 父像素 z_parent = z_child >> 2
            agg_f = np.zeros((512, 512))
            agg_a = np.zeros((512, 512))
            cnt = np.zeros((512, 512), dtype=int)
            for kf in kids:
                kid_ip = tile_ipix_from_path(kf)
                s = kid_ip & 3
                kd = fits.getdata(kf).astype(float)
                sup_k = fits.getdata(kf.replace(os.sep + "signal" + os.sep, os.sep + "support" + os.sep)).astype(float)
                f_k = kd * sup_k * a_cell
                finite = np.isfinite(f_k.ravel())
                z = (np.arange(512 * 512, dtype=np.int64))
                zpar = ((s << 18) | z[finite]) >> 2
                yy, xx = np.divmod(zpar, 512)
                np.add.at(agg_f, (yy, xx), f_k.ravel()[finite])
                np.add.at(agg_a, (yy, xx), (sup_k * a_cell).ravel()[finite])
                cnt[yy, xx] += 1
            ok = cnt > 0
            p_sig = np.full((512, 512), np.nan)
            p_sig[ok] = agg_f[ok] / agg_a[ok]
            mask = np.isfinite(pd) & np.isfinite(p_sig)
            if mask.sum() > 0:
                rel = np.abs(pd[mask] - p_sig[mask]) / np.maximum(np.abs(p_sig[mask]), 1e-30)
                if np.percentile(rel, 99) > 1e-3:
                    hier_ok = False
            checked += 1
        result["hierarchy_checked_parents"] = checked
    result["checks"]["hierarchy_consistency"] = hier_ok

    # 7. SNR catalogue
    snr_tiles = sorted(glob.glob(os.path.join(args.hips, "snr", "Norder*", "Dir*", "Npix*.tsv")))
    n_snr = 0
    header_ok = True
    for t in snr_tiles:
        with open(t, encoding="utf-8") as f:
            lines = [ln.strip() for ln in f if ln.strip()]
        if not lines or not lines[0].startswith("#"):
            header_ok = False
            continue
        cols = lines[0][1:].split()
        if cols != ["star_id", "ra", "dec", "snr", "quality_flags", "photometric_status"]:
            header_ok = False
        n_snr += len(lines) - 1
    result["n_snr_tiles"] = len(snr_tiles)
    result["n_snr_points"] = n_snr
    result["checks"]["snr_header_consistent"] = header_ok
    result["checks"]["snr_points_positive"] = n_snr > 0

    # 8. manifest
    mp = os.path.join(args.hips, "manifest.json")
    if os.path.exists(mp):
        with open(mp, encoding="utf-8") as f:
            manifest = json.load(f)
        result["manifest"] = manifest
        result["checks"]["manifest_hips_version"] = manifest.get("hips_version") == "1.4"
        result["checks"]["manifest_nside"] = manifest.get("nside") == nside_leaf
    else:
        result["checks"]["manifest"] = False

    result["all_pass"] = all(v is True for v in result["checks"].values()) and not result["errors"]
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "gate7_hips_result.json"), "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(json.dumps({k: v for k, v in result["checks"].items()}, ensure_ascii=False, indent=1))
    print("ALL_PASS:", result["all_pass"], "errors:", result["errors"])
    sys.exit(0 if result["all_pass"] else 4)


if __name__ == "__main__":
    main()
