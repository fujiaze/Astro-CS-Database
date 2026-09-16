# -*- coding: utf-8 -*-
"""
Gate 4 (Phase1 Full Freeze v2): GaiaXPy 官方 Oracle 样本下载

步骤:
  1. Gaia DR3 TAP 查询固定样本 (>=1000 星, G in [8,15], 银心 T4 天区)
  2. ESA Datalink 批量下载 XP_CONTINUOUS 连续系数 (CSV)
  3. 缓存到本地 CSV (含 source_id/ra/dec/photometry/BP/RP 系数)

用法 (规范 Python 3.12):
  py -3.12 download_xp_sample.py --out <csv> [--n 1100] [--mag-max 15.0]

输出: 标准 GaiaXPy 输入格式 CSV (source_id, solution_id, bp_coefficients,
      bp_coefficient_errors, rp_coefficients, rp_coefficient_errors,
      bp_n_terms, rp_n_terms, phot_g_mean_mag, phot_bp_mean_mag,
      phot_rp_mean_mag, ra, dec)
"""

import argparse
import csv
import io
import os
import sys
import time
import urllib.parse
import urllib.request
import zipfile

import numpy as np
import pyvo
from astropy.io.votable import parse as votable_parse

TAP_URL = "https://gea.esac.esa.int/tap-server/tap"
DATALINK_URL = "https://gea.esac.esa.int/data-server/datalink/links"
UA = {"User-Agent": "AstroCS-Gate4-Oracle/1.0 (Phase1 v2)"}


def fetch_url(url, timeout=90):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def query_gaia_sources(n, ra0, dec0, radius_deg, mag_min, mag_max):
    """TAP 查询 source_id + 测光 (固定天区)."""
    svc = pyvo.dal.TAPService(TAP_URL)
    q = (
        f"SELECT TOP {n} source_id, ra, dec, phot_g_mean_mag, "
        "phot_bp_mean_mag, phot_rp_mean_mag "
        "FROM gaiadr3.gaia_source "
        f"WHERE phot_g_mean_mag BETWEEN {mag_min} AND {mag_max} "
        "AND 1=CONTAINS(POINT('ICRS',ra,dec),"
        f"CIRCLE('ICRS',{ra0},{dec0},{radius_deg}))"
    )
    last_err = None
    for attempt in range(5):
        try:
            tab = svc.search(q, maxrec=n).to_table()
            return tab
        except Exception as e:  # noqa: BLE001
            last_err = e
            print(f"[download_xp] TAP 查询重试 {attempt + 1}/5: {type(e).__name__} {str(e)[:120]}")
            time.sleep(2.0 * (attempt + 1))
    raise RuntimeError(f"TAP 查询失败: {last_err}")


def fmt_array(a):
    """GaiaXPy 系数列格式: '[c1, c2, ...]' (空格分隔)."""
    vals = ", ".join(f"{float(x):.10e}" for x in np.asarray(a).ravel())
    return "[" + vals + "]"


