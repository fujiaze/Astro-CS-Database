#!/usr/bin/env python3
# CR-63 复算：把 lib/algorithms/shared/healpix/healpix_core.cpp 逐行转写成 Python，
# 与 astropy-healpix（本仓 THIRD_PARTY_NOTICE.md:26-29 自称所使用的外部 Oracle）对拍，
# 并对极区/环绕/越界/NaN 做独立属性检查。
# 不 import 仓库内任何 Python；只用自写转写 + 站点包 astropy_healpix。
import math
import random
import sys

sys.stdout.reconfigure(encoding="utf-8")

kPi = 3.14159265358979323846264338327950288
kTwoPi = 2.0 * kPi
kHalfPi = 0.5 * kPi
kTwoThird = 2.0 / 3.0
kRoot3 = 1.73205080756887729352744634150587237


def is_north_polar(base):
    return 0 <= base <= 3


def is_south_polar(base):
    return 8 <= base <= 11


def xy_to_nest(ix, iy, bits):
    ip = 0
    for i in range(bits):
        ip |= ((ix >> i) & 1) << (2 * i)
        ip |= ((iy >> i) & 1) << (2 * i + 1)
    return ip


def nest_to_xy(ip_low, bits):
    ix = iy = 0
    for i in range(bits):
        ix |= ((ip_low >> (2 * i)) & 1) << i
        iy |= ((ip_low >> (2 * i + 1)) & 1) << i
    return ix, iy


def radec_to_xyz(ra_rad, dec_rad):
    return (math.cos(dec_rad) * math.cos(ra_rad),
            math.cos(dec_rad) * math.sin(ra_rad),
            math.sin(dec_rad))


def xyz_to_radec(vx, vy, vz):
    ra = math.atan2(vy, vx)
    if ra < 0.0:
        ra += kTwoPi
    z = min(1.0, max(-1.0, vz))
    return ra, math.asin(z)


def xyz_to_hp(vx, vy, vz, nside):
    phi = math.atan2(vy, vx)
    if phi < 0.0:
        phi += kTwoPi
    phi_t = math.fmod(phi, kHalfPi)
    ns = nside
    base = 0
    ix = iy = 0
    if vz >= kTwoThird or vz <= -kTwoThird:
        north = vz >= kTwoThird
        zz = vz if north else -vz
        coz = math.sqrt(vx * vx + vy * vy)
        kx = (coz / math.sqrt(1.0 + zz)) * kRoot3 * abs(ns * (2.0 * phi_t - kPi) / kPi)
        ky = (coz / math.sqrt(1.0 + zz)) * kRoot3 * ns * 2.0 * phi_t / kPi
        if north:
            xx, yy = ns - kx, ns - ky
        else:
            xx, yy = ky, kx
        ix = min(ns - 1, math.floor(xx))
        iy = min(ns - 1, math.floor(yy))
        ix = max(ix, 0)
        iy = max(iy, 0)
        sector = (phi - phi_t) / kHalfPi
        offset = int(round(sector)) % 4
        base = offset if north else (8 + offset)
    else:
        zunits = (vz + kTwoThird) / (4.0 / 3.0)
        phiunits = phi_t / kHalfPi
        u1 = zunits + phiunits
        u2 = zunits - phiunits + 1.0
        xx = u1 * ns
        yy = u2 * ns
        sector = (phi - phi_t) / kHalfPi
        offset = int(round(sector)) % 4
        if xx >= ns:
            xx -= ns
            if yy >= ns:
                yy -= ns
                base = offset
            else:
                base = ((offset + 1) % 4) + 4
        else:
            if yy >= ns:
                yy -= ns
                base = offset + 4
            else:
                base = 8 + offset
        ix = math.floor(xx)
        iy = math.floor(yy)
        ix = max(ix, 0)
        iy = max(iy, 0)
        ix = min(ix, ns - 1)
        iy = min(iy, ns - 1)
    return base, ix, iy


