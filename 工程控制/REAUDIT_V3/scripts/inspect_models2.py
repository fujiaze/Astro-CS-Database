import json
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m2 = json.load(open(B + "fatduck_2f_upm.json"))
c = m2["C"]
print("C len:", len(c))
for i in range(len(c)):
    ci = c[i]
    print(f"C[{i}] len={len(ci)} sample={ci[:4] if isinstance(ci,list) and len(ci) else ci}")
# maybe C is dict keyed?
print("cell_index sample:", m2["cell_index"][:4])
print("frame_component:", m2.get("frame_component"))
print("component_ref_frame:", m2.get("component_ref_frame"))
