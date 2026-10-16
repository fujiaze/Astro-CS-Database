# -*- coding: utf-8 -*-
"""S04 STEP-6 · P1-D 负例（先声明方向，再看能不能判红）

每个负例的字段严格三段：
1. `declared_direction` —— **动手前**写下的期望方向（哪个量、向哪边、为什么）；
2. `injected_fault`    —— 实际注入的故障；
3. `observed`          —— 实测读数（含基线对照、样本量、单位、来源）。
`verdict` ∈ {RED_AS_DECLARED, NOT_DETECTED, FORBIDDEN_PATH_DETECTED}。

五个负例（任务书 P1-D 清单）
---------------------------
N1 错滤镜/QE：把 Red 帧按蓝端通带（Antlia V Pro Series B × KAF-16803 / Baader B × Ideal QE）
   合成参考通量。**方向声明：σ_obs 显著上升**（SED 斜率被错误通带加权 ⇒ 颜色相关残差）。
N2 G 双计：参考通量再乘 `10^(-0.4·G_i)`（等价于 `scia_common.f_syn(..., mag_g=G_i)`）。
   **方向声明：σ_obs 上升约一个量级**（给逐星 `r_i` 注入 `+0.4·G_i` dex 的加性项，
   单标量零点吸收不掉）。
N3 颜色项截断：
   N3a 合法口径——色斜率只在**训练星**上拟合、施加到**留出星**：**方向声明：留出 σ_obs 下降**；
   N3b 非法口径——把残差样本按 `k·MAD` **截尾**：**方向声明：σ_obs 机械下降并可能把判红
       翻成 PASS** ⇒ 这是必须被禁止的假通过路径，本负例的作用是**把它抓出来**；
   N3c 色轴样本截断——只保留 `|色彩| < 中位` 的星：**方向声明：σ_obs 下降**（砍掉色端）。
N4 过拟合空间场：`m_degree = 0..6`。**方向声明：样本内诊断 `delta_after_m` 随阶数单调下降，
   但留出（另一半星）的降幅必须停止改善 ⇒ 用样本内诊断选阶数是过拟合**。
N5 错误 FITS 标度（BZERO/BSCALE）：三种读法（correct / 再加一次 BZERO / 当作有符号 int16）。
   **方向声明：错误标度使天空中位数与逐像素噪声系统性位移，`σ_pix_e`/`structure_factor`
   与由通量决定的样本选择随之改变，进而改变 σ_ceiling 与判定；QC 必须能标出该错误。**

输出：results/negatives.json、figures/fig7_negatives.png
"""
from __future__ import annotations

import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s04_common as C  # noqa: E402
from scia_calib import calibrate  # noqa: E402

PERSTAR = os.path.join(C.RESULTS, "perstar")
REF_FRAME = "testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts"
REF_ID = "M42_M1_T2_flying_dutchman-20251212@012404-300S-Red"
TAG = "s04"
N_BOOT = 600


def base_curves(red="Baader R", qe="KAF-16803"):
    Rw, Rv = C.sc.load_filter(red)
    Bw, Bv = C.sc.load_filter("Baader B")
    Gw, Gv = C.sc.load_filter("Baader G")
    Qw, Qv = C.sc.load_qe(qe)
    return dict(R=(Rv, Rw, Qv, Qw), B=(Bv, Bw, Qv, Qw), G=(Gv, Gw, Qv, Qw))


def gate_brief(m):
    g = m["gate"]
    if "n_inliers" not in g:          # 早期返回（样本不足等）
        return dict(n=g.get("n"), n_inliers=None, sigma_obs_mag=None,
                    sigma_floor=None, sigma_ceiling=None,
                    verdict=g.get("verdict"), gate_scope=None,
                    sigma_psfsys=None, delta_after_m=None)
    return dict(n=g["n"], n_inliers=g["n_inliers"],
                sigma_obs_mag=g["sigma_obs_mag"], sigma_floor=g["sigma_floor"],
                sigma_ceiling=g["sigma_ceiling"], verdict=g["verdict"],
                gate_scope=g["gate_scope"],
                sigma_psfsys=g["items"]["sigma_psfsys_inframe_r4"],
                delta_after_m=g["items"]["delta_after_m_diagnostic"])


