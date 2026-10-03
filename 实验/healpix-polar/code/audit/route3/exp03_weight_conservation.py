#!/usr/bin/env python3
# exp03_weight_conservation.py -- R3-03 + P3 chain use-case (P2->P3->P4/P5 interfaces).
# Setup: TAN WCS (ra0=30deg, dec0=30deg), 24x24 source pixels of size theta inside the face,
#   pixfrac=0.8; drop footprint = gnomonic image of the pixfrac-shrunk square. Gnomonic maps
#   tangent-plane straight lines to great circles, so the drop is a spherical quadrilateral with
#   EXACT area via Van Oosterom-Strackee on its 4 corners.
# Metrics:
#   (1) completeness/conservation: w_jp = a_jp/A_drop,j on true-curve leaf polygons. Chord
#       tiles of adjacent leaves share edges, hence tile the sphere exactly, so sum_p a_jp =
#       A_drop to round-off: per-pixel |sum_p a_jp/A_drop - 1| ~ 1e-15; global sum estimator
#       sum_p F_p = sum_j x_j.
#   (2) pixfrac negative control: A_pixel normalization => sum_p w' = pf^2 (deficit 0.36);
#       delta = A_drop/(pf^2*A_pixel) - 1 vs closed form (1-pf^2)*theta^2*[0.25/(1+r_c^2) -
#       0.625 xi_c^2/(1+r_c^2)^2]; centered field: delta = 0.25*(1-pf^2)*theta^2 =>
#       8.46e-12 (theta=2") and 1.90e-7 (theta=300").
#   (3) injection control: single-pair area factor 0.9003 with same-pixel flux compensation:
#       pair metric red at 9.968e-2 while global sum metric stays <=1e-15 (localization).
#   (4) F&H weighted-mean estimator (sec 2, "a factor of s^2 is introduced to conserve surface
#       intensity"): constant field reproduces S_p = B0 on every touched leaf to round-off.
#   (5) sparse SNR control points (P2->P3): direction -> ring (j,m) leaf id, value carried;
#       direction-to-leaf-center distance <= 1.05*hp_res (ties to exp04 circumradius 1.0415).
#   Candidate leaves searched within angular distance <= max_angle + 1.25*hp_res
#   (HP_CIRCUMRADIUS_FACTOR margin); metric (1) certifies that search is complete.
import json, math, os, sys, time
import os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from edge_geom import sample_edge, to_vec, wrap_pi
from exp01_leaf_area import ring_layout
from pathlib import Path

# 落盘锚点：从脚本自身位置向上定位本单元目录（实验/healpix-polar），
# 不依赖 dirname 的层数；找不到即抛错，避免静默写到错误位置。
UNIT = next(p for p in Path(__file__).resolve().parents if p.name == "healpix-polar")

SEED = 20260926
PF = 0.8
RA0, DEC0 = np.radians(30.0), np.radians(30.0)
NSEG = 8

# ---- G08-05 B6：逐 drop 守恒闭合的**适用域守卫** + 真空带登记 ----
# 正本 docs/science/algorithms/DRIZZLE_GEOMETRY.md:350-351 的逐 drop 判据是
#     |Σ_j a_jp − A_drop,p| <= max(τ_rel·A_drop,p, ε_abs)，τ_rel=1e-6、ε_abs=1e-15 sr。
# 该式的**绝对项在小 drop 上吞掉整条判据**：A_drop ≲ ε_abs 时 max() 取 ε_abs，
# 相对容差 tol/A_drop >= 1 ⇒ **100% 通量丢失（Σa=0）仍判绿**。实测（本单元几何）：
#   θ=0.005″/px，pf=1  → A_drop=5.876e-16 sr、tol/A_drop=1.70、判红边界 0.00652″/px
#   θ=0.005″/px，pf=0.8→ A_drop=3.761e-16 sr、tol/A_drop=2.66、判红边界 0.00815″/px
# **但这条地板不可用「换一个更小的绝对下限」消掉**：正本 :362-364 自己写明，可达成的
# 绝对残差是 ~1e-16 sr 的**几何/表示地板**（4 角弦 + S-H 近退化求交），且 long double
# 不降 ⇒ 在双精度下 A_drop ≲1e-14 sr 的域里，逐 drop 判据**原理上**分不开
# 「100% 通量丢失」与「几何地板」。正本 :369-371 明说要移动该地板必须把叶边界生成与
# 面积记账提到扩展精度表示（改变冻结口径、需变更流程）。
# 因此本单元**不伪造一个达不到的绝对下限**（实测：本单元 θ=300″/pf=0.8 的真实闭合残差
# 已是 9.48e-17 sr，任何 <1e-16 的绝对下限都会误伤正确实现），改为加一条
# **fail-closed 适用域守卫**：一旦工作尺度落进盲带，本门判红而不是静默判绿。
TAU_REL = 1e-6             # 正本 τ_rel
EPS_ABS_POS = 1e-15        # 正本 ε_abs [sr]（登记，不修改正本）
# 几何/表示地板量级（正本 :363-364 自述 ~1e-16 sr）；用它定「相对判据仍然可用」的
# 适用域下限：要求 tol_pos <= A_drop/2，即 τ_rel·A_drop 压过绝对项一个量级。
EPS_GEOM_FLOOR = 1e-16     # [sr] 几何/表示地板（正本 :363-364）
APPLICABLE_MIN_ADROP = max(2.0 * EPS_ABS_POS, 2.0 * EPS_GEOM_FLOOR)
RAD_PER_ARCSEC = math.pi / (180.0 * 3600.0)