def hp_to_xyz(basehp, px, py, dx, dy, nside):
    ns = nside
    chp0 = basehp
    x = float(px) + dx
    y = float(py) + dy
    equatorial = True
    zfactor = 1.0
    if is_north_polar(chp0) and (x + y) > ns:
        equatorial = False
        zfactor = 1.0
    if is_south_polar(chp0) and (x + y) < ns:
        equatorial = False
        zfactor = -1.0
    if equatorial:
        chp = chp0
        zoff = phioff = 0.0
        x /= float(ns)
        y /= float(ns)
        if chp <= 3:
            phioff = 1.0
        elif chp <= 7:
            zoff = -1.0
            chp -= 4
        else:
            phioff = 1.0
            zoff = -2.0
            chp -= 8
        z = kTwoThird * (x + y + zoff)
        ph = kPi / 4.0 * (x - y + phioff + 2.0 * chp)
        rad = math.sqrt(1.0 - z * z)
        return rad * math.cos(ph), rad * math.sin(ph), z
    if zfactor == -1.0:
        x, y = y, x
        x = ns - x
        y = ns - y
    if y == ns and x == ns:
        phi_t = 0.0
    else:
        phi_t = kPi * (ns - y) / (2.0 * ((ns - x) + (ns - y)))
    if phi_t < kPi / 4.0:
        vv = abs(kPi * (ns - x) / ((2.0 * phi_t - kPi) * ns) / kRoot3)
    else:
        vv = abs(kPi * (ns - y) / (2.0 * phi_t * ns) / kRoot3)
    z = (1.0 - vv) * (1.0 + vv)
    rad = math.sqrt(1.0 + z) * vv
    z = z * zfactor
    if is_south_polar(chp0):
        ph = kHalfPi * (chp0 - 8) + phi_t
    else:
        ph = kHalfPi * chp0 + phi_t
    if ph < 0.0:
        ph += kTwoPi
    return rad * math.cos(ph), rad * math.sin(ph), z


def order_of(nside):
    k = 0
    while (1 << k) < nside:
        k += 1
    return k


def ang2pix_nest(nside, ra_deg, dec_deg):
    order = order_of(nside)
    ns = 1 << order
    vx, vy, vz = radec_to_xyz(ra_deg * kPi / 180.0, dec_deg * kPi / 180.0)
    base, x, y = xyz_to_hp(vx, vy, vz, ns)
    npface = ns * ns
    return base * npface + xy_to_nest(x, y, order)


def pix2ang_nest(nside, ipix):
    """返回 (ra,dec)；越界时按 :249-255 的语义返回 (0.0, 0.0)。"""
    order = order_of(nside)
    ns = 1 << order
    npface = ns * ns
    if ipix >= 12 * npface:
        return 0.0, 0.0
    base = ipix // npface
    x, y = nest_to_xy(ipix % npface, order)
    rx, ry, rz = hp_to_xyz(base, x, y, 0.5, 0.5, ns)
    ra_rad, dec_rad = xyz_to_radec(rx, ry, rz)
    return ra_rad * 180.0 / kPi, dec_rad * 180.0 / kPi


kNbXOffset = [-1, -1, 0, 1, 1, 1, 0, -1]
kNbYOffset = [0, 1, 1, 1, 0, -1, -1, -1]
kNbFaceArray = [
    [8, 9, 10, 11, -1, -1, -1, -1, 10, 11, 8, 9],
    [5, 6, 7, 4, 8, 9, 10, 11, 9, 10, 11, 8],
    [-1, -1, -1, -1, 5, 6, 7, 4, -1, -1, -1, -1],
    [4, 5, 6, 7, 11, 8, 9, 10, 11, 8, 9, 10],
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
    [1, 2, 3, 0, 0, 1, 2, 3, 5, 6, 7, 4],
    [-1, -1, -1, -1, 7, 4, 5, 6, -1, -1, -1, -1],
    [3, 0, 1, 2, 3, 0, 1, 2, 4, 5, 6, 7],
    [2, 3, 0, 1, -1, -1, -1, -1, 0, 1, 2, 3],
]
kNbSwapArray = [[0, 0, 3], [0, 0, 6], [0, 0, 0], [0, 0, 5], [0, 0, 0],
                [5, 0, 0], [0, 0, 0], [6, 0, 0], [3, 0, 0]]
