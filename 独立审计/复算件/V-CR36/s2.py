"""CR-36-18 复算（纯标准库）：DISP-009 门 P 判据 |S_p/B0 - 1| < 1e-3 是否
在"夹具注入面积 A_test 与生产算面积 A_prod 同式"的前提下代数上不含几何信息。

Part A: 我自己写的两式互校 —— 生产球面支（扇形有符号累加后取 |·|，
        cpp:230-256 的形式）vs 夹具侧（逐三角形取 |det|，geom:144-147 的形式）
        是否同一函数；并给反例说明"同形"仅对凸多边形成立。
Part B: 用夹具同款尺度（16x16、300"/px、CRVAL2=20°）的等纬/等经像元四角算
        A_test / A_prod 的真实 δ_j 分布 + 生产分支阈 ang0 的分布。
Part C: 合成门演示 —— 任意逐对缩放 a_jp（含 ×1e6 / ÷1e6、以及按 CR-36-01
        给含极像元整体少算 9.97%、以及整批候选丢失）下 S_p/B0 的可达区间。

（记录：B 段原先用自写的 gnomonic 反投影四角，因该 helper 的模长归一式写错
 ——L 漏了 z 分量 —— 使输入向量 |v|=1.064。l'Huilier（只用边长）对此不敏感，
 Van Oosterom（用点积/三重积）敏感 ⇒ 两式立刻差 9.6% 而暴露问题。这正是
 "两套独立实现互校"的价值，也说明 VOS 类面积核对输入必须严格单位化。）
"""
import random
from math import sqrt, sin, cos, pi, fabs
import hpx_ring as H

SEC = pi / 648000.0
DEG = pi / 180.0


def iso_quad(a0, d0, sa, sd):
    """等纬/等经四角像元（真面积解析已知 = sa·(sin δ_top − sin δ_bot)）。
    用它代替 gnomonic 反投影四角：两种类实现（有符号扇形 / l'Huilier）在此
    族上与解析值三方吻合到 1e-7 量级，可作为 δ_j 量级的可信测量。"""
    out = []
    for (da, dd) in ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5)):
        al = a0 + da * sa
        dl = d0 + dd * sd
        out.append((cos(dl) * cos(al), cos(dl) * sin(al), sin(dl)))
    return out


def prod_branch_area(v):
    """复现生产的分支选择：ang0 < 1e-3 -> 切平面式；否则球面 VOS 扇形。
    （式子全部用我自己写的实现，不用仓库代码。）"""
    return (H.area_planar_tangent(v) if H.ang0(v) < 1e-3
            else H.area_vos(v))


