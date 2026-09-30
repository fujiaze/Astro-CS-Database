#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""exp_sim01_m16_forward_conservation.py -- P3 守恒映射算子 × M16 物理前向仿真腿。

问题
----
本单元既有全部守恒读数（路线1/2/3、kcorr）建立在**解析或常数**信号面上：常数
面亮度夹具、平面受控 MC 网格、chart 逆映射采样。真实 HST M16 在本单元只贡献
**头部 WCS**（CRVAL 旋至极点），信号面不是由物理前向过程生成的。本实验补上
最高设计 §12.2 第 1 类数据腿：M16 真实帧作**纯信号模板**，经共享物理链
（实验/shared/synthetic/m16_sampling.py -> noise_model.expose）生成仿真采样帧，
取其**无噪声期望率面**作守恒算子的输入信号面。

为什么这条腿有判别力（不是把常数场换成有结构场而已）
--------------------------------------------------------
守恒算子的逐叶读出是面亮度 S_p = F_p / N_p（N_p = Σ_j w_jp·A_pixel,j，生产
累加器 sumNorm 口径）。对**结构无关**的场，几何项完全可预测：drop 在球面上是
大圆四边形，chart 侧用直线多边形逼近，两者之差

    (pi/3)·A_chart / A_exact - 1,   A_exact = ∫∫ dxi deta / (1+xi^2+eta^2)^{3/2}

是**弦-曲线效应**，实测约 √(1+rho^2)-1 **以下**、随足迹缩小而下降（见下）。
它是实测，不是闭式，也没有解析推导。

判别力从哪来：

* **真值无效应**（把物理前向信号面换成同均值 B 的**平坦**面）⇒ 逐叶残差
  S_p/B - 1 恒等于逐叶内 (pi/3)A_chart/A_exact - 1 的面积加权平均，其上界就是
  逐像元面积比的最大值（NC-A）。这条负例不再要求"归零"：正确指数下它等于
  chart 侧几何项本身（约 6e-09），与信号场无关。
* 信号面带结构（M16 星云核 + 星场）⇒ 逐叶面亮度对全视场均值的偏离必须远离零
  （M4，防恒真；它测的是信号对比度，不是几何零）。
* 换错分母（w = a_jp/A_pixel，pf=0.8）⇒ 度量必须 = pf^2 - 1 = -0.36 判红（NC-B）；
* rho -> 0 的 drop ⇒ chart 侧项必归零到数值地板（NC-C，rho = 0 处）；
* 视场跨面界（M5）⇒ 逐 drop 分割仍完备，且跨面 drop 被 fail-closed 检出。

关于 A_exact 的指数
-------------------
`A_exact` 的被积函数指数必须是 **3/2**。用 gnomonic 圆盘半径 r 的解析立体角
2pi(1 - 1/sqrt(1+r^2)) 逐点判指数：p = 3/2 的相对误差在 r = 0.1/0.5/1/2 处是
+3.8e-15 / -2.2e-16 / +4.4e-16 / 0.0；p = 2 则是 -2.5e-3 / -5.3e-2 /
**-1.46e-1** / -2.8e-1。指数取 2 会让 A_exact 系统性偏小一个因子 sqrt(1+rho^2)，
于是一切 (pi/3)A_chart/A_exact - 1 的读数都被抬成 sqrt(1+rho^2) - 1。

四条与既有纯解析腿互补、且绑在物理帧几何上的判据：
  M1 全局守恒 Σ_p F_p / Σ_j x_j - 1（物理信号面，结构非退化）；
  M2 chart 侧 drop 面积偏差 (pi/3)A_chart/A_exact - 1，rho 由 0 扫到 10000"，
     跨 4 个足迹尺度（0.2/0.8/3.2/12.8 px）；
  M3 逐 drop 权重分割完备 Σ_p w_jp - 1（整叶内 drop 逐位 0；跨叶 drop 受 chart 角点
     绝对分辨率限制，地板 ≈ ulp(u)/drop 尺度，本实验登记实测值与地板估计）；
  M4 逐叶面亮度对信号场均值的偏离（结构场远离零）；
  M5 构造的「视场跨面界」用例：跨面 drop 的 fail-closed 与跨面界的分割完备。

固定 seed：场景配方 scenes/m16_sampling_overlap_common.json 内置
（scene seed 20261010 / frame cm_f0 seed 101），本脚本不引入新随机源。
探测器尺寸在内存内缩小（配方文件不动），不落盘任何 FITS。

