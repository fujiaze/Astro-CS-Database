#!/usr/bin/env python3
"""#80 P5 E0 — 独立白化 SVD/QR 与投影 GLS/Schur oracle（纯 NumPy，不读 C++ 状态）。

目的：对冻结的 p2_samples.json（生产 sampler 落盘的真实 17237 样本）独立重建
生产 sky plane 的联合加权最小二乘正规方程，并逐项比较：

  1) 生产同序装配：H_data = Σ_k A_kᵀW A_k（一次性累加），再逐帧减 S_k M_k⁻¹ S_kᵀ；
  2) 分帧 Schur：H_red = Σ_k [A_kᵀW A_k − S_k M_k⁻¹ S_kᵀ]（数学恒等，浮点分帧）；
  3) 白化 QR 残差 Gram：A_wᵀ(I−Q Qᵀ)A_w（Gram ⇒ 构造性 PSD）；
  4) 单位归一分帧 Schur（把两代 P1 产品换算到同一数值尺度后的物理模型）。

真值口径（与 lib/algorithms/coverage/src/sky_plane.cpp 逐行对应，行号见注释）：
  · 切平面中心 = used 的球面均值方向（:1629-1645），gnomonic 投影（:174-193）；
  · B 样条 d=cfg.spline_degree（默认 3），nx/ny/t0 见 :1666-1695；basis 见 :197-222；
  · δ 归一化多项式 order=cfg.frame_gradient_order（默认 1）⇒ m=3，见 :224-237；
  · gauge = 逐连通分量最小 frame_id 为锚帧，δ_anchor ≡ 0（:640,724-728）；锚帧不进 Schur（:2057）；
  · W = 1/variance（weight_mode=0，:1726），variance = observations[].control_variance
    （module_adapters.cpp:11386-11394）；
  · λ_eff = rank_rtol(1e-10) · mean(diag(H_red))（:2129-2132）。

本脚本只读、只诊断：不加 ridge、不改生产算法、不宣称 P5 科学闭合。
"""
from __future__ import annotations

import argparse
import json
import math
import os
import sys

import numpy as np

RANK_RTOL = 1e-10
FLAG_BAD = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4)  # MASKED|LOW_SUPPORT|HIGH_CONTAM|REJECTED


# ---------------------------------------------------------------- 生产逐行复刻
def gnomonic(ra0, dec0, ra, dec):
    d2r = math.pi / 180.0
    r0, d0 = ra0 * d2r, dec0 * d2r
    r, d = ra * d2r, dec * d2r
    dl = r - r0
    while dl > math.pi:
        dl -= 2.0 * math.pi
    while dl < -math.pi:
        dl += 2.0 * math.pi
    cd, sd = math.cos(d), math.sin(d)
    cd0, sd0 = math.cos(d0), math.sin(d0)
    cdl, sdl = math.cos(dl), math.sin(dl)
    cosc = sd0 * sd + cd0 * cd * cdl
    if cosc <= 1e-12:
        raise ValueError("sample outside tangent hemisphere")
    u = cd * sdl / cosc
    v = (cd0 * sd - sd0 * cd * cdl) / cosc
    return u / d2r, v / d2r


def bspline_basis(u, t0, h, n_coef, d):
    span = int(math.floor((u - t0) / h))
    span = max(span, d)
    span = min(span, n_coef - 1)
    N = [0.0] * (d + 1)
    left = [0.0] * (d + 1)
    right = [0.0] * (d + 1)
    N[0] = 1.0
    for j in range(1, d + 1):
        left[j] = u - (t0 + (span + 1 - j) * h)
        right[j] = (t0 + (span + j) * h) - u
        saved = 0.0
        for r in range(j):
            denom = right[r + 1] + left[j - r]
            temp = (N[r] / denom) if denom != 0.0 else 0.0
            N[r] = saved + right[r + 1] * temp
            saved = left[j - r] * temp
        N[j] = saved
    return span - d, N


def delta_basis(u, v, uc, vc, us, vs, order):
    x = (u - uc) / us
    y = (v - vc) / vs
    out = []
    for total in range(order + 1):
        for aa in range(total + 1):
            b = total - aa
            out.append(x ** aa * y ** b)
    return out


