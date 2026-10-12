#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-FIX 任务 1：追 ~0.014 dex 的 SNR 无关底——逐候选给斜率与显著性。

候选：
  (i)   F_syn 精度（Gaia XP × QE × 滤光片）：用 F_syn = F_instr/10^r 作 **Gaia 侧亮度**代理
        （越暗 ⇒ XP 谱信噪比越低 ⇒ F_syn 误差越大）。关键：控制住**像面**光子噪声后，
        e 是否仍随 F_syn 变（2D 分层）；若随 ⇒ Gaia 侧有贡献。
  (ii)  帧级面源残差：e vs 局部背景（p1_flux.background）斜率。
  (iii) 颜色项：**无法判**（见 docs/p1fix_process.md §C.3：谱查询路径把 magBP/magRP 置 0）。
  (iv)  PSF 模型系统差：e vs FWHM 与 e vs 椭圆率 斜率；与蹭叠（局部密度）区分。

统计：逐帧 scipy.stats.linregress(e, x) → slope/slope_stderr/r/p；跨帧报中位斜率、
显著帧占比（|t|>2）、以及合并后的 p 值分布。同时给分箱中位（带 bootstrap 误差）。

单帧内完成，无跨帧耦合。输出 results/p1fix/floor_attribution.json
"""
from __future__ import annotations
import json, math
from pathlib import Path
import numpy as np
from scipy import stats

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD = 0.6744897501960817
DEX = 0.4342944819032518


def mad_sigma(v):
    v = np.asarray(v, float)
    if v.size == 0:
        return float("nan")
    return float(np.median(np.abs(v - np.median(v))) / MAD)


def basis(xn, yn):
    return np.column_stack([xn, yn, xn * xn, xn * yn, yn * yn])


def canon(name):
    return name.replace("cleaned_", "").replace(".fts", "").replace("@", "_")


def nn_index(src_xy, dst_xy, tol=5.0):
    out = np.full(len(dst_xy), -1, dtype=int)
    if len(src_xy) == 0:
        return out
    sx = src_xy[:, 0]; sy = src_xy[:, 1]
    for i, (dx, dy) in enumerate(dst_xy):
        d2 = (sx - dx) ** 2 + (sy - dy) ** 2
        j = int(np.argmin(d2))
        if d2[j] <= tol * tol:
            out[i] = j
    return out


def reg(e, x):
    """返回 slope, stderr, r, p（n<10 或方差为0 返回 None）。"""
    m = np.isfinite(e) & np.isfinite(x)
    if m.sum() < 10:
        return None
    ee, xx = e[m], x[m]
    if np.std(ee) == 0 or np.std(xx) == 0:
        return None
    lr = stats.linregress(xx, ee)
    return {"slope": float(lr.slope), "stderr": float(lr.stderr),
            "r": float(lr.rvalue), "p": float(lr.pvalue), "n": int(m.sum())}


def main():
    records = []
    strata_rec = []
    for tag in ("t2", "t3"):
        phot = json.loads((SRC / tag / "p1_phot.json").read_text())
        flux = json.loads((SRC / tag / "p1_flux.json").read_text())
        srcs = json.loads((SRC / tag / "p1_sources.json").read_text())
        fmap = {canon(f["file"]): f for f in flux["frames"]}
        smap = {canon(f["file"]): f for f in srcs["frames"]}
        for fk in sorted(phot["spatial_gain"]):
            sg = phot["spatial_gain"][fk]; pd = phot["photscale_detail"][fk]
            s = sg.get("samples", {})
            if not s.get("x"):
                continue
            r = np.array(s["r_log10_finstr_over_fsyn"])
            X = np.array(s["x"]); Y = np.array(s["y"]); used = np.array(s["used"], bool)
            kp = pd.get("k_photo")
            if not (kp and kp > 0):
                continue
            loc = -math.log10(kp)
            norm = sg["norm"]; coef = np.array(sg["coef"]); ct = np.array(sg["center"])
            xn = (X - norm["x_ref"]) / norm["x_scale"]; yn = (Y - norm["y_ref"]) / norm["y_scale"]
            e = r - (loc + (basis(xn, yn) - ct) @ coef)          # 已验证的重建（2e-14）

            n = len(X)
            fsnr = np.full(n, np.nan); ffl = np.full(n, np.nan); ffe = np.full(n, np.nan); fbg = np.full(n, np.nan)
            if fk in fmap:
                R = fmap[fk]["results"]
                rxy = np.array([[q["x"], q["y"]] for q in R])
                for i, j in enumerate(nn_index(rxy, np.column_stack([X, Y]))):
                    if j >= 0:
                        fsnr[i] = R[j].get("snr", np.nan) or np.nan
                        ffl[i] = R[j].get("flux", np.nan) or np.nan
                        ffe[i] = R[j].get("flux_error", np.nan) or np.nan
                        fbg[i] = R[j].get("background", np.nan) or np.nan
            fwhm = np.full(n, np.nan); ell = np.full(n, np.nan); qual = np.full(n, -1)
            if fk in smap:
                S = smap[fk]["sources"]
                sxy = np.array([[q["x"], q["y"]] for q in S])
                for i, j in enumerate(nn_index(sxy, np.column_stack([X, Y]))):
                    if j >= 0:
                        fwhm[i] = S[j].get("fwhm_px", np.nan)
                        ell[i] = S[j].get("ellipticity", np.nan)
                        qual[i] = S[j].get("quality", -1)
            photon = DEX * (ffe / ffl)
            fsyn = ffl / np.power(10.0, r)                       # Gaia 侧期望通量
            logfsyn = np.log10(np.where(fsyn > 0, fsyn, np.nan))
            m = used
            rec = {"tag": tag, "frame": fk, "n": int(m.sum()),
                   "sigma_e": mad_sigma(e[m]),
                   "sigma_photon_med": float(np.nanmedian(photon[m])) if np.isfinite(photon[m]).any() else None,
                   "reg": {}}
            # (i) Gaia 侧亮度代理
            rec["reg"]["log10_fsyn"] = reg(e[m], logfsyn[m])
            # (ii) 局部背景
            rec["reg"]["background"] = reg(e[m], fbg[m])
            # (iv) PSF 系统差
            rec["reg"]["fwhm"] = reg(e[m], fwhm[m])
            rec["reg"]["ellipticity"] = reg(e[m], ell[m])
            # 像面侧对照
            rec["reg"]["photon"] = reg(e[m], photon[m])
            rec["reg"]["log10_finstr"] = reg(e[m], np.log10(np.where(ffl[m] > 0, ffl[m], np.nan)))
            records.append(rec)

            # (i) 2D 分层：按像面光子噪声四分位分层，层内 e vs log10(F_syn)
            ph = photon[m]; lf = logfsyn[m]; ee = e[m]
            ok = np.isfinite(ph) & np.isfinite(lf)
            if ok.sum() > 200:
                qs = np.quantile(ph[ok], [0.25, 0.5, 0.75])
                for qi in range(4):
                    if qi == 0:
                        sel = ok & (ph <= qs[0])
                    elif qi == 3:
                        sel = ok & (ph > qs[2])
                    else:
                        sel = ok & (ph > qs[qi - 1]) & (ph <= qs[qi])
                    rr = reg(ee[sel], lf[sel])
                    if rr:
                        strata_rec.append({"tag": tag, "frame": fk, "stratum": qi,
                                           "sigma_e": mad_sigma(ee[sel]), **rr})
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "floor_attribution.json").write_text(json.dumps(
        {"per_frame": records, "fsyn_strata": strata_rec}, indent=1, ensure_ascii=False))

    print(f"frames={len(records)} strata_rows={len(strata_rec)}")
    vars_ = ["log10_fsyn", "log10_finstr", "photon", "background", "fwhm", "ellipticity"]
    for v in vars_:
        sl, tvals = [], []
        for rec in records:
            rr = rec["reg"].get(v)
            if rr and np.isfinite(rr["slope"]) and rr["stderr"] > 0:
                sl.append(rr["slope"]); tvals.append(rr["slope"] / rr["stderr"])
        if not sl:
            continue
        sl_s = sorted(sl); t_s = sorted(tvals)
        n = len(sl_s)
        frac_sig = sum(1 for t in t_s if abs(t) > 2) / n
        print(f"  e vs {v:14s} n={n:2d}  slope med={sl_s[n//2]:+.4g}  "
              f"(p25 {sl_s[n//4]:+.3g}, p75 {sl_s[3*n//4]:+.3g})  |t|>2 fraction={frac_sig:.2f}")
    print()
    print("  (i) 分层（按像面光子噪声四分位）内 e vs log10(F_syn)：")
    for q in range(4):
        sub = [x for x in strata_rec if x["stratum"] == q]
        if not sub:
            continue
        sl = sorted(x["slope"] for x in sub)
        tv = sorted(x["slope"] / x["stderr"] for x in sub if x["stderr"] > 0)
        sg = sorted(x["sigma_e"] for x in sub)
        fr = sum(1 for t in tv if abs(t) > 2) / max(1, len(tv))
        print(f"    stratum {q}: n={len(sub):2d}  slope med={sl[len(sl)//2]:+.4g}  "
              f"|t|>2 frac={fr:.2f}  sigma_e med={sg[len(sg)//2]:.5f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
