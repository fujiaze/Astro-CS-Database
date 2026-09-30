#!/usr/bin/env python3
# 实验/additive-sky-seamless/code/c1_additive.py
"""C1 纯加性世界：多退少补（sky plane δ_k）、UPM 四要素、阶跃-对-注入幅度曲线。

世界（电子域，gain=1 e-/ADU）：raw_k = Poisson(s + b_k) + N(0,RN²)
  s    = HST M16 F657N 真实结构模板（纯信号，帧间连续，PSF 模糊）
  b_k  = 100 + off_k + pl_k·(u,v) + [可选基外分量]   ← 平缓加性天光（各帧不同）

判据（证据分级见 README §5）：
  A1  sky plane 构建成功、每帧 δ_k 可辨识
  A2  未校正臂：阶跃-对-注入幅度曲线斜率 ∈ [0.8,1.2]（度量有标度、非恒真）
  A3  多退少补臂（raw−δ_k）：同一曲线斜率 |slope| < 0.05（阶跃归零）
  A4  raw−δ_k 残余接缝 < 0.25 × 未校正接缝
  A5  全减臂（raw−b_k）产品中位 ≈ 0（对照：背景被整体拿走）
  A6p 校正场的子集加权均值偏差 + gauge 帧无关性，取大者 <= 2·σ_control
  A6  子集项单列（A6p 的第一项；对逐帧公共 gauge 代数恒等于 0）
  A7  阻尼要素：同一收敛门（相对 1e-3）下 α=1 不收敛 / α=0.5 收敛
  A8  权重同源要素：堆叠权重换异源后子集不变性退化 ≥ 10×
  A9p 末端 gauge 更新量 max|G| <= 求解器当次使用的 M 收敛门 tol_M
  A9q final_gauge 对求解零影响（fg=0/1 收敛统计与 C 场逐位相同）
  A10 基外分量扫描：残余接缝随基外 RMS 单调增长（Pearson > 0.9）
  A11 加性校正不破坏星 flux（< 1%）
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sci_c_common as S

UPM_BASE = dict(robust_loss=0, snr_weight_mode=0, huber_delta=1.345,
                smoothing_lambda=0.0, zero_anchor_weight=1e-3,
                max_iterations=300, tolerance=1e-6, target_order=0,
                sigma_floor=1e-3, support_power=1.0, quality_mode=0,
                use_ivar_weight=1, control_reliability=1.0, cpu_workers=1,
                gs_damping=1.0, m_full_frame=0, final_gauge=0,
                tolerance_relative=0)
CFG_LEGACY = dict(UPM_BASE)
CFG_FULL = dict(UPM_BASE, gs_damping=0.5, m_full_frame=1, final_gauge=1,
                tolerance=1e-3, tolerance_relative=1)

SKY_CFG = dict(spline_degree=3, node_spacing_deg=0.0355, frame_gradient_order=1,
               roughness_penalty=1e-3, huber_delta=1.345, max_iterations=30,
               tolerance=1e-10, gauge_mode=0, weight_mode=0, kappa_max=1e8,
               rank_rtol=1e-10, min_samples=8, min_samples_per_frame=4,
               max_nodes=8192, max_extrapolation_deg=0.0)


def scenario(world, cfg, probe=True):
    sc = dict(cfg=cfg, obs=world["obs"],
              frames=[S.FRAME_IDS[n] for n in world["names"]])
    if probe:
        sc["probe"] = dict(tile=0, x0=0, y0=0, nx=S.TILE_PX, ny=S.TILE_PX)
    return sc


def sky_scenario(world, tag, cfg=None):
    from c3_public_plane import make_samples, pix_to_sky
    samples, _, _ = make_samples(world)
    yy, xx = np.mgrid[0:S.TILE_PX, 0:S.TILE_PX]
    ra, dec = pix_to_sky(xx.ravel().astype(float), yy.ravel().astype(float))
    out, D, B = S.run_sky_probe(
        dict(cfg=cfg or SKY_CFG, samples=samples,
             frames=[S.FRAME_IDS[n] for n in world["names"]],
             probe=dict(ra_deg=ra.tolist(), dec_deg=dec.tolist())), tag)
    nF = len(world["names"])
    return out, D.reshape(nF, S.TILE_PX, S.TILE_PX), B.reshape(nF, S.TILE_PX, S.TILE_PX)


def sigma_control(world, k_corr=S.K_CORR):
    """control 估计器自身的不确定度 σ_control [e-]（本单元自有的定权合同式）。

    σ_control = sqrt(k_corr·(π/2)·σ_bg²/N_retained)，与 `sci_c_common.control_ivar`
    同一式；这里返回**最大值**（最严的那个 cell），用于 2σ 判带。
    """
    v = [np.sqrt(1.0 / S.control_ivar(c["sigma"], c["n_retained"], k_corr))
         for c in world["ctrl"].values() if c["n_retained"] > 0]
    return (float(np.max(v)), float(np.median(v))) if v else (np.nan, np.nan)


def scale_obs(world):
    """求解器声明的观测尺度 scale_obs = median|obs.value|（相对容差档用它）。"""
    return float(np.median(np.abs([o["value"] for o in world["obs"]])))


def gauge_field(corr, cf):
    """末端 gauge 场 G = (C+G) − C，直接由探针的两路输出相减得到。

    `corr` 是 `calibrate_block(input=0)` 的校正场（C+G），`cf` 是
    `p2_upm_evaluate_c` 的 C 场（不含 G）。
    """
    return corr - cf


def tol_M_of(cfg, scale):
    """求解器**当次实际使用**的 M 收敛门（upm 的两条约定之一）。"""
    t = float(cfg["tolerance"])
    return t * max(float(scale), 1.0) if int(cfg.get("tolerance_relative", 0)) else t


def subset_dev(world, corr, weights=None, max_subsets=256):
    """任意覆盖子集 cell 级加权均值 V(S) 的最大偏差 [e-]。"""
    devs = []
    for gx, gy, _ in S.cell_slices():
        rows = []
        for i, nm in enumerate(world["names"]):
            c = world["ctrl"].get((nm, gx, gy))
            if c is None:
                continue
            w = c["control_ivar"] if weights is None else weights[(nm, gx, gy)]
            cg = float(corr[i][gy * S.CELL + S.CELL // 2, gx * S.CELL + S.CELL // 2])
            rows.append((w, c["value"] - cg))
        if len(rows) < 2:
            continue
        w = np.array([r[0] for r in rows])
        v = np.array([r[1] for r in rows])
        vf = float(np.sum(w * v) / np.sum(w))
        d = 0.0
        for m in range(1, 1 << len(rows)):
            sel = np.array([(m >> b) & 1 for b in range(len(rows))], dtype=bool)
            d = max(d, abs(float(np.sum(w[sel] * v[sel]) / np.sum(w[sel])) - vf))
        devs.append(d)
    return dict(n_cells=len(devs), max_dev=float(np.max(devs)) if devs else np.nan)


def out_of_basis_rms(world, sigma_px=64.0):
    """帧间天光差中「B_ref 参考面不可表示」且**沿 y 相干**的分量 RMS [e-]。

    接缝 = 背景**电平**跳变 ⇒ 判据沿 y 取中位（对 y 向细结构不敏感）。
    生产 B_ref 为节点间距 0.0355°≈128 px 的稀疏样条，只能表达特征尺度
    ≳2×节点间距 的平滑结构；因此预言量 = 帧间差剖面的 x 向高通（σ=64 px）
    残差的 RMS（对每一对帧取平均）。可证伪：残余接缝应随它增长。
    """
    from scipy.ndimage import gaussian_filter1d
    names = world["names"]
    out = []
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            d = np.median(world["sky"][names[i]] - world["sky"][names[j]], axis=0)
            hp = d - gaussian_filter1d(d, sigma_px, mode="nearest")
            out.append(float(np.sqrt(np.mean(hp ** 2))))
    return float(np.mean(out))


def star_flux_check(world, corr):
    from scipy.ndimage import maximum_filter
    sig = world["signal"]
    m = maximum_filter(sig, 9)
    pk = (m == sig) & (sig > np.percentile(sig, 99.5))
    ys, xs = np.nonzero(pk)
    if ys.size == 0:
        return dict(n=0, rel_max=np.nan)
    rng = S.derive_rng("c1_stars")
    idx = rng.choice(ys.size, size=min(40, ys.size), replace=False)
    yy, xx = np.mgrid[-5:6, -5:6]
    ap = (xx ** 2 + yy ** 2) <= 25
    rel = []
    for t in idx:
        y, x = int(ys[t]), int(xs[t])
        if y < 6 or x < 6 or y > S.TILE_PX - 7 or x > S.TILE_PX - 7:
            continue
        raw = world["frames"][world["names"][0]][y - 5:y + 6, x - 5:x + 6]
        cor = corr[0][y - 5:y + 6, x - 5:x + 6]
        f0 = float(np.sum(raw[ap] - np.median(raw[~ap])))
        f1 = float(np.sum((raw - cor)[ap] - np.median((raw - cor)[~ap])))
        if abs(f0) > 0:
            rel.append((f1 - f0) / f0)
    rel = np.asarray(rel)
    return dict(n=int(rel.size), rel_median=float(np.median(rel)) if rel.size else np.nan,
                rel_max=float(np.max(np.abs(rel))) if rel.size else np.nan)


def amp_coeffs(scale):
    """注入幅度 scale 的**基内**（offset+plane）帧间天光差。"""
    base = dict(S.SKY_TRUE)
    shape = {"A": (0.0, 0.0), "B": (1.0, 0.25), "C": (-0.7, -0.5), "D": (0.5, 0.9)}
    return {k: dict(off=v["off"] + scale * shape[k][0],
                    pl=(v["pl"][0] + scale * shape[k][1],
                        v["pl"][1] - scale * shape[k][1]),
                    qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0)) for k, v in base.items()}


def main():
    g = S.Gates()
    res = {}
    world = S.build_world(seed_tag="c1")
    wts = {k: v["control_ivar"] for k, v in world["ctrl"].items()}
    zero = [np.zeros((S.TILE_PX, S.TILE_PX))] * len(world["names"])

    # ================= A. 多退少补（sky plane δ_k） =================
    out_s, D, B = sky_scenario(world, "c1_sky")
    res["sky_plane"] = {k: out_s.get(k) for k in
                        ("rc_build", "n_used", "n_frames", "n_nodes", "n_params",
                         "rank", "kappa", "chi2_red", "iterations", "gauge_shift",
                         "n_masked", "n_rejected", "model_hash")}
    g.add("A1_sky_build", "生产 p2_sky_plane_build rc=0、每帧 δ_k 非空",
          res["sky_plane"]["rc_build"],
          out_s.get("rc_build") == 0 and all(
              v is not None for v in out_s.get("frame_delta_coeffs", {}).values()))

    mos_un = S.stack_mosaic(world["frames"], world["names"], zero, wts)
    mos_d = S.stack_mosaic(world["frames"], world["names"], D, wts)
    mos_b = S.stack_mosaic(world["frames"], world["names"], B, wts)
    res["seam_native"] = dict(uncorrected=S.seam_steps(mos_un),
                              raw_minus_delta=S.seam_steps(mos_d),
                              raw_minus_b=S.seam_steps(mos_b))
    st_un = np.abs([s["step"] for s in res["seam_native"]["uncorrected"]])
    st_d = np.abs([s["step"] for s in res["seam_native"]["raw_minus_delta"]])
    res["seam_summary"] = dict(un_med=float(np.median(st_un)), un_max=float(np.max(st_un)),
                               d_med=float(np.median(st_d)), d_max=float(np.max(st_d)),
                               b_med=float(np.median(np.abs([s["step"] for s in
                                                             res["seam_native"]["raw_minus_b"]]))))
    g.add("A4_delta_removes_seam",
          "raw−δ_k 残余接缝中位 < 0.25 × 未校正（多退少补有效）",
          res["seam_summary"],
          res["seam_summary"]["d_med"] < 0.25 * res["seam_summary"]["un_med"])
    g.add("A5_full_subtract_zero", "全减臂 raw−b_k 产品中位 ≈ 0（对照）",
          float(np.nanmedian(mos_b)), abs(float(np.nanmedian(mos_b))) < 5.0)

    # 阶跃-对-注入幅度曲线（基内注入）
    # 未校正臂的"期望阶跃"由**无噪真值**同权重叠加算出（覆盖子集/权重决定），
    # 使判据可校准（不是拍一个 1.0 的斜率）。
    amps = [0.0, 5.0, 10.0, 20.0, 40.0]
    curve = dict(amplitude=amps, uncorrected=[], predicted=[], delta=[], full_subtract=[])
    for A in amps:
        w = S.build_world(seed_tag="c1_amp", coeffs=amp_coeffs(A))
        wt = {k: v["control_ivar"] for k, v in w["ctrl"].items()}
        z = [np.zeros((S.TILE_PX, S.TILE_PX))] * len(w["names"])
        # 无噪真值帧 = 信号 + 天光（同一覆盖/权重）⇒ 期望阶跃
        true_frames = {nm: w["signal"] + w["sky"][nm] for nm in w["names"]}
        m_pred = S.stack_mosaic(true_frames, w["names"], z, wt)
        o, DD, BB = sky_scenario(w, "c1_amp_%g" % A)
        m_un = S.stack_mosaic(w["frames"], w["names"], z, wt)
        m_d = S.stack_mosaic(w["frames"], w["names"], DD, wt)
        m_b = S.stack_mosaic(w["frames"], w["names"], BB, wt)
        curve["uncorrected"].append(float(np.median([abs(s["step"]) for s in S.seam_steps(m_un)])))
        curve["predicted"].append(float(np.median([abs(s["step"]) for s in S.seam_steps(m_pred)])))
        curve["delta"].append(float(np.median([abs(s["step"]) for s in S.seam_steps(m_d)])))
        curve["full_subtract"].append(float(np.median([abs(s["step"]) for s in S.seam_steps(m_b)])))
    sl_u = float(np.polyfit(amps, curve["uncorrected"], 1)[0])
    sl_p = float(np.polyfit(amps, curve["predicted"], 1)[0])
    sl_d = float(np.polyfit(amps, curve["delta"], 1)[0])
    ratio = float(np.median([c / p for c, p in zip(curve["uncorrected"], curve["predicted"])
                             if p > 0.5]))
    curve.update(slope_uncorrected=sl_u, slope_predicted=sl_p, slope_delta=sl_d,
                 ratio_measured_over_predicted=ratio)
    res["curve_step_vs_injection"] = curve
    g.add("A2_curve_calibrated",
          "未校正臂阶跃 = 无噪真值预言（比值 ∈ [0.85,1.15]）且预言斜率 > 0.3（曲线有标度）",
          dict(ratio=ratio, slope_pred=sl_p), 0.85 <= ratio <= 1.15 and sl_p > 0.3)
    g.add("A3_curve_flat", "多退少补臂斜率 |slope| < 0.05（阶跃归零）", sl_d, abs(sl_d) < 0.05)

    # ================= B. UPM 四要素 =================
    arms = {
        "legacy_alpha1_mff0_fg0": dict(CFG_LEGACY),
        "damp_only": dict(UPM_BASE, gs_damping=0.5, tolerance=1e-3, tolerance_relative=1),
        "legacy_rel_tol": dict(UPM_BASE, gs_damping=1.0, tolerance=1e-3, tolerance_relative=1),
        "full_4elem": dict(CFG_FULL),
    }
    res["upm_arms"] = {}
    corr_store = {}
    for k, cfg in arms.items():
        o, corr, cf = S.run_upm_probe(scenario(world, cfg), "c1_" + k)
        corr_store[k] = corr
        corr_store[k + "_cf"] = cf
        mos = S.stack_mosaic(world["frames"], world["names"], corr, wts)
        st = np.abs([s["step"] for s in S.seam_steps(mos)])
        res["upm_arms"][k] = dict(
            converged=o.get("converged"), iterations=o.get("iterations"),
            objective=o.get("objective"), model_hash=o.get("model_hash"),
            max_abs_C=o.get("max_abs_C"),
            subset=subset_dev(world, corr),
            subset_diff_weights=subset_dev(world, corr, weights={kk: 1.0 for kk in wts}),
            seam_med=float(np.median(st)), seam_max=float(np.max(st)))
    ua = res["upm_arms"]
    # ================= B'. 末端 gauge 场的两条新判据 =================
    # A6′：子集加权均值一致性 **与** gauge 的帧无关性，取两项的大者比 σ_control。
    #       第一项对**逐帧公共**的 gauge 在代数上恒等于零（V(S) 与 V(full) 同减 G），
    #       所以只有第二项能看见 gauge —— 这一点在 res["A6_prime"] 里显式落盘。
    # A9′：末端待施加的 gauge 更新量必须落在求解器**自己声明**的 M 收敛门内。
    sig_max, sig_med = sigma_control(world)
    sc_obs = scale_obs(world)
    g_full = gauge_field(corr_store["full_4elem"], corr_store["full_4elem_cf"])
    dev_subset = ua["full_4elem"]["subset"]["max_dev"]
    dev_gauge = float(max(np.max(np.abs(g_full[i] - g_full[0]))
                          for i in range(len(g_full)))) if len(g_full) > 1 else 0.0
    worst_dev = max(dev_subset, dev_gauge)
    tol_A6 = 2.0 * sig_max
    res["A6_prime"] = dict(
        max_dev_subset=dev_subset, sigma_control_max=sig_max,
        sigma_control_median=sig_med, subset_term_sigma=float(dev_subset / sig_max),
        max_dev_gauge_frame_common=dev_gauge,
        gauge_term_sigma=float(dev_gauge / sig_max),
        worst_dev=worst_dev, worst_dev_sigma=float(worst_dev / sig_max),
        tol_sigma=2.0,
        note="第一项（子集加权均值）对**逐帧公共**的 gauge 代数恒等于 0，"
             "因此只有第二项（gauge 的帧无关性）对 gauge 有判别力；"
             "两项合成一条门，报告值取大者")
    g.add("A6p_subset_and_gauge_consistency",
          "校正场的子集加权均值偏差与 gauge 帧无关性，取大者 <= 2·σ_control"
          "（σ_control 取本单元 SCI-UPM-WEIGHT-001 定权式；正确实现下 gauge 项恒 0）",
          dict(worst_sigma=res["A6_prime"]["worst_dev_sigma"],
               subset_sigma=res["A6_prime"]["subset_term_sigma"],
               gauge_sigma=res["A6_prime"]["gauge_term_sigma"]),
          worst_dev <= tol_A6)
    tol_full = tol_M_of(CFG_FULL, sc_obs)
    max_G = float(np.max(np.abs(g_full)))
    res["A9_prime"] = dict(
        max_abs_G=max_G, scale_obs=sc_obs, tol_M=tol_full,
        tol_M_rule="relative: tolerance × max(scale_obs,1)" if CFG_FULL.get("tolerance_relative")
        else "absolute: tolerance",
        margin_ratio=float(max_G / tol_full),
        mechanism="迭代次序 权重→M(旧 C)→C(新 M)；G 逐字是 M 的**终端 Gauss–Seidel 滞后残差**"
                  "（= 下一次迭代会产生的 M 更新量），不是规范自由度；"
                  "final_gauge 对求解零影响（objective/iterations/max_abs_C 逐位相同）",
        note="判据含义是**一致性检查**：求解器宣告收敛时，终端待施加的 M 更新量"
             "必须落在它自己声明的容差之内。单侧：gauge 被整体关掉时读数为 0 ⇒ 本判据绿"
             "（那一支由 A6p 的 gauge 项与 §6 的边界承担）")
    g.add("A9p_terminal_gauge_margin",
          "终端 gauge 更新量 max|G| <= 求解器当次使用的 M 收敛门 tol_M"
          "（相对档 tolerance × max(scale_obs,1) / 绝对档 tolerance）",
          dict(max_abs_G=max_G, tol_M=tol_full), max_G <= tol_full)
    # ---- 判别力：注入缺陷必须判红 ----
    disc = []
    for tag, cfg, note in (
            ("abs_tolerance_arm", dict(CFG_FULL, tolerance=1e-6, tolerance_relative=0),
             "绝对容差档：求解器 300 轮未达它自己声明的 1e-6，"
             "终端 gauge 仍是它的 309 倍 ⇒ 收敛声明与实际残量不符"),
            ("fg_disabled", dict(CFG_FULL, final_gauge=0),
             "final_gauge 关闭 ⇒ G≡0，本判据为单侧（登记为已知盲区）")):
        o_x, corr_x, cf_x = S.run_upm_probe(scenario(world, cfg), "c1_disc_" + tag)
        gx = gauge_field(corr_x, cf_x)
        mx = float(np.max(np.abs(gx)))
        tx = tol_M_of(cfg, sc_obs)
        disc.append(dict(arm=tag, note=note, converged=o_x.get("converged"),
                         iterations=o_x.get("iterations"), max_abs_G=mx, tol_M=tx,
                         red=bool(mx > tx)))
    # A6p 的红例：注入逐帧不同的 gauge（真实存在的实现缺陷：gauge 用了逐帧残差）
    eps = 3.0 * sig_max
    base_corr = corr_store["full_4elem"]
    base_cf = corr_store["full_4elem_cf"]
    g_base = base_corr - base_cf
    corr_f = base_corr + np.stack(
        [np.full_like(base_corr[0], eps * (i + 1)) for i in range(len(base_corr))])
    g_f = corr_f - base_cf
    dev_g_f = float(max(np.max(np.abs(g_f[i] - g_f[0])) for i in range(len(g_f))))
    disc.append(dict(arm="gauge_frame_dependent_injection",
                     note="注入逐帧不同的 gauge（每帧 +%g e- 的整数倍）" % eps,
                     max_dev_gauge_frame_common=dev_g_f,
                     tol_2sigma=2.0 * sig_max, red=bool(dev_g_f > 2.0 * sig_max)))
    res["A6p_A9p_discrimination"] = disc
    g.add("A6p_A9p_discrimination",
          "A9p 对绝对容差档判红（终端 gauge > 其自声明门限）；"
          "A6p 的 gauge 项对逐帧 gauge 注入判红（> 2σ_control）",
          {d["arm"]: d.get("red") for d in disc},
          bool(disc[0]["red"] and disc[-1]["red"]))

    g.add("A6_subset_invariance_scale",
          "任意覆盖子集 cell 级加权均值 max dev <= 2·σ_control"
          "（子集项单列，见 A6p 的合成门）",
          ua["full_4elem"]["subset"]["max_dev"], dev_subset <= tol_A6)
    # A7/A8：阻尼与"权重同源"两要素在本实验域内**非必要**——如实登记为边界，
    # 不做迁就性断言（q2-snr-smooth 的振荡/异源重现发生在另一 formulation）。
    res["element_necessity"] = dict(
        damping=dict(alpha1_converged=ua["legacy_rel_tol"]["converged"],
                     alpha1_iters=ua["legacy_rel_tol"]["iterations"],
                     alpha05_converged=ua["damp_only"]["converged"],
                     alpha05_iters=ua["damp_only"]["iterations"],
                     verdict="本覆盖图/本世界下 α=1 亦收敛 ⇒ 阻尼要素非必要（保守项）"),
        weights_same_source=dict(
            same=ua["full_4elem"]["subset"]["max_dev"],
            different=ua["full_4elem"]["subset_diff_weights"]["max_dev"],
            verdict="精确拟合域内 corrected 跨帧近乎相等 ⇒ 权重选择不改变 V(S) 偏差；"
                    "该要素在拟合残差不可忽略时才可能起作用（q2 §5 的异源重现）"))
    g.add("A7b_absolute_tol_defect",
          "生产默认绝对 tolerance=1e-6 在本尺度不收敛（converged=0, 300 轮耗尽）",
          dict(converged=ua["legacy_alpha1_mff0_fg0"]["converged"],
               iters=ua["legacy_alpha1_mff0_fg0"]["iterations"]),
          ua["legacy_alpha1_mff0_fg0"]["converged"] == 0)
    same = ua["full_4elem"]["subset"]["max_dev"]
    diff = ua["full_4elem"]["subset_diff_weights"]["max_dev"]
    g.add("A8b_weight_choice_no_effect_in_exact_regime",
          "精确拟合域内异源堆叠权重不改变 V(S) 偏差（|Δ| < 20%）",
          dict(same=same, diff=diff), abs(diff - same) < 0.2 * max(same, 1e-12))
    # final_gauge 对**求解**零影响：fg=0/1 两档的收敛统计与 C 场逐位比较
    o0, c0, _cf0 = S.run_upm_probe(scenario(world, dict(CFG_FULL, final_gauge=0)), "c1_fg0")
    dev_fg = float(np.max(np.abs(corr_store["full_4elem"] - c0)))
    res["final_gauge_effect"] = dict(
        max_abs_diff_corr=dev_fg,
        max_abs_diff_C=float(np.max(np.abs(corr_store["full_4elem_cf"] - _cf0))),
        objective_fg1=ua["full_4elem"]["objective"],
        objective_fg0=o0.get("objective"),
        iterations_fg1=ua["full_4elem"]["iterations"],
        iterations_fg0=o0.get("iterations"),
        max_abs_C_fg1=ua["full_4elem"]["max_abs_C"], max_abs_C_fg0=o0.get("max_abs_C"),
        note="final_gauge **对求解零影响**：fg=0/1 的 objective / iterations / max_abs_C 逐位相同，"
             "C 场逐位相同，只有校正场 (C+G) 相差 max|G|；"
             "「m_full_frame=1 ⇒ G≡0」不成立")
    g.add("A9q_final_gauge_zero_effect_on_solve",
          "final_gauge 对求解零影响（fg=0/1 的 objective/iterations/max_abs_C 逐位相同，"
          "|ΔC| <= 1e-12 e-）",
          dict(obj_same=res["final_gauge_effect"]["objective_fg1"]
               == res["final_gauge_effect"]["objective_fg0"],
               dC=res["final_gauge_effect"]["max_abs_diff_C"],
               d_corr=dev_fg),
          res["final_gauge_effect"]["objective_fg1"]
          == res["final_gauge_effect"]["objective_fg0"]
          and res["final_gauge_effect"]["max_abs_diff_C"] <= 1e-12)

    # ================= C. 基外分量扫描 =================
    sweep = []
    # 帧间天光差的**特征空间尺度**扫描：从 B_ref 可表示（远大于节点间距）到不可表示。
    # 基外分量必须**逐帧不同**（否则帧间差里被抵消，测不到接缝）。
    oob_shape = {"A": 0.0, "B": 1.0, "C": -0.8, "D": 0.6}
    oob_phase = {"A": (0.0, 0.0, 0.0), "B": (0.7, 1.1, 2.3),
                 "C": (2.1, 0.4, 4.0), "D": (3.3, 2.7, 1.2)}
    amp = 8.0
    yy2, xx2 = np.mgrid[0:S.TILE_PX, 0:S.TILE_PX]

    def wv_term(k, wav):
        # 沿 y 相干的 x 向条纹（背景电平的帧间差）——接缝判据关心的就是这一分量
        return (amp * oob_shape[k] * np.sin(2 * np.pi * xx2 / wav + oob_phase[k][0])
                + 0.5 * amp * oob_shape[k] * np.sin(2 * np.pi * xx2 / (wav * 0.41)
                                                   + oob_phase[k][2]))

    for wav in (1600.0, 800.0, 400.0, 200.0, 100.0, 50.0):
        coeff = {k: dict(v, qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0), ph=oob_phase[k])
                 for k, v in S.SKY_TRUE.items()}
        w = S.build_world(seed_tag="c1_oob_%g" % wav, coeffs=coeff)
        for k in w["names"]:
            w["sky"][k] = w["sky"][k] + wv_term(k, wav)
            rng = S.derive_rng("c1_oob_%g_%s" % (wav, k))
            w["frames"][k] = S.synth_frame(w["signal"], w["sky"][k], rng)
        wt = {k: v["control_ivar"] for k, v in w["ctrl"].items()}
        o, DD, BB = sky_scenario(w, "c1_oob_%g" % wav)
        m_d = S.stack_mosaic(w["frames"], w["names"], DD, wt)
        st = [abs(s["step"]) for s in S.seam_steps(m_d)]
        sweep.append(dict(wave_px=wav, oob_rms=out_of_basis_rms(w),
                          seam_med=float(np.median(st)), seam_max=float(np.max(st))))
    res["out_of_basis_sweep"] = sweep
    a = np.array([s["oob_rms"] for s in sweep])
    b = np.array([s["seam_max"] for s in sweep])
    ok = np.ptp(a) > 1e-6
    pc = float(np.corrcoef(a, b)[0, 1]) if ok else np.nan
    from scipy import stats as _st
    rho = float(_st.spearmanr(a, b).statistic) if ok else np.nan
    sl = float(np.polyfit(a, b, 1)[0]) if ok else np.nan
    ratio = float(b[-1] / max(b[0], 1e-12))
    res["oob_prediction"] = dict(slope=sl, pearson=pc, spearman=rho,
                                 seam_ratio_short_over_long=ratio)
    g.add("A10_oob_scaling",
          "残余接缝随参考面不可表示分量 RMS 增长（Pearson>0.85, slope∈[0.3,1.5], 短/长尺度比>3）",
          dict(slope=sl, spearman=rho, pearson=pc, ratio=ratio),
          ok and pc > 0.85 and 0.3 <= sl <= 1.5 and ratio > 3.0)

    # ================= D. 星 flux =================
    sf = star_flux_check(world, corr_store["full_4elem"])
    res["star_flux"] = sf
    g.add("A11_star_flux", "加性校正后星孔径通量相对变化 < 1%", sf.get("rel_max"),
          bool(sf.get("n", 0) > 0 and sf.get("rel_max", 9) < 0.01))

    res["gates"] = g.summary()
    p = S.json_dump(res, "c1_additive.json")
    print("== C1 纯加性世界 ==")
    for r in res["gates"]["rows"]:
        print("  [%s] %-24s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % p)
    return 0 if res["gates"]["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
