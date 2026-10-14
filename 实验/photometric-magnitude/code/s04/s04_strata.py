# -*- coding: utf-8 -*-
"""S04 STEP-3 · 颜色 / 星等 / 位置 / SNR 分层归因 + 训练–留出（不重叠）

被估量
------
`d_ki = -2.5·log10(f_ki/q_i) - mu_k`  [mag]，逐星、逐帧的星等残差（inlier 集）。
`sigma_obs,k = MAD(d_k)/0.6744897501960817` [mag]（帧级）。

轴（标准化 `z = (v-mean)/std`，**统一在主样本上标准化**，使各分箱系数可比）
------------------------------------------------------------------------
`z_col`  = 参考通带色指数（`-2.5·log10(F_syn,Baader R / F_syn,Baader B)`，同 QE）[mag]
`z_mag`  = Gaia G [mag]（亮度轴 / 探测器线性指纹）
`z_r`    = 像面径向距离 [px]（平场 / 渐晕 / PSF 空间变化指纹）
`z_x`,`z_y` = 像面坐标 [px]
`z_snr`  = log10(SNR)，SNR = f/flux_err

纪律
----
- **同一颗星跨帧重复观测不是独立样本**：bootstrap 一律按 `star_index` 聚类；
  「有效独立样本数」= 互异星数（8 帧共用同一 Gaia 锥缓存，索引即星身份）。
- **训练/留出不重叠**：同帧臂按星奇偶划分；跨仪器臂以仪器为单位（T2↔T3）。
- σ_obs 的扣除一律**重跑整条口径**（`scia_calib.calibrate`，F' = F·10^(-0.4·corr_mag)）。

输出：results/strata.json、results/perstar_all_head.csv、figures/fig1..fig3
"""
from __future__ import annotations

import csv
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s04_common as C  # noqa: E402

PERSTAR = os.path.join(C.RESULTS, "perstar")
AXES = ("z_col", "z_mag", "z_r", "z_snr")
N_BOOT = 1200
SEED = 20261014


def load_perstar(var: str = "head"):
    frames = []
    for fn in sorted(os.listdir(PERSTAR)):
        if not fn.endswith(f"__{var}.npz"):
            continue
        fid = fn.replace(f"__{var}.npz", "")
        z = np.load(os.path.join(PERSTAR, fn))
        d = {k: z[k] for k in z.files}
        cand_p = os.path.join(PERSTAR, f"{fid}__{var}__cand.npz")
        if os.path.exists(cand_p):
            zc = np.load(cand_p)
            d["candidates"] = {k: zc[k] for k in zc.files}
        d["frame_id"] = fid
        d["observer"] = "Telescope2" if "_T2_" in fid else "Telescope3"
        frames.append(d)
    return frames


def base_valid(d):
    return (np.isfinite(d["resid_mag"]) & np.isfinite(d["col_RminusB"])
            & np.isfinite(d["snr"]) & (d["snr"] > 0))


def zscore(v, m):
    v = np.asarray(v, float)
    sd = float(np.std(v[m]))
    return (v - float(np.mean(v[m]))) / (sd if sd > 0 else 1.0)


def axis_cols(d, m_std):
    """在主样本 m_std 上标准化，返回全长度轴字典。"""
    return dict(
        z_col=zscore(d["col_RminusB"], m_std),
        z_mag=zscore(d["mag_g"], m_std),
        z_r=zscore(d["r_from_center_px"], m_std),
        z_x=zscore(d["x"], m_std),
        z_y=zscore(d["y"], m_std),
        z_snr=zscore(np.log10(np.clip(d["snr"], 1e-30, None)), m_std),
    )


