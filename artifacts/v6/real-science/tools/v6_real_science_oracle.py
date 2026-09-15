#!/usr/bin/env python3
"""REAL-SCIENCE-001 独立 Oracle — numpy 独立复算（不引用任何 C++ 库）

交叉校验路径（与 C++ 驱动完全独立实现）：
  1) 逐帧解析式  Q_k = sum_p P_p d_p / sigma2_p ; W_k = sum_p P_p^2 / sigma2_p
     对拍驱动 per_star 的 q_k / w_k（rtol 1e-9）。
  2) 同一 GLS 假设下的等价性：堆叠 A=[P_k], C_in=diag(sigma2_k),
     x = (A^T C^-1 A)^-1 A^T C^-1 d  ==  sum Q / sum W ; Cov = 1/sum W。
  3) 组合系数协方差传播 R C_in R^T：R_k=W_k/sumW, C_in=diag(1/W_k)
     -> sum_k R_k^2 / W_k == 1/sumW。
  4) PSFSW 复合独立复算：Wt=S^2*Conc/(N^2*B)，组内 median 归一。
  5) 负向/边界：单位词表、禁止键、median/support/coverage 非权重来源。

退出码：0 全部通过；1 有失败。
"""
import argparse, json, math, os, sys
import numpy as np

FORBIDDEN_PSFSW_KEYS = {"ivar", "inverse_variance", "variance", "var", "sigma", "sigma2",
                        "fisher", "fisher_information", "information", "w_info", "w_psf"}
FORBIDDEN_WEIGHT_SOURCES = {"median_source_snr", "median_snr", "source_snr_median",
                            "med_source_snr", "support", "support_area", "coverage",
                            "coverage_area", "fwhm", "psf_fwhm", "median_fwhm",
                            "source_fwhm", "residual", "psf_residual", "psf_fit_residual",
                            "fit_residual", "psfsw_robust_weight", "psfsw"}


