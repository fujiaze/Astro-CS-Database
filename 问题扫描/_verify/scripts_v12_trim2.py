# -*- coding: utf-8 -*-
import math, random
from statistics import NormalDist
nd=NormalDist()
# 口径 A: 对 |r| 的 10-90 百分位截尾均值（半正态分位）
def A(a=0.10,b=0.90):
    phi=lambda x: math.exp(-x*x/2)/math.sqrt(2*math.pi)
    lo=nd.inv_cdf((a+1)/2); hi=nd.inv_cdf((b+1)/2)
    return 2*(phi(lo)-phi(hi))/(b-a)
# 口径 B: 对带号 r 取 |r|<=q(0.9 of signed central 80%) 的截尾（等价于 ±1.2816σ）
def B():
    phi=lambda x: math.exp(-x*x/2)/math.sqrt(2*math.pi)
    z=nd.inv_cdf(0.90)
    return 2*(phi(0)-phi(z))/0.80
print("口径A 半正态10-90截尾均值 =", repr(A()))
print("口径B ±1.2816 截尾       =", repr(B()))
print("在册 16 位              =", repr(0.7316727929211932))
print("docs 7 位               =", repr(0.7316728))
print("A - 在册 rel =", (0.7316727929211932-A())/A())
print("docs7 - 在册 rel =", (0.7316728-0.7316727929211932)/0.7316727929211932)
# 蒙特卡洛核对口径 A
random.seed(12345)
N=2_000_000
vals=sorted(abs(random.gauss(0,1)) for _ in range(N))
lo=vals[int(0.10*N)]; hi=vals[int(0.90*N)]
mid=vals[int(0.10*N):int(0.90*N)]
tm=sum(mid)/len(mid)
print("MC(N=2e6, 对|r|去10%/10%) 截尾均值 =", tm, " 10%/90% 分位点:",lo,hi)
print("相对偏差 vs 口径A =", (tm-A())/A())
print()
print("== 换算影响: sigma = residual_scale / c ==")
for c,lab in [(0.7316727929211932,"在册16位"),(0.7316728,"docs7位"),(A(),"解析A")]:
    print(f"  {lab}: rs=1000 -> sigma={1000/c:.9f}")
print("  在册 vs 解析 相对 sigma 差 =", (1000/0.7316727929211932-1000/A())/(1000/A()))