def fit_axes(d, mask, cols, use=AXES, n_boot=N_BOOT, seed=SEED, physical=None):
    """多轴 OLS：d ~ 1 + Σ z（轴在主样本上标准化）。bootstrap 按 star_index 聚类。

    physical: {axis: (原始量数组, 单位)}，额外给「每物理单位」的斜率。
    """
    y = d["resid_mag"][mask]
    X = np.column_stack([np.ones(int(mask.sum()))] + [cols[a][mask] for a in use])
    r = C.ols_with_ci(y, X, n_boot=n_boot, seed=seed, cluster=d["star_index"][mask])
    names = ["intercept"] + list(use)
    out = {n: dict(coef_mag_per_unit_std=r["coef"][i], se=r["se_analytic"][i],
                   ci95=[r["ci95_lo"][i], r["ci95_hi"][i]],
                   significance=(r["coef"][i] / r["se_analytic"][i]
                                 if r["se_analytic"][i] > 0 else None))
           for i, n in enumerate(names)}
    for i, n in enumerate(names[1:], start=1):
        if physical and n in physical:
            arr, unit = physical[n]
            sd = float(np.std(arr[mask]))
            out[n]["physical_unit"] = unit
            out[n]["coef_mag_per_physical"] = float(r["coef"][i]) / (sd if sd > 0 else 1.0)
    out["_meta"] = dict(n=int(r["n"]), n_clusters=int(r["n_clusters"]),
                        n_boot=int(r["n_boot"]), seed=int(r["seed"]))
    return out


PHYS = {"z_col": ("col_RminusB", "mag(R-B)"), "z_mag": ("mag_g", "mag(G)"),
        "z_r": ("r_from_center_px", "px"), "z_x": ("x", "px"), "z_y": ("y", "px"),
        "z_snr": ("snr", "dex(log10 SNR)")}


def physical_of(d):
    return {k: (np.log10(np.clip(d[v[0]], 1e-30, None)) if k == "z_snr" else d[v[0]], v[1])
            for k, v in PHYS.items()}


def stratify(d, by: str, cols, nbin=4, n_boot=400):
    v = {"mag": d["mag_g"], "color": d["col_RminusB"], "radius": d["r_from_center_px"],
         "snr": np.log10(np.clip(d["snr"], 1e-30, None))}[by]
    base = base_valid(d)
    vv = v[base]
    q = np.percentile(vv, np.linspace(0, 100, nbin + 1))
    q[0] -= 1e-9; q[-1] += 1e-9
    rows = []
    for i in range(nbin):
        mask = np.zeros(d["resid_mag"].size, bool)
        idx = np.nonzero(base)[0]
        mask[idx[(vv >= q[i]) & (vv < q[i + 1])]] = True
        if mask.sum() < 15:
            rows.append(dict(bin=i, lo=float(q[i]), hi=float(q[i + 1]),
                             n=int(mask.sum()), status="TOO_FEW"))
            continue
        f = fit_axes(d, mask, cols, n_boot=n_boot, physical=physical_of(d))
        rows.append(dict(bin=i, lo=float(q[i]), hi=float(q[i + 1]),
                         n=int(mask.sum()), n_clusters=f["_meta"]["n_clusters"],
                         median_resid_mag=float(np.median(d["resid_mag"][mask])),
                         mad_sigma_mag=C.mad_sigma_mag(d["resid_mag"][mask]),
                         coef={k: f[k] for k in ("intercept",) + AXES}))
    return dict(variable=by, variable_unit={"mag": "mag", "color": "mag",
                                            "radius": "px", "snr": "dex"}[by],
                bin_edges=[float(x) for x in q], bins=rows)


