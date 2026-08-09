# -*- coding: utf-8 -*-
"""V4 G4 actual-buffer trace 校验 (运行于 drizzle trace 输出目录).

输入: <diag>/trace/ 下的 trace_selection.json / pixel_lineage.jsonl /
      drizzle_lineage.jsonl / leaf_internal.jsonl / stage_trace.jsonl,
      以及 HiPS 产品根目录 (AIO Reader 对照 readback)。
输出: trace_g4_result.json (硬门结果) + star_lineage.jsonl + hips_readback.jsonl。

硬门 (FP32):
  - 源像素链样本 >= 512 (raw->calibrated->photometric 均有值)
  - drizzle 逐源贡献样本 >= 512, sum_contribution == Sigma(contribution)
  - drizzle effective value == photometric 阶段实际 buffer 值 (同像素)
  - leaf 样本 >= 8192, readback 重建通量相对误差目标 1e-5
  - SNR star_id 无 i+1 重编号 (SNR ids subset-of PSF ids)
用法: py -3.12 trace_g4_validate.py --trace <dir> --hips <root> --out <dir>
"""
import argparse
import json
import math
import os
import sys
from collections import defaultdict

import numpy as np


def add_dll_dirs():
    import os as _os
    for d in (r"F:\Astro dev\Astro CS Normalization Database\lib\astro_image_io",
              r"C:\msys64\mingw64\bin"):
        if _os.path.isdir(d):
            _os.add_dll_directory(d)
            _os.environ["PATH"] = d + _os.pathsep + _os.environ.get("PATH", "")


