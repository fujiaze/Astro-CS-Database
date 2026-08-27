import json
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
def load(f): return json.load(open(B + f))
m2 = load("fatduck_2f_upm.json")
m10 = load("fatduck_10f_upm.json")
m26 = load("fatduck_26f_upm.json")
TARGET = 6464786466758008003  # f11 common
def leaf_map(m, target):
    fi = None
    for i, f in enumerate(m["frames"]):
        if f == target: fi = i
    if fi is None: return {}
    c = {k: v for k, v in m["C"][fi]}
    return {ctr[6]: c[idx] for idx, ctr in enumerate(m["controls"]) if idx in c}
l2 = leaf_map(m2, TARGET); l10 = leaf_map(m10, TARGET); l26 = leaf_map(m26, TARGET)
def cmp(a, b, na, nb):
    sh = set(a) & set(b)
    if not sh: return (na, nb, 0, None, None, None)
    d = [abs(b[k]-a[k]) for k in sh]
    return (na, nb, len(sh), max(d), sum(d)/len(d), max(d)/max(abs(v) for v in a.values()))
print("compare 2f vs 10f:", cmp(l2, l10, len(l2), len(l10)))
print("compare 2f vs 26f:", cmp(l2, l26, len(l2), len(l26)))
print("compare 10f vs 26f:", cmp(l10, l26, len(l10), len(l26)))
