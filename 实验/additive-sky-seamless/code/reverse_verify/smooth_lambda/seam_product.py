#!/usr/bin/env python3
"""成品层（p2_integrated_signal/nused）真实覆盖边界台阶测量。

台阶 = 相邻像素 nused 不同（覆盖子集突变）的 |Δsignal| 中位 /
       相邻像素 nused 相同（内部）的 |Δsignal| 中位（与 FIX-P2a/Q2 同口径）。
另出亮区保真：M42 核心区（signal 前 1%）的面亮度随 λs 的变化。
用法: python3 seam_product.py <out_dir> <tag>
"""
import json, os, sys, math
import numpy as np
_HERE = os.path.dirname(os.path.abspath(__file__))   # .../实验/<unit>/code/reverse_verify/smooth_lambda
ROOT = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))   # 仓库根（从脚本自身位置推导）
def main():
    out_dir, tag = sys.argv[1], sys.argv[2]
    integ = json.load(open(os.path.join(out_dir, "p2_integrated.json")))
    files = integ["files"]
    # 实测 dtype：signal/support=float64（n_pixels 元素），nused=int32，wsum=float32
    sig = np.fromfile(files["signal"], np.float64)
    nused = np.fromfile(files["nused"], np.int32) if os.path.exists(files["nused"]) else None
    tiles = integ["tiles"]
    res = {"tag": tag, "n_pixels": int(sig.size), "out_dir": out_dir}
    if nused is None:
        print("no nused"); return
    assert sig.size == nused.size, (sig.size, nused.size)
    tot_b = []; tot_i = []; rel_b = []; rel_i = []
    for t in tiles:
        off, n = t["offset"], t["n_pixels"]
        s = sig[off:off+n]; u = nused[off:off+n]
        side = 512
        S = s.reshape(side, side); U = u.reshape(side, side)
        for axis in (0, 1):
            dS = np.abs(np.diff(S, axis=axis)); dU = np.abs(np.diff(U, axis=axis))
            loc = 0.5*(np.abs(S[:-1,:] if axis==0 else S[:,:-1]) + np.abs(S[1:,:] if axis==0 else S[:,1:]))
            m = (loc > 0) & np.isfinite(dS) & np.isfinite(loc)
            b = m & (dU > 0); i = m & (dU == 0)
            tot_b.append(dS[b]); tot_i.append(dS[i])
            rel_b.append(dS[b]/np.maximum(loc[b],1e-30)); rel_i.append(dS[i]/np.maximum(loc[i],1e-30))
    B = np.concatenate(tot_b); I = np.concatenate(tot_i)
    RB = np.concatenate(rel_b); RI = np.concatenate(rel_i)
    res.update(n_edge_boundary=int(B.size), n_edge_internal=int(I.size),
               step_boundary_med=float(np.median(B)), step_internal_med=float(np.median(I)),
               step_ratio=float(np.median(B)/max(1e-30,np.median(I))),
               step_ratio_rel=float(np.median(RB)/max(1e-30,np.median(RI))),
               step_boundary_rel=float(np.median(RB)), step_internal_rel=float(np.median(RI)))
    fin = os.path.join(out_dir, "p2_final.json")
    if os.path.exists(fin):
        res["final"] = json.load(open(fin))
    print(json.dumps(res, indent=1))
    json.dump(res, open(os.path.join(ROOT, "run/reverse_verify/smooth_lambda", "product_seam_%s.json" % tag), "w"), indent=1)
if __name__ == "__main__":
    main()
