#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-SPATIAL-GAIN ③ 真实数据实验: L4-rebuild 49 帧的低阶空间乘法增益.

数据: run/RELEASE-02/L4-rebuild/norm/<tile>/<frame>/
  - p1_flux.json  固定孔径测光 (x, y, flux, flux_error, snr, valid) —— 在 calibrated_*.fts 上
  - p1_wcs.json   TAN+SIP 解算 WCS
  - calibrated_*.fts 像素 (仅像素级复核用)
注意: 这批 L4 产物 photometry_applied=false / photscal=1.0 (FIX-P1 之前),
      所以帧间乘性失配是"未做任何标量校准"的原始失配。

方法 (与生产实现同构):
  对每一帧 k 设空间增益曲面 surf_k(p) = sum_j c_{k,j} phi_j(p_k)   (p_k = 该帧像素坐标)
  观测量: 帧对 (A,B) 的匹配星 i 的 z_i = log10(f_{A,i}/f_{B,i})
        = surf_A(p_{A,i}) - surf_B(p_{B,i}) + noise        (同一天体, 内禀流量相同)
  联合最小二乘 (逆方差权重 + Tukey-IRLS 重加权), gauge: 对每个基函数 j, sum_k c_{k,j}=0
  => 逐帧 m_k(p) = 10^(-(surf_k(p) - mean_surf_k))   (星集合上几何均值 1)
     k_photo_k   = 10^(-mean_surf_k)                 (order=0 时严格退化为标量)

