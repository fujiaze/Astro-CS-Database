p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/elig_probe.c"
s = open(p).read()
s = s.replace("uint8_t el[3]={0,0,0};", "uint8_t el[3]={0,0,0}; uint32_t ec=0;")
s = s.replace("out.eligible_count = &(uint32_t){0};", "out.eligible_count = &ec;")
s = s.replace("uint8_t el2[3]={0,0,0};", "uint8_t el2[3]={0,0,0}; uint32_t ec2=0;")
s = s.replace("out2.eligible_count=&(uint32_t){0};", "out2.eligible_count=&ec2;")
open(p, "w").write(s)
print("patched")
