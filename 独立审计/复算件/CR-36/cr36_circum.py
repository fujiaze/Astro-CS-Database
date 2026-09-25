"""CR-36 复算 (c)：HEALPix 像元「中心→最远角」大圆角距 / hp_res 的全域最大值。
这是 spherical_overlap.cpp 里 HP_CIRCUMRADIUS_FACTOR=1.25（quick-reject 与候选
缓冲的唯一依据）以及注释「穷举证明全局最大 = 1.043827」的直接检验。
注释称穷举只做 NSIDE 16/32/64/128/256 ⇒ 极点像素是否被覆盖是关键。
"""
import math
import sys
sys.stdout.reconfigure(encoding='utf-8')
PI = math.pi


def xyf2ang(bighp, xf, yf, Ns):
    """HEALPix (bighp, xf, yf) -> (theta, phi)。与 cr36_recompute.py 同一转写。"""
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
        else:
            t = PI * (Ns - yy) / (2.0 * phi_t * Ns)
        z = (1.0 - t * t / 3.0) * zfactor
        ph = (PI / 2.0) * (bighp - 8 if bighp >= 8 else bighp) + phi_t
    if ph < 0.0:
        ph += 2 * PI
    if ph >= 2 * PI:
        ph -= 2 * PI
    z = max(-1.0, min(1.0, z))
    return math.acos(z), ph


def to_vec(theta, phi):
    st = math.sin(theta)
    return (st * math.cos(phi), st * math.sin(phi), math.cos(theta))



def cell_geom(bighp, x, y, Ns):
    tc, pc = xyf2ang(bighp, x + 0.5, y + 0.5, Ns)
    c = to_vec(tc, pc)
    best = 0.0
    for (cx, cy) in ((x, y), (x + 1, y), (x + 1, y + 1), (x, y + 1)):
        t, p = xyf2ang(bighp, float(cx), float(cy), Ns)
        v = to_vec(t, p)
        d = max(-1.0, min(1.0, sum(u * w for u, w in zip(c, v))))
        best = max(best, math.acos(d))
    return best


def hp_res(Ns):
    return math.sqrt(PI / 3.0) / Ns


print('=== (c) max(中心->最远角角距)/hp_res  ===')
for Ns in (4, 8, 16, 32, 64):
    worst = (0.0, None)
    for b in range(12):
        for y in range(Ns):
            for x in range(Ns):
                r = cell_geom(b, x, y, Ns) / hp_res(Ns)
                if r > worst[0]:
                    worst = (r, (b, x, y))
    # 极点像素单独值（bighp 0-3 的 (Ns-1,Ns-1)、bighp 8-11 的 (0,0)）
    pole = cell_geom(0, Ns - 1, Ns - 1, Ns) / hp_res(Ns)
    print('  nside=%4d  全域 max=%.6f @ %s ; 极点像素 b0(Ns-1,Ns-1)=%.6f'
          % (Ns, worst[0], worst[1], pole))

print('  --- 大 NSIDE 只看 8 个触点像素 + 赤道带/冠界典型位置 ---')
for Ns in (256, 512, 131072, 131072 * 2, 65536):
    pole = cell_geom(0, Ns - 1, Ns - 1, Ns) / hp_res(Ns)
    junction = cell_geom(4, Ns // 2, 0, Ns) / hp_res(Ns)
    eq = cell_geom(4, Ns // 2, Ns // 2, Ns) / hp_res(Ns)
    print('  nside=%7d  极点像素=%.6f  赤道带 base4(N/2,0)=%.6f  belt 中心=%.6f'
          % (Ns, pole, junction, eq))
