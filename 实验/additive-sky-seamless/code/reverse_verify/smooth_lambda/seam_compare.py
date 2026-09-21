#!/usr/bin/env python3
"""成品层台阶：两次运行在**同一边集合**上的配对比较（消除 nused 边界集差异的混杂）。"""
import json, os, sys
import numpy as np
_HERE = os.path.dirname(os.path.abspath(__file__))   # .../实验/<unit>/code/reverse_verify/smooth_lambda
ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))   # 仓库根（从脚本自身位置推导）

def load(out_dir):
    integ = json.load(open(os.path.join(out_dir, "p2_integrated.json")))
    f = integ["files"]
    sig = np.fromfile(f["signal"], np.float64)
    nused = np.fromfile(f["nused"], np.int32)
    return integ, sig, nused

def edges(integ, sig, nused):
    """返回每条边的 (dS, dU, loc)，边按 tile 内水平/垂直统一编号。"""
    dS = []; dU = []; loc = []
    for t in integ["tiles"]:
        off, n = t["offset"], t["n_pixels"]
        S = sig[off:off+n].reshape(512, 512); U = nused[off:off+n].reshape(512, 512)
        for ax in (0, 1):
            a = S[:-1, :] if ax == 0 else S[:, :-1]
            b = S[1:, :] if ax == 0 else S[:, 1:]
            ua = U[:-1, :] if ax == 0 else U[:, :-1]
            ub = U[1:, :] if ax == 0 else U[:, 1:]
            dS.append(np.abs(a-b).ravel()); dU.append(np.abs(ua-ub).ravel())
            loc.append((0.5*(np.abs(a)+np.abs(b))).ravel())
    return np.concatenate(dS), np.concatenate(dU), np.concatenate(loc)

def main():
    A = sys.argv[1]; B = sys.argv[2]
    ia, sa, ua = load(A); ib, sb, ub = load(B)
    dSa, dUa, la = edges(ia, sa, ua)
    dSb, dUb, lb = edges(ib, sb, ub)
    assert dSa.size == dSb.size, (dSa.size, dSb.size)
    ok = np.isfinite(dSa) & np.isfinite(dSb) & np.isfinite(la) & np.isfinite(lb) & (la > 0) & (lb > 0)
    mA = ok & (dUa > 0); mB = ok & (dUb > 0); mI = ok & (dUa == 0) & (dUb == 0)
    both = mA & mB
    res = {"A": A, "B": B, "n_edges": int(dSa.size),
           "n_boundary_A": int(mA.sum()), "n_boundary_B": int(mB.sum()),
           "n_boundary_both": int(both.sum()), "n_internal_both": int(mI.sum())}
    for tag, m in (("A_all_boundary", mA), ("B_all_boundary", mB),
                   ("paired_boundary", both), ("paired_internal", mI)):
        if m.sum() < 10: continue
        for lbl, d in (("A", dSa), ("B", dSb)):
            res["%s_%s_step_med" % (tag, lbl)] = float(np.median(d[m]))
            res["%s_%s_step_rel" % (tag, lbl)] = float(np.median(d[m]/np.maximum(la if lbl=="A" else lb, 1e-300)[m]))
    for tag in ("A_all_boundary", "B_all_boundary", "paired_boundary"):
        if tag+"_A_step_med" in res and "paired_internal_A_step_med" in res:
            base = res["paired_internal_A_step_med"] if tag != "B_all_boundary" else res["paired_internal_A_step_med"]
            res[tag+"_ratio_A"] = res[tag+"_A_step_med"]/max(1e-300, base)
            res[tag+"_ratio_B"] = res[tag+"_B_step_med"]/max(1e-300, res["paired_internal_B_step_med"])
    print(json.dumps(res, indent=1))
    json.dump(res, open(os.path.join(ROOT, "run/reverse_verify/smooth_lambda/product_seam_paired.json"), "w"), indent=1)

if __name__ == "__main__":
    main()
