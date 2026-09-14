# -*- coding: utf-8 -*-
import math
def sep(ra1,d1,ra2,d2):
    p1,p2=math.radians(d1),math.radians(d2); dl=math.radians(ra2-ra1)
    return math.degrees(math.atan2(math.hypot(math.cos(p2)*math.sin(dl),math.cos(p1)*math.sin(p2)-math.sin(p1)*math.cos(p2)*math.cos(dl)),
                                   math.sin(p1)*math.sin(p2)+math.cos(p1)*math.cos(p2)*math.cos(dl)))
def prunes(dec0,r,dra,cos0): return dra*cos0 > r*1.2
best=[]
for dec0 in [x*0.5 for x in range(0,179)]:
    cos0=math.cos(math.radians(dec0))
    if cos0<0.01: continue
    for r in [y*0.25 for y in range(1,41)]:   # 半径 0.25..10 度 (FOV<=20 度)
        # 叶在 dec 上限 dec_leaf，RA 距 dra
        for dl in range(1,720):
            dra=float(dl)
            if not prunes(dec0,r,dra,cos0): continue
            # 找 dec_leaf 使 (dra,dec_leaf) 角距 < r
            lo,hi=dec0,89.999
            s_lo=sep(0.0,dec0,dra,lo); s_hi=sep(0.0,dec0,dra,hi)
            if min(s_lo,s_hi)>=r: continue
            dmin=max(lo,hi)-0.0
            best.append((r,dec0,dra,min(s_lo,s_hi)))
            break
best.sort(key=lambda t:t[0])
print("最小圆锥半径的假阴性组合 (r, dec0, d_ra, 叶最近角距):")
seen=set()
for t in best:
    k=round(t[0],2)
    if k in seen: continue
    seen.add(k)
    print("   r=%.2f° dec0=%.1f° d_ra=%.0f° 叶最近角距=%.3f°"%(t[0],t[1],t[2],t[3]))
    if len(seen)>8: break
print()
print("若 FOV<=20° 即 r<=10° 且 |dec|<=45°（极外交给 AE 树），是否有反例：")
sub=[t for t in best if t[0]<=10 and t[1]<=45]
print("   反例条数 =",len(sub), (sub[:3] if sub else ''))
