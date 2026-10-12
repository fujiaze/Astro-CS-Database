#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-FIX：P-A 分箱归因 / P-B 选星前后 / P-C k_photo 稳健性。

全部只用**该帧自身**数据（无跨帧耦合）。只读 run/PHOT-FIX/**。

模型（唯一约定，spatial_gain.h，已由产物逐位反证）：
  r_i = log10(F_instr/F_syn);  log10 m_i = −Σ_j coef_j·(B_j(x̃,ỹ) − center_j)
  r_pred = location + Σ_j coef_j·(B_j − center_j);   location = −log10(k_photo)
  残差 e_i = r_i − r_pred

P-B 口径说明（关键）：
  · 文档 §5 失效域明确「饱和或质量异常星不进入定标」⇒ 剔除 saturated 是**文档口径**；
  · 文档 §1 明确「用**全部非饱和的匹配星点**拟合」⇒ 加 SNR 下限**不是**文档口径，
    本脚本把它作为**诊断臂**跑，并单独报出，供裁决。

输出 results/p1fix/pA_pB_pC.json
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


def mad_sigma(v):
    v = np.asarray(v, float)
    if v.size == 0:
        return float("nan")
    m = np.median(v)
    return float(np.median(np.abs(v - m)) / MAD)


def basis(xn, yn):
    return np.column_stack([xn, yn, xn * xn, xn * yn, yn * yn])


def fit_lsq(x, y, r, xr, xsc, yr, ysc):
    """与产物同基、同归一化；返回 (coef, center)。中心化用样本均值（离线口径，前后一致）。"""
    xn = (x - xr) / xsc
    yn = (y - yr) / ysc
    B = basis(xn, yn)
    ct = B.mean(axis=0)
    bc = B - ct
    coef, *_ = np.linalg.lstsq(bc, r, rcond=None)
    return coef, ct


def resid(x, y, r, loc, coef, ct, xr, xsc, yr, ysc):
    xn = (x - xr) / xsc
    yn = (y - yr) / ysc
    B = basis(xn, yn)
    return r - (loc + (B - ct) @ coef)


def nn_index(src_xy, dst_xy, tol=5.0):
    """对 dst 中每个点，找 src 中最近的（返回索引或 -1）。"""
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


def main():
    recs = []
    pa_recs = []
    pc_recs = []
    for tag in ("t2", "t3"):
        phot = json.loads((SRC / tag / "p1_phot.json").read_text())
        flux = json.loads((SRC / tag / "p1_flux.json").read_text())
        srcs = json.loads((SRC / tag / "p1_sources.json").read_text())
        fmap = {f["file"]: f for f in flux["frames"]}
        smap = {f["file"]: f for f in srcs["frames"]}

        def canon(name):
            """cleaned_M42_M1_T3_...-20251126@041940-300S-Red.fts -> 与 p1_phot 的 frame key 同形"""
            s = name.replace("cleaned_", "").replace(".fts", "")
            return s.replace("@", "_")

        fmap_c = {canon(k): v for k, v in fmap.items()}
        smap_c = {canon(k): v for k, v in smap.items()}
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
            prod_e = resid(X, Y, r, loc, coef, ct, norm["x_ref"], norm["x_scale"], norm["y_ref"], norm["y_scale"])

            # 匹配 flux / sources（用帧内文件键）
            fkey = fk if fk in fmap_c else None
            skey = fk if fk in smap_c else None
            fsnr = np.full(len(X), np.nan); ffl = np.full(len(X), np.nan); ffe = np.full(len(X), np.nan)
            if fkey:
                R = fmap_c[fkey]["results"]
                rxy = np.array([[q["x"], q["y"]] for q in R])
                idx = nn_index(rxy, np.column_stack([X, Y]))
                for i, j in enumerate(idx):
                    if j >= 0:
                        fsnr[i] = R[j].get("snr", np.nan) or np.nan
                        ffl[i] = R[j].get("flux", np.nan) or np.nan
                        ffe[i] = R[j].get("flux_error", np.nan) or np.nan
            fwhm = np.full(len(X), np.nan); qual = np.full(len(X), -1); den = np.zeros(len(X))
            if skey:
                S = smap_c[skey]["sources"]
                sxy = np.array([[q["x"], q["y"]] for q in S])
                sidx = nn_index(sxy, np.column_stack([X, Y]))
                for i, j in enumerate(sidx):
                    if j >= 0:
                        fwhm[i] = S[j].get("fwhm_px", np.nan)
                        qual[i] = S[j].get("quality", -1)
                # 局部密度：2.5*median(FWHM) 半径内源数
                med_fw = np.nanmedian([q["fwhm_px"] for q in S if q["fwhm_px"] == q["fwhm_px"]])
                rad = 2.5 * med_fw
                d2m = (sxy[:, None, 0] - sxy[None, :, 0]) ** 2 + (sxy[:, None, 1] - sxy[None, :, 1]) ** 2
                dens_all = (d2m <= rad * rad).sum(axis=1)
                for i, j in enumerate(sidx):
                    if j >= 0:
                        den[i] = dens_all[j]
            photon = DEX * (ffe / ffl)
            m = used

            # ───── P-A：分箱 ─────
            def bins_report(vals, e, name, qs=(0.25, 0.5, 0.75)):
                ok = np.isfinite(vals)
                if ok.sum() < 50:
                    return {"var": name, "error": "insufficient"}
                edges = [np.nanmin(vals[ok])] + [np.nanquantile(vals[ok], q) for q in qs] + [np.nanmax(vals[ok])]
                out = []
                for a, b in zip(edges[:-1], edges[1:]):
                    sel = ok & (vals >= a) & (vals <= b) if b == edges[-1] else ok & (vals >= a) & (vals < b)
                    if sel.sum() < 20:
                        continue
                    out.append({"lo": float(a), "hi": float(b), "n": int(sel.sum()),
                                "sigma_e": mad_sigma(e[sel]),
                                "median_abs_e": float(np.median(np.abs(e[sel] - np.median(e[sel]))))})
                return {"var": name, "bins": out}

            e_all = prod_e[m]
            _pa = {
                "snr": bins_report(fsnr[m], e_all, "snr"),
                "fwhm": bins_report(fwhm[m], e_all, "fwhm"),
                "density": bins_report(den[m], e_all, "density"),
                "photon": bins_report(photon[m], e_all, "photon"),
            }
            pa_recs.append({"tag": tag, "frame": fk, "n_used": int(m.sum()),
                            "snr_med": float(np.nanmedian(fsnr[m])) if np.isfinite(fsnr[m]).any() else None,
                            "fwhm_med": float(np.nanmedian(fwhm[m])) if np.isfinite(fwhm[m]).any() else None,
                            "density_med": float(np.nanmedian(den[m])),
                            "bins": _pa})

            # ───── P-B：选星前后 ─────
            sat = (qual == 1)
            blended = den >= 3                     # 局部密度阈值（诊断臂）
            for fsnr_floor in (0.0, 10.0, 20.0):
                if fsnr_floor <= 0:
                    M = m.copy()
                else:
                    M = m & np.isfinite(fsnr) & (fsnr >= fsnr_floor)
                M = M & (~sat)
                if M.sum() < 60:
                    continue
                # (a) 用产物系数在 M 上评估（纯"选星"效应）
                sig_base_on_M = mad_sigma(prod_e[M])
                # (b) 在 M 上重解（选星 + 重解）
                c2, ct2 = fit_lsq(X[M], Y[M], r[M], norm["x_ref"], norm["x_scale"], norm["y_ref"], norm["y_scale"])
                e2 = resid(X[M], Y[M], r[M], loc, c2, ct2, norm["x_ref"], norm["x_scale"], norm["y_ref"], norm["y_scale"])
                recs.append({
                    "tag": tag, "frame": fk,
                    "n_used_prod": int(m.sum()), "n_mask": int(M.sum()),
                    "snr_floor": fsnr_floor,
                    "n_saturated_in_used": int((m & sat).sum()),
                    "n_blended_in_used": int((m & blended).sum()),
                    "sigma_prod_on_used": mad_sigma(prod_e[m]),
                    "sigma_prod_on_mask": sig_base_on_M,
                    "sigma_refit_on_mask": mad_sigma(e2),
                    "photon_med_mask": float(np.nanmedian(photon[M])) if np.isfinite(photon[M]).any() else None,
                    "loc_prod": loc,
                    "loc_median": float(np.median(r[m])),
                })
            # ───── P-C：k_photo 稳健性 / 影响函数 ─────
            rv = r[m]
            med = float(np.median(rv))
            loc_irls = med
            for _ in range(50):
                S = float(np.median(np.abs(rv - loc_irls)) / MAD)
                if S <= 0:
                    break
                u = (rv - loc_irls) / (4.685 * S)
                w = np.where(np.abs(u) < 1, (1 - u * u) ** 2, 0.0)
                if w.sum() <= 0:
                    break
                new = float((w * rv).sum() / w.sum())
                if abs(new - loc_irls) < 1e-6:
                    loc_irls = new
                    break
                loc_irls = new
            # 影响：去掉 |r−median| 最大的 1% 后 location 的移动
            k = max(1, int(0.01 * len(rv)))
            order = np.argsort(-np.abs(rv - med))
            keep = np.ones(len(rv), bool); keep[order[:k]] = False
            loc_trim = float(np.median(rv[keep]))
            pc_recs.append({"tag": tag, "frame": fk, "n_used": int(m.sum()),
                            "loc_prod": loc, "loc_median": med, "loc_irls": loc_irls,
                            "d_irls_minus_prod": loc_irls - loc,
                            "d_median_minus_prod": med - loc,
                            "loc_trim1pct": loc_trim,
                            "d_trim_minus_irls": loc_trim - loc_irls,
                            "sigma_resid_dex_stored": pd.get("sigma_residual_dex")})
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "pA_pB.json").write_text(json.dumps(recs, indent=1, ensure_ascii=False))
    (OUT / "pA_bins.json").write_text(json.dumps(pa_recs, indent=1, ensure_ascii=False))
    (OUT / "pC_location.json").write_text(json.dumps(pc_recs, indent=1, ensure_ascii=False))
    print("records:", len(recs))
    # 汇总
    for floor in (0.0, 10.0, 20.0):
        sub = [x for x in recs if x["snr_floor"] == floor]
        if not sub:
            continue
        a = sorted(x["sigma_prod_on_used"] for x in sub)
        b = sorted(x["sigma_prod_on_mask"] for x in sub)
        c = sorted(x["sigma_refit_on_mask"] for x in sub)
        med = lambda v: v[len(v) // 2]
        print(f"  floor={floor:>4}  n={len(sub)}  sigma_prod_on_used med={med(a):.5f}  "
              f"prod_on_mask med={med(b):.5f}  refit_on_mask med={med(c):.5f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