kSlotMap = [4, 3, 2, 1, 0, 7, 6, 5]


def neighbors(nside, ipix):
    if nside == 0:
        return []
    order = order_of(nside)
    ns = 1 << order
    npface = ns * ns
    if ipix >= 12 * npface:
        return []
    face = ipix // npface
    x, y = nest_to_xy(ipix % npface, order)
    out = []
    Ns, nsm1, ix, iy = ns, ns - 1, x, y
    for i in range(8):
        s = kSlotMap[i]
        xx = ix + kNbXOffset[s]
        yy = iy + kNbYOffset[s]
        nbnum = 4
        if xx < 0:
            xx += Ns
            nbnum -= 1
        elif xx > nsm1:
            xx -= Ns
            nbnum += 1
        if yy < 0:
            yy += Ns
            nbnum -= 3
        elif yy > nsm1:
            yy -= Ns
            nbnum += 3
        f = kNbFaceArray[nbnum][face]
        if f < 0:
            continue
        bits = kNbSwapArray[nbnum][face >> 2]
        if bits & 1:
            xx = nsm1 - xx
        if bits & 2:
            yy = nsm1 - yy
        if bits & 4:
            xx, yy = yy, xx
        out.append(f * npface + xy_to_nest(xx, yy, order))
    return out


def angular_distance_deg(ra1, dec1, ra2, dec2):
    d1, d2 = dec1 * kPi / 180.0, dec2 * kPi / 180.0
    dra = (ra2 - ra1) * kPi / 180.0
    c = math.sin(d1) * math.sin(d2) + math.cos(d1) * math.cos(d2) * math.cos(dra)
    c = min(1.0, max(-1.0, c))
    return math.acos(c) * 180.0 / kPi


def pixel_resolution_arcsec(nside):
    if nside == 0:
        return 0.0
    area = 4.0 * kPi / (12.0 * float(nside) * float(nside))
    return math.sqrt(area) * (180.0 * 3600.0 / kPi)


def nested_local_to_fits_index(local, shift, tile_width):
    if shift >= 32:
        shift = 31
    x, y = nest_to_xy(local, shift)
    maxv = (tile_width - 1) if tile_width > 0 else 0
    row = (maxv - x) if x <= maxv else 0
    return row * tile_width + y


def fits_index_to_nested_local(fits_index, shift, tile_width):
    if tile_width == 0:
        return 0
    if shift >= 32:
        shift = 31
    row = fits_index // tile_width
    col = fits_index % tile_width
    maxv = tile_width - 1
    x = (maxv - row) if row <= maxv else 0
    return xy_to_nest(x, col, shift)


# ---------------- 对拍与属性检查 ----------------
def hr(title):
    print("\n" + "=" * 70)
    print(title)
    print("=" * 70)


try:
    import astropy_healpix
    import astropy.version as av
    HAVE = True
except Exception as exc:  # noqa
    HAVE = False
    print("astropy_healpix 不可用:", exc)

if HAVE:
    print("astropy_healpix 版本 =", astropy_healpix.__version__,
          "| astropy 版本 =", av.version)

