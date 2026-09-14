# -*- coding: utf-8 -*-
import math
def dRA_max(deg_r, deg_dec0, n=36000):
    r=math.radians(deg_r); d0=math.radians(deg_dec0)
    sd0,cd0=math.sin(d0),math.cos(d0)
    cr,sr=math.cos(r),math.sin(r)
    best=0.0; bestdec=None
    for k in range(n+1):
        th=2*math.pi*k/n
        s=math.asin(max(-1.0,min(1.0, sd0*cr+cd0*sr*math.cos(th))))
        da=abs(math.degrees(math.atan2(math.sin(th)*sr*cd0, cr-sd0*math.sin(s))))
        if da>best: best=da; bestdec=math.degrees(s)
    return best,bestdec
worst=None; rows=[]
for dec0 in [0.0,5,10,20,30,40,50,60,70,75,80,84,85,86,87,88,88.5,89,89.5,89.9]:
    for r in [0.01,0.05,0.1,0.2,0.5,1,2,3,5,10,20,30,45,60,80,89]:
        cd0=math.cos(math.radians(dec0))
        if cd0<0.01: continue
        thr=1.2*r/cd0
        dm,dd=dRA_max(r,dec0)
        ratio=dm/thr
        if worst is None or ratio>worst[0]: worst=(ratio,dec0,r,dm,thr,dd)
        if ratio>1.0: rows.append((dec0,r,dm,thr))
print("修正后：最大比 ΔRA_max/(1.2 r/cos dec0) = %.6f  @dec0=%.1f r=%.3g (ΔRA_max=%.6f°, 需 d_ra>%.6f°, 圆上最北/南 dec=%.3f)"%worst)
print("反例条数:",len(rows))
for t in rows[:20]: print("   ",t)
print()
print("对照：极冠保护阈 cos_dec<0.01 → |dec|>%.4f°"%math.degrees(math.acos(0.01)))
for dec0,r in [(0,1),(40,1),(60,1),(80,1),(84,1),(85.9,1),(86.5,0.1),(88,0.05)]:
    cd=math.cos(math.radians(dec0)); dm,_=dRA_max(r,dec0)
    print("  dec0=%5.2f r=%-5g  ΔRA_max=%.6f  1.2r/cos=%.6f  比=%.6f  紧界 r/cos(dec+r)=%.6f"%(dec0,r,dm,1.2*r/cd,dm/(1.2*r/cd), r/math.cos(math.radians(min(89.999,dec0+r)))))
