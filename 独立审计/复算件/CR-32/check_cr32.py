"""CR-32 独立复算：dpsf_image.cpp 3x3 中值网络 + dpsf_psf.cpp 常数自洽性。
不导入仓库任何代码，只逐字复刻被审表达式。"""
import itertools, math, random, sys
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

# ---- 1. dpsf_image.cpp:74-82 的 9 元素中值网络（逐字复刻 SW 序列） ----
NET = [(1,2),(4,5),(7,8), (0,1),(3,4),(6,7), (1,2),(4,5),(7,8),
       (0,3),(5,8),(4,7), (3,6),(1,4),(2,5), (4,7),(4,2),(6,4), (4,2)]

def run_net(p, net=NET):
    p = list(p)
    for a, b in net:
        if p[a] > p[b]:
            p[a], p[b] = p[b], p[a]
    return p

bad_perm = 0
for perm in itertools.permutations(range(9)):
    if run_net(perm)[4] != 4:
        bad_perm += 1
print("[1a] 全 9! 排列: p[4] != 中位 的排列数 =", bad_perm)

bad_val = 0
tot_val = 0
for combo in itertools.product((0, 1, 2), repeat=9):
    got = run_net(combo)[4]
    want = sorted(combo)[4]
    tot_val += 1
    if got != want:
        bad_val += 1
print("[1b] 3^9 多重集(含并列): 中值错判数 = %d / %d" % (bad_val, tot_val))
if bad_val:
    ex = next(c for c in itertools.product((0,1,2), repeat=9) if run_net(c)[4] != sorted(c)[4])
    print("     首个反例:", ex, "网络给出", run_net(ex)[4], "真中位", sorted(ex)[4])

# 对照：文献里常见的 median-of-9 网络（PSF.md/README 未记录任何一份）
CLASSIC = [(1,2),(4,5),(7,8), (0,1),(3,4),(6,7), (1,2),(4,5),(7,8),
           (3,6),(4,7),(5,8), (0,3),(4,7),(1,4), (2,5),(5,2),(6,4), (5,2)]
bad_c = sum(1 for perm in itertools.permutations(range(9)) if run_net(perm, CLASSIC)[4] != 4)
bad_ct = sum(1 for c in itertools.product((0,1,2), repeat=9) if run_net(c, CLASSIC)[4] != sorted(c)[4])
print("[1c] 经典变体对照: 排列错 %d / 多重集错 %d" % (bad_c, bad_ct))

# ---- 2. 常数自洽 ----
print("\n[2a] MAD 一致性常数 1/Phi^-1(0.75):")
from statistics import NormalDist
nd = NormalDist()
print("     1/0.75 分位 =", 1.0/nd.inv_cdf(0.75), " 代码值 1.482602218505602")

print("[2b] Moffat4 FWHM 因子: 精确 =",
      2*math.sqrt(2)*math.sqrt(2**0.25-1), " 代码 1.230310  高斯对照 2.3548 =",
      2*math.sqrt(2*math.log(2)))

# flux 解析积分 2*pi*A*sx*sy/3 数值验证（被审式: dpsf_psf.cpp:429）
def num_flux(sx, sy, A=1.0, R=4000.0, n=4000):
    import math
    s = 0.0
    dr = R/n
    th = 64
    for i in range(n):
        r = (i+0.5)*dr
        p1 = 1/(2*sx*sx); p3 = 1/(2*sy*sy)
        # 角度积分用椭圆半径 r_e(ang) = r / sqrt(cos^2/sx^2*... ) —— 直接数值二重积分
    return None
# 直接二重积分（θ=0）
def flux2d(sx, sy, half=60.0, step=0.01):
    import math
    tot = 0.0
    n = int(2*half/step)
    for iy in range(n):
        y = -half + (iy+0.5)*step
        for ix in range(n):
            x = -half + (ix+0.5)*step
            Q = x*x/(2*sx*sx) + y*y/(2*sy*sy)
            tot += 1.0/(1.0+Q)**4
    return tot*step*step
sx, sy = 2.0, 1.3
ana = 2*math.pi*1.0*sx*sy/3.0
print("[2c] flux: 解析 2πA·sxsy/3 =", round(ana, 6), " 数值二重积分 =", round(flux2d(sx, sy), 6))

# ---- 3. bkg0 估计里 1.4826 的适用域：mad_lh 在真高斯样本上不再是 sigma/1.4826 ----
random.seed(12345)
for m in (81, 121, 289, 1024):
    thr_over_sigma = []
    for _ in range(400):
        xs = sorted(random.gauss(0, 1) for _ in range(m))
        med = (xs[m//2] if m % 2 else (xs[m//2-1]+xs[m//2])/2)
        lh = [v for v in xs if v < med]
        n = len(lh)
        mlh = (lh[n//2] if n % 2 else (lh[n//2-1]+lh[n//2])/2)
        dev = sorted(abs(v-mlh) for v in lh)
        madlh = (dev[n//2] if n % 2 else (dev[n//2-1]+dev[n//2])/2)
        thr_over_sigma.append(2.0*1.482602218505602*madlh)
    thr_over_sigma.sort()
    print("[3] m=%4d  threshold = %.3f sigma (中位), 名义 2.0 sigma ⇒ 实际是标称的 %.0f%%"
          % (m, thr_over_sigma[len(thr_over_sigma)//2],
             100*thr_over_sigma[len(thr_over_sigma)//2]/2.0))

# ---- 4. 10-90% 截尾均值的有限-m 偏（对照 0.7316727929211932） ----
for m in (81, 121, 289, 1024):
    acc = []
    for _ in range(200):
        rs = sorted(abs(random.gauss(0, 1)) for _ in range(m))
        lo, hi = int(m*0.1), int(m*0.9)
        acc.append(sum(rs[lo:hi])/(hi-lo))
    acc.sort()
    v = acc[len(acc)//2]
    print("[4] m=%4d  E[截尾均值|r|] 实测 = %.6f  vs 常数 0.731673  ⇒ σ̂ 偏 %+0.2f%%"
          % (m, v, 100*(0.7316727929211932/v - 1)))
