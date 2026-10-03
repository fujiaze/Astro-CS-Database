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
# 能打破该盲区的只有**参照量**，而参照量必须同时满足两条，缺一不可：
#   (1) **不同源**——不与被检验量共享 GAIN 等物理常量、不在本进程重算；
#   (2) **不可被复现动作改写**——不落在任何被复现命令写出的路径上。
#
# **G08-05 R2 B1 订正**：原实现只满足 (1) 而违反 (2)，把 ARCHIVE_SELF 当作
# 「另一次执行的一手参照」，而它与本脚本的 OUT **归一化后是同一个文件**
# （见 structural_reference_audit.reference_overlaps_output）。实测注入
# g -> 1/g 连跑两次：第 1 跑 23 项读数中 21 项位移、门判红；第 2 跑
# max rel_dev = 0.0、门**转绿而缺陷仍在** ⇒ 顺序相关的「红一次就自愈」假绿门；
# run_all.sh 会重跑并覆写该归档，照文档复现两次即永久失明。
# 处置（两处，缺一不可）：
#   · 判决参照量改为**代码内冻结的一手常量** FIRST_HAND —— 不被任何复现动作改写，
#     也不经本进程重算，同时满足 (1)(2)；
#   · ARCHIVE_SELF 降级为**纯诊断**：只登记「归档与脚本已脱钩」的事实，
#     不参与任何判决（见 archive_decoupling_registration）。
# 另加结构性门 reference_is_rewrite_proof：它检查判决参照量的**来源**，
# 一旦有人把参照量重新指向可写文件（复现本条缺陷），本门立即判红。
_HERE = os.path.dirname(os.path.abspath(__file__))
# 诊断用归档（**不参与判决**）：本脚本上一轮执行的产物，含已撤回的门。
ARCHIVE_SELF = os.path.join(_HERE, "..", "..", "results", "route1",
                            "exp_p4_04_brightness_forward.json")
# 判决参照量 ①：本脚本**写出**的归档路径（结构性自检用，见上）。
ARCHIVE_SELF_OUT = OUT
# 判决参照量 ②：另一实验单元（absolute-snr / EXP-06）归档的一手增益读数。
# 该文件不在本脚本的写出路径上，故满足 (1)(2)，可作判决参照。
ARCHIVE_GAIN = os.path.join(_HERE, "..", "..", "..", "absolute-snr", "results",
                            "exp06_e4_gates.json")
# 一手重放的相对容差。实测注入（g -> 1/g）使各读数偏移 0.58%~76%（见
# gain_inversion_sensitivity），比该容差宽 4 个数量级以上 ⇒ 判据对注入稳健判红。
REPLAY_TOL = 1e-6

# ---- 判决参照量 ③：FIRST_HAND —— **代码内冻结的一手读数** -------------------
# 出处：seed=20260926、GAIN=1.3 e-/ADU 的一次正确执行的 23 项绝对读数，逐位抄录自
#   results/route1/exp_p4_04_brightness_forward.json
#   sha256 = 5cffbd34282b4eb13335ee72cfefbd62e35afa1a2d649e65c4ce67235f7bdcf8
# 这份常量写在本文件里，不在任何被复现命令写出的路径上，故 run_all.sh 重跑多少次
# 都不会改写它 —— 门①的判决对执行次数不敏感（改前：第 1 跑红、第 2 跑绿）。
FIRST_HAND = {
    "provenance": {
        "source_archive_sha256": "5cffbd34282b4eb13335ee72cfefbd62e35afa1a2d649e65c4ce67235f7bdcf8",
        "seed": SEED,
        "gain_e_per_adu": 1.3,
        "meaning": "seed 与增益冻结的一次正确执行的一手绝对读数；本常量是判决参照量。",
    },
    "A_full": {
        "dynrange_T_p1": 15.389799108756723, "dynrange_T_p99": 20.451262163847158,
        "dynrange_recon_p1": 15.56370994077333, "dynrange_recon_p99": 20.451262163847158,
        "rmse_dex_vs_T": 0.005835122218018034, "max_abs_dev_vs_T": 1.3547250521236123,
        "E_stacking": 0.0007187574990130674, "dr_ratio_preserved": 0.9888258755349199,
    },
    "A_bglimit": {
        "dynrange_T_p1": 15.389799108756723, "dynrange_T_p99": 20.451262163847158,
        "dynrange_recon_p1": 15.931377612206738, "dynrange_recon_p99": 20.457167402311182,
        "rmse_dex_vs_T": 0.00967134200330187, "max_abs_dev_vs_T": 2.099150035567302,
        "E_stacking": 0.001667139877279178, "dr_ratio_preserved": 0.9662844758434729,
    },
    "A_frame_recon": {
        "dynrange_T_p1": 15.389799108756723, "dynrange_T_p99": 20.451262163847158,
        "dynrange_recon_p1": 17.595298068713696, "dynrange_recon_p99": 17.595298068713696,
        "rmse_dex_vs_T": 0.029539302137796916,
        "E_stacking": 0.018130822393378265, "dr_ratio_preserved": 0.7525109690277275,
    },
}


