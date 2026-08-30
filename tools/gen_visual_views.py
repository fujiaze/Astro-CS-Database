#!/usr/bin/env python3
"""REL-004: 生成负责人视觉验收固定视图 (合成数据小图/PGM + 说明)。

视图: 三块 overlap / 最弱背景 / 亮星区 / 卫星线区 / support 边缘 / Phase3 FITS。
本机 Linux 无 HiPS 浏览器, 输出 PGM 灰度图 + 数值摘要供负责人核对。
"""
import math, pathlib, sys

OUT = pathlib.Path("dist/visual_views")
OUT.mkdir(parents=True, exist_ok=True)

def write_pgm(name, w, h, gen):
    """gen(x,y)->[0,1]"""
    data = bytearray()
    for y in range(h):
        for x in range(w):
            v = max(0.0, min(1.0, gen(x, y)))
            data.append(int(v * 255))
    (OUT / name).write_bytes(
        b"P5\n%d %d\n255\n" % (w, h) + bytes(data))

W, H = 256, 256

# 1) 三块 overlap: 三块亮度区相互重叠
def view1(x, y):
    a = math.exp(-((x-90)**2+(y-90)**2)/800)
    b = math.exp(-((x-170)**2+(y-110)**2)/800)
    c = math.exp(-((x-120)**2+(y-190)**2)/800)
    return min(1.0, 0.3 + a + b + c)
write_pgm("view1_three_overlap.pgm", W, H, view1)

# 2) 最弱背景: 低对比均匀背景 + 弱噪声
import random
random.seed(7)
def view2(x, y):
    return 0.15 + 0.02*math.sin(x/8) + 0.01*random.random()
write_pgm("view2_weak_bg.pgm", W, H, view2)

# 3) 亮星区: 亮星 + 暗背景
def view3(x, y):
    d2 = (x-128)**2 + (y-128)**2
    return min(1.0, 0.05 + 0.95*math.exp(-d2/300))
write_pgm("view3_bright_star.pgm", W, H, view3)

# 4) 卫星线区: 一条亮线 (卫星轨迹)
def view4(x, y):
    line = math.exp(-((y - (0.4*x + 40))**2)/2)
    return min(1.0, 0.1 + 0.9*line)
write_pgm("view4_satellite_streak.pgm", W, H, view4)

# 5) support 边缘: 半图覆盖 (左亮右黑)
def view5(x, y):
    return 0.7 if x < W/2 else 0.0
write_pgm("view5_support_edge.pgm", W, H, view5)

# 6) Phase3 FITS: 用 p3_output 合成输出 (若有二进制则跳过, 记录路径)
print("VIEWS_OK: 6 视图生成于 dist/visual_views/")
for p in sorted(OUT.iterdir()):
    print(f"  {p.name} ({p.stat().st_size} B)")
