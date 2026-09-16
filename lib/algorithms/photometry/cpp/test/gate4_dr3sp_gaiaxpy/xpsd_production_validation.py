# -*- coding: utf-8 -*-
"""
Phase B (Phase1 Final Closure V3): 真实 XPSD / DR3SP 生产闭环 (官方解码)

背景: 旧验证发现 "uint8 byte 不线性表示绝对 SED" (蓝端残差大)。
      根因: XPSD 记录内光谱是每星线性量化, 解码必须使用记录内的
      fluxMin / fluxMul (PCL GaiaDatabaseFile::EncodedStarSPData):

          F(lambda) = byte * fluxMul + fluxMin     (W*m^-2*nm^-1)

      不能脱离 fluxMin/fluxMul 单独用 byte (byte 只是 [fluxMin, fluxMax]
      区间内的 8-bit 线性量化值)。

本脚本:
  1. 用生产 gaia_client (gaia_client.dll, 已扩展 flux_min/flux_mul 输出)
     在真实 XPSD 文件查询 >=1000 颗 GaiaXPy 样本 (匹配 <=0.5\", |dG|<=0.03);
  2. 官方线性解码 F = byte*fluxMul + fluxMin;
  3. 与 GaiaXPy 官方绝对 XP sampled spectrum 逐点对比 (形状残差);
  4. 合成测光 (G/BP/RP, Riello+2021 passband) 颜色与自定通带对比;
  5. 输出 Gate 结果 (matched/颜色/自定通带)。

用法:
  py -3.12 xpsd_production_validation.py --csv <sample.csv> --out <dir> [--n 1000]
"""

import argparse
import ctypes
import json
import os
import sys

import numpy as np
import pandas as pd

ROOT = r"F:\Astro dev\Astro CS Normalization Database"
TEST_DIR = os.path.dirname(os.path.abspath(__file__))


def add_dll_dirs():
    for d in (ROOT + r"\lib\photometric_calib\cpp",
              ROOT + r"\lib\gaia_xpsd_client",
              r"C:\msys64\mingw64\bin", r"C:\msys64\mingw64\lib"):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")


class GaiaSpectrumStar(ctypes.Structure):
    _fields_ = [("ra", ctypes.c_double),
                ("dec", ctypes.c_double),
                ("magG", ctypes.c_double),
                ("flux_min", ctypes.c_float),
                ("flux_mul", ctypes.c_float)]


def query_xpsd(gaia, handle, ra, dec, radius_deg=0.0005, mag_lo=-5.0, mag_hi=20.0):
    stars = ctypes.POINTER(GaiaSpectrumStar)()
    spec = ctypes.POINTER(ctypes.c_uint8)()
    n = ctypes.c_int(0)
    gaia.gaia_client_cone_search_with_spectrum.restype = ctypes.c_int
    gaia.gaia_client_cone_search_with_spectrum.argtypes = [
        ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
        ctypes.c_double, ctypes.c_double,
        ctypes.POINTER(ctypes.POINTER(GaiaSpectrumStar)),
        ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
        ctypes.POINTER(ctypes.c_int)]
    rc = gaia.gaia_client_cone_search_with_spectrum(
        handle, ra, dec, radius_deg, mag_lo, mag_hi,
        ctypes.byref(stars), ctypes.byref(spec), ctypes.byref(n))
    if rc != 0 or n.value <= 0:
        return None
    arr = np.ctypeslib.as_array(stars, shape=(n.value,)).copy()
    n_spec = 343
    spectra = np.ctypeslib.as_array(spec, shape=(n.value * n_spec,)).copy().reshape(n.value, n_spec)
    return arr, spectra


def load_passbands(path):
    """解析 Gaia EDR3/DR3 passband.dat (Riello+2021).
    列: wl, G, G_err, BP, BP_err, RP, RP_err (99.99 = 带外哨兵)"""
    arr = np.loadtxt(path)
    out = {}
    for band, col in (("G", 1), ("BP", 3), ("RP", 5)):
        tr = arr[:, col].copy()
        tr[(tr >= 99.0) | (tr <= 0.0)] = 0.0
        out[band] = (arr[:, 0], tr)
    return out


def synthetic_band_flux(flux, wl_grid, pb_wl, pb_tr):
    """对网格光谱求合成通量 ∫ F(λ)·T(λ)·λ dλ (等间距 Simpson 近似,
    与生产 integrator 的 λ 加权一致; 仅用于方法间相对比较, 零点自行定义)."""
    flux = np.asarray(flux, dtype=float)
    tr = np.interp(wl_grid, pb_wl, pb_tr, left=0.0, right=0.0)
    y = flux * tr * wl_grid
    return float(np.trapz(y, wl_grid))


