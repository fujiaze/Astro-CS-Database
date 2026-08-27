import re
p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/drizzle_reverse_probe.c"
s = open(p).read()
s = s.replace("const int nside = 16;", "const int nside = 4;")
s = s.replace("int W = 256, H = 256;", "int W = 64, H = 64;")
s = s.replace("in.crval[0] = 0.0; in.crval[1] = 0.0;", "in.crval[0] = 30.0; in.crval[1] = 10.0;")
s = s.replace("in.crpix[0] = (W + 1) / 2.0; in.crpix[1] = (H + 1) / 2.0;", "in.crpix[0] = 32.5; in.crpix[1] = 32.5;")
s = s.replace("in.cd[0] = 0.1; in.cd[1] = 0.0;   /* roughly 0.1 deg/px */\n    in.cd[2] = 0.0; in.cd[3] = 0.1;", "in.cd[0] = 0.05; in.cd[1] = 0.0; /* 0.05 deg/px */\n    in.cd[2] = 0.0; in.cd[3] = 0.05;")
open(p, "w").write(s)
print("patched to small config")