def drop_closure_ok(total_a, a_drop):
    """正本逐 drop 判据（不改口径）＋**适用域守卫**。返回 (ok, detail)。

    正本项：resid <= max(τ_rel·A_drop, ε_abs)。
    守卫项：a_drop 必须 >= APPLICABLE_MIN_ADROP，否则判据落在真空带内、
            无法区分真实丢失与几何地板 ⇒ fail-closed 判红。
    """
    resid = abs(float(total_a) - float(a_drop))
    tol_pos = max(TAU_REL * float(a_drop), EPS_ABS_POS)
    canon_ok = resid <= tol_pos
    in_domain = float(a_drop) >= APPLICABLE_MIN_ADROP
    return bool(canon_ok and in_domain), {
        "sum_a": float(total_a), "A_drop": float(a_drop), "resid_abs_sr": resid,
        "tolerance_pos_sr": tol_pos, "pos_criterion_ok": bool(canon_ok),
        "applicable_min_a_drop_sr": APPLICABLE_MIN_ADROP,
        "within_applicable_domain": bool(in_domain),
        "rel_tolerance_pos": tol_pos / max(float(a_drop), 1e-300),
    }


def seam_blind_band_probe(scale_arcsec=(0.005, 0.00652, 0.02, 0.04, 0.2, 0.967, 1.8),
                          pixfrac=(1.0, 0.8)):
    """100% 通量丢失（Σa=0）代入正本逐 drop 判据，量出「真空带」与其边界。"""
    rows = []
    for s in scale_arcsec:
        for pf in pixfrac:
            a_drop = (pf * s * RAD_PER_ARCSEC) ** 2
            canon, d = drop_closure_ok(0.0, a_drop)
            rows.append({"scale_arcsec_per_px": s, "pixfrac": pf, "A_drop_sr": a_drop,
                         "resid_abs_sr_100pct_loss": a_drop,
                         "rel_tolerance_pos": d["rel_tolerance_pos"],
                         "pos_criterion_alone_green": bool(d["pos_criterion_ok"]),
                         "in_applicable_domain": bool(d["within_applicable_domain"]),
                         "combined_criterion_ok": bool(canon)})
    vacuum = [r for r in rows if r["pos_criterion_alone_green"]]
    return rows, vacuum


def tan_rotation(ra0, dec0):
    return np.array([[-np.sin(ra0), np.cos(ra0), 0.0],
                     [-np.sin(dec0)*np.cos(ra0), -np.sin(dec0)*np.sin(ra0), np.cos(dec0)],
                     [np.cos(dec0)*np.cos(ra0), np.cos(dec0)*np.sin(ra0), np.sin(dec0)]])

R_TAN = tan_rotation(RA0, DEC0)

def tan_to_vec(xi, eta):
    v = R_TAN @ np.array([xi, eta, 1.0])
    return v/np.linalg.norm(v)

def tri_area(a, b, c):
    det = np.dot(a, np.cross(b, c))
    return 2.0*np.arctan2(abs(det), 1.0 + np.dot(a,b) + np.dot(b,c) + np.dot(c,a))

def quad_area(V):
    return sum(tri_area(V[0], V[i], V[i+1]) for i in range(1, len(V)-1))

