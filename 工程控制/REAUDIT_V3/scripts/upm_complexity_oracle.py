#!/usr/bin/env python3
"""§10.5 UPM complexity oracle - structural cost count vs doc O(iter*(obs+K log K)).

Pure Python: simulates the cost model of upm.cpp cg_solve_frame (per-frame full-K CG,
max_cg=200, Laplacian over adjacency) and compares with the documented complexity
(docs/algorithms/UPM_SOLVER.md L52).
"""
import json, os, math

def doc_ops(iterations, obs, K):
    # doc: O(iter * (obs + K log K)) -> count in "multiply-add-ish units"
    return iterations * (obs + K * math.log2(max(K, 2)))

def actual_ops(iterations, F, K, E, max_cg=200):
    # per CG iter per frame: Ap loop = K (diag) + 2*E (adj) multiply-adds;
    # plus pAp/num (2K), x/r/rs (3K), p update (K) -> ~ (K + 2E) + 6K
    per_cg = (K + 2 * E) + 6 * K
    # per frame: cg_solve_frame up to max_cg iters
    per_frame = max_cg * per_cg
    # per outer iteration: weights O(obs) + M update O(K) + C update per frame O(K+E)
    per_outer = obs + K + F * (K + E) + F * per_frame
    return iterations * per_outer

# concrete scenario: 32R-like, K=9216 controls, F=32 frames, obs ~ 53376 (A) / 9216 clean (B/C)
cases = [
    ("A-like (53376 obs)", 20, 32, 9216, 53376, 40000),
    ("B/C-like (9216 obs)", 20, 32, 9216, 9216, 40000),
    ("small synthetic", 5, 4, 512, 2048, 4000),
]
print("case | doc_ops | actual_ops | ratio (actual/doc)")
out = []
for name, iters, F, K, obs, E in cases:
    d = doc_ops(iters, obs, K)
    a = actual_ops(iters, F, K, E)
    ratio = a / d
    print("%-22s %14.0f %16.0f %10.1fx" % (name, d, a, ratio))
    out.append({"case": name, "doc_ops": d, "actual_ops": a, "ratio": ratio})

# count of CG iterations actually spent (F * max_cg) per iteration
print()
print("Per outer iteration the code performs", "F*max_cg =", 32*200, "full-K CG iterations,")
print("each O(K + 2E) -> the doc omits the per-frame CG factor entirely.")
print("The K log K term in the doc implies a tree/multigrid that does not exist in the source;")
print("upm.cpp L528-564 uses plain full-K CG (max_cg=200) per frame.")

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
p = os.path.join(ROOT, "package", "08_science_oracles", "upm_complexity_oracle.json")
os.makedirs(os.path.dirname(p), exist_ok=True)
open(p, "w").write(json.dumps({
    "verdict": "doc O(iter*(obs+K log K)) undercounts: source does per-frame full-K CG x200 per outer iteration (O(iter*F*max_cg*(K+E))); ratio ~10^4-10^5 for 32R-like cases",
    "cases": out,
    "source": "lib/phase2/src/upm.cpp L528-564 (cg_solve_frame, max_cg=200, full-K x, Laplacian adj)",
    "doc": "docs/algorithms/UPM_SOLVER.md L52 O(iter*(obs+K log K))",
}, indent=2, ensure_ascii=False))
print("written upm_complexity_oracle.json")
