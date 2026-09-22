"""EXP-06 消融：驱动量 D(p) 的构造方式如何影响物理模型（报告 3.2/3.3 的产出脚本）。

回答两个问题：
  1. mesh 中值滤波（SExtractor BACK_FILTTHRESH 同构物）+ 双线性展开 vs 样条展开（无中值滤波）
     —— 哪种构造更接近真值弥散分量 D_true，以及它如何传播到斜率（=1/g）估计；
  2. 控制网格最外圈是否必须剔除（边界 mesh 的背景估计是否可靠）。
两个场景各跑一遍（平滑曲率主导 / 真实结构），**结论按场景分别陈述**。
固定 seed；只读依赖 exp06_common。
"""
import argparse, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exp06_common as X

SCENES = {
    # 平滑曲率主导：弥散分量在 cell 尺度上光滑、几乎无小尺度结构
    "smooth_curvature": dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.4, sky_theta_deg=25.0,
                             blobs=[{"cy": 512, "cx": 512, "sigma_px": 300,
                                     "amp_e_per_s": 1.2}],
                             n_stars=0, seed=X.SEED),
    # 真实结构：星云 + 80 星 + 线状结构 + 宇宙线 + PRNU + 低阶平场
    "realistic": dict(sky_e_per_s=0.5, sky_grad_e_per_s=0.4, sky_theta_deg=25.0,
                      blobs=[{"cy": 600, "cx": 400, "sigma_px": 300, "amp_e_per_s": 1.2}],
                      filament={"y1": 150, "x1": 60, "y2": 860, "x2": 940,
                                "width_px": 3.0, "amp_e": 8e3},
                      cr_rate_per_frame=400.0, n_stars=80, seed=X.SEED),
}


def _stat(e: np.ndarray) -> dict:
    f = np.isfinite(e)
    v = np.abs(e[f])
    return {"abs_err_median": float(np.median(v)), "abs_err_p95": float(np.percentile(v, 95)),
            "signed_median": float(np.median(e[f])),
            "rms": float(np.sqrt(np.mean(e[f] ** 2)))}


def _inner_mask(shape) -> np.ndarray:
    v = np.ones(shape, dtype=bool)
    v[0, :] = v[-1, :] = False
    v[:, 0] = v[:, -1] = False
    return v


