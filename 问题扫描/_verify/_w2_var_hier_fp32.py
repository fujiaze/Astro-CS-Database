import math, numpy as np

# ---------- S4: hierarchy parent variance missing cross terms ----------
# single input drop j (variance v, area A_j) split across N children of the SAME parent
for N in (2,4):
    w = [1.0/N]*N            # w_jp = a_jp/A_j, sum = 1
    v = 100.0
    # leaf variances: var_p = v*w^2/A^2  (A=A_cell, area per child = w*A... use A=1)
    # code parent: sum(v*w_p^2)/(sum a_p)^2 ; a_p = w_p*A, A=1
    code = sum(v*wp*wp for wp in w)/(sum(w))**2
    true = v*(sum(w))**2/(sum(w))**2     # Var of (sum_j x_j w_j)/D with D=sum w (single frame, full cov)
    print("S4 N=%d children: var_parent(code)=%.3f  true(=Var of same estimator)=%.3f ratio=%.3f"%(N,code,true,code/true))
# two independent frames, one drop each crossing 2 children:
v=100.0; N=2
w=[0.5,0.5]
code = 2*sum(v*wp*wp for wp in w)        # denominator (sum areas=2*0.5*2 children...) normalize later
den = (2*(0.5+0.5))**2
true = ( (0.5+0.5)**2*v + (0.5+0.5)**2*v )
print("S4b 2 frames x 2 children: code=%.3f/%.3f=%.3f ; true=%.3f/%.3f=%.3f ; ratio=%.3f"%(code,den,code/den,true,den,true/den,(code/den)/(true/den)))

# ---------- S5: ivar mosaic sanity ----------
# two equal-weight pixels var=100 -> combined var?
W = 1/100.0 + 1/100.0
print("S5 inverse-variance add: 1/(1/100+1/100)=%.1f (correct 50)"%(1.0/W))

# ---------- S6: FP32 accumulation ----------
# 6a saturation non-additivity
x = np.float32(16777216.0)
print("S6a float32 2^24 + 1.0 ->", np.float32(x + np.float32(1.0)), " (lost)")
# 6b realistic leaf sums: 16 contributions of v*w^2 with w~0.25, v=100
vals = [100.0*(0.15+0.1*(k%5)/4.0)**2 for k in range(16)]
s32 = np.float32(0.0)
for vv in vals: s32 = np.float32(s32 + np.float32(vv))
s64 = sum(vals)
print("S6b 16-contrib leaf sum: f32=%.8f f64=%.8f rel.err=%.2e"%(s32,s64,abs(s32-s64)/s64))
# 6c hierarchy: 2^22 additions of ~25.0 (vnum per leaf at v=100, w~0.5) in float32
N=1<<22; inc=np.float32(25.0); acc=np.float32(0.0)
step=1<<16
# do it in python loop but chunk-accurate: emulate sequential float32 adds
import array
vals32 = np.full(N, 25.0, dtype=np.float32)
# pairwise float32 sum to mimic tree not available; sequential loop on 2^22 is slow; use 2^20 sample and scale analysis
M=1<<20
acc=np.float32(0.0)
b = np.float32(25.0)
for k in range(M):
    acc = np.float32(acc + b)
exact = 25.0*M
print("S6c float32 sequential sum of %d x 25.0: got %.1f exact %.1f rel.err=%.2e"%(M,acc,exact,(acc-exact)/exact))
# once acc passes 2^24*? -> show ulp: value at end
ulp = np.spacing(acc)
print("S6c ulp at end = %.4f (increment was 25.0; when ulp>25 increments quantize)"%ulp)
