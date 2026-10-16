#!/usr/bin/env python3
"""analyze_s02.py — S02 修前/修后成对分析与判据适用域。

输入（由 build_and_run_s02.sh 产出）：
  results/results_before.json            修前（git HEAD e31b2be66 的 spherical_overlap.cpp）
  results/results_after.json             修后（工作树 P3-NORM-01 快照）
  results/results_negative_legacy_cross.json  负例（ACSD_DRZ_P3_FAULT=legacy_cross_normal）

被估量：epsilon_j = Σ_p a_jp/A_drop,j − 1（无量纲；分母 = 生产 g.drop_area，与正本同口径）。
本脚本输出：
  results/s02_summary.json   机器可读汇总（含负例/回归判红、分组分位数、最坏格）
  stdout                     人读 markdown 表
"""
import json, math, os, statistics

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, "..", "results")

MACH = 2.220446049250313e-16
ARCSEC = math.pi / 180.0 / 3600.0


def load(name):
    with open(os.path.join(RES, name)) as f:
        return json.load(f)["experiments"]


def by_id(rows, i):
    return [r for r in rows if r.get("id") == i]


def cell_key(r):
    return (r["pos_class"], r["ra"], r["dec"], r["nside"], r["pixfrac"], r["scale_arcsec"])


def quant(vals, q):
    if not vals:
        return None
    s = sorted(vals)
    if len(s) == 1:
        return s[0]
    idx = q * (len(s) - 1)
    lo = int(math.floor(idx)); hi = min(lo + 1, len(s) - 1)
    return s[lo] + (s[hi] - s[lo]) * (idx - lo)


