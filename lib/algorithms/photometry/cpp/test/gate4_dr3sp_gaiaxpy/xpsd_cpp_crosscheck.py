# -*- coding: utf-8 -*-
"""
Phase B (Phase1 Final Closure V3): 生产 C++ 积分路径 vs GaiaXPy 官方 Oracle

证明:
  1. 生产 C++ compute_f_syn_cached_xpsd (官方线性解码 + 绝对单位积分)
     与 Python 同算法实现数值一致 (algorithm equivalence);
  2. 生产 C++ F_syn(解码 XPSD) 与 GaiaXPy 绝对光谱 F_syn 比值 ~1
     (production path vs official Oracle)。

用法:
  py -3.12 xpsd_cpp_crosscheck.py --csv <sample.csv> --out <dir> [--n 200]
"""

import argparse
import ctypes
import json
import os
import subprocess
import sys

import numpy as np
import pandas as pd

ROOT = r"F:\Astro dev\Astro CS Normalization Database"
TEST_DIR = os.path.dirname(os.path.abspath(__file__))
XPSD_WL = 336.0 + 2.0 * np.arange(343, dtype=float)

sys.path.insert(0, TEST_DIR)
from fsyn_astrocs import akima_interpolate, simpson_integrate  # noqa: E402


def add_dll_dirs():
    for d in (ROOT + r"\lib\photometric_calib\cpp",
              ROOT + r"\lib\gaia_xpsd_client",
              r"C:\msys64\mingw64\bin", r"C:\msys64\mingw64\lib"):
        if os.path.isdir(d):
            os.add_dll_directory(d)
            os.environ["PATH"] = d + os.pathsep + os.environ.get("PATH", "")


class GaiaSpectrumStar(ctypes.Structure):
    _fields_ = [("ra", ctypes.c_double), ("dec", ctypes.c_double), ("magG", ctypes.c_double),
                ("flux_min", ctypes.c_float), ("flux_mul", ctypes.c_float)]


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
    spectra = np.ctypeslib.as_array(spec, shape=(n.value * 343,)).copy().reshape(n.value, 343)
    return arr, spectra