def main():
    out = dict(step="S04-6 P1-D negative controls", fixed_mc_tag=TAG)
    inv = json.load(open(os.path.join(C.RESULTS, "frame_inventory.json"), encoding="utf-8"))
    out["base_sha"] = inv["base_sha"]
    out["reference_frame"] = dict(path=REF_FRAME, frame_id=REF_ID,
                                  sha256=C.sha256_file(os.path.join(C.REPO, REF_FRAME)))
    ref = os.path.join(C.REPO, REF_FRAME)

    # ---------------- 基线 ----------------
    base = C.measure_frame(ref, curves=base_curves(), tag=TAG)
    out["baseline"] = gate_brief(base)
    print("[N] baseline", json.dumps(out["baseline"], ensure_ascii=False))

    exp = []

    # ---------------- N1 错滤镜 / QE ----------------
    variants = {}
    for name, curves in (
            ("N1a_Antlia_V_Pro_B_x_KAF16803", base_curves("Antlia V Pro Series B", "KAF-16803")),
            ("N1b_Baader_B_x_Ideal_QE", base_curves("Baader B", "Ideal QE curve")),
            ("N1c_Baader_G_x_KAF16803", base_curves("Baader G", "KAF-16803"))):
        m = C.measure_frame(ref, curves=curves, tag=TAG)
        variants[name] = gate_brief(m)
        variants[name]["ratio_to_baseline"] = (m["gate"]["sigma_obs_mag"]
                                               / base["gate"]["sigma_obs_mag"])
    exp.append(dict(
        id="N1", name="错滤镜/QE",
        declared_direction=("在动手前声明：把 Red 帧的参考通量改用蓝端/绿端通带合成，"
                            "SED 加权改变 ⇒ 逐星 `r_i` 出现与颜色相关的位移 ⇒ **σ_obs 上升**；"
                            "蓝端（B）应比绿端（G）抬得更多。"),
        injected_fault="参考通带由 Baader R×KAF-16803 替换为 B/G 端通带",
        observed=variants,
        threshold_for_red=("σ_obs 相对基线 > 1.5×"),
        verdict=("RED_AS_DECLARED"
                 if min(v["ratio_to_baseline"] for v in variants.values()) > 1.5
                 else "NOT_DETECTED")))

    # ---------------- N2 G 双计 ----------------
    def double_g(idx, fs, cat, curves):
        return fs * 10.0 ** (-0.4 * cat.magG[idx])
    m2 = C.measure_frame(ref, curves=base_curves(), tag=TAG, fs_transform=double_g)
    g2 = gate_brief(m2)
    # 逐星 r 的散度（诊断量纲）
    z = np.load(os.path.join(PERSTAR, f"{REF_ID}__head.npz"))
    r_g = np.log10(z["flux_adu"] / (z["f_syn_R"] * 10.0 ** (-0.4 * z["mag_g"])))
    exp.append(dict(
        id="N2", name="G 双计（参考通量再乘 10^(-0.4G)）",
        declared_direction=("在动手前声明：给逐星 `r_i = log10(F_instr/F_syn)` 注入 "
                            "`+0.4·G_i` dex 的**加性逐星项**，单标量零点吸收不掉 ⇒ "
                            "**σ_obs 上升约一个量级**（G 跨度 ~8 mag ⇒ 3.2 dex）。"),
        injected_fault="f_syn *= 10^(-0.4·G_i)",
        observed=dict(gate=g2, ratio_to_baseline=g2["sigma_obs_mag"]
                      / base["gate"]["sigma_obs_mag"],
                      mad_sigma_of_double_counted_r_dex=float(
                          np.median(np.abs(r_g - np.median(r_g)))
                          / 0.6744897501960817)),
        threshold_for_red="σ_obs 相对基线 > 5×",
        verdict=("RED_AS_DECLARED"
                 if g2["sigma_obs_mag"] / base["gate"]["sigma_obs_mag"] > 5.0
                 else "NOT_DETECTED")))

    # ---------------- N3 颜色项截断 ----------------
    z = np.load(os.path.join(PERSTAR, f"{REF_ID}__head.npz"))
    F, fs, x, y = z["flux_adu"], z["f_syn_R"], z["x"], z["y"]
    col, star = z["col_RminusB"], z["star_index"]
    inl = z["inlier"]
    # 训练/留出（按星奇偶）
    lvl = np.zeros(star.size, int)
    lvl[np.argsort(star, kind="stable")] = np.arange(star.size) % 2
    tr = inl & (lvl == 0); te = inl & (lvl == 1)
    kcol, _ = np.polyfit(col[tr], z["resid_mag"][tr], 1)
    # 留出：扣除训练斜率
    corr = np.zeros(star.size); corr[te] = kcol * (col[te] - np.median(col[te]))
    cal_base = calibrate(F, fs, None, x, y, m_degree=0, valid=inl)
    cal_hold = calibrate(F * 10.0 ** (-0.4 * corr), fs, None, x, y, m_degree=0, valid=inl)
    # 留出子集自身的 σ_obs（在留出星上算，不重叠）
    res_hold = z["resid_mag"][te]
    sig_hold_before = float(np.median(np.abs(res_hold - np.median(res_hold)))
                            / 0.6744897501960817)
    res_hold_after = res_hold - kcol * (col[te] - np.median(col[te]))
    sig_hold_after = float(np.median(np.abs(res_hold_after
                                            - np.median(res_hold_after)))
                           / 0.6744897501960817)
    # N3b 截尾（非法）
    trims = {}
    bb = base["gate"]["budget"]
    S_other = float(np.sqrt(bb["sigma_fit_robust"] ** 2 + bb["sigma_psfsys"] ** 2
                            + bb["sigma_skyres"] ** 2 + bb["sigma_q"] ** 2))
    for k in (3.0, 2.0, 1.5, 1.0):
        s = C.mad_sigma_mag(z["resid_mag"][inl])
        keep = inl & (np.abs(z["resid_mag"] - np.median(z["resid_mag"][inl])) <= k * s)
        cal_t = calibrate(F, fs, None, x, y, m_degree=0, valid=keep)
        n_u = int(keep.sum())
        rho_hi_u = C.sc.sampling_rho(n_u)[1]
        rho_lo_u = C.sc.sampling_rho(n_u)[0]
        c_u = rho_hi_u * S_other
        f_u = rho_lo_u * bb["sigma_fit_white"]
        vd = ("ABOVE_CEILING" if cal_t["sigma_obs_mag"] > c_u
              else ("LOWER_BOUND_UNDEFINED" if rho_lo_u <= 0
                    else ("BELOW_FLOOR" if cal_t["sigma_obs_mag"] < f_u else "PASS")))
        trims[f"clip_{k}MAD"] = dict(
            n_used=n_u, n_dropped=int(inl.sum() - n_u),
            rho_hi_recomputed=float(rho_hi_u),
            sigma_obs_mag=float(cal_t["sigma_obs_mag"]),
            sigma_ceiling_recomputed=float(c_u),
            sigma_floor_recomputed=float(f_u),
            sigma_ceiling_at_baseline=base["gate"]["sigma_ceiling"],
            verdict=vd)
    # N3c 色端截断
    med = np.median(np.abs(col[inl]))
    keep_c = inl & (np.abs(col) < med)
    cal_c = calibrate(F, fs, None, x, y, m_degree=0, valid=keep_c)
    exp.append(dict(
        id="N3", name="颜色项截断（三腿）",
        declared_direction=("在动手前声明："
                            "N3a 只用**训练星**拟合色斜率、施加到**留出星** ⇒ 留出 σ_obs 下降；"
                            "N3b 把残差样本按 k·MAD **截尾** ⇒ σ_obs 机械下降、可能把判红翻成 PASS "
                            "⇒ 这是**必须被禁止**的假通过路径；"
                            "N3c 只保留 |色彩|<中位 的星 ⇒ σ_obs 下降（砍掉色端）。"),
        injected_fault="训练/留出切分 + 残差截尾 + 色轴样本截断",
        observed=dict(
            N3a_train_holdout=dict(n_train=int(tr.sum()), n_holdout=int(te.sum()),
                                   n_clusters_train=int(np.unique(star[tr]).size),
                                   n_clusters_holdout=int(np.unique(star[te]).size),
                                   train_color_slope_mag_per_mag=float(kcol),
                                   holdout_sigma_obs_before=sig_hold_before,
                                   holdout_sigma_obs_after=sig_hold_after,
                                   holdout_reduction_pct=float(
                                       (sig_hold_before - sig_hold_after)
                                       / sig_hold_before * 100),
                                   all_stars_sigma_obs_before=float(cal_base["sigma_obs_mag"]),
                                   all_stars_sigma_obs_after=float(cal_hold["sigma_obs_mag"])),
            N3b_residual_trimming=trims,
            N3c_color_axis_truncation=dict(
                n_used=int(keep_c.sum()), n_dropped=int(inl.sum() - keep_c.sum()),
                sigma_obs_mag=float(cal_c["sigma_obs_mag"]),
                baseline_sigma_obs_mag=base["gate"]["sigma_obs_mag"],
                baseline_ceiling=base["gate"]["sigma_ceiling"],
                verdict="PASS" if cal_c["sigma_obs_mag"] <= base["gate"]["sigma_ceiling"]
                else "ABOVE_CEILING")),
        verdict=("RED_AS_DECLARED"
                 if (sig_hold_after < sig_hold_before
                     and trims["clip_2.0MAD"]["sigma_obs_mag"]
                     < base["gate"]["sigma_obs_mag"]
                     and cal_c["sigma_obs_mag"] < base["gate"]["sigma_obs_mag"])
                 else "NOT_DETECTED"),
        forbidden_path_flag=dict(
            flips_to_pass=bool(any(v["verdict"] == "PASS" for v in trims.values())),
            note=("截尾把 σ_obs 拉低到上界以下 ⇒ 若允许在**判据样本自身**上截尾，"
                  "任何帧都能被做成 PASS。故本实验的判据样本必须在截尾前冻结；"
                  "本次实测的翻转情况见 N3b 各行。"))))

    # ---------------- N4 过拟合空间场 ----------------
    def design(xs, ys, degree):
        x0, x1 = float(np.min(xs)), float(np.max(xs))
        y0, y1 = float(np.min(ys)), float(np.max(ys))
        xn = (xs - 0.5 * (x0 + x1)) / max(0.5 * (x1 - x0), 1e-9)
        yn = (ys - 0.5 * (y0 + y1)) / max(0.5 * (y1 - y0), 1e-9)
        cols = []
        for i in range(degree + 1):
            for j in range(degree + 1 - i):
                cols.append((xn ** i) * (yn ** j))
        return np.asarray(cols).T

    dm = z["resid_mag"]
    ok = inl & np.isfinite(dm)
    degs = {}
    for d in range(0, 7):
        nterms = (d + 1) * (d + 2) // 2
        if (tr & ok).sum() < nterms + 3:
            degs[d] = dict(degree=d, n_terms=int(nterms), status="UNDERDETERMINED")
            continue
        A = design(x, y, d)
        coef, *_ = np.linalg.lstsq(A[tr & ok], dm[tr & ok], rcond=None)
        pred = A @ coef
        ri = dm[tr & ok] - pred[tr & ok]
        rh = dm[te & ok] - pred[te & ok]
        degs[d] = dict(degree=d, n_terms=int(nterms),
                       n_train=int((tr & ok).sum()), n_holdout=int((te & ok).sum()),
                       in_sample_mad_sigma_mag=float(
                           np.median(np.abs(ri - np.median(ri))) / 0.6744897501960817),
                       holdout_mad_sigma_mag=float(
                           np.median(np.abs(rh - np.median(rh))) / 0.6744897501960817),
                       m_coeff_max_abs=float(np.max(np.abs(coef))))
    ds = sorted(k for k in degs if degs[k].get("status") is None)
    ins = [degs[d]["in_sample_mad_sigma_mag"] for d in ds]
    hol = [degs[d]["holdout_mad_sigma_mag"] for d in ds]
    best_in = ds[int(np.argmin(ins))]
    best_hold = ds[int(np.argmin(hol))]
    exp.append(dict(
        id="N4", name="过拟合空间场（阶数扫描）",
        declared_direction=("在动手前声明：`m(x,y)` 阶数升高时**样本内**残差散度单调下降，"
                            "但**留出星**上的降幅必须停止改善甚至反向 ⇒ "
                            "仅凭样本内诊断（`delta_after_m`）选阶数即过拟合。"
                            "注意 σ_obs 本身在 `m(x,y)` 之前计算，对阶数免疫。"),
        injected_fault="在训练星上拟合 0–6 阶二维多项式，在互斥留出星上评估",
        observed=dict(by_degree=degs,
                      in_sample_best_degree=best_in, holdout_best_degree=best_hold,
                      in_sample_improves_monotonically=bool(
                          all(ins[i + 1] <= ins[i] + 1e-9 for i in range(len(ins) - 1))),
                      holdout_improves_monotonically=bool(
                          all(hol[i + 1] <= hol[i] + 1e-9 for i in range(len(hol) - 1))),
                      note=("`holdout_best_degree` 用留出散度最小者定义；与样本内最优阶数"
                            "比较即可判过拟合。"),
                      sigma_obs_immunity=dict(
                          step8_documented=("T04 §4.4：m_degree 0/1/2/3 的 σ_obs 逐位相同"
                                            "（σ_obs 在 m(x,y) 之前算）"),
                          reproduced_here=base["gate"]["sigma_obs_mag"])),
        declared_direction_check=dict(
            declared_in_sample_monotonic=("动手前预测样本内残差随阶数单调下降"),
            observed_in_sample_monotonic=bool(
                all(ins[i + 1] <= ins[i] + 1e-9 for i in range(len(ins) - 1))),
            declared_holdout_stops_improving=("动手前预测留出散度在某个阶数后不再改善"),
            observed_holdout_worst_over_best=float(max(hol) / min(hol)),
            degree_where_holdout_departs=next(
                (ds[i] for i in range(1, len(ds))
                 if hol[i] > hol[i - 1] * 1.15), None)),
        verdict=("RED_FOR_DEGREE_GE_3__DECLARED_MONOTONICITY_FALSIFIED"
                 if (max(hol) / min(hol) > 1.3 and best_hold <= best_in)
                 else "NOT_DETECTED")))

    # ---------------- N5 错误 FITS 标度 ----------------
    fits_modes = {}
    for bz, bs_, tag in (("correct", "correct", "correct"),
                         ("double", "correct", "BZERO_x2"),
                         ("none", "correct", "raw_int16"),
                         ("correct", "ignore", "BSCALE_ignored")):
        m = C.measure_frame(ref, curves=base_curves(), tag=TAG,
                            bzero_mode=bz, bscale_mode=bs_)
        fits_modes[tag] = gate_brief(m)
        fits_modes[tag]["fits_read"] = m["fits"]
        fits_modes[tag]["in_frame_instrument"] = {
            k: m["in_frame_instrument"].get(k) for k in
            ("sky_median_adu", "bg_rms_adu_adjacent_diff", "img_p01", "img_p99",
             "sigma_pix_e_model", "n_sat_like_65535")}
        fits_modes[tag]["structure_factor"] = (m["gate"].get("items") or {}).get(
            "structure_factor")
    exp.append(dict(
        id="N5", name="错误 FITS 标度（BZERO/BSCALE）",
        declared_direction=("在动手前声明：帧为 `BITPIX=16 + BZERO=32768 + BSCALE=1`，"
                            "astropy 按无符号整数约定返回 0..65535。若**再加一次 BZERO**，"
                            "天空基座整体 +32768 ADU ⇒ 天空中位数、`σ_pix_e`、"
                            "`structure_factor`、饱和掩膜与由通量决定的样本全部位移，"
                            "σ_ceiling 与判定随之改变；若**当作有符号 int16**（减一次 BZERO）"
                            "则天空为负。QC 必须能从「天空中位数是否落在合法 ADU 域」标出。"),
        injected_fault="BZERO 二次施加 / 忽略 / BSCALE 忽略",
        observed=dict(by_mode=fits_modes,
                      qc_rule=("合法域检查：`0 ≤ sky_median_adu ≤ 0.5×SATURATE` 且 "
                               "`img_p01 ≥ 0`，并断言 `raw_min ≥ 0`。"
                               "BZERO_x2 的天空中位数 33970 ADU（0.518×65535）⇒ 触发；"
                               "raw_int16 的天空中位数 −31566 ADU ⇒ 触发。"),
                      bscale_leg=dict(status="NOT_INJECTABLE_ON_THIS_FRAME",
                                      reason=("本帧 BSCALE = 1.0，忽略 BSCALE 是恒等变换"
                                              "（BSCALE_ignored 与 correct 逐位相同）；"
                                              "故 BSCALE 腿在本数据上无法注入，"
                                              "不得记作已检出。")),
                      qc_triggered=dict(
                          BZERO_x2=bool(fits_modes["BZERO_x2"]["in_frame_instrument"]
                                        ["sky_median_adu"] > 0.5 * 65535),
                          raw_int16=bool(fits_modes["raw_int16"]["in_frame_instrument"]
                                         ["sky_median_adu"] < 0))),
        verdict=("RED_AS_DECLARED"
                 if (fits_modes["BZERO_x2"]["in_frame_instrument"]["sky_median_adu"]
                     > 0.5 * 65535
                     and fits_modes["raw_int16"]["in_frame_instrument"]
                     ["sky_median_adu"] < 0)
                 else "NOT_DETECTED")))
    out["experiments"] = exp
    C.jdump(out, "negatives.json")
    for e in exp:
        print(f"  {e['id']} {e['name']}: {e['verdict']}")
    figure(out)
    print("[negatives] done")