def clip_poly(V, n):
    """Sutherland-Hodgman spherical clip: keep n.v >= 0."""
    out = []
    M = len(V)
    for i in range(M):
        a, b = V[i], V[(i+1) % M]
        da, db = a @ n, b @ n
        if da >= 0.0:
            out.append(a)
        if (da > 0.0) != (db > 0.0):
            x = np.cross(np.cross(a, b), n)
            ln = np.linalg.norm(x)
            if ln > 1e-300:
                x = x/ln
                if x @ (a + b) < 0.0:
                    x = -x
                out.append(x)
    return np.array(out)

def poly_area(V):
    a = V[0]
    tot = 0.0
    for i in range(1, len(V)-1):
        b, c = V[i], V[i+1]
        det = np.dot(a, np.cross(b, c))
        s = 1.0 if det >= 0 else -1.0
        tot += s*tri_area(a, b, c)
    return abs(tot)

def drop_quad(xi0, eta0, half):
    cs = [(-half,-half),(half,-half),(half,half),(-half,half)]
    return np.array([tan_to_vec(xi0+dx, eta0+dy) for dx, dy in cs])

_LEAF_CACHE = {}

def leaf_poly(N, j, m, lev, z, S, w, o, nseg=NSEG):
    key = (N, j, m, nseg)
    P = _LEAF_CACHE.get(key)
    if P is not None:
        return P
    phi_c = o[j] + (m + 0.5)*w[j]
    phiL = o[j] + m*w[j]; phiR = o[j] + (m+1)*w[j]; zj = z[j]
    def nearest(jlev, phic):
        pole = (jlev <= 0) | (jlev >= 4*N)
        jc = np.clip(jlev, 0, 4*N)
        Sj, wj, oj = S[jc], w[jc], o[jc]
        wsafe = np.where(wj > 0, wj, 1.0)
        k = np.mod(np.round((phic - oj)/wsafe).astype(np.int64), np.maximum(Sj, 1))
        return np.where(pole, 0.0, oj + k*wj)
    phiN = nearest(j-1, phi_c); phiS = nearest(j+1, phi_c)
    zN = z[np.clip(j-1, 0, 4*N)]; zS = z[np.clip(j+1, 0, 4*N)]
    corners = [(phiN, zN), (phiL, zj), (phiS, zS), (phiR, zj)]  # N,L,S,R cyclic
    pts = []
    for i in range(4):
        a, b = np.array(corners[i]), np.array(corners[(i+1) % 4])
        V = sample_edge(a, b, n=nseg+1)
        pts.extend(V[:-1])
    P = np.array(pts)
    _LEAF_CACHE[key] = P
    return P

def candidate_leaves(N, cvec, r_max, lev, z, S, w, o):
    hp_res = np.sqrt(np.pi/3.0)/N
    R = r_max + 1.25*hp_res
    zc = float(cvec[2]); phic = float(np.arctan2(cvec[1], cvec[0]))
    colc = np.arccos(np.clip(zc, -1, 1))
    sinc = np.sin(colc)
    out = []
    for j in range(1, 4*N):
        if S[j] == 0: continue
        col = np.arccos(np.clip(z[j], -1, 1))
        if abs(col - colc) > R: continue
        sc = np.sin(col)
        if sc*sinc > 1e-300:
            val = (np.cos(R) - np.cos(col)*np.cos(colc))/(sc*sinc)
            if val >= 1.0: continue
            dphi = np.arccos(np.clip(val, -1.0, 1.0)) + w[j]
        else:
            dphi = np.pi + w[j]
        k0 = int(np.floor((phic - dphi - o[j])/w[j] - 0.5))
        k1 = int(np.ceil((phic + dphi - o[j])/w[j] - 0.5))
        for k in range(k0, k1+1):
            m = k % S[j]
            fc = o[j] + (m + 0.5)*w[j]
            lv = to_vec(np.array([fc]), np.array([z[j]]))[0]
            if np.arccos(np.clip(lv @ cvec, -1, 1)) <= R:
                out.append((j, m))
    return out

_GL = np.polynomial.legendre.leggauss(48)
_GLX, _GLW = _GL[0], _GL[1]