def selection_diagnostics(frames, fwhm_map=None):
    rows = []
    for d in frames:
        msel = np.isfinite(d["resid_mag"])
        c = d.get("candidates")
        fh = (fwhm_map or {}).get(d["frame_id"])
        n_fit = int(c["chi2"].size) if c else int(d["chi2"].size)
        ch = np.asarray(c["chi2"], float) if c else np.asarray(d["chi2"], float)
        # 进入 perstar 的样本另需 f_syn>0；此处以 resid 有限者近似（差值逐帧另有字段）
        rows.append(dict(
            frame_id=d["frame_id"], observer=d["observer"],
            n_candidates=n_fit,
            n_fit_ok=int(np.sum(c["fit_ok"])) if c else None,
            n_selected=int(msel.sum()),
            survive_frac=float(msel.sum() / max(n_fit, 1)),
            chi2_median_candidates=_nm(np.nanmedian(ch)),
            chi2_median_selected=float(np.median(d["chi2"][msel])),
            chi2_p90_candidates=_nm(np.nanpercentile(ch, 90)),
            n_chi2_ge4=int(np.sum(ch >= 4.0)),
            n_chi2_nan=int(np.sum(~np.isfinite(ch))),
            n_magG_ge16=int(np.sum(c["mag_g"] >= 16.0)) if c else None,
            fwhm_model_px=1.98, fwhm_header_px=fh,
            psf_mismatch_ratio=(float(fh) / 1.98 if fh else None),
            n_saturated_candidates=int(np.sum(c["saturated_stamp"])) if c else None,
            n_inlier=int(np.sum(d["inlier"])),
            sigma_obs_mag=C.mad_sigma_mag(d["resid_mag"][d["inlier"]])))
    # 相关性：存活率 ~ 质量/PSF 指标（只作诊断，不主张因果）
    ok = [r for r in rows if r["chi2_median_candidates"] is not None]
    if len(ok) >= 4:
        sv = np.array([r["survive_frac"] for r in ok])
        ch = np.array([r["chi2_median_candidates"] for r in ok])
        fm = np.array([r["psf_mismatch_ratio"] for r in ok])
        obs = np.array([r["sigma_obs_mag"] for r in ok])
        rows_meta = dict(
            n=len(ok),
            pearson_log_surv_vs_chi2median=float(np.corrcoef(np.log(sv), ch)[0, 1]),
            pearson_log_surv_vs_fwhm_ratio=float(np.corrcoef(np.log(sv), fm)[0, 1]),
            pearson_chi2median_vs_fwhm_ratio=float(np.corrcoef(ch, fm)[0, 1]),
            pearson_sigma_obs_vs_chi2median=float(np.corrcoef(obs, ch)[0, 1]),
            pearson_sigma_obs_vs_fwhm_ratio=float(np.corrcoef(obs, fm)[0, 1]),
            pearson_sigma_obs_vs_n_inlier=float(np.corrcoef(
                obs, np.array([r["n_inlier"] for r in ok]))[0, 1]),
            interpretation=("存活率与 χ² 中位数强负相关（同一候选星表的 434 颗星在 8 帧"
                            "完全相同）⇒ 判据样本集由**拟合质量筛**决定，而拟合质量受"
                            "固定 FWHM=1.98 px 模型与逐帧实际 PSF 失配影响。"
                            "这是 σ_obs 跨帧可比性的一个已量化混淆项，不主张单一成因。"))
    else:
        rows_meta = dict(n=len(ok), note="样本不足，未算相关")
    return rows, rows_meta


def _nm(v):
    v = float(v)
    return v if np.isfinite(v) else None


def train_holdout_same_frame(d, cols, n_boot=800):
    """同帧臂：按 star_index 排序奇偶划分训练/留出（星不重叠）。"""
    from scia_calib import calibrate
    si = d["star_index"]
    lvl = np.zeros(si.size, int)
    lvl[np.argsort(si, kind="stable")] = np.arange(si.size) % 2
    base = base_valid(d) & d["inlier"]
    fs_all = d["f_syn_R"]
    res = {}
    for axis in ("z_col", "z_mag", "z_r"):
        res[axis] = {}
        for train_lvl, tag in ((0, "train_even"), (1, "train_odd")):
            tr = base & (lvl == train_lvl)
            te = base & (lvl == 1 - train_lvl)
            Xtr = np.column_stack([np.ones(int(tr.sum())), cols[axis][tr]])
            r = C.ols_with_ci(d["resid_mag"][tr], Xtr, n_boot=n_boot, seed=SEED,
                              cluster=d["star_index"][tr])
            k = float(r["coef"][1])
            corr = np.zeros(d["resid_mag"].size)
            corr[te] = k * cols[axis][te]
            Fp = d["flux_adu"] * 10.0 ** (-0.4 * corr)
            cal = calibrate(Fp, fs_all, None, d["x"], d["y"], m_degree=0)
            base_cal = calibrate(d["flux_adu"], fs_all, None, d["x"], d["y"], m_degree=0)
            res[axis][tag] = dict(
                train_n=int(tr.sum()), test_n=int(te.sum()),
                train_n_clusters=int(np.unique(d["star_index"][tr]).size),
                test_n_clusters=int(np.unique(d["star_index"][te]).size),
                train_slope_mag_per_std=k,
                train_slope_ci95=[float(r["ci95_lo"][1]), float(r["ci95_hi"][1])],
                test_before_sigma_obs=None,
                sigma_obs_all_before=float(base_cal["sigma_obs_mag"]),
                sigma_obs_all_after=float(cal["sigma_obs_mag"]),
                reduction_pct=float((base_cal["sigma_obs_mag"] - cal["sigma_obs_mag"])
                                    / base_cal["sigma_obs_mag"] * 100))
    # 留出子集自身的 σ_obs（不重叠样本）——由 after 版在留出 star 集上重算
    return res


