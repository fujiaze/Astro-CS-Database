p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/raww_probe2.c"
s = open(p).read()
s = s.replace("&raw_w);", "&w);")
open(p, "w").write(s)
print("fixed var name")