def chol_pivots(A):
    """返回 (ok, min_pivot, fail_index, 全部主元)；A 视为对称，只读下三角。"""
    n = A.shape[0]
    L = np.zeros_like(A)
    piv = np.empty(n)
    for j in range(n):
        s = A[j, j] - float(np.dot(L[j, :j], L[j, :j]))
        if not (s > 0.0) or not np.isfinite(s):
            piv[j] = s
            return False, float(np.nanmin(piv[:j + 1]) if j else s), j, piv
        d = math.sqrt(s)
        L[j, j] = d
        piv[j] = s
        if j + 1 < n:
            L[j + 1:, j] = (A[j + 1:, j] - L[j + 1:, :j] @ L[j, :j]) / d
    return True, float(piv.min()), -1, piv


def eig_report(H, rtol=RANK_RTOL):
    Hs = 0.5 * (H + H.T)
    w = np.linalg.eigvalsh(Hs)
    wmax = float(w[-1])
    wmin = float(w[0])
    thresh = rtol * abs(wmax) if wmax != 0 else 0.0
    rank = int(np.count_nonzero(np.abs(w) > thresh))
    kappa = float(abs(wmax) / abs(wmin)) if wmin != 0 else float("inf")
    return {
        "lambda_min": wmin,
        "lambda_max": wmax,
        "effective_rank": rank,
        "n": int(H.shape[0]),
        "kappa": kappa,
        "n_negative": int(np.count_nonzero(w < 0.0)),
        "most_negative": wmin,
        "spectrum_head": [float(x) for x in w[:6]],
        "spectrum_tail": [float(x) for x in w[-4:]],
    }


def sym_err(M):
    nrm = float(np.linalg.norm(M))
    if nrm == 0.0:
        return 0.0
    return float(np.linalg.norm(M - M.T)) / nrm