hr("T1 逐像素中心对拍：本实现 pix2ang vs astropy-healpix（全像素）")
if HAVE:
    import astropy.units as u

    def vec(ra_deg, dec_deg):
        r = kPi / 180.0
        d = dec_deg * r
        a = ra_deg * r
        return (math.cos(d) * math.cos(a), math.cos(d) * math.sin(a), math.sin(d))

    def stable_angle_deg(ra1, dec1, ra2, dec2):
        """atan2(|v1xv2|, v1.v2)：对小角距数值稳定（acos 形式在 1e-6 deg 量级会失真）。"""
        v1, v2 = vec(ra1, dec1), vec(ra2, dec2)
        cx = v1[1] * v2[2] - v1[2] * v2[1]
        cy = v1[2] * v2[0] - v1[0] * v2[2]
        cz = v1[0] * v2[1] - v1[1] * v2[0]
        return math.atan2(math.sqrt(cx * cx + cy * cy + cz * cz),
                          v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2]) * 180.0 / kPi

    for nside in (1, 2, 4, 8, 16, 64, 256):
        hp = astropy_healpix.HEALPix(nside, order="nested")
        npix = 12 * nside * nside
        lon, lat = hp.healpix_to_lonlat(list(range(npix)))
        lon = lon.to(u.deg).value
        lat = lat.to(u.deg).value
        n_1e9 = n_1e6 = n_1e3 = 0
        worst = 0.0
        worst_i = -1
        for i in range(npix):
            ra, dec = pix2ang_nest(nside, i)
            d = stable_angle_deg(ra, dec, float(lon[i]), float(lat[i]))
            if d > 1e-9:
                n_1e9 += 1
            if d > 1e-6:
                n_1e6 += 1
            if d > 1e-3:
                n_1e3 += 1
            if d > worst:
                worst, worst_i = d, i
        print(f"  nside={nside:>3} npix={npix:>7}: >1e-9°={n_1e9:>6} >1e-6°={n_1e6:>6} "
              f">1e-3°={n_1e3:>6} 最大={worst:.3e}°(={worst*3600*1000:.4f} mas) @{worst_i}")

hr("T2 方向→像素对拍：每阶 5 万随机点（含极区与 ±90 边界）")
if HAVE:
    import astropy.units as u
    random.seed(20260915)
    for nside in (1, 4, 32, 128):
        hp = astropy_healpix.HEALPix(nside, order="nested")
        bad = 0
        examples = []
        for _ in range(50000):
            dec = math.degrees(math.asin(2 * random.random() - 1))
            ra = random.random() * 360.0
            mine = ang2pix_nest(nside, ra, dec)
            theirs = int(hp.lonlat_to_healpix(ra * u.deg, dec * u.deg))
            if mine != theirs:
                bad += 1
                if len(examples) < 3:
                    examples.append((round(ra, 6), round(dec, 6), mine, theirs))
        print(f"  nside={nside:>3} 随机 50000 点不一致={bad}", examples if examples else "")

hr("T3 结构性属性（不依赖外部 Oracle）")
for nside in (1, 2, 8, 64):
    npix = 12 * nside * nside
    hit = set()
    inv_bad = 0
    for i in range(npix):
        ra, dec = pix2ang_nest(nside, i)
        hit.add(ang2pix_nest(nside, ra, dec))
        if not (0.0 <= ra < 360.0) or not (-90.0 <= dec <= 90.0):
            inv_bad += 1
    print(f"  nside={nside:>3}: pix2ang→ang2pix 双射覆盖={len(hit)}/{npix}"
          f"  出域(ra/dec 越界)={inv_bad}")

hr("T4 等积性（蒙特卡洛：按像素计数，应平坦）")
for nside in (4, 16):
    npix = 12 * nside * nside
    counts = [0] * npix
    random.seed(4242)
    N = 2000000
    for _ in range(N):
        dec = math.degrees(math.asin(2 * random.random() - 1))
        ra = random.random() * 360.0
        counts[ang2pix_nest(nside, ra, dec)] += 1
    exp = N / npix
    # 等积 ⇒ 每像素期望相同；泊松噪声 ~ sqrt(exp)
    mx = max(counts)
    mn = min(counts)
    zmax = (mx - exp) / math.sqrt(exp)
    zmin = (mn - exp) / math.sqrt(exp)
    print(f"  nside={nside:>2}: 期望={exp:.1f} min={mn} max={mx} z(min)={zmin:+.2f} z(max)={zmax:+.2f}"
          f"  空像素={counts.count(0)}")

