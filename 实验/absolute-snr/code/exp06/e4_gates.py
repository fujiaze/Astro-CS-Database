#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-06 门与故障注入（**能红能绿**）。

每条门都带显式阈值、实测值与预期方向；「错误做法」类门要求**必须判红**，
否则该门本身 FAIL（判据退化）。全部结论来自本脚本实测，不接受恒真门。

固定 seed：20260927。不运行任何 ACSD 可执行文件。
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp06_common as X  # noqa: E402
from exp05_common import weight_efficiency as X5_weight_efficiency  # noqa: E402

SEED = X.SEED
SHAPE = (1024, 1024)
DELTA = 64
EDGE_MARGIN = 1


def _scene(**kw) -> Dict[str, Any]:
    return X.analytic_scene(SHAPE, seed=SEED + 7, **kw)


def _E(ev: Dict[str, Any], key: str, truth: str = "vs_T1_var_bg") -> float:
    """默认口径 = T1（var_bg，ACSD 帧级 SNR 的冻结口径）；T2 只在专门的门里用。"""
    return float(ev.get(key, {}).get(truth, {}).get("eff_loss", float("nan")))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp06_e4_gates.json"))
    a = ap.parse_args()
    t0 = time.time()
    gates: List[Dict[str, Any]] = []
    detail: Dict[str, Any] = {}
    stars = dict(n_stars=80, star_flux_log10=(3.0, 6.0))

    # ---------------- G1 平坦真值场：重建场必须退化为常数 ----------------
    sc = _scene(sky_e_per_s=0.5)
    built = X.build_fields(sc["adu"], gain=sc["meta"]["gain"], delta=DELTA,
                           edge_margin=EDGE_MARGIN)
    ev = X.evaluate_fields(built["fields"], sc["truth"])
    disp = {k: v["dispersion"] for k, v in ev.items() if not k.startswith("_")}
    d_phys = disp.get("phys", float("nan"))
    d_int = disp.get("interp_spline", float("nan"))
    d_fr = disp.get("frame_scalar", float("nan"))
    detail["G1_dispersion"] = disp
    X.gate(gates, "G1a_flat_truth_physics_field_is_constant", bool(d_phys <= 1e-2),
           "平坦天光 + 无源（真值无空间效应）⇒ 物理重建场必须退化为常数；实测离散度 %.3g" % d_phys,
           {"dispersion_phys": d_phys, "per_method": disp}, expect="dispersion <= 1e-2")
    X.gate(gates, "G1b_flat_truth_physics_flatter_than_interpolation",
           bool(np.isfinite(d_phys) and np.isfinite(d_int) and d_phys <= d_int / 5.0),
           "同一平坦真值场上，物理重建的伪空间结构必须比纯插值至少小 5 倍；"
           "实测 phys=%.3g interp=%.3g frame=%.3g（比 %.1f）"
           % (d_phys, d_int, d_fr, (d_int / d_phys) if d_phys > 0 else float("inf")),
           {"phys": d_phys, "interp": d_int, "frame": d_fr}, expect="disp <= disp_interp/5")

    # ---------------- G2 零噪声真值：度量必须判退化 ----------------
    scz = X.analytic_scene(SHAPE, seed=SEED + 7, sky_e_per_s=0.0, dark_e_per_s=0.0,
                           read_noise_e=0.0, n_stars=0, noise=False, quantize=False)
    emask = X.eval_mask_of(scz["adu"].shape)
    is_zero = X.degenerate_zero_truth(scz["truth"]["var_bg"], emask)
    s_r1 = X.sigma_ctrl_resid(scz["adu"], DELTA)
    rz = X.metrics(X.recon_frame_scalar(s_r1, scz["adu"].shape), scz["truth"]["sigma_bg"], emask)
    detail["G2"] = {"truth_is_zero": bool(is_zero), "degenerate_flag": bool(rz.get("degenerate", False)),
                    "level_ratio": rz.get("level_ratio"), "n_eval": rz.get("n_eval")}
    X.gate(gates, "G2_zero_noise_truth_must_flag_degenerate",
           bool(is_zero and rz.get("degenerate", False)),
           "零噪声真值：真值方差恒 0 ⇒ 度量必须显式判退化（不得静默产出数字）；"
           "实测 degenerate=%s n_eval=%s" % (rz.get("degenerate"), rz.get("n_eval")),
           detail["G2"], expect="degenerate=True")

    # ---------------- G3 平坦场 + 极亮像素：分离必须挡住，逐像素代入必须红 ----------------
    scb = _scene(sky_e_per_s=0.5, n_stars=6, star_flux_log10=(6.5, 7.0))
    bb = X.build_fields(scb["adu"], gain=scb["meta"]["gain"], delta=DELTA,
                        edge_margin=EDGE_MARGIN)
    eb = X.evaluate_fields(bb["fields"], scb["truth"])
    d_phys = eb["phys"]["dispersion"]
    src_naive = eb["naive_pixel"]["vs_T1_var_bg"].get("src_snr_p05", float("nan"))
    src_phys = eb["phys"]["vs_T1_var_bg"].get("src_level_ratio", float("nan"))
    detail["G3"] = {"phys_dispersion": d_phys, "phys_src_level_ratio": src_phys,
                    "naive_src_level_ratio": src_naive}
    X.gate(gates, "G3a_separation_blocks_bright_pixels", bool(d_phys <= 1e-2),
           "平坦天光 + 6 颗极亮星（flux 1e6.5~1e7 e-）⇒ 结构分离后物理场仍须近似常数；"
           "实测离散度 %.3g" % d_phys, d_phys, expect="dispersion <= 1e-2")
    X.gate(gates, "G3b_naive_pixel_substitution_must_be_red", bool(src_naive <= 0.5),
           "逐像素代入亮度：源像素上 sigma 被高估 ⇒ SNR 伪暗洞必须判红；实测源区 SNR 比值 p05 = %.3f"
           % src_naive, src_naive, expect="src_snr_p05 <= 0.5（错误臂必须红）")

    # ---------------- G4 真实结构上：逐像素代入的源区 SNR 伪暗洞 ----------------
    scr = _scene(sky_e_per_s=0.5, sky_grad_e_per_s=0.4, sky_theta_deg=25.0,
                 blobs=[{"cy":600, "cx":400, "sigma_px":300, "amp_e_per_s":1.2}],
                 filament={"y1":150, "x1":60, "y2":860, "x2":940, "width_px":3.0, "amp_e":8e3},
                 cr_rate_per_frame=400.0, **stars)
    br = X.build_fields(scr["adu"], gain=scr["meta"]["gain"], delta=DELTA,
                        edge_margin=EDGE_MARGIN)
    er = X.evaluate_fields(br["fields"], scr["truth"])
    p05_naive = er["naive_pixel"]["vs_T1_var_bg"].get("src_snr_p05", float("nan"))
    p05_phys = er["phys"]["vs_T1_var_bg"].get("src_snr_p05", float("nan"))
    E_phys = _E(er, "phys"); E_int = _E(er, "interp_spline"); E_naive = _E(er, "naive_pixel")
    E_unsep = _E(er, "phys_unsep"); E_frame = _E(er, "frame_scalar")
    detail["G4"] = {"naive_src_snr_p05": p05_naive, "phys_src_snr_p05": p05_phys,
                    "E_phys": E_phys, "E_interp": E_int, "E_naive": E_naive,
                    "E_unsep": E_unsep, "E_frame": E_frame}
    X.gate(gates, "G4a_naive_pixel_source_snr_dip_must_be_red", bool(p05_naive <= 0.5),
           "逐像素代入亮度：源像素 SNR 比值的 p05 必须显著低于 1（伪暗洞）；实测 %.3f" % p05_naive,
           p05_naive, expect="p05 <= 0.5（错误臂必须红）")
    snr_max_phys = [("A1_flat_bright_pixels",
                     eb["phys"]["vs_T1_var_bg"].get("snr_max_all", float("nan"))),
                    ("A4_realistic",
                     er["phys"]["vs_T1_var_bg"].get("snr_max_all", float("nan")))]
    detail["G4c"] = dict(snr_max_phys)
    X.gate(gates, "G4c_physics_no_snr_overshoot_anywhere",
           bool(all(np.isfinite(x) and x <= 1.10 for _, x in snr_max_phys)),
           "负责人点名的失效模式是「异常**高**信噪比」：口径 F 下物理重建的**全帧最大** SNR 比"
           "必须有上界（不得在星点/线状结构上伪造高 SNR）；实测 %s"
           % ", ".join("%s=%.4f" % (nm, x) for nm, x in snr_max_phys), detail["G4c"],
           expect="snr_max_all <= 1.10（全部场景）")
    X.gate(gates, "G4b_physics_source_zone_clean", bool(np.isfinite(p05_phys) and p05_phys >= 0.9),
           "物理重建在源像素上不得产生 SNR 伪结构；实测源区 SNR p05 = %.3f" % p05_phys,
           p05_phys, expect="p05 >= 0.9")

    # ---------------- G5 结构分离的必要性 ----------------
    X.gate(gates, "G5_no_separation_must_be_red", bool(np.isfinite(E_unsep) and E_unsep >= 2.0 * E_phys),
           "不做结构分离（大尺度高斯平滑原始帧作驱动量）⇒ 权重效率损失必须显著变差；"
           "实测 E_unsep=%.5f vs E_phys=%.5f（比 %.2f）"
           % (E_unsep, E_phys, (E_unsep / E_phys) if E_phys > 0 else float("inf")),
           {"E_unsep": E_unsep, "E_phys": E_phys}, expect="E_unsep >= 2*E_phys")

    # ---------------- G6 洗牌负例 ----------------
    rng = np.random.default_rng(SEED + 99)
    ctrl = br["ctrl"]
    s_sh = ctrl["s_r1"].copy()
    flat = s_sh[ctrl["valid"]]
    rng.shuffle(flat)
    s_sh[ctrl["valid"]] = flat
    fit_sh = X.fit_nlf(ctrl["D_sep"], s_sh, valid=ctrl["valid"])   # 自由斜率：配对信息才起作用
    fld_sh = X.recon_physics(fit_sh, ctrl["B_sep"])
    ev_sh = X.evaluate_fields({"shuffled": fld_sh}, scr["truth"])
    E_sh = _E(ev_sh, "shuffled")
    detail["G6"] = {"E_shuffled": E_sh, "E_frame": E_frame, "E_phys": E_phys,
                    "c_shuffled": fit_sh.get("c"), "c_true": br["fits"]["free"].get("c")}
    X.gate(gates, "G6a_shuffled_controls_destroy_the_brightness_law",
           bool(np.isfinite(E_sh) and E_sh >= 10.0 * max(E_phys, 1e-12)),
           "控制点值随机洗牌（破坏与亮度的配对）⇒ 物理模型的增益信息被破坏，必须显著劣于物理臂；"
           "实测 E_shuffled=%.5f vs E_phys=%.5f（比 %.1f）；洗牌后斜率 %.4f vs 正确 %.4f"
           % (E_sh, E_phys, E_sh / max(E_phys, 1e-12), fit_sh.get("c", float("nan")),
              br["fits"]["free"].get("c", float("nan"))), detail["G6"],
           expect="E_shuffled >= 10*E_phys")
    X.gate(gates, "G6b_shuffled_model_degenerates_to_frame_scalar",
           bool(np.isfinite(E_sh) and abs(E_sh / max(E_frame, 1e-12) - 1.0) <= 0.5),
           "洗牌后模型退化为帧级标量（亮度项归零）⇒ E 必须与帧级臂同量级；"
           "实测 E_shuffled=%.5f vs E_frame=%.5f" % (E_sh, E_frame), detail["G6"],
           expect="|E_shuffled/E_frame - 1| <= 0.5")

    # ---------------- G7/G8 干净场景上的斜率（=1/g）与幂律指数 ----------------
    gains, powers = [], []
    for nm, kw in (("A2_gradient", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.5, sky_theta_deg=30.0)),
                   ("A6_strong_gradient", dict(sky_e_per_s=0.6, sky_grad_e_per_s=2.4,
                                               sky_theta_deg=15.0, **stars))):
        s = _scene(**kw)
        b = X.build_fields(s["adu"], gain=s["meta"]["gain"], delta=DELTA, edge_margin=EDGE_MARGIN)
        gains.append({"scene": nm, "gain_hat": b["fits"]["free"].get("gain_hat"),
                      "gain_true": s["meta"]["gain"],
                      "rel_dev": (b["fits"]["free"].get("gain_hat", float("nan"))
                                  / s["meta"]["gain"] - 1.0),
                      "lever_var": b["diag"]["lever_var"],
                      "c_se_over_c": (b["fits"]["free"].get("c_se", float("nan"))
                                      / max(b["fits"]["free"].get("c", float("nan")), 1e-300))})
        powers.append({"scene": nm, "p": b["diag"]["power"].get("p"),
                       "p_se": b["diag"]["power"].get("p_se"),
                       "consistent_with_1": b["diag"]["power"].get("p_consistent_with_1"),
                       "p_se_degenerate": b["diag"]["power"].get("p_se_degenerate"),
                       "p_at_bound": b["diag"]["power"].get("p_at_bound")})
    worst_g = max(abs(g["rel_dev"]) for g in gains)
    detail["G7_gain_recovery"] = gains
    detail["G8_power_law"] = powers
    X.gate(gates, "G7_gain_recovery_on_clean_backgrounds", bool(worst_g <= 0.05),
           "弥散分量在 cell 尺度上足够平滑的场景（线性/强梯度）⇒ 自由斜率必须还原 1/g，偏差 <= 5%%；"
           "实测最差 %.2f%%" % (100.0 * worst_g), gains, expect="|gain_hat/g - 1| <= 5%")
    X.gate(gates, "G8a_power_law_exponent_consistent_with_1",
           bool(powers and all(p["consistent_with_1"] is True for p in powers)),
           "物理模型预言 Var 对电平的幂律指数 p = 1；自由指数拟合必须与 1 在 3 sigma 内一致",
           powers, expect="p 与 1 在 3 sigma 内一致")

    X.gate(gates, "G8b_power_law_diagnostic_itself_not_degenerate",
           bool(powers and all(not p.get("p_se_degenerate") and not p.get("p_at_bound")
                               for p in powers)),
           "幂律诊断自身必须非退化：p_se 不得为零（否则 |p-1|<=3*p_se 退化为恒真门），"
           "p 不得触到拟合上界 3.0；实测 %s"
           % "; ".join("%s p=%.3f se=%.3g%s%s" % (p["scene"], p["p"], p["p_se"],
                                                 " [se=0 退化]" if p.get("p_se_degenerate") else "",
                                                 " [触界]" if p.get("p_at_bound") else "")
                      for p in powers),
           powers, expect="全部 p_se > 1e-9 且 p 不触界")

    # ---------------- G9 物理建模 vs 插值框架 ----------------
    better = []
    for nm, kw in (("A2_gradient", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.5)),
                   ("A4_realistic", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.4,
                                         blobs=[{"cy":600, "cx":400, "sigma_px":300, "amp_e_per_s":1.2}],
                                         filament={"y1":150, "x1":60, "y2":860, "x2":940,
                                                   "width_px":3.0, "amp_e":8e3},
                                         cr_rate_per_frame=400.0, **stars))):
        s = _scene(**kw)
        b = X.build_fields(s["adu"], gain=s["meta"]["gain"], delta=DELTA, edge_margin=EDGE_MARGIN)
        e = X.evaluate_fields(b["fields"], s["truth"])
        better.append({"scene": nm, "E_phys": _E(e, "phys"), "E_interp": _E(e, "interp_spline"),
                       "E_phys_resid": _E(e, "phys_resid")})
    detail["G9_phys_vs_interp"] = better
    X.gate(gates, "G9_physics_beats_interpolation_on_smooth_backgrounds",
           bool(all(x["E_phys"] < x["E_interp"] for x in better)),
           "平滑背景（弥散分量在 cell 尺度可分辨）上，物理建模的权重效率损失必须优于纯插值",
           better, expect="E_phys < E_interp（全部场景）")

    # ---------------- G10 常数驱动量注入（亮度信息必须真的在起作用） ----------------
    s6 = _scene(sky_e_per_s=0.6, sky_grad_e_per_s=2.4, sky_theta_deg=15.0, **stars)
    b6 = X.build_fields(s6["adu"], gain=s6["meta"]["gain"], delta=DELTA, edge_margin=EDGE_MARGIN)
    e6 = X.evaluate_fields(b6["fields"], s6["truth"])
    fld_const = X.recon_physics(b6["fits"]["fix"],
                                np.full(s6["adu"].shape, float(b6["fits"]["fix"]["D_ref"])))
    e6c = X.evaluate_fields({"const_driver": fld_const}, s6["truth"])
    E_c = _E(e6c, "const_driver")
    E_p6 = _E(e6, "phys")
    detail["G10"] = {"E_const_driver": E_c, "E_phys": E_p6, "E_frame": _E(e6, "frame_scalar")}
    X.gate(gates, "G10_constant_driver_injection_must_be_red", bool(E_c >= 2.0 * E_p6),
           "把驱动量置为常数（等价于丢掉亮度信息）⇒ 必须显著劣于物理重建；"
           "实测 E_const=%.5f vs E_phys=%.5f" % (E_c, E_p6), detail["G10"],
           expect="E_const >= 2*E_phys")

    # ---------------- G11 错误增益注入 ----------------
    fit_bad = X.fit_nlf(b6["ctrl"]["D_sep"], b6["ctrl"]["s_r1"], valid=b6["ctrl"]["valid"],
                        fix_c=1.0 / (2.0 * s6["meta"]["gain"]))
    fld_bad = X.recon_physics(fit_bad, b6["ctrl"]["B_sep"])
    e6b = X.evaluate_fields({"wrong_gain": fld_bad}, s6["truth"])
    E_b = _E(e6b, "wrong_gain")
    detail["G11"] = {"E_wrong_gain": E_b, "E_phys": E_p6}
    X.gate(gates, "G11_wrong_gain_injection_must_be_red", bool(E_b >= 2.0 * E_p6),
           "把帧级增益用错一倍（斜率 1/(2g)）⇒ 必须显著劣于正确增益；"
           "实测 E_wrong=%.5f vs E_phys=%.5f" % (E_b, E_p6), detail["G11"],
           expect="E_wrong >= 2*E_phys")

    # ---------------- G12 caliber P（逐像素显著性）的异常高信噪比 ----------------
    emask6 = X.eval_mask_of(s6["adu"].shape)
    sig_phys = b6["fields"]["phys"]
    Dhat = b6["ctrl"]["B_sep"]
    sig_sig = (s6["adu"] - Dhat) / sig_phys            # caliber P：逐像素显著性
    m6 = emask6 & np.isfinite(sig_sig)
    dyn = float(np.percentile(sig_sig[m6], 99.9) / max(np.percentile(sig_sig[m6], 50), 1e-12))
    # caliber P 的**权重**（w = SNR_P^2 = ((I-D)/sigma)^2）的效率损失
    var_true = s6["truth"]["var_local"]
    mm6 = emask6 & np.isfinite(var_true) & (var_true > 0)
    E_wP = X5_weight_efficiency(
        np.maximum((s6["adu"] - Dhat) ** 2 / np.maximum(sig_phys ** 2, 1e-30), 0.0)[mm6],
        var_true[mm6])
    E_wI = X5_weight_efficiency(1.0 / np.maximum(sig_phys ** 2, 1e-30)[mm6], var_true[mm6])
    detail["G12"] = {"snr_P_dynrange_p999_over_p50": dyn, "E_weight_caliberP": E_wP,
                     "E_weight_inverse_variance": E_wI}
    X.gate(gates, "G12a_caliberP_weight_efficiency_loss_detected",
           bool(np.isfinite(E_wP) and E_wP >= 10.0 * max(E_wI, 1e-9)),
           "把逐像素显著性平方（w = ((I-D)/sigma)^2）当作权重：必须显著劣于逆方差权重；"
           "实测 E_wP=%.5f vs E_ivar=%.5f（比 %.1f）"
           % (E_wP, E_wI, E_wP / max(E_wI, 1e-12)), detail["G12"],
           expect="E_wP >= 10*E_ivar（口径错配必须判红）")
    X.gate(gates, "G12b_caliberP_field_is_not_smooth", bool(dyn >= 3.0),
           "逐像素显著性场的空间动态范围 p99.9/p50（星点处爆表 ⇒ 场不平滑）；实测 %.1f" % dyn,
           dyn, expect=">= 3（该口径下场不平滑）")

    # ---------------- G14 phys_auto 与两个候选的对照（为 G13c 提供数据） ----------------
    auto_rows = []
    for nm, kw in (("A0_flat", dict(sky_e_per_s=0.5)),
                   ("A4_realistic", dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.4, sky_theta_deg=25.0,
                                        blobs=[{"cy":600, "cx":400, "sigma_px":300, "amp_e_per_s":1.2}],
                                        filament={"y1":150, "x1":60, "y2":860, "x2":940,
                                                  "width_px":3.0, "amp_e":8e3},
                                        cr_rate_per_frame=400.0, **stars)),
                   ("A6_strong_gradient", dict(sky_e_per_s=0.6, sky_grad_e_per_s=2.4,
                                               sky_theta_deg=15.0, **stars)),
                   ("A7_read_noise_dominated", dict(sky_e_per_s=0.05, read_noise_e=12.0))):
        s = _scene(**kw)
        b = X.build_fields(s["adu"], gain=s["meta"]["gain"], delta=DELTA, edge_margin=EDGE_MARGIN)
        e = X.evaluate_fields(b["fields"], s["truth"])
        auto_rows.append({"scene": nm, "E_auto": _E(e, "phys_auto"), "E_frame": _E(e, "frame_scalar"),
                          "E_free": _E(e, "phys_free"), "E_interp": _E(e, "interp_spline"),
                          "choose_free": b["diag"]["slope_cv"].get("choose_free"),
                          "lever_var": b["diag"]["lever_var"]})
    detail["G14_auto_vs_candidates"] = auto_rows

    # ---------------- G13 交叉验证选斜率的正确性（两端各一个场景） ----------------
    s_flat = _scene(sky_e_per_s=0.5)                       # 背景恒定：亮度项无增益
    s_grad = _scene(sky_e_per_s=0.6, sky_grad_e_per_s=2.4, sky_theta_deg=15.0, **stars)
    bf = X.build_fields(s_flat["adu"], gain=s_flat["meta"]["gain"], delta=DELTA,
                        edge_margin=EDGE_MARGIN)
    bg = X.build_fields(s_grad["adu"], gain=s_grad["meta"]["gain"], delta=DELTA,
                        edge_margin=EDGE_MARGIN)
    cv_f = bf["diag"]["slope_cv"]
    cv_g = bg["diag"]["slope_cv"]
    detail["G13"] = {"flat": cv_f, "gradient": cv_g,
                     "lever_flat": bf["diag"]["lever_var"], "lever_grad": bg["diag"]["lever_var"],
                     "gain_dev_flat": abs(bf["fits"]["free"].get("gain_hat", float("nan")) / 1.3 - 1.0),
                     "gain_dev_grad": abs(bg["fits"]["free"].get("gain_hat", float("nan")) / 1.3 - 1.0)}
    e_flat = X.evaluate_fields(bf["fields"], s_flat["truth"])
    X.gate(gates, "G13a_flat_background_auto_is_harmless_and_constant",
           bool(e_flat["phys_auto"]["vs_T1_var_bg"]["eff_loss"] <= 1e-4
                and bf["fields"]["phys_auto"].std() / max(
                    float(np.median(bf["fields"]["phys_auto"])), 1e-30) <= 1e-2),
           "背景恒定时斜率不可辨识，但驱动量恒定 ⇒ 模型场与斜率无关，auto 必须仍退化为常数且 E 触底；"
           "实测 E_auto=%.3g，场相对标准差 %.3g；CV 误差 free=%.6f vs zero=%.6f（几乎并列）"
           % (e_flat["phys_auto"]["vs_T1_var_bg"]["eff_loss"],
              bf["fields"]["phys_auto"].std() / max(float(np.median(bf["fields"]["phys_auto"])), 1e-30),
              cv_f.get("cv_err_free", float("nan")), cv_f.get("cv_err_zero", float("nan"))),
           {"cv": cv_f, "E_auto": e_flat["phys_auto"]["vs_T1_var_bg"]["eff_loss"]},
           expect="E_auto <= 1e-4 且场近似常数")
    X.gate(gates, "G13b_cv_accepts_slope_on_strong_gradient",
           bool(cv_g.get("ok") and cv_g["choose_free"]),
           "强梯度（lever_var=%.3f）时交叉验证必须接受自由斜率" % bg["diag"]["lever_var"],
           cv_g, expect="choose_free = True")
    X.gate(gates, "G13c_auto_never_loses_to_either_candidate",
           bool(all(x["E_auto"] <= max(1.1 * min(x["E_frame"], x["E_free"]), 1e-10)
                    for x in detail.get("G14_auto_vs_candidates", [])
                    if min(x["E_frame"], x["E_free"]) > 1e-6)),
           "phys_auto（CV 选斜率）在**较优候选本身不在数值地板**（min(E_frame,E_free) > 1e-6）的场景上，"
           "不得劣于 frame 与 phys_free 中的较优者超过 10%"
           "（相对判据；地板场景由 G13d 的绝对容差门覆盖）",
           detail.get("G14_auto_vs_candidates", []), expect="E_auto <= 1.1*min(E_frame, E_free)")

    # ---------------- G15 正齐次性（与冻结的算子要求 ② 对接） ----------------
    sh = _scene(sky_e_per_s=0.5, sky_grad_e_per_s=0.6, sky_theta_deg=20.0, **stars)
    bh = X.build_fields(sh["adu"], gain=sh["meta"]["gain"], delta=DELTA, edge_margin=EDGE_MARGIN)
    alpha = 1.7
    Dc_h = bh["ctrl"]["D_sep"]
    v_h = bh["ctrl"]["s_r1"]
    vh = bh["ctrl"]["valid"]
    fld_ref = bh["fields"]["phys_free"]
    fit_f = X.fit_nlf(Dc_h, v_h * alpha, valid=vh)
    fit_x = X.fit_nlf(Dc_h, v_h * alpha, valid=vh, fix_c=1.0 / sh["meta"]["gain"])
    fld_f = X.recon_physics(fit_f, bh["ctrl"]["B_sep"])
    fld_x = X.recon_physics(fit_x, bh["ctrl"]["B_sep"])
    hom_f = float(np.nanmax(np.abs(fld_f / (alpha * fld_ref) - 1.0)))
    hom_x = float(np.nanmax(np.abs(fld_x / (alpha * fld_ref) - 1.0)))
    detail["G15_homogeneity"] = {"alpha": alpha, "max_dev_free_slope": hom_f,
                                 "max_dev_fixed_gain": hom_x}
    abs_rows = detail.get("G14_auto_vs_candidates", [])
    worst_abs = max((x["E_auto"] - min(x["E_frame"], x["E_free"]) for x in abs_rows),
                    default=float("nan"))
    worst_rel = max((x["E_auto"] / max(min(x["E_frame"], x["E_free"]), 1e-12) for x in abs_rows),
                    default=float("nan"))
    detail["G13d"] = {"worst_abs_excess": worst_abs, "worst_rel_ratio": worst_rel,
                     "rows": abs_rows}
    # **绝对容差门**：E 是「逆方差叠加的方差超额」。E <= 1e-4 表示方差超额 <= 0.01%，
    # 低于任何可测的工程意义。在帧级标量已是最优、且其 E 本身在 1e-15 量级的场景上，
    # 「相对劣化倍数」可以任意大（实测 A0：E_auto=4.1e-8 vs E_frame=2.2e-16，相对 1.9e8 倍），
    # 因此判据用**绝对**超额，并把相对倍数作为诊断量一并登记（不隐藏）。
    X.gate(gates, "G13d_auto_absolute_excess_is_bounded",
           bool(np.isfinite(worst_abs) and worst_abs <= 1e-4),
           "phys_auto 相对两个候选的**绝对** E 超额必须有界（<= 1e-4 = 方差超额 0.01%%）；"
           "实测最差绝对超额 %.3g，对应最差相对倍数 %.3g 倍（下限 1e-12；相对倍数可任意大，"
           "因为最优候选的 E 本身在 1e-15 量级时无工程意义）"
           % (worst_abs, worst_rel), detail["G13d"],
           expect="max(E_auto - min(E_frame, E_free)) <= 1e-4")

    X.gate(gates, "G15a_free_slope_model_is_positive_homogeneous", bool(hom_f <= 1e-9),
           "自由斜率模型对控制值整体缩放严格正齐次 R[a*v]=a*R[v]（冻结算子要求 ②）；"
           "实测最大相对偏差 %.3g" % hom_f, detail["G15_homogeneity"], expect="<= 1e-9")
    X.gate(gates, "G15b_fixed_gain_model_breaks_homogeneity", bool(hom_x >= 0.05),
           "用帧级增益固定斜率时模型**不正齐次**（斜率项不随控制值缩放）；实测最大相对偏差 %.3f"
           % hom_x, detail["G15_homogeneity"], expect=">= 0.05（必须识别出该差异）")

    out = {"meta": {"unit": "EXP-06-SNR-PHYS", "arm": "E 门与故障注入",
                    "seed_base": SEED, "shape": list(SHAPE), "delta_px": DELTA,
                    "elapsed_s": time.time() - t0},
           "gates": gates, "detail": detail, "all_pass": X.all_pass(gates)}
    X.save_json(a.out, out)
    for g in gates:
        print("%-4s %-52s %s" % (g["verdict"], g["gate"], g["detail"][:110]))
    print("ALL_PASS =", out["all_pass"], "elapsed %.1fs" % (time.time() - t0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
