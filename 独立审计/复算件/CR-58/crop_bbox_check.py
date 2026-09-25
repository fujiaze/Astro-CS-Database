"""CR-58 独立复算：p3_wcs.h p3_crop_window_from_sky 的 257 点边界包围盒是否
"保证不切掉任何中心落在矩形内的像素"。自己按读到的源码表达式实现 world2pix，
不 import 仓库任何 Python。"""
import math
import sys

sys.stdout.reconfigure(encoding="utf-8")


def make_frame(ra0, dec0, scale, W, H, parity="east_left", pa=0.0):
    sgn_x = -1.0 if parity == "east_left" else 1.0
    sgn_y = -sgn_x
    p = math.radians(pa)
    cp, sp = math.cos(p), math.sin(p)
    return dict(
        crval_ra_deg=ra0, crval_dec_deg=dec0,
        crpix_x=(W + 1) / 2.0, crpix_y=(H + 1) / 2.0,
        cd=((sgn_x * scale * cp, sgn_y * scale * sp),
            (-sgn_x * scale * sp, sgn_y * scale * cp)),
        W=W, H=H)


def world2pix(d, ra_deg, dec_deg):
    """逐式 p3_wcs.cpp:542-565。"""
    a0 = math.radians(d["crval_ra_deg"])
    d0 = math.radians(d["crval_dec_deg"])
    a = math.radians(ra_deg)
    dd = math.radians(dec_deg)
    denom = math.sin(d0) * math.sin(dd) + math.cos(d0) * math.cos(dd) * math.cos(a - a0)
    if denom <= 0.0:
        return None
    xi = math.cos(dd) * math.sin(a - a0) / denom
    eta = (math.sin(dd) * math.cos(d0) - math.cos(dd) * math.sin(d0) * math.cos(a - a0)) / denom
    xid, etad = math.degrees(xi), math.degrees(eta)
    cd = d["cd"]
    det = cd[0][0] * cd[1][1] - cd[0][1] * cd[1][0]
    dx = (cd[1][1] * xid - cd[0][1] * etad) / det
    dy = (-cd[1][0] * xid + cd[0][0] * etad) / det
    return dx + d["crpix_x"] - 1.0, dy + d["crpix_y"] - 1.0


def bbox_sample(d, ra_min, ra_max, dec_min, dec_max, n):
    """逐式 p3_wcs.h:305-316 的边界加密采样（含绕回）。"""
    span = (ra_max - ra_min) if ra_max > ra_min else (ra_max + 360.0 - ra_min)
    xs, ys = [], []
    for i in range(n):
        t = i / (n - 1)
        dec = dec_min + t * (dec_max - dec_min)
        for ra in (ra_min, ra_max):
            p = world2pix(d, ra, dec)
            if p:
                xs.append(p[0]); ys.append(p[1])
        ra = ra_min + t * span
        for dec in (dec_min, dec_max):
            p = world2pix(d, ra, dec)
            if p:
                xs.append(p[0]); ys.append(p[1])
    return min(xs), max(xs), min(ys), max(ys)


def bbox_true(d, ra_min, ra_max, dec_min, dec_max, n=4001):
    """矩形内部（含边界）密集扫描，当作真极值。"""
    span = (ra_max - ra_min) if ra_max > ra_min else (ra_max + 360.0 - ra_min)
    xs, ys = [], []
    for i in range(n):
        u = i / (n - 1)
        for j in range(n):
            v = j / (n - 1)
            p = world2pix(d, ra_min + u * span, dec_min + v * (dec_max - dec_min))
            if p:
                xs.append(p[0]); ys.append(p[1])
    return min(xs), max(xs), min(ys), max(ys)


def case(name, frame, rect):
    s = bbox_sample(frame, *rect, n=257)
    t = bbox_true(frame, *rect)
    # 代码取 x0=floor(min), x1=floor(max)+1
    def win(b):
        return math.floor(b[0]), math.floor(b[1]) + 1, math.floor(b[2]), math.floor(b[3]) + 1
    ws, wt = win(s), win(t)
    lost = max(0, wt[0] - ws[0]) + max(0, ws[1] - wt[1]) + max(0, wt[2] - ws[2]) + max(0, ws[3] - wt[3])
    print(f"{name}: sampled bbox=({s[0]:.4f},{s[1]:.4f})x({s[2]:.4f},{s[3]:.4f}) "
          f"true=({t[0]:.4f},{t[1]:.4f})x({t[2]:.4f},{t[3]:.4f}) "
          f"shortfall_px=({wt[0]-ws[0]:+.3f},{ws[1]-wt[1]:+.3f},{wt[2]-ws[2]:+.3f},{ws[3]-wt[3]:+.3f}) "
          f"win_sample={ws} win_true={wt} LOST={lost}")


# FOV 上限内的最大帧：1024x1024 @ 0.013 deg/px -> FOV = 0.013*1448.07 = 18.82 deg (<20)
f_eq = make_frame(150.0, 0.0, 0.013, 1024, 1024)
f_hi = make_frame(150.0, 84.0, 0.013, 1024, 1024)

print("=== 合同保证：257 点边界外扩包围盒是否覆盖真极值 ===")
case("赤道 全幅", f_eq, (140.0, 160.0, -9.0, 9.0))
case("赤道 半幅 RA 边", f_eq, (145.0, 155.0, -9.0, 9.0))
case("高纬 dec=84 中心 全幅", f_hi, (140.0, 160.0, 75.0, 93.0))
case("高纬 窄 RA 跨", f_hi, (149.0, 151.0, 75.5, 92.5))
case("高纬 极窄 RA 跨", f_hi, (149.9, 150.1, 76.0, 92.0))
case("跨 RA=0", f_eq, (355.0, 5.0, -9.0, 9.0))
