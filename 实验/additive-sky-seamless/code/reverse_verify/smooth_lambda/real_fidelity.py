#!/usr/bin/env python3
"""真实数据亮区保真（无真值口径）：产品面亮度随 λs 的漂移 + 分区天光去除率。

- bright = 未校正 stack 前 10% 的 control（M42 核心区），peak = 前 1%，faint = 后 50%
- drift_X(λs) = median_{k in X} (M_est_λs(k) - M_est_0(k)) / M_est_0(k)   产品面亮度相对漂移
- removal_X(λs) = 1 - median_{k in X} std_f(z_fk)/std_f(y_fk)              分区天光去除率
"""
import json, os, sys, importlib.util
import numpy as np
_HERE = os.path.dirname(os.path.abspath(__file__))   # .../实验/<unit>/code/reverse_verify/smooth_lambda
ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))   # 仓库根（从脚本自身位置推导）
D = os.path.join(ROOT, "run/reverse_verify/smooth_lambda")
spec = importlib.util.spec_from_file_location("an", os.path.join(_HERE, "analyze.py"))
an = importlib.util.module_from_spec(spec); spec.loader.exec_module(an)

def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "real49"
    U = an.read_upmb(os.path.join(D, name + ".upmb"))
    summ = [json.loads(l) for l in open(os.path.join(D, "sw_%s.summary.jsonl" % name)) if '"lambda"' in l]
    summ = [e for e in summ if "bin" in e]; summ.sort(key=lambda e: e["lambda"])
    F, K = U["n_frames"], U["n_nodes"]
    o_f, o_k = U["o_f"], U["o_k"]
    val = U["obs"]["value"]; unc = U["obs"]["unc"]
    cover = np.zeros((F, K), bool); cover[o_f, o_k] = True
    multi = cover.sum(0) >= 2
    s = np.zeros(K); c = np.zeros(K); np.add.at(s, o_k, val); np.add.at(c, o_k, 1.0)
    raw = np.where(c > 0, s/np.maximum(c,1), np.nan)
    q = np.nanpercentile(raw[multi], [50, 90, 99])
    bright = multi & (raw >= q[1]); peak = multi & (raw >= q[2]); faint = multi & (raw <= q[0])
    def sc(M):
        Mo = M[o_f, o_k] if M.ndim == 2 else M
        s = np.zeros(K); c = np.zeros(K); np.add.at(s, o_k, Mo); np.add.at(c, o_k, 1.0)
        m = np.where(c>0, s/np.maximum(c,1), 0.0)
        v = np.zeros(K); np.add.at(v, o_k, (Mo-m[o_k])**2)
        return np.sqrt(np.where(c>1, v/np.maximum(c-1,1), np.nan))
    Sb = sc(val)
    rows = []; ref = None
    for e in summ:
        _, C, Z, W = an.read_result(e["bin"])
        s2 = np.zeros(K); np.add.at(s2, o_k, Z[o_f, o_k]); np.add.at(c, o_k, 0.0)
        cn = np.zeros(K); np.add.at(cn, o_k, 1.0)
        Mest = np.where(cn>0, s2/np.maximum(cn,1), np.nan)
        if ref is None: ref = Mest
        Sa = sc(Z)
        row = {"lambda": e["lambda"], "converged": e["converged"], "iterations": e["iterations"],
               "c_grad_rms": e["c_grad_rms"], "c_rms": e["c_rms"]}
        for lab, m in (("bright", bright), ("peak", peak), ("faint", faint)):
            row["drift_" + lab] = float(np.nanmedian((Mest[m]-ref[m])/np.maximum(np.abs(ref[m]),1e-300)))
            row["removal_" + lab] = float(1.0 - np.nanmedian(Sa[m])/np.nanmedian(Sb[m]))
            row["residnoise_" + lab] = float(np.nanmedian(Sa[m])/np.nanmedian(unc))
            row["cabs_" + lab] = float(np.nanmedian(np.abs(C[:, m])))
        rows.append(row)
    json.dump(rows, open(os.path.join(D, "real_fidelity.json"), "w"), indent=1)
    hdr = ["lambda","converged","iterations","c_grad_rms","drift_bright","drift_peak","drift_faint",
           "removal_bright","removal_peak","removal_faint","residnoise_bright","residnoise_faint","cabs_bright","cabs_faint"]
    with open(os.path.join(D, "real_fidelity.csv"), "w") as f:
        f.write(",".join(hdr) + "\n")
        for r in rows:
            f.write(",".join(("%.10g" % r[k]) if isinstance(r.get(k),(int,float)) else str(r.get(k,"")) for k in hdr) + "\n")
    for r in rows:
        print("lam=%-8g conv=%d it=%-3d cgrad=%.3e | drift B/P/F = %+.4f%% %+.4f%% %+.4f%% | removal B/P/F = %.4f %.4f %.4f"
              % (r["lambda"], r["converged"], r["iterations"], r["c_grad_rms"],
                 r["drift_bright"]*100, r["drift_peak"]*100, r["drift_faint"]*100,
                 r["removal_bright"], r["removal_peak"], r["removal_faint"]))
    print("n_bright", int(bright.sum()), "n_peak", int(peak.sum()), "n_faint", int(faint.sum()))
if __name__ == "__main__":
    main()