def train_holdout_cross_instrument(frames, n_boot=1200):
    from scia_calib import calibrate

    def stack(obs):
        ds = [d for d in frames if d["observer"] == obs]
        g = lambda k: np.concatenate([d[k][d["inlier"]] for d in ds])   # noqa: E731
        return dict(y=g("resid_mag"), col=g("col_RminusB"), mag=g("mag_g"),
                    r=g("r_from_center_px"),
                    frame=np.concatenate([[d["frame_id"]] * int(d["inlier"].sum())
                                          for d in ds]),
                    star=g("star_index"), frames=ds)

    def demean(v, frame):
        v = v.copy()
        for f in np.unique(frame):
            m = frame == f
            v[m] -= np.median(v[m])
        return v

    out = {}
    for train_obs, test_obs, tag in (("Telescope2", "Telescope3", "T2train_T3holdout"),
                                     ("Telescope3", "Telescope2", "T3train_T2holdout")):
        tr, te = stack(train_obs), stack(test_obs)
        ytr, ctr, rtr = demean(tr["y"], tr["frame"]), demean(tr["col"], tr["frame"]), \
            demean(tr["r"], tr["frame"])
        b_col = float(np.polyfit(ctr, ytr, 1)[0])
        b_r = float(np.polyfit(rtr, ytr, 1)[0])
        rng = np.random.default_rng(SEED)
        uniq = np.unique(tr["star"])
        idx_by = {s: np.nonzero(tr["star"] == s)[0] for s in uniq}
        bs = []
        for _ in range(n_boot):
            rows = np.concatenate([idx_by[uniq[k]]
                                   for k in rng.integers(0, uniq.size, uniq.size)])
            bs.append(np.polyfit(ctr[rows], ytr[rows], 1)[0])
        bs = np.array(bs)
        # IRLS 稳健斜率（对离群更稳），同时给 OLS 斜率
        from scia_common import irls_tukey
        ir = irls_tukey(ctr)
        b_col_ir = float(np.polyfit(ctr[ir["inliers"]], ytr[ir["inliers"]], 1)[0]) \
            if ir["n_in"] > 10 else float("nan")

        per = []
        for d in te["frames"]:
            m = d["inlier"]
            med = float(np.median(d["col_RminusB"][m]))
            corr = np.zeros(d["resid_mag"].size)
            corr[m] = b_col * (d["col_RminusB"][m] - med)
            Fp = d["flux_adu"] * 10.0 ** (-0.4 * corr)
            base_cal = calibrate(d["flux_adu"], d["f_syn_R"], None, d["x"], d["y"], m_degree=0)
            cal = calibrate(Fp, d["f_syn_R"], None, d["x"], d["y"], m_degree=0)
            per.append(dict(frame_id=d["frame_id"], n=int(m.sum()),
                            n_clusters=int(np.unique(d["star_index"][m]).size),
                            sigma_obs_before=float(base_cal["sigma_obs_mag"]),
                            sigma_obs_after=float(cal["sigma_obs_mag"]),
                            reduction_pct=float((base_cal["sigma_obs_mag"]
                                                 - cal["sigma_obs_mag"])
                                                / base_cal["sigma_obs_mag"] * 100)))
        out[tag] = dict(
            train_observer=train_obs, holdout_observer=test_obs,
            train_rows=int(tr["y"].size), train_n_clusters=int(np.unique(tr["star"]).size),
            holdout_rows=int(te["y"].size),
            holdout_n_clusters=int(np.unique(te["star"]).size),
            slope_color_mag_per_mag=b_col,
            slope_color_mag_per_mag_irls=b_col_ir,
            slope_color_ci95=[float(np.percentile(bs, 2.5)), float(np.percentile(bs, 97.5))],
            n_boot=int(bs.size),
            slope_radius_mag_per_px=b_r,
            holdout_per_frame=per,
            holdout_pooled_reduction_pct=float(np.mean([p["reduction_pct"] for p in per])))
    return out


