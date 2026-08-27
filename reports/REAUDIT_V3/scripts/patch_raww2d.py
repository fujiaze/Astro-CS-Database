p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/raww_probe2.c"
s = open(p).read()
s = s.replace('printf("[value=10] civar=%g -> rc=%d raw_w=%.6f\\n", civ, rc2, raw_w); }',
              'printf("[value=10] civar=%g -> rc=%d raw_w=%.6f\\n", civ, rc2, w); }')
open(p, "w").write(s)
print("fixed printf var")
