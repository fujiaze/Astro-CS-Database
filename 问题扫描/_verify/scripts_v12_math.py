import math, struct
from statistics import NormalDist
q = NormalDist().inv_cdf(0.75)
ref = 1.0/q
def show(name, v):
    d = v-ref
    print(f"{name:24s} = {v!r:26s} absdiff={d:+.6e} reldiff={d/ref:+.6e} ulp={abs(d)/math.ulp(ref):.3f}")
print("Phi^-1(3/4) =", repr(q))
show("ref 1/Phi^-1(3/4)", ref)
show("frozen15 1.482602218505602", 1.482602218505602)
show("k4 1.4826", 1.4826)
show("k11 1.4826022185", 1.4826022185)
show("1/0.6745", 1/0.6745)
show("float32(1.4826)", struct.unpack('f', struct.pack('f',1.4826))[0])
show("float32(1.482602218505602)", struct.unpack('f', struct.pack('f',1.482602218505602))[0])
show("0.7316728", 0.7316728)
# trimmed-mean-to-sigma reference
print()
print("== 场景量级：σ_res 门 0.2 ==")
g_old = 0.2/1.4826
g_new = 0.2/1.482602218505602
print("用 1.4826 时等价 σ/A 阈 =", g_old, " 用 15 位 =", g_new, " 相对差", (g_old-g_new)/g_new)
print()
print("== 0.6745 倒数 vs 冻结 15 位：MAD/0.6745 与 1.482602218505602*MAD 的相对差 ==")
a = 1/0.6745
print("rel =", (a-1.482602218505602)/1.482602218505602)
print("在 5σ 门 (sigma_low=5) 上，等效阈差 =", 5*((a-1.482602218505602)/1.482602218505602))
print()
print("== Moffat4 FWHM factor ==")
t = 2*math.sqrt(2*(2**0.25-1))
print("exact 2*sqrt(2*(2^(1/4)-1)) =", repr(t))
print("1.230310 rel diff =", (1.230310-t)/t)
print("1.2303   rel diff =", (1.2303-t)/t)
print("1.230308 rel diff =", (1.230308-t)/t)
print("在 FWHM=3px 上 sigma 偏差(px) =", 3/1.230310 - 3/t)