def fetch_xp_rows(source_ids):
    """批量请求 XP_CONTINUOUS (data 端点, ZIP 内含每源一个 VOTable),
    返回每源的系数行 dict 列表."""
    ids = ",".join("Gaia+DR3+" + str(s) for s in source_ids)
    url = (f"https://gea.esac.esa.int/data-server/data?ID={ids}"
           "&RETRIEVAL_TYPE=XP_CONTINUOUS&LINKING_PARAMETER=SOURCE_ID")
    data = fetch_url(url, timeout=180)
    if data[:2] != b"PK":
        raise RuntimeError("data 端点未返回 ZIP 包")
    z = zipfile.ZipFile(io.BytesIO(data))
    rows = []
    for name in z.namelist():
        if not name.lower().endswith(".xml"):
            continue
        raw = z.read(name)
        tab = votable_parse(io.BytesIO(raw)).get_first_table().to_table()
        if len(tab) != 1:
            continue
        r = tab[0]
        rows.append({
            "source_id": int(r["source_id"]),
            "solution_id": int(r["solution_id"]),
            "bp_basis_function_id": int(r["bp_basis_function_id"]),
            "bp_degrees_of_freedom": int(r["bp_degrees_of_freedom"]),
            "bp_n_parameters": int(r["bp_n_parameters"]),
            "bp_n_measurements": int(r["bp_n_measurements"]),
            "bp_n_rejected_measurements": int(r["bp_n_rejected_measurements"]),
            "bp_standard_deviation": float(r["bp_standard_deviation"]),
            "bp_chi_squared": float(r["bp_chi_squared"]),
            "bp_coefficients": fmt_array(r["bp_coefficients"]),
            "bp_coefficient_errors": fmt_array(r["bp_coefficient_errors"]),
            "bp_coefficient_correlations": fmt_array(r["bp_coefficient_correlations"]),
            "bp_n_relevant_bases": int(r["bp_n_relevant_bases"]),
            "bp_relative_shrinking": float(r["bp_relative_shrinking"]),
            "rp_basis_function_id": int(r["rp_basis_function_id"]),
            "rp_degrees_of_freedom": int(r["rp_degrees_of_freedom"]),
            "rp_n_parameters": int(r["rp_n_parameters"]),
            "rp_n_measurements": int(r["rp_n_measurements"]),
            "rp_n_rejected_measurements": int(r["rp_n_rejected_measurements"]),
            "rp_standard_deviation": float(r["rp_standard_deviation"]),
            "rp_chi_squared": float(r["rp_chi_squared"]),
            "rp_coefficients": fmt_array(r["rp_coefficients"]),
            "rp_coefficient_errors": fmt_array(r["rp_coefficient_errors"]),
            "rp_coefficient_correlations": fmt_array(r["rp_coefficient_correlations"]),
            "rp_n_relevant_bases": int(r["rp_n_relevant_bases"]),
            "rp_relative_shrinking": float(r["rp_relative_shrinking"]),
        })
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--n", type=int, default=1100)
    ap.add_argument("--mag-min", type=float, default=8.0)
    ap.add_argument("--mag-max", type=float, default=15.0)
    ap.add_argument("--ra0", type=float, default=272.886)
    ap.add_argument("--dec0", type=float, default=-23.254)
    ap.add_argument("--radius", type=float, default=0.35)
    ap.add_argument("--batch", type=int, default=50)
    args = ap.parse_args()

    tab = query_gaia_sources(args.n, args.ra0, args.dec0, args.radius,
                             args.mag_min, args.mag_max)
    n = len(tab)
    print(f"[download_xp] TAP 查询: {n} 颗星")
    if n == 0:
        sys.exit(1)
    src_ids = [int(s) for s in tab["source_id"]]

    chunks = []
    n_fail = 0
    for i in range(0, n, args.batch):
        batch = src_ids[i:i + args.batch]
        try:
            rows = fetch_xp_rows(batch)
            if rows:
                chunks.append(rows)
            print(f"[download_xp] batch {i // args.batch}: {len(rows)}/{len(batch)} OK")
        except Exception as e:  # noqa: BLE001
            n_fail += 1
            print(f"[download_xp] batch {i // args.batch} FAIL: {type(e).__name__} {str(e)[:160]}")
        time.sleep(0.3)

    if not chunks:
        print("[download_xp] 无任何批次成功, 退出")
        sys.exit(2)

    # 合并系数行 + TAP 测光
    photo = {int(s): (tab["phot_g_mean_mag"][k], tab["phot_bp_mean_mag"][k],
                      tab["phot_rp_mean_mag"][k], tab["ra"][k], tab["dec"][k])
             for k, s in enumerate(tab["source_id"])}
    uniq = {}
    for rows in chunks:
        for r in rows:
            sid = r["source_id"]
            if sid in uniq:
                continue
            g, bp, rp, ra, dec = photo.get(sid, (np.nan, np.nan, np.nan, np.nan, np.nan))
            r["phot_g_mean_mag"] = g
            r["phot_bp_mean_mag"] = bp
            r["phot_rp_mean_mag"] = rp
            r["ra"] = ra
            r["dec"] = dec
            uniq[sid] = r
    print(f"[download_xp] 合并后 {len(uniq)} 行, 失败批次 {n_fail}")
    cols = ["source_id", "solution_id", "bp_basis_function_id", "bp_degrees_of_freedom",
            "bp_n_parameters", "bp_n_measurements", "bp_n_rejected_measurements",
            "bp_standard_deviation", "bp_chi_squared", "bp_coefficients",
            "bp_coefficient_errors", "bp_coefficient_correlations",
            "bp_n_relevant_bases", "bp_relative_shrinking",
            "rp_basis_function_id", "rp_degrees_of_freedom",
            "rp_n_parameters", "rp_n_measurements", "rp_n_rejected_measurements",
            "rp_standard_deviation", "rp_chi_squared", "rp_coefficients",
            "rp_coefficient_errors", "rp_coefficient_correlations",
            "rp_n_relevant_bases", "rp_relative_shrinking",
            "phot_g_mean_mag", "phot_bp_mean_mag", "phot_rp_mean_mag", "ra", "dec"]
    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    with open(args.out, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f, quoting=csv.QUOTE_MINIMAL, lineterminator="\n")
        writer.writerow(cols)
        for sid in sorted(uniq):
            r = uniq[sid]
            writer.writerow([r[c] for c in cols])
    print(f"[download_xp] 已保存: {args.out}")


if __name__ == "__main__":
    main()
