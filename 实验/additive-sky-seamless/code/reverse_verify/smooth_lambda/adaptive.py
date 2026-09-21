#!/usr/bin/env python3
"""§7 自适应 λs 依据：逐 control 的局部代价随 λs 的最优分布。

局部代价（尺度无关）：
  J_res(k)  = std_f(z_fk) / std_f(y_fk)               天光残留比（越小越好）
  J_dim(k)  = |(M_est(k) - M_target(k)) / M_target(k)| 局部压暗（越小越好）
  J_step(k) = 该 control 参与的覆盖子集突变边的 |ΔM_est| / 局部电平（越小越好）
合成场景用真值；真实数据只用 J_res / J_step。
"""
import json, os, sys, importlib.util
import numpy as np
_HERE = os.path.dirname(os.path.abspath(__file__))   # .../实验/<unit>/code/reverse_verify/smooth_lambda
ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))   # 仓库根（从脚本自身位置推导）
D = os.path.join(ROOT, "run/reverse_verify/smooth_lambda")
spec = importlib.util.spec_from_file_location("an", os.path.join(_HERE, "analyze.py"))
an = importlib.util.module_from_spec(spec); spec.loader.exec_module(an)

def load(name):
    U = an.read_upmb(os.path.join(D, name + ".upmb"))
    summ = [json.loads(l) for l in open(os.path.join(D, "sw_%s.summary.jsonl" % name)) if '"lambda"' in l]
    summ = [e for e in summ if "bin" in e]
    truth = None
    p = os.path.join(D, name + ".truth.npz")
    if os.path.exists(p): truth = dict(np.load(p))
    return U, summ, truth

def per_cell(U, C, Z, W, truth):
    F, K = U["n_frames"], U["n_nodes"]
    o_f, o_k = U["o_f"], U["o_k"]
    val = U["obs"]["value"]
    cover = np.zeros((F, K), bool); cover[o_f, o_k] = True
    def sc(M):
        Mo = M[o_f, o_k] if M.ndim == 2 else M
        s = np.zeros(K); c = np.zeros(K); np.add.at(s, o_k, Mo); np.add.at(c, o_k, 1.0)
        m = np.where(c>0, s/np.maximum(c,1), 0.0)
        v = np.zeros(K); np.add.at(v, o_k, (Mo-m[o_k])**2)
        return np.sqrt(np.where(c>1, v/np.maximum(c-1,1), np.nan))
    Sb = sc(val); Sa = sc(Z)
    s = np.zeros(K); c = np.zeros(K); np.add.at(s, o_k, Z[o_f, o_k]); np.add.at(c, o_k, 1.0)
    Mest = np.where(c>0, s/np.maximum(c,1), np.nan)
    out = dict(J_res=Sa/np.maximum(Sb,1e-300), Mest=Mest)
    if truth is not None:
        Mt = truth["M_target"]
        s2 = np.zeros(K); c2 = np.zeros(K)
        for f in range(F):
            m = cover[f]; s2[m] += Mt[f, m]; c2[m] += 1
        tgt = np.where(c2>0, s2/np.maximum(c2,1), np.nan)
        out["J_dim"] = np.abs((Mest-tgt)/np.maximum(np.abs(tgt),1e-300))
        out["tgt"] = tgt
    return out

def main():
    names = sys.argv[1:] or ["A_base","B_prod","real49"]
    for name in names:
        try: U, summ, truth = load(name)
        except Exception as e:
            print(name, "skip", e); continue
        lams = [e["lambda"] for e in summ]
        per = []
        for e in summ:
            _, C, Z, W = an.read_result(e["bin"])
            per.append(per_cell(U, C, Z, W, truth))
        K = U["n_nodes"]
        multi = np.zeros(K, bool)
        cover = np.zeros((U["n_frames"], K), bool); cover[U["o_f"], U["o_k"]] = True
        multi = cover.sum(0) >= 2
        # 逐 cell 最优 λs（先按 J_res 单独，再按 J_res+J_dim 合成）
        R = np.stack([p["J_res"] for p in per], 0)            # (L,K)
        valid = np.isfinite(R).all(0) & multi
        best_res = np.array(lams)[np.nanargmin(np.where(np.isfinite(R), R, np.inf), axis=0)]
        rows = []
        if "J_dim" in per[0]:
            Dm = np.stack([p["J_dim"] for p in per], 0)
            tgt = per[0]["tgt"]
            bright = valid & (tgt >= np.nanpercentile(tgt[valid], 90))
            faint = valid & (tgt <= np.nanpercentile(tgt[valid], 50))
            # 局部合成代价：残留 + 10x 压暗（压暗是保真度硬约束）
            J = R + 10.0*np.where(np.isfinite(Dm), Dm, 0.0)
            best_j = np.array(lams)[np.nanargmin(np.where(np.isfinite(J), J, np.inf), axis=0)]
            rows.append(dict(group="bright", n=int(bright.sum()),
                             best_res_median=float(np.median(best_res[bright])),
                             best_j_median=float(np.median(best_j[bright])),
                             frac_best_res_gt_0p1=float(np.mean(best_res[bright] > 0.1))))
            rows.append(dict(group="faint", n=int(faint.sum()),
                             best_res_median=float(np.median(best_res[faint])),
                             best_j_median=float(np.median(best_j[faint])),
                             frac_best_res_gt_0p1=float(np.mean(best_res[faint] > 0.1))))
        # 全局最优 vs 逐 cell oracle 的代价差（自适应上界收益）
        def tot(Rm):
            Rm = np.asarray(Rm, float)
            return float(np.nanmedian(Rm[valid] if Rm.ndim == 1 else Rm[:, valid]))
        glob = {l: tot(R[i]) for i, l in enumerate(lams)}
        gbest = min(glob, key=glob.get)
        oracle = float(np.nanmedian(np.nanmin(np.where(np.isfinite(R), R, np.inf), axis=0)[valid]))
        rows.append(dict(group="GLOBAL", n=int(valid.sum()), best_res_median=float(gbest),
                         global_cost=glob[gbest], percell_oracle_cost=oracle,
                         adaptive_gain=glob[gbest]-oracle,
                         adaptive_gain_rel=(glob[gbest]-oracle)/max(1e-30, glob[gbest])))
        print("==", name, "lambda grid", lams)
        for r in rows: print("  ", json.dumps(r))
        json.dump(dict(name=name, lams=lams, rows=rows, global_cost=glob),
                  open(os.path.join(D, "adaptive_%s.json" % name), "w"), indent=1)

if __name__ == "__main__":
    main()
