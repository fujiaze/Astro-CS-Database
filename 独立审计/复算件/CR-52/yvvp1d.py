import sys, math
sys.stdout.reconfigure(encoding='utf-8')
# 只取 yvv.py 里 factors 的定义段（避免执行其测试主体）
_ns = {'math': math, 'sys': sys}
exec(open('yvv.py', encoding='utf-8').read().split("# 参考")[0], _ns)
factors = _ns['factors']

def filt_1d(sig, sigma):
    """转录 sdet_image.cpp:241-263 的水平单趟（前向+M 边界+反向），1D 版本"""
    N = len(sig)
    B, b1, b2, b3, M = factors(sigma)
    t = [0.0]*N
    t[0] = sig[0]*(B+b1+b2+b3)
    t[1] = B*sig[1] + b1*t[0] + sig[0]*(b2+b3)
    t[2] = B*sig[2] + b1*t[1] + b2*t[0] + b3*sig[0]
    for x in range(3, N):
        t[x] = B*sig[x] + b1*t[x-1] + b2*t[x-2] + b3*t[x-3]
    sW = sig[N-1]
    t1 = sW + M[0][0]*(t[N-1]-sW) + M[0][1]*(t[N-2]-sW) + M[0][2]*(t[N-3]-sW)
    t2 = sW + M[1][0]*(t[N-1]-sW) + M[1][1]*(t[N-2]-sW) + M[1][2]*(t[N-3]-sW)
    t3 = sW + M[2][0]*(t[N-1]-sW) + M[2][1]*(t[N-2]-sW) + M[2][2]*(t[N-3]-sW)
    ow2, ow3 = t[N-2], t[N-3]
    t[N-1] = t1
    t[N-2] = B*ow2 + b1*t[N-1] + b2*t2 + b3*t3
    t[N-3] = B*ow3 + b1*t[N-2] + b2*t[N-1] + b3*t2
    for x in range(N-4, -1, -1):
        t[x] = B*t[x] + b1*t[x+1] + b2*t[x+2] + b3*t[x+3]
    return t

def exact_g(sig, sigma, R=None):
    N = len(sig); R = R or int(math.ceil(8*sigma))
    k = [math.exp(-0.5*d*d/(sigma*sigma)) for d in range(-R, R+1)]
    s = sum(k); k = [v/s for v in k]
    def mf(i):
        if i < 0: i = -i-1
        if i >= N: i = 2*N-i-1
        return sig[max(0, min(N-1, i))]
    return [sum(mf(x+i-R)*k[i] for i in range(len(k))) for x in range(N)]

N = 121; xc = 60
print("== 测试 1：单位脉冲 (x=60, N=121) 的 1D 响应 ==")
for sigma in (2.0, 4.0):
    sig = [0.0]*N; sig[xc] = 1.0
    got = filt_1d(sig, sigma)
    ref = exact_g(sig, sigma)
    # 关于 x=60 的对称性
    asym = max(abs(got[xc+d]-got[xc-d]) for d in range(1, 30))
    peak = max(got)
    err = max(abs(got[i]-ref[i]) for i in range(N))
    # 半质量中心偏移（对称性错误的另一种度量）
    m0 = sum(got); m1 = sum(i*got[i] for i in range(N))/m0
    print(f" sigma={sigma}: 响应峰={peak:.6f} (精确峰={ref[xc]:.6f})  "
          f"关于 x=60 的不对称度={asym:.3e} ({100*asym/peak:.3f}% of peak)  "
          f"质心=xc+{m1-xc:+.3e}  max|IIR-镜像精确FIR|={err:.3e} ({100*err/ref[xc]:.2f}% of exact peak)")

print("== 测试 2：宽平滑隆起（低曲率，检验内部逼近精度） N=241, bump sigma=30 ==")
N2 = 241
sig = [1000.0*math.exp(-0.5*((i-120)/30.0)**2) + 50.0 for i in range(N2)]
for sigma in (2.0,):
    got = filt_1d(sig, sigma); ref = exact_g(sig, sigma)
    interior = range(20, N2-20)   # 距边 >=20px，边界效应可忽略
    e_in = max(abs(got[i]-ref[i]) for i in interior)
    e_edge = max(abs(got[i]-ref[i]) for i in list(range(0,6))+list(range(N2-6,N2)))
    print(f" sigma={sigma}: 内部最大偏差={e_in:.4f} ADU ({100*e_in/1050:.3f}% of 峰值)  "
          f"最外 6 像素偏差={e_edge:.4f} ADU ({100*e_edge/1050:.3f}%)")
    asym = max(abs(got[i]-got[N2-1-i]) for i in range(N2))
    print(f"   输入严格镜像对称，输出不对称度={asym:.4e} ADU")

print("== 测试 3：因果/反对称检验 —— 单边阶跃 (左 0 右 1000, N=121, 阶跃在 60) ==")
sig = [0.0]*60 + [1000.0]*61
got = filt_1d(sig, 2.0); ref = exact_g(sig, 2.0)
print(" 中心 3 点 IIR =", ["%.2f" % v for v in got[59:62]], " 精确 =", ["%.2f" % v for v in ref[59:62]])
print(" 左边界 IIR[0..3] =", ["%.2f" % v for v in got[0:4]], " 精确 =", ["%.2f" % v for v in ref[0:4]])
