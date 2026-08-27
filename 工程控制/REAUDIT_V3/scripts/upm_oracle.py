#!/usr/bin/env python3
"""Independent pure-Python UPM oracle (Control 10.1/10.2).
No numpy. Implements y = M_k + C_fk, per-component gauge C_ref=0,
Huber IRLS on standardized residual z = r/sigma_eff with dimensionless delta.
"""
import math, json, os

def huber_w(z, delta):
    az = abs(z)
    if az <= delta:
        return 1.0
    return delta / az

def solve_upm(obs, K, F, ref_frames, sigma_floor=1e-3, delta=1.345, max_iter=60, anchor=0.0):
    """obs: list of (f, k, value, sigma). Coordinate descent like upm.cpp."""
    M = [0.0]*K
    C = [[0.0]*K for _ in range(F)]
    by_frame = [[] for _ in range(F)]
    for (f, k, v, s) in obs:
        by_frame[f].append((k, v, s))
    for it in range(max_iter):
        W = {}
        for (f, k, v, s) in obs:
            r = v - M[k] - C[f][k]
            se = max(abs(s), sigma_floor)
            W[(f, k)] = huber_w(r / se, delta)
        for k in range(K):
            num = den = 0.0
            for rf in ref_frames:
                for (k2, v, s) in by_frame[rf]:
                    if k2 != k:
                        continue
                    w = W[(rf, k2)]
                    num += w * (v - C[rf][k2])
                    den += w
            if den > 1e-12:
                M[k] = num / den
        for f in range(F):
            if f in ref_frames:
                for k in range(K):
                    C[f][k] = 0.0
                continue
            rhs = [0.0]*K
            diag = [0.0]*K
            for (k, v, s) in by_frame[f]:
                w = W[(f, k)]
                rhs[k] += w * (v - M[k])
                diag[k] += w
            for k in range(K):
                d = diag[k] + anchor
                C[f][k] = rhs[k] / d if d > 1e-12 else 0.0
    return M, C

print("=== 10.1 constant field raw=C (no noise, same coverage) ===")
C_val = 1000.0
F, K = 3, 2
obs = [(f, k, C_val, 1.0) for f in range(F) for k in range(K)]
M, C = solve_upm(obs, K, F, [0])
print("M =", [round(m, 6) for m in M])
for f in range(F):
    print("frame", f, "C_f =", [round(c, 6) for c in C[f]])
print("verdict: M=C, C_f=0 (reference-frame gauge) => science doc 'C_f same constant' CONTRADICTED")

print()
print("=== 10.2 Huber: source (z=r/sigma_eff, delta=1.345) vs doc (delta=1.345*median_abs_r) ===")
F2, K2 = 2, 1
obs2 = [(0, 0, 500.0, 5.0)]
for _ in range(20):
    obs2.append((1, 0, 500.0, 5.0))
obs2.append((1, 0, 3000.0, 1.0))
M2, C2 = solve_upm(obs2, K2, F2, [0])
res = sorted(abs(o[2] - 500.0) for o in obs2)
med = res[len(res)//2] if len(res) % 2 else (res[len(res)//2-1] + res[len(res)//2]) / 2
print("source: M =", round(M2[0], 4), " C_f1 =", round(C2[1][0], 4))
print("median_abs_r =", round(med, 4), " doc_delta =", round(1.345*med, 4), " source_delta = 1.345 (dimensionless)")
print("doc delta formula (1.345*median_abs_r) is NOT what the source uses => CONTRADICTED")

out = {
    "10.1_constant_field": {"M": M, "C_f": C,
        "verdict": "M=C, C_f=0; science doc states C_f=C -> contradiction confirmed numerically"},
    "10.2_huber": {"source_M": M2[0], "source_C_f1": C2[1][0],
        "median_abs_r": med, "doc_delta": 1.345*med, "source_delta": 1.345,
        "verdict": "doc delta=1.345*median_abs_r not in source; source uses dimensionless delta on z=r/sigma_eff"},
}
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
p = os.path.join(ROOT, "package", "08_science_oracles", "upm_oracle_pure_python.json")
os.makedirs(os.path.dirname(p), exist_ok=True)
open(p, "w").write(json.dumps(out, indent=2))
print("oracle json saved")

print()
print("=== 10.4 Drizzle constant-field S=F/D numeric check ===")
# simple model: N source pixels, each contributes overlap o_i over drop area A
# doc: w = overlap/drop_area, F = sum(x*w), D = sum(overlap), S = F/D
A = 2.0  # drop_area (px^2 or sr)
overlaps = [0.8, 0.9, 1.0, 1.1, 1.2]  # overlap areas
C = 100.0
F = sum(C * (o / A) for o in overlaps)
D = sum(overlaps)
S = F / D
print("x=C=%.1f, drop_area A=%.1f, S=F/D=%.4f, C/A=%.4f, C=%.1f" % (C, A, S, C/A, C))
print("S == C? ", abs(S - C) < 1e-9, " (only if A==1 or equal-area normalized grid)")
verdict10_4 = "S = C/A, not C: doc constant-field invariant S=C is dimensionally inconsistent with S=F/D when D is area"
out["10.4_drizzle_constant_field"] = {"drop_area": A, "S": S, "C_over_A": C/A, "C": C,
    "verdict": verdict10_4}
open(os.path.join(ROOT, "package", "08_science_oracles", "upm_oracle_pure_python.json"), "w").write(json.dumps(out, indent=2))
print("oracle json re-saved with 10.4")