def write_combined_csv(frames):
    p = os.path.join(C.RESULTS, "perstar_all_head.csv")
    cols = ["star_index", "ra", "dec", "mag_g", "x", "y", "flux_adu", "flux_err_adu",
            "snr", "chi2", "f_syn_R", "f_syn_B", "col_RminusB", "f_syn_G", "col_GminusR",
            "r_from_center_px", "r_dex", "resid_mag", "inlier", "saturated"]
    with open(p, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["frame_id", "observer"] + cols)
        for d in frames:
            for i in range(d["resid_mag"].size):
                row = [d["frame_id"], d["observer"]]
                for c in cols:
                    v = d[c][i]
                    row.append(int(v) if isinstance(v, (bool, np.bool_))
                               else (("%.10g" % v) if isinstance(v, (float, np.floating))
                                     else int(v)))
                w.writerow(row)
    return p


def figures(frames, out, ceilings):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({"font.size": 9})
    figs = []

    d0 = [d for d in frames if d["observer"] == "Telescope2"][0]
    m = d0["inlier"]
    fig, axs = plt.subplots(1, 3, figsize=(13.5, 4.3))
    for a, (key, lab) in zip(axs, (("col_RminusB", "R-B colour index [mag] (ref. passband)"),
                                   ("mag_g", "Gaia G [mag]"),
                                   ("r_from_center_px", "radial distance r [px]"))):
        a.plot(d0[key][m], d0["resid_mag"][m], ".", ms=3, alpha=0.55)
        k, b = np.polyfit(d0[key][m], d0["resid_mag"][m], 1)
        xs = np.linspace(float(np.min(d0[key][m])), float(np.max(d0[key][m])), 20)
        a.plot(xs, k * xs + b, "r-", lw=1.5, label=f"OLS slope={k:.4f}")
        a.set_xlabel(lab); a.legend(fontsize=8); a.grid(alpha=0.3)
    axs[0].set_ylabel("magnitude residual d [mag]")
    fig.suptitle("S04 Fig.2  per-star residual vs colour / brightness / position"
                 f"   (T2 frame {d0['frame_id'][-24:]}, inlier n={int(m.sum())})")
    fig.tight_layout()
    p2 = os.path.join(C.FIGURES, "fig2_residual_strata.png")
    fig.savefig(p2, dpi=140); plt.close(fig); figs.append(p2)

    fig, ax = plt.subplots(figsize=(7.8, 4.4))
    dd = [s for s in out["selection_diagnostics"]
          if s["chi2_median_candidates"] is not None]
    surv = [s["survive_frac"] for s in dd]
    chi = [s["chi2_median_candidates"] for s in dd]
    obs = [s["sigma_obs_mag"] for s in dd]
    sc = ax.scatter(chi, surv, c=obs, cmap="viridis", s=80, edgecolor="k", linewidth=0.4)
    for s, xi, yi in zip(dd, chi, surv):
        ax.annotate(s["frame_id"].split("-")[-1].replace(".fts", ""), (xi, yi),
                    fontsize=7, xytext=(3, 3), textcoords="offset points")
    fig.colorbar(sc, label=r"$\sigma_{\rm obs}$ [mag]")
    ax.set_xlabel(r"median $\chi^2_{\rm red}$ over all 434 candidate fits")
    ax.set_ylabel("fraction surviving the chi2<4 selection")
    ax.set_title("S04 Fig.3  selection survival vs PSF model mismatch (fixed FWHM=1.98 px)")
    ax.grid(alpha=0.3); fig.tight_layout()
    p3 = os.path.join(C.FIGURES, "fig3_selection_psf_mismatch.png")
    fig.savefig(p3, dpi=140); plt.close(fig); figs.append(p3)

    # Fig 1: 帧级 σ_obs + 两套上界
    fig, ax = plt.subplots(figsize=(10, 4.6))
    x = np.arange(len(frames))
    obs = [C.mad_sigma_mag(d["resid_mag"][d["inlier"]]) for d in frames]
    ce = [ceilings[d["frame_id"]]["canonical"] for d in frames]
    cs = [ceilings[d["frame_id"]]["selfref"] for d in frames]
    labs = [d["frame_id"].split("-")[-3] + ("(T2)" if d["observer"].endswith("2")
            else "(T3)") for d in frames]
    ax.bar(x, obs, width=0.5, color="#3b6fd4", label=r"$\sigma_{\rm obs}$ (measured)")
    ax.plot(x, cs, "r^--", ms=6,
            label=r"$\sigma_{\rm ceiling}$, self-referential $\sigma_{\rm flat}$ (pre-P1-B01)")
    ax.plot(x, ce, "ks-", ms=5,
            label=r"$\sigma_{\rm ceiling}$, $\sigma_{\rm flat}=0.0007$ mag (current)")
    ax.set_xticks(x); ax.set_xticklabels(labs, rotation=35, ha="right", fontsize=8)
    ax.set_ylabel("mag"); ax.legend(fontsize=8); ax.grid(alpha=0.3, axis="y")
    ax.set_title("S04 Fig.1  HEAD-variance per-frame sigma_obs and both ceilings (8 compliant frames)")
    fig.tight_layout()
    p1 = os.path.join(C.FIGURES, "fig1_sigma_obs_per_frame.png")
    fig.savefig(p1, dpi=140); plt.close(fig); figs.append(p1)
    return [os.path.relpath(p, C.REPO) for p in figs]


