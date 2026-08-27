p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/raww_probe2.c"
s = open(p).read()
# C-style: replace the range-for with an array loop
s = s.replace("for (double civ : {4.0, 16.0, 1.0}) {", "double civs[3] = {4.0, 16.0, 1.0}; for (int i=0;i<3;++i) { double civ = civs[i];")
open(p, "w").write(s)
print("patched range-for")
