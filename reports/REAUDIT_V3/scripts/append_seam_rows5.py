import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "02_historical_seam", "seam_semantic_diff.csv")
rows = list(csv.DictReader(open(P, encoding="utf-8")))
keys = list(rows[0].keys())
A = "b38b446e63d0d27eac672b85ce30527399a057fc"
B = "83471979a1dd778b4e557a9c7a92e22c137107f3"
C = "535e73879662346ee1f599d7a9cae96c6c23680d"
new_row = [A + "|" + B + "|" + C, "lib/phase2/include/astro/phase2/rejection.h", "API surface: rejection module grew from 1 export (B) to 7 (C)",
 "api_surface_expansion",
 "B exposed ONLY p2_reject_stack (compat adapter). C adds p2_reject_plan_resolve, p2_rejection_semantic_id, p2_eligibility_filter, p2_collect_candidate_stack, p2_reject_stack_ex (explicit plan kernel), p2_large_scale_apply - the whole eligibility/plan/semantic-id/large-scale architecture is C-era",
 "VERIFIED via git show of rejection.h at A/B/C; A/B have the header but B lists only p2_reject_stack under P2_API",
 "MAJOR seam-relevant API expansion: the single-path eligibility + explicit-plan + semantic-id architecture (and large-scale clip) does not exist at A/B; any A/B vs C comparison exercises a fundamentally different rejection stack",
 "SEAM_RELEVANT_REJECTION_ARCH_C_ONLY",
 "rejection architecture differs across anchors",
 "git show rejection.h at 3 anchors; executable oracles 10.13-10.15/10.23/10.26 are C-only APIs"],
rows.append(dict(zip(keys, new_row)))
with open(P, "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=keys); w.writeheader(); w.writerows(rows)
print("rows:", len(rows))
