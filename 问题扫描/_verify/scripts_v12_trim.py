import math
from statistics import NormalDist
nd=NormalDist()
def halfnorm_cdf(x): return 2*nd.cdf(x)-1
def halfnorm_ppf(p): return nd.inv_cdf((p+1)/2)
def trimmed_mean_abs(a,b):
    lo,hi=halfnorm_ppf(a),halfnorm_ppf(b)
    # E[|X| ; a<F<b] = int_a^b x*2phi(x) dx = 2*(phi(lo)-phi(hi))
    phi=lambda x: math.exp(-x*x/2)/math.sqrt(2*math.pi)
    return 2*(phi(lo)-phi(hi))/(b-a)
for a,b,lab in [(0.1,0.9,"10-90%"),(0.0,0.8,"0-80%"),(0.05,0.95,"5-95%")]:
    print(f"{lab} trimmed mean |N(0,1)| =", trimmed_mean_abs(a,b))
print()
code=0.7316727929211932
doc7=0.7316728
tm=trimmed_mean_abs(0.1,0.9)
print("code kTrimMeanToSigma  =", repr(code))
print("docs 0.7316728         =", repr(doc7))
print("真值 10-90% 截尾均值   =", repr(tm))
print("code  vs 真值 rel =", (code-tm)/tm)
print("doc7 vs 真值 rel =", (doc7-tm)/tm)
print("doc7 - code      =", doc7-code, " rel", (doc7-code)/code)
print("1/doc7 (除法式)  =", 1/doc7, " 1/code =", 1/code)
print()
print("== 在 q_psf / robust_residual_sigma 场景 ==")
print("robust_sigma=residual_scale/0.7316728 vs /code 相对差 =", (code-doc7)/doc7)
print("在 sigma=1000 ADU 上绝对差 =", 1000*((code-doc7)/doc7), "ADU")