def load_jsonl(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                try:
                    rows.append(json.loads(line))
                except Exception:
                    pass
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace", required=True)
    ap.add_argument("--hips", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    result = {"checks": {}, "counts": {}}
    errors = []

    # 1. trace_selection
    sel = json.load(open(os.path.join(args.trace, "trace_selection.json"), encoding="utf-8"))
    pixels = [(p["x"], p["y"]) for p in sel["pixels"]]
    result["counts"]["selection_pixels"] = len(pixels)
    result["checks"]["selection_ge_512"] = len(pixels) >= 512

    # 2. pixel_lineage: 按 (x,y) -> stage -> value
    pl = load_jsonl(os.path.join(args.trace, "pixel_lineage.jsonl"))
    by_px = defaultdict(dict)
    for r in pl:
        by_px[(r["x"], r["y"])][r["stage"]] = r["value"]
    chain_ok = 0
    chain_bad = []
    photometric_values = {}
    for (x, y) in pixels:
        st = by_px.get((x, y), {})
        if all(k in st for k in ("READ_FITS", "CALIBRATE", "PHOTOMETRIC")):
            chain_ok += 1
            photometric_values[(x, y)] = st["PHOTOMETRIC"]
        else:
            chain_bad.append((x, y, sorted(st.keys())))
    result["counts"]["pixel_chain_samples"] = chain_ok
    result["checks"]["pixel_chain_ge_512"] = chain_ok >= 512
    if chain_bad:
        result["pixel_chain_missing"] = chain_bad[:10]

    # 3. drizzle_lineage
    dl = load_jsonl(os.path.join(args.trace, "drizzle_lineage.jsonl"))
    result["counts"]["drizzle_sources"] = len(dl)
    result["checks"]["drizzle_sources_ge_512"] = len(dl) >= 512
    sum_ok = 0
    eff_ok = 0
    eff_bad = []
    contrib_by_leaf = defaultdict(float)
    for r in dl:
        cs = sum(c["contribution"] for c in r["contribs"])
        if abs(cs - r["sum_contribution"]) <= 1e-6 * max(1.0, abs(r["sum_contribution"])):
            sum_ok += 1
        else:
            errors.append(f"sum_contribution mismatch @({r['x']},{r['y']})")
        for c in r["contribs"]:
            contrib_by_leaf[c["ipix"]] += c["contribution"]
        # effective value == photometric stage value (同像素)
        pv = photometric_values.get((round(r["x"]), round(r["y"])))
        if pv is not None:
            if abs(pv - r["value"]) <= 1e-6 * max(1.0, abs(pv)):
                eff_ok += 1
            else:
                eff_bad.append((r["x"], r["y"], pv, r["value"]))
    result["counts"]["sum_contribution_ok"] = sum_ok
    result["checks"]["sum_contribution_ok_all"] = sum_ok == len(dl) and len(dl) > 0
    result["counts"]["effective_value_ok"] = eff_ok
    result["checks"]["effective_value_matches_photometric"] = (
        eff_ok == len(dl) and len(dl) > 0)
    result["effective_value_bad"] = eff_bad[:5]

    # 4. leaf_internal -> readback via AIO (ctypes)
    li = load_jsonl(os.path.join(args.trace, "leaf_internal.jsonl"))
    result["counts"]["leaf_internal"] = len(li)
    result["checks"]["leaf_ge_8192"] = len(li) >= 8192

    import ctypes as C
    add_dll_dirs()
    aio = C.CDLL(r"F:\Astro dev\Astro CS Normalization Database\lib\astro_image_io\astro_image_io.dll")
    aio.aio_hips_open.restype = C.c_void_p
    aio.aio_hips_open.argtypes = [C.c_char_p, C.c_int]
    aio.aio_hips_read_tile_f32.restype = C.c_int
    aio.aio_hips_read_tile_f32.argtypes = [C.c_void_p, C.c_uint64, C.POINTER(C.c_float)]
    aio.aio_hips_read_tile_f64.restype = C.c_int
    aio.aio_hips_read_tile_f64.argtypes = [C.c_void_p, C.c_uint64, C.POINTER(C.c_double)]
    aio.aio_hips_close.argtypes = [C.c_void_p]
    hsig = aio.aio_hips_open(args.hips.encode(), 0)
    hsup = aio.aio_hips_open(args.hips.encode(), 1)
    if not hsig or not hsup:
        print("AIO open failed"); return 3

    # leaf order: signal properties
    props = {}
    with open(os.path.join(args.hips, "signal", "properties"), encoding="utf-8") as f:
        for line in f:
            if "=" in line:
                k, v = line.split("=", 1)
                props[k.strip()] = v.strip()
    order = int(props["hips_order"])
    leaf_nside = 1 << (order + 9)
    a_cell = 4.0 * math.pi / (12.0 * leaf_nside * leaf_nside)

    buf32 = (C.c_float * (512 * 512))()
    buf64 = (C.c_double * (512 * 512))()
    fp64 = props.get("astrocs_signal_dtype") == "float64"
    readback_rows = []
    flux_bad = 0
    area_bad = 0
    n_checked = 0
    cache = {}
    for r in li:
        ipix = r["ipix"]
        tile = ipix >> 18
        z = ipix & ((1 << 18) - 1)
        if tile not in cache:
            if fp64:
                if aio.aio_hips_read_tile_f64(hsig, tile, buf64) != 0:
                    cache[tile] = None
                else:
                    cache[tile] = list(buf64)
                sup64 = (C.c_double * (512 * 512))()
                aio.aio_hips_read_tile_f64(hsup, tile, sup64)
                cache_sup = list(sup64)
            else:
                if aio.aio_hips_read_tile_f32(hsig, tile, buf32) != 0:
                    cache[tile] = None
                    cache_sup = None
                else:
                    cache[tile] = list(buf32)
                aio.aio_hips_read_tile_f32(hsup, tile, buf32)
                cache_sup = list(buf32)
            cache[tile] = (cache[tile], cache_sup)
        entry = cache[tile]
        if entry is None or entry[0] is None:
            flux_bad += 1
            continue
        sig_arr, sup_arr = entry
        x = z % 512
        y = z // 512
        idx = y * 512 + x
        signal = sig_arr[idx]
        support = sup_arr[idx]
        if not math.isfinite(signal) or support <= 0:
            flux_bad += 1
            readback_rows.append({"ipix": ipix, "sumFlux": r["sumFlux"],
                                  "signal": signal, "support": support,
                                  "reconstructed_flux": None})
            continue
        flux_rb = signal * support * a_cell
        area_rb = support * a_cell
        n_checked += 1
        rel = abs(flux_rb - r["sumFlux"]) / max(1e-30, abs(r["sumFlux"]))
        rel_area = abs(area_rb - r["sumArea"]) / max(1e-30, abs(r["sumArea"]))
        if rel > 1e-5 and abs(flux_rb - r["sumFlux"]) > 1e-12:
            flux_bad += 1
        if rel_area > 1e-5 and abs(area_rb - r["sumArea"]) > 1e-12:
            area_bad += 1
        readback_rows.append({"ipix": ipix, "sumFlux": r["sumFlux"],
                              "sumArea": r["sumArea"],
                              "signal": signal, "support": support,
                              "reconstructed_flux": flux_rb,
                              "rel_err_flux": rel, "rel_err_area": rel_area})
    aio.aio_hips_close(hsig)
    aio.aio_hips_close(hsup)
    result["counts"]["readback_checked"] = n_checked
    result["counts"]["readback_flux_bad"] = flux_bad
    result["counts"]["readback_area_bad"] = area_bad
    result["checks"]["readback_flux_tol_1e-5"] = flux_bad == 0 and n_checked >= 8192
    result["checks"]["readback_area_tol_1e-5"] = area_bad == 0 and n_checked >= 8192
    with open(os.path.join(args.out, "hips_readback.jsonl"), "w", encoding="utf-8") as f:
        for r in readback_rows:
            f.write(json.dumps(r) + "\n")

    # 5. 贡献聚合 vs leaf sumFlux (跨文件守恒)
    leaf_flux_bad = 0
    for r in li[:20000]:
        s = contrib_by_leaf.get(r["ipix"], 0.0)
        if abs(s - r["sumFlux"]) > 1e-5 * max(1.0, abs(r["sumFlux"])) and \
           abs(s - r["sumFlux"]) > 1e-9:
            leaf_flux_bad += 1
    result["counts"]["leaf_flux_agg_bad"] = leaf_flux_bad
    result["checks"]["drizzle_contrib_conserves_leaf_flux"] = leaf_flux_bad == 0

    # 6. star lineage: PSF ids -> photometric ids -> SNR ids (从 stage_trace + snr TSV)
    st = load_jsonl(os.path.join(args.trace, "stage_trace.jsonl"))
    psf_ids = set()
    pm_ids = set()
    for r in st:
        if r.get("stage") == "PSF":
            psf_ids |= {int(x) for x in r.get("all_psf_star_ids", [])}
        if r.get("stage") == "PHOTOMETRIC":
            pm_ids |= {int(x) for x in r.get("all_photometric_star_ids", [])}
    snr_ids = set()
    import glob
    for tsv in glob.glob(os.path.join(args.hips, "snr", "Norder*", "Dir*", "Npix*.tsv")):
        with open(tsv, encoding="utf-8") as f:
            for line in f:
                p = line.split()
                if len(p) >= 6 and not line.startswith("#"):
                    snr_ids.add(int(p[0]))
    lineage = []
    for sid in sorted(psf_ids):
        lineage.append({"star_id": sid, "in_psf": True,
                        "in_photometric": sid in pm_ids,
                        "in_snr_catalog": sid in snr_ids})
    result["counts"]["psf_ids"] = len(psf_ids)
    result["counts"]["snr_ids"] = len(snr_ids)
    result["checks"]["snr_ids_subset_psf"] = snr_ids <= psf_ids
    result["checks"]["snr_ids_no_sequential_renumber"] = (
        not (len(snr_ids) > 0 and snr_ids == {i + 1 for i in range(len(snr_ids))}))
    with open(os.path.join(args.out, "star_lineage.jsonl"), "w", encoding="utf-8") as f:
        for r in lineage:
            f.write(json.dumps(r) + "\n")

    result["errors"] = errors[:20]
    ok = all(result["checks"].values())
    result["PASS"] = ok
    with open(os.path.join(args.out, "trace_g4_result.json"), "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