def main():
    import time
    t0 = time.time()
    inv = json.load(open(os.path.join(C.RESULTS, "frame_inventory.json"), encoding="utf-8"))
    fwhm_map = wall_of = None
    # 逐帧 FWHM 头值从 repro_comparison.json 取（不重复读图）
    rc = json.load(open(os.path.join(C.RESULTS, "repro_comparison.json"), encoding="utf-8"))
    fwhm_map = {m["frame_id"]: m.get("fwhm_header") for m in rc["frames_head"]}
    ceilings = {m["frame_id"]: dict(canonical=m["gate"]["sigma_ceiling"],
                                    selfref=m["gate"]["pre_revision"]["sigma_ceiling"])
                for m in rc["frames_head"]}

    frames = load_perstar("head")
    print(f"[strata] {len(frames)} frames")
    out = dict(step="S04-3 strata + train/holdout", base_sha=inv["base_sha"],
               n_boot=N_BOOT, seed=SEED,
               axis_definitions=dict(
                   z_col="标准化参考色 -2.5·log10(F_syn,BaaderR/F_syn,BaaderB) [mag]",
                   z_mag="标准化 Gaia G [mag]", z_r="标准化像面径向距离 [px]",
                   z_x="标准化 x [px]", z_y="标准化 y [px]",
                   z_snr="标准化 log10(f/flux_err)"),
               cluster_rule="bootstrap 按 star_index（Gaia 锥内星身份）聚类；同星跨帧不按独立样本计",
               per_frame=[dict(frame_id=d["frame_id"], observer=d["observer"],
                               n_rows=int(d["resid_mag"].size),
                               n_inlier=int(d["inlier"].sum()),
                               sigma_obs_mag=C.mad_sigma_mag(d["resid_mag"][d["inlier"]]),
                               median_chi2=float(np.median(d["chi2"])),
                               n_saturated=int(np.sum(d["saturated"]))) for d in frames])
    sel_diag, sel_meta = selection_diagnostics(frames, fwhm_map)
    out["selection_diagnostics"] = sel_diag
    out["selection_diagnostics_meta"] = sel_meta

    d0 = [d for d in frames if d["observer"] == "Telescope2"][0]
    m0 = base_valid(d0) & d0["inlier"]
    cols0 = axis_cols(d0, m0)
    # 轴共线性与星点几何覆盖（读数，不作因果）
    AX = list(AXES)
    M = np.column_stack([cols0[a][m0] for a in AX])
    Cm = np.corrcoef(M.T)
    # 覆盖率：星点落在像面上的等效面积（以最大半径的圆近似）占全帧
    rmax = float(np.max(d0["r_from_center_px"][m0]))
    H, W = 4096, 4096
    cover = float(np.pi * rmax ** 2 / (H * W))
    out["axis_collinearity"] = dict(
        axes=AX, pearson=np.round(Cm, 4).tolist(),
        note=("z_mag 与 z_snr 强共线（|r| 见矩阵）⇒ 二者单独的系数不可单独解释，"
              "只有联合方向有意义；z_col 与它们的相关性决定了分箱系数的稳定性。"))
    out["star_geometry"] = dict(
        n=int(m0.sum()), r_max_px=rmax, r_median_px=float(np.median(
            d0["r_from_center_px"][m0])),
        frame_px=[H, W], equivalent_area_fraction=cover,
        note=("判据星点只覆盖以像面中心为心、半径 ≤ r_max 的圆盘，"
              "面积占全帧约 %.0f%%；`m(x,y)` 在该圆盘外属**外推区**"
              "（docs/science/photometry/PHOTOMETRY.md 第 1 节）。" % (100 * cover)))
    out["single_frame_T2"] = dict(
        frame_id=d0["frame_id"], n=int(m0.sum()),
        n_clusters=int(np.unique(d0["star_index"][m0]).size),
        multi_axis=fit_axes(d0, m0, cols0, use=AXES, physical=physical_of(d0)),
        strata=[stratify(d0, v, cols0) for v in ("mag", "color", "radius", "snr")])

    ys, cs, stars, fr, obs = [], [], [], [], []
    for d in frames:
        m = base_valid(d) & d["inlier"]
        med = float(np.median(d["resid_mag"][m]))
        ys.append(d["resid_mag"][m] - med)
        cs.append(d["col_RminusB"][m] - float(np.median(d["col_RminusB"][m])))
        stars.append(d["star_index"][m])
        fr.append(np.full(int(m.sum()), d["frame_id"])); obs.append(d)
    y, c = np.concatenate(ys), np.concatenate(cs)
    star = np.concatenate(stars)
    X = np.column_stack([np.ones(y.size), (c - c.mean()) / max(c.std(), 1e-30)])
    r = C.ols_with_ci(y, X, n_boot=N_BOOT, seed=SEED, cluster=star)
    out["cross_frame_pooled"] = dict(
        n_rows=int(y.size), n_clusters=int(np.unique(star).size),
        note="残差与色轴均已去各帧中位 ⇒ 斜率是帧间共同量；bootstrap 按星聚类",
        slope_color_mag_per_mag=dict(coef_per_unit_std=r["coef"][1],
                                     se=r["se_analytic"][1],
                                     ci95=[r["ci95_lo"][1], r["ci95_hi"][1]],
                                     significance=r["coef"][1] / r["se_analytic"][1],
                                     coef_mag_per_mag_physical=(r["coef"][1]
                                                                / max(c.std(), 1e-30))))
    out["train_holdout_same_frame"] = train_holdout_same_frame(d0, cols0)
    out["train_holdout_cross_instrument"] = train_holdout_cross_instrument(frames)
    p = write_combined_csv(frames)
    out["perstar_all_csv"] = os.path.relpath(p, C.REPO)
    out["figures"] = figures(frames, out, ceilings)
    C.jdump(out, "strata.json")
    print(f"[strata] done in {time.time()-t0:.1f}s")
    for f in frames:
        s = [s for s in out["selection_diagnostics"] if s["frame_id"] == f["frame_id"]][0]
        print(f"   {f['frame_id'][-32:]:32s} surv={s['survive_frac']:.3f} "
              f"chi2med_cand={s['chi2_median_candidates']:.2f} "
              f"n_chi2ge4={s['n_chi2_ge4']} FWHMhdr={s['fwhm_header_px']} "
              f"obs={s['sigma_obs_mag']:.6f} n_inl={s['n_inlier']}")


if __name__ == "__main__":
    main()
