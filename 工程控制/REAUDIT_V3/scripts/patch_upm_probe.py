import re
p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/upm_const_probe.c"
s = open(p).read()
s = s.replace('double c0 = p2_upm_evaluate_c(model, f, obs[0].leaf_ipix, 0.0, 0.0);',
              'double c0 = p2_upm_evaluate_c(model, f, obs[0].leaf_ipix);')
open(p, "w").write(s)
print("patched")