hr("T5 层级一致性（NESTED 父像素 = 粗阶像素）")
for nside in (4, 32):
    bad = 0
    random.seed(7)
    for _ in range(20000):
        dec = math.degrees(math.asin(2 * random.random() - 1))
        ra = random.random() * 360.0
        fine = ang2pix_nest(nside, ra, dec)
        coarse = ang2pix_nest(nside // 4, ra, dec)
        if fine >> 4 != coarse:
            bad += 1
    print(f"  nside={nside}->nside//4: 违反 {bad}")

hr("T6 极区与环绕")
nside = 64
pole = {ang2pix_nest(nside, ra, 90.0) for ra in (0, 45, 123.456, 315, 359.999)}
spole = {ang2pix_nest(nside, ra, -90.0) for ra in (0, 90, 200, 359.9)}
print("  北极 (dec=+90) 与 ra 无关（单值）:", len(pole) == 1, pole)
print("  南极 (dec=-90) 单值:", len(spole) == 1, spole)
w1 = ang2pix_nest(nside, 359.9999999, 17.0)
w2 = ang2pix_nest(nside, -1e-7, 17.0)
w3 = ang2pix_nest(nside, 719.9999999, 17.0)
print(f"  RA 环绕: 359.9999999->{w1}  -1e-7->{w2}  719.9999999(=超一圈)->{w3}")
print("  ra 归一化后 -1e-7 与 359.9999999 是否同像素:", w1 == w2)
print("  dec 略微出域 (90.5):", ang2pix_nest(nside, 10.0, 90.5),
      " vs dec=90:", ang2pix_nest(nside, 10.0, 90.0))
for probe, args in (("dec=NaN", (nside, 10.0, float("nan"))),
                    ("ra=NaN", (nside, float("nan"), 10.0)),
                    ("dec=inf", (nside, 10.0, float("inf"))),
                    ("ra=inf", (nside, float("inf"), 10.0))):
    try:
        print(f"  {probe}: ->", ang2pix_nest(*args))
    except Exception as exc:
        print(f"  {probe}: Python 转写在 int(NaN) 处抛 {type(exc).__name__}；"
              f"C++ 侧为未定义行为（int(round(NaN))/int(floor(NaN)))，"
              f"且 ((offset%4)+4)%4 与负值钳位使其返回一个合法索引而非报错")
print("  pix2ang 越界 ipix（npix 与 npix+1）->",
      pix2ang_nest(nside, 12 * nside * nside), pix2ang_nest(nside, 12 * nside * nside + 5))
print("  => 越界哨兵 (0,0) 反查所得像素 =", ang2pix_nest(nside, 0.0, 0.0),
      "（是一个合法天区位置）")

if HAVE:
    import astropy.units as u
    hr("T6b 极点归属：本实现 vs astropy-healpix（dec=±90 逐 RA）")
    for nside in (8, 64):
        hp = astropy_healpix.HEALPix(nside, order="nested")
        for dec in (90.0, -90.0):
            mine = {}
            theirs = {}
            for ra in (0.0, 10.0, 45.0, 90.0, 135.0, 180.0, 225.0, 270.0, 315.0, 359.9):
                mine.setdefault(ang2pix_nest(nside, ra, dec), []).append(ra)
                theirs.setdefault(int(hp.lonlat_to_healpix(ra * u.deg, dec * u.deg)), []).append(ra)
            print(f"    nside={nside:>3} dec={dec:+.0f}: 本实现不同像素数={len(mine)} {sorted(mine)}"
                  f" | astropy={len(theirs)} {sorted(theirs)}")
    hr("T6c 近极点（dec=90-1e-9）逐 RA：本实现 vs astropy 是否一致")
    nside = 64
    hp = astropy_healpix.HEALPix(nside, order="nested")
    for dec in (90.0 - 1e-9, 90.0 - 1e-5):
        diff = 0
        for ra in [i * 3.0 for i in range(120)]:
            if ang2pix_nest(nside, ra, dec) != int(hp.lonlat_to_healpix(ra * u.deg, dec * u.deg)):
                diff += 1
        print(f"    dec={dec}: 120 个 RA 中与 astropy 不同 = {diff}")

hr("T7 邻居：槽数 / 对称性 / 角距界")
for nside in (1, 2, 8, 32):
    npix = 12 * nside * nside
    counts = {}
    asym = 0
    far = 0
    res = pixel_resolution_arcsec(nside) / 3600.0
    for i in range(npix):
        nb = neighbors(nside, i)
        counts[len(nb)] = counts.get(len(nb), 0) + 1
        ra0, dec0 = pix2ang_nest(nside, i)
        for j in nb:
            if i not in neighbors(nside, j):
                asym += 1
            d = angular_distance_deg(ra0, dec0, *pix2ang_nest(nside, j))
            if d > 2.6 * res:
                far += 1
    print(f"  nside={nside:>2}: 邻居数分布={counts} 非对称对={asym} 角距超界={far}"
          f"（界=2.6*res={2.6*res:.4f} deg）")

if HAVE:
    hr("T7b 邻居集合 vs astropy-healpix HEALPix.neighbours（逐像素集合等值）")
    for nside in (1, 2, 8, 32):
        npix = 12 * nside * nside
        hp = astropy_healpix.HEALPix(nside, order="nested")
        ref = hp.neighbours(list(range(npix)))  # shape (8, npix), -1 = 无
        bad = 0
        ex = []
        for i in range(npix):
            r = set(int(x) for x in ref[:, i] if x >= 0)
            m = set(neighbors(nside, i))
            if r != m:
                bad += 1
                if len(ex) < 3:
                    ex.append((i, sorted(r), sorted(m)))
        print(f"  nside={nside:>2}: 集合不一致像素数={bad}/{npix}", ex if ex else "")

hr("T8 HiPS FITS 索引：定义域内互逆 + 参数不自洽时的行为")
ok = 0
for shift, tw in ((9, 512), (6, 64), (3, 8)):
    bad = 0
    for local in range(0, (1 << (2 * shift))):
        fi = nested_local_to_fits_index(local, shift, tw)
        back = fits_index_to_nested_local(fi, shift, tw)
        if back != local or not (0 <= fi < tw * tw):
            bad += 1
    print(f"  shift={shift} tile_width={tw}: 互逆/界内失败={bad} / {1 << (2*shift)}")
    ok += bad
print("  参数不自洽（shift=9 但 tile_width=64）：")
tw_bad = 64
locals_ = range(0, 512 * 512, 7)  # shift=9 的全 local 域，但 tile_width 只给 64
outs = [nested_local_to_fits_index(l, 9, tw_bad) for l in locals_]
print(f"  shift=9 全 local 域(每 7 个取 1) + tile_width={tw_bad}（两参数不自洽）：")
print(f"    产生 fits 索引 max={max(outs)}，tile 只有 {tw_bad*tw_bad} 个像元"
      f" ⇒ 越界条数={sum(1 for o in outs if o >= tw_bad*tw_bad)}/{len(outs)}")
coll = {}
for l in locals_:
    coll.setdefault(nested_local_to_fits_index(l, 9, tw_bad), []).append(l)
multi = [v for v in coll.values() if len(v) > 1]
print(f"    碰撞（同一索引对应多个 local）组数={len(multi)} 最大组={max(len(v) for v in multi)}"
      f"；无错误、无异常返回")
print("    逆映射是否还原：", fits_index_to_nested_local(
    nested_local_to_fits_index(9 * 64 + 3, 9, tw_bad), 9, tw_bad) != 9 * 64 + 3)

hr("T9 query_disc 的 NaN / 边界行为（转写 :438-508）")


def max_pixrad_order(order):
    nside = float(1 << order)
    za = kTwoThird
    phi_a = kPi / (4.0 * nside)
    t1 = 1.0 - 1.0 / nside
    zb = 1.0 - t1 * t1 / 3.0
    dot = za * zb + math.sqrt((1.0 - za * za) * (1.0 - zb * zb)) * math.cos(phi_a)
    return math.acos(min(1.0, max(-1.0, dot)))


def query_disc(nside, ra_deg, dec_deg, radius_arcsec):
    order = order_of(nside)
    radius_rad = radius_arcsec * kPi / (180.0 * 3600.0)
    decR = dec_deg * kPi / 180.0
    raR = ra_deg * kPi / 180.0
    cz = math.sin(decR)
    out = []
    if not (radius_rad == radius_rad):  # NaN
        pass  # 让 NaN 自然走进下面的比较（如实建模）
    if radius_rad <= 0.0:
        return [ang2pix_nest(nside, ra_deg, dec_deg)]
    if radius_rad >= kPi:
        return list(range(12 * nside * nside))
    cosrad = math.cos(radius_rad)
    crpdr, crmdr = [], []
    for o in range(order + 1):
        dr = max_pixrad_order(o)
        crpdr.append(-1.0 if radius_rad + dr > kPi else math.cos(radius_rad + dr))
        crmdr.append(1.0 if radius_rad - dr < 0.0 else math.cos(radius_rad - dr))
    stk = [(11 - i, 0) for i in range(12)]
    while stk:
        pix, o = stk.pop()
        ra_c, dec_c = pix2ang_nest(1 << o, pix)
        z = math.sin(dec_c * kPi / 180.0)
        ph = ra_c * kPi / 180.0
        c = z * cz + math.sqrt(max(0.0, (1.0 - z * z) * (1.0 - cz * cz))) * math.cos(ph - raR)
        c = min(1.0, max(-1.0, c))
        if c <= crpdr[o]:
            continue
        zone = 1 if c < cosrad else (2 if c <= crmdr[o] else 3)
        if o < order:
            if zone >= 3:
                sdist = 2 * (order - o)
                for s in range(pix << sdist, (pix + 1) << sdist):
                    out.append(s)
            else:
                for i in range(4):
                    stk.append((4 * pix + (3 - i), o + 1))
        else:
            if zone >= 2:
                out.append(pix)
    return sorted(set(out))


nside = 16
npix = 12 * nside * nside
nan = float("nan")
print(f"  nside={nside} npix={npix}")
print("  radius=NaN   -> 返回像素数 =", len(query_disc(nside, 30.0, 10.0, nan)),
      "（应为 0/报错，实测=全天空?", len(query_disc(nside, 30.0, 10.0, nan)) == npix, "）")
print("  dec=NaN      -> 返回像素数 =", len(query_disc(nside, 30.0, nan, 60.0)))
print("  ra=NaN       -> 返回像素数 =", len(query_disc(nside, nan, 10.0, 60.0)))
print("  radius=0     -> ", query_disc(nside, 30.0, 10.0, 0.0), "(中心像素)")
print("  radius=-5    -> ", query_disc(nside, 30.0, 10.0, -5.0))
print("  radius=1e-9  -> 像素数 =", len(query_disc(nside, 30.0, 10.0, 1e-9)))
print("  radius=60\"   -> 像素数 =", len(query_disc(nside, 30.0, 10.0, 60.0)),
      " 每像素边长 =", round(pixel_resolution_arcsec(nside), 1), "\"")
print("  radius=181deg-> 像素数 =", len(query_disc(nside, 30.0, 10.0, 181 * 3600)))
if HAVE:
    import astropy.units as u
    hp = astropy_healpix.HEALPix(nside, order="nested")
    print("  astropy cone_search_lonlat docstring 首行:",
          (hp.cone_search_lonlat.__doc__ or "").strip().splitlines()[:2])
    ref = set(int(x) for x in hp.cone_search_lonlat(30.0 * u.deg, 10.0 * u.deg,
                                                    60.0 * u.arcsec))
    mine = set(query_disc(nside, 30.0, 10.0, 60.0))
    print("  astropy 返回数 =", len(ref), " 本实现返回数 =", len(mine))
    print("  差集 ref-mine =", sorted(ref - mine)[:8], " mine-ref =", sorted(mine - ref)[:8])

hr("T10 分辨率↔角尺度换算 vs 独立推导")
for nside in (1, 8, 512, 2 ** 17):
    mine = pixel_resolution_arcsec(nside)
    ind = math.sqrt(4 * math.pi / (12 * nside * nside)) * 206264.80624709636
    print(f"  nside={nside:>6}: 代码={mine:.6f}\" 独立 sqrt(area)={ind:.6f}\" 差={abs(mine-ind):.2e}")
print("  nside=0 ->", pixel_resolution_arcsec(0), "（require_valid_nside 对 0 抛异常，此处返回 0）")
print("  npix(0) ->", 12 * 0 * 0, "；neighbors(0,*) ->", neighbors(0, 0))