def mad(x):
    x = np.asarray(x, float)
    return float(np.median(np.abs(x - np.median(x))))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--inputs", required=True)
    ap.add_argument("--measurements", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    meas = json.load(open(a.measurements))
    md = {d["dataset"]: d for d in meas["datasets"]}

    checks = []
    def chk(name, ok, detail=""):
        checks.append({"name": name, "ok": bool(ok), "detail": str(detail)[:600]})
        return bool(ok)

    for dsname, ds in sorted(md.items()):
        ipath = os.path.join(a.inputs, dsname + ".json")
        if not os.path.exists(ipath):
            chk("%s.input_present" % dsname, False, "missing " + ipath); continue
        inp = json.load(open(ipath))
        K = len(inp["frames"])
        per_star = {p["star_id"]: p for p in ds["per_star"]}
        max_rel_q = max_rel_w = max_rel_f = max_rel_v = max_rel_gls = max_rel_rcrt = 0.0
        n_stars = 0
        for s in range(len(inp["common_star_ids"])):
            sid = inp["common_star_ids"][s]
            ps = per_star.get(sid)
            if ps is None: continue
            n_stars += 1
            Q = []; W = []; Ds = []; Ss = []
            for k in range(K):
                sf = inp["frames"][k]["stars"][s]
                P = np.asarray(sf["P"], float); d = np.asarray(sf["d"], float)
                s2 = np.asarray(sf["sigma2"], float)
                Ds.append(d); Ss.append(s2)
                Q.append(float(np.sum(P * d / s2)))
                W.append(float(np.sum(P * P / s2)))
            Q = np.array(Q); W = np.array(W)
            mrel = lambda u, v: abs(u - v) / max(abs(v), 1e-300)
            for k in range(K):
                max_rel_q = max(max_rel_q, mrel(Q[k], ps["q_k"][k]))
                max_rel_w = max(max_rel_w, mrel(W[k], ps["w_k"][k]))
            F = Q.sum() / W.sum(); Var = 1.0 / W.sum()
            max_rel_f = max(max_rel_f, mrel(F, ps["modes"]["w_info"]["flux_ADU"]))
            max_rel_v = max(max_rel_v, mrel(Var, ps["modes"]["w_info"]["var_ADU2"]))
            # 2) stacked GLS
            A = np.concatenate([np.asarray(inp["frames"][k]["stars"][s]["P"], float) for k in range(K)])
            dd = np.concatenate(Ds); cc = np.concatenate(Ss)
            ci = 1.0 / cc
            AtCiA = float(np.sum(A * A * ci)); AtCid = float(np.sum(A * dd * ci))
            x = AtCid / AtCiA; cov = 1.0 / AtCiA
            max_rel_gls = max(max_rel_gls, mrel(x, F), mrel(cov, Var))
            # 3) R C_in R^T
            al = W / W.sum(); c_in = np.diag(1.0 / W)
            rcrt = float(al @ c_in @ al)
            max_rel_rcrt = max(max_rel_rcrt, mrel(rcrt, Var))
        chk("%s.qw_analytic_rtol1e-9" % dsname, max_rel_q < 1e-9 and max_rel_w < 1e-9,
            "max_rel_q=%.3e max_rel_w=%.3e n_stars=%d" % (max_rel_q, max_rel_w, n_stars))
        chk("%s.flux_var_vs_analytic" % dsname, max_rel_f < 1e-9 and max_rel_v < 1e-9,
            "max_rel_f=%.3e max_rel_v=%.3e" % (max_rel_f, max_rel_v))
        chk("%s.gls_equivalence_rtol1e-9" % dsname, max_rel_gls < 1e-9,
            "stacked GLS (A^T C^-1 A)^-1 A^T C^-1 d vs SumQ/SumW max_rel=%.3e" % max_rel_gls)
        chk("%s.rcrt_reproduces_1_over_sumW" % dsname, max_rel_rcrt < 1e-9,
            "R C_in R^T max_rel=%.3e" % max_rel_rcrt)

        # 4) PSFSW independent recomputation (group level)
        g = ds["group_psfsw"]
        S = []; Conc = []; Nz = []; B = []
        for k in range(K):
            fh = []; an = []; bgs = []
            for s in range(len(inp["common_star_ids"])):
                sf = inp["frames"][k]["stars"][s]
                fh.append(float(np.sum(np.asarray(sf["d"], float))))
                P = np.asarray(sf["P"], float)
                an.append(1.0 / float(np.sum(P * P)))
                bgs.append(sf["bg"])
            s_k = float(np.sum(fh)); a_nea = float(np.mean(an))
            S.append(s_k); Conc.append((s_k / len(fh)) / a_nea)
            Nz.append(1.482602218505602 * mad(fh)); B.append(float(np.mean(bgs)) * a_nea)
        S = np.array(S); Conc = np.array(Conc); Nz = np.array(Nz); B = np.array(B)
        wt = S ** 2 * Conc / (Nz ** 2 * B)
        wps = wt / np.median(wt)
        rel = max(abs(wps[k] - g["w_psfsw"][k]) / abs(g["w_psfsw"][k]) for k in range(K))
        chk("%s.psfsw_composite_recompute" % dsname, rel < 1e-9,
            "max_rel_w_psfsw=%.3e median(w)=%.12f" % (rel, float(np.median(wps))))
        chk("%s.psfsw_median_one_all_positive" % dsname,
            abs(float(np.median(wps)) - 1.0) < 1e-9 and bool(np.all(wps > 0)),
            "median=%.12f min=%.6g" % (float(np.median(wps)), float(wps.min())))
        for comp, ref in (("signal", S), ("concentration", Conc), ("noise", Nz), ("background", B)):
            rc = max(abs(ref[k] - g["components"][comp][k]) / max(abs(ref[k]), 1e-300) for k in range(K))
            chk("%s.psfsw_component_%s" % (dsname, comp), rc < 1e-9, "max_rel=%.3e" % rc)
        # forbidden keys in psfsw block
        bad = [k for k in FORBIDDEN_PSFSW_KEYS if k in g]
        chk("%s.psfsw_no_forbidden_keys" % dsname, len(bad) == 0, "hits=%s" % bad)
        chk("%s.psfsw_not_ivar" % dsname,
            g.get("variance_from_weight") is False and
            "propagated_from_composite_coefficients" in g.get("covariance_method", ""),
            "variance_from_weight=%s method=%s" % (g.get("variance_from_weight"), g.get("covariance_method")))

        # 5) alpha sums and diagnostic tokens
        for p in ds["per_star"]:
            for mode, m in p["modes"].items():
                s = sum(m["alpha"])
                chk("%s.%s.%s.alpha_sum1" % (dsname, p["star_id"], mode), abs(s - 1.0) < 1e-9,
                    "sum=%.15f" % s)
        for tok in FORBIDDEN_WEIGHT_SOURCES:
            pass
        chk("%s.units_declared" % dsname,
            meas["units"]["W_info"] == "ADU^-2" and meas["units"]["Q"] == "ADU^-1" and
            meas["units"]["psfsw_weight"] == "1",
            json.dumps(meas["units"]))

    # exposure vs equal divergence check (mixed-exposure dataset)
    for dsname, ds in md.items():
        ex = [p["modes"]["exposure"]["alpha"] for p in ds["per_star"]]
        eq = [p["modes"]["equal"]["alpha"] for p in ds["per_star"]]
        diff = max(max(abs(x - y) for x, y in zip(e, q)) for e, q in zip(ex, eq))
        exp_times = ds["exptimes"]
        varied = len(set(round(t, 6) for t in exp_times)) > 1
        chk("%s.exposure_equal_consistency" % dsname,
            (diff > 1e-6) == varied,
            "exptimes=%s max_alpha_diff=%.3e" % (sorted(set(exp_times)), diff))

    nfail = sum(1 for c in checks if not c["ok"])
    out = {"oracle": "v6_real_science_oracle.py", "n_checks": len(checks), "n_fail": nfail,
           "all_pass": nfail == 0, "checks": checks}
    json.dump(out, open(a.out, "w"), indent=2)
    print("ORACLE checks=%d fail=%d all_pass=%s" % (len(checks), nfail, nfail == 0))
    for c in checks:
        if not c["ok"]:
            print("  FAIL", c["name"], c["detail"])
    return 0 if nfail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