def band_mag(f):
    return -2.5 * np.log10(f) if f > 0 else np.nan


def color_stats(diff):
    diff = np.asarray(diff, dtype=float)
    diff = diff[np.isfinite(diff)]
    if len(diff) == 0:
        return {"n": 0}
    return {"n": int(len(diff)),
            "median": float(np.median(np.abs(diff))),
            "mean": float(np.mean(diff)),
            "p95": float(np.percentile(np.abs(diff), 95)),
            "max": float(np.max(np.abs(diff)))}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--n", type=int, default=0)
    args = ap.parse_args()
    add_dll_dirs()

    df = pd.read_csv(args.csv)
    if args.n > 0:
        df = df.head(args.n)
    print(f"[xpsd] 样本 {len(df)} 颗")

    gaia = ctypes.CDLL(ROOT + r"\lib\photometric_calib\cpp\gaia_client.dll")
    gaia.gaia_client_create_ex.restype = ctypes.c_void_p
    gaia.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
    handle = gaia.gaia_client_create_ex((ROOT + r"\GaiaDR3SP").encode(), 2)
    if not handle:
        print("FATAL: gaia_client 初始化失败")
        sys.exit(2)

    from gaiaxpy import calibrate
    XPSD_WL = 336.0 + 2.0 * np.arange(343, dtype=float)
    cal = calibrate(args.csv, sampling=XPSD_WL, save_file=False)
    caldf = cal[0].set_index("source_id")
    caldf.index = caldf.index.astype(np.int64)

    passbands = load_passbands(os.path.join(TEST_DIR, "GaiaEDR3_passband.dat"))

    rows = []
    n_match = 0
    n_mag_fail = 0
    n_fit = 0
    resid_list = []
    d_bp_g = []
    d_g_rp = []
    d_g_mag = []
    d_bp_mag = []
    d_rp_mag = []

    for _, r in df.iterrows():
        sid = int(r["source_id"])
        ra, dec = float(r["ra"]), float(r["dec"])
        g_gaia = float(r["phot_g_mean_mag"])
        bp_gaia = float(r["phot_bp_mean_mag"])
        rp_gaia = float(r["phot_rp_mean_mag"])
        q = query_xpsd(gaia, handle, ra, dec)
        if q is None:
            continue
        stars, spectra = q
        best = None
        for i in range(len(stars)):
            sep = np.degrees(np.arccos(np.clip(
                np.sin(dec * np.pi / 180) * np.sin(stars[i]["dec"] * np.pi / 180) +
                np.cos(dec * np.pi / 180) * np.cos(stars[i]["dec"] * np.pi / 180) *
                np.cos((ra - stars[i]["ra"]) * np.pi / 180), -1, 1))) * 3600.0
            if sep <= 0.5:
                best = (i, sep, float(stars[i]["magG"]))
                break
        if best is None:
            continue
        i, sep_arcsec, g_xpsd = best
        if abs(g_xpsd - g_gaia) > 0.03:
            n_mag_fail += 1
            continue
        n_match += 1
        if sid not in caldf.index:
            continue
        sub = caldf.loc[sid]
        xp_abs = np.asarray(sub.iloc[0]["flux"] if isinstance(sub, pd.DataFrame) else sub["flux"],
                            dtype=float)
        if len(xp_abs) != 343:
            continue

        byte = spectra[i].astype(float)
        flux_min = float(stars[i]["flux_min"])
        flux_mul = float(stars[i]["flux_mul"])
        # 官方线性解码 (PCL GaiaDatabaseFile::EncodedStarSPData)
        if not (flux_mul > 0.0) or not np.isfinite(flux_min) or not np.isfinite(flux_mul):
            continue
        flux_dec = byte * flux_mul + flux_min

        # 逐点相对残差 (以 GaiaXPy 官方绝对光谱为真值)
        denom = np.maximum(np.abs(xp_abs), 1e-300)
        rel = np.abs(flux_dec - xp_abs) / denom
        rel = rel[np.isfinite(rel)]
        if len(rel) == 0:
            continue
        n_fit += 1
        resid_list.append(float(np.median(rel)))

        # 合成测光: 解码光谱 vs GaiaXPy 官方光谱 (同一 passband 积分)
        f_g_dec = synthetic_band_flux(flux_dec, XPSD_WL, *passbands["G"])
        f_bp_dec = synthetic_band_flux(flux_dec, XPSD_WL, *passbands["BP"])
        f_rp_dec = synthetic_band_flux(flux_dec, XPSD_WL, *passbands["RP"])
        f_g_xp = synthetic_band_flux(xp_abs, XPSD_WL, *passbands["G"])
        f_bp_xp = synthetic_band_flux(xp_abs, XPSD_WL, *passbands["BP"])
        f_rp_xp = synthetic_band_flux(xp_abs, XPSD_WL, *passbands["RP"])
        d_bp_g.append((band_mag(f_bp_dec) - band_mag(f_g_dec)) -
                      (band_mag(f_bp_xp) - band_mag(f_g_xp)))
        d_g_rp.append((band_mag(f_g_dec) - band_mag(f_rp_dec)) -
                      (band_mag(f_g_xp) - band_mag(f_rp_xp)))
        d_g_mag.append(band_mag(f_g_dec) - band_mag(f_g_xp))
        d_bp_mag.append(band_mag(f_bp_dec) - band_mag(f_bp_xp))
        d_rp_mag.append(band_mag(f_rp_dec) - band_mag(f_rp_xp))

        rows.append({"source_id": sid, "ra": ra, "dec": dec,
                     "g_gaia": g_gaia, "g_xpsd": g_xpsd,
                     "sep_arcsec": sep_arcsec,
                     "flux_min": flux_min, "flux_mul": flux_mul,
                     "rel_median": float(np.median(rel)),
                     "xp_sum": float(xp_abs.sum()),
                     "dec_sum": float(flux_dec.sum()),
                     "d_bp_g": d_bp_g[-1], "d_g_rp": d_g_rp[-1],
                     "d_g": d_g_mag[-1], "d_bp": d_bp_mag[-1], "d_rp": d_rp_mag[-1]})

    print(f"[xpsd] 匹配 {n_match} 颗 (mag_fail={n_mag_fail}), 可拟合 {n_fit} 颗")
    if n_fit == 0:
        print("[xpsd] 无可拟合样本")
        sys.exit(3)

    resid_arr = np.array(resid_list)
    stats = {
        "n_sample": int(len(df)),
        "n_matched": n_match,
        "n_mag_fail": n_mag_fail,
        "n_fit": n_fit,
        "rel_median": float(np.median(resid_arr)),
        "rel_p95": float(np.percentile(resid_arr, 95)),
        "d_bp_g": color_stats(d_bp_g),
        "d_g_rp": color_stats(d_g_rp),
        "d_g": color_stats(d_g_mag),
        "d_bp": color_stats(d_bp_mag),
        "d_rp": color_stats(d_rp_mag),
    }
    gates = {
        "matched_ge_900": n_match >= 900,
        "bp_g_median_le_0.005": stats["d_bp_g"].get("median", 9e9) <= 0.005,
        "bp_g_p95_le_0.02": stats["d_bp_g"].get("p95", 9e9) <= 0.02,
        "g_rp_median_le_0.005": stats["d_g_rp"].get("median", 9e9) <= 0.005,
        "g_rp_p95_le_0.02": stats["d_g_rp"].get("p95", 9e9) <= 0.02,
        "custom_G_median_le_0.005": stats["d_g"].get("median", 9e9) <= 0.005,
        "custom_G_p95_le_0.02": stats["d_g"].get("p95", 9e9) <= 0.02,
    }
    stats["gates"] = gates
    stats["all_pass"] = all(gates.values())

    print(f"[xpsd] 形状残差 median={np.median(resid_arr):.5f} p95={np.percentile(resid_arr,95):.5f}")
    print(f"[xpsd] d(BP-G) {stats['d_bp_g']}")
    print(f"[xpsd] d(G-RP) {stats['d_g_rp']}")
    print(f"[xpsd] d(G)    {stats['d_g']}")
    print(f"[xpsd] GATES: {json.dumps(gates, ensure_ascii=False)}")

    os.makedirs(args.out, exist_ok=True)
    pd.DataFrame(rows).to_csv(os.path.join(args.out, "xpsd_match.csv"), index=False)
    with open(os.path.join(args.out, "xpsd_result.json"), "w", encoding="utf-8") as f:
        json.dump(stats, f, ensure_ascii=False, indent=2)
    print(f"[xpsd] DONE -> {args.out}")
    sys.exit(0 if stats["all_pass"] else 4)


if __name__ == "__main__":
    main()
