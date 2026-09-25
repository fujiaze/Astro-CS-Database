import importlib.util, math, numpy as np, sys
sys.stdout.reconfigure(encoding='utf-8')
spec = importlib.util.spec_from_file_location("A", "aud202_recompute.py")
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
P_ = m.moffat4_profile
print("half | double-count bias % at base(F=1000e-,B=100,D=0.5,RN=10,g=1.3) | sum_p2")
for half in (12, 18, 24, 30, 40, 60):
    P = P_(1.5, half)
    F_adu = 1000.0/1.3
    sig_shot = math.sqrt((100.0+0.5)/1.3**2)
    a = m.sigma_f_horne(F_adu, sig_shot, 1.3, 10.0, P, False)
    b = m.sigma_f_horne(F_adu, sig_shot, 1.3, 10.0, P, True)
    print("%5d | %+8.3f | %.8f" % (half, 100*(b/a-1), m.sum_p2_of(P)))
print()
print("grid-dependence of the *no-gain upper bound* inflation (F_e=1000, sig=1.5):")
for half in (12,18,24,30,40,60):
    P = P_(1.5, half); F=F_e=1000.0; g,rn,sky,dark=1.3,10.0,100.0,0.5
    st=math.sqrt((sky+dark)/g**2); tot=math.sqrt((sky+dark+rn**2)/g**2)
    t=m.sigma_f_horne(F/g, st, g, rn, P); ng=tot/math.sqrt(m.sum_p2_of(P))
    print("  half=%3d  SNR_rep/SNR_true=%.4f" % (half, t/ng))
