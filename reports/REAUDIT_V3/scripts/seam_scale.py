import json, math
B = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/"
m2 = json.load(open(B + "fatduck_2f_upm.json"))
m26 = json.load(open(B + "fatduck_26f_upm.json"))
TARGET = 6464786466758008003  # the common non-reference frame (f11)
def frame_c(frames, C, target):
    for i, f in enumerate(frames):
        if f == target:
            return {k: v for k, v in C[i]}
    return None
c2 = frame_c(m2["frames"], m2["C"], TARGET)
c26 = frame_c(m26["frames"], m26["C"], TARGET)
print("2f: C entries for shared frame =", len(c2), " 26f: =", len(c26) if c26 else "N/A")
if c2 and c26:
    shared = set(c2) & set(c26)
    print("shared control indices:", len(shared))
    diffs = [abs(c26[k] - c2[k]) for k in shared]
    print("max|dC| over shared controls = %.6f" % max(diffs))
    print("mean|dC| = %.6f" % (sum(diffs)/len(diffs)))
    # count large diffs
    big = sum(1 for d in diffs if d > 1e-4)
    print("controls where |dC|>1e-4:", big, "/", len(shared))
    # ratio: how big relative to 2f C scale
    scale2 = max(abs(v) for v in c2.values())
    print("max|C2f| = %.6f, max|dC|/max|C2f| = %.4f" % (scale2, max(diffs)/scale2 if scale2 else 0))
