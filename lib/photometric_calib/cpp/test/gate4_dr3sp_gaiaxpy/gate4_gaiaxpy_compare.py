# -*- coding: utf-8 -*-
"""
Gate 4 (Phase1 Full Freeze v2): DR3SP 积分 vs GaiaXPy 官方 Oracle 对比

流程:
  1. 读取 XP 样本 CSV (>=1000 星, Gaia DR3 XP_CONTINUOUS 系数)
  2. GaiaXPy generate -> Gaia_DR3_Vega 合成测光 (G/BP/RP)  = Oracle 参考
  3. GaiaXPy 自洽校验: 合成 G/BP/RP 星等 vs Gaia DR3 发布测光 (TAP)
  4. gaiaxpy convert 采样到 XPSD 网格 (336-1020nm @2nm)
  5. AstroCS 参考积分 (fsyn_astrocs.compute_f_syn, λ 加权 + G 归一化)
     × 官方 Gaia EDR3/DR3 通带 (Riello+2021 passband.dat)
  6. 对比颜色 (m_BP-m_G, m_G-m_RP, m_BP-m_RP): AstroCS vs GaiaXPy
  7. numpy 移植 vs 生产 C++ (fsyn_export.exe) 交叉验证 (uint8 光谱)

用法:
  py -3.12 gate4_gaiaxpy_compare.py --csv <sample.csv> --out <dir>
     [--fsyn-exe <fsyn_export.exe>] [--n 1000]
"""

import argparse
import json
import os
import subprocess
import sys

import numpy as np
import pandas as pd

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fsyn_astrocs import akima_interpolate, compute_f_syn, simpson_integrate, synthetic_band_flux

try:
    from gaiaxpy import convert, generate
    from gaiaxpy.generator.photometric_system import PhotometricSystem
    _HAS_GAIAXPY = True
except Exception:  # noqa: BLE001
    _HAS_GAIAXPY = False


XPSD_WL = 336.0 + 2.0 * np.arange(343, dtype=float)
# GaiaXPy Gaia_DR3_Vega 系统零点点 (来自官方 XpFilter_GaiaDr3Vega XML)
ZP = {"G": -26.4899, "BP": -25.9655, "RP": -27.2164}


def load_passbands(path):
    """解析 Gaia EDR3/DR3 passband.dat (Riello+2021).
    列: wl, G, G_err, BP, BP_err, RP, RP_err"""
    arr = np.loadtxt(path)
    out = {}
    for band, col in (("G", 1), ("BP", 3), ("RP", 5)):
        tr = arr[:, col].copy()
        tr[(tr >= 99.0) | (tr <= 0.0)] = 0.0  # 99.99 = 带外哨兵值
        out[band] = (arr[:, 0], tr)
    return out


def astrocs_fsyn(flux, passbands, band):
    """对 XPSD 网格绝对光谱计算合成测光通量 (光子加权平均)."""
    wl, tr = passbands[band]
    return synthetic_band_flux(flux, XPSD_WL, wl, tr)