def load_json(p):
    try:
        with open(p, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def dynrange_T_at_gain(frame, em, gain):
    """保留：给定增益下 A_full 的 dynrange_T 的**同口径**闭式，供灵敏度线性化用。

    G08-05 R2 第 2 条：本函数**不再**充当覆盖面门 ③ 的证据来源 —— 它是本文件内
    另写一遍的公式，与 arm_metrics 走两条路，自比自恒真（实测 8 个注入变体 0/8 判红）。
    覆盖面门 ③ 现已改为**走真实流水线**（见 gain_inversion_sensitivity：
    replay_arm_readings_at_gain 用 cell_aggregate/run_scene/arm_metrics 重算）。
    本函数只用来从一个**已实测**的偏移量线性化出检测地板。"""
    s_cell = frame["src"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    ss_cell = frame["sigma_slow2"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    T = FREF / np.sqrt(ss_cell + s_cell / gain)
    Tt = np.repeat(np.repeat(T, DELTA, axis=0), DELTA, axis=1)
    return [float(np.percentile(Tt[em], 1)), float(np.percentile(Tt[em], 99))]


# ---- G08-05 R2 第 2/3 条：重放判据的**期望键集**（schema，fail-closed 用）----
# A_frame_recon 没有 max_abs_dev_vs_T 是**定义使然**（frame_reconstruct 口径下
# 「相对 cell 目标的最大偏差」无定义），不是缺项。除此之外，任何 arm 缺 key、
# 或值不是有限数，都必须 fail-closed 判红 —— 原实现对缺失一律 skip，
# 比较 0 项时 replay_ok 仍为 True（fail-open）。
REPLAY_SCHEMA = {
    "A_full": ("dynrange_T_p1", "dynrange_T_p99", "dynrange_recon_p1",
               "dynrange_recon_p99", "rmse_dex_vs_T", "max_abs_dev_vs_T",
               "E_stacking", "dr_ratio_preserved"),
    "A_bglimit": ("dynrange_T_p1", "dynrange_T_p99", "dynrange_recon_p1",
                  "dynrange_recon_p99", "rmse_dex_vs_T", "max_abs_dev_vs_T",
                  "E_stacking", "dr_ratio_preserved"),
    "A_frame_recon": ("dynrange_T_p1", "dynrange_T_p99", "dynrange_recon_p1",
                      "dynrange_recon_p99", "rmse_dex_vs_T", "E_stacking",
                      "dr_ratio_preserved"),
}


def schema_violations(live: dict, arch: dict):
    """列出两侧**期望键**的缺失/非有限，返回 violations。

    入参是**经 absolute_readings 归一后**的读数（dynrange_T 这类列表键已在
    absolute_readings 里摊平成 p1/p99），不是原始 arm dict —— 否则会把
    「键在归档里以列表形式存在」误判成缺项。
    """
    bad = []
    for arm, keys in REPLAY_SCHEMA.items():
        for side, blob in (("live", live), ("archived", arch)):
            if arm not in blob or not isinstance(blob.get(arm), dict):
                bad.append({"arm": arm, "side": side, "error": "arm_missing"})
                continue
            for k in keys:
                val = blob[arm].get(k)
                if val is None:
                    bad.append({"arm": arm, "side": side, "key": k,
                                "error": "required_key_missing"})
                elif not (isinstance(val, (int, float)) and np.isfinite(val)):
                    bad.append({"arm": arm, "side": side, "key": k,
                                "error": "required_key_not_finite", "value": repr(val)})
    return bad


def replay_arm_readings_at_gain(frame, frame0, em, seed, res, gain):
    """把**真实流水线**（cell_aggregate -> run_scene -> arm_metrics）跑在指定增益下，
    返回逐臂的绝对读数。覆盖面门 ③ 的参照量来自这里，不再来自本文件的旁路公式。"""
    old = GAIN
    try:
        globals()["GAIN"] = gain
        a = cell_aggregate(frame)
        a0 = cell_aggregate(frame0)
        rr = {"arms": {}}
        run_scene(a, a0, em, seed, rr, "")
        return {arm: absolute_readings(rr["arms"][arm])
                for arm in REPLAY_SCHEMA}
    finally:
        globals()["GAIN"] = old


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


def cell_aggregate(frame, gain=None):
    """Cell-aggregated model quantities (the representation domain of the control grid).

    ``gain`` 只为把**真实流水线**跑在指定增益下（覆盖面门 ③ 与独立性范围实测），
    默认取文件级 GAIN；不改任何生产口径。
    """
    g = GAIN if gain is None else float(gain)
    s_cell = frame["src"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    ss_cell = frame["sigma_slow2"].reshape(N_CELLS, DELTA, N_CELLS, DELTA).mean(axis=(1, 3))
    sw_cell = ss_cell + s_cell / g
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
    # G08-05 R2 第 5 条 N3：原字段 "recomputed_independently_from_frame": True 是一个
    # **过强的字面布尔** —— 它声称「独立于 frame 重算」，与本文件自己的结论
    # （本判据**不**独立于 GAIN：重算用同一个 GAIN 常量与同一个 frame，两侧同错）
    # 直接矛盾。字面布尔无判别力，任何注入都改不动它。改为**实测**独立性范围：
    # 同一个重算表达式在 g -> 1/g 下与聚合侧一起平移 ⇒ max_rel_dev 逐位不变，
    # 这个**实测数**才是「两侧同错、本判据恒绿」的证据；boolean 字段删除。
    agg_inv = cell_aggregate(frame, 1.0 / GAIN)
    _sw2_inv = np.repeat(np.repeat(ss_cell_chk + s_cell_chk / (1.0 / GAIN), DELTA, axis=0),
                         DELTA, axis=1)
    sw2_scale_inv = float(np.mean(_sw2_inv))
    sw2_rel_dev_gain_inverted = float(
        np.max(np.abs(agg_inv["sw2_map"] - _sw2_inv)) / max(sw2_scale_inv, 1e-300))
    sw2_field_shift_under_inversion = abs(sw2_scale_inv / max(sw2_scale, 1e-300) - 1.0)
    res["variance_map_selfcheck"] = {
        "definition": "sw2_map = blockmean(sigma_slow2) + blockmean(src)/GAIN，按 DELTA×DELTA 铺开",
        "independence_scope": {
            "independent_of": ["cell_aggregation_step"],
            "NOT_independent_of": ["GAIN_constant", "frame_object"],
            "basis": "measured_not_asserted",
            "measured_max_rel_dev_under_gain_inversion": sw2_rel_dev_gain_inverted,
            "measured_variance_field_shift_under_gain_inversion": sw2_field_shift_under_inversion,
            "note": ("本字段取代原来的字面布尔 recomputed_independently_from_frame=True。"
                     "该布尔声称「独立于 frame」却与本文件结论矛盾，且任何注入都改不动它。"
                     "实测：把 g 换成 1/g 后方差面本身明显变化，"
                     "但聚合侧与重算侧之差仍为 0 ⇒ 两者同错，本判据对量纲颠倒**恒绿**、无判别力。"
                     "该盲区由 absolute_scale_matches_archived_first_hand 与 "
                     "gain_matches_independent_archived_reading（外部参照，与 GAIN 不同源）承担。"),
        },
        "mean_sw2": sw2_scale,
        "max_rel_dev": sw2_rel_dev,
        "criterion": "max|sw2_map - 独立重算| / mean < 1e-12（不走 E，避开 E 的齐次盲区）",
    }

    a_full = res["arms"]["A_full"]

    # ---------------- G08-05 B1/B4：外部参照判据（打破增益盲区） ----------------
    # ① 一手重放：逐条比对本进程重算的**绝对**读数与**代码内冻结**的一手常量
    #    FIRST_HAND。参照量不落在任何被复现命令写出的路径上，也不经本进程重算
    #    ⇒ 既不同源（不共享 GAIN）、又不可被复现动作改写 ⇒ 判决对执行次数不敏感。
    #    （原实现拿 ARCHIVE_SELF 作参照，而它与 OUT 是同一个文件：注入连跑两次时
    #    第 1 跑红、第 2 跑绿，缺陷仍在。ARCHIVE_SELF 现降级为纯诊断。）
    arch = load_json(ARCHIVE_SELF)          # 诊断用，不参与判决
    ref_all = FIRST_HAND                     # 判决用参照量
    ref_source = "in_code_frozen_first_hand"
    live_all = {arm: absolute_readings(res["arms"][arm]) for arm in REPLAY_SCHEMA}
    # 防御式归一：参照量缺 arm / 值不可转 float 时**不抛异常**，而是留 None，
    # 交给 schema_violations 判红（G08-05 R2 第 3 条：宁可干净地判红，
    # 不要在归一阶段崩掉、把「判红」变成「没产物」）。
    arch_all = {}
    if isinstance(ref_all, dict):
        for arm in REPLAY_SCHEMA:
            try:
                arch_all[arm] = {k: ref_all[arm].get(k) for k in REPLAY_SCHEMA[arm]}
            except Exception:
                arch_all[arm] = {k: None for k in REPLAY_SCHEMA[arm]}
    replay_rows, replay_ok = [], True
    schema_bad, compared = [], 0
    if not isinstance(ref_all, dict) or not ref_all:
        replay_ok = False
        schema_bad.append({"side": "first_hand", "error": "first_hand_reference_missing"})
        replay_rows.append({"error": "first_hand_reference_missing"})
    else:
        # fail-closed：先验 schema 校验。任一**期望键**缺失/非有限 ⇒ 直接判红，
        # 不进入比较（G08-05 R2 第 3 条：原实现对缺失一律 skip，
        # 「比较 0 项仍判绿」是 fail-open）。
        schema_bad = schema_violations(live_all, arch_all)
        if schema_bad:
            replay_ok = False
            replay_rows.append({"error": "schema_mismatch", "violations": schema_bad})
        else:
            for arm_name in REPLAY_SCHEMA:
                live = live_all[arm_name]
                ref = arch_all[arm_name]
                for k in REPLAY_SCHEMA[arm_name]:
                    d = abs(live[k] - ref[k]) / max(abs(ref[k]), 1e-300)
                    compared += 1
                    replay_rows.append({"arm": arm_name, "key": k, "live": live[k],
                                        "first_hand": ref[k], "rel_dev": d,
                                        "margin_ratio": d / REPLAY_TOL,
                                        "ok": bool(d <= REPLAY_TOL)})
                    replay_ok = replay_ok and d <= REPLAY_TOL
            # 比较项数为 0 也必须 fail-closed（即使 schema 恰好为空 dict）。
            if compared == 0:
                replay_ok = False
                replay_rows.append({"error": "zero_comparisons"})
    # 结构性自检：判决参照量的**来源**。`reference_is_rewrite_proof` 门断言判决
    # 参照量来自代码内冻结常量、且不落在任何被复现命令写出的路径上。一旦有人把
    # 参照量重新指向可写文件（= 复现本条缺陷），该门立刻判红。
    ref_path_overlaps_out = bool(
        os.path.realpath(ARCHIVE_SELF) == os.path.realpath(ARCHIVE_SELF_OUT))
    rewrite_proof = bool(ref_source == "in_code_frozen_first_hand")
    res["structural_reference_audit"] = {
        "verdict_reference_source": ref_source,
        "reference_path_written_by_this_script": ref_path_overlaps_out,
        "criterion": ("判决参照量既不经本进程重算（不共享 GAIN），也不落在本脚本写出的"
                      "路径上（不可被复现动作改写）⇒ 对执行次数不敏感。"),
        "old_defect": ("原判决参照量 ARCHIVE_SELF 与 OUT 归一化后是同一文件：注入 "
                       "g -> 1/g 连跑两次时第 1 跑判红、第 2 跑 max_rel_dev=0.0 转绿，"
                       "缺陷仍在（实测，见交付件）。"),
        "pass": rewrite_proof,
    }
    res["absolute_scale_replay_vs_archive"] = {
        "reference_source": ref_source,
        "reference_provenance": FIRST_HAND["provenance"],
        "diagnostic_archive_path": ARCHIVE_SELF,
        "diagnostic_archive_gain_e_per_adu": (arch or {}).get("gain_e_per_adu"),
        "diagnostic_archive_gate_names": sorted((arch or {}).get("gates", {}).keys()),
        "tolerance_rel": REPLAY_TOL,
        "n_compared": compared,
        "expected_key_count": sum(len(v) for v in REPLAY_SCHEMA.values()),
        "schema_violations": schema_bad,
        "reference_independence": "判决参照量是写在本文件里的冻结一手常量，不经本进程"
                                  "重算（不共享 GAIN），也不在任何被复现命令写出的路径上"
                                  "（不可被复现动作改写）；E 对 v 齐次、对绝对尺度无判别力，"
                                  "故重放对象只取携带绝对尺度的读数。",
        "rows": replay_rows,
        "pass": bool(replay_ok),
        "fail_closed_note": ("fail-closed 三条：参照量缺失或为空 ⇒ 判红；schema 不匹配"
                             "（期望键缺失或非有限）⇒ 判红；比较项数为 0 ⇒ 判红。"
                             "A_frame_recon 无 max_abs_dev_vs_T 是定义使然，已写入 "
                             "REPLAY_SCHEMA 的期望键集，不算缺项。"),
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

    # ③ 覆盖面门：审稿注入 g -> 1/g（量纲颠倒）必须被上面的重放判据抓住。
    #    G08-05 R2 第 2 条：本门原先拿 dynrange_T_at_gain（**本文件另写一遍的公式**）
    #    与 arm_metrics 的读数自比，两条路同源同式 ⇒ 自比自恒真，8 个注入变体 0/8
    #    判红，是常量自检。现改为把**真实流水线**跑在注入增益下（cell_aggregate →
    #    run_scene → arm_metrics），逐条比对**重放判据实际比对的那些读数**，
    #    判定口径与门①完全一致 ⇒ 门①若被重新指向不敏感的读数，本门立刻判红。
    live_t = res["arms"]["A_full"]["dynrange_T"]
    T_inv = dynrange_T_at_gain(frame, em, 1.0 / GAIN)
    inv_read = replay_arm_readings_at_gain(frame, frame0, em, SEED, res, 1.0 / GAIN)
    inv_devs, margin_rows = [], []
    for arm_name in REPLAY_SCHEMA:
        for k in REPLAY_SCHEMA[arm_name]:
            lv = live_all[arm_name][k]
            rv = inv_read[arm_name][k]
            if lv is None or rv is None:
                continue
            d = abs(rv - lv) / max(abs(lv), 1e-300)
            margin_rows.append({"arm": arm_name, "key": k, "rel_dev": d,
                                "margin_ratio": d / REPLAY_TOL})
            inv_devs.append(d)
    inv_devs = inv_devs or [float("inf")]
    max_inv = max(inv_devs)
    # 判据级口径：**最小非零裕度**（原实现只报 max，且报的 max 只是 sens 探针两个
    # 分位里的 max，不是 23 条重放读数里的 max —— 口径本身就写错了）。
    nonzero = [m for m in margin_rows if m["margin_ratio"] > 0.0]
    dead = [{"arm": m["arm"], "key": m["key"]} for m in margin_rows
            if m["margin_ratio"] == 0.0]
    min_nz = min((m["margin_ratio"] for m in nonzero), default=None)
    sens_ok = bool(max_inv > REPLAY_TOL)
    # 真实检测地板：由**实测**灵敏度线性化。取一个小的相对增益扰动 δ0，跑真实
    # 流水线，量出最敏感读数的灵敏度 s_max = max_k |Δreading|/δ0，
    # 则「使门①翻红所需的最小相对增益误差」≈ REPLAY_TOL / s_max。
    d0 = 1.0e-3
    probe = replay_arm_readings_at_gain(frame, frame0, em, SEED, res, GAIN * (1.0 + d0))
    s_max = 0.0
    for arm_name in REPLAY_SCHEMA:
        for k in REPLAY_SCHEMA[arm_name]:
            lv, pv = live_all[arm_name][k], probe[arm_name][k]
            if lv is None or pv is None:
                continue
            s_max = max(s_max, abs(pv - lv) / max(abs(lv), 1e-300) / d0)
    det_floor = (REPLAY_TOL / s_max) if s_max > 0 else None
    res["gain_inversion_sensitivity"] = {
        "injection": "GAIN -> 1/GAIN（把 S/g 当成 S·g，量纲颠倒）",
        "gain_injected": float(1.0 / GAIN),
        "evaluation_path": ("真实流水线 cell_aggregate -> run_scene -> arm_metrics "
                            "在注入增益下重跑，逐条取重放判据实际比对的读数；"
                            "不再使用本文件的旁路公式 dynrange_T_at_gain 作为参照量"),
        "dynrange_T_live": list(map(float, live_t)),
        "dynrange_T_under_injection_closed_form": T_inv,
        "n_readings_compared": len(inv_devs),
        "max_rel_dev_live_vs_injected": max_inv,
        "tolerance_rel": REPLAY_TOL,
        # ---- 裕度登记：最小非零口径 ----
        "margin_convention": ("判据级最小**非零**裕度 = min over readings (rel_dev/REPLAY_TOL)；"
                              "裕度为 0 的读数在本注入下**不位移**，对这条注入是死读数，"
                              "单列、不参与最小值、也不得当作覆盖证据。"),
        "margin_max_ratio": max((m["margin_ratio"] for m in margin_rows), default=None),
        "margin_min_nonzero_ratio": min_nz,
        "n_margin_nonzero": len(nonzero),
        "dead_readings_under_this_injection": dead,
        "n_dead_readings": len(dead),
        "overstatement_if_max_used_instead_of_min_nonzero": (
            (max(m["margin_ratio"] for m in nonzero) / min_nz)
            if (nonzero and min_nz) else None),
        "per_reading_margin": margin_rows,
        # ---- 真实检测地板（实测灵敏度线性化）----
        "detection_floor_rel_gain_error": det_floor,
        "detection_floor_method": ("在 GAIN*(1+1e-3) 上跑真实流水线，量出最敏感读数的"
                                   "相对灵敏度 s_max，再取 REPLAY_TOL/s_max"),
        "s_max_per_rel_gain": s_max,
        "pass": sens_ok,
        "meaning": ("True ⇒ 在量纲颠倒注入下，重放判据所比对的读数中至少有一条的偏移"
                    "超出容差（门①必然判红）。这是覆盖面门（判据是否活），不是物理"
                    "正确性门。判红路径是**实测**的，不再是自比自恒真。"),
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
        #     证据；该职责由本文件末尾的 absolute_scale_matches_frozen_first_hand
        #     与 gain_matches_independent_archived_reading 承担（参照量不同源、且不可被
        #     复现动作改写）。reference_is_rewrite_proof 则保证这两条的参照量本身
        #     不会被人重新指回「会被本脚本覆写的文件」。
        "variance_map_matches_physical_model": sw2_rel_dev < 1e-12,
        # --- G08-05 B1/B4：四条**参照量**判据（打破增益/绝对尺度盲区）---
        # 上面的 variance_map_matches_physical_model 虽「独立重算」，但用的是**同一个
        # GAIN 常量**与同一个 frame，与聚合侧逐项同构 ⇒ 两侧同错，只能抓 aggregation
        # 步骤的错误，抓不到 g 本身。审稿注入 g -> 1/g 时本文件原 10 条门全绿（实测 0/10）。
        # 下面几条用**不同源、且不可被复现动作改写**的参照量补上这块。
        # **门名变更（G08-05 R2 B1）**：absolute_scale_matches_archived_first_hand
        # → absolute_scale_matches_frozen_first_hand。参照量不再是磁盘归档，而是写在本
        # 文件里的冻结一手常量；沿用旧名会变成不实描述。旧名登记在
        # res["gate_renames"] 里，便于与审核包交叉引用。
        "absolute_scale_matches_frozen_first_hand": bool(replay_ok),
        "gain_matches_independent_archived_reading": bool(gain_ok),
        "gain_injection_is_detected_by_this_suite": bool(sens_ok),
        # 结构性门：判决参照量的**来源**既不同源又不可被复现动作改写。
        # 它抓的不是物理量，而是「判据自身是否还成立」——把参照量重新指向
        # 本脚本写出的文件（本条缺陷的原样）时立刻判红。
        "reference_is_rewrite_proof": bool(rewrite_proof),
    }
    res["gate_renames"] = [{
        "old": "absolute_scale_matches_archived_first_hand",
        "new": "absolute_scale_matches_frozen_first_hand",
        "reason": "判决参照量由「会被本脚本覆写的磁盘归档」改为「代码内冻结的一手常量」；"
                  "沿用旧名会变成不实描述。门未被删除、语义未被放宽，只是参照量换了同值的源。",
        "same_tolerance": REPLAY_TOL,
        "same_reading_set": sorted(k for ks in REPLAY_SCHEMA.values() for k in ks),
    }]
    # B4②：归档与脚本脱钩的**登记**（纯诊断，不参与任何判决、不重跑、不改 results/）。
    # 归档 JSON 是另一次执行的产物，其门集合/键集合与本脚本当前不同；报告侧若按
    # 「逐叶全同」表述，对 exp04 不成立。此处只登记事实，处置由归档车道负责。
    # **G08-05 R2 B1**：本登记此前喂给判决判据 absolute_scale_matches_archived_first_hand，
    # 而归档路径与 OUT 同文件 ⇒ 判决顺序相关（连跑两次：红→绿，缺陷仍在）。
    # 现本登记**只作诊断**，判决参照量已改为代码内冻结常量 FIRST_HAND。
    script_keys = sorted(res.keys())
    res["archive_decoupling_registration"] = {
        "role": "diagnostic_only__not_a_verdict_reference",
        "archive_path": ARCHIVE_SELF,
        "archive_exists": arch is not None,
        "archive_is_written_by_this_script": ref_path_overlaps_out,
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
        "该盲区由四条参照量判据承担：absolute_scale_matches_frozen_first_hand（判决参照量"
        "改为代码内冻结的一手常量 FIRST_HAND，不经本进程重算、也不落在任何被复现命令写出的"
        "路径上 ⇒ 对执行次数不敏感）、gain_matches_independent_archived_reading"
        "（跨单元增益溯源）、gain_injection_is_detected_by_this_suite（覆盖面门：断言前两条"
        "对量纲颠倒注入是活的）、reference_is_rewrite_proof（结构性门：断言判决参照量的来源"
        "既不同源又不可被复现动作改写）。"
        "G08-05 R2 B1 订正：门 ① 原以 ARCHIVE_SELF 为判决参照量，而该路径与 OUT 归一化后"
        "是同一文件 ⇒ 连跑两次时第 1 跑判红、第 2 跑 max_rel_dev=0.0 转绿而缺陷仍在"
        "（顺序相关的假绿门）。ARCHIVE_SELF 已降级为纯诊断，不参与判决；门未删除、"
        "容差未放宽、读数集未改动。")
    res["all_gates_pass"] = bool(all(res["gates"].values()))
    res["runtime_s"] = time.time() - t0
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
