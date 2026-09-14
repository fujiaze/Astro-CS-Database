# -*- coding: utf-8 -*-
import math
DEG2RAD=math.pi/180.0
def d_ra_of(ra,ra_min,ra_max):
    if ra<ra_min: d=ra_min-ra
    elif ra>ra_max: d=ra-ra_max
    else: d=0.0
    return 360-d if d>180 else d
def bbox_intersects(ra,dec,radius,ra_min,ra_max,dec_min,dec_max):
    if dec+radius<dec_min or dec-radius>dec_max: return 0
    d=d_ra_of(ra,ra_min,ra_max)
    cos=math.cos(dec*DEG2RAD)
    if cos<0.01: return 1
    if d*cos > radius*1.2: return 0
    return 1
def sep(ra1,d1,ra2,d2):
    p1,p2=math.radians(d1),math.radians(d2); dl=math.radians(ra2-ra1)
    return math.degrees(math.atan2(math.hypot(math.cos(p2)*math.sin(dl),math.cos(p1)*math.sin(p2)-math.sin(p1)*math.cos(p2)*math.cos(dl)),
                                   math.sin(p1)*math.sin(p2)+math.cos(p1)*math.cos(p2)*math.cos(dl)))
cases=[(0.0,70.0,20.0,80.0,85.0,88.0,89.0),
       (0.0,70.0,20.0,76.0,80.0,87.0,89.0),
       (0.0,60.0,30.0,100.0,110.0,85.0,89.0),
       (0.0,75.0,16.0,60.0,70.0,89.0,90.0),
       (0.0,50.0,40.0,130.0,140.0,80.0,88.0)]
print("反例搜索（逐字复刻 gaia_client.c:914-925 bbox_intersects；查询=以 (ra,dec) 为心 radius 为半角的圆锥）")
for ra,dec,r,rmin,rmax,dmin,dmax in cases:
    got=bbox_intersects(ra,dec,r,rmin,rmax,dmin,dmax)
    dra=d_ra_of(ra,rmin,rmax); cos=math.cos(math.radians(dec))
    best=1e9;bp=None;N=240
    for i in range(N+1):
        x=rmin+(rmax-rmin)*i/N
        for j in range(N+1):
            y=dmin+(dmax-dmin)*j/N
            s=sep(ra,dec,x,y)
            if s<best: best=s;bp=(x,y)
    print(f"  q=({ra},{dec},r={r}) leaf ra {rmin}..{rmax} / dec {dmin}..{dmax}")
    print(f"     d_ra={dra:.4f}  d_ra*cos(dec)={dra*cos:.4f}  1.2r={1.2*r:.4f}  -> 谓词={got}")
    print(f"     叶内最近点角距={best:.4f}° @ ({bp[0]:.3f},{bp[1]:.4f})  {'*** 假阴性=漏星 ***' if (got==0 and best<r) else '一致'}")
print()
print("对照 docs/algorithms/GAIA_QUERY.md:143『赤道带 bbox 1.2 裕量 | 经验保守（差分验证），非数学证明』")
print("      gaia_client.h:9   『裕量 1.2 保持无假阴性』")
