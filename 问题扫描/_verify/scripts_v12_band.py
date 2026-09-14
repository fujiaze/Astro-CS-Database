# -*- coding: utf-8 -*-
import math
def cone_pts(r_deg, dec0_deg, n=6000):
    r=math.radians(r_deg); d0=math.radians(dec0_deg)
    sd0,cd0=math.sin(d0),math.cos(d0); cr,sr=math.cos(r),math.sin(r)
    out=[]
    for k in range(n+1):
        th=2*math.pi*k/n
        s=math.asin(max(-1.0,min(1.0, sd0*cr+cd0*sr*math.cos(th))))
        a=abs(math.degrees(math.atan2(math.sin(th)*sr*cd0, cr-sd0*math.sin(s))))
        out.append((math.degrees(s), a))
    return out
def band_max(r,dec0,B):
    pts=[a for d,a in cone_pts(r,dec0) if abs(d)<=B]
    return max(pts) if pts else 0.0
print("Equirectangular 赤道带 bbox 剪枝：判据 d_ra*cos(dec0) > 1.2*r -> 剪（gaia_client.c:922-923 逐字复刻）")
for B in (45.0, 90.0):
    print(f"=== 假设节点赤纬受限于 |dec|<={B} ===")
    for dec0 in (10,20,30,40,44,50,60,70,80):
        fail=None
        for step in range(1,401):
            r=step*0.25
            if abs(dec0)+r>90: break
            md=band_max(r,dec0,B); thr=1.2*r/math.cos(math.radians(dec0))
            if md>thr: fail=(r,md,thr); break
        if fail: print(f"  dec0={dec0:3d}: 首个失效 r={fail[0]:6.2f}°  锥内最大ΔRA={fail[1]:7.3f}° > 剪枝阈 {fail[2]:7.3f}°")
        else:   print(f"  dec0={dec0:3d}: 至 r=90-|dec0| 无失效")