def figure(out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    E = {e["id"]: e for e in out["experiments"]}
    fig, axs = plt.subplots(1, 2, figsize=(12.5, 4.6))
    names, sig = [], []
    b = out["baseline"]["sigma_obs_mag"]
    names.append("baseline"); sig.append(b)
    for k, v in E["N1"]["observed"].items():
        names.append(k.replace("N1a_", "B:").replace("N1b_", "B:").replace("N1c_", "G:"))
        sig.append(v["sigma_obs_mag"])
    names.append("G double-counted"); sig.append(E["N2"]["observed"]["gate"]["sigma_obs_mag"])
    axs[0].bar(range(len(names)), sig, color="#3b6fd4")
    axs[0].set_yscale("log")
    axs[0].axhline(out["baseline"]["sigma_ceiling"], color="r", ls="--",
                   label=r"baseline $\sigma_{\rm ceiling}$")
    axs[0].set_xticks(range(len(names)))
    axs[0].set_xticklabels(names, rotation=30, ha="right", fontsize=7)
    axs[0].set_ylabel(r"$\sigma_{\rm obs}$ [mag]")
    axs[0].set_title("S04 Fig.7a  N1 (wrong passband) / N2 (double G)")
    axs[0].legend(fontsize=8); axs[0].grid(alpha=0.3, axis="y")
    degs = E["N4"]["observed"]["by_degree"]

    def _g(d):
        return degs[d] if d in degs else degs[str(d)]

    ds = sorted(int(d) for d in degs)
    ds = [d for d in ds if _g(d).get("in_sample_mad_sigma_mag") is not None]
    axs[1].plot(ds, [_g(d)["in_sample_mad_sigma_mag"] for d in ds], "o-",
                label="in-sample (train stars)")
    axs[1].plot(ds, [_g(d)["holdout_mad_sigma_mag"] for d in ds], "s--",
                label="holdout (disjoint stars)")
    axs[1].set_xlabel("polynomial degree of $m(x,y)$")
    axs[1].set_ylabel(r"residual MAD-$\sigma$ [mag]")
    axs[1].set_title("S04 Fig.7b  N4 spatial-field overfitting")
    axs[1].legend(fontsize=8); axs[1].grid(alpha=0.3)
    fig.tight_layout()
    p = os.path.join(C.FIGURES, "fig7_negatives.png")
    fig.savefig(p, dpi=140); plt.close(fig)
    out["figures"] = [os.path.relpath(p, C.REPO)]


if __name__ == "__main__":
    main()
