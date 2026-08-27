import json
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m26 = json.load(open(B + "fatduck_26f_upm.json"))
print("frames:", m26["frames"][:3], "...")
print("frame_component:", m26["frame_component"][:6])
# pick non-reference frame 1 (index 1) leaves with nonzero C
c1 = {k: v for k, v in m26["C"][1]}
leaves = []
for idx, ctr in enumerate(m26["controls"]):
    if idx in c1 and abs(c1[idx]) > 1e-5:
        leaves.append(ctr[6])
print("nonzero-C leaves for frame1:", len(leaves))
print("sample leaves:", leaves[:6])
# emit C header
with open(B + "leaf_set.txt", "w") as f:
    f.write("\n".join(str(x) for x in leaves))
print("leaf count written:", len(leaves))