用法：python3 code/audit/sim/exp_sim01_m16_forward_conservation.py
结果：results/audit/sim/exp_sim01_m16_forward_conservation.json
"""
import json
import math
import sys
import time
from pathlib import Path

import numpy as np

UNIT = Path(__file__).resolve().parents[3]
ROOT = UNIT.parents[1]
SYNTH = ROOT / "实验" / "shared" / "synthetic"
sys.path.insert(0, str(SYNTH))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "route2"))

import m16_sampling as MS          # noqa: E402  真实信号模板 -> 仿真采样帧
import p3lib as P                  # noqa: E402  独立几何原语（chart / TAN / VOS）

RESULTS = UNIT / "results" / "audit" / "sim"
RESULTS.mkdir(parents=True, exist_ok=True)

SCENE = "m16_sampling_overlap_common"
FRAME_SHAPE = [512, 512]           # 内存内缩小探测器尺寸（不写盘、不改配方文件）
CROP = 160                        # 中央裁切（px）
PF = 0.8                          # pixfrac（与既有解析腿同档）
HP_RES_PER_PIX = 4.0              # 目标叶尺度：hp_res = 4 个探测器像元
RHO_SCAN_MAX_ARCSEC = 10000.0      # M2 几何扫描半径上限（gnomonic 极限内）
RHO_SCAN_N = 25
RHO_SCAN_K = 16.0                   # 几何扫描同时跨 4 个足迹尺度（0.2/0.8/3.2/12.8 px）
NSIDE_M5_GRID = 4_000_000           # M5 跨面界用例的叶格：取得比主腿细，使 drop 必然跨叶
UV_TOL = 1e-12                      # chart (u,v) 落在 [0,1] 的判定容差（面界公共边上两侧会同时落空）


# ---------------------------------------------------------------- chart <-> sphere
def sphere_to_chart(z, phi):
    r"""(z, phi) -> (face, u, v)：p3lib.chart_to_vec **赤道带分支** 的精确逆映射。

    赤道带分支在面上是仿射的（Gorski et al. 2005, arXiv:astro-ph/0409513；正本
    `p3lib.chart_to_vec`）：对每个基面 f

        z  = (2/3)(u + v + zoff),   phi = (pi/4)(u - v + phioff + 2 chp)

    所以给定 (z, phi)，12 个面各给一组候选 (u, v)，落在 [0,1)^2 内的那个即所属面。
    面在带内构成一个**划分**（p3lib.roundtrip_tiling 全天逐点核验），故候选唯一。

    为什么不能只按方位角选面
    ------------------------
    面 4-7 的菱形**不单独铺满** |z| <= 2/3 的带。给定 z=s 换算出的 (u+v)，面 f
    的方位角窗口是 |phi - (pi/2)(f-4)| <= 45 deg * min(s, 2-s)：|z| 越远离 0，
    窗口越窄。于是带内有两段方位角落在任何面 4-7 之外，它们归入面 0-3（z>0）
    或面 8-11（z<0）的赤道带分支。按 phi 选面在这两段必然挑错面。

    实测（R=0.24 rad, CRVAL 同主腿）：phi = -85.4 deg 属面 7、phi = -60.4 deg 与
    -30 deg 属面 11、phi = 40 deg（z=+0.20）属面 0。只按 phi 选面会把它们全部
    送进错误面，逆映射给出 u<0 或 v>1 的点，逐 drop 权重于是全 0。

    |z| > 2/3 需要极冠分支；本腿视场 |z| < 0.3，不适用，此时返回 face = -1。
    """
    z = np.asarray(z, dtype=float)
    phi = np.asarray(phi, dtype=float)
    face = np.full(z.shape, -1, dtype=np.int64)
    uu = np.full(z.shape, np.nan)
    vv = np.full(z.shape, np.nan)
    for f in range(12):
        zoff = 0.0 if f <= 3 else (-1.0 if f <= 7 else -2.0)
        phioff = 0.0 if 4 <= f <= 7 else 1.0
        chp = f if f <= 3 else (f - 4 if f <= 7 else f - 8)
        ax = (np.pi / 4.0) * (phioff + 2.0 * chp)      # 面的方位轴
        s = 1.5 * z - zoff                             # = u + v
        # 折到该面轴的 2pi 主支。注意只能按 **2pi** 折，不能按相邻面轴间距 90 deg 折：
        # 面 4-7 的窗口半宽随 s 收缩（<= 45 deg），按 90 deg 折会把 180 deg 处的点
        # 误判成面 4 的中心点。
        d = phi - ax
        d = d - 2.0 * np.pi * np.round(d / (2.0 * np.pi))
        u = 0.5 * (s + 4.0 * d / np.pi)                # = (s + (u-v)) / 2
        v = 0.5 * (s - 4.0 * d / np.pi)
        hit = (u >= -UV_TOL) & (u <= 1.0 + UV_TOL) & (v >= -UV_TOL) & (v <= 1.0 + UV_TOL) \
              & (face < 0)
        # 面界是两条窗口的公共边；严格不等式会让边界点被两侧同时拒绝（返回 -1）。
        # 放宽到 UV_TOL 后边界点确定地归给编号较小的一面，再夹回 [0,1)。
        face = np.where(hit, f, face)
        uu = np.where(hit, np.clip(u, 0.0, 1.0 - UV_TOL), uu)
        vv = np.where(hit, np.clip(v, 0.0, 1.0 - UV_TOL), vv)
    return face, uu, vv


def tan_project_rad(vec, ra0, dec0):
    """gnomonic 投影的解析逆（与 tan_deproject_rad 同帧）：xi = (v.ea)/(v.n)。"""
    n, ea, ed = P.tangent_frame(ra0, dec0)
    v = np.asarray(vec, dtype=float)
    dn = np.einsum('...i,i->...', v, n)
    dxi = np.einsum('...i,i->...', v, ea)
    det = np.einsum('...i,i->...', v, ed)
    return dxi / dn, det / dn


def clip_rect(pts, u0, u1, v0, v1):
    """2D Sutherland-Hodgman（refs.md V4）：把 (u,v) 多边形裁到轴对齐矩形。"""
    def half(poly, val, lo, ax):
        out = []
        m = len(poly)
        for k in range(m):
            cur, prv = poly[k], poly[k - 1]
            sc, sp = cur[ax] - val, prv[ax] - val
            keep_c = (sc >= 0) if lo else (sc <= 0)
            keep_p = (sp >= 0) if lo else (sp <= 0)
            if keep_c:
                if not keep_p:
                    t = sp / (sp - sc)
                    out.append(prv + t * (cur - prv))
                out.append(cur)
            elif keep_p:
                t = sp / (sp - sc)
                out.append(prv + t * (cur - prv))
        return out

    poly = [np.asarray(p, dtype=float) for p in pts]
    for val, lo, ax in ((u0, True, 0), (u1, False, 0), (v0, True, 1), (v1, False, 1)):
        poly = half(poly, val, lo, ax)
        if not poly:
            return []
    return poly


def shoelace_abs(pts):
    """有向面积绝对值（质心平移，抑制 O(coord^2) 相消；见 REPORT_paper §2.1 精度约定）。"""
    if len(pts) < 3:
        return 0.0
    a = np.asarray(pts, dtype=float)
    a = a - a.mean(axis=0)
    x, y = a[:, 0], a[:, 1]
    return abs(0.5 * float(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1))))


def solid_angle_gnomonic(cx, cy, ex, ey, h, n=5):
    # noqa: D401  (docstring 内的 LaTeX 转义用原始字符串)
    r"""Gnomonic 平行四边形足迹的**精确**立体角（Gauss-Legendre 积分，机器精度）。

    gnomonic 把大圆映成直线（WCS Paper II，refs.md V5），所以"drop 足迹"在球面上
    恰是 {g 点 : gnomonic(g) 落在该平行四边形内}，其立体角是**精确**积分

        Omega = \int\int  dxi deta / (1 + xi^2 + eta^2)^{3/2}

    被积函数指数必须是 3/2。gnomonic 圆盘（半��� r）的解析立体角是
    2pi(1 - 1/sqrt(1+r^2))，等价于

        \int\int dxi deta /(1+xi^2+eta^2)^{p} = pi*[1-(1+r^2)^{1-p}]/(p-1)

    据此逐点核验（相对误差）：

    |  gnomonic 半径 r | 指数 p = 3/2 | 指数 p = 2 |
    |---|---|---|
    | 0.10 | +3.8e-15 | -2.5e-3 |
    | 0.50 | -2.2e-16 | -5.3e-2 |
    | 1.00 | +4.4e-16 | **-1.46e-1** |
    | 2.00 |  0.0e+00 | -2.8e-1 |

    指数取 2 会让"精确面积"系统性偏小一个因子 sqrt(1+rho^2)，于是
    (pi/3)A_chart/A_exact - 1 读成 sqrt(1+rho^2) - 1（其 rho^2 展开的前���项
    恰为 0.5*rho^2）。这是 oracle 自身的误差，不是被测几何的性质。

    被积函数在足迹上是光滑的 O(1) 函数，n 阶 Gauss 求积的相对误差 O((足迹张角)^{2n})
    ——对 0.16" 级 drop（张角 7.4e-7 rad）远在双精度之下。

    为什么不走 VOS 三角形公式：VOS 的 det = a·(b x c) 由 O(1) 分量相消得到，而
    小三角形的 det ~ (张角)^2；顶点分量本身只精确到 1 ulp(O(1))，故相对误差
    ~1e-16/(张角)^2 —— 在 0.16" drop 上就到 1e-3 量级（实测 1.7e-6）。本腿因此把
    精确面积判据建在 gnomonic 积分上，不建在 VOS 上。
    """
    gx, gw = np.polynomial.legendre.leggauss(n)
    s = gx[:, None]
    t = gx[None, :]
    xi = cx + h * (s * ex[0] + t * ey[0])
    eta = cy + h * (s * ex[1] + t * ey[1])
    jac = abs(h * h * (ex[0] * ey[1] - ex[1] * ey[0]))
    w = gw[:, None] * gw[None, :]
    return float(jac * np.sum(w / (1.0 + xi * xi + eta * eta) ** 1.5))


def tan_deproject_rad(xi, eta, ra0, dec0):
    """p3lib.tan_deproject 的向量化同式（WCS Paper II 式(55)，refs.md V5）。

    p3lib 版只吃标量；本实验要对 (CROP,CROP,4) 的角点批量求球面矢量，故按同式
    向量化。标量极限下与 p3lib 逐位一致（自检项 gnomonic_vectorised_parity）。
    """
    n, ea, ed = P.tangent_frame(ra0, dec0)
    p = (n + np.asarray(xi, dtype=float)[..., None] * ea
         + np.asarray(eta, dtype=float)[..., None] * ed)
    return p / np.linalg.norm(p, axis=-1, keepdims=True)


def chart_roundtrip(crv1, crv2, n_z=41, n_phi=721):
    r"""sphere_to_chart 的正确性自检：(z,phi) -> (f,u,v) -> p3lib.chart_to_vec。

    返回 (max_roundtrip_chord_error, coverage_gap_fraction)。
    往返误差用**弦长** |a-b| 度量而不是 arccos(a·b)：arccos 在近零角处的分辨率
    下限约 sqrt(2e-16) ~ 1.4e-08，会把逐位正确的结果报成 1e-08 量级。
    coverage_gap_fraction = 未落进任何面的方向占比；在 |z| < 2/3 的带内它必须是 0，
    这正是"面 4-7 的菱形不铺满该带、补面 0-3/8-11 才构成划分"这一事实的量化。
    |z| = 2/3 是赤道带分支与极冠分支的接缝，采样网格去掉端点。
    """
    z = np.linspace(-2.0 / 3.0, 2.0 / 3.0, n_z)[1:-1]   # 去掉 |z| = 2/3 的接缝
    phi = np.linspace(-np.pi, np.pi, n_phi)
    ZZ, PP = np.meshgrid(z, phi, indexing="ij")
    f, u, v = sphere_to_chart(ZZ, PP)
    gap = float(np.count_nonzero(f < 0)) / f.size
    vec = np.stack([np.sqrt(np.maximum(0.0, 1.0 - ZZ ** 2)) * np.cos(PP),
                    np.sqrt(np.maximum(0.0, 1.0 - ZZ ** 2)) * np.sin(PP),
                    ZZ], axis=-1)
    back = P.chart_to_vec(f, u, v)
    err = float(np.max(np.linalg.norm(back - vec, axis=-1)))
    return err, gap


def face_boundary_case(crv1, crv2, ex, ey, nside_grid, floor):
    r"""构造「视场跨面界」用例：在一段跨面界的视场里做逐 drop 权重分割。

    M16 视场只有 22.5" 宽，而面界在几度之外，所以主腿的读数**永远**不覆盖面界
    路径。本用例把视场直接造在面界上。

    面 4-7 的菱形在带内的方位角窗口是 |phi - axis_f| <= 45 deg * s，s = 1 + 1.5z
    （s = u + v）。|z| 越远离 0 窗口越窄，窗口之外的那段方位角归入面 0-3（z>0）
    或面 8-11（z<0）。因此面 7 / 面 11 的界是一条随 z 走的曲线

        phi_edge(z) = -90 deg + 45 deg * (1 + 1.5 z)

    两段构造：
      A. z 由 -0.26 扫到 -0.22，方位角在 phi_edge(z) 两侧各 3 deg —— 视场横跨该界，
         drop 落在面 7 与面 11 上；
      B. z 同上，方位角**恰在** phi_edge(z) 上 —— drop 本身跨面界（四角点分属两面），
         必须被 fail-closed 检出并剔除。

    逐 drop 走**与主腿完全相同**的路径（sphere_to_chart -> chart 面积 -> 叶矩形
    S-H 裁剪 -> Σ_p w_jp）。nside_grid 取得比主腿细，使 drop 必然跨叶，Σ_p w_jp
    才真的在做分割而不是恒等地落在单叶里。

    另附一条注入：同一构造改用"只按方位角选面"的旧规则 _legacy_face_uv，读它给出
    的 Σ_p w_jp。这条注入证明判据能红能绿 —— 旧规则把面 11 上的 drop 送进面 4-7 的
    逆映射，拿到越界 (u,v)，权重静默全 0 且不被任何计数捕获。
    """
    hq = 0.5 * PF                                  # drop 半宽（px 尺度），远小于视场
    zs = np.linspace(-0.26, -0.22, 13)
    spread = np.linspace(-3.0, 3.0, 41)            # deg，相对 phi_edge
    faces_touched, n_flagged, worst = set(), 0, 0.0
    worst_legacy, n_legacy_zero, n_kept, n_built = 0.0, 0, 0, 0

    for zc in zs:
        phi_edge = math.radians(-90.0 + 45.0 * (1.0 + 1.5 * zc))
        for case, dphis in (("A", spread), ("B", np.zeros(1))):
            for dphi in dphis:
                phi = phi_edge + math.radians(dphi)
                R = math.sqrt(1.0 - zc * zc)
                v0 = np.array([R * math.cos(phi), R * math.sin(phi), zc])
                cx, cy = tan_project_rad(v0, crv1, crv2)
                corners = [(cx + hq * (sx * ex[0] + sy * ey[0]),
                            cy + hq * (sx * ex[1] + sy * ey[1]))
                           for sx, sy in ((-1, -1), (-1, 1), (1, 1), (1, -1))]
                vv = tan_deproject_rad(np.array([q[0] for q in corners]),
                                       np.array([q[1] for q in corners]), crv1, crv2)
                phic = np.arctan2(vv[..., 1], vv[..., 0])
                n_built += 1
                fc, u, v = sphere_to_chart(vv[..., 2], phic)
                if bool((fc >= 0).all() and (fc == fc[0]).all()):
                    faces_touched.add(int(fc[0]))
                    worst = max(worst, abs(_partition_sum_w(
                        u, v, shoelace_abs(np.stack([u, v], axis=-1)), nside_grid) - 1.0))
                    n_kept += 1
                else:
                    n_flagged += 1
                _, lu, lv = _legacy_face_uv(vv[..., 2], phic)
                lsw = _partition_sum_w(lu, lv,
                                       shoelace_abs(np.stack([lu, lv], axis=-1)), nside_grid)
                worst_legacy = max(worst_legacy, abs(lsw - 1.0))
                if lsw == 0.0:
                    n_legacy_zero += 1
    return {
        "construction": "z 由 -0.26 扫到 -0.22；A 段方位角在 phi_edge(z)=-90+45(1+1.5z) "
                        "度两侧各 3 deg（视场横跨面 7 / 面 11 界），B 段方位角恰在 "
                        "phi_edge(z)（drop 本身跨面界）；drop 半宽 = PF/2 像元",
        "nside_grid": int(nside_grid),
        "nside_grid_note": "取得比主腿细，使 drop 必然跨叶：Σ_p w_jp 才真的在做分割。",
        "drops_constructed": n_built,
        "drops_kept_in_sum": n_kept,
        "faces_touched": sorted(faces_touched),
        "n_cross_face_flagged": n_flagged,
        "max_sum_w_minus_1_kept": float(worst),
        "injected_legacy_azimuth_only_rule": {
            "max_sum_w_minus_1": float(worst_legacy),
            "n_drops_with_all_zero_weights": n_legacy_zero,
            "note": "同一构造改用只按方位角选面的旧规则：面 11 上的 drop 被送进面 4-7 "
                    "的逆映射，拿到越界 (u,v)，裁剪循环为空，Σ_p w_jp 静默变成 0 且"
                    "不被任何计数捕获。本用例以此证明判据能红：主规则 <= 地板，"
                    "旧规则 >= 1。",
        },
        "gates": {
            "M5_field_really_spans_faces": bool(len(faces_touched) >= 2),
            "M5_cross_face_drop_flagged": bool(n_flagged > 0),
            "M5_kept_drops_conserve": bool(worst <= max(floor, 1e-12)),
            "M5_injected_legacy_rule_goes_red": bool(
                n_legacy_zero > 0 and worst_legacy >= 1.0 - 1e-9),
        },
    }


def _partition_sum_w(u, v, a_drop, nside):
    """drop 的 chart 多边形 -> 叶矩形裁剪，返回 Σ_p w_jp（与主腿同一条路径）。"""
    if not (a_drop > 0.0) or not np.isfinite(a_drop):
        return 0.0
    poly = [np.array([u[k], v[k]]) for k in range(4)]
    gi0 = max(int(np.floor(u.min() * nside)), 0)
    gi1 = min(int(np.floor(u.max() * nside)), nside - 1)
    gj0 = max(int(np.floor(v.min() * nside)), 0)
    gj1 = min(int(np.floor(v.max() * nside)), nside - 1)
    sw = 0.0
    for gi in range(gi0, gi1 + 1):
        for gj in range(gj0, gj1 + 1):
            cl = clip_rect(poly, gi / nside, (gi + 1) / nside,
                           gj / nside, (gj + 1) / nside)
            if len(cl) < 3:
                continue
            a = shoelace_abs(cl)
            if a > 0.0:
                sw += a / a_drop
    return sw


def _legacy_face_uv(z, phi):
    """注入用：**只按方位角**选面的旧规则（4 + round(phi/(pi/2))，赤道带逆映射）。

    保留在文件里只为让 M5 能演示判据能红能绿，不参与任何求和。
    """
    f = 4 + np.round(np.asarray(phi, dtype=float) / (np.pi / 2.0)).astype(np.int64)
    s = 1.0 + 1.5 * np.asarray(z, dtype=float)
    t = (4.0 / np.pi) * (np.asarray(phi, dtype=float) - (np.pi / 2.0) * (f - 4))
    return f, 0.5 * (s + t), 0.5 * (s - t)


def _named_gates(out):
    """把 out 里各处的门摊平成 {门名: bool}，便于点名报告红灯。"""
    named = {}
    for k, v in out.items():
        if isinstance(v, dict) and isinstance(v.get("gates"), dict):
            for gn, gv in v["gates"].items():
                named[gn] = bool(gv)
        if k == "negative_control":
            for cn, cv in v.items():
                if isinstance(cv, dict) and isinstance(cv.get("gates"), dict):
                    for gn, gv in cv["gates"].items():
                        named[gn] = bool(gv)
    return named


# ---------------------------------------------------------------- 物理前向仿真腿
def forward_frame():
    """共享合成链：真实 M16 帧 -> 期望率面 -> 仿真采样帧（不落盘）。"""
    scene = MS.load_scene(SYNTH / "scenes" / ("%s.json" % SCENE))
    scene["shape"] = list(FRAME_SHAPE)          # 内存内缩小，配方文件不动
    scene["frames"] = scene["frames"][:1]      # 只用第 0 帧（seed 101）
    rate, valid, cmeta = MS.load_canvas(scene, verbose=False)
    base = scene["sampling"].get("base_fwhm_px")
    base_psf = (MS.measure_base_psf(rate, valid) if base is None
                else {"fwhm_px": float(base), "n_stars": 0, "method": "config_override"})
    canvas = {"rate": rate, "valid": valid, "meta": cmeta, "base_psf": base_psf}
    frame, truth, vmask, sig = MS.render_sampling_frame(scene, canvas, frame_index=0,
                                                        verbose=False)
    return frame, truth, vmask, sig


def main():
    t0 = time.time()
    frame, truth, vmask, sig = forward_frame()
    ny, nx = sig.shape
    y0 = (ny - CROP) // 2
    x0 = (nx - CROP) // 2
    r = np.asarray(sig[y0:y0 + CROP, x0:x0 + CROP], dtype=float)
    vmask_c = np.asarray(vmask[y0:y0 + CROP, x0:x0 + CROP], dtype=bool)
    neg = int((r < 0.0).sum())
    r = np.where(vmask_c, np.maximum(r, 0.0), 0.0)     # 三次样条重采样过冲钳零（登记）

    w = truth["wcs"]
    crv1, crv2 = np.deg2rad(w["CRVAL1"]), np.deg2rad(w["CRVAL2"])
    cdx, cdy = w["CRPIX1"] - 1.0, w["CRPIX2"] - 1.0   # 0-based index 空间
    cd = np.deg2rad(np.array([[w["CD1_1"], w["CD1_2"]], [w["CD2_1"], w["CD2_2"]]],
                             dtype=float))            # 头部 CD 单位是 deg，转 rad
    as2r = math.pi / 180.0 / 3600.0
    scale_as = math.sqrt(abs(cd[0, 0] * cd[1, 1] - cd[0, 1] * cd[1, 0])) / as2r
    ex = np.array([cd[0, 0], cd[1, 0]])               # d(xi,eta)/dx [rad/px]
    ey = np.array([cd[0, 1], cd[1, 1]])               # d(xi,eta)/dy [rad/px]
    nside = int(2 ** round(math.log2(math.sqrt(math.pi / 3.0)
                                     / (HP_RES_PER_PIX * scale_as * as2r))))
    hp_res_as = math.sqrt(math.pi / 3.0) / nside / as2r

    yy, xx = np.mgrid[0:CROP, 0:CROP]
    gx = (x0 + xx) - cdx
    gy = (y0 + yy) - cdy
    xi = cd[0, 0] * gx + cd[0, 1] * gy
    eta = cd[1, 0] * gx + cd[1, 1] * gy
    rho2 = xi * xi + eta * eta                       # gnomonic 半径平方 [rad^2]

    def corners(cx, cy, pf):
        """以 (cx,cy) 为中心的像元四角 (...,4,2) 的 (xi,eta)。"""
        h = 0.5 * pf
        base = np.stack([cx, cy], axis=-1)
        out = np.empty(base.shape[:-1] + (4, 2), dtype=float)
        for k, (sx, sy) in enumerate(((-1, -1), (-1, 1), (1, 1), (1, -1))):
            out[..., k, 0] = base[..., 0] + h * (sx * ex[0] + sy * ey[0])
            out[..., k, 1] = base[..., 1] + h * (sx * ex[1] + sy * ey[1])
        return out

    drop_v = tan_deproject_rad(*[corners(xi, eta, PF)[..., k] for k in (0, 1)], crv1, crv2)
    pix_v = tan_deproject_rad(*[corners(xi, eta, 1.0)[..., k] for k in (0, 1)], crv1, crv2)

    def to_chart(vec):
        """(...,4,3) 单位矢量 -> (face, uv, single)：**逐角点**定面。

        逐角点定面是本腿能安全处理面界的唯一办法：一个 drop 只要有角点落在不同面，
        它的四角点就无法在单一 (u,v) 仿射片里比较。此时返回 single=False，
        调用方必须把它**排除**在守恒求和外（fail-closed），而不是让它带着越界的
        (u,v) 去求权重——后者会让 Σ_p w_jp 静默变成 0。
        """
        z = vec[..., 2]
        phi = np.arctan2(vec[..., 1], vec[..., 0])
        fc, u, v = sphere_to_chart(z, phi)
        uv = np.stack([u, v], axis=-1)
        in_face = fc >= 0
        same = (fc == fc[..., :1]).all(axis=-1)
        single = in_face.all(axis=-1) & same
        face = np.where(single, fc[..., 0], -1)
        return face, uv, single

    f_drop, drop_uv, drop_single = to_chart(drop_v)
    _, pix_uv, _ = to_chart(pix_v)
    outside = int(np.count_nonzero(~drop_single))

    a_drop_vos = np.empty((CROP, CROP))
    a_pix_vos = np.empty((CROP, CROP))
    a_drop_chart = np.empty((CROP, CROP))
    a_pix_chart = np.empty((CROP, CROP))
    outside = 0
    for i in range(CROP):
        for j in range(CROP):
            u, v = drop_uv[i, j, :, 0], drop_uv[i, j, :, 1]
            up, vp = pix_uv[i, j, :, 0], pix_uv[i, j, :, 1]
            if drop_single[i, j]:
                a_drop_chart[i, j] = shoelace_abs(np.stack([u, v], axis=-1))
                a_pix_chart[i, j] = shoelace_abs(np.stack([up, vp], axis=-1))
            a_drop_vos[i, j] = solid_angle_gnomonic(xi[i, j], eta[i, j], ex, ey, 0.5 * PF)
            a_pix_vos[i, j] = solid_angle_gnomonic(xi[i, j], eta[i, j], ex, ey, 0.5)

    # --- M2：rho 扫描（chart 侧几何项，含 rho=0 精确零） ---
    scan = []
    for k in range(RHO_SCAN_N + 1):
        rad = RHO_SCAN_MAX_ARCSEC * k / RHO_SCAN_N
        for az in (0.0, 0.5, 0.25, 0.75):
            cx = rad * as2r * math.cos(2 * math.pi * az)
            cy = rad * as2r * math.sin(2 * math.pi * az)
            r2 = cx * cx + cy * cy
            for kk in (0.25, 1.0, 4.0, RHO_SCAN_K):
                h = 0.5 * PF * kk
                quads = ((cx - h * (ex[0] + ey[0]), cy - h * (ex[1] + ey[1])),
                         (cx + h * (-ex[0] + ey[0]), cy + h * (-ex[1] + ey[1])),
                         (cx + h * (ex[0] + ey[0]), cy + h * (ex[1] + ey[1])),
                         (cx + h * (ex[0] - ey[0]), cy + h * (ex[1] - ey[1])))
                us2, vs2, ok = [], [], True
                for a, b in quads:
                    v1 = tan_deproject_rad(np.array(a), np.array(b), crv1, crv2)
                    fq, uq, vq = sphere_to_chart(v1[2], math.atan2(v1[1], v1[0]))
                    if int(fq) < 0:
                        ok = False
                        break
                    us2.append(uq)
                    vs2.append(vq)
                if not ok:
                    continue
                a_ch = shoelace_abs(np.stack([np.array(us2), np.array(vs2)], axis=-1))
                a_sp = solid_angle_gnomonic(cx, cy, ex, ey, h, n=9)
                scan.append({"rho_arcsec": rad, "footprint_scale_px": 0.5 * PF * kk * 2.0,
                             "rho2_rad2": r2,
                             "chart_over_exact_minus_1": float(a_ch * (math.pi / 3.0) / a_sp - 1.0)})
    scan_arr = np.array([q["chart_over_exact_minus_1"] for q in scan])
    scan_r2 = np.array([q["rho2_rad2"] for q in scan])
    scan_k = np.array([q["footprint_scale_px"] for q in scan])
    use = scan_r2 > 0.0
    slope_scan = float(np.sum(scan_arr[use] * scan_r2[use]) / np.sum(scan_r2[use] ** 2))
    zero_dev = float(np.max(np.abs(scan_arr[scan_r2 == 0.0]))) if bool((scan_r2 == 0.0).any()) \
        else float("nan")
    per_scale = []
    for kk in np.unique(scan_k[use]):
        m2 = use & (scan_k == kk)
        per_scale.append(float(np.sum(scan_arr[m2] * scan_r2[m2]) / np.sum(scan_r2[m2] ** 2)))
    # 跨足迹尺度的斜率**绝对**展度：双侧判据。原公式是相对展度
    # (max-min)/mean(per_scale)，而 mean 可以为 0 或反号，负数无条件通过。
    slope_spread = float(max(per_scale) - min(per_scale))
    # 同一 scan 的逐尺度峰值：用来判"chart 侧项随足迹缩小而下降"
    per_scale_peak = {float(kk): float(np.max(np.abs(scan_arr[use & (scan_k == kk)])))
                      for kk in np.unique(scan_k[use])}

    # --- M2'：物理裁切面上的同一读数（结构场，rho 动态范围约 8 倍） ---
    rel_geom = a_drop_chart * (math.pi / 3.0) / a_drop_vos - 1.0
    finite = np.isfinite(rel_geom) & (a_drop_vos > 0)
    gr, rg = rel_geom[finite], rho2[finite]
    slope_crop = float(np.sum(gr * rg) / np.sum(rg * rg))

    # --- 逐 drop 权重：drop chart 多边形 -> 叶矩形 S-H 裁剪 ---
    Fp_s, Fp_f, Nn, Gs = {}, {}, {}, {}
    wdev_int, wdev_str, n_int, n_str, n_tot = 0.0, 0.0, 0, 0, 0
    for i in range(CROP):
        for j in range(CROP):
            ad = a_drop_chart[i, j]
            if not (ad > 0.0) or not np.isfinite(ad):
                continue
            u, v = drop_uv[i, j, :, 0], drop_uv[i, j, :, 1]
            poly = [np.array([u[k], v[k]]) for k in range(4)]
            gi0 = max(int(np.floor(u.min() * nside)), 0)
            gi1 = min(int(np.floor(u.max() * nside)), nside - 1)
            gj0 = max(int(np.floor(v.min() * nside)), 0)
            gj1 = min(int(np.floor(v.max() * nside)), nside - 1)
            f = int(f_drop[i, j])
            om, ap = a_pix_vos[i, j], a_pix_chart[i, j] * (math.pi / 3.0)
            xj = r[i, j] * om
            hits, sw = [], 0.0
            for gi in range(gi0, gi1 + 1):
                for gj in range(gj0, gj1 + 1):
                    cl = clip_rect(poly, gi / nside, (gi + 1) / nside,
                                   gj / nside, (gj + 1) / nside)
                    if len(cl) < 3:
                        continue
                    a = shoelace_abs(cl)
                    if a <= 0.0:
                        continue
                    wg = a / ad
                    sw += wg
                    hits.append(((f, gi, gj), wg))
            n_tot += 1
            if len(hits) == 1:
                wdev_int = max(wdev_int, abs(sw - 1.0))
                n_int += 1
            else:
                wdev_str = max(wdev_str, abs(sw - 1.0))
                n_str += 1
            for key, wg in hits:
                Fp_s[key] = Fp_s.get(key, 0.0) + xj * wg
                Fp_f[key] = Fp_f.get(key, 0.0) + om * wg
                Nn[key] = Nn.get(key, 0.0) + wg * ap
                Gs[key] = Gs.get(key, 0.0) + wg * ap * rho2[i, j]

    keys = sorted(Fp_s)
    Fs = np.array([Fp_s[k] for k in keys])
    Ff = np.array([Fp_f[k] for k in keys])
    Np = np.array([Nn[k] for k in keys])
    Gp = np.array([Gs[k] for k in keys])
    geo = np.where(Np > 0, Gp / np.where(Np > 0, Np, 1.0), 0.0)

    x_tot = float(np.sum(r * a_pix_vos))
    closure = float(np.sum(Fs)) / x_tot - 1.0
    B = float(np.mean(r[r > 0.0]))
    # 残差定义：**不含**任何"扣几何项"的补偿量。
    # 旧定义里的 +0.5*<rho^2> 恰好是被测 oracle 自身指数误差 sqrt(1+rho^2)-1 的
    # rho^2 展开首项 —— 用被测量的误差去抵消被测量的量，负例因此自证。
    # 指数改正后该项没有物理含义，已整体撤掉。
    res_struct = Fs / Np / B - 1.0
    res_flat = Ff / Np - 1.0

    # 逐像元 chart 面积 vs 精确立体角：(pi/3)A_chart/A_exact - 1。
    # 平坦真值下 S_p/B - 1 恒等于逐叶内该量的面积加权平均，故它的上界是这个数组的极值。
    pix_ok = drop_single & (a_pix_chart > 0.0)
    rel_pix = np.where(pix_ok, a_pix_chart * (math.pi / 3.0) / a_pix_vos - 1.0, 0.0)
    pix_area_ratio_max = float(np.max(np.abs(rel_pix)))

    # 跨叶 drop 的 Σw 数值地板：chart 角点绝对分辨率 / drop 尺度
    drop_extent_uv = float(np.median([np.max(drop_uv[i, j, :, 0]) - np.min(drop_uv[i, j, :, 0])
                                      for i in range(0, CROP, 8) for j in range(0, CROP, 8)]))
    w_floor_est = 8.0 * np.spacing(1.0) / drop_extent_uv
    # rho=0 处的读数地板：同一类相消（chart 角点绝对分辨率 / drop 尺度），量级同为 1e-9
    zero_floor = 4.0 * np.spacing(1.0) / drop_extent_uv

    # --- NC-B：错分母臂（w = a_jp / A_pixel），只算全局守恒 ---
    ok = (a_drop_chart > 0.0) & np.isfinite(a_drop_chart) & (a_pix_chart > 0.0)
    got = float(np.sum((r * a_pix_vos)[ok] * a_drop_chart[ok] / a_pix_chart[ok])) / x_tot - 1.0

    # 向量化 gnomonic 与 p3lib 标量版的逐位一致性自检
    parity = 0.0
    for qx, qy in ((0.0, 0.0), (1.3e-4, -2.7e-4), (-9.1e-5, 4.4e-5)):
        parity = max(parity, float(np.max(np.abs(
            P.tan_deproject(qx, qy, crv1, crv2)
            - tan_deproject_rad(np.array(qx), np.array(qy), crv1, crv2)))))

    # --- chart 划分自检：sphere_to_chart -> p3lib.chart_to_vec 往返 ---
    rt_err, tiling_gaps = chart_roundtrip(crv1, crv2)

    # --- M5：视场跨面界的构造用例 ---
    m5 = face_boundary_case(crv1, crv2, ex, ey, NSIDE_M5_GRID, zero_floor)

    out = {
        "experiment": "exp_sim01_m16_forward_conservation",
        "data_class": "hst_physical_forward（真实 M16 帧作纯信号模板 -> 共享物理链 -> 仿真采样帧）",
        "scene": SCENE, "frame_id": truth["frame_id"], "seed": int(truth["seed"]),
        "exposure_s": float(truth["exposure_s"]),
        "band": truth["band"], "line": truth["line"],
        "detector_scale_arcsec_per_px": scale_as,
        "detector_scale_note": "由帧头部 CD 行列式现场导出（头部 CD 单位 deg），"
                               "与 truth['wcs']['scale_arcsec_per_px'] 及配方 "
                               "sampling.pixel_scale_arcsec 三者互校一致。",
        "psf_fwhm_px_detector": float(truth["psf"]["fwhm_px_detector"]),
        "seeing_floor_reached": bool(truth["psf"].get("seeing_floor_reached", False)),
        "nside": nside, "pixfrac": PF, "crop_px": CROP,
        "hp_res_arcsec": hp_res_as,
        "hp_res_per_detector_px": hp_res_as / scale_as,
        "rho_max_arcsec": float(np.sqrt(rho2.max()) / as2r),
        "signal_rate_e_per_s": {
            "min": float(r.min()), "median": float(np.median(r)),
            "p99": float(np.percentile(r, 99.0)), "max": float(r.max()),
            "contrast_p99_over_median": float(np.percentile(r, 99.0)
                                              / max(float(np.median(r)), 1e-30)),
            "negative_pixels_clipped_from_cubic_resample": neg,
            "note": "期望率面（无噪声真值）；三次样条重采样过冲产生的负值已钳零并计数。",
        },
        "noise_model": {
            "physical_arm": bool(frame.provenance.get("physical")),
            "poisson_terms": frame.provenance.get("poisson_terms"),
            "quantization": frame.provenance.get("quantization"),
            "saturation": frame.provenance.get("saturation"),
            "flat_applied_to": frame.provenance.get("flat_applied_to"),
            "saturated_pixels": int(frame.provenance.get("saturated_pixels", 0)),
            "note": "本实验消费的是**期望率面**（truth，不含噪声实现）；物理链六环节的"
                    "自洽性由共享自检 run_selftests.sh 承担，此处只登记本帧确实走过物理臂。",
        },
        "gnomonic_vectorised_parity_max_abs": parity,
        "M1_global_conservation": {
            "sum_F_over_sum_x_minus_1": float(closure),
            "x_total_e_per_s_sr": x_tot,
            "gates": {"M1_closure_le_1e-12": bool(abs(closure) <= 1e-12)},
        },
        "M2_drop_area_chart_vs_exact": {
            "definition": "(pi/3)*A_chart/A_exact - 1，A_exact 是 gnomonic 精确积分 "
                          "（被积函数 (1+xi^2+eta^2)^{-3/2}）。被测的是 chart 直线多边形"
                          "相对大圆足迹的弦-曲线偏差，不是闭式 0.5*rho^2。",
            "rho_scan_arcsec_max": RHO_SCAN_MAX_ARCSEC,
            "rho_scan_points": len(scan),
            "footprint_scales_px": [0.5 * PF * kk * 2.0 for kk in (0.25, 1.0, 4.0, RHO_SCAN_K)],
            "slope_vs_rho2_per_rad2": slope_scan,
            "slope_on_physical_crop": slope_crop,
            "slope_abs_spread_across_footprint_scales": slope_spread,
            "max_abs_by_footprint_scale_px": per_scale_peak,
            "rho_zero_max_abs_deviation": zero_dev,
            "rho_zero_numerical_floor": zero_floor,
            "chart_term_shrinks_with_footprint": bool(
                per_scale_peak[min(per_scale_peak)] > per_scale_peak[max(per_scale_peak)]),
            "scan_rows": scan,
            "gates": {
                # 双侧、以绝对量为单位的斜率展度门。原门是相对展度
                # (max-min)/mean(per_scale) <= 0.01，负数无条件通过 ⇒ 无判别力。
                "M2_slope_abs_spread_le_2e-3": bool(abs(slope_spread) <= 2e-3),
                "M2_rho_zero_at_floor": bool(zero_dev <= zero_floor),
                "M2_chart_term_decreases_with_footprint": bool(
                    per_scale_peak[min(per_scale_peak)] > per_scale_peak[max(per_scale_peak)]),
            },
        },
        "M3_weight_partition": {
            "sum_w_minus_1_max_abs_interior": float(wdev_int),
            "sum_w_minus_1_max_abs_straddling": float(wdev_str),
            "straddling_numerical_floor_estimate": float(w_floor_est),
            "drops_interior": int(n_int), "drops_straddling": int(n_str),
            "drops_total": int(n_tot),
            "drops_total_note": "留在守恒求和内的 drop 数。跨面界的 drop 被 fail-closed "
                                "剔除（见 drops_cross_face_excluded），不参与 Σw 与闭合判定。",
            "leaves_touched": int(len(keys)),
            "drops_cross_face_excluded": int(outside),
            "floor_reason": "跨叶 drop 的 Σw 受 chart 角点的**绝对**分辨率限制："
                            "u≈O(0.3) 处 1 ulp ≈ 1e-16，drop 在 (u,v) 的尺度 %.3g，"
                            "故地板 ≈ 8·ulp/drop（与 REPORT_paper §5 第 1 条同一类相消）。"
                            % drop_extent_uv,
            "gates": {
                "M3_interior_bitwise_zero": bool(wdev_int == 0.0),
                "M3_straddling_within_floor": bool(wdev_str <= max(1e-12, 100 * w_floor_est)),
                "M3_gnomonic_vectorised_parity": bool(parity == 0.0),
                "M3_chart_roundtrip_exact": bool(rt_err <= 1e-14),
                "M3_chart_tiling_has_no_gap": bool(tiling_gaps == 0.0),
            },
        },
        "M4_leaf_readout_vs_signal_contrast": {
            "definition": "残差 = S_p/B - 1（S_p=F_p/N_p，N_p=sum_j w_jp A_pixel,j，B=信号面均值）。"
                          "残差里不含任何几何补偿项。",
            "what_it_measures": "本读数量的是**信号场自身对比度**：逐叶面亮度相对全视场均值 "
                                "的偏离。它检验算子对结构场的响应不退化（防恒真），"
                                "**不**检验几何零 —— 指数改正前后该读数 9 位不变，"
                                "对 chart 侧几何项不敏感，故不再声称它测几何零。",
            "structured_residual_max_abs": float(np.max(np.abs(res_struct))),
            "structured_residual_rms": float(np.sqrt(np.mean(res_struct ** 2))),
            "structured_surface_brightness_contrast_rms": float(
                np.sqrt(np.mean((Fs / Np / B - 1.0) ** 2))),
            "signal_field_contrast_rms": float(np.sqrt(np.mean((r / B - 1.0) ** 2))),
            "gates": {"M4_leaf_readout_responds_to_structure": bool(
                float(np.max(np.abs(res_struct))) > 1e-3)},
        },
        "M5_face_boundary_crossing": m5,
        "negative_control": {
            "NC-A_flat_truth_area_normalisation": {
                "construction": "把物理前向信号面换成同均值 B 的**平坦**面（真值无结构效应）",
                "residual_max_abs": float(np.max(np.abs(res_flat))),
                "pixel_area_ratio_max_abs": pix_area_ratio_max,
                "judgement": "平坦真值下 S_p/B - 1 恒等于逐叶内 (pi/3)A_chart/A_exact - 1 的"
                             "面积加权平均，所以它**本就不该归零**：正确指数下它等于 chart "
                             "侧的弦-曲线几何项（约 6e-09），与信号场无关。判据因此改为"
                             "「逐叶残差不超过逐像元面积比的最大值」——"
                             "该上界成立当且仅当每个叶权重非负且分割完备，"
                             "权重为负或漏归一化时立刻破。旧判据（归零到地板）靠残差里"
                             "的 +0.5·<rho^2> 抵消项成立，而那正是被测 oracle 自身的"
                             "指数误差，属循环负例，已撤。",
                "numerical_floor": zero_floor,
                "floor_reason": "与 M3/NC-C 同一类相消：chart 角点绝对分辨率 / drop 尺度",
                "gates": {"NC-A_flat_residual_bounded_by_pixel_area_ratio": bool(
                    float(np.max(np.abs(res_flat)))
                    <= pix_area_ratio_max + max(zero_floor, 1e-15))},
            },
            "NC-B_wrong_denominator_RED": {
                "construction": "改用 A_pixel 归一（w = a_jp / A_pixel），pf=0.8",
                "predicted_sum_F_over_sum_x_minus_1": PF ** 2 - 1.0,
                "measured": float(got),
                "judgement": "报警（红）：应精确等于 pf^2-1 = -0.36",
                "gates": {"NC-B_equals_pf2_minus_1": bool(abs(got - (PF ** 2 - 1.0)) < 1e-9)},
            },
            "NC-C_rho_zero_zero_effect": {
                "construction": "rho = 0 的 drop 中心恰在视场切点（gnomonic 投影中心）",
                "max_abs_deviation": zero_dev,
                "numerical_floor": zero_floor,
                "judgement": "归零（红）：rho=0 时 chart 侧项与精确立体角之比必归零到读数地板"
                             "（地板来源与 M3 同类：chart 角点绝对分辨率 / drop 尺度）",
            },
        },
        "elapsed_s": time.time() - t0,
    }

    gates = []
    for k in ("M1_global_conservation", "M2_drop_area_chart_vs_exact",
              "M3_weight_partition", "M4_leaf_readout_vs_signal_contrast",
              "M5_face_boundary_crossing"):
        gates += list(out[k]["gates"].values())
    gates += list(out["negative_control"]["NC-A_flat_truth_area_normalisation"]["gates"].values())
    gates += list(out["negative_control"]["NC-B_wrong_denominator_RED"]["gates"].values())
    out["verdict"] = "PASS" if all(gates) else "FAIL"
    out["gates_total"] = len(gates)
    out["gates_failed"] = [n for n, g in _named_gates(out).items() if not g]
    (RESULTS / "exp_sim01_m16_forward_conservation.json").write_text(
        json.dumps(out, indent=2, ensure_ascii=False), encoding="utf-8")
    slim = {k: v for k, v in out.items() if k != "M2_drop_area_chart_vs_exact"}
    slim["M2_drop_area_chart_vs_exact"] = {
        k: v for k, v in out["M2_drop_area_chart_vs_exact"].items() if k != "scan_rows"}
    print(json.dumps(slim, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
