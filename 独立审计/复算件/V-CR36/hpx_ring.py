"""V-CR36 独立复算内核（纯标准库；不复用被审 C++ 的任何函数）。

我自己转写的 HEALPix 环序(ring scheme)像元角点定义 + 两套独立球面多边形面积式
（l'Huilier 三角剖分、Van Oosterom & Stricker 扇形）+ 极冠内按环求差的解析真面积。

角点定义来源（公开定义，不来自仓库代码）：
 (1) 电平 z_j（Gorski, Hivon & Wand 2005 的 HEALPix 环序标准式；即任务书提示的
     "cos(theta_i) = 1 - i^2/(3 nside^2)" 一族）。环 j = 0..4n：
       北极冠 1<=j<=n     : z_j = 1 - j^2/(3 n^2)
       赤道带 n<j<3n      : z_j = (4/3)(1 - j/(2n)) = 2(2n-j)/(3n)
       南极冠 3n<=j<=4n-1 : z_j = -1 + (4n-j)^2/(3 n^2)
     端点电平 z_0 = +1（北极）、z_{4n} = -1（南极）。
 (2) 每环像元数 S_j = 4j (j<=n) / 4n (n<j<3n) / 4(4n-j) (j>=3n)
     => Σ S_j = 12 n^2；等面积 => 单格真面积 A = 4π/(12 n^2) = π/(3 n^2)。
 (3) 环内方位锚定与四角排布：由 (1)(2) + 等面积 + "冠区像元有一条子午线边、
     两极各被 4 个像元以一角占据" 的结构转写，并用 astropy_healpix 2.0.0 的
     boundaries_lonlat() 做穷举校准（见 calibrate.py）。外部参考只用于校准，
     不是计算依赖——交付的 s1.py / s2.py 只跑标准库。
       w_j = 2π/S_j
       赤道带环 shift_j = 0 (j 偶) / -w_j/2 (j 奇)；冠区环 shift_j = 0
       phi_c(j,m) = shift_j + (m+1/2) w_j
       4 顶点（循环序 N,L,S,R）：
         N = (phi_N, z_{j-1})   L = (phi_c - w_j/2, z_j)
         S = (phi_S, z_{j+1})   R = (phi_c + w_j/2, z_j)
       冠区（j<=n 或 j>=3n）："朝赤道"的那个顶点取 phi_c，"朝极"的那个取
         phi_c + sgn，sgn = +w_j/2 (m 奇) / -w_j/2 (m 偶)
       赤道带：N 与 S 都取 phi_c。
"""
from math import sin, cos, sqrt, atan2, fabs, pi, tan

A2 = 2.0 * pi


def nring(n):
    return 4 * n - 1


def zlevel(j, n):
    if j <= n:
        return 1.0 - (j * j) / (3.0 * n * n)
    if j < 3 * n:
        return (4.0 / 3.0) * (1.0 - j / (2.0 * n))
    k = 4 * n - j
    return -1.0 + (k * k) / (3.0 * n * n)


def sweep(j, n):
    if j <= n:
        return 4 * j
    if j < 3 * n:
        return 4 * n
    return 4 * (4 * n - j)


def ring_pix_index(j, m, n):
    tot = 0
    for k in range(1, j):
        tot += sweep(k, n)
    return tot + m


def grid(k, n):
    """第 k 电平的顶点方位网格 (w_k, o_k)。
    冠区电平 o_k = 0；赤道带电平 o_k = 0 (k 偶) / w_k/2 (k 奇)。
    （n 为 2 的幂 ⇒ n、3n 均偶 ⇒ 冠区与赤道带在 j=n、j=3n 处网格对齐。）"""
    S = sweep(k, n) if 0 < k < 4 * n else 4
    w = A2 / S
    if n < k < 3 * n:
        return w, (0.0 if k % 2 == 0 else 0.5 * w)
    return w, 0.0


def nearest_grid(phi, k, n):
    """电平 k 网格上离 phi 最近的格点（两极电平 k=0/4n 的 φ 无定义，原样返回）。"""
    if k <= 0 or k >= 4 * n:
        return phi, None
    w, o = grid(k, n)
    i = int(round((phi - o) / w))
    p = o + i * w
    tie = fabs((phi - o) / w - (i + 0.5)) < 1e-9
    return p, tie


TIES = []


def corners(j, m, n):
    """4 角点的 (phi, z)，循环序 N,L,S,R；phi 不取模。

    定义：像元中心 phi_c 在电平 j 的顶点网格上取相邻两格点为 L/R；
    朝两极方向的两个顶点 = 相邻电平 (j-1)、(j+1) 顶点网格上离 phi_c 最近的格点
    （即与上下左右邻格共用的那个顶点）。赤道带两电平网格与 phi_c 重合 ⇒
    N、S 都落在 phi_c 上，像元成菱形；冠区则交替出现一条子午线边。"""
    S = sweep(j, n)
    w = A2 / S
    wc, oc = grid(j, n)
    phic = oc + (m + 0.5) * w
    pN, t1 = nearest_grid(phic, j - 1, n)
    pS, t2 = nearest_grid(phic, j + 1, n)
    if t1 or t2:
        TIES.append((j, m, n))
    zc = zlevel(j, n)
    return [((pN, zlevel(j - 1, n)), (phic - 0.5 * w, zc),
             (pS, zlevel(j + 1, n)), (phic + 0.5 * w, zc))]


