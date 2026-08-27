p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/stats_probe.c"
s = open(p).read()
s = s.replace("double mad = p2_stats_mad(vals, 7, med);", "double med2 = 0; double mad = p2_stats_mad(vals, 7, &med2);")
open(p, "w").write(s)
print("patched mad signature (out_median ptr)")