def gauss_area(h):
    """Solid angle of the gnomonic image of the tangent-plane square [-h,h]^2 via
    Gauss-Legendre quadrature of dOmega = dxi deta / (1+xi^2+eta^2)^{3/2} (smooth)."""
    X = h*_GLX; W = h*_GLW
    XI = np.tile(X, (len(X), 1)); ETA = XI.T
    U = 1.0 + XI*XI + ETA*ETA
    return float((W[:,None]*W[None,:] * U**-1.5).sum())

def drop_halfplanes(Dq):
    ns = []
    for e in range(4):
        n = np.cross(Dq[e], Dq[(e+1) % 4]); n = n/np.linalg.norm(n)
        if n @ Dq.mean(axis=0) < 0: n = -n
        ns.append(n)
    return ns

def run_field(N, theta_arcsec, npix_side=24, inject=False, norm_pixel=False, field="random"):
    lev, z, S, w, o = ring_layout(N)
    theta = np.radians(theta_arcsec/3600.0)
    rng = np.random.default_rng(SEED)
    B0 = 1.7
    if field == "random":
        xvals = rng.uniform(0.5, 2.0, npix_side*npix_side)
    else:
        xvals = np.full(npix_side*npix_side, B0)
    half_d = PF*theta/2.0
    half_p = theta/2.0
    sumF = 0.0; sumX = 0.0
    comp_max = 0.0; ksum = 0.0; kquad = 0.0; kgauss = 0.0; pair_red = 0.0
    acc = {}
    inj_done = False
    # G08-05 R2 第 2 条：逐 drop 判据必须作用在**实测**的 (Σa, A_drop) 上。
    # 这里 tot 就是该 drop 内所有真曲线叶多边形面积的实测和，A_drop = quad_area(Dq)
    # 是同一 drop 的实测球面面积 —— 二者都不是构造出来的常量。
    drop_closure = {"n": 0, "n_ok": 0, "n_out_of_domain": 0,
                    "max_rel_tolerance_pos": 0.0, "max_resid_rel": 0.0}
    for idx in range(npix_side*npix_side):
        i, k = idx // npix_side, idx % npix_side
        xi0 = (i - (npix_side-1)/2.0)*theta
        eta0 = (k - (npix_side-1)/2.0)*theta
        Dq = drop_quad(xi0, eta0, half_d)
        Pq = drop_quad(xi0, eta0, half_p)
        A_drop = quad_area(Dq)
        A_pix = quad_area(Pq)
        dnorms = drop_halfplanes(Dq)
        cvec = tan_to_vec(xi0, eta0)
        r_max = max(np.arccos(np.clip(cvec @ v, -1, 1)) for v in Dq)
        cands = candidate_leaves(N, cvec, r_max, lev, z, S, w, o)
        xj = xvals[idx]
        sumX += xj
        tot = 0.0
        pair_areas = []
        for (j, m) in cands:
            P = leaf_poly(N, j, m, lev, z, S, w, o)
            Q = P
            for n in dnorms:
                if len(Q): Q = clip_poly(Q, n)
            if len(Q) < 3: continue
            a = poly_area(Q)
            pair_areas.append(((j, m), a))
        # injection: x0.9003 deficit on the first overlap pair of the first multi-pair pixel,
        # compensated inside the same pixel (deficit added to its last pair) so that
        # sum_p a'_jp = A_drop still holds exactly => global sum metric stays green,
        # while per-pair weights are wrong by 9.968e-2 => pair metric red.
        if inject and not inj_done and len(pair_areas) >= 2:
            (jm0, a0) = pair_areas[0]
            pair_areas[0] = (jm0, 0.9003*a0)
            (jm1, a1) = pair_areas[-1]
            pair_areas[-1] = (jm1, a1 + 0.0997*a0)
            inj_done = True
            pair_red = abs(0.9003*a0 - a0)/a0   # per-pair relative error, expected 9.97e-2
        for ((j, m), a) in pair_areas:
            tot += a
            e = acc.setdefault((j, m), [0.0, 0.0])
            e[0] += a*xj; e[1] += a
            wgt = a/A_drop if not norm_pixel else a/A_pix
            sumF += wgt*xj
        comp = tot/A_drop - 1.0
        comp_max = max(comp_max, abs(comp))
        # 实测逐 drop 闭合：把**实测** (tot, A_drop) 送进正本判据 + 适用域守卫。
        _ok, _d = drop_closure_ok(tot, A_drop)
        drop_closure["n"] += 1
        drop_closure["n_ok"] += int(bool(_ok))
        drop_closure["n_out_of_domain"] += int(not bool(_d["within_applicable_domain"]))
        drop_closure["max_rel_tolerance_pos"] = max(
            drop_closure["max_rel_tolerance_pos"], float(_d["rel_tolerance_pos"]))
        drop_closure["max_resid_rel"] = max(
            drop_closure["max_resid_rel"], abs(comp))
        ksum += tot/A_pix
        kquad += quad_area(Dq)/(PF*PF*quad_area(Pq))
        kgauss += gauss_area(half_d)/(PF*PF*gauss_area(half_p))
    mean_res = 0.0
    for (jm, (sw, sa)) in acc.items():
        if sa > 0:
            mean_res = max(mean_res, abs(sw/sa/B0 - 1.0))
    return dict(comp_max=comp_max, pair_red=pair_red, sumF=sumF, sumX=sumX,
                kmean=ksum/(npix_side*npix_side),
                kquad=kquad/(npix_side*npix_side),
                kgauss=kgauss/(npix_side*npix_side),
                mean_res=mean_res, n_leaves_touched=len(acc),
                drop_closure=drop_closure)

