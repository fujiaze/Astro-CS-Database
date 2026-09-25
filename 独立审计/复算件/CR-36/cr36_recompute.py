"""CR-36 复算：(a) HEALPix leaf 的 4 角大圆弧四边形面积 vs 解析 pi/(3n^2)
              (b) 微小多边形 切平面面积 vs 球面面积 在 theta=1e-3 的相对偏差
纯标准库实现（自带球面多边形面积 = Van Oosterom 扇形），不复用被测 C++ 逻辑。
"""
import math
import sys

try:
    import numpy as np
    HAVE_NP = True
except Exception:
    HAVE_NP = False

sys.stdout.reconfigure(encoding='utf-8')

PI = math.pi


def xyf2ang(bighp, xf, yf, Ns):
    """HEALPix (bighp, xf, yf) -> (theta, phi)。独立转写（Gorski 2005 结构）。"""
    chp = bighp
    equatorial = True
    zfactor = 1.0
    if chp <= 3 and xf + yf > Ns:
        equatorial, zfactor = False, 1.0
    if chp >= 8 and xf + yf < Ns:
        equatorial, zfactor = False, -1.0
    if equatorial:
        zoff, phioff = 0.0, 0.0
        xn, yn = xf / Ns, yf / Ns
        if chp <= 3:
            phioff = 1.0
        elif chp <= 7:
            zoff, chp = -1.0, chp - 4
        else:
            phioff, zoff, chp = 1.0, -2.0, chp - 8
        z = (2.0 / 3.0) * (xn + yn + zoff)
        ph = (PI / 4.0) * (xn - yn + phioff + 2.0 * chp)
    else:
        xx, yy = xf, yf
        if zfactor == -1.0:
            xx, yy = yy, xf
            xx = Ns - xx
            yy = Ns - yy
        if yy >= Ns and xx >= Ns:
            phi_t = 0.0
        else:
            phi_t = PI * (Ns - yy) / (2.0 * ((Ns - xx) + (Ns - yy)))
        if phi_t < PI / 4.0:
            t = PI * (Ns - xx) / ((2.0 * phi_t - PI) * Ns)
            z = 1.0 - t * t / 3.0
        else:
            t = PI * (Ns - yy) / (2.0 * phi_t * Ns)
            z = 1.0 - t * t / 3.0
        z *= zfactor
        ph = (PI / 2.0) * (bighp - 8 if bighp >= 8 else bighp) + phi_t
    if ph < 0.0:
        ph += 2 * PI
    if ph >= 2 * PI:
        ph -= 2 * PI
    z = max(-1.0, min(1.0, z))
    return math.acos(z), ph


def tri_solid(a, b, c):
    """有向立体角 (单位向量)，Van Oosterom & Strackee 1983。"""
    bx = b[1] * c[2] - b[2] * c[1]
    by = b[2] * c[0] - b[0] * c[2]
    bz = b[0] * c[1] - b[1] * c[0]
    det = a[0] * bx + a[1] * by + a[2] * bz
    den = (1.0 + sum(p * q for p, q in zip(a, b))
           + sum(p * q for p, q in zip(b, c))
           + sum(p * q for p, q in zip(c, a)))
    return 2.0 * math.atan2(det, den)


def poly_area(pts):
    """扇形三角剖分的球面多边形面积（绝对值）。"""
    s = 0.0
    for i in range(1, len(pts) - 1):
        s += tri_solid(pts[0], pts[i], pts[i + 1])
    return abs(s)


def to_vec(theta, phi):
    st = math.sin(theta)
    return (st * math.cos(phi), st * math.sin(phi), math.cos(theta))


def quad_area_at(bighp, x, y, Ns):
    pts = []
    for (cx, cy) in ((x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)):
        t, p = xyf2ang(bighp, float(cx), float(cy), Ns)
        pts.append(to_vec(t, p))
    return poly_area(pts), pts


def planar_area(pts, c):
    """正交投影到过 c 的切平面后的平面多边形面积（生产 planar_polygon_area_n 同式）。"""
    n = len(pts)
    s = 0.0
    for i in range(n):
        p, q = pts[i], pts[(i + 1) % n]
        dp = sum(u * v for u, v in zip(p, c))
        dq = sum(u * v for u, v in zip(q, c))
        u = [p[k] - dp * c[k] for k in range(3)]
        v = [q[k] - dq * c[k] for k in range(3)]
        s += ((u[1] * v[2] - u[2] * v[1]) * c[0]
              + (u[2] * v[0] - u[0] * v[2]) * c[1]
              + (u[0] * v[1] - u[1] * v[0]) * c[2])
    return 0.5 * abs(s)