def fsyn_cached_like(flux, wl, filter_wl, filter_trans):
    """复刻 C++ compute_f_syn_cached_xpsd: T Akima 重采样到光谱网格,
    被积函数 F*T*λ, 在 2nm 网格 Simpson 积分."""
    flux = np.asarray(flux, dtype=float)
    tr = np.asarray(akima_interpolate(filter_wl, filter_trans, wl, 0.0), dtype=float)
    y = flux * tr * wl
    return simpson_integrate(wl, y)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--n", type=int, default=200)
    args = ap.parse_args()
    add_dll_dirs()

    df = pd.read_csv(args.csv)
    if args.n > 0:
        df = df.head(args.n)

    gaia = ctypes.CDLL(ROOT + r"\lib\photometric_calib\cpp\gaia_client.dll")
    gaia.gaia_client_create_ex.restype = ctypes.c_void_p
    gaia.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
    handle = gaia.gaia_client_create_ex((ROOT + r"\GaiaDR3SP").encode(), 2)
    if not handle:
        print("FATAL: gaia_client 初始化失败")
        sys.exit(2)

    from gaiaxpy import calibrate
    cal = calibrate(args.csv, sampling=XPSD_WL, save_file=False)
    caldf = cal[0].set_index("source_id")
    caldf.index = caldf.index.astype(np.int64)

    # G passband (Riello+2021) 作为自定义通带
    pb = np.loadtxt(os.path.join(TEST_DIR, "GaiaEDR3_passband.dat"))
    fwl = pb[:, 0]
    ftr = pb[:, 1].copy()
    ftr[(ftr >= 99.0) | (ftr <= 0.0)] = 0.0

    # 写 filter 文件 (fsyn_export load_curve 格式)
    filt_path = os.path.join(args.out, "G_passband.curve")
    os.makedirs(args.out, exist_ok=True)
    with open(filt_path, "w") as f:
        f.write(f"{len(fwl)}\n")
        for w, t in zip(fwl, ftr):
            f.write(f"{w:.2f} {t:.10e}\n")

    exe = os.path.join(TEST_DIR, "fsyn_export.exe")
    rows = []
    n_ok = 0
    for _, r in df.iterrows():
        sid = int(r["source_id"])
        q = query_xpsd(gaia, handle, float(r["ra"]), float(r["dec"]))
        if q is None or sid not in caldf.index:
            continue
        stars, spectra = q
        best = None
        for i in range(len(stars)):
            sep = np.degrees(np.arccos(np.clip(
                np.sin(float(r["dec"]) * np.pi / 180) * np.sin(stars[i]["dec"] * np.pi / 180) +
                np.cos(float(r["dec"]) * np.pi / 180) * np.cos(stars[i]["dec"] * np.pi / 180) *
                np.cos((float(r["ra"]) - stars[i]["ra"]) * np.pi / 180), -1, 1))) * 3600.0
            if sep <= 0.5:
                best = i
                break
        if best is None:
            continue
        flux_min = float(stars[best]["flux_min"])
        flux_mul = float(stars[best]["flux_mul"])
        if not (flux_mul > 0.0):
            continue
        byte = spectra[best].astype(int)
        flux_dec = byte * flux_mul + flux_min
        sub = caldf.loc[sid]
        xp_abs = np.asarray(sub.iloc[0]["flux"] if isinstance(sub, pd.DataFrame) else sub["flux"],
                            dtype=float)
        if len(xp_abs) != 343:
            continue

        # 生产 C++ (fsyn_export xpsd)
        inp = " ".join(map(str, byte)).encode()
        cp = subprocess.run([exe, "xpsd", filt_path, "none",
                             f"{flux_min:.9e}", f"{flux_mul:.9e}"],
                            input=inp, capture_output=True, timeout=60)
        if cp.returncode != 0:
            continue
        f_cpp = float(cp.stdout.decode().strip())

        f_py_dec = fsyn_cached_like(flux_dec, XPSD_WL, fwl, ftr)
        f_py_xp = fsyn_cached_like(xp_abs, XPSD_WL, fwl, ftr)
        if not (f_py_dec > 0 and f_py_xp > 0 and np.isfinite(f_cpp)):
            continue
        n_ok += 1
        rows.append({
            "source_id": sid,
            "flux_min": flux_min, "flux_mul": flux_mul,
            "f_cpp": f_cpp,
            "f_py_dec": f_py_dec,
            "f_py_xp": f_py_xp,
            "ratio_cpp_py": f_cpp / f_py_dec,
            "ratio_cpp_xp": f_cpp / f_py_xp,
        })

    if n_ok == 0:
        print("[xpsd_cpp] 无有效样本")
        sys.exit(3)
    res = pd.DataFrame(rows)
    r_cpp_py = res["ratio_cpp_py"]
    r_cpp_xp = res["ratio_cpp_xp"]
    stats = {
        "n": n_ok,
        "ratio_cpp_py_median": float(np.median(r_cpp_py)),
        "ratio_cpp_py_p95_absdev": float(np.percentile(np.abs(r_cpp_py - 1.0), 95)),
        "ratio_cpp_py_max_absdev": float(np.max(np.abs(r_cpp_py - 1.0))),
        "ratio_cpp_xp_median": float(np.median(r_cpp_xp)),
        "ratio_cpp_xp_median_absdev": float(np.median(np.abs(r_cpp_xp - 1.0))),
        "ratio_cpp_xp_p95_absdev": float(np.percentile(np.abs(r_cpp_xp - 1.0), 95)),
        "mag_cpp_xp_median": float(np.median(np.abs(-2.5 * np.log10(r_cpp_xp)))),
        "mag_cpp_xp_p95": float(np.percentile(np.abs(-2.5 * np.log10(r_cpp_xp)), 95)),
    }
    stats["gates"] = {
        "cpp_py_algorithm_eq": stats["ratio_cpp_py_p95_absdev"] < 1e-6,
        "cpp_vs_gaiaxpy_median_mag_le_0.005": stats["mag_cpp_xp_median"] <= 0.005,
        "cpp_vs_gaiaxpy_p95_mag_le_0.02": stats["mag_cpp_xp_p95"] <= 0.02,
    }
    stats["all_pass"] = all(stats["gates"].values())
    print(f"[xpsd_cpp] n={n_ok}")
    print(f"[xpsd_cpp] C++ vs Python(同解码): median={np.median(r_cpp_py):.12f} "
          f"p95|1-ratio|={stats['ratio_cpp_py_p95_absdev']:.3e}")
    print(f"[xpsd_cpp] C++ vs GaiaXPy: median ratio={np.median(r_cpp_xp):.6f} "
          f"|dG| median={stats['mag_cpp_xp_median']:.5f} p95={stats['mag_cpp_xp_p95']:.5f}")
    print(f"[xpsd_cpp] GATES: {json.dumps(stats['gates'])}")
    res.to_csv(os.path.join(args.out, "xpsd_cpp_crosscheck.csv"), index=False)
    with open(os.path.join(args.out, "xpsd_cpp_crosscheck.json"), "w", encoding="utf-8") as f:
        json.dump(stats, f, ensure_ascii=False, indent=2)
    print(f"[xpsd_cpp] DONE -> {args.out}")
    sys.exit(0 if stats["all_pass"] else 4)


if __name__ == "__main__":
    main()