# ---------------------------------------------------------------- 载入与装配
class Design:
    def __init__(self, samples_path, h, spline_degree=3, order=1):
        doc = json.load(open(samples_path))
        obs = doc["observations"]
        self.n_obs_total = len(obs)
        self.frame_ids = sorted(doc["frame_ids"])
        self.raw_frame_ids = list(doc["frame_ids"])
        self.input_manifest_hash = doc.get("input_manifest_hash")
        fid = np.array([o["frame_id"] for o in obs], dtype=np.uint64)
        ra = np.array([o["ra_deg"] for o in obs], dtype=float)
        dec = np.array([o["dec_deg"] for o in obs], dtype=float)
        val = np.array([o["value"] for o in obs], dtype=float)
        var = np.array([o["control_variance"] for o in obs], dtype=float)
        flags = np.zeros(len(obs), dtype=np.uint32)  # NO_LOCAL_SNR 不在 bad 掩码内
        keep = ((flags & FLAG_BAD) == 0) & np.isfinite(ra) & np.isfinite(dec) \
            & np.isfinite(val) & (var > 0.0) & np.isfinite(var)
        self.n_masked = int((~keep).sum())
        fid, ra, dec, val, var = fid[keep], ra[keep], dec[keep], val[keep], var[keep]
        self.fid, self.ra, self.dec, self.val, self.var = fid, ra, dec, val, var
        self.n = len(val)
        self.d = spline_degree
        self.order = order
        self.m = (order + 1) * (order + 2) // 2
        # 切平面中心 = 球面均值方向
        d2r = math.pi / 180.0
        sx = float(np.sum(np.cos(dec * d2r) * np.cos(ra * d2r)))
        sy = float(np.sum(np.cos(dec * d2r) * np.sin(ra * d2r)))
        sz = float(np.sum(np.sin(dec * d2r)))
        nrm = math.sqrt(sx * sx + sy * sy + sz * sz)
        sx, sy, sz = sx / nrm, sy / nrm, sz / nrm
        self.dec0 = math.degrees(math.asin(max(-1.0, min(1.0, sz))))
        ra0 = math.degrees(math.atan2(sy, sx))
        self.ra0 = ra0 + 360.0 if ra0 < 0 else ra0
        su = np.empty(self.n)
        sv = np.empty(self.n)
        for i in range(self.n):
            su[i], sv[i] = gnomonic(self.ra0, self.dec0, ra[i], dec[i])
        self.su, self.sv = su, sv
        umin, umax = float(su.min()), float(su.max())
        vmin, vmax = float(sv.min()), float(sv.max())
        if umax - umin < h:
            c = 0.5 * (umin + umax)
            umin, umax = c - 0.5 * h, c + 0.5 * h
        if vmax - vmin < h:
            c = 0.5 * (vmin + vmax)
            vmin, vmax = c - 0.5 * h, c + 0.5 * h
        self.umin, self.umax, self.vmin, self.vmax = umin, umax, vmin, vmax
        self.h = h
        self.t0u = umin - spline_degree * h
        self.t0v = vmin - spline_degree * h
        self.nx = int(math.ceil((umax - umin) / h)) + spline_degree
        self.ny = int(math.ceil((vmax - vmin) / h)) + spline_degree
        self.nx = max(self.nx, spline_degree + 1)
        self.ny = max(self.ny, spline_degree + 1)
        self.n_full = self.nx * self.ny
        self.uc = 0.5 * (umin + umax)
        self.vc = 0.5 * (vmin + vmax)
        self.us = max(0.5 * (umax - umin), 1e-9)
        self.vs = max(0.5 * (vmax - vmin), 1e-9)
        # 基
        nb = (spline_degree + 1) * (spline_degree + 1)
        bidx = np.full((self.n, nb), -1, dtype=np.int64)
        bval = np.zeros((self.n, nb))
        for t in range(self.n):
            jx0, nxv = bspline_basis(su[t], self.t0u, h, self.nx, spline_degree)
            jy0, nyv = bspline_basis(sv[t], self.t0v, h, self.ny, spline_degree)
            c = 0
            for a in range(spline_degree + 1):
                for b in range(spline_degree + 1):
                    bidx[t, c] = (jy0 + a) * self.nx + (jx0 + b)
                    bval[t, c] = nyv[a] * nxv[b]
                    c += 1
        self.bidx, self.bval = bidx, bval
        node_weight = np.zeros(self.n_full)
        for c in range(nb):
            np.add.at(node_weight, bidx[:, c], bval[:, c] ** 2)
        mx = node_weight.max() if self.n_full else 0.0
        floor = 1e-9 * max(mx, 1e-300)
        free_of_full = np.full(self.n_full, -1, dtype=np.int64)
        full_of_free = []
        for i in range(self.n_full):
            if node_weight[i] > floor:
                free_of_full[i] = len(full_of_free)
                full_of_free.append(i)
        self.free_of_full = free_of_full
        self.full_of_free = np.array(full_of_free, dtype=np.int64)
        self.n_free = len(full_of_free)
        bfree = np.where(bidx >= 0, free_of_full[np.clip(bidx, 0, None)], -1)
        bfree[bidx < 0] = -1
        self.bfree = bfree
        # δ 基
        pu = np.zeros((self.n, self.m))
        for t in range(self.n):
            pu[t, :] = delta_basis(su[t], sv[t], self.uc, self.vc, self.us, self.vs, order)
        self.pu = pu

    def sparse_dense(self, mat, cols):
        """把 (n, nb) 稀疏行 + 映射展开成 (n, n_free or K*m) 稠密设计。"""
        out = np.zeros((self.n, cols))
        for c in range(mat.shape[1]):
            idx = self.bfree[:, c]
            ok = idx >= 0
            out[ok, idx[ok]] += mat[ok, c]
        return out

    def assemble(self, anchor_frame_id):
        """返回 A(n×n_free)、P(n×K*m)、w、以及 frame 分组。P 只在非锚帧有块。"""
        A = self.sparse_dense(self.bval, self.n_free)
        # P 只含**非锚帧**的 δ 块（锚帧 δ≡0，生产在 Schur 循环里 continue，
        # 等价于把该块整列去掉）⇒ q = (K - n_anchor) * m。
        blocks = []
        for f in self.frame_ids:
            sel = self.fid == np.uint64(f)
            if f == anchor_frame_id:
                continue
            blocks.append(self.pu[sel, :])
        if blocks:
            P = np.zeros((self.n, sum(b.shape[1] for b in blocks)))
            off = 0
            for f, b in zip([x for x in self.frame_ids if x != anchor_frame_id], blocks):
                sel = self.fid == np.uint64(f)
                P[sel, off:off + b.shape[1]] = b
                off += b.shape[1]
        else:
            P = np.zeros((self.n, 0))
        w = 1.0 / self.var
        return A, P, w


