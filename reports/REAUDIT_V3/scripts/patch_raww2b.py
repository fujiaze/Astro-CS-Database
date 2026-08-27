p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/raww_probe2.c"
s = open(p).read()
# probe3 works with value=10; probe2 has value=100. Toggle ONLY value on the probe2 struct.
s = s.replace("o.value=100.0;", "double vtest[2] = {100.0, 10.0}; (void)vtest; o.value=100.0;")
# add a second pass with value=10
extra = '''
    /* value toggle: does value=100 vs 10 change raw_w? */
    o.value = 10.0;
    for (int i=0;i<3;++i) { double civ = civs[i]; o.control_ivar = civ;
        int rc2 = p2_upm_raw_weight(&o, &cfg, &raw_w);
        printf("[value=10] civar=%g -> rc=%d raw_w=%.6f\\n", civ, rc2, raw_w); }
'''
s = s.replace('    // ivar<=0 refusal check with explicit cfg (not NULL):\n', extra + '    // ivar<=0 refusal check with explicit cfg (not NULL):\n')
open(p, "w").write(s)
print("patched probe2")
