#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CR-46 复算：wcs_tan.cpp 的 TAN 投影/去投影是否与一手定义一致。

两条独立途径：
  A) 逐式实现 C++ 本体（平移 wcs_tan.cpp:8-64 的表达式）；
  B) 独立向量基推导（切点单位向量 r0 + 东向 e + 北向 n，gnomonic r ∝ r0+ξe+ηn 归一化），
     与 A 的式子来源不同（不共享 atan2/asin 组合形式）。
比较 A 与 B 的角度差、A 的往返残差、退化域行为。
"""
import math
import sys

sys.stdout.reconfigure(encoding="utf-8")

D2R = math.pi / 180.0
R2D = 180.0 / math.pi


# ---------------------------------------------------------------- A: 逐式移植
def pix2sky(w, x, y):
    dx = x - w["crpix1"]
    dy = y - w["crpix2"]
    xi = (w["cd11"] * dx + w["cd12"] * dy) * D2R
    eta = (w["cd21"] * dx + w["cd22"] * dy) * D2R
    R = math.sqrt(xi * xi + eta * eta)
    dec0 = w["crval2"] * D2R
    ra0 = w["crval1"] * D2R
    if R < 1e-12:
        return ra0 / D2R, dec0 / D2R
    rho = math.atan(R)
    cr, sr = math.cos(rho), math.sin(rho)
    dec = math.asin(cr * math.sin(dec0) + (eta * sr * math.cos(dec0)) / R)
    ra = ra0 + math.atan2(xi * sr, R * math.cos(dec0) * cr - eta * math.sin(dec0) * sr)
    if ra > math.pi:
        ra -= 2 * math.pi
    if ra < -math.pi:
        ra += 2 * math.pi
    return ra * R2D, dec * R2D


def sky2pix(w, ra, dec):
    dec0 = w["crval2"] * D2R
    dra = (ra - w["crval1"]) * D2R
    guard = 0
    while dra > math.pi:
        dra -= 2 * math.pi
        guard += 1
        if guard > 10:
            raise RuntimeError("while-loop did not terminate for finite input")
    while dra < -math.pi:
        dra += 2 * math.pi
        guard += 1
        if guard > 10:
            raise RuntimeError("while-loop did not terminate for finite input")
    ddec = dec * D2R
    c0, s0 = math.cos(dec0), math.sin(dec0)
    cd, sd = math.cos(ddec), math.sin(ddec)
    den = s0 * sd + c0 * cd * math.cos(dra)
    eta = (c0 * sd - s0 * cd * math.cos(dra)) / den * R2D
    xi = (cd * math.sin(dra)) / den * R2D
    det = w["cd11"] * w["cd22"] - w["cd12"] * w["cd21"]
    if abs(det) < 1e-30:
        return w["crpix1"], w["crpix2"]
    return (w["crpix1"] + (w["cd22"] * xi - w["cd12"] * eta) / det,
            w["crpix2"] + (-w["cd21"] * xi + w["cd11"] * eta) / det)


# ------------------------------------------------- B: 独立向量基 gnomonic
def _vec(ra_deg, dec_deg):
    r = ra_deg * D2R
    d = dec_deg * D2R
    return (math.cos(d) * math.cos(r), math.cos(d) * math.sin(r), math.sin(d))


def ref_pix2sky(w, x, y):
    """独立推导：先解 CD 得 deg 域 (xi,eta)，转无量纲弧度切平面坐标，
    在切点正交基上 r = (r0 + xi*e + eta*n)/|...|，取 asin/atan2。"""
    a = x - w["crpix1"]
    b = y - w["crpix2"]
    xd = w["cd11"] * a + w["cd12"] * b          # deg 中间世界坐标
    yd = w["cd21"] * a + w["cd22"] * b
    xi = xd * D2R                                # 无量纲 = tan(theta)*sin(phi)
    eta = yd * D2R
    ra0, dec0 = w["crval1"], w["crval2"]
    r0 = _vec(ra0, dec0)
    e = (-math.sin(ra0 * D2R), math.cos(ra0 * D2R), 0.0)            # 东向单位向量
    # 北向 = r0 x e
    n = (r0[1] * e[2] - r0[2] * e[1],
         r0[2] * e[0] - r0[0] * e[2],
         r0[0] * e[1] - r0[1] * e[0])
    v = (r0[0] + xi * e[0] + eta * n[0],
         r0[1] + xi * e[1] + eta * n[1],
         r0[2] + xi * e[2] + eta * n[2])
    norm = math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2)
    dec = math.asin(v[2] / norm) * R2D
    ra = math.atan2(v[1], v[0]) * R2D
    # 归一到 [-180,180)，与 A 的输出域一致，便于逐点比较
    while ra > 180.0:
        ra -= 360.0
    while ra <= -180.0:
        ra += 360.0
    return ra, dec


def sep_deg(ra1, d1, ra2, d2):
    """大圆角距（haversine-free 标准式）。"""
    r1, r2 = ra1 * D2R, ra2 * D2R
    f1, f2 = d1 * D2R, d2 * D2R
    df = f2 - f1
    dl = r2 - r1
    a = math.sin(df / 2) ** 2 + math.cos(f1) * math.cos(f2) * math.sin(dl / 2) ** 2
    return 2 * math.asin(math.sqrt(a)) * R2D


CASES = [
    ("典型 0.18\"/px 窄场", dict(crpix1=2048.5, crpix2=2048.5, crval1=150.0, crval2=2.0,
                                cd11=-1.8e-4, cd12=0.0, cd21=0.0, cd22=1.8e-4)),
    ("带旋角 CD", dict(crpix1=100.5, crpix2=100.5, crval1=83.822, crval2=-5.391,
                       cd11=-1.0e-4 * math.cos(30 * D2R), cd12=1.0e-4 * math.sin(30 * D2R),
                       cd21=1.0e-4 * math.sin(30 * D2R), cd22=1.0e-4 * math.cos(30 * D2R))),
    ("反子午线附近 CRVAL1=0.2", dict(crpix1=50.5, crpix2=50.5, crval1=0.2, crval2=45.0,
                                     cd11=-2.5e-4, cd12=0.0, cd21=0.0, cd22=2.5e-4)),
    ("CRVAL1=359.9 (东跨 0)", dict(crpix1=50.5, crpix2=50.5, crval1=359.9, crval2=-30.0,
                                   cd11=-2.5e-4, cd12=0.0, cd21=0.0, cd22=2.5e-4)),
    ("极区参考 CRVAL2=89.5", dict(crpix1=50.5, crpix2=50.5, crval1=12.0, crval2=89.5,
                                  cd11=-2.0e-4, cd12=0.0, cd21=0.0, cd22=2.0e-4)),
    ("参考点在赤道上 CRVAL2=0", dict(crpix1=50.5, crpix2=50.5, crval1=100.0, crval2=0.0,
                                     cd11=-2.0e-4, cd12=0.0, cd21=0.0, cd22=2.0e-4)),
    ("6\"/px 宽场 4096px", dict(crpix1=2048.5, crpix2=2048.5, crval1=200.0, crval2=-15.0,
                               cd11=-1.6667e-3, cd12=0.0, cd21=0.0, cd22=1.6667e-3)),
]


def main():
    print("=== 1. A(逐式移植) vs B(独立向量基)：pix2sky 一致性 + 往返残差 ===")
    for name, w in CASES:
        half = 2048 if w["cd11"] and abs(w["cd11"]) < 5e-4 and "4096" in name else 50
        max_dev = 0.0
        max_rt = 0.0
        neg_ra = 0
        nan = 0
        for i in range(-half, half + 1, max(1, (2 * half) // 200)):
            for j in range(-half, half + 1, max(1, (2 * half) // 200)):
                x = w["crpix1"] + i
                y = w["crpix2"] + j
                ra, dec = pix2sky(w, x, y)
                rr, dd = ref_pix2sky(w, x, y)
                if not (math.isfinite(ra) and math.isfinite(dec)):
                    nan += 1
                    continue
                d = sep_deg(ra, dec, rr, dd)
                max_dev = max(max_dev, d)
                x2, y2 = sky2pix(w, ra, dec)
                max_rt = max(max_rt, math.hypot(x2 - x, y2 - y))
                if ra < 0:
                    neg_ra += 1
        print(f"  {name:26s} 独立交叉 max={max_dev:.3e} deg | 往返 max={max_rt:.3e} px"
              f" | 负 RA 点数={neg_ra} | NaN={nan}")

    print("\n=== 2. asin 参数是否越界（pix2sky 是否可能返回 NaN）：扫大视场到 179° 分离 ===")
    w = dict(crpix1=1.0, crpix2=1.0, crval1=45.0, crval2=30.0,
             cd11=-1e-3, cd12=0.0, cd21=0.0, cd22=1e-3)
    worst_arg = -9.0
    first_nan = None
    for k in range(1, 400000):
        # 沿 +eta 方向推到接近/越过 90° 分离
        eta_rad = k * 1e-4
        x = 1.0
        y = 1.0 + eta_rad * R2D / 1e-3
        try:
            ra, dec = pix2sky(w, x, y)
        except ValueError as exc:              # asin(|.|>1) -> ValueError in python
            first_nan = (k, f"math domain error @sep~{math.atan(eta_rad)*R2D:.4f} deg: {exc}")
            break
        if not math.isfinite(dec):
            first_nan = (k, "non-finite dec")
            break
    print(f"  沿 eta 推 40 万点：{'未见 domain error/NaN' if first_nan is None else first_nan}")
    # 直接测 asin 参数在 den<0（>90° 分离）时的取值域
    over = 0
    for k in range(1, 20000):
        r = k * 1e-2
        xi, eta = r, 0.0
        rho = math.atan(math.hypot(xi, eta))
        cr, sr = math.cos(rho), math.sin(rho)
        dec0 = 30.0 * D2R
        arg = cr * math.sin(dec0) + (eta * sr * math.cos(dec0)) / max(math.hypot(xi, eta), 1e-300)
        worst_arg = max(worst_arg, arg)
        if arg > 1.0:
            over += 1
    print(f"  asin 参数扫描 max={worst_arg:.17f}, >1 次数={over}")

    print("\n=== 3. 反子午线 / RA 输出域 ===")
    w = dict(crpix1=50.5, crpix2=50.5, crval1=359.9, crval2=-30.0,
             cd11=-2.5e-4, cd12=0.0, cd21=0.0, cd22=2.5e-4)
    for dx in (0, 100, 200, 400, -100, -400):
        ra, dec = pix2sky(w, 50.5 + dx, 50.5)
        rr, dd = ref_pix2sky(w, 50.5 + dx, 50.5)
        back = sky2pix(w, ra, dec)
        print(f"  dx={dx:5d}  pix2sky RA={ra:11.6f}  独立参考(归一[-180,180))={rr:11.6f}"
              f"  sky2pix 回={back[0]:10.5f} px")
    # 用 [0,360) 的 RA 喂回 sky2pix：是否仍自洽（dra wrap 的正确性）
    ra, dec = pix2sky(w, 50.5 + 400, 50.5)
    ra_pos = ra % 360.0
    print(f"  同一像素：pix2sky 给 RA={ra:.6f}；改喂 RA%360={ra_pos:.6f} 给 x="
          f"{sky2pix(w, ra_pos, dec)[0]:.6f}（应等于 {50.5+400}）")

    print("\n=== 4. den<=0（分离 > 90°）：sky2pix 的静默行为 ===")
    w = dict(crpix1=50.5, crpix2=50.5, crval1=0.0, crval2=0.0,
             cd11=-2.5e-4, cd12=0.0, cd21=0.0, cd22=2.5e-4)
    for sepd in (10.0, 80.0, 89.0, 89.99, 90.0, 90.01, 120.0, 179.0):
        try:
            x, y = sky2pix(w, sepd, 0.0)
            print(f"  星点相对参考点分离 {sepd:7.3f}° -> x={x:16.4f} px (CRPIX 偏移 {x-50.5:+.4f})")
        except Exception as exc:
            print(f"  星点相对参考点分离 {sepd:7.3f}° -> 异常 {type(exc).__name__}: {exc}")

    print("\n=== 5. inf 输入是否让 sky2pix 的 while 循环无穷（无超时保护） ===")
    try:
        import signal
        def h(s, f):
            raise TimeoutError("hang")
        signal.signal(signal.SIGALRM, h)
        signal.alarm(2)
        try:
            x, y = sky2pix(w, float("inf"), 0.0)
            print(f"  inf RA -> x={x}")
        except TimeoutError as exc:
            print(f"  inf RA -> {exc}（while 归一化环对非有限输入不收敛）")
        finally:
            signal.alarm(0)
    except (ValueError, OSError) as exc:       # SIGALRM 不可用（Windows）
        print(f"  无法用信号做超时（{exc}），改判逻辑：while (dra>pi) dra-=2pi 对 +inf 恒真")
    for bad in (float("nan"), float("inf"), -float("inf")):
        try:
            if math.isinf(bad):
                # 只判定循环式子是否恒真，不真跑
                print(f"  输入 {bad}: while(dra>pi) dra-=2*pi 条件恒真 => 无界循环（C++ 同式）")
                continue
            x, y = sky2pix(w, bad, 0.0)
            print(f"  输入 {bad} -> x={x:.1f}, y={y:.1f}")
        except Exception as exc:
            print(f"  输入 {bad} -> {type(exc).__name__}: {exc}")


if __name__ == "__main__":
    main()