def schur_joint(A, P, w):
    """生产同序：H_data 一次性累加，再逐帧减 S_k M_k⁻¹ S_kᵀ（:1836-2120）。"""
    sw = np.sqrt(w)
    Aw = A * sw[:, None]
    Pw = P * sw[:, None]
    H_data = Aw.T @ Aw
    H_red = H_data.copy()
    detail = []
    q = P.shape[1]
    for j in range(q):
        pj = Pw[:, j]
        nj = float(pj @ pj)
        detail.append(nj)
    if q:
        M = Pw.T @ Pw
        S = Aw.T @ Pw
        # 逐帧（逐 δ 块）消元，复刻生产循环（:2055-2120）
        H_red = H_data.copy()
        for b in range(0, q, 3):
            sl = slice(b, b + 3)
            Mb = M[sl, sl]
            Sb = S[:, sl]
            try:
                Z = np.linalg.solve(Mb, Sb.T)      # M⁻¹Sᵀ
                H_red = H_red - Sb @ Z
            except np.linalg.LinAlgError:
                H_red = H_red - Sb @ np.linalg.pinv(Mb) @ Sb.T
    return H_red, H_data


def schur_perframe(A, P, w, frame_ids, fid):
    """数学恒等的分帧 Schur：M 块对角（帧行互斥）⇒ H_red = Σ_k [A_kᵀW A_k − S_k M_k⁻¹ S_kᵀ]。"""
    H_red = np.zeros((A.shape[1], A.shape[1]))
    parts = []
    for k, f in enumerate(frame_ids):
        sel = fid == np.uint64(f)
        sw = np.sqrt(w[sel])
        Ak = A[sel] * sw[:, None]
        Pk = P[sel] * sw[:, None]
        cols = [c for c in range(P.shape[1]) if np.any(Pk[:, c] != 0.0)]
        if not cols:
            parts.append(Ak.T @ Ak)
            continue
        Pk = Pk[:, cols]
        S = Ak.T @ Pk
        M = Pk.T @ Pk
        parts.append(Ak.T @ Ak - S @ np.linalg.solve(M, S.T))
    for p in parts:
        H_red = H_red + p
    return H_red, parts


def schur_qr(A, P, w):
    """白化 QR 残差 Gram：对每个 δ 块做 Householder/QR，取 A 在 δ 列空间正交补上的投影。"""
    sw = np.sqrt(w)
    Aw = A * sw[:, None]
    Pw = P * sw[:, None]
    q = Pw.shape[1]
    if q == 0:
        return Aw.T @ Aw
    Q, _ = np.linalg.qr(Pw)
    R = Aw - Q @ (Q.T @ Aw)
    return R.T @ R


def projected_ranks(A, P, w, rtol=RANK_RTOL):
    """独立白化 SVD/QR 的投影 GLS 可辨识性判据（对应 p5_projected_gls_reference.projected_gls）。

    · rank(Q_B P)：消去公共场 B_ref 后，逐帧加性场 δ 是否满列秩（q=(K-1)m）；
    · rank(Q_P A)：消去逐帧 δ 后，公共场 B_ref 是否满列秩（n_free）；
    · 增广设计 [A | P] 的奇异谱 ⇒ 有效秩与条件数。
    两个投影秩分别等于列数，等价于"同一连通分量内没有额外规范/观测零模"。
    """
    sw = np.sqrt(w)
    Aw = A * sw[:, None]
    Pw = P * sw[:, None]
    q = Pw.shape[1]
    Qa, _ = np.linalg.qr(Aw)
    Pa = Pw - Qa @ (Qa.T @ Pw)
    sa = np.linalg.svd(Pa, compute_uv=False)
    rank_a = int(np.count_nonzero(sa > rtol * sa[0])) if sa.size else 0
    Qp, _ = np.linalg.qr(Pw)
    Ap = Aw - Qp @ (Qp.T @ Aw)
    sp = np.linalg.svd(Ap, compute_uv=False)
    rank_p = int(np.count_nonzero(sp > rtol * sp[0])) if sp.size else 0
    D = np.hstack([Aw, Pw])
    sd = np.linalg.svd(D, compute_uv=False)
    rank_d = int(np.count_nonzero(sd > rtol * sd[0])) if sd.size else 0
    return {
        "rank_Q_B_P": rank_a, "q_delta": int(q),
        "sigma_Q_B_P_max": float(sa[0]) if sa.size else 0.0,
        "sigma_Q_B_P_min": float(sa[-1]) if sa.size else 0.0,
        "rank_Q_P_A": rank_p, "n_free": int(A.shape[1]),
        "sigma_Q_P_A_max": float(sp[0]) if sp.size else 0.0,
        "sigma_Q_P_A_min": float(sp[-1]) if sp.size else 0.0,
        "augmented_rank": rank_d, "augmented_cols": int(D.shape[1]),
        "augmented_sigma_max": float(sd[0]) if sd.size else 0.0,
        "augmented_sigma_min": float(sd[-1]) if sd.size else 0.0,
        "augmented_kappa": float(sd[0] / sd[-1]) if sd.size and sd[-1] > 0 else float("inf"),
    }