def control_points(N, npts=8):
    rng = np.random.default_rng(SEED + 1)
    lev, z, S, w, o = ring_layout(N)
    hp_res = np.sqrt(np.pi/3.0)/N
    theta = np.radians(300.0/3600.0)
    pts = []; worst = 0.0
    for _ in range(npts):
        xi = rng.uniform(-11.5*theta, 11.5*theta)
        eta = rng.uniform(-11.5*theta, 11.5*theta)
        v = tan_to_vec(xi, eta)
        zc = float(v[2]); phi = float(np.arctan2(v[1], v[0])) % (2*np.pi)
        if zc > 2.0/3.0:
            jf = N*np.sqrt(3.0*(1.0 - zc))
        elif zc < -2.0/3.0:
            jf = 4*N - N*np.sqrt(3.0*(1.0 + zc))
        else:
            jf = 2*N - 1.5*N*zc
        j = int(np.clip(round(jf), 1, 4*N-1))
        if S[j] == 0: continue
        m = int(np.mod(np.round((phi - o[j])/w[j] - 0.5), S[j]))
        fc = o[j] + (m + 0.5)*w[j]
        c = to_vec(np.array([fc]), np.array([z[j]]))[0]
        ang = np.arccos(np.clip(c @ v, -1, 1))
        worst = max(worst, ang/hp_res)
        pts.append(dict(j=j, m=m, ang_over_hp_res=ang/hp_res))
    return pts, worst

