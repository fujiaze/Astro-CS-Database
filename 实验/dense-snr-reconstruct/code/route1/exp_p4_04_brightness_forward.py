#!/usr/bin/env python3
"""EXP-P4-04: Brightness-forward validation of dense-SNR reconstruction and its
downstream (P5-side) consumption -- the chain use-case.

Audit items I8 (brightness forward constraint: docs/ASTROCS_DESIGN S2.4 /
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
import os
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "results", "route1", "exp_p4_04_brightness_forward.json")

GRID = 256
DELTA = 32
GAIN = 1.3            # e-/ADU (NOISE_MODEL.md S5c frozen convention)
FWHM = 6.0
BETA = 2.5
FREF = 100.0
N_CELLS = GRID // DELTA

# ---------------------------------------------------------------- G08-05 B1/B4
# **增益 g 在本实验内部不可辨识**（可辨识性论证，不是阈值问题）：
# 本文件存储的每一个量都只是 (sigma_slow², S/g) 的函数——S [ADU] 与 g [e-/ADU]
# 只以比值 S/g 出现。任何「独立重算」（含 variance_map_matches_physical_model）
# 都只独立于 aggregation 步骤，**不独立于 g 这个物理常量本身**：两侧同错。
# 能打破该盲区的只有**外部参照**。下面两条外部参照都与本文件的 GAIN 常量不同源：
#   ARCHIVE_SELF —— 本单元已固化的归档一手读数（另一次执行的产物，运行时从磁盘读，
#                   不在本进程重算，故不共享 GAIN）；
#   ARCHIVE_GAIN —— 另一实验单元（absolute-snr / EXP-06）归档的一手增益读数。
# 两者任一缺失 ⇒ 对应判据判红（fail-closed），不得静默转绿。
_HERE = os.path.dirname(os.path.abspath(__file__))
ARCHIVE_SELF = os.path.join(_HERE, "..", "..", "results", "route1",
                            "exp_p4_04_brightness_forward.json")
ARCHIVE_GAIN = os.path.join(_HERE, "..", "..", "..", "absolute-snr", "results",
                            "exp06_e4_gates.json")
# 归档重放的相对容差。实测注入（g -> 1/g）使各读数偏移 0.58%~76%（见
# gain_inversion_sensitivity），比该容差宽 4 个数量级以上 ⇒ 判据对注入稳健判红。
REPLAY_TOL = 1e-6


def load_json(p):
    try:
        with open(p, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def dynrange_T_at_gain(frame, em, gain):
    """给定增益下 A_full 的 dynrange_T —— 与 arm_metrics() 的 q(1,T[em])/q(99,T[em])
    逐字同口径（同 DELTA 铺开、同 em 掩膜、同分位），只把 g 参数化。
    供灵敏度探针使用：保证「注入下的读数」与「重放判据比对的读数」是同一个量。"""
    s_cell = frame["src"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    ss_cell = frame["sigma_slow2"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    T = FREF / np.sqrt(ss_cell + s_cell / gain)
    Tt = np.repeat(np.repeat(T, DELTA, axis=0), DELTA, axis=1)
    return [float(np.percentile(Tt[em], 1)), float(np.percentile(Tt[em], 99))]


def absolute_readings(arm):
    """臂的**绝对**读数（携带绝对尺度与增益信息的量；不含任何归一化/齐次量）。
    缺失键按缺登记、不静默补值——A_frame_recon 没有 max_abs_dev_vs_T。"""
    keys = ("dynrange_T_p1", "dynrange_T_p99", "dynrange_recon_p1", "dynrange_recon_p99",
            "rmse_dex_vs_T", "max_abs_dev_vs_T", "E_stacking", "dr_ratio_preserved")
    src = {}
    src["dynrange_T_p1"] = arm.get("dynrange_T", [None])[0]
    src["dynrange_T_p99"] = arm.get("dynrange_T", [None, None])[1]
    src["dynrange_recon_p1"] = arm.get("dynrange_recon", [None])[0]
    src["dynrange_recon_p99"] = arm.get("dynrange_recon", [None, None])[1]
    src["rmse_dex_vs_T"] = arm.get("rmse_dex_vs_T")
    src["max_abs_dev_vs_T"] = arm.get("max_abs_dev_vs_T")
    src["E_stacking"] = arm.get("E_stacking")
    src["dr_ratio_preserved"] = arm.get("dr_ratio_preserved")
    return {k: (float(src[k]) if src[k] is not None else None) for k in keys}


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
    # SNR carries the source term. E(bglim weights) > 0 quantifies what dropping the
    # source term costs.
    #
    # G08-04 整改 T6 —— E(algebraic w=1/sw2) **不是**判据，是代数恒等式：
    #   w_oracle_full = 1/v_pix，而 efficiency() 的 v_true 收到的**就是同一个 v_pix**，
    #   故 var_w = Σ(1/v)/(Σ1/v)² = 1/Σ(1/v) = var_opt ⇒ E ≡ 0 对**任意**正方差数组成立。
    #   该等式对 w 的全局缩放还不变（efficiency 齐次），所以整条射线 {c/v} 都给 0。
    #   它只验证公式自洽，**不携带**任何关于 sw2_map 是否正确的信息。
    #   原 :294-295 的 gate `oracle_full_is_optimal` 因而是恒真门（实测注入 sw2_map
    #   面积×2/×0.5/×1e9/×1e18、抹平、量纲颠倒，E 恒为 0 或 1 ulp，门恒绿），
    #   已从 gates 中**撤下**，改记入 algebraic_identity_checks（仅作恒等式自检）。
    #   真正有判别力的替代表述见下方 gates：手算闭式 + 非均匀扰动灵敏度。
    rng = np.random.default_rng(SEED + 5)
    pick = rng.choice(np.arange(GRID * GRID), size=4000, replace=False)
    v_pix = agg["sw2_map"].ravel()[pick]
    w_oracle_full = 1.0 / v_pix
    w_oracle_bglim = 1.0 / agg["ss_cell"].repeat(DELTA, axis=0).repeat(DELTA, axis=1).ravel()[pick]
    E_oracle_full = efficiency(w_oracle_full, v_pix, np.ones(len(pick), bool))
    E_bglim = efficiency(w_oracle_bglim, v_pix, np.ones(len(pick), bool))
    E_equal = efficiency(np.ones(len(pick)), v_pix, np.ones(len(pick), bool))
    # 独立闭式（不与 efficiency 共享中间量）：E = mean(v)*mean(1/v) - 1（等权情形）
    E_equal_closed = float(np.mean(v_pix) * np.mean(1.0 / v_pix) - 1.0)
    # 非均匀、零均值扰动的 oracle 权重：E 不再是恒等式，可预测、可证伪
    delta = 0.02 * np.cos(np.arange(len(pick)) * (2.0 * np.pi / 16.0))   # 确定性、零均值
    w_oracle_pert = w_oracle_full * (1.0 + delta)
    E_oracle_pert = efficiency(w_oracle_pert, v_pix, np.ones(len(pick), bool))
    S0 = float(np.sum(1.0 / v_pix)); S1 = float(np.sum(delta / v_pix))
    S2 = float(np.sum(delta * delta / v_pix))
    E_oracle_pert_closed = float(S0 * (S0 + 2.0 * S1 + S2) / (S0 + S1) ** 2 - 1.0)
    res["oracle_information_test"] = {
        "E_oracle_full": E_oracle_full,
        "E_oracle_bglim": E_bglim,
        "E_equal": E_equal,
        "E_oracle_perturbed": E_oracle_pert,
        "E_oracle_perturbed_closed_form": E_oracle_pert_closed,
        "E_equal_closed_form": E_equal_closed,
        "perturbation_delta_rms": float(np.sqrt(np.mean(delta * delta))),
        "dilute_limit_prediction_mean_delta2": float(np.mean(delta * delta)),
        "portability": (
            "E_oracle_full 已撤出 gates：它是 w=1/v 与 v_true=v 的代数恒等式，"
            "只验证公式自洽，不构成 sw2_map 正确性的证据。代价量化由 E_oracle_bglim 承担。"),
    }
    res["algebraic_identity_checks"] = {
        "E_oracle_full_machine_zero": E_oracle_full,
        "tolerance": 1e-12,
        "note": ("代数恒等式自检：w=1/v 且 v_true=v ⇒ var_w ≡ var_opt ⇒ E ≡ 0，"
                 "对任意正方差数组成立、且对 w 的全局缩放不变（efficiency 齐次）。"
                 "**不得**作为实现正确性的证据；保留它只为确认公式定义无误。"),
    }

    # --- 手算闭式：对 efficiency() 本身做单元校验（与被测代码零共享） ---
    # v = [1,1,1,4]，n=4。等权时 E = mean(v)*mean(1/v) - 1 = 1.75*0.8125 - 1 = 0.421875
    # 反最优 w = v 时 E = (Σv³)(Σ1/v)/(Σv)² - 1 = 67*3.25/49 - 1 = 3.443877551020408
    hand_v = np.array([1.0, 1.0, 1.0, 4.0])
    hand_E_equal = efficiency(np.ones(4), hand_v, np.ones(4, bool))
    hand_E_anti = efficiency(hand_v, hand_v, np.ones(4, bool))
    hand_E_oracle = efficiency(1.0 / hand_v, hand_v, np.ones(4, bool))
    res["efficiency_metric_handcheck"] = {
        "v": hand_v.tolist(),
        "E_equal_meas": float(hand_E_equal), "E_equal_hand": 0.421875,
        "E_antioptimal_meas": float(hand_E_anti),
        "E_antioptimal_hand": 3.443877551020408,
        "E_oracle_identity_meas": float(hand_E_oracle), "E_oracle_identity_hand": 0.0,
        "criterion": "手算闭式：等权 0.421875、反最优 3.443877551020408、oracle 恒等式 0",
    }

    # --- 走 E 的判据对 sw2_map 的两类结构性盲区，补一条**不走 E**的物理自洽判据 ---
    # E 对 v 齐次（var_w 与 var_opt 同比例放大，比值不变），且 E_equal 对 v -> 1/v
    # 不变（mean(v)*mean(1/v) 是对称的）。故 sw2_map 的「全局缩放」与「量纲颠倒」
    # 在 oracle 段**原理上不可观测**。这里直接按物理式独立重算 sw2_map 并比对。
    s_cell_chk = frame["src"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    ss_cell_chk = frame["sigma_slow2"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    sw2_recomputed = np.repeat(
        np.repeat(ss_cell_chk + s_cell_chk / GAIN, DELTA, axis=0), DELTA, axis=1)
    sw2_scale = float(np.mean(sw2_recomputed))
    sw2_rel_dev = float(np.max(np.abs(agg["sw2_map"] - sw2_recomputed)) / sw2_scale)
    res["variance_map_selfcheck"] = {
        "definition": "sw2_map = blockmean(sigma_slow2) + blockmean(src)/GAIN，按 DELTA×DELTA 铺开",
        "recomputed_independently_from_frame": True,
        "mean_sw2": sw2_scale,
        "max_rel_dev": sw2_rel_dev,
        "criterion": "max|sw2_map - 独立重算| / mean < 1e-12（不走 E，避开 E 的齐次盲区）",
    }

    a_full = res["arms"]["A_full"]

    # ---------------- G08-05 B1/B4：外部参照判据（打破增益盲区） ----------------
    # ① 归档重放：逐条比对本进程重算的**绝对**读数与归档一手读数。
    #    参照量在磁盘上、来自另一次执行，不在本进程重算 ⇒ 不共享 GAIN。
    arch = load_json(ARCHIVE_SELF)
    replay_rows, replay_ok = [], True
    if arch is None:
        replay_ok = False
        replay_rows.append({"error": "archive_unreadable", "path": ARCHIVE_SELF})
    else:
        for arm_name in ("A_full", "A_bglimit", "A_frame_recon"):
            live, ref = absolute_readings(res["arms"][arm_name]), \
                absolute_readings(arch["arms"][arm_name])
            for k in live:
                if live[k] is None or ref[k] is None:
                    replay_rows.append({"arm": arm_name, "key": k, "skipped": "absent_in_one_side",
                                        "live": live[k], "archived": ref[k]})
                    continue
                d = abs(live[k] - ref[k]) / max(abs(ref[k]), 1e-300)
                replay_rows.append({"arm": arm_name, "key": k, "live": live[k],
                                    "archived": ref[k], "rel_dev": d,
                                    "ok": bool(d <= REPLAY_TOL)})
                replay_ok = replay_ok and d <= REPLAY_TOL
    res["absolute_scale_replay_vs_archive"] = {
        "archive_path": ARCHIVE_SELF,
        "archive_gain_e_per_adu": (arch or {}).get("gain_e_per_adu"),
        "archive_gate_names": sorted((arch or {}).get("gates", {}).keys()),
        "tolerance_rel": REPLAY_TOL,
        "reference_independence": "参照量取自磁盘归档（另一次执行），不在本进程重算，"
                                  "因此不共享本文件的 GAIN 常量；E 对 v 齐次、对绝对尺度无判别力，"
                                  "故重放对象只取携带绝对尺度的读数。",
        "rows": replay_rows,
        "pass": bool(replay_ok),
        "fail_closed_note": "归档缺失或不可读 ⇒ 判红，不静默转绿。",
    }

    # ② 增益常量溯源：与**另一实验单元**归档的一手增益读数对拍。
    ga = load_json(ARCHIVE_GAIN)
    g7 = ((ga or {}).get("detail", {}) or {}).get("G7_gain_recovery") or []
    g_ref = sorted({float(r["gain_true"]) for r in g7 if "gain_true" in r})
    gain_ok = bool(len(g_ref) == 1 and abs(GAIN - g_ref[0]) <= 1e-12 * g_ref[0])
    res["gain_constant_provenance"] = {
        "reference_archive": ARCHIVE_GAIN,
        "reference_gain_e_per_adu": g_ref,
        "script_gain_e_per_adu": float(GAIN),
        "rel_dev": (abs(GAIN - g_ref[0]) / g_ref[0]) if len(g_ref) == 1 else None,
        "what_this_is": "冻结常量的**跨单元溯源对拍**，不是从第一性原理导出增益。"
                        "本实验内部不可辨识 g（见文件头论证），外部一手读数是唯一能定住它的东西。",
        "pass": gain_ok,
        "fail_closed_note": "参照归档缺失或不含恰好一个增益值 ⇒ 判红。",
    }

    # ③ 灵敏度/覆盖门：审稿注入 g -> 1/g（量纲颠倒）必须被上面的重放判据抓住。
    #    这条门自身不判「数值对不对」，只判「整套判据对该缺陷是活的」——
    #    未加①②时它恒红（实测 0 条重放读数超差），是覆盖面被高估的机器证据。
    T_inv = dynrange_T_at_gain(frame, em, 1.0 / GAIN)
    live_t = res["arms"]["A_full"]["dynrange_T"]
    ref_t = absolute_readings(arch["arms"]["A_full"]) if arch else None
    inv_devs = ([abs(T_inv[0] - live_t[0]) / max(abs(live_t[0]), 1e-300),
                 abs(T_inv[1] - live_t[1]) / max(abs(live_t[1]), 1e-300)] if ref_t else
                [float("inf"), float("inf")])
    sens_ok = bool(max(inv_devs) > REPLAY_TOL)
    res["gain_inversion_sensitivity"] = {
        "injection": "GAIN -> 1/GAIN（把 S/g 当成 S·g，量纲颠倒）",
        "gain_injected": float(1.0 / GAIN),
        "dynrange_T_live": list(map(float, live_t)),
        "dynrange_T_under_injection": T_inv,
        "rel_dev_live_vs_injected": inv_devs,
        "tolerance_rel": REPLAY_TOL,
        "margin_ratio": (max(inv_devs) / REPLAY_TOL) if sens_ok else None,
        "pass": sens_ok,
        "meaning": "True ⇒ 重放判据所比对的 dynrange_T 读数在量纲颠倒注入下偏移超出容差 "
                   "（即该注入必然使 absolute_scale_matches_archived_first_hand 判红）。"
                   "这是覆盖面门（判据是否活），不是物理正确性门。",
    }

    res["gates"] = {
        # **构造性恒真门（G08-04 整改 R2 订正归因）**：本 gate **不得**引用为
        # 「sw2_map 缺陷被抓」的证据。零源臂里 with_sources=False ⇒ src ≡ 0 ⇒
        # s_cell ≡ 0 ⇒ sw_cell ≡ ss_cell ⇒ snr_cell 与 snr_cell_bglim **逐位相同**
        # ⇒ 两臂经**同一个** reconstruct() 输出的差恒 0、E_full ≡ E_bglim。
        # 实测（8 个注入变体）：凡缺陷落在**两臂共享的输入**上（sigma_slow2 x2、
        # GAIN 取倒数、sw2 整体缩放/抹平/取倒数的上游量），本 gate **恒绿**且
        # full_equals_bglim_max_abs_dev 逐位为 0.0；只有当注入**只改 sw_cell 一侧**
        # （使 snr_cell 与 snr_cell_bglim 的分母不再同源）时它才翻转。
        # ⇒ 它只能作「两臂同源」的构造回归守卫；sw2_map 的判别力由本文件里
        # 不走 E 的 variance_map_matches_physical_model 承担。
        # 归因订正见 实验/dense-snr-reconstruct/REPORT_paper.md §7.23b。
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
        "oracle_bglim_suboptimal": res["oracle_information_test"]["E_oracle_bglim"] > 5e-4,
        # --- G08-04 T6 替换进来的三条真判据（替代恒真的 oracle_full_is_optimal）---
        # (1) efficiency() 本身对手算闭式的单元校验：能抓度量实现的错（分母用错、
        #     v 取错、平方漏掉…），且与被测代码零共享计算路径。
        "efficiency_metric_handcheck":
            abs(hand_E_equal - 0.421875) < 1e-12
            and abs(hand_E_anti - 3.443877551020408) < 1e-12
            and abs(hand_E_oracle) < 1e-12,
        # (2) 等权在**实际** sw2_map 上必须次优，且等于闭式。该量对 sw2_map 的
        #     **内容**敏感：方差面被抹平（v 变常数）时 E_equal → 0（判红）；
        #     源项被丢掉（方差面反差消失）时 E_equal 明显变小（判红）。
        "equal_weights_suboptimal_on_actual_variance_map":
            E_equal > 1e-3 and abs(E_equal - E_equal_closed) < 1e-9 * max(1.0, abs(E_equal)),
        # (3) oracle 权重的**非均匀**扰动必须被度量捕捉，且幅值等于闭式预测。
        #     恒等式只在 w ∝ 1/v 时成立；一旦逐像素偏离，E 立刻变成可预测的非零量。
        "oracle_weight_perturbation_detected":
            E_oracle_pert > 0.0
            and abs(E_oracle_pert - E_oracle_pert_closed)
                < 1e-6 * max(1.0, abs(E_oracle_pert_closed))
            and 0.5 * float(np.mean(delta * delta)) <= E_oracle_pert
                <= 2.0 * float(np.mean(delta * delta)),
        # (4) 方差面物理自洽（不走 E）：E 对 v 齐次、且 E_equal 对 v->1/v 不变，
        #     故「全局缩放」「量纲颠倒」在 oracle 段原理上不可观测；此条按物理式
        #     独立重算 sw2_map 来抓这两类缺陷。
        #     **G08-05 B1 覆盖范围订正**：本条的「独立」只独立于 aggregation 步骤，
        #     **不独立于 GAIN 常量本身**——重算用的是同一个 GAIN 与同一个 frame，
        #     与聚合侧逐项同构，属「两侧同错」。实测注入 g -> 1/g 时 max_rel_dev 逐位
        #     为 0.0、本条恒绿。**不得**再引用本条作为「抓得住量纲颠倒/增益错误」的
        #     证据；该职责已移交本文件末尾的 absolute_scale_matches_archived_first_hand
        #     与 gain_matches_independent_archived_reading（外部参照，与 GAIN 不同源）。
        "variance_map_matches_physical_model": sw2_rel_dev < 1e-12,
        # --- G08-05 B1/B4：三条**外部参照**判据（打破增益/绝对尺度盲区）---
        # 上面的 variance_map_matches_physical_model 虽「独立重算」，但用的是**同一个
        # GAIN 常量**与同一个 frame，与聚合侧逐项同构 ⇒ 两侧同错，只能抓 aggregation
        # 步骤的错误，抓不到 g 本身。审稿注入 g -> 1/g 时本文件原 10 条门全绿（实测 0/10）。
        # 下面三条用**磁盘上的外部一手参照**补上这块（参照量与被检验量不同源）：
        "absolute_scale_matches_archived_first_hand": bool(replay_ok),
        "gain_matches_independent_archived_reading": bool(gain_ok),
        "gain_injection_is_detected_by_this_suite": bool(sens_ok),
    }
    # B4②：归档与脚本脱钩的**登记**（不重跑、不改 results/）。归档 JSON 是另一次
    # 执行的产物，其门集合/键集合与本脚本当前不同；报告侧若按「逐叶全同」表述，
    # 对 exp04 不成立。此处只登记事实，处置由归档车道负责。
    script_keys = sorted(res.keys())
    res["archive_decoupling_registration"] = {
        "archive_path": ARCHIVE_SELF,
        "archive_exists": arch is not None,
        "archive_top_level_key_count": len(arch) if arch else None,
        "script_top_level_key_count": len(script_keys),
        "archive_only_keys": sorted(set(arch.keys()) - set(script_keys)) if arch else None,
        "script_only_keys": sorted(set(script_keys) - set(arch.keys())) if arch else None,
        "archive_only_gates": sorted(set((arch or {}).get("gates", {}).keys())
                                     - set(res["gates"].keys())),
        "script_only_gates": sorted(set(res["gates"].keys())
                                    - set((arch or {}).get("gates", {}).keys())),
        "withdrawn_gate_still_in_archive": bool(
            arch and "oracle_full_is_optimal" in arch.get("gates", {})),
        "note": "results/ 不在本单写入面，故只登记不重跑。归档与脚本已脱钩：归档缺 "
                "variance_map_matches_physical_model 等新门，且仍含已撤回的 "
                "oracle_full_is_optimal。报告侧「19/19 逐叶全同」对 exp04 不成立。",
    }
    res["gates_note"] = (
        "G08-04 T6：原 gate `oracle_full_is_optimal`（|E(1/v)| < 1e-12）是代数恒等式、"
        "恒真且无判别力，已撤下并改记入 algebraic_identity_checks。"
        "现由 efficiency_metric_handcheck / equal_weights_suboptimal_on_actual_variance_map / "
        "oracle_weight_perturbation_detected 三条承担 oracle 段的判别力；"
        "原 oracle_bglim_suboptimal 判别力真实，保留不变。"
        "G08-05 B1/B4：variance_map_matches_physical_model 与前三条**都不独立于 GAIN**"
        "（重算复用同一 GAIN 常量与同一 frame，两侧同错），故它们对 g -> 1/g 恒绿；"
        "该盲区由三条外部参照判据承担：absolute_scale_matches_archived_first_hand（归档重放，"
        "参照量在磁盘上、来自另一次执行）、gain_matches_independent_archived_reading"
        "（跨单元增益溯源）、gain_injection_is_detected_by_this_suite（覆盖面门：断言前两条"
        "对量纲颠倒注入是活的）。")
    res["all_gates_pass"] = bool(all(res["gates"].values()))
    res["runtime_s"] = time.time() - t0
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