# ---------------- (a) leaf 四边形面积 vs 解析面积 ----------------
print('numpy available:', HAVE_NP)
print('=== (a) HEALPix leaf: 4 角大圆弧多边形面积 A_quad 对比解析 pi/(3 n^2) ===')
print('nside   A_analytic(sr)     mean(A_quad)     max|A_quad-A_an|   rel_deficit   sum/4pi-1')
for Ns in (2, 4, 8, 16, 32, 64):
    a_an = PI / (3.0 * Ns * Ns)
    total = 0.0
    mx = 0.0
    worst = 0.0
    nleaf = 0
    for bighp in range(12):
        for y in range(Ns):
            for x in range(Ns):
                a, _ = quad_area_at(bighp, x, y, Ns)
                total += a
                d = abs(a - a_an)
                if d > mx:
                    mx, worst = d, (bighp, x, y)
                nleaf += 1
    print('%5d  %.6e  %.6e  %.6e  %.4e  %+.2e' %
          (Ns, a_an, total / nleaf, mx, mx / a_an, total / (4 * PI) - 1.0))

# 用 max 亏缺拟合幂律  A 亏缺 ~ nside^-p
res = {}
for Ns in (4, 8, 16, 32, 64):
    a_an = PI / (3.0 * Ns * Ns)
    mx = 0.0
    for bighp in range(12):
        for y in range(Ns):
            for x in range(Ns):
                a, _ = quad_area_at(bighp, x, y, Ns)
                mx = max(mx, abs(a - a_an))
    res[Ns] = mx
print()
ks = sorted(res)
for i in range(1, len(ks)):
    n1, n2 = ks[i - 1], ks[i]
    p = math.log(res[n1] / res[n2]) / math.log(n2 / n1)
    print('  幂律指数 (nside %d -> %d): p = %.3f  [亏缺 ~ nside^-p]' % (n1, n2, p))
print('  外推 nside=512 的 max 亏缺: %.3e   (登记面 KNOWN_LIMITATIONS #45 称 1.905e-06)'
      % (res[64] * (64.0 / 512.0) ** sum(math.log(res[ks[i-1]]/res[ks[i]])/math.log(ks[i]/ks[i-1])
                                         for i in range(1, len(ks))) / len(ks)))
p_avg = sum(math.log(res[ks[i-1]]/res[ks[i]])/math.log(ks[i]/ks[i-1]) for i in range(1, len(ks)))/len(ks)
print('  平均幂指数 p=%.3f -> nside=512 外推 = %.3e ; nside=262144 外推 = %.3e'
      % (p_avg, res[64]*(64.0/512.0)**p_avg, res[64]*(64.0/262144.0)**p_avg))
print('  登记面按 0.5/nside^2 给的值: nside=512 -> %.3e ; 262144 -> %.3e'
      % (0.5/512.0**2, 0.5/262144.0**2))

print()
print('=== (a2) 有符号偏差的归属：哪些 leaf 的 4 角大圆弧四边形偏离解析面积最多 ===')
for Ns in (32, 64, 128):
    a_an = PI / (3.0 * Ns * Ns)
    rows = []
    over = under = 0
    big = 0
    for bighp in range(12):
        for y in range(Ns):
            for x in range(Ns):
                a, _ = quad_area_at(bighp, x, y, Ns)
                d = (a - a_an) / a_an
                if d > 0:
                    over += 1
                else:
                    under += 1
                if abs(d) > 0.01:
                    big += 1
                rows.append((d, bighp, x, y, a))
    rows.sort()
    print('  nside=%d  npix=%d  高估侧 %d 个 / 低估侧 %d 个 / |相对偏差|>1%%: %d 个'
          % (Ns, 12*Ns*Ns, over, under, big))
    print('    最负 3 个 (d, bighp, x, y, A_quad):', ['%+.4f b%d (%d,%d) %.6e' % r[:5] for r in rows[:3]])
    print('    最正 3 个:', ['%+.4f b%d (%d,%d) %.6e' % r[:5] for r in rows[-3:]])
    # 按 bighp 分组给出 max|d|
    per = {}
    for d, b, x, y, a in rows:
        per[b] = max(per.get(b, 0.0), abs(d))
    print('    各 base pixel 的 max|相对偏差|:', {b: round(v, 4) for b, v in sorted(per.items())})

# ---------------- (b) 切平面 vs 球面 在 theta=1e-3 ----------------
print()
print('=== (b) 大地测量正方形 (大地距 c 为 theta) : planar / spherical - 1 ===')
for theta in (1e-2, 3e-3, 1e-3, 3e-4, 1e-4):
    pts = []
    for k in range(4):
        az = PI / 4.0 + k * PI / 2.0
        # 以北极为中心的圆上取点：theta = 角半径
        pts.append((math.sin(theta) * math.cos(az), math.sin(theta) * math.sin(az),
                    math.cos(theta)))
    c = [sum(p[i] for p in pts) for i in range(3)]
    cl = math.sqrt(sum(x * x for x in c))
    c = [x / cl for x in c]
    sph = poly_area(pts)
    pl = planar_area(pts, c)
    print('  theta=%.1e  spherical=%.6e  planar=%.6e  planar/sph-1 = %+.3e  (theta^2=%+.1e)'
          % (theta, sph, pl, pl / sph - 1.0, theta * theta))
print('  代码注释 (spherical_overlap.cpp:1002) 主张 "theta<1e-3 时偏差 < 4e-8"')
