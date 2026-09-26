#!/usr/bin/env python3
"""EXP-P4-04: Brightness-forward validation of dense-SNR reconstruction and its
downstream (P5-side) consumption -- the chain use-case.

Audit items I8 (brightness forward constraint: ASTROCS_DESIGN S2.4 /
CONTROL_WEIGHT_SNR.md S2b) and I6 (dense / sparse_reconstruct / frame_reconstruct
are reconstruction manners of ONE physical quantity; weights are NOT a caliber).

Physical forward (pure synthetic, NOISE_MODEL.md S5b/S5c conventions):
    S_src(x,y)   = sum_i F_i P_i(x,y)          Moffat(beta=2.5) sources
    sigma_slow^2 = smooth background variance map         (variance map IS smooth)
    sigma_w^2    = sigma_slow^2 + S_src/g                 (weighted face; source term in)
    SNR(x,y)     = S_src / sigma_w                        (numerator is source only)
Representation contract under test: a sparse control grid with spacing Delta can only
carry structure at scales >= Delta, so control values are the CELL-AGGREGATED model
quantities (the same aggregation the 8x8-patch pipeline performs), and the dense
reconstruction target T(x,y) is the cell-level SNR field. The pointwise PSF-scale SNR
is outside the representation domain -- measured and reported as the honest boundary.

Arms (all consume w = SNR_hat^2 / F_ref^2 downstream, the single weight formula):
    A_full        control values carry the exact cell model SNR (representation test)
    A_bglimit     control values carry the background-limited SNR S_cell/sigma_slow
                  (source term dropped from sigma_w) -- mis-modelling quantification
    A_zerosource  S_src == 0 -- negative control: every metric must collapse to zero
    A_frame_recon frame_reconstruct caliber: one frame scalar tiled (no-sparse-layer
                  fallback; same weight formula, degraded by construction)
Two scenes: primary = moderate (2 dex) cell-SNR contrast (gates); extreme = 4 dex
(boundary findings only, prefixed extreme_).

Downstream consumption (chain use-case):
    stacking estimate mu from heterogeneous pixels weighted by the reconstructed SNR
    (efficiency E = Var_w/Var_opt - 1 against the cell-scale truth), plus a P5-style
    sky-plane fit in two weightings -- showing that SNR-derived weights are the WRONG
    face on blank-sky samples (background-variance face), which numerically supports
    the two-variance-faces discipline of NOISE_MODEL.md S1/S5c.

Pure python + numpy. No repository imports. Seed hardcoded.
Run:  python3 exp_p4_04_brightness_forward.py
Out:  ../results/exp_p4_04_brightness_forward.json
"""
import json, time
import numpy as np

SEED = 20260926
OUT = "../results/exp_p4_04_brightness_forward.json"

GRID = 256
DELTA = 32
GAIN = 1.3            # e-/ADU (NOISE_MODEL.md S5c frozen convention)
FWHM = 6.0
BETA = 2.5
FREF = 100.0
N_CELLS = GRID // DELTA


def moffat(dy, dx, fwhm, beta):
    alpha = fwhm / (2.0 * np.sqrt(2.0 ** (1.0 / beta) - 1.0))
    r2 = dx ** 2 + dy ** 2
    return (beta - 1.0) / (np.pi * alpha ** 2) * (1.0 + r2 / alpha ** 2) ** (-beta)


def build_frame(seed, n_src=25, with_sources=True, flux_hi_dex=4.0):
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:GRID, 0:GRID].astype(float)
    sigma_slow2 = 25.0 * (1.0 + 0.5 * xx / (GRID - 1)) * (1.0 + 0.08 * np.sin(2 * np.pi * yy / GRID))
    src = np.zeros((GRID, GRID))
    if with_sources:
        for _ in range(n_src):
            F = 10 ** rng.uniform(2.0, flux_hi_dex)
            cx, cy = rng.uniform(8, GRID - 8, 2)
            src += F * moffat(yy - cy, xx - cx, FWHM, BETA)
    return {"xx": xx, "yy": yy, "src": src, "sigma_slow2": sigma_slow2}