输出: ../data/real_gain.json  (逐帧 m 形态/幅度, before/after 度量, 交叉验证)
"""
import os, sys, json, math, time, argparse, glob
import numpy as np
from scipy.spatial import cKDTree
from scipy.sparse import coo_matrix, vstack
from scipy.sparse.linalg import lsqr

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)                                            # wcs_lib.py
sys.path.insert(0, os.path.abspath(os.path.join(HERE, "..", "..", "..", "synthetic")))  # gainlib.py
from gainlib import basis_terms, mad_sigma, TUKEY_C  # noqa: E402

DATA = os.path.abspath(os.path.join(HERE, "..", "data"))


def _find_root(start):
    """向上找含 ASTROCS_DESIGN.md 的仓库根; 可用 ASTROCS_ROOT 覆盖."""
    env = os.environ.get("ASTROCS_ROOT")
    if env:
        return os.path.abspath(env)
    d = start
    for _ in range(8):
        if os.path.exists(os.path.join(d, "ASTROCS_DESIGN.md")):
            return d
        d = os.path.dirname(d)
    return os.path.abspath(os.path.join(start, "..", "..", "..", ".."))


ROOT = _find_root(HERE)
NORM = os.path.join(ROOT, "run/RELEASE-02/L4-rebuild/norm")
from wcs_lib import Wcs  # noqa: E402

MIN_FLUX = 20000.0        # 只取亮星 (孔径流量 ADU)
MIN_SNR = 50.0
MAX_FLUX_RATIO = 8.0
MATCH_TOL_ARCSEC = 0.7
MIN_PAIR_STARS = 40
NB = 3                    # 逐星 3x3 分箱 (Q3 口径)


# ---------------------------------------------------------------- 载入

def frame_dir(tile, cleaned_file):
    b = os.path.basename(cleaned_file)
    if b.endswith('.fts'):
        b = b[:-4]
    if b.startswith('cleaned_'):
        b = b[len('cleaned_'):]
    return os.path.join(NORM, tile, b.replace('@', '_'))


def parse_flux_file(path):
    """流式解析 p1_flux.json (2 空格缩进), 只保留亮星, 避免 json.load 的 2.8GB 峰值.

    返回 [(file, x, y, flux, ferr, snr), ...] 每帧一项.
    """
    frames = []
    cur = None
    keys = ("x", "y", "flux", "flux_error", "snr", "valid")
    vals = None
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            s = line.strip()
            if not s:
                continue
            ind = len(line) - len(line.lstrip(' '))
            if ind == 6 and s.startswith('"file"'):
                if cur is not None:
                    frames.append((cur, *[np.array(vals[k], float) for k in keys]))
                cur = s.split(':', 1)[1].strip().strip(',').strip('"')
                vals = {k: [] for k in keys}
                continue
            if vals is None or ind != 10:
                continue
            k = s.split(':', 1)[0].strip('"')
            if k not in keys:
                continue
            v = s.split(':', 1)[1].strip().rstrip(',')
            if k == "valid":
                vals[k].append(1.0 if v == "true" else 0.0)
            else:
                try:
                    vals[k].append(float(v))
                except ValueError:
                    vals[k].append(np.nan)
    if cur is not None:
        frames.append((cur, *[np.array(vals[k], float) for k in keys]))
    return frames


class Frame:
    __slots__ = ("tile", "idx", "file", "dir", "wcs", "x", "y", "flux", "ferr", "snr", "ra", "dec", "label")

    def __init__(self, tile, idx, file, x, y, fl, fe, sn, vd):
        self.tile = tile; self.idx = idx; self.file = file
        self.dir = frame_dir(tile, file)
        self.wcs = Wcs(json.load(open(os.path.join(self.dir, 'p1_wcs.json'))))
        keep = (vd > 0.5) & np.isfinite(fl) & (fl > MIN_FLUX) & np.isfinite(fe) & (fe > 0) & (sn > MIN_SNR)
        self.x, self.y, self.flux, self.ferr, self.snr = x[keep], y[keep], fl[keep], fe[keep], sn[keep]
        self.ra, self.dec = self.wcs.pixel_to_sky(self.x, self.y)
        self.label = os.path.basename(self.dir)

    @property
    def sig_dex(self):
        return self.ferr / (math.log(10.0) * self.flux)


def load_all(verbose=True):
    frames = []
    for tile in sorted(os.listdir(NORM)):
        d = os.path.join(NORM, tile)
        if not os.path.isdir(d):
            continue
        t0 = time.time()
        arrs = parse_flux_file(os.path.join(d, "p1_flux.json"))
        for i, (file, x, y, fl, fe, sn, vd) in enumerate(arrs):
            frames.append(Frame(tile, i, file, x, y, fl, fe, sn, vd))
        if verbose:
            print("  %s: %d frames, parse %.1fs" % (tile, len(arrs), time.time() - t0))
            sys.stdout.flush()
    return frames


# ---------------------------------------------------------------- 匹配

def match_pair(A, B):
    cd = math.cos(math.radians(0.5 * (np.median(A.dec) + np.median(B.dec))))
    pa = np.column_stack([A.ra * cd * 3600.0, A.dec * 3600.0])
    pb = np.column_stack([B.ra * cd * 3600.0, B.dec * 3600.0])
    tree = cKDTree(pb)
    d, j = tree.query(pa, k=1)
    ok = d < MATCH_TOL_ARCSEC
    i = np.where(ok)[0]; j = j[ok]
    r = A.flux[i] / B.flux[j]
    good = (r > 1.0 / MAX_FLUX_RATIO) & (r < MAX_FLUX_RATIO)
    return i[good], j[good]


def build_pairs(frames, verbose=True):
    """返回 (pair_list, obs) —— obs 为 (kA, kB, xA, yA, xB, yB, z, w)."""
    pairs = []
    for a in range(len(frames)):
        for b in range(a + 1, len(frames)):
            i, j = match_pair(frames[a], frames[b])
            if len(i) < MIN_PAIR_STARS:
                continue
            A, B = frames[a], frames[b]
            z = np.log10(A.flux[i] / B.flux[j])
            var = A.sig_dex[i] ** 2 + B.sig_dex[j] ** 2
            # 3 轮 MAD 粗剔
            for _ in range(3):
                m = np.median(z); s = 1.482602218505602 * np.median(np.abs(z - m))
                if s <= 0:
                    break
                k = np.abs(z - m) < 4.0 * s
                if k.all():
                    break
                i, j, z, var = i[k], j[k], z[k], var[k]
            if len(i) < MIN_PAIR_STARS:
                continue
            pairs.append(dict(a=a, b=b, n=len(i), tileA=A.tile, tileB=B.tile,
                              labelA=A.label, labelB=B.label,
                              kind=("same-tile" if A.tile == B.tile else
                                    ("cross-tel" if A.tile.split('_')[0] != B.tile.split('_')[0]
                                     else "adjacent-panel")),
                              i=i, j=j))
            if verbose:
                print("  pair %s[%d] x %s[%d]: %d stars, median z=%.4f" %
                      (A.tile, A.idx, B.tile, B.idx, len(i), float(np.median(z))))
    return pairs


# ---------------------------------------------------------------- 联合拟合

def design_terms(order):
    return basis_terms(order)


def _nrm(F, X, Y):
    x0, sx, y0, sy = frame_norm(F)
    return (np.asarray(X, float) - x0) / sx, (np.asarray(Y, float) - y0) / sy


def joint_fit(frames, pairs, order, sel=None, max_iter=12, verbose=True,
              tau_spatial=None, tau_const=None):
    """联合拟合逐帧空间增益曲面.

    sel: dict pair_index -> boolean mask (用于交叉验证半样本)
    tau_spatial/tau_const: Tikhonov 先验尺度 (dex). 联合差分模型只有**相对**空间增益
      可辨识: 同一指向下所有帧共有的像素空间图案在帧间差里抵消, 属近零空间
      (见 report §可辨识性). 生产链每帧独立对 Gaia 拟合, 有绝对锚点, 无此退化;
      本联合拟合作为代理测量必须加先验收缩 (m -> 1), 否则零空间方向被噪声主导.
      先验: 每个空间系数 ~ N(0, tau_spatial^2), 常数项 ~ N(0, tau_const^2).
    返回 coef: [n_frame, 1+nterm]  (gauge: sum_k c_{k,j}=0 for every j)
    """
    terms = design_terms(order)
    nt = 1 + len(terms)
    nf = len(frames)
    nu = nf * nt
    rows = []; cols = []; vals = []; zs = []; ws = []
    r = 0
    pair_row_slices = []
    for pi, p in enumerate(pairs):
        A, B = frames[p['a']], frames[p['b']]
        i, j = p['i'], p['j']
        if sel is not None:
            m = sel[pi]
            i, j = i[m], j[m]
        if len(i) == 0:
            pair_row_slices.append((r, r)); continue
        z = np.log10(A.flux[i] / B.flux[j])
        var = A.sig_dex[i] ** 2 + B.sig_dex[j] ** 2
        w = 1.0 / var
        n = len(i)
        xnA, ynA = _nrm(A, A.x[i], A.y[i])
        xnB, ynB = _nrm(B, B.x[j], B.y[j])
        vA = [np.ones(n)] + [(xnA ** ii) * (ynA ** jj) for _, (ii, jj) in terms]
        vB = [np.ones(n)] + [(xnB ** ii) * (ynB ** jj) for _, (ii, jj) in terms]
        rowidx = np.arange(r, r + n)
        # 列顺序 = [常数项, 各空间项], 与 vA/vB 一致
        for t in range(nt):
            cols.append(np.full(n, p['a'] * nt + t)); vals.append(vA[t]); rows.append(rowidx)
        for t in range(nt):
            cols.append(np.full(n, p['b'] * nt + t)); vals.append(-vB[t]); rows.append(rowidx)
        zs.append(z); ws.append(w)
        pair_row_slices.append((r, r + len(i)))
        r += len(i)
    if r == 0:
        return None, None, None
    A_sp = coo_matrix((np.concatenate(vals), (np.concatenate(rows), np.concatenate(cols))),
                      shape=(r, nu)).tocsr()
    z = np.concatenate(zs); w = np.concatenate(ws)
    # gauge 约束: 对每个基函数 j, sum_k c_{k,j} = 0
    grows = []; gcols = []; gvals = []
    for j in range(nt):
        for k in range(nf):
            grows.append(j); gcols.append(k * nt + j); gvals.append(1.0)
    G = coo_matrix((gvals, (grows, gcols)), shape=(nt, nu)).tocsr()
    # Tikhonov 先验行 (收缩到 m==1; 常数项用很弱的先验)
    if tau_spatial is not None or tau_const is not None:
        rr = []; cc = []; vv = []
        rid = 0
        for k in range(nf):
            for t in range(nt):
                tau = tau_const if t == 0 else tau_spatial
                if tau is None:
                    continue
                rr.append(rid); cc.append(k * nt + t); vv.append(1.0 / tau); rid += 1
        R = coo_matrix((vv, (rr, cc)), shape=(rid, nu)).tocsr()
    else:
        R = None
    lam = 1e4 * math.sqrt(float(np.mean(w)))
    wt = np.sqrt(w)
    n_iter = 0
    for it in range(max_iter):
        n_iter = it + 1
        Aw = A_sp.multiply(wt[:, None]).tocsr()
        zw = z * wt
        blocks = [Aw, G * lam] + ([R] if R is not None else [])
        M = vstack(blocks).tocsr()
        rhs = np.concatenate([zw, np.zeros(nt)] + ([np.zeros(R.shape[0])] if R is not None else []))
        sol = lsqr(M, rhs, atol=1e-12, btol=1e-12, iter_lim=8000)[0]
        resid = A_sp @ sol - z
        s = mad_sigma(resid)
        if s <= 0:
            break
        u = resid / (TUKEY_C * s)
        wt_new = np.sqrt(w) * np.where(np.abs(u) >= 1.0, 0.0, (1.0 - u * u))
        if np.max(np.abs(wt_new - wt)) < 1e-4 * np.max(wt):
            wt = wt_new
            break
        wt = wt_new
    coef = sol.reshape(nf, nt)
    # 逐帧诊断
    resid = A_sp @ sol - z
    info = dict(sigma_dex=mad_sigma(resid), n_obs=int(r), n_iter=n_iter,
                n_outlier=int(np.sum(np.abs(resid) > TUKEY_C * max(mad_sigma(resid), 1e-12))),
                pair_rows=pair_row_slices)
    return coef, info, resid


def frame_norm(F):
    x0 = 0.5 * (F.x.min() + F.x.max()); y0 = 0.5 * (F.y.min() + F.y.max())
    sx = max(0.5 * (F.x.max() - F.x.min()), 1e-9); sy = max(0.5 * (F.y.max() - F.y.min()), 1e-9)
    return x0, sx, y0, sy


def surf_frame(F, coef_row, order):
    terms = design_terms(order)
    x0, sx, y0, sy = frame_norm(F)
    xn = (F.x - x0) / sx; yn = (F.y - y0) / sy
    z = np.full(len(F.x), coef_row[0])
    for t, (_, (ii, jj)) in enumerate(terms):
        z = z + coef_row[1 + t] * (xn ** ii) * (yn ** jj)
    return z


def surf_at(F, coef_row, order, X, Y):
    terms = design_terms(order)
    x0, sx, y0, sy = frame_norm(F)
    xn = (np.asarray(X, float) - x0) / sx; yn = (np.asarray(Y, float) - y0) / sy
    z = np.full(xn.shape, coef_row[0])
    for t, (_, (ii, jj)) in enumerate(terms):
        z = z + coef_row[1 + t] * (xn ** ii) * (yn ** jj)
    return z


# ---------------------------------------------------------------- 度量

def binned_ptp(x, y, z, nb=NB, minn=6):
    """(x,y) 上 nb x nb 分箱中位数的峰峰值 (dex)."""
    if len(x) < nb * nb * minn:
        return float('nan'), None
    xb = np.linspace(x.min(), x.max(), nb + 1)
    yb = np.linspace(y.min(), y.max(), nb + 1)
    xi = np.clip(np.digitize(x, xb) - 1, 0, nb - 1)
    yi = np.clip(np.digitize(y, yb) - 1, 0, nb - 1)
    M = np.full((nb, nb), np.nan)
    for a in range(nb):
        for b in range(nb):
            m = (xi == a) & (yi == b)
            if m.sum() >= minn:
                M[a, b] = np.median(z[m])
    v = M[np.isfinite(M)]
    if v.size < 4:
        return float('nan'), M
    return float(v.max() - v.min()), M


def pair_metrics(frames, p, coef_by_order, order_list, sel=None):
    """对单个帧对计算 before/after 度量 (同一批星, 配对比较)."""
    A, B = frames[p['a']], frames[p['b']]
    i, j = p['i'], p['j']
    if sel is not None:
        i, j = i[sel], j[sel]
    if len(i) < 20:
        return None
    z = np.log10(A.flux[i] / B.flux[j])
    xm = 0.5 * (A.x[i] + B.x[j]); ym = 0.5 * (A.y[i] + B.y[j])
    out = {"n": int(len(i)), "kind": p['kind'], "labelA": p['labelA'], "labelB": p['labelB'],
           "tileA": p['tileA'], "tileB": p['tileB']}
    for o in order_list:
        c = coef_by_order[o]
        model = surf_at(A, c[p['a']], o, A.x[i], A.y[i]) - surf_at(B, c[p['b']], o, B.x[j], B.y[j])
        r = z - model
        # 去掉整体常数 (帧级标量 gauge) 后再量空间结构
        r0 = z - np.median(z)                       # 只用帧级标量
        ptp0, _ = binned_ptp(xm, ym, r0)
        ptp1, _ = binned_ptp(xm, ym, r - np.median(r))
        out["order%d" % o] = dict(
            z_median=float(np.median(z)),
            rms_before_dex=float(np.sqrt(np.mean((r0 - r0.mean()) ** 2))),
            rms_after_dex=float(np.sqrt(np.mean((r - r.mean()) ** 2))),
            ptp3x3_before_pct=float((10 ** ptp0 - 1) * 100) if np.isfinite(ptp0) else None,
            ptp3x3_after_pct=float((10 ** ptp1 - 1) * 100) if np.isfinite(ptp1) else None,
            sigma_after_dex=float(mad_sigma(r)),
        )
    return out


# ---------------------------------------------------------------- 主流程

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--nproc", type=int, default=1)
    args = ap.parse_args()
    t0 = time.time()
    print("[load] frames ..."); sys.stdout.flush()
    frames = load_all()
    print("  %d frames, stars/frame: min=%d med=%d max=%d" % (
        len(frames), min(len(f.x) for f in frames),
        int(np.median([len(f.x) for f in frames])), max(len(f.x) for f in frames)))
    sys.stdout.flush()
    print("[match] pairs ..."); sys.stdout.flush()
    pairs = build_pairs(frames)
    kinds = {}
    for p in pairs:
        kinds[p['kind']] = kinds.get(p['kind'], 0) + 1
    print("  %d pairs %s" % (len(pairs), kinds)); sys.stdout.flush()

    orders = [0, 1, 2]
    coef_by_order = {}; info_by_order = {}
    for o in orders:
        print("[fit] order=%d ..." % o); sys.stdout.flush()
        c, info, resid = joint_fit(frames, pairs, o)
        coef_by_order[o] = c; info_by_order[o] = {k: v for k, v in info.items() if k != 'pair_rows'}
        print("  order=%d sigma=%.4f dex, n_obs=%d, n_iter=%d" % (o, info['sigma_dex'], info['n_obs'], info['n_iter']))
        sys.stdout.flush()

    # 逐帧 m 形态/幅度 (order=1 与 2)
    per_frame = {}
    for k, F in enumerate(frames):
        rec = {"tile": F.tile, "idx": F.idx, "label": F.label, "n_star": int(len(F.x))}
        for o in (1, 2):
            c = coef_by_order[o][k]
            s = surf_frame(F, c, o)
            ms = float(np.mean(s))
            rec["order%d" % o] = dict(
                k_photo=float(10 ** (-ms)),
                m_ptp_pct=float((10 ** float(s.max() - s.min()) - 1) * 100),
                m_rms_pct=float((10 ** float(np.std(s)) - 1) * 100),
                # 线性项 -> 整帧梯度 (%)
                grad_x_pct=float((10 ** float(2.0 * abs(c[1])) - 1) * 100) if o >= 1 else 0.0,
                grad_y_pct=float((10 ** float(2.0 * abs(c[2])) - 1) * 100) if o >= 1 else 0.0,
                coef=[float(v) for v in c],
            )
        per_frame["%s[%d]" % (F.tile, F.idx)] = rec

    # before/after 度量 (全样本 + 半样本交叉验证)
    rng = np.random.default_rng(4242)
    selA = {}; selB = {}
    for pi, p in enumerate(pairs):
        n = len(p['i'])
        m = rng.random(n) < 0.5
        selA[pi] = m; selB[pi] = ~m
    cv = {"fit_half_A": {}, "fit_half_B": {}}
    coefA, _, _ = joint_fit(frames, pairs, 1, sel=selA)
    coefB, _, _ = joint_fit(frames, pairs, 1, sel=selB)
    coef2A, _, _ = joint_fit(frames, pairs, 2, sel=selA)
    coef2B, _, _ = joint_fit(frames, pairs, 2, sel=selB)
    cA = {0: coef_by_order[0], 1: coefA, 2: coef2A}
    cB = {0: coef_by_order[0], 1: coefB, 2: coef2B}

    results = []
    cvres = []
    for pi, p in enumerate(pairs):
        r_all = pair_metrics(frames, p, coef_by_order, orders)
        if r_all:
            results.append(r_all)
        # 交叉验证: 用 A 半拟合的 m 在 B 半上评估 (order 1,2)
        rA = pair_metrics(frames, p, cA, [0, 1, 2], sel=selB[pi])
        rB = pair_metrics(frames, p, cB, [0, 1, 2], sel=selA[pi])
        if rA:
            rA['eval_half'] = 'B'; cvres.append(rA)
        if rB:
            rB['eval_half'] = 'A'; cvres.append(rB)

    def agg(rows, order, key):
        v = [r["order%d" % order][key] for r in rows if r.get("order%d" % order, {}).get(key) is not None]
        v = np.asarray(v, float); v = v[np.isfinite(v)]
        if v.size == 0:
            return None
        return dict(n=int(v.size), median=float(np.median(v)), mean=float(v.mean()),
                    p25=float(np.percentile(v, 25)), p75=float(np.percentile(v, 75)))

    summary = {}
    for kind in ("same-tile", "adjacent-panel", "cross-tel", "ALL"):
        rows = results if kind == "ALL" else [r for r in results if r['kind'] == kind]
        if not rows:
            continue
        s = {"n_pairs": len(rows)}
        for o in orders:
            s["order%d" % o] = {
                "ptp3x3_before_pct": agg(rows, o, "ptp3x3_before_pct"),
                "ptp3x3_after_pct": agg(rows, o, "ptp3x3_after_pct"),
                "rms_before_pct": _pct(agg(rows, o, "rms_before_dex")),
                "rms_after_pct": _pct(agg(rows, o, "rms_after_dex")),
            }
        summary[kind] = s

    cvsum = {}
    for kind in ("same-tile", "adjacent-panel", "cross-tel", "ALL"):
        rows = cvres if kind == "ALL" else [r for r in cvres if r['kind'] == kind]
        if not rows:
            continue
        cvsum[kind] = {"n": len(rows)}
        for o in (1, 2):
            cvsum[kind]["order%d" % o] = {
                "ptp3x3_before_pct": agg(rows, o, "ptp3x3_before_pct"),
                "ptp3x3_after_pct": agg(rows, o, "ptp3x3_after_pct"),
            }

    out = dict(n_frames=len(frames), n_pairs=len(pairs), kinds=kinds,
               fit_info={str(o): info_by_order[o] for o in orders},
               per_frame=per_frame, pair_metrics=results,
               summary=summary, cross_validation=cvsum,
               cv_pair_metrics=cvres, elapsed_s=time.time() - t0,
               params=dict(MIN_FLUX=MIN_FLUX, MIN_SNR=MIN_SNR, MATCH_TOL_ARCSEC=MATCH_TOL_ARCSEC,
                           MIN_PAIR_STARS=MIN_PAIR_STARS, NB=NB))
    os.makedirs(DATA, exist_ok=True)
    with open(os.path.join(DATA, "real_gain.json"), "w") as fh:
        json.dump(out, fh, indent=1)
    print("[done] %.1fs -> %s" % (time.time() - t0, os.path.join(DATA, "real_gain.json")))
    print(json.dumps(summary, indent=1)[:4000])


def _pct(st):
    if st is None:
        return None
    return {k: (float((10 ** v - 1) * 100) if k in ("median", "mean", "p25", "p75") else v)
            for k, v in st.items()}


if __name__ == "__main__":
    main()
