import json
p = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/stage2_real_config2.json"
d = json.load(open(p))
d["output"]["hips"] = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/stage2_real_out2/stage2_real2.mosaic.hips"
json.dump(d, open(p, "w"), indent=2)
print("config2 output set")