def main():
    print('== A. 两处式子是否同形（我的两套独立实现互校）==')
    rnd = random.Random(20260925)
    worst_convex = 0.0
    for _ in range(4000):
        # 随机凸球面四边形：中心 + 小扰动
        z = rnd.uniform(-0.99, 0.99)
        p = rnd.uniform(0, 2 * pi)
        r = sqrt(max(0.0, 1 - z * z))
        c = (r * cos(p), r * sin(p), z)
        # 以 c 为中心随机造 4 个切向扰动点（保序 -> 凸）
        e1 = H.cross(c, (0.0, 0.0, 1.0))
        L1 = sqrt(sum(t * t for t in e1)) or 1.0
        e1 = (e1[0] / L1, e1[1] / L1, e1[2] / L1)
        e2 = H.cross(c, e1)
        angs = sorted(rnd.uniform(0, 2 * pi) for _ in range(4))
        th = rnd.uniform(1e-6, 1e-2)
        vs = []
        for a in angs:
            u = (cos(a) * th, sin(a) * th)
            v = (c[0] + u[0] * e1[0] + u[1] * e2[0],
                 c[1] + u[0] * e1[1] + u[1] * e2[1],
                 c[2] + u[0] * e1[2] + u[1] * e2[2])
            Lv = sqrt(sum(t * t for t in v))
            vs.append((v[0] / Lv, v[1] / Lv, v[2] / Lv))
        a1, a2 = H.area_vos(vs), H.area_vos_absperi(vs)
        if a1 > 0:
            worst_convex = max(worst_convex, fabs(a2 / a1 - 1))
    print('  凸四边形 4000 例: |area_vos_absperi / area_vos - 1| 最差 = %.3e' % worst_convex)
    # 差别只在"顶点序不是循环序"（交叉序）时出现：有符号扇形给出两三角之差，
    # 逐三角取绝对值给出两者之和 —— 用同一个凸四边形的交叉序作反例。
    z = 0.3
    r = sqrt(1 - z * z)
    c = (r, 0.0, z)
    e1 = (0.0, 1.0, 0.0)
    e2 = H.cross(c, e1)
    pts = [(0.0, 0.004), (0.004, 0.0), (0.0, -0.004), (-0.004, 0.0)]
    vs = []
    for u, v2 in pts:
        w = (c[0] + u * e1[0] + v2 * e2[0], c[1] + u * e1[1] + v2 * e2[1],
             c[2] + u * e1[2] + v2 * e2[2])
        L = sqrt(sum(t * t for t in w))
        vs.append((w[0] / L, w[1] / L, w[2] / L))
    cross_order = [vs[0], vs[1], vs[3], vs[2]]
    av_c = H.area_vos(cross_order)
    aa_c = H.area_vos_absperi(cross_order)
    print('  循环序菱形: 扇形=%.9e 逐三角|·|=%.9e 比=%.6f'
          % (H.area_vos(vs), H.area_vos_absperi(vs),
             H.area_vos_absperi(vs) / H.area_vos(vs)))
    print('  交叉序(顶点序非循环序): 扇形=%.9e 逐三角|·|=%.9e 比值=%s  '
          '<- 两式在此分岔（生产用前者、夹具用后者；本例扇形相消为 0）'
          % (av_c, aa_c, ('%.4f' % (aa_c / av_c)) if av_c > 0 else 'INF(扇形=0)'))

    print('== B. 夹具同款几何下 δ_j = A_test/A_prod - 1 与生产分支阈 ==')
    W = Hh = 16
    s = 300.0 * SEC
    DEG = pi / 180.0
    a0, d0 = 0.0, 20.0 * DEG          # 对齐 fix:74 CRVAL2=20 的档位
    sa, sd = s / cos(d0), s           # 探测器正方像元在天空的 (dRA, dDec)
    deltas = []
    ang0s = []
    d_sph = []
    d_plan = []
    exact_dev = []
    for y in range(Hh):
        for x in range(W):
            dc = d0 + (y - Hh / 2.0) * sd
            vp = iso_quad(a0 + (x - W / 2.0) * sa, dc, sa, sd)          # 未收缩
            ex = sa * (sin(dc + sd / 2.0) - sin(dc - sd / 2.0))         # 解析真面积
            at = H.area_vos_absperi(vp)                                # 夹具式
            ap = prod_branch_area(vp)                                  # 生产式(分支自选)
            aps = H.area_vos(vp)                                       # 生产球面支
            deltas.append(at / ap - 1.0)
            d_sph.append(at / aps - 1.0)
            d_plan.append(at / H.area_planar_tangent(vp) - 1.0)
            exact_dev.append(max(abs(aps / ex - 1.0), abs(at / ex - 1.0)))
            ang0s.append(H.ang0(vp))
    lo, hi = min(deltas), max(deltas)
    print('  两式(夹具/生产球面)对解析真面积的最大偏差 = %.3e (互校 l.Huilier 见 A 段)'
          % max(exact_dev))
    print('  ang0(未收缩像素) 范围 = [%.6e, %.6e] rad ; 分支阈 1e-3 -> 走切平面支 %d/%d'
          % (min(ang0s), max(ang0s), sum(1 for a in ang0s if a < 1e-3), len(ang0s)))
    print('  δ_j(实际分支) ∈ [%.4e, %.4e]  max|δ| = %.4e' % (lo, hi, max(fabs(lo), fabs(hi))))
    print('  δ_j(若两边都走球面支) max|δ| = %.4e   (夹具式 vs 生产球面支)'
          % max(fabs(min(d_sph)), fabs(max(d_sph))))
    th = max(ang0s)
    print('  参考: θ²/2 (θ=max ang0) = %.4e ; (θ/2)²·? 同阶核对' % (0.5 * th * th))

    print('== C. 合成门：任意逐对缩放 a_jp 下 S_p/B0 的可达区间 ==')
    B0 = 1000.0
    NJ = W * Hh
    Aprod = []
    Aprod_pf08 = []
    for y in range(Hh):
        for x in range(W):
            dc = d0 + (y - Hh / 2.0) * sd
            vp = iso_quad(a0 + (x - W / 2.0) * sa, dc, sa, sd)
            Aprod.append(prod_branch_area(vp))
            # pixfrac=0.8 的 drop（收缩四角）走哪一支：ang0 缩到 0.8 倍
            v8 = iso_quad(a0 + (x - W / 2.0) * sa, dc, 0.8 * sa, 0.8 * sd)
            Aprod_pf08.append(prod_branch_area(v8))
    Aprod_test = []
    for y in range(Hh):
        for x in range(W):
            dc = d0 + (y - Hh / 2.0) * sd
            vp = iso_quad(a0 + (x - W / 2.0) * sa, dc, sa, sd)
            Aprod_test.append(H.area_vos_absperi(vp))
    d8 = [Aprod_test[j] / Aprod_pf08[j] - 1.0 for j in range(W * Hh)]
    print('  对照 pixfrac=0.8（drop 角点收缩）: 生产分支走切平面, '
          'δ_j = A_夹具/A_生产 − 1 ∈ [%.4e, %.4e]' % (min(d8), max(d8)))
    NP = 9
    rnd2 = random.Random(7)
    Adrop = [Aprod_test[j] for j in range(NJ)]          # pixfrac=1: drop=pixel

    def Sp(atest, a, w, apart):
        vals = []
        for p in range(NP):
            f = 0.0
            nn = 0.0
            for j in range(NJ):
                ww = a[j][p] / w[j]
                f += atest[j] * ww
                nn += apart[j] * ww
            vals.append(f / nn if nn > 0 else float('nan'))
        return vals

    # 物理一致的 a_jp：每源像素把 A_drop,j 分给若干 leaf
    def make_a(scale=None):
        a = []
        for j in range(NJ):
            ws = [rnd2.random() + 1e-3 for _ in range(NP)]
            tot = sum(ws)
            row = [Adrop[j] * w / tot for w in ws]
            if scale is not None:
                row = [row[p] * scale(j, p) for p in range(NP)]
            a.append(row)
        return a

    tol = 1e-3
    # C1: δ_j == 0（A_test 与 A_prod 逐字同式）+ 任意逐对缩放
    a0_ = make_a()
    v = Sp(Aprod, a0_, Adrop, Aprod)
    print('  C1 A_test:=A_prod, 无缩放        : max|S/B0-1| = %.3e' %
          max(fabs(x - 1.0) for x in v))
    worst = 0.0
    for trial in range(40):
        sc = rnd2.choice([
            lambda j, p: rnd2.uniform(1e-6, 1e6),
            lambda j, p: (0.9003 if p in (0, NP - 1) else 1.0),
            lambda j, p: (0.0 if (j + p) % 7 == 0 else 1.0),
            lambda j, p: rnd2.choice([0.0, 1e-9, 1e9]),
        ])
        a = make_a(sc)
        vv = [x for x in Sp(Aprod, a, Adrop, Aprod)]
        ok = [x for x in vv if x == x and fabs(x) != float('inf')]
        worst = max(worst, max(fabs(x - 1.0) for x in ok))
    print('  C1 A_test:=A_prod, 40 组随机逐对缩放(含 ×1e6/÷1e6/置零/CR-36-01 的 '
          '0.9003): max|S/B0-1| = %.3e  -> 判红(>=%g)? %s'
          % (worst, tol, worst >= tol))
    # C2: 真实 δ_j != 0
    v2 = Sp(Aprod_test, a0_, Adrop, Aprod)
    print('  C2 A_test=夹具球面式, A_prod=生产分支: max|S/B0-1| = %.3e' %
          max(fabs(x - 1.0) for x in v2))
    worst2 = 0.0
    for trial in range(40):
        sc = rnd2.choice([
            lambda j, p: rnd2.uniform(1e-6, 1e6),
            lambda j, p: (0.9003 if p in (0, NP - 1) else 1.0),
            lambda j, p: (0.0 if (j + p) % 5 == 0 else 1.0),
        ])
        a = make_a(sc)
        vv = Sp(Aprod_test, a, Adrop, Aprod)
        worst2 = max(worst2, max(fabs(x - 1.0) for x in vv))
    print('  C2 40 组随机逐对缩放下 max|S/B0-1| = %.3e ; 理论界 max|δ_j| = %.3e ; '
          '界/容差 = %.3e' % (worst2, max(fabs(lo), fabs(hi)),
                              max(fabs(lo), fabs(hi)) / tol))
    print('  结论性核对: C2 的可达区间是否始终低于容差 %g ? %s'
          % (tol, worst2 < tol))


if __name__ == '__main__':
    main()