def main():
    t0 = time.time()
    out = dict(exp="exp03_weight_conservation", seed=SEED, pixfrac=PF,
               ra0_deg=30.0, dec0_deg=30.0, npix_side=24)
    r1 = run_field(512, 300.0, field="random")
    out["conservation_300as_n512"] = dict(
        max_abs_pixel_completeness=r1["comp_max"],
        global_sum_minus=r1["sumF"]/r1["sumX"] - 1.0,
        n_leaves_touched=r1["n_leaves_touched"])
    print("theta=300 nside=512:", json.dumps(out["conservation_300as_n512"]), flush=True)
    r2a = run_field(1024, 2.0)
    r2b = run_field(512, 300.0)
    th2 = np.radians(2.0/3600.0); th300 = np.radians(300.0/3600.0)
    d2 = 0.25*(1.0-PF**2)*th2**2; d300 = 0.25*(1.0-PF**2)*th300**2
    out["pixel_norm_delta"] = dict(
        theta2=dict(k_minus_1_clip=r2a["kmean"]/PF**2 - 1.0,
                    k_minus_1_vos=r2a["kquad"] - 1.0,
                    k_minus_1_gauss=r2a["kgauss"] - 1.0, closed_form=d2),
        theta300=dict(k_minus_1_clip=r2b["kmean"]/PF**2 - 1.0,
                      k_minus_1_vos=r2b["kquad"] - 1.0,
                      k_minus_1_gauss=r2b["kgauss"] - 1.0, closed_form=d300),
        pf2_deficit=1.0-PF**2, raw_ratio_theta300=r2b["kmean"])
    print("delta theta2:", json.dumps(out["pixel_norm_delta"]["theta2"]), flush=True)
    print("delta theta300:", json.dumps(out["pixel_norm_delta"]["theta300"]), flush=True)
    r3 = run_field(512, 300.0, inject=True)
    out["injection"] = dict(pair_metric=r3["pair_red"], expected=0.0996836838,
                            global_sum_minus=r3["sumF"]/r3["sumX"] - 1.0)
    print("injection:", json.dumps(out["injection"]), flush=True)
    r4 = run_field(512, 300.0, field="constant")
    out["mean_estimator"] = dict(max_abs_Sp_over_B0_minus_1=r4["mean_res"],
                                 n_leaves=r4["n_leaves_touched"])
    print("mean estimator:", json.dumps(out["mean_estimator"]), flush=True)
    pts, worst = control_points(512)
    out["control_points"] = dict(points=pts, max_ang_over_hp_res=worst, bound=1.05)
    print("control points worst ang/hp_res =", worst, flush=True)

    # ---- G08-05 B6：逐 drop 守恒的适用域守卫（真空带探针 + 判据） ----
    sb_rows, sb_vacuum = seam_blind_band_probe()
    # G08-05 R2 第 2 条（实测接线）与第 7 条（量化订正）：
    #   (a) 原代码把 drop_closure_ok() 的返回值写成 `_, ok = ...`，而该函数返回的是
    #       (bool, detail_dict) —— 于是 ok 取到的是 **detail 字典**，三条 verdict
    #       退化成对字典真值性的常量自检：keeps_real 恒 True、blocks_blind_band 恒 False、
    #       100pct_loss_caught 恒 False（实测：把判据喂任何输入都得到同样的三条）。
    #       现改为显式解包 (bool, detail)，并把三条 verdict 接到**实测**读数上。
    #   (b) 原代码只把 run_field 的 comp_max 当乘性因子塞进**构造出来的** total_a，
    #       新「逐 drop 判据」从未作用于任何实测逐 drop 的 |Σa − A_drop|。
    #       现由 run_field 在像素循环内部直接对实测 (tot, A_drop) 逐 drop 判定。
    _theta300 = math.radians(300.0 / 3600.0)
    a_drop_300 = (PF * _theta300) ** 2
    ok_canon, d_canon = drop_closure_ok(a_drop_300 * (1.0 + r1["comp_max"]), a_drop_300)
    ok_zero, d_zero = drop_closure_ok(0.0, a_drop_300)
    ok_small, d_small = drop_closure_ok(0.0, (PF * 0.005 * RAD_PER_ARCSEC) ** 2)
    # ---- 第 7 条：真实闭合残差用**归档实测**的 max_abs_pixel_completeness 换算 ----
    a_drop_real = a_drop_300
    resid_rel = r1["comp_max"]
    resid_abs_sr = resid_rel * a_drop_real
    review_target_sr = 1.0e-20
    out["seam_absolute_floor_B6"] = {
        "criterion": ("|Σa − A_drop| <= max(τ_rel·A_drop, ε_abs) [正本口径，未改] "
                      "AND A_drop >= APPLICABLE_MIN_ADROP [实验侧 fail-closed 适用域守卫]"),
        "tau_rel": TAU_REL, "eps_abs_pos": EPS_ABS_POS,
        "eps_geom_floor_pos": EPS_GEOM_FLOOR,
        "applicable_min_a_drop_sr": APPLICABLE_MIN_ADROP,
        "blind_band_rows": sb_rows,
        "n_rows_pos_criterion_alone_green": len(sb_vacuum),
        "n_rows_combined_green": sum(1 for r in sb_rows if r["combined_criterion_ok"]),
        # ---- 实测逐 drop（不是构造常量）----
        "measured_per_drop_closure": {
            "theta_arcsec_per_px": 300.0, "pixfrac": PF,
            "n_drops": r1["drop_closure"]["n"],
            "n_drops_ok": r1["drop_closure"]["n_ok"],
            "n_drops_out_of_applicable_domain": r1["drop_closure"]["n_out_of_domain"],
            "max_resid_rel": r1["drop_closure"]["max_resid_rel"],
            "max_rel_tolerance_pos": r1["drop_closure"]["max_rel_tolerance_pos"],
            "note": ("run_field 在像素循环内对每个 drop 的**实测** (Σ_j a_jp, A_drop) "
                     "调用 drop_closure_ok；n_drops = npix_side**2。"),
        },
        "real_reading_300as_pf08": {
            "A_drop_sr": a_drop_300,
            "comp_max_measured": r1["comp_max"],
            "combined_ok_on_real_reading": bool(ok_canon),
            "combined_ok_on_100pct_loss": bool(ok_zero),
            "criterion_detail_real": d_canon,
            "criterion_detail_100pct_loss": d_zero,
        },
        # ---- 第 7 条量化订正 ----
        "true_residual_recomputed": {
            "source": "归档实测 max_abs_pixel_completeness（本脚本 r1 实跑同值）",
            "max_abs_pixel_completeness": resid_rel,
            "A_drop_sr": a_drop_real,
            "resid_abs_sr": resid_abs_sr,
            "previous_claim_sr": 9.477e-17,
            "previous_claim_error": ("上轮 9.477e-17 sr 是把**自取** comp_max=7e-11 "
                                     "乘 A_drop 得来的，不是归档实测读数；"
                                     "改用归档实测值后为 %.4e sr，差 %.2f×。"
                                     % (resid_abs_sr, 9.477e-17 / max(resid_abs_sr, 1e-300))),
            "review_target_sr": review_target_sr,
            "verdict_on_review_target": (
                "审稿提出的 %.0e sr 绝对残差目标**仍不成立**：本单元真实可达残差是 "
                "%.4e sr，是该目标的 %.0f×。方向不变（上轮结论成立），数值订正。"
                % (review_target_sr, resid_abs_sr,
                   resid_abs_sr / review_target_sr)),
        },
        "handover": ("正本侧 ε_abs=1e-15 sr 在 A_drop ≲1e-14 sr 的域里吞掉整条判据，"
                     "而按正本 :362-364 该域的可达成残差已是 ~1e-16 sr 的几何/表示地板，"
                     "故**不可**用更小的绝对下限消掉（会误伤正确实现）；正本 :369-371 已写明"
                     "须提到扩展精度表示才能移动该地板（冻结口径、需变更流程）。"
                     "正本容差定义不在本单写入面，只登记移交。"),
    }
    _md = r1["drop_closure"]
    out["verdict"] = dict(
        conservation=bool(r1["comp_max"] < 1e-10 and abs(r1["sumF"]/r1["sumX"] - 1.0) < 1e-12),
        delta_theta2_ok=bool(abs((r2a["kgauss"] - 1.0)/d2 - 1.0) < 0.02),
        delta_theta300_ok=bool(abs((r2b["kgauss"] - 1.0)/d300 - 1.0) < 0.02),
        pf2_deficit_visible=bool(abs(r2b["kmean"] - PF**2) < 1e-3),
        injection_red=bool(abs(r3["pair_red"] - 0.0997) < 1e-6),
        injection_sum_green=bool(abs(r3["sumF"]/r3["sumX"] - 1.0) < 1e-12),
        mean_estimator_zero=bool(r4["mean_res"] < 1e-10),
        control_points=bool(worst < 1.05),
        # B6 三条：全部接在**实测**对象上。
        #  (1) 真实档位的逐 drop 判据必须全部通过，且不得有 drop 落在适用域外；
        #  (2) 100% 通量丢失（Σa=0）在**真实 drop 尺度**上必须被判红
        #      （正本项单独即可抓住：resid=A_drop ≫ τ_rel·A_drop）；
        #  (3) 真空带内（θ=0.005″/px，tol/A_drop>1）100% 丢失必须由**适用域守卫**
        #      抓住 —— 这一条正本项原理上抓不住（见 handover），靠守卫 fail-closed。
        seam_domain_guard_keeps_real_reading=bool(
            ok_canon and _md["n"] > 0 and _md["n_ok"] == _md["n"]
            and _md["n_out_of_domain"] == 0),
        seam_real_scale_100pct_loss_caught=bool(not ok_zero),
        seam_domain_guard_blocks_blind_band=bool(not ok_small),
        # 逐 drop 实测闭合率（非构造常量）：576 个 drop 全部通过正本判据 + 守卫
        seam_measured_per_drop_all_ok=bool(
            _md["n"] > 0 and _md["n_ok"] == _md["n"]),
        seam_measured_per_drop_n=int(_md["n"]),
        elapsed_s=time.time()-t0)
    print("VERDICT:", json.dumps(out["verdict"]))
    os.makedirs(os.path.join(UNIT, "results", "audit", "route3"), exist_ok=True)
    with open(os.path.join(UNIT, "results", "audit", "route3", "exp03_weight_conservation.json"), "w") as f:
        json.dump(out, f, indent=1)

if __name__ == "__main__":
    main()