def unit(pz):
    p, z = pz
    r = sqrt(max(0.0, 1.0 - z * z))
    return (r * cos(p), r * sin(p), z)


def quad_vecs(j, m, n):
    return [unit(x) for x in corners(j, m, n)[0]]


# ------------------------------------------------------------------ 面积核
def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def arc_len(a, b):
    cr = cross(a, b)
    return atan2(sqrt(dot(cr, cr)), dot(a, b))


def lhuilier_tri(a, b, c):
    """l'Huilier：3 边弧长 -> 球面三角面积（恒非负）。"""
    aa, bb, cc = arc_len(b, c), arc_len(a, c), arc_len(a, b)
    s = 0.5 * (aa + bb + cc)
    t = 1.0
    for x in (s, s - aa, s - bb, s - cc):
        if x <= 0.0:
            return 0.0
        t *= tan(0.5 * x)
    if t <= 0.0:
        return 0.0
    return 4.0 * atan2(sqrt(t), 1.0)


def area_lhuilier(v):
    return (lhuilier_tri(v[0], v[1], v[2]) + lhuilier_tri(v[0], v[2], v[3]))


def area_vos(v):
    """Van Oosterom & Stricker 扇形有符号累加，整体取绝对值。"""
    a = v[0]
    tot = 0.0
    for i in range(1, len(v) - 1):
        b, c = v[i], v[i + 1]
        det = dot(a, cross(b, c))
        den = 1.0 + dot(a, b) + dot(b, c) + dot(c, a)
        tot += 2.0 * atan2(det, den)
    return fabs(tot)


def area_vos_absperi(v):
    """逐三角形取 |det| 的独立写法（与 area_vos 互校）。"""
    a = v[0]
    tot = 0.0
    for i in range(1, len(v) - 1):
        b, c = v[i], v[i + 1]
        det = dot(a, cross(b, c))
        den = 1.0 + dot(a, b) + dot(b, c) + dot(c, a)
        tot += 2.0 * atan2(fabs(det), den)
    return tot


def area_planar_tangent(v):
    """切平面有向叉积和 × 1/2（质心法向）——独立写法，用于 δ_j 量级核对。"""
    cx = sum(x[0] for x in v)
    cy = sum(x[1] for x in v)
    cz = sum(x[2] for x in v)
    L = sqrt(cx * cx + cy * cy + cz * cz)
    if L == 0.0:
        return 0.0
    c = (cx / L, cy / L, cz / L)
    s = 0.0
    nv = len(v)
    for i in range(nv):
        p, q = v[i], v[(i + 1) % nv]
        u = (p[0] - dot(p, c) * c[0], p[1] - dot(p, c) * c[1],
             p[2] - dot(p, c) * c[2])
        w = (q[0] - dot(q, c) * c[0], q[1] - dot(q, c) * c[1],
             q[2] - dot(q, c) * c[2])
        s += dot(cross(u, w), c)
    return 0.5 * fabs(s)


def ang0(v):
    """max_i arccos(v_i . c_hat)（与生产 polygon_area_consistent 同定义）。"""
    cx = sum(x[0] for x in v)
    cy = sum(x[1] for x in v)
    cz = sum(x[2] for x in v)
    L = sqrt(cx * cx + cy * cy + cz * cz)
    if L == 0.0:
        return 0.0
    c = (cx / L, cy / L, cz / L)
    mx = 0.0
    for x in v:
        d = min(1.0, max(-1.0, dot(x, c)))
        mx = max(mx, atan2(sqrt(max(0.0, 1.0 - d * d)), d))
    return mx


def a_true(n):
    """解析单格面积（等面积构造）：4π/(12 n^2) = π/(3 n^2)。"""
    return pi / (3.0 * n * n)


def band_check(n, j):
    """极冠内按环求差：带界 z_bd(k) = 1 - k(k+1)/(3n^2)（自上极累计等面积），
    带面积 = 2π(z^+ - z^-)，单格 = 带面积 / S_j。赤道带带宽恒 2/(3n)。"""
    if j <= n:
        hi = 1.0 - (j - 1) * j / (3.0 * n * n)
        lo = 1.0 - j * (j + 1) / (3.0 * n * n)
    elif j < 3 * n:
        hi = lo = 0.0
        return 2.0 * pi * (2.0 / (3.0 * n)) / sweep(j, n)
    else:
        k = 4 * n - j
        hi = -1.0 + k * (k + 1) / (3.0 * n * n)
        lo = -1.0 + (k - 1) * k / (3.0 * n * n)
    return 2.0 * pi * (hi - lo) / sweep(j, n)
