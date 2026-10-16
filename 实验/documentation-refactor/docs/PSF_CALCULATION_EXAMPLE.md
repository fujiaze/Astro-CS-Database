# 闭式与有限样本复算

来源：`docs/science/psf/PSF.md`，整理前 Git blob `e3f6ec61e1db2d7757c0a9b5496cfe72d2e9e623`。以下原文保留观察、假设和复核限制；没有重新运行，不作为当前生产读数或正式门限。引用编号按来源文件参考表解释。

以下自包含代码保留闭式、求积、旋转和截尾统计的复算入口；它是推导示例，不调用生产代码，不构成生产运行或真实数据验证。注释数字为待复核的示例期望，有限样本随机输出依Python实现和浮点环境解释。Moffat轮廓的一般参数命名可与[2]对照；本页固定β=4的口径由正文定义。

```bash
python3 - <<'PY'
import math, random

def Phi_inv(p):                       # 标准正态分位点，二分法
    lo, hi = -12.0, 12.0
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if 0.5 * (1 + math.erf(mid / math.sqrt(2))) < p: lo = mid
        else: hi = mid
    return 0.5 * (lo + hi)

phi = lambda x: math.exp(-0.5*x*x) / math.sqrt(2*math.pi)
a, b = Phi_inv(0.55), Phi_inv(0.95)
print("Phi_inv(0.55)     =", a)                                # 0.12566134685507402
print("Phi_inv(0.95)     =", b)                                # 1.6448536269514715
print("kTrimMeanToSigma  =", 2*(phi(a)-phi(b))/0.8)            # 0.731673095280613
print("FWHM/sigma        =", 2*math.sqrt(2)*math.sqrt(2**0.25-1))   # 1.2303076525901024
print("MAD factor        =", 1/Phi_inv(0.75))                  # 1.4826022185056023

# 解析通量：复合辛普森积分对任意 sx, sy, theta 都应给出 2*pi*A*sx*sy/3
sx, sy, A = 2.3, 3.1, 7.0
th = math.pi/2*(3.0 + math.sqrt(5.0))
p1 = math.cos(th)**2/(2*sx*sx) + math.sin(th)**2/(2*sy*sy)
p2 = math.sin(2*th)/(4*sx*sx) - math.sin(2*th)/(4*sy*sy)
p3 = math.sin(th)**2/(2*sx*sx) + math.cos(th)**2/(2*sy*sy)
N, L = 4000, 30.0
h = 2*L/N
acc = 0.0
for i in range(N+1):
    x = -L + i*h
    wx = 1 if i in (0, N) else (4 if i % 2 else 2)
    for j in range(N+1):
        y = -L + j*h
        wy = 1 if j in (0, N) else (4 if j % 2 else 2)
        acc += wx*wy*A/(1 + p1*x*x + 2*p2*x*y + p3*y*y)**4
num = acc*h*h/9
ana = 2*math.pi*A*sx*sy/3
print("flux (Simpson)    =", num)                              # 104.5309487261
print("flux (analytic)   =", ana)                              # 104.5312595604
print("flux rel diff     =", abs(num-ana)/ana)                 # 2.974e-06

# 旋转简并：固定 sx,sy 时四个候选给出三个不同的二次型；真简并生成元须同时交换两轴
Q = lambda t, sx, sy: (math.cos(t)**2/(2*sx*sx) + math.sin(t)**2/(2*sy*sy),
                       math.sin(2*t)/(4*sx*sx) - math.sin(2*t)/(4*sy*sy),
                       math.sin(t)**2/(2*sx*sx) + math.cos(t)**2/(2*sy*sy))
base = Q(0.37, 3.0, 2.0)
for name, t2 in [("theta", 0.37), ("pi/2-theta", math.pi/2-0.37),
                 ("pi/2+theta", math.pi/2+0.37), ("pi-theta", math.pi-0.37)]:
    print("  candidate %-12s max|dQ| = %.12f" % (name, max(abs(a-b) for a, b in zip(Q(t2, 3.0, 2.0), base))))
print("  generator g (sy,sx,th+pi/2)  max|dQ| = %.3e" %
      max(abs(a-b) for a, b in zip(Q(0.37+math.pi/2, 2.0, 3.0), base)))

# 拟合窗截断：f_out > 3% 的精确阈值，以及 sqrt(2)*sigma 处的取值
r3 = math.sqrt(2*((0.03)**(-1/3) - 1))
print("f_out=3% r/sigma  =", r3)                              # 2.1063228378790524
print("f_out(sqrt2 sigma)=", (1+1)**-3)                        # 0.125
for r in (1.5, 1.8, 2.0):
    print("  f_out(%.1f sigma)          =" % r, (1+r*r/2)**-3)  # 0.1042 / 0.0556 / 0.0370

# 另一口径 sigma_g：beta 一般式，与 FWHM 因子的比值恰为 sqrt(2)
F_sig   = 2*math.sqrt(2)*math.sqrt(2**0.25-1)                  # FWHM / 本册 sigma
F_sig_g = 4*math.sqrt(2**0.25-1)                               # FWHM / 逐轴 sigma_g
print("FWHM/sigma_g      =", F_sig_g)                          # 1.739917768184329
print("FWHM_sigma_g/sqrt2=", F_sig_g/math.sqrt(2))             # 1.2303076525901024
print("sqrt(2)           =", math.sqrt(2))                    # 1.4142135623730951
print("ratio of factors  =", F_sig_g/F_sig)                   # 1.414213562373095
print("sigma_g (beta=4)  =", 1/math.sqrt(2*(4-2)))             # 0.5

# 半高点方程自洽性；跨口径代入确实给不出 0.5，且原路反解只回到被代入的因子
rh = math.sqrt(2*(2**0.25-1))
print("r_half/sigma      =", rh)                               # 0.6151538262950512
print("(1+r^2/2)^-4       =", (1 + rh*rh/2)**-4)                # 0.5000000000000001
r_cross = F_sig_g/2
print("cross-conv r      =", r_cross)                          # 0.8699588840921645
print("cross-conv value  =", (1 + r_cross*r_cross/2)**-4)      # 0.2770007905069461
v_back = (1 + r_cross*r_cross/2)**-4
print("cross-conv back   =", 2*math.sqrt(2*(v_back**(-0.25) - 1)))  # 1.739917768184329

# 面元方向半高全宽：闭式 1.2303076525901024/sqrt(2*p1)，及两端严格界
p1_of = lambda t, sx, sy: math.cos(t)**2/(2*sx*sx) + math.sin(t)**2/(2*sy*sy)
for mj, mn in ((3.1, 2.3), (2.3, 3.1)):
    row = []
    for t in (0.0, 0.30, 0.70, 1.20, math.pi/2):
        fx = F_sig/math.sqrt(2*p1_of(t, mj, mn))
        row.append("%+.2f%%" % (100*(1.230310*mj/fx - 1)))
    print("  sx=%s sy=%s  1.230310*sx/FWHM_x - 1 = %s" % (mj, mn, " ".join(row)))

# 截尾均值偏差随 m mod 10 的符号：标准库蒙特卡洛，种子 20260901+m，每点 20000 次
for m in (8, 10, 11, 12, 20, 25, 30, 101):
    rnd = random.Random(20260901 + m)
    lo, hi = int(m*0.1), int(m*0.9)
    s = sum(sum(sorted(abs(rnd.gauss(0.0, 1.0)) for _ in range(m))[lo:hi])/(hi-lo)
            for _ in range(20000))
    print("  m=%4d (m%%10=%d)  trimmed-mean dev = %+.2f%%" % (m, m % 10, 100*(s/20000/0.7316730952806130 - 1)))
PY
```