def harmonize_weights(w, fid, frame_ids, scal):
    """光度归一（单位换算，非法定 ridge）：
       模型 y_k = B + δ_k，若帧 k 的数值尺度为 s_k（photscal 或未施加=1），
       则换到公共单位后 ỹ_k = y_k/s_k、ṽ_k = v_k/s_k² ⇒ w_k ← w_k · s_k²。
       设计矩阵 A/P 不变，B、δ_k 同步换单位；这是同一物理模型的合法换算，
       不是正则化，也不改变可辨识性。"""
    wh = w.copy()
    for f in frame_ids:
        sel = fid == np.uint64(f)
        s = scal.get(int(f), 1.0)
        wh[sel] = wh[sel] * (s * s)
    return wh


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", nargs="+", required=True,
                    help="name=path 形式的 p2_samples.json（name 进输出）")
    ap.add_argument("--h", type=float, default=0.553742,
                    help="缺省节点间距（生产失败臂 upper_deg）")
    ap.add_argument("--h-map", default="",
                    help="name=h,... 逐例覆盖（生产 info.adaptive.node_spacing_deg）")
    ap.add_argument("--out", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--photscal", default="",
                    help="frame_id=photscal,... 光度尺度（用于归一臂；缺省 1.0）")
    a = ap.parse_args()

    cases = []
    for spec in a.samples:
        name, _, path = spec.partition("=")
        cases.append((name, path))

    photscal = {}
    if a.photscal:
        for item in a.photscal.split(","):
            k, _, v = item.partition("=")
            photscal[int(k)] = float(v)

    hmap = {}
    if a.h_map:
        for item in a.h_map.split(","):
            k, _, v = item.partition("=")
            hmap[k] = float(v)

    results = {}
    for name, path in cases:
        des = Design(path, hmap.get(name, a.h))
        K = len(des.frame_ids)
        anchor = min(des.frame_ids)  # 生产口径：分量内最小 frame_id
        A, P, w = des.assemble(anchor)
        # 每帧尺度
        per_frame = {}
        for f in des.frame_ids:
            sel = des.fid == np.uint64(f)
            per_frame[str(int(f))] = {
                "n": int(sel.sum()),
                "value_median": float(np.median(des.val[sel])),
                "variance_median": float(np.median(des.var[sel])),
                "weight_median": float(np.median(w[sel])),
                "is_anchor": bool(f == anchor),
            }
        wmed = [v["weight_median"] for v in per_frame.values()]
        scale_span = max(wmed) / min(wmed) if min(wmed) > 0 else float("inf")

        H_joint, H_data = schur_joint(A, P, w)
        H_pf, parts = schur_perframe(A, P, w, des.frame_ids, des.fid)
        H_qr = schur_qr(A, P, w)

        # 归一臂：按各帧真实光度尺度换到公共单位（photscal；未施加=1）
        w_h = harmonize_weights(w, des.fid, des.frame_ids, photscal)
        H_h, _ = schur_joint(A, P, w_h)
        scale_span_h = None
        wmed_h = []
        for f in des.frame_ids:
            sel = des.fid == np.uint64(f)
            wmed_h.append(float(np.median(w_h[sel])))
        scale_span_h = max(wmed_h) / min(wmed_h) if min(wmed_h) > 0 else float("inf")

        lam = RANK_RTOL * float(np.mean(np.diag(H_joint))) if des.n_free else 0.0
        # 惩罚：二阶差分（两个方向），与生产 :2148-2165 同构
        D = np.zeros((des.n_free, des.n_free))
        idx_of = {int(fi): i for i, fi in enumerate(des.full_of_free)}
        for iy in range(des.ny):
            for ix in range(des.nx - 2):
                t = [iy * des.nx + ix, iy * des.nx + ix + 1, iy * des.nx + ix + 2]
                if any(int(x) not in idx_of for x in t):
                    continue
                ii = [idx_of[int(x)] for x in t]
                for aa in range(3):
                    for bb in range(3):
                        D[ii[aa], ii[bb]] += (1.0, -2.0, 1.0)[aa] * (1.0, -2.0, 1.0)[bb]
        for ix in range(des.nx):
            for iy in range(des.ny - 2):
                t = [iy * des.nx + ix, (iy + 1) * des.nx + ix, (iy + 2) * des.nx + ix]
                if any(int(x) not in idx_of for x in t):
                    continue
                ii = [idx_of[int(x)] for x in t]
                for aa in range(3):
                    for bb in range(3):
                        D[ii[aa], ii[bb]] += (1.0, -2.0, 1.0)[aa] * (1.0, -2.0, 1.0)[bb]

        def solve_state(H):
            Hsolve = H + lam * D
            ok, minpiv, failj, _ = chol_pivots(Hsolve)
            return Hsolve, ok, minpiv, failj

        Hs_j, ok_j, piv_j, fail_j = solve_state(H_joint)
        Hs_p, ok_p, piv_p, fail_p = solve_state(H_pf)
        Hs_q, ok_q, piv_q, fail_q = solve_state(H_qr)

        rec = {
            "name": name,
            "samples_path": path,
            "n_obs": des.n,
            "n_masked": des.n_masked,
            "frame_ids": [int(f) for f in des.frame_ids],
            "anchor_frame_id": int(anchor),
            "n_frames": K,
            "h": des.h,
            "spline_degree": des.d,
            "delta_order": des.order,
            "m": des.m,
            "nx": des.nx, "ny": des.ny, "n_full": des.n_full, "n_free": des.n_free,
            "rank_rtol": RANK_RTOL,
            "lambda_eff": lam,
            "per_frame": per_frame,
            "weight_median_span": scale_span,
            "sym_err_joint": sym_err(H_joint),
            "eig_joint": eig_report(H_joint),
            "eig_perframe": eig_report(H_pf),
            "eig_qr": eig_report(H_qr),
            "max_abs_diff_joint_vs_perframe": float(np.max(np.abs(H_joint - H_pf))),
            "rel_diff_joint_vs_perframe": float(np.max(np.abs(H_joint - H_pf))
                                                / max(np.max(np.abs(H_pf)), 1e-300)),
            "chol_joint_ok": bool(ok_j), "chol_joint_min_pivot": piv_j,
            "chol_joint_fail_index": fail_j,
            "chol_perframe_ok": bool(ok_p), "chol_perframe_min_pivot": piv_p,
            "chol_qr_ok": bool(ok_q), "chol_qr_min_pivot": piv_q,
            "H_data_diag_median": float(np.median(np.diag(H_data))),
            "H_red_diag_median_joint": float(np.median(np.diag(H_joint))),
            "schur_cancellation_ratio": float(
                np.linalg.norm(H_data) / max(np.linalg.norm(H_joint), 1e-300)),
            "photscal_used": {str(k): v for k, v in photscal.items()},
            "harmonized_weight_median_span": scale_span_h,
            "eig_harmonized": eig_report(H_h),
            "harmonized_max_abs_diff_vs_joint": float(np.max(np.abs(H_h - H_joint))),
            "projected_ranks_raw": projected_ranks(A, P, w),
            "projected_ranks_harmonized": projected_ranks(A, P, w_h),
        }
        # 锚帧置换研究（同一份数值数据，只改 gauge 锚帧）
        anchor_study = {}
        for f in des.frame_ids:
            Af, Pf, wf = des.assemble(f)
            Hf, _ = schur_joint(Af, Pf, wf)
            Hpf, _ = schur_perframe(Af, Pf, wf, des.frame_ids, des.fid)
            lamf = RANK_RTOL * float(np.mean(np.diag(Hf)))
            okf, pivf, failf, _ = chol_pivots(Hf + lamf * D)
            e = eig_report(Hf)
            anchor_study[str(int(f))] = {
                "chol_joint_ok": bool(okf), "min_pivot": pivf, "fail_index": failf,
                "lambda_min_joint": e["lambda_min"], "lambda_max_joint": e["lambda_max"],
                "n_negative": e["n_negative"],
                "lambda_min_perframe": eig_report(Hpf)["lambda_min"],
            }
        rec["anchor_study"] = anchor_study
        results[name] = rec

        print(f"=== {name}")
        print(f"  n_obs={des.n} frames={K} n_full={des.n_full} n_free={des.n_free} "
              f"nx*ny={des.nx}x{des.ny} h={des.h} anchor={anchor}")
        print(f"  weight_median_span(max/min across frames) = {scale_span:.3e}")
        print(f"  ||H_data||/||H_red_joint|| = {rec['schur_cancellation_ratio']:.3e}")
        print(f"  joint   : lam_min={rec['eig_joint']['lambda_min']:.6e} "
              f"lam_max={rec['eig_joint']['lambda_max']:.6e} "
              f"rank={rec['eig_joint']['effective_rank']}/{des.n_free} "
              f"neg={rec['eig_joint']['n_negative']} chol_ok={ok_j} min_piv={piv_j:.3e}")
        print(f"  perframe: lam_min={rec['eig_perframe']['lambda_min']:.6e} "
              f"chol_ok={ok_p} min_piv={piv_p:.3e}")
        print(f"  qr      : lam_min={rec['eig_qr']['lambda_min']:.6e} "
              f"chol_ok={ok_q} min_piv={piv_q:.3e}")
        pr = rec["projected_ranks_raw"]
        prh = rec["projected_ranks_harmonized"]
        print(f"  projected ranks RAW       : rank(Q_B P)={pr['rank_Q_B_P']}/{pr['q_delta']} "
              f"rank(Q_P A)={pr['rank_Q_P_A']}/{pr['n_free']} "
              f"rank([A|P])={pr['augmented_rank']}/{pr['augmented_cols']} "
              f"sigma_min={pr['augmented_sigma_min']:.3e} kappa={pr['augmented_kappa']:.3e}")
        print(f"  projected ranks NORMALIZED: rank(Q_B P)={prh['rank_Q_B_P']}/{prh['q_delta']} "
              f"rank(Q_P A)={prh['rank_Q_P_A']}/{prh['n_free']} "
              f"rank([A|P])={prh['augmented_rank']}/{prh['augmented_cols']} "
              f"sigma_min={prh['augmented_sigma_min']:.3e} kappa={prh['augmented_kappa']:.3e}")
        print(f"  |H_joint - H_perframe|max = {rec['max_abs_diff_joint_vs_perframe']:.3e} "
              f"(rel {rec['rel_diff_joint_vs_perframe']:.3e})")
        for f in des.frame_ids:
            Af, Pf, wf = des.assemble(f)
            wfh = harmonize_weights(wf, des.fid, des.frame_ids, photscal)
            Hfh, _ = schur_joint(Af, Pf, wfh)
            lamfh = RANK_RTOL * float(np.mean(np.diag(Hfh)))
            okfh, pivfh, failfh, _ = chol_pivots(Hfh + lamfh * D)
            anchor_study[str(int(f))]["harmonized_chol_ok"] = bool(okfh)
            anchor_study[str(int(f))]["harmonized_min_pivot"] = pivfh
            anchor_study[str(int(f))]["harmonized_lambda_min"] = eig_report(Hfh)["lambda_min"]
        rec["anchor_study"] = anchor_study
        Hs_h_ok, Hs_h_piv, Hs_h_fail, _ = chol_pivots(H_h + RANK_RTOL * float(np.mean(np.diag(H_h))) * D)
        rec["harmonized_chol_ok"] = bool(Hs_h_ok)
        rec["harmonized_min_pivot"] = Hs_h_piv
        print(f"  normalized (photscal): weight_span={scale_span_h:.3e} "
              f"lam_min={rec['eig_harmonized']['lambda_min']:.6e} "
              f"rank={rec['eig_harmonized']['effective_rank']}/{des.n_free} "
              f"chol_ok={Hs_h_ok} min_piv={Hs_h_piv:.3e}")
        for f, st in anchor_study.items():
            print(f"    anchor={f}: joint chol_ok={st['chol_joint_ok']} "
                  f"lam_min={st['lambda_min_joint']:.3e} "
                  f"perframe lam_min={st['lambda_min_perframe']:.3e} "
                  f"normalized chol_ok={st.get('harmonized_chol_ok')} "
                  f"lam_min={st.get('harmonized_lambda_min'):.3e}")

    if a.json_out:
        with open(a.json_out, "w") as fh:
            json.dump(results, fh, indent=1, ensure_ascii=False)
        print("wrote", a.json_out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