def color_mag(f_ref, f_band):
    return -2.5 * np.log10(f_band / f_ref) if f_band > 0 and f_ref > 0 else np.nan


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--passband", default=None,
                    help="Gaia passband.dat (默认从包旁查找)")
    ap.add_argument("--fsyn-exe", default=None)
    ap.add_argument("--n", type=int, default=0, help="0=全部")
    args = ap.parse_args()

    if not _HAS_GAIAXPY:
        print("FATAL: gaiaxpy 不可用")
        sys.exit(2)
    pb_path = args.passband or os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                            "GaiaEDR3_passband.dat")
    passbands = load_passbands(pb_path)

    df = pd.read_csv(args.csv)
    if args.n > 0:
        df = df.head(args.n)
    n = len(df)
    print(f"[gate4] 样本: {n} 星")
    if n == 0:
        sys.exit(3)

    # 1. GaiaXPy 合成测光 (Oracle)
    print("[gate4] GaiaXPy generate (Gaia_DR3_Vega) ...")
    phot = generate(args.csv, photometric_system=[PhotometricSystem.Gaia_DR3_Vega],
                    save_file=False)
    phot = phot.set_index("source_id")
    phot.index = phot.index.astype(np.int64)
    if args.n > 0:
        phot = phot.loc[phot.index.isin(df["source_id"])]
    print(f"[gate4] 合成测光完成: {len(phot)} 星")

    # 2. GaiaXPy 自洽校验 vs 发布测光
    rows = []
    g_pub = dict(zip(df["source_id"], df["phot_g_mean_mag"]))
    bp_pub = dict(zip(df["source_id"], df["phot_bp_mean_mag"]))
    rp_pub = dict(zip(df["source_id"], df["phot_rp_mean_mag"]))
    dg, dbp, drp = [], [], []
    for sid in phot.index:
        if sid in g_pub:
            dg.append(phot.loc[sid, "GaiaDr3Vega_mag_G"] - g_pub[sid])
            dbp.append(phot.loc[sid, "GaiaDr3Vega_mag_BP"] - bp_pub[sid])
            drp.append(phot.loc[sid, "GaiaDr3Vega_mag_RP"] - rp_pub[sid])
    selfcheck = {
        "n": len(dg),
        "G_median_absdiff": float(np.nanmedian(np.abs(dg))),
        "BP_median_absdiff": float(np.nanmedian(np.abs(dbp))),
        "RP_median_absdiff": float(np.nanmedian(np.abs(drp))),
        "G_p95_absdiff": float(np.nanpercentile(np.abs(dg), 95)),
    }
    print(f"[gate4] GaiaXPy vs 发布测光: G median|d|={selfcheck['G_median_absdiff']:.5f} "
          f"BP={selfcheck['BP_median_absdiff']:.5f} RP={selfcheck['RP_median_absdiff']:.5f} "
          f"(n={selfcheck['n']})")

    # 3. gaiaxpy calibrate -> 绝对定标合并光谱 (XPSD 网格 336-1020nm @2nm)
    print("[gate4] gaiaxpy calibrate -> 绝对光谱 (XPSD 网格) ...")
    from gaiaxpy import calibrate
    cal = calibrate(args.csv, sampling=XPSD_WL, save_file=False)
    sampled = cal[0].set_index("source_id")
    sampled.index = sampled.index.astype(np.int64)
    if args.n > 0:
        sampled = sampled.loc[sampled.index.isin(df["source_id"])]
    print(f"[gate4] 绝对光谱完成: {len(sampled)} 星")

    # 4. AstroCS 参考积分 (merged BP+RP 光谱, 模拟 XPSD 单条合并光谱)
    print("[gate4] AstroCS 参考积分 ...")
    band_cols = {"G": "GaiaDr3Vega_mag_G", "BP": "GaiaDr3Vega_mag_BP", "RP": "GaiaDr3Vega_mag_RP"}
    rows = []
    for sid in sampled.index.unique():
        sub = sampled.loc[sid]
        flux = np.asarray(sub.iloc[0]["flux"] if isinstance(sub, pd.DataFrame) else sub["flux"],
                          dtype=float)
        fsyn = {}
        for band in ("G", "BP", "RP"):
            fsyn[band] = astrocs_fsyn(flux, passbands, band)
        g_ref = fsyn["G"]
        colors = {
            "BP_G": color_mag(g_ref, fsyn["BP"]),
            "G_RP": color_mag(fsyn["RP"], fsyn["G"]),     # m_G - m_RP
            "BP_RP": color_mag(fsyn["RP"], fsyn["BP"]),
        }
        row = {"source_id": int(sid)}
        for band in ("G", "BP", "RP"):
            row[f"astrocs_fsyn_{band}"] = fsyn[band]
            row[f"astrocs_mag_{band}"] = -2.5 * np.log10(fsyn[band]) + ZP[band]
        for cname, cval in colors.items():
            row[f"astrocs_color_{cname}"] = cval
        if sid in phot.index:
            row["gaiagx_mag_G"] = phot.loc[sid, "GaiaDr3Vega_mag_G"]
            row["gaiagx_mag_BP"] = phot.loc[sid, "GaiaDr3Vega_mag_BP"]
            row["gaiagx_mag_RP"] = phot.loc[sid, "GaiaDr3Vega_mag_RP"]
            row["gaiagx_color_BP_G"] = phot.loc[sid, "GaiaDr3Vega_mag_BP"] - phot.loc[sid, "GaiaDr3Vega_mag_G"]
            row["gaiagx_color_G_RP"] = phot.loc[sid, "GaiaDr3Vega_mag_G"] - phot.loc[sid, "GaiaDr3Vega_mag_RP"]
            row["gaiagx_color_BP_RP"] = phot.loc[sid, "GaiaDr3Vega_mag_BP"] - phot.loc[sid, "GaiaDr3Vega_mag_RP"]
        rows.append(row)

    res = pd.DataFrame(rows)
    res["source_id"] = res["source_id"].astype(np.int64)
    os.makedirs(args.out, exist_ok=True)
    csv_out = os.path.join(args.out, "gate4_compare_per_star.csv")
    res.to_csv(csv_out, index=False)

    # 5. 颜色对比统计
    stats = {}
    for cname in ("BP_G", "G_RP", "BP_RP"):
        b1, b2 = {"BP_G": ("BP", "G"), "G_RP": ("G", "RP"), "BP_RP": ("BP", "RP")}[cname]
        d = ((res[f"astrocs_mag_{b1}"] - res[f"astrocs_mag_{b2}"]) -
             (res[f"gaiagx_mag_{b1}"] - res[f"gaiagx_mag_{b2}"])).dropna()
        stats[cname] = {
            "n": int(len(d)),
            "median_diff_mag": float(np.nanmedian(d)),
            "p90_absdiff_mag": float(np.nanpercentile(np.abs(d), 90)),
            "p95_absdiff_mag": float(np.nanpercentile(np.abs(d), 95)),
            "max_absdiff_mag": float(np.nanmax(np.abs(d))),
        }
        print(f"[gate4] 颜色 {cname}: n={len(d)} median_diff={np.nanmedian(d):+.4f} "
              f"p95|d|={np.nanpercentile(np.abs(d),95):.4f} mag")
    # 5.1 逐带星等对比 (官方零点)
    mag_stats = {}
    for band in ("G", "BP", "RP"):
        d = (res[f"astrocs_mag_{band}"] - res[f"gaiagx_mag_{band}"]).dropna()
        mag_stats[band] = {
            "n": int(len(d)),
            "median_diff_mag": float(np.nanmedian(d)),
            "median_absdiff_mag": float(np.nanmedian(np.abs(d))),
            "p95_absdiff_mag": float(np.nanpercentile(np.abs(d), 95)),
            "max_absdiff_mag": float(np.nanmax(np.abs(d))),
        }
        print(f"[gate4] 星等 {band}: n={len(d)} median_diff={np.nanmedian(d):+.5f} "
              f"median|d|={np.nanmedian(np.abs(d)):.5f} p95|d|={np.nanpercentile(np.abs(d),95):.5f} mag")

    # 6. numpy 移植 vs C++ fsyn_export 交叉验证 (uint8 光谱)
    cpp_check = None
    if args.fsyn_exe and os.path.exists(args.fsyn_exe):
        cpp_diffs = []
        # 用采样光谱归一化到 uint8 (max=255) 模拟 XPSD, 取前 5 颗
        sid_list = list(dict.fromkeys(int(v) for v in sampled.index))[:5]
        for sid in sid_list:
            sub = sampled.loc[np.int64(sid)]
            fl = np.asarray(sub.iloc[0]["flux"] if isinstance(sub, pd.DataFrame) else sub["flux"],
                            dtype=float)
            fl = np.clip(fl, 0, None)
            mx = fl.max()
            if mx <= 0:
                continue
            u8 = np.rint(fl / mx * 255.0).astype(np.uint8)
            mag_g = float(g_pub.get(sid, 12.0))
            wl_g, tr_g = passbands["G"]
            inp = " ".join(str(int(v)) for v in u8)
            filt_file = os.path.join(args.out, "g_passband.txt")
            with open(filt_file, "w", encoding="utf-8") as f:
                f.write(f"{len(wl_g)}\n")
                for w, t in zip(wl_g, tr_g):
                    f.write(f"{w:.2f} {t:.10e}\n")
            try:
                cp = subprocess.run([args.fsyn_exe, filt_file, "none", f"{mag_g:.4f}"],
                                    input=inp, stdout=subprocess.PIPE,
                                    stderr=subprocess.DEVNULL, text=True, timeout=30)
                cpp_fsyn = float(cp.stdout.strip())
            except Exception as e:  # noqa: BLE001
                print(f"[gate4] C++ 校验失败 {sid}: {e}")
                continue
            py_fsyn = compute_f_syn(u8, XPSD_WL, wl_g, tr_g, None, None, mag_g)
            rel = abs(cpp_fsyn - py_fsyn) / max(abs(cpp_fsyn), 1e-300)
            cpp_diffs.append(rel)
        if cpp_diffs:
            cpp_check = {"n": len(cpp_diffs), "max_rel_diff": float(max(cpp_diffs))}
            print(f"[gate4] numpy vs C++ fsyn_export: n={len(cpp_diffs)} "
                  f"max_rel_diff={cpp_check['max_rel_diff']:.3e}")

    result = {"n_stars": int(len(res)), "selfcheck": selfcheck, "mag_stats": mag_stats,
              "color_stats": stats,
              "cpp_validation": cpp_check, "passband_source": pb_path}
    with open(os.path.join(args.out, "gate4_result.json"), "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(f"[gate4] 结果已保存: {args.out}")
    print(f"[gate4] DONE n_stars={len(res)}")


if __name__ == "__main__":
    main()
