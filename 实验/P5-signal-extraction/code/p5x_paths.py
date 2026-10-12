"""P5 信号提取：四条路径与接缝成因归因。

全部路径都从**还原的逐帧原始数据** `raw_k = corrected_k + δ_k` 出发（δ 为生产联合解给出的
逐帧低阶项 + 残差场），修正只作用在**逐帧项**上，从不减公共分量——这是科学正本
「公共面与覆盖集无关」不变量（记号①）的要求；把公共分量定义成覆盖相关的堆栈会让修正退化为恒等操作。

路径 A 差模对齐      逐帧加性项（偏移 + 平面 + 粗格场）只用帧间**差分**拟合，公共信号在差分里自动消掉
路径 B 接缝驱动 LSQ  目标函数直接取覆盖边界的台阶，逐帧加性基函数线性求解；公共 θ 落在规范零空间
路径 C 天光样点拟合  只用通过结构筛的天光控制样点拟合逐帧平面，结构保护由筛子承担
路径 D 权重设计      修正沿用生产 δ，只改叠加权重（覆盖无关 / 截顶 / 平滑），隔离「权重引发台阶」这一因
归因  掩膜一致性 / 乘性残差 / 加性天光三分
"""

from __future__ import annotations

import numpy as np
from scipy import ndimage, sparse
from scipy.sparse.linalg import lsqr

import p5x_metrics as M

CELL = 64          # 粗格场格边长（像素）≈ 6.4″
TILE = M.TILE      # 瓦片边长（像素）
SUBSAMPLE = 4      # 差分 LSQ 的像素抽样步长


