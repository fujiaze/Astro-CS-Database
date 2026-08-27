p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/raww_probe.c"
s = open(p).read()
# The probe's memset(0) + field assignment was correct, but the struct has more fields
# (ivar, snr_available). The production raw_w=1.980198 for ivar=4 means quality_factor=0.495?
# No: qf(flags=0)=0.5 -> 0.5*4=2.0. Got 1.980198. That's 2.0*0.990099? Actually the probe set
# o->quality_flags but memset(0) then assignments - check: probe assigned quality_flags=q (0).
# qf(0) = 0.5. So expect 0.5*4 = 2.0. Got 1.980198 = 2 * 0.990099. Hmm - maybe ivar field vs
# control_ivar confusion: the struct ALSO has 'double ivar' and our probe didn't set it; but
# p2_upm_raw_weight uses obs->control_ivar only. Let me recompute: 1.980198... = 200/101?
# Actually 1.980198 = 2.0 * 0.990099 = 2 * (100/101)? odd.
# Wait - the ablation case gave exactly 1.8 = 0.9/0.25*0.5? no: qf=0.5, sp^1=1, 0.9/(0.25)
# -> 0.5*1*0.9*16=7.2? Got 1.8. So qf must be 0.25? No...
# Simplest explanation: quality_factor(0)=0.5 and support clamped to [0,1] fine...
# 1.8 = 0.9/0.25 * 0.5 = 3.6*0.5. YES! qf=0.5 multiplies in the ABLATION path too:
# raw = qf * sp^p * snr2/(1+snr2)/unc^2 = 0.5*1*0.9/0.25 = 1.8. My 'expect' forgot qf!
# And production: 1.980198? if qf applied twice? 0.5*4=2.0 not 1.98.
# Unless control_ivar got normalized... let me just print with explicit flags=1 (qf=1).
s = s.replace('mkobs(&o, 100.0, 0.5, 4.0 /*ivar*/, 10.0, 1.0, 0);',
              'mkobs(&o, 100.0, 0.5, 4.0 /*ivar*/, 10.0, 1.0, 1);')
s = s.replace('printf("production ivar=4: rc=%d raw_w=%.6f (expect 4.0)\\n", rc, w);',
              'printf("production ivar=4 flags=PSF_OK(qf=1): rc=%d raw_w=%.6f (expect 4.0)\\n", rc, w);')
s = s.replace('double expect = 9.0/10.0 / (0.25);   // snr^2/(1+snr^2) / unc^2 = 0.9/0.25',
              'double expect = 0.5 * (9.0/10.0) / (0.25);   // qf(0)=0.5 * sp * snr2/(1+snr2) / unc^2')
open(p, "w").write(s)
print("patched")