def run_scene(name: str, kw: dict, delta: int) -> dict:
    sc = X.analytic_scene(**kw)
    adu = np.asarray(sc["adu"], dtype=np.float64)
    g = float(sc["meta"]["gain"])
    bias = float(sc["meta"]["bias_adu"])
    D_true = bias + np.asarray(sc["truth"]["diffuse_e"], dtype=np.float64) * \
        np.asarray(sc["truth"]["flat"], dtype=np.float64) / g
    s_r1 = X.sigma_ctrl_resid(adu, delta)

    drivers = {}
    rows = []
    for mode in ("bilin_med3", "spline"):
        D_hat, dmeta = X.diffuse_component(adu, box=delta, mode=mode)
        drivers[mode] = D_hat
        rows.append(dict(_stat(D_hat - D_true), driver=mode, meta=dmeta))
    drivers["oracle_D_true"] = D_true
    rows.append(dict(_stat(D_true - D_true), driver="oracle_D_true", meta={"mode": "oracle"}))

    valid = _inner_mask(X.sample_at_ctrl(D_true, delta).shape)
    for r in rows:
        Dc = X.sample_at_ctrl(drivers[r["driver"]], delta)
        fit = X.fit_nlf(Dc, s_r1, valid=valid)
        ok = bool(fit.get("ok")) and np.isfinite(fit.get("gain_hat", np.nan))
        r["gain_hat_free"] = float(fit["gain_hat"]) if ok else None
        r["gain_dev_free"] = (abs(fit["gain_hat"] / g - 1.0)) if ok else None
        r["c_se_over_c"] = (float(fit["c_se"] / fit["c"])) if (ok and fit.get("c")) else None
        r["lever_var"] = float(X.lever_arm_diag(Dc, fit.get("a", float("nan")),
                                                valid=valid)["lever_var"])

    Dc_sep = X.sample_at_ctrl(drivers["spline"], delta)
    f_all = X.fit_nlf(Dc_sep, s_r1, valid=np.ones(Dc_sep.shape, dtype=bool))
    f_in = X.fit_nlf(Dc_sep, s_r1, valid=valid)
    border = ~valid
    s_border = float(np.median(s_r1[border]))
    s_inner = float(np.median(s_r1[valid]))
    boundary = {"sigma_ctrl_median_border": s_border, "sigma_ctrl_median_inner": s_inner,
                "border_ratio": s_border / s_inner,
                "gain_hat_all_cells": f_all.get("gain_hat"),
                "gain_hat_inner_only": f_in.get("gain_hat"),
                "gain_dev_all_cells": (abs(f_all["gain_hat"] / g - 1.0)
                                       if f_all.get("ok") and np.isfinite(f_all.get("gain_hat", np.nan)) else None),
                "gain_dev_inner_only": (abs(f_in["gain_hat"] / g - 1.0)
                                        if f_in.get("ok") and np.isfinite(f_in.get("gain_hat", np.nan)) else None)}

    gates = []
    X.gate(gates, "AB1_%s_spline_closer_to_truth" % name,
           bool(rows[1]["abs_err_median"] < rows[0]["abs_err_median"]),
           "[%s] 样条展开（无 mesh 中值滤波）比中值+双线性更接近真值弥散分量；实测 %.3f vs %.3f ADU"
           % (name, rows[1]["abs_err_median"], rows[0]["abs_err_median"]),
           {"spline": rows[1]["abs_err_median"], "bilin_med3": rows[0]["abs_err_median"]},
           expect="spline < bilin_med3")
    X.gate(gates, "AB2_%s_med3_degrades_slope" % name,
           bool(rows[0].get("gain_dev_free") is not None and rows[1].get("gain_dev_free") is not None
                and rows[0]["gain_dev_free"] >= rows[1]["gain_dev_free"]),
           "[%s] 中值+双线性驱动量下的斜率偏差不小于样条驱动量；实测 %.1f%% vs %.1f%%"
           % (name, 100.0 * (rows[0].get("gain_dev_free") or float("nan")),
              100.0 * (rows[1].get("gain_dev_free") or float("nan"))),
           {"bilin_med3": rows[0].get("gain_dev_free"), "spline": rows[1].get("gain_dev_free"),
            "oracle": rows[2].get("gain_dev_free")}, expect="med3 >= spline")
    dev = {r["driver"]: r.get("gain_dev_free") for r in rows}
    X.gate(gates, "AB3_%s_slope_error_is_estimator_dominated" % name,
           bool(dev["oracle_D_true"] is not None and dev["spline"] is not None
                and dev["oracle_D_true"] >= 0.5 * dev["spline"]),
           "[%s] 用**解析真值**作驱动量（下界）时斜率偏差仍达样条驱动量的一半以上 "
           "⇒ 斜率误差的主导项是控制点 sigma 估计器本身，不是驱动量构造；实测下界 %.2f%% vs 样条 %.2f%%"
           % (name, 100.0 * (dev["oracle_D_true"] or float("nan")),
              100.0 * (dev["spline"] or float("nan"))), dev,
           expect="oracle_dev >= 0.5 * spline_dev")
    X.gate(gates, "AB4_%s_spline_driver_reduces_slope_error" % name,
           bool(dev["spline"] is not None and dev["bilin_med3"] is not None
                and dev["spline"] <= 0.9 * dev["bilin_med3"]),
           "[%s] 样条展开相对中值+双线性把斜率偏差降低 >= 10%%（相对）；实测 %.2f%% -> %.2f%%"
           % (name, 100.0 * (dev["bilin_med3"] or float("nan")), 100.0 * (dev["spline"] or float("nan"))),
           dev, expect="spline_dev <= 0.9 * med3_dev")
    X.gate(gates, "AB5_%s_dropping_border_cells_reduces_slope_error" % name,
           bool(boundary["gain_dev_inner_only"] is not None
                and boundary["gain_dev_all_cells"] is not None
                and boundary["gain_dev_inner_only"] < boundary["gain_dev_all_cells"]),
           "[%s] 剔除控制网格最外圈必须降低斜率偏差；实测全网格 %.2f%% -> 仅内圈 %.2f%%"
           % (name, 100.0 * boundary["gain_dev_all_cells"], 100.0 * boundary["gain_dev_inner_only"]),
           boundary, expect="inner_only < all_cells")
    X.gate(gates, "AB6_%s_border_sigma_is_biased_low" % name,
           bool(boundary["border_ratio"] < 1.0),
           "[%s] 控制网格最外圈的 sigma_hat 系统性**偏低**（边界 mesh 的背景电平被低估）；"
           "实测 外圈/内圈 = %.3f" % (name, boundary["border_ratio"]), boundary,
           expect="border_ratio < 1")
    return {"scene": name, "gain_true": g, "drivers": rows, "boundary": boundary, "gates": gates}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(X.RESULTS / "exp06_e6_ablation.json"))
    ap.add_argument("--delta", type=int, default=X.DELTA_PX)
    args = ap.parse_args()

    results = [run_scene(n, kw, args.delta) for n, kw in SCENES.items()]
    gates = [g for r in results for g in r["gates"]]
    out = {"meta": {"unit": "EXP-06-SNR-PHYS", "arm": "F 消融（驱动量构造）",
                    "seed": int(X.SEED), "delta": int(args.delta),
                    "note": "报告 3.2/3.3 的产出脚本；固定 seed，可一键复跑"},
           "scenes": results, "gates": gates, "all_pass": X.all_pass(gates)}
    X.save_json(args.out, out)
    for r in results:
        print("=== %s (g_true=%.2f)" % (r["scene"], r["gain_true"]))
        for d in r["drivers"]:
            print("  %-14s |D-D_true| med=%7.3f p95=%8.3f ADU | gain_dev=%s lever=%.3f" % (
                d["driver"], d["abs_err_median"], d["abs_err_p95"],
                ("%.1f%%" % (100 * d["gain_dev_free"])) if d.get("gain_dev_free") is not None else "n/a",
                d["lever_var"]))
        b = r["boundary"]
        print("  boundary: sigma border/inner=%.3f (%+.1f%%), gain_dev all=%.1f%% inner=%.1f%%" % (
            b["border_ratio"], 100 * (b["border_ratio"] - 1.0),
            100 * (b["gain_dev_all_cells"] or float("nan")),
            100 * (b["gain_dev_inner_only"] or float("nan"))))
    for g in gates:
        print(" ", g["verdict"], g["gate"], "|", g["detail"][:130])
    print("ALL_PASS =", out["all_pass"])
    return 0 if out["all_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())