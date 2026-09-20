#!/usr/bin/env python3
"""RELEASE-02 A5：λs 扫描结果分析（合成 + 真实数据通用）。

输入：UPMB 观测文件 + upm_sweep 输出的 <prefix>.summary.jsonl 与 <prefix>_lam*.bin
输出：<prefix>.metrics.json / .csv（三曲线）+ 可选 PNG。
"""
import argparse, glob, json, math, os, re, sys
import numpy as np

ROOT = "/workspace/Astro CS Database"
D = os.path.join(ROOT, "run/reverse_verify/smooth_lambda")

def read_upmb(path):
    with open(path, "rb") as f:
        buf = f.read()
    off = 0
    magic, ver = np.frombuffer(buf, np.uint32, 2, off); off += 8
    assert magic == 0x55504D31, hex(magic)
    n_obs, n_nodes, n_frames = np.frombuffer(buf, np.uint64, 3, off); off += 24
    frames = np.frombuffer(buf, np.uint64, n_frames, off).copy(); off += 8*n_frames
    dt = np.dtype([("frame_id","<u8"),("control_id","<u8"),("leaf_ipix","<u8"),
                   ("ra","<f8"),("dec","<f8"),("value","<f8"),("unc","<f8"),
                   ("snr","<f8"),("ivar","<f8"),("cvar","<f8"),("civar","<f8"),
                   ("snrav","<i4"),("support","<f8"),("qflags","<u4")])
    obs = np.frombuffer(buf, dt, n_obs, off).copy(); off += dt.itemsize*n_obs
    nd = np.dtype([("control_id","<u8"),("tile_ipix","<u8"),("gx","<i4"),("gy","<i4"),
                   ("ra","<f8"),("dec","<f8"),("leaf_ipix","<u8")])
    nodes = np.frombuffer(buf, nd, n_nodes, off).copy(); off += nd.itemsize*n_nodes
    fidx = {int(v): i for i, v in enumerate(frames)}
    o_f = np.array([fidx[int(v)] for v in obs["frame_id"]], np.int32)
    cid2k = {int(c): i for i, c in enumerate(nodes["control_id"])}
    o_k = np.array([cid2k[int(v)] for v in obs["control_id"]], np.int32)
    return dict(frames=frames, obs=obs, nodes=nodes, n_obs=int(n_obs),
                n_nodes=int(n_nodes), n_frames=int(n_frames), o_f=o_f, o_k=o_k)

def read_result(path):
    with open(path, "rb") as f:
        buf = f.read()
    off = 0
    magic, ver = np.frombuffer(buf, np.uint32, 2, off); off += 8
    assert magic == 0x55504D32, hex(magic)
    F, K, N = np.frombuffer(buf, np.uint64, 3, off); off += 24
    F, K, N = int(F), int(K), int(N)
    frames = np.frombuffer(buf, np.uint64, F, off).copy(); off += 8*F
    C = np.frombuffer(buf, np.float64, F*K, off).copy().reshape(F, K); off += 8*F*K
    Z = np.frombuffer(buf, np.float64, F*K, off).copy().reshape(F, K); off += 8*F*K
    W = np.frombuffer(buf, np.float64, N, off).copy()
    return frames, C, Z, W

def wcell(U):
    """份额式 w_cell = control_ivar / Σ_cell control_ivar（尺度无关判据，与 upm.cpp:645-649 同式）。
    注意：公共 API p2_upm_normalized_weights 仍用绝对门 s>1e-12（已登记缺陷
    CONFORM-SWEEP-3-003），生产 control_ivar 尺度下返回全 0，故此处自行复算。"""
    civ = U["obs"]["civar"]; o_k = U["o_k"]
    s = np.zeros(U["n_nodes"]); np.add.at(s, o_k, civ)
    ss = s[o_k]
    w = np.where((ss > 0) & np.isfinite(ss), civ/np.maximum(ss, 1e-300), 0.0)
    return w

def med(a):
    a = np.asarray(a, float)
    if not a.size: return float("nan")
    with np.errstate(all="ignore"):
        return float(np.nanmedian(a))

