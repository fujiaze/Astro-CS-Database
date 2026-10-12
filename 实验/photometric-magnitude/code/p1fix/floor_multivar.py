#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-FIX 任务 1（续）：多元回归分离 (i) Gaia 侧 F_syn 与 (iv) PSF 系统差。

问题：单帧内 `log10(F_syn)` 与像面光子噪声近似共线（同一颗星两边同源），
单变量斜率不可归因。用**标准化多元回归**取偏效应（beta），并给偏 t 与显著性；
再用 `background` 提供的杠杆（同 F_syn 不同天空 → 不同 σ_F）辅助解耦。

模型（逐帧，单帧内）：
  e_std ~ β1·log10(F_syn)_std + β2·photon_std + β3·fwhm_std + β4·background_std + β5·ellip_std
报：各 β 的中位、|t|>2 帧占比、模型 R²、以及逐步加入后的 ΔR²。

输出 results/p1fix/floor_multivar.json
"""
from __future__ import annotations
import json, math
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
SRC = ROOT / "run/PHOT-FIX/p1_rerun"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
MAD = 0.6744897501960817
DEX = 0.4342944819032518


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


def z(v):
    v = np.asarray(v, float)
    s = np.nanstd(v)
    if not np.isfinite(s) or s == 0:
        return np.zeros_like(v)
    return (v - np.nanmean(v)) / s


def ols(y, Xcols, names):
    """最小二乘 + 偏 t。返回 betas 字典与 R²。"""
    m = np.isfinite(y)
    for c in Xcols:
        m &= np.isfinite(c)
    if m.sum() < 40:
        return None
    Y = z(y[m])
    X = np.column_stack([z(c[m]) for c in Xcols])
    X = np.column_stack([np.ones(len(Y)), X])
    try:
        beta, *_ = np.linalg.lstsq(X, Y, rcond=None)
    except Exception:
        return None
    resid = Y - X @ beta
    dof = len(Y) - X.shape[1]
    if dof <= 0:
        return None
    s2 = float(resid @ resid) / dof
    try:
        XtX_inv = np.linalg.inv(X.T @ X)
    except Exception:
        return None
    se = np.sqrt(np.maximum(s2 * np.diag(XtX_inv), 1e-300))
    t = beta / se
    ss_tot = float(Y @ Y)
    r2 = 1.0 - float(resid @ resid) / ss_tot if ss_tot > 0 else float("nan")
    out = {"r2": r2, "n": int(m.sum())}
    for i, nm in enumerate(names):
        out[nm] = {"beta": float(beta[i + 1]), "t": float(t[i + 1])}
    return out


def main():
    recs = []
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
            loc = -math.log10(kp); norm = sg["norm"]
            coef = np.array(sg["coef"]); ct = np.array(sg["center"])
            xn = (X - norm["x_ref"]) / norm["x_scale"]; yn = (Y - norm["y_ref"]) / norm["y_scale"]
            e = r - (loc + (basis(xn, yn) - ct) @ coef)
            n = len(X)
            ffl = np.full(n, np.nan); ffe = np.full(n, np.nan); fbg = np.full(n, np.nan)
            if fk in fmap:
                R = fmap[fk]["results"]
                rxy = np.array([[q["x"], q["y"]] for q in R])
                for i, j in enumerate(nn_index(rxy, np.column_stack([X, Y]))):
                    if j >= 0:
                        ffl[i] = R[j].get("flux", np.nan) or np.nan
                        ffe[i] = R[j].get("flux_error", np.nan) or np.nan
                        fbg[i] = R[j].get("background", np.nan) or np.nan
            fwhm = np.full(n, np.nan); ell = np.full(n, np.nan)
            if fk in smap:
                S = smap[fk]["sources"]
                sxy = np.array([[q["x"], q["y"]] for q in S])
                for i, j in enumerate(nn_index(sxy, np.column_stack([X, Y]))):
                    if j >= 0:
                        fwhm[i] = S[j].get("fwhm_px", np.nan)
                        ell[i] = S[j].get("ellipticity", np.nan)
            photon = DEX * (ffe / ffl)
            logfsyn = np.log10(np.where(ffl / np.power(10.0, r) > 0, ffl / np.power(10.0, r), np.nan))
            m = used
            ee = e[m]
            models = {
                "fwhm_only": (["fwhm"], [fwhm[m]]),
                "fsyn_only": (["logfsyn"], [logfsyn[m]]),
                "bg_only": (["background"], [fbg[m]]),
                "fsyn+photon": (["logfsyn", "photon"], [logfsyn[m], photon[m]]),
                "fsyn+photon+fwhm": (["logfsyn", "photon", "fwhm"], [logfsyn[m], photon[m], fwhm[m]]),
                "full": (["logfsyn", "photon", "fwhm", "background", "ellip"],
                         [logfsyn[m], photon[m], fwhm[m], fbg[m], ell[m]]),
            }
            row = {"tag": tag, "frame": fk, "n": int(m.sum()), "models": {}}
            for key, (names, cols) in models.items():
                f = ols(ee, cols, names)
                if f:
                    row["models"][key] = f
            recs.append(row)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "floor_multivar.json").write_text(json.dumps(recs, indent=1, ensure_ascii=False))

    print(f"frames={len(recs)}")
    for key in ("fwhm_only", "fsyn_only", "bg_only", "fsyn+photon", "fsyn+photon+fwhm", "full"):
        sub = [r["models"][key] for r in recs if key in r["models"]]
        if not sub:
            continue
        r2 = sorted(x["r2"] for x in sub)
        print(f"  [{key}] n={len(sub)} R² med={r2[len(r2)//2]:.3f}")
        vars_ = set()
        for x in sub:
            vars_ |= {k for k in x if k not in ("r2", "n")}
        for v in sorted(vars_):
            b = sorted(x[v]["beta"] for x in sub if v in x)
            tt = sorted(x[v]["t"] for x in sub if v in x)
            if not b:
                continue
            fr = sum(1 for t in tt if abs(t) > 2) / len(tt)
            print(f"      {v:10s} beta med={b[len(b)//2]:+.3f} (p25 {b[len(b)//4]:+.3f}, p75 {b[3*len(b)//4]:+.3f})  "
                  f"|t|>2 frac={fr:.2f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
