# -*- coding: utf-8 -*-
"""AUD-204/T7: 生产小 drop 分支（切平面 planar_polygon_area）下
   delta = A_drop/(pf^2 A_pixel) - 1 的实测；与球面 VOS 分支对照。
   生产选支判据 = max_angle < 1e-3 rad (= 206.3") => theta<=200" 走 planar。
   面积算法逐字按 spherical_overlap.cpp:1007-1034 移植（切平面投影+叉积和）。"""
import json, math
import numpy as np

def basis(ra0, dec0):
    a, d = math.radians(ra0), math.radians(dec0)
    T = np.array([math.cos(d)*math.cos(a), math.cos(d)*math.sin(a), math.sin(d)])
    e1 = np.array([-math.sin(d)*math.cos(a), -math.sin(d)*math.sin(a), math.cos(d)])
    e2 = np.array([-math.sin(a), math.cos(a), 0.0])
    return T, e1, e2

def corners(T, e1, e2, th, pf):
    h = 0.5*pf*math.radians(th/3600.0)
    out = []
    for (x, y) in [(-h,-h),(h,-h),(h,h),(-h,h)]:
        p = T + y*e1 + x*e2
        out.append(p/np.linalg.norm(p))
    return np.array(out)

def planar_area(pts):                      # spherical_overlap.cpp:1007
    c = pts.sum(axis=0); c = c/np.linalg.norm(c)
    s = 0.0
    for i in range(len(pts)):
        p, q = pts[i], pts[(i+1) % len(pts)]
        u = p - np.dot(p, c)*c; v = q - np.dot(q, c)*c
        s += float(np.dot(np.cross(u, v), c))
    return 0.5*abs(s)

def vos(pts):                              # 球面扇形剖分（= 生产 theta>206" 分支）
    tot = 0.0
    for i in range(1, len(pts)-1):
        a, b, d = pts[0], pts[i], pts[i+1]
        det = float(np.dot(a, np.cross(b, d)))
        den = 1.0 + float(np.dot(a,b)) + float(np.dot(b,d)) + float(np.dot(d,a))
        tot += 2.0*math.atan2(det, den)
    tot = abs(tot)
    return 4*math.pi - tot if tot > 2*math.pi else tot

rows = []
for th in (0.2, 2.0, 6.3, 60.0, 200.0, 210.0, 300.0):
    T = basis(40.0, 10.0)
    r = {"theta_arcsec": th, "branch_production": "planar" if th < 206.3 else "spherical"}
    for name, fn in (("planar", planar_area), ("spherical_vos", vos)):
        Ap, Ad = fn(corners(*T, th, 1.0)), fn(corners(*T, th, 0.8))
        r[name+"_delta"] = Ad/(0.64*Ap) - 1.0
        r[name+"_A_pixel_sr"] = Ap
    rows.append(r); print(json.dumps(r), flush=True)
with open("t7_out.json","w",encoding="utf-8") as fh: json.dump(rows, fh, indent=1)
