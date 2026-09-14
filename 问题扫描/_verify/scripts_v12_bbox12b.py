# -*- coding: utf-8 -*-
import math
def cone_max_dRA(r_deg, dec0_deg, n=200000):
    r=math.radians(r_deg); d0=math.radians(dec0_deg)
    sd0,cd0=math.sin(d0),math.cos(d0); cr,sr=math.cos(r),math.sin(r)
    best=0.0; bdec=0.0; bra=0.0
    for k in range(n+1):
        th=2*math.pi*k/n
        s=math.asin(max(-1.0,min(1.0, sd0*cr+cd0*sr*math.cos(th))))
        da=math.atan2(math.sin(th)*sr*cd0, cr-sd0*math.sin(s))
        a=abs(math.degrees(da))
        if a>best: best=a; bdec=math.degrees(s); bra=math.degrees(da)
    return best,bdec,bra
def angsep(ra1,d1,ra2,d2):
    p1,p2=math.radians(d1),math.radians(d2); dl=math.radians(ra2-ra1)
    return math.degrees(math.atan2(math.hypot(math.cos(p2)*math.sin(dl), math.cos(p1)*math.sin(p2)-math.sin(p1)*math.cos(p2)*math.cos(dl)), math.sin(p1)*math.sin(p2)+math.cos(p1)*math.cos(p2)*math.cos(dl)))
print("== 自检：边界点到中心的角距应 = r ==")
for (d,r) in [(70,20),(0,89),(40,60),(85.9,1)]:
    dm,bdec,bra=cone_max_dRA(r,d,60000)
    print(f"  dec0={d} r={r}: Δαmax={dm:.6f} 处 dec={bdec:.6f} -> 反算角距={angsep(0.0,d,bra,bdec):.9f} (应≈{r})")
print()
print("== 失效边界：给定 dec0，最小使 1.2 裕量失效的 r ==")
for dec0 in [0,5,10,20,30,40,50,60,70,75,80,84,85,86,88,89]:
    if math.cos(math.radians(dec0))<0.01: 
        print(f"  dec0={dec0}: cos<0.01 → 走保守不剪枝分支"); continue
    lo,hi=0.05,90.0
    for _ in range(40):
        mid=(lo+hi)/2
        dm,_,_=cone_max_dRA(mid,dec0,20000)
        if dm > 1.2*mid/math.cos(math.radians(dec0)): hi=mid
        else: lo=mid
    print(f"  dec0={dec0:5.1f}  cos={math.cos(math.radians(dec0)):.5f}  r_crit≈{hi:.4f}°  (r>此值 1.2 裕量数学不足)")