def main():
    before = load("results_before.json")
    after = load("results_after.json")
    neg = load("results_negative_legacy_cross.json")

    cb = {cell_key(r): r for r in by_id(before, "S02_matrix_cell")}
    ca = {cell_key(r): r for r in by_id(after, "S02_matrix_cell")}
    cn = {cell_key(r): r for r in by_id(neg, "S02_matrix_cell")}
    assert set(cb) == set(ca) == set(cn), "cell sets differ"

    out = {
        "estimand": "epsilon_j = sum_p a_jp / A_drop,j - 1  (dimensionless)",
        "denominator": "production g.drop_area (build_drop_geometry), sr",
        "n_matrix_cells": len(cb),
        "negative_control": {},
        "large_drop_regression": {},
    }

    # ---- 负例判红：legacy_cross_normal 必须逐位复现"修前" ----
    neg_mismatch = []
    for k in cb:
        if cb[k]["epsilon_j"] != cn[k]["epsilon_j"]:
            neg_mismatch.append({"cell": list(k), "before": cb[k]["epsilon_j"], "fault": cn[k]["epsilon_j"]})
    # 全部实验（含 E12/E13/回归锁）逐字段比较
    def canon(rows):
        return json.dumps(rows, sort_keys=True)
    neg_all_equal = (canon(by_id(neg, "S02_matrix_cell")) == canon(by_id(before, "S02_matrix_cell")) and
                     canon(by_id(neg, "E12_seam_ra_survey")) == canon(by_id(before, "E12_seam_ra_survey")) and
                     canon(by_id(neg, "E13_perdrop_close")) == canon(by_id(before, "E13_perdrop_close")))
    fault_on_vs_after_maxdiff = max(abs(cb[k]["epsilon_j"] - cn[k]["epsilon_j"]) for k in cb)
    out["negative_control"] = {
        "injected_fault": "ACSD_DRZ_P3_FAULT=legacy_cross_normal (restores P1 x P2 cross-product normals)",
        "must_fail": True,
        "negative_reproduces_prepatch_all_experiments": neg_all_equal,
        "negative_eq_after_within": None,
        "max_abs_epsilon_fault_on": max(abs(cn[k]["epsilon_j"]) for k in cn),
        "max_abs_epsilon_fault_off": max(abs(ca[k]["epsilon_j"]) for k in ca),
        "n_cells_fault_on_over_1e-6": sum(1 for k in cn if abs(cn[k]["epsilon_j"]) > 1e-6),
        "n_cells_fault_off_over_1e-6": sum(1 for k in ca if abs(ca[k]["epsilon_j"]) > 1e-6),
        "verdict": "RED (fault-on reproduces the pre-patch defect)" if (neg_all_equal and
                   sum(1 for k in cn if abs(cn[k]["epsilon_j"]) > 1e-6) >
                   sum(1 for k in ca if abs(ca[k]["epsilon_j"]) > 1e-6)) else "UNEXPECTED",
        "mismatch_cells": neg_mismatch[:5],
    }

    # ---- 大 drop（max_angle >= 1e-3 rad）默认叉积路径零回归 ----
    rb = {(r["nside"], r["pixfrac"], r["scale_arcsec"]): r for r in by_id(before, "S02_large_drop_reg")}
    ra = {(r["nside"], r["pixfrac"], r["scale_arcsec"]): r for r in by_id(after, "S02_large_drop_reg")}
    reg_bad, reg_n = [], 0
    for k in rb:
        reg_n += 1
        if json.dumps(rb[k], sort_keys=True) != json.dumps(ra[k], sort_keys=True):
            reg_bad.append(list(k))
    out["large_drop_regression"] = {
        "criterion": "max_angle >= 1e-3 rad -> legacy cross-product path must be bitwise unchanged",
        "n_cases": reg_n,
        "n_bitwise_different": len(reg_bad),
        "different_cases": reg_bad[:10],
        "verdict": "PASS (bitwise identical)" if not reg_bad else "FAIL",
    }

    # ---- 分组：每个 (nside, pixfrac, scale) 上跨位置类的分位数 ----
    groups = {}
    for k in cb:
        _, _, _, ns, pf, sc = k
        groups.setdefault((ns, pf, sc), []).append(k)
    table = []
    for g in sorted(groups, key=lambda x: (x[0], x[1], x[2])):
        ks = groups[g]
        eb = [abs(cb[k]["epsilon_j"]) for k in ks]
        ea = [abs(ca[k]["epsilon_j"]) for k in ks]
        worst = max(ks, key=lambda k: abs(ca[k]["epsilon_j"]))
        table.append({
            "nside": g[0], "pixfrac": g[1], "scale_arcsec": g[2],
            "drop_side_arcsec": g[1] * g[2],
            "theta_drop_rad": g[1] * g[2] * ARCSEC,
            "n_positions": len(ks),
            "before_max_abs": max(eb), "before_p50": quant(eb, .5),
            "before_p90": quant(eb, .9), "before_p99": quant(eb, .99),
            "after_max_abs": max(ea), "after_p50": quant(ea, .5),
            "after_p90": quant(ea, .9), "after_p99": quant(ea, .99),
            "after_max_cell": {"pos_class": worst[0], "ra": ca[worst]["ra"], "dec": ca[worst]["dec"],
                               "epsilon_j": ca[worst]["epsilon_j"],
                               "n_leaves": ca[worst]["n_leaves"],
                               "n_candidates_query": ca[worst]["n_candidates_query"],
                               "n_candidates_fast": ca[worst]["n_candidates_fast"]},
            "before_max_cell": {
                "pos_class": max(ks, key=lambda k: abs(cb[k]["epsilon_j"]))[0],
                "epsilon_j": max((cb[k]["epsilon_j"] for k in ks), key=abs)},
        })
    out["groups"] = table

    # ---- ε_mach/θ 标度律：仅取数值项主导的小 θ 子域 ----
    fit_rows = [t for t in table if t["scale_arcsec"] <= 2.0]
    xs = [math.log(t["theta_drop_rad"]) for t in fit_rows if t["after_max_abs"] > 0]
    ys = [math.log(t["after_max_abs"]) for t in fit_rows if t["after_max_abs"] > 0]
    slope = None
    if len(xs) >= 2:
        n = len(xs)
        mx = sum(xs) / n; my = sum(ys) / n
        num = sum((xs[i]-mx)*(ys[i]-my) for i in range(n))
        den = sum((xs[i]-mx)**2 for i in range(n))
        slope = num / den
    # C in eps = C*eps_mach/theta
    Cs = [t["after_max_abs"] * t["theta_drop_rad"] / MACH for t in fit_rows if t["after_max_abs"] > 0]
    out["scaling_law"] = {
        "hypothesis_after": "epsilon_j ~ C * eps_mach / theta_drop  (numerical term, small drop)",
        "log_log_slope_after_theta_0.04..2arcsec": slope,
        "C_min": min(Cs) if Cs else None, "C_max": max(Cs) if Cs else None,
        "C_median": statistics.median(Cs) if Cs else None,
        "theta_at_1e-6_rad_from_Cmedian": (statistics.median(Cs) * MACH / 1e-6) if Cs else None,
    }

    # ---- 修前/修后：全局最坏格 ----
    wb = max(cb, key=lambda k: abs(cb[k]["epsilon_j"]))
    wa = max(ca, key=lambda k: abs(ca[k]["epsilon_j"]))
    out["worst_cell_before"] = dict(cb[wb], cell=list(wb))
    out["worst_cell_after"] = dict(ca[wa], cell=list(wa))

    # ---- 判定：修前/修后可过 1e-6 的 (nside,pixfrac,scale) 组合 ----
    def gate(d, thr=1e-6):
        bad = [k for k in d if abs(d[k]["epsilon_j"]) > thr]
        return len(d) - len(bad), len(bad), bad
    nb, fb, bad_b = gate(cb)
    na, fa, bad_a = gate(ca)
    out["gate_1e-6"] = {
        "tau_rel": 1e-6,
        "denominator": "A_drop,j (production g.drop_area)",
        "before_n_pass_cells": nb, "before_n_fail_cells": fb,
        "after_n_pass_cells": na, "after_n_fail_cells": fa,
        "after_failing_cells": [dict(ca[k], cell=list(k)) for k in bad_a][:20],
        "before_failing_cells": [dict(cb[k], cell=list(k)) for k in bad_b][:20],
    }
    # 逐 nside 判定（组合维度）
    per_ns = {}
    for ns in sorted({k[3] for k in cb}):
        kb = [k for k in cb if k[3] == ns]
        ka = [k for k in ca if k[3] == ns]
        per_ns[str(ns)] = {
            "before_max_abs": max(abs(cb[k]["epsilon_j"]) for k in kb),
            "after_max_abs": max(abs(ca[k]["epsilon_j"]) for k in ka),
            "before_fail": sum(1 for k in kb if abs(cb[k]["epsilon_j"]) > 1e-6),
            "after_fail": sum(1 for k in ka if abs(ca[k]["epsilon_j"]) > 1e-6),
            "n": len(kb),
        }
    out["per_nside"] = per_ns
    per_pf = {}
    for pf in sorted({k[4] for k in cb}):
        kb = [k for k in cb if k[4] == pf]
        ka = [k for k in ca if k[4] == pf]
        per_pf[str(pf)] = {
            "before_max_abs": max(abs(cb[k]["epsilon_j"]) for k in kb),
            "after_max_abs": max(abs(ca[k]["epsilon_j"]) for k in ka),
            "before_fail": sum(1 for k in kb if abs(cb[k]["epsilon_j"]) > 1e-6),
            "after_fail": sum(1 for k in ka if abs(ca[k]["epsilon_j"]) > 1e-6),
            "n": len(kb),
        }
    out["per_pixfrac"] = per_pf

    with open(os.path.join(RES, "s02_summary.json"), "w") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)

    # ---------------- markdown 输出 ----------------
    print("### 负例（legacy_cross_normal）")
    print(json.dumps(out["negative_control"], ensure_ascii=False, indent=1))
    print("\n### 大 drop 回归锁")
    print(json.dumps(out["large_drop_regression"], ensure_ascii=False, indent=1))
    print("\n### 分组分位数（|ε_j|，跨位置类）")
    print("| nside | pixfrac | scale″ | θ_drop[rad] | n_pos | before max | before p90 | after max | after p90 | after p99 | 最坏格 |")
    print("|---|---|---|---|---|---|---|---|---|---|---|")
    for t in table:
        print(f"| {t['nside']} | {t['pixfrac']} | {t['scale_arcsec']} | {t['theta_drop_rad']:.3e} | {t['n_positions']} | "
              f"{t['before_max_abs']:.3e} | {t['before_p90']:.3e} | {t['after_max_abs']:.3e} | "
              f"{t['after_p90']:.3e} | {t['after_p99']:.3e} | {t['after_max_cell']['pos_class']} |")
    print("\n### 1e-6 门")
    print(json.dumps({k: v for k, v in out["gate_1e-6"].items() if not k.endswith("cells")},
                     ensure_ascii=False, indent=1))
    print("\n### 标度律")
    print(json.dumps(out["scaling_law"], ensure_ascii=False, indent=1))
    print("\n### 最坏格")
    print("before:", json.dumps({k: out["worst_cell_before"][k] for k in
          ("cell", "epsilon_j", "n_leaves", "n_candidates_query", "max_angle_rad")}, ensure_ascii=False))
    print("after :", json.dumps({k: out["worst_cell_after"][k] for k in
          ("cell", "epsilon_j", "n_leaves", "n_candidates_query", "max_angle_rad")}, ensure_ascii=False))
    print("\n### per nside / per pixfrac")
    print(json.dumps(out["per_nside"], ensure_ascii=False, indent=1))
    print(json.dumps(out["per_pixfrac"], ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()
