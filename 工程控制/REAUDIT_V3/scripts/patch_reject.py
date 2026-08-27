import re
p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/reject_probe.c"
s = open(p).read()
s = s.replace("in.method = 0; // P2_REJECT_SIGMA", "in.method = 1; // P2_REJECT_SIGMA")
s = s.replace("in2.values = v2; in2.count = 2; in2.method = 0;", "in2.values = v2; in2.count = 2; in2.method = 1;")
open(p, "w").write(s)
print("patched to P2_REJECT_SIGMA=1")
