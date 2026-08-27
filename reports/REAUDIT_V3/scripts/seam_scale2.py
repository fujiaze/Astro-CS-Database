import json
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m2 = json.load(open(B + "fatduck_2f_upm.json"))
m26 = json.load(open(B + "fatduck_26f_upm.json"))
TARGET = 6464786466758008003
def leaf_map(m, target):
    # controls[i][6] = leaf_ipix ; C[frame][pairs] control_index->value ; control_index = cell_index[...][3] order = controls index
    # cell_index is list of [tile,gx,gy,index] where index is the control position; C pairs use that index
    leaf_by_idx = {}
    # controls list index == control index (per build); but C pairs reference cell_index[..][3]
    # Use cell_index[..][3] as idx, and map idx->leaf via controls[idx]
    ctrls = m["controls"]
    # controls[i] leaf = ctrls[i][6]
    fi = None
    for i, f in enumerate(m["frames"]):
        if f == target: fi = i
    if fi is None: return {}
    c = {k: v for k, v in m["C"][fi]}
    res = {}
    for idx, leaf in enumerate([ctr[6] for ctr in ctrls]):
        if idx in c:
            res[leaf] = c[idx]
    return res
l2 = leaf_map(m2, TARGET)
l26 = leaf_map(m26, TARGET)
print("leaf entries 2f:", len(l2), " 26f:", len(l26))
shared = set(l2) & set(l26)
print("shared leaves:", len(shared))
if shared:
    diffs = [abs(l26[k] - l2[k]) for k in shared]
    print("max|dC| = %.6f" % max(diffs))
    print("mean|dC| = %.6f" % (sum(diffs)/len(diffs)))
    big = sum(1 for d in diffs if d > 1e-4)
    print("|dC|>1e-4:", big, "/", len(shared))
    scale2 = max(abs(v) for v in l2.values())
    print("max|C2f|=%.6f  max|dC|/max|C2f|=%.4f" % (scale2, max(diffs)/scale2 if scale2 else 0))