def metrics(U, C, Z, W, truth=None):
    F, K, N = U["n_frames"], U["n_nodes"], U["n_obs"]
    o_f, o_k = U["o_f"], U["o_k"]
    val = U["obs"]["value"]; unc = U["obs"]["unc"]
    # 覆盖矩阵
    cover = np.zeros((F, K), bool)
    cover[o_f, o_k] = True
    ncov = cover.sum(0)
    multi = ncov >= 2
    def po(M):
        return M[o_f, o_k] if M.ndim == 2 else M
    # 逐 control 帧间散差（校正前/后）
    def scatter(M):
        Mo = po(M)
        s = np.zeros(K); c = np.zeros(K)
        np.add.at(s, o_k, Mo); np.add.at(c, o_k, 1.0)
        mean = np.where(c > 0, s/np.maximum(c,1), 0.0)
        v = np.zeros(K); np.add.at(v, o_k, (Mo-mean[o_k])**2)
        return np.sqrt(np.where(c > 1, v/np.maximum(c-1, 1), np.nan))
    S_before = scatter(val)
    S_after = scatter(Z)
    # 逐 control 的测量噪声（合同 uncertainty）
    u2 = np.zeros(K); np.add.at(u2, o_k, unc**2); cn = np.zeros(K); np.add.at(cn, o_k, 1.0)
    S_noise = np.sqrt(u2/np.maximum(cn,1))
    # 产品（stack）：w_cell 加权与单位权（生产 integrate weight_mode=1）
    def stack(M, w=None):
        Mo = po(M)
        num = np.zeros(K); den = np.zeros(K)
        if w is None:
            np.add.at(num, o_k, Mo); np.add.at(den, o_k, 1.0)
        else:
            np.add.at(num, o_k, w*Mo); np.add.at(den, o_k, w)
        return np.where(den > 0, num/np.maximum(den,1e-300), np.nan), den
    M_est_u, _ = stack(Z, None)
    M_est_w, _ = stack(Z, W)
    y_stack_u, _ = stack(val, None)
    M_raw_w, _ = stack(val, W)
    out = dict(
        wcell_sum=float(np.sum(W)),
        n_multi=int(multi.sum()),
        S_before_med=med(S_before[multi]), S_after_med=med(S_after[multi]),
        S_noise_med=med(S_noise[multi]),
        removal_frac=1.0 - med(S_after[multi])/max(1e-300, med(S_before[multi])),
        resid_over_noise=med(S_after[multi])/max(1e-300, med(S_noise[multi])),
        c_rms=float(np.sqrt(np.nanmean(C**2))), c_absmax=float(np.nanmax(np.abs(C))),
    )
    # C 场空间梯度（同 tile 内相邻 node）
    tiles = U["nodes"]["tile_ipix"]; gx = U["nodes"]["gx"]; gy = U["nodes"]["gy"]
    key = np.array([(int(t) & 0xFFFFFF)*64 + int(b)*8 + int(a) for t,a,b in zip(tiles,gx,gy)], np.int64)
    order = np.argsort(key, kind="stable")
    ks = key[order]
    same = (ks[1:] == ks[:-1] + 1)
    a = order[:-1][same]; b = order[1:][same]
    dC = C[:, a] - C[:, b]
    out["c_grad_rms"] = float(np.sqrt(np.nanmean(dC**2)))
    # 覆盖子集边界台阶（Q2 §6.2 口径）：相邻 node 的 |Δ stack|
    sub = cover[:, a]  # (F, E)
    diff_sub = (sub != cover[:, b]).any(0)
    dS = np.abs(M_est_u[a] - M_est_u[b])
    dSw = np.abs(M_est_w[a] - M_est_w[b])
    loc = 0.5*(np.abs(M_est_u[a]) + np.abs(M_est_u[b]))
    out["edge_boundary"] = int(diff_sub.sum()); out["edge_internal"] = int((~diff_sub).sum())
    out["step_boundary_med"] = med(dS[diff_sub]); out["step_internal_med"] = med(dS[~diff_sub])
    out["step_ratio"] = out["step_boundary_med"]/max(1e-300, out["step_internal_med"])
    out["step_ratio_w"] = med(dSw[diff_sub])/max(1e-300, med(dSw[~diff_sub]))
    out["step_boundary_rel"] = med(dS[diff_sub]/np.maximum(loc[diff_sub],1e-300))
    out["step_internal_rel"] = med(dS[~diff_sub]/np.maximum(loc[~diff_sub],1e-300))
    out["step_ratio_rel"] = out["step_boundary_rel"]/max(1e-300, out["step_internal_rel"])
    # 亮度匹配台阶比：边界边与**同亮度分位**内部边比较（消除「边界边恰好落在暗/亮区」混杂）
    lv = np.log10(np.maximum(loc, 1e-30))
    edges_ok = np.isfinite(lv) & np.isfinite(dS)
    if edges_ok.sum() > 100:
        qs = np.nanpercentile(lv[edges_ok], np.linspace(0, 100, 11))
        bq = np.clip(np.digitize(lv, qs[1:-1]), 0, 9)
        num = []; den = []
        for b in range(10):
            mB = edges_ok & (bq == b) & diff_sub; mI = edges_ok & (bq == b) & (~diff_sub)
            if mB.sum() >= 5 and mI.sum() >= 5:
                num.append(med(dS[mB])); den.append(med(dS[mI]))
        if num:
            out["step_ratio_matched"] = float(np.median(np.array(num)/np.maximum(np.array(den), 1e-300)))
            out["step_ratio_matched_nbin"] = len(num)
    # 亮区保真（真值在场）
    if truth is not None:
        Mt = truth["M_target"]; B = truth["B"]; Mtrue = truth["M_true"]
        # 逐 control 真值公共场（w 加权）
        tgt = np.zeros(K); den = np.zeros(K)
        for f in range(F):
            m = cover[f]
            tgt[m] += Mt[f, m]; den[m] += 1
        tgt = np.where(den>0, tgt/np.maximum(den,1), np.nan)
        out["M_target_med"] = med(tgt[multi])
        # 压暗：产品 vs 真值目标（单位权，与生产 integrate 一致）
        dim = (M_est_u - tgt)/np.maximum(np.abs(tgt), 1e-300)
        base = tgt.copy()
        q = np.nanpercentile(base[multi], [50, 90, 99])
        out["q50_tgt"], out["q90_tgt"], out["q99_tgt"] = [float(x) for x in q]
        bright = multi & (base >= q[1]); faint = multi & (base <= q[0]); peak = multi & (base >= q[2])
        out["dim_all_med"] = med(dim[multi]); out["dim_bright_med"] = med(dim[bright])
        out["dim_faint_med"] = med(dim[faint]); out["dim_peak_med"] = med(dim[peak])
        out["dim_bright_diff"] = out["dim_bright_med"] - out["dim_faint_med"]
        out["dim_peak_diff"] = out["dim_peak_med"] - out["dim_faint_med"]
        out["n_bright"] = int(bright.sum()); out["n_peak"] = int(peak.sum())
        # 天光残留（真值）：S_sky
        S_sky = np.zeros(K); c = np.zeros(K)
        np.add.at(S_sky, o_k, 0.0)
        # 用真值 B 的帧间散差
        Bs = np.zeros(K); Bc = np.zeros(K)
        for f in range(F):
            m = cover[f]
            Bs[m] += B[f, m]; Bc[m] += 1
        Bm = np.where(Bc>0, Bs/np.maximum(Bc,1), 0.0)
        Bv = np.zeros(K)
        for f in range(F):
            m = cover[f]
            Bv[m] += (B[f, m]-Bm[m])**2
        S_sky = np.sqrt(np.where(Bc>1, Bv/np.maximum(Bc-1,1), np.nan))
        out["S_sky_med"] = med(S_sky[multi])
        out["resid_over_sky"] = med(S_after[multi])/max(1e-300, med(S_sky[multi]))
        # 逐帧 C 场与逐帧天光的空间相关（是否真的在扣天光）
        cs = []
        for f in range(F):
            m = cover[f]
            if m.sum() < 20: continue
            cf = C[f, m]; bf = B[f, m]
            if np.std(cf) < 1e-30 or np.std(bf) < 1e-30: continue
            cs.append(float(np.corrcoef(cf, bf)[0,1]))
        out["corr_C_B_med"] = med(cs)
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--upmb", required=True)
    ap.add_argument("--prefix", required=True)
    ap.add_argument("--truth", default=None)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    U = read_upmb(a.upmb)
    truth = dict(np.load(a.truth)) if a.truth and os.path.exists(a.truth) else None
    rows = []
    for line in open(a.prefix + ".summary.jsonl"):
        e = json.loads(line)
        if "lambda" not in e: continue
        if "bin" not in e:
            rows.append(dict(e, **{"error": "build_failed"})); continue
        _, C, Z, W = read_result(e["bin"])
        if not np.any(W > 0):
            W = wcell(U)          # 绕开 CONFORM-SWEEP-3-003（公共 API 绝对门缺陷）
        m = metrics(U, C, Z, W, truth)
        rows.append(dict(e, **m))
        print(f"lam={e['lambda']:<10g} removal={m['removal_frac']:.4f} "
              f"resid/noise={m['resid_over_noise']:.3f} stepratio={m['step_ratio']:.3f} "
              f"crms={m['c_rms']:.4g} cgrad={m['c_grad_rms']:.4g} "
              + (f"dimB={m['dim_bright_med']*100:+.4f}% dimPk={m['dim_peak_med']*100:+.4f}%" if truth is not None else ""))
    rows.sort(key=lambda r: r["lambda"])
    json.dump(rows, open(a.out + ".metrics.json", "w"), indent=1)
    keys = ["lambda","seconds","iterations","converged","objective","c_rms","c_grad_rms",
            "removal_frac","resid_over_noise","S_before_med","S_after_med","S_noise_med",
            "step_ratio","step_ratio_w","step_ratio_rel","step_ratio_matched","step_boundary_med","step_internal_med",
            "step_boundary_rel","step_internal_rel"]
    if truth is not None:
        keys += ["S_sky_med","resid_over_sky","corr_C_B_med","dim_all_med","dim_bright_med",
                 "dim_faint_med","dim_peak_med","dim_bright_diff","dim_peak_diff",
                 "q50_tgt","q90_tgt","q99_tgt","n_bright","n_peak"]
    with open(a.out + ".csv", "w") as f:
        f.write(",".join(keys) + "\n")
        for r in rows:
            f.write(",".join(("%.10g" % r[k]) if isinstance(r.get(k), (int,float)) else str(r.get(k,"")) for k in keys) + "\n")
    print("wrote", a.out + ".csv")

if __name__ == "__main__":
    main()