# ------------------------------------------------------------------ 工具
def _cells(prod):
    """并集网格上的粗格编号（瓦片内 (row//CELL, col//CELL)）。"""
    per = TILE // CELL
    return (prod.tile_of * per * per + (prod.row // CELL) * per + (prod.col // CELL)).astype(np.int64)


def _basis_plane(prod):
    """逐帧低阶基（每像素一行，帧无关）：[1, u, v]。"""
    return np.stack([np.ones_like(prod.u), prod.u, prod.v], axis=1)


def _pair_fit(prod, basis, cell=None, w=None, sample=SUBSAMPLE, huber=None):
    """逐帧加性项的最小二乘：只用帧间差分（公共分量在差分里消掉）。

    返回 theta: (nf, nbasis) 逐帧系数；另返回每对帧的差分残差 RMS。
    """
    nf = prod.nframe
    raw = np.nan_to_num(prod.raw)
    valid = prod.valid
    nb = basis.shape[1]
    rows, cols, vals, rhs = [], [], [], []
    r = 0
    rms = {}
    for k in range(nf):
        for l in range(k + 1, nf):
            m = valid[k] & valid[l]
            if sample > 1:
                idxm = np.flatnonzero(m)[::sample]
            else:
                idxm = np.flatnonzero(m)
            if idxm.size == 0:
                continue
            d = raw[k, idxm] - raw[l, idxm]
            B = basis[idxm]                       # (n, nb)
            # 逐像素方程： Σ_j B[p,j](θ_kj − θ_lj) = d
            n = idxm.size
            if w is not None:
                wt = np.sqrt(w[idxm])
                B = B * wt[:, None]
                d = d * wt
            rows.append(np.repeat(np.arange(r, r + n), nb))
            cols.append((k * nb + np.tile(np.arange(nb), n)))
            vals.append(B.reshape(-1))
            rows.append(np.repeat(np.arange(r, r + n), nb))
            cols.append((l * nb + np.tile(np.arange(nb), n)))
            vals.append(-B.reshape(-1))
            rhs.append(d)
            r += n
    A = sparse.csr_matrix((np.concatenate(vals), (np.concatenate(rows), np.concatenate(cols))),
                          shape=(r, nf * nb))
    b = np.concatenate(rhs)
    # 规范：Σ_k θ_k = 0（每列），用加权弱锚实现
    g = np.zeros((nb, nf * nb))
    for j in range(nb):
        for k in range(nf):
            g[j, k * nb + j] = 1.0
    Ag = sparse.vstack([A, sparse.csr_matrix(g * 1e-3)], format="csr")
    bg = np.concatenate([b, np.zeros(nb)])
    th = lsqr(Ag, bg, atol=1e-10, btol=1e-10, iter_lim=2000)[0]
    th = th.reshape(nf, nb)
    return th, rms


# ------------------------------------------------------------------ 路径 A
def path_A_mode_align(prod, use_plane=True, use_cell=True, label="A"):
    """差模对齐：估计逐帧 (偏移 + 平面 + 粗格场)，修正只减这些逐帧项。"""
    basis = _basis_plane(prod) if use_plane else np.ones((prod.npix, 1))
    ncell = int(prod.tile_of.max() + 1) * (TILE // CELL) ** 2
    cell_id = _cells(prod)
    th, _ = _pair_fit(prod, basis)
    off = basis @ th.T                                  # (npix, nf)
    corr = prod.raw - off.T
    info = {"theta": th.tolist(), "basis": "plane" if use_plane else "const"}
    if use_cell:
        mu_common = np.nanmean(np.where(prod.valid, corr, np.nan), axis=0)
        resid = corr - np.nan_to_num(mu_common)[None, :]
        # 逐帧残差 → 逐格均值 → 跨帧去均值（等价于只保留差模）
        m = np.zeros((prod.nframe, ncell))
        cnt = np.zeros((prod.nframe, ncell))
        for k in range(prod.nframe):
            idx = cell_id[prod.valid[k]]
            np.add.at(m[k], idx, resid[k][prod.valid[k]])
            np.add.at(cnt[k], idx, 1.0)
        with np.errstate(invalid="ignore"):
            m = np.where(cnt > 0, m / np.maximum(cnt, 1), 0.0)
        for it in range(4):
            tot = np.where(cnt > 0, m, 0.0).sum(axis=0)
            n = (cnt > 0).sum(axis=0)
            mu = np.where(n > 0, tot / np.maximum(n, 1), 0.0)
            m = np.where(cnt > 0, m - mu[None, :], 0.0)
        corr = corr - m[:, cell_id]
        info["cell_field_rms"] = float(np.sqrt(np.mean(m ** 2)))
    return corr, info


# ------------------------------------------------------------------ 路径 B
def path_B_seam_lsq(prod, p, q, lam_smooth=0.3, damp=1e-3, cell=CELL, label="B"):
    """接缝驱动最小二乘：直接用覆盖边界台阶解**逐帧逐格电平**。

    一像素宽的边界只约束「阶」不约束「斜率」，故基函数取**逐格常数**（格边长 `cell` px），
    并对格点场加 Laplacian 平滑与整体阻尼；规范按每格跨帧均值置零，公共分量落在零空间。

        stack(p) = S_raw(p) − g(p)·Θ(p)，  Θ(p) = Σ_k w_k(p)θ_k / W(p)
        step_e   = d_e − Σ_k [ (w_k(p)/W_p) g_k(p) − (w_k(q)/W_q) g_k(q) ]·θ_k
    其中 g_k 是第 k 帧在该像素所属格的指示基（逐帧独立一套格点场）。
    """
    nf = prod.nframe
    per = TILE // cell
    ncell = len(prod.tiles) * per * per
    cid = prod.tile_of * per * per + (prod.row // cell) * per + (prod.col // cell)
    w = np.where(prod.valid, 1.0 / np.maximum(prod.var, 1e-300), 0.0)
    W = np.where(w.sum(axis=0) > 0, w.sum(axis=0), 1.0)
    s_raw, _ = M.stack_weighted(prod.raw, prod)
    npar = nf * ncell
    # 边界台阶行
    cp, cq = cid[p], cid[q]
    rr = np.repeat(np.arange(p.size), 2)
    R, C, V = [], [], []
    for k in range(nf):
        # 目标：Σ_k [(w_k(p)/W_p)θ_{k,cp} − (w_k(q)/W_q)θ_{k,cq}] = step_raw
        vp = (w[k, p] / W[p])
        vq = -(w[k, q] / W[q])
        cc = np.stack([vp, vq], axis=1)
        R.append(rr)
        C.append(np.stack([k * ncell + cp, k * ncell + cq], axis=1).reshape(-1))
        V.append(cc.reshape(-1))
    A = sparse.csr_matrix((np.concatenate(V), (np.concatenate(R), np.concatenate(C))),
                          shape=(p.size, npar))
    b = s_raw[p] - s_raw[q]
    blocks_A, blocks_b = [A], [b]
    # 规范：每格跨帧均值 = 0
    if ncell <= 20000:
        gi, gj = [], []
        for c in range(ncell):
            for k in range(nf):
                gi.append(c); gj.append(k * ncell + c)
        G = sparse.csr_matrix((np.ones(len(gi)), (gi, gj)), shape=(ncell, npar))
        blocks_A.append(G * 1e-2); blocks_b.append(np.zeros(ncell))
    # Laplacian 平滑（同瓦片内 4 邻格）+ 阻尼
    if lam_smooth > 0:
        rl, cl, vl = [], [], []
        r = 0
        for k in range(nf):
            base = k * ncell
            for t in range(len(prod.tiles)):
                for i in range(per):
                    for j in range(per):
                        c0 = t * per * per + i * per + j
                        for di, dj in ((0, 1), (1, 0)):
                            i2, j2 = i + di, j + dj
                            if i2 >= per or j2 >= per:
                                continue
                            c1 = t * per * per + i2 * per + j2
                            rl += [r, r]; cl += [base + c0, base + c1]; vl += [1.0, -1.0]
                            r += 1
        L = sparse.csr_matrix((np.array(vl), (np.array(rl), np.array(cl))), shape=(r, npar))
        blocks_A.append(L * lam_smooth); blocks_b.append(np.zeros(r))
    A = sparse.vstack(blocks_A, format="csr")
    b = np.concatenate(blocks_b)
    th = lsqr(A, b, damp=damp, atol=1e-12, btol=1e-12, iter_lim=6000)[0]
    field = (th.reshape(nf, ncell))[:, cid]
    corr = prod.raw - field
    s1, _ = M.stack_weighted(corr, prod)
    def _med(s):
        d = np.abs(s[p] - s[q])
        return float(np.median(d)) if d.size else float("nan")
    info = {"ncell": int(ncell), "cell_px": int(cell), "lam_smooth": lam_smooth, "damp": damp,
            "seam_abs_median_before": _med(s_raw), "seam_abs_median_after": _med(s1),
            "field_rms": float(np.sqrt(np.nanmean(field ** 2)))}
    return corr, info


# ------------------------------------------------------------------ 路径 C
def path_C_sky_samples(prod, structure, huber=1.345, iters=6, label="C"):
    """只用通过结构筛的天光控制样点拟合逐帧平面（结构保护由筛子承担）。"""
    sp = prod.sp
    s = prod.samples
    obs = s["observations"]
    fid = {int(f): i for i, f in enumerate(prod.frame_ids)}
    k = np.array([fid[int(o["frame_id"])] for o in obs])
    from p5x_io import gnomonic
    u, v = gnomonic(sp["ra0_deg"], sp["dec0_deg"],
                    np.array([o["ra_deg"] for o in obs]), np.array([o["dec_deg"] for o in obs]))
    val = np.array([o["value"] for o in obs], dtype=float)
    wv = np.array([o["control_ivar"] for o in obs], dtype=float)
    # 结构筛：控制点位置落在结构掩膜内即剔除（结构掩膜由未校正堆栈冻结）
    pk = np.empty(val.size, dtype=np.int64)
    # 最近叶像素：把样点 ra/dec 投到 grid 上再匹配
    import healpy as hp
    leaf = hp.ang2pix(1 << 21,
                      np.array([o["ra_deg"] for o in obs]),
                      np.array([o["dec_deg"] for o in obs]), nest=True, lonlat=True)
    order = np.argsort(prod.leaf_nest)
    pos = np.searchsorted(prod.leaf_nest[order], leaf)
    pos = np.clip(pos, 0, prod.leaf_nest.size - 1)
    good = prod.leaf_nest[order][pos] == leaf
    pk[good] = order[pos[good]]
    pk[~good] = -1
    in_struct = np.zeros(val.size, dtype=bool)
    sel = pk >= 0
    in_struct[sel] = structure[pk[sel]]
    keep = (~in_struct) & (val > 0)
    # 逐帧平面拟合 + Huber 重加权
    th = np.zeros((prod.nframe, 3))
    B = np.stack([np.ones_like(u), u, v], axis=1)
    for kk in range(prod.nframe):
        m = (k == kk) & keep
        if m.sum() < 8:
            continue
        Bm = B[m]
        ym = val[m]
        wm = wv[m]
        t = np.zeros(3)
        for it in range(iters):
            r = ym - Bm @ t
            s_ = 1.482602218505602 * np.median(np.abs(r - np.median(r)))
            s_ = max(s_, 1e-30)
            z = r / s_
            hw = np.where(np.abs(z) <= huber, 1.0, huber / np.maximum(np.abs(z), 1e-30))
            wgt = wm * hw
            A = Bm.T @ (wgt[:, None] * Bm)
            t = np.linalg.solve(A + 1e-12 * np.eye(3) * np.trace(A) / 3, Bm.T @ (wgt * ym))
        th[kk] = t
    # 规范：公共分量（逐帧均值）不得整体平移 ⇒ 逐帧常数的覆盖加权均值为零
    wnum = np.array([wv[(k == kk) & keep].sum() for kk in range(prod.nframe)])
    c0 = th[:, 0]
    mu = np.sum(wnum * c0) / max(wnum.sum(), 1e-300)
    th[:, 0] -= mu
    delta = np.zeros_like(prod.raw)
    Bfull = _basis_plane(prod)
    for kk in range(prod.nframe):
        delta[kk] = Bfull @ th[kk]
    corr = prod.raw - delta
    info = {"theta": th.tolist(), "n_samples": int(val.size), "n_used": int(keep.sum()),
            "n_struct_rejected": int(in_struct.sum())}
    return corr, info


# ------------------------------------------------------------------ 路径 D
def path_D_weights(prod):
    """修正沿用生产 δ（即 corrected 就是修正后的帧），只改叠加权重。"""
    w0 = 1.0 / np.maximum(prod.var, 1e-300)
    w0 = np.where(prod.valid, w0, 0.0)
    out = {}
    out["ivar"] = w0
    # 覆盖无关：逐帧常数权重（取该帧权重中位数）
    wc = np.zeros_like(w0)
    for k in range(prod.nframe):
        m = prod.valid[k]
        wc[k] = np.where(m, np.median(w0[k][m]), 0.0)
    out["frame_const"] = wc
    # 截顶：限制权重跳变
    cap = np.nanpercentile(w0[prod.valid], 90)
    out["capped_p90"] = np.where(prod.valid, np.minimum(w0, cap), 0.0)
    # 平滑：权重在瓦片内做小核平滑（抹掉边界附近的权重突变）
    ws = np.empty_like(w0)
    arr = w0.reshape(prod.nframe, len(prod.tiles), 512, 512)
    for k in range(prod.nframe):
        a = arr[k]
        num = ndimage.uniform_filter(np.nan_to_num(a), size=9)
        cnt = ndimage.uniform_filter((a > 0).astype(float), size=9)
        ws[k] = np.where(cnt > 0, num / np.maximum(cnt, 1e-30), 0.0).reshape(-1)
    out["smoothed9"] = ws
    return out
