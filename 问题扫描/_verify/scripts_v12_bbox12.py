# -*- coding: utf-8 -*-
import math, sys
sys.path.insert(0,"/usr/lib/python3/dist-packages")
def dRA_max(deg_r, deg_dec0, n=72000):
    """小球面(以 (0,dec0) 为心、角半径 r)上 |ΔRA| 的最大值(度)。"""
    r=math.radians(deg_r); d0=math.radians(deg_dec0)
    sd0,cd0=math.sin(d0),math.cos(d0)
    best=0.0; bestdec=None
    for k in range(n+1):
        th=2*math.pi*k/n                 # position angle
        # 沿大圆走距离 r：δ=asin(sin d0 cos r + cos d0 sin r cos th)
        s=math.asin(max(-1.0,min(1.0, sd0*math.cos(r)+cd0*math.sin(r)*math.cos(th))))
        # Δα = atan2( sin th sin r, cos r - sin d0 sin δ )  (标准球面公式)
        da=math.atan2(math.sin(th)*math.sin(r), math.cos(r)-sd0*math.sin(s))
        da=abs(math.degrees(da))
        if da>best: best=da; bestdec=math.degrees(s)
    return best,bestdec
rows=[]
worst=None
for dec0 in [0.0,10,20,30,40,50,60,70,75,80,84,85,86,87,88,88.5,89,89.5,89.9]:
    for r in [0.05,0.1,0.2,0.5,1,2,3,5,10,20,30,45,60,80,89]:
        cd0=math.cos(math.radians(dec0))
        if cd0 < 0.01: continue
        thr=1.2*r/cd0
        dm,_=dRA_max(r,dec0,18000)
        ratio=dm/(1.2*r/cd0)
        if worst is None or ratio>worst[0]:
            worst=(ratio,dec0,r,dm,thr)
        if ratio>1.0:
            rows.append((dec0,r,dm,thr))
print("最危险比 ΔRA_max / (1.2 r / cos dec0) = %.6f  @ dec0=%.1f r=%.3g (ΔRA_max=%.4f°, 剪枝阈 d_ra>%.4f°)"%worst)
print("反例条数:",len(rows))
for t in rows[:12]: print("   dec0=%.1f r=%.3g  ΔRA_max=%.4f  需要 d_ra>%.4f 才剪 -> 会漏"%(t[0],t[1],t[2],t[3]))