def cell_aggregate(frame):
    """Cell-aggregated model quantities (the representation domain of the control grid)."""
    s_cell = frame["src"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    ss_cell = frame["sigma_slow2"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    sw_cell = ss_cell + s_cell / GAIN
    # Control-point semantics (sparse_snr_semantics = absolute_flux_type_snr):
    # the stored value is the ABSOLUTE reference-flux SNR  SNR_c = F_ref / sigma_F,c,
    # with sigma_F^2 = sigma_slow^2 + S_src/g  (source term INCLUDED). Brightness
    # enters through the denominator's source term.
    snr_cell = FREF / np.sqrt(sw_cell)
    snr_cell_bglim = FREF / np.sqrt(ss_cell)          # source term dropped (mis-model)
    T = np.repeat(np.repeat(snr_cell, DELTA, axis=0), DELTA, axis=1)
    sw2_map = np.repeat(np.repeat(sw_cell, DELTA, axis=0), DELTA, axis=1)
    return {"s_cell": s_cell, "ss_cell": ss_cell, "sw_cell": sw_cell,
            "snr_cell": snr_cell, "snr_cell_bglim": snr_cell_bglim,
            "T": T, "sw2_map": sw2_map}


def reconstruct(ctrl_values, delta=DELTA, op="spline"):
    import importlib.util, os
    spec = importlib.util.spec_from_file_location(
        "e02", os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "exp_p4_02_interpolators.py"))
    e02 = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(e02)
    yy, xx = np.mgrid[0:GRID, 0:GRID].astype(float)
    origin = float((delta - 1) // 2)
    if op == "spline":
        rec = e02.Spline2D(ctrl_values, delta, origin=origin)(xx.ravel(), yy.ravel())
    elif op == "bilinear":
        rec = e02.Bilinear2D(ctrl_values, delta, origin=origin)(xx.ravel(), yy.ravel())
    elif op == "idw":
        rec = e02.IDW2D(ctrl_values, delta, power=2.0, origin=origin)(xx.ravel(), yy.ravel())
    else:
        raise ValueError(op)
    rec = rec.reshape(GRID, GRID)
    lo, hi = ctrl_values.min(), ctrl_values.max()
    return np.clip(rec, lo, hi) if lo > 0 else np.maximum(rec, 0.0)


def node_grid(values, delta=DELTA):
    idx = np.arange(N_CELLS) * delta + (delta - 1) // 2
    g = np.asarray(values, float)
    assert g.shape == (N_CELLS, N_CELLS)
    return g, idx


def efficiency(w_used, v_true, eligible):
    w = w_used[eligible]
    v = v_true[eligible]
    var_w = np.sum(w ** 2 * v) / np.sum(w) ** 2
    var_opt = 1.0 / np.sum(1.0 / v)
    return float(var_w / var_opt - 1.0)


def arm_metrics(rec, T, sw2_map, em):
    m = {}
    with np.errstate(divide="ignore", invalid="ignore"):
        dex = np.abs(np.log10(np.where(rec > 0, rec, np.nan))
                     - np.log10(np.where(T > 0, T, np.nan)))
    dex = dex[em & np.isfinite(dex)]
    m["rmse_dex_vs_T"] = float(np.sqrt(np.mean(dex ** 2)))
    v_true = sw2_map                              # true variance of each pixel
    w = (rec / FREF) ** 2
    elig = em & np.isfinite(w) & (w > 0)
    m["E_stacking"] = efficiency(w, v_true, elig)
    q = lambda f, a: float(np.percentile(a, f))
    m["dynrange_T"] = [q(1, T[em]), q(99, T[em])]
    m["dynrange_recon"] = [q(1, rec[em]), q(99, rec[em])]
    m["dr_ratio_preserved"] = float(
        (m["dynrange_recon"][1] / max(m["dynrange_recon"][0], 1e-12))
        / max(m["dynrange_T"][1] / max(m["dynrange_T"][0], 1e-12), 1e-12))
    return m


def stack_usecase(rec_snr, agg, seed, n_px=3000, mu_true=50.0):
    """Estimate mu from pixel observations with heterogeneous true variances taken
    from the cell-aggregated weighted-variance map; weights from the reconstructed SNR."""
    rng = np.random.default_rng(seed)
    pick = rng.choice(np.arange(GRID * GRID), size=n_px, replace=False)
    v = agg["sw2_map"].ravel()[pick]
    x = rng.normal(mu_true, np.sqrt(v))
    snr_hat = rec_snr.ravel()[pick]
    elig = np.isfinite(snr_hat) & (snr_hat > 0)
    w = (snr_hat[elig] / FREF) ** 2
    mu_hat = np.sum(w * x[elig]) / np.sum(w)
    return {"mu_hat_rel_err": float(abs(mu_hat / mu_true - 1.0)),
            "E": efficiency((snr_hat / FREF) ** 2, v, elig)}


def sky_fit_usecase(frame, seed, rec_snr):
    """P5-style additive sky-plane fit on source-masked samples, two weightings:
    background inverse variance (correct face) vs SNR-derived weights (wrong face)."""
    rng = np.random.default_rng(seed)
    mask = frame["src"] < 0.01 * frame["src"].max()
    b_true = 12.0 + 0.02 * frame["xx"] / GRID - 0.015 * frame["yy"] / GRID
    flat_mask = mask.ravel() & (np.arange(GRID * GRID) % 7 == 0)
    idxs = np.array(np.where(flat_mask)[0])
    pick = rng.choice(idxs, size=min(4000, len(idxs)), replace=False)
    v = frame["sigma_slow2"].ravel()[pick]                 # background face variance
    obs = b_true.ravel()[pick] + rng.normal(0, np.sqrt(v))
    A = np.column_stack([np.ones(len(pick)), frame["xx"].ravel()[pick] / GRID,
                         frame["yy"].ravel()[pick] / GRID])
    out = {"snr_weight_median_at_sky": float(np.median(rec_snr.ravel()[pick]))}
    for name, wts in {"true_bg_ivar": 1.0 / v,
                      "snr_weight": (rec_snr.ravel()[pick] / FREF) ** 2}.items():
        WA = A * wts[:, None]
        coef, *_ = np.linalg.lstsq(WA, wts * obs, rcond=None)
        pred = A @ coef
        out[name] = {"b0_rel_err": float(abs(coef[0] / 12.0 - 1.0)),
                     "plane_rmse_adu": float(np.sqrt(np.mean((pred - b_true.ravel()[pick]) ** 2)))}
    return out


def run_scene(agg, agg0, em, seed, res, tag):
    g_full, _ = node_grid(agg["snr_cell"])
    g_bg, _ = node_grid(agg["snr_cell_bglim"])
    g_zero, _ = node_grid(agg0["snr_cell"])
    for name, g, T in (("A_full", g_full, agg["T"]),
                       ("A_bglimit", g_bg, agg["T"]),
                       ("A_zerosource", g_zero, agg0["T"])):
        rec = reconstruct(g)
        arm = {"max_abs_dev_vs_T": float(np.max(np.abs(rec - T)))}
        if name == "A_zerosource":
            # negative control: with no sources the source term is absent everywhere,
            # so the full and background-limited control grids coincide and the
            # brightness-carrying benefit must collapse to zero.
            g0_full, _ = node_grid(agg0["snr_cell"])
            g0_bg, _ = node_grid(agg0["snr_cell_bglim"])
            rec_f = reconstruct(g0_full)
            rec_b = reconstruct(g0_bg)
            arm["full_equals_bglim_max_abs_dev"] = float(np.max(np.abs(rec_f - rec_b)))
            arm["benefit_collapsed"] = bool(np.max(np.abs(rec_f - rec_b)) <= 1e-9)
            arm["E_full"] = arm_metrics(rec_f, T, agg0["sw2_map"], em)["E_stacking"]
            arm["E_bglim"] = arm_metrics(rec_b, T, agg0["sw2_map"], em)["E_stacking"]
        else:
            arm.update(arm_metrics(rec, T, agg["sw2_map"], em))
            arm["stack_usecase"] = stack_usecase(rec, agg, seed + 1)
        res["arms"][tag + name] = arm
    frame_scalar = float(np.median(g_full[g_full > 0])) if np.any(g_full > 0) else 0.0
    rec_fr = np.full((GRID, GRID), frame_scalar)
    arm_fr = {"frame_scalar_snr": frame_scalar}
    arm_fr.update(arm_metrics(rec_fr, agg["T"], agg["sw2_map"], em))
    arm_fr["stack_usecase"] = stack_usecase(rec_fr, agg, seed + 1)
    arm_fr["note"] = ("frame_reconstruct caliber: constant tiled field "
                      "(reconstruction manner, same weight formula)")
    res["arms"][tag + "A_frame_recon"] = arm_fr
    return reconstruct(g_full)


def main():
    t0 = time.time()
    res = {"seed": SEED, "grid": GRID, "delta": DELTA, "gain_e_per_adu": GAIN,
           "n_cells": N_CELLS, "arms": {}}
    em = np.zeros((GRID, GRID), bool)
    em[::2, ::2] = True

    # ---- primary scene: moderate (2 dex) cell-SNR contrast ----
    frame = build_frame(SEED, flux_hi_dex=4.0)
    frame0 = build_frame(SEED, with_sources=False)
    agg = cell_aggregate(frame)
    agg0 = cell_aggregate(frame0)
    rec_full = run_scene(agg, agg0, em, SEED, res, "")

    # ---- boundary scene: extreme (4 dex) contrast, reported as honest boundary ----
    agg_x = cell_aggregate(build_frame(SEED + 50, flux_hi_dex=6.0))
    agg_x0 = cell_aggregate(build_frame(SEED + 50, with_sources=False))
    run_scene(agg_x, agg_x0, em, SEED + 50, res, "extreme_")
    res["extreme_contrast_note"] = ("extreme_* arms: 4-dex cell-SNR contrast pushes "
                                    "linear-space interpolation to its representation limit; "
                                    "numbers are boundary findings, not gates")

    # representational honesty boundary: pointwise PSF-scale SNR vs the cell field
    sigma_w2_pw = frame["sigma_slow2"] + frame["src"] / GAIN
    with np.errstate(divide="ignore", invalid="ignore"):
        snr_pw = np.where(frame["src"] > 0, frame["src"] / np.sqrt(sigma_w2_pw), 0.0)
    with np.errstate(divide="ignore", invalid="ignore"):
        gap = np.abs(np.log10(np.where(snr_pw > 0, snr_pw, np.nan))
                     - np.log10(np.where(agg["T"] > 0, agg["T"], np.nan)))
    res["representation_boundary"] = {
        "note": ("pointwise PSF-scale SNR deviates from the cell-aggregated target by "
                 "this much; the sparse control grid does NOT represent this component "
                 "(validity domain: fields smooth on scale >= Delta)"),
        "median_dex": float(np.nanmedian(gap)),
        "p99_dex": float(np.nanpercentile(gap, 99)),
    }

    # P5-side sky fit: SNR weights are the wrong face on blank-sky samples
    res["sky_fit_twofaces"] = sky_fit_usecase(frame, SEED + 2, rec_full)

    # information test at oracle level: the weight formula is exactly optimal IFF the
    # SNR carries the source term. E(algebraic w=1/sw2) must be machine zero;
    # E(bglim weights) > 0 quantifies what dropping the source term costs.
    rng = np.random.default_rng(SEED + 5)
    pick = rng.choice(np.arange(GRID * GRID), size=4000, replace=False)
    v_pix = agg["sw2_map"].ravel()[pick]
    w_oracle_full = 1.0 / v_pix
    w_oracle_bglim = 1.0 / agg["ss_cell"].repeat(DELTA, axis=0).repeat(DELTA, axis=1).ravel()[pick]
    res["oracle_information_test"] = {
        "E_oracle_full": efficiency(w_oracle_full, v_pix, np.ones(len(pick), bool)),
        "E_oracle_bglim": efficiency(w_oracle_bglim, v_pix, np.ones(len(pick), bool)),
        "E_equal": efficiency(np.ones(len(pick)), v_pix, np.ones(len(pick), bool)),
    }

    a_full = res["arms"]["A_full"]
    res["gates"] = {
        "no_source_benefit_collapses":
            res["arms"]["A_zerosource"]["benefit_collapsed"]
            and abs(res["arms"]["A_zerosource"]["E_full"]
                    - res["arms"]["A_zerosource"]["E_bglim"]) < 1e-9,
        "full_beats_bglimit_efficiency":
            a_full["E_stacking"] < res["arms"]["A_bglimit"]["E_stacking"],
        "full_beats_frame_recon_rmse":
            a_full["rmse_dex_vs_T"] < res["arms"]["A_frame_recon"]["rmse_dex_vs_T"],
        "brightness_structure_carried": a_full["dr_ratio_preserved"] > 0.5,
        "sky_fit_snr_weight_tracks_bg_ivar":
            max(res["sky_fit_twofaces"]["snr_weight"]["b0_rel_err"], 1e-12)
            < 10.0 * max(res["sky_fit_twofaces"]["true_bg_ivar"]["b0_rel_err"], 1e-12),
        "oracle_full_is_optimal":
            abs(res["oracle_information_test"]["E_oracle_full"]) < 1e-12,
        "oracle_bglim_suboptimal": res["oracle_information_test"]["E_oracle_bglim"] > 5e-4,
    }
    res["all_gates_pass"] = bool(all(res["gates"].values()))
    res["runtime_s"] = time.time() - t0
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
