#!/usr/bin/env python3
"""Generate seam_semantic_diff.csv v1 from hunk_symbol_index.csv plus verified anchor facts.
Honesty policy: unanalyzed rows keep NOT_YET_ASSESSED markers; only facts verified by
direct inspection of commit content are marked VERIFIED."""
import csv, os

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
HS = os.path.join(ROOT, "package", "02_historical_seam")
A = "b38b446e63d0d27eac672b85ce30527399a057fc"
B = "83471979a1dd778b4e557a9c7a92e22c137107f3"
C = "535e73879662346ee1f599d7a9cae96c6c23680d"

fields = ["commit_from","commit_to","module","file","symbol","change_kind",
          "science_effect","configuration_effect","execution_effect",
          "candidate_regression","reason","evidence"]

NA = "NOT_YET_ASSESSED"

rows = []

# --- verified fact rows (checked directly this round) ---
rows.append({
    "commit_from": B, "commit_to": C, "module": "lib/phase2", "file": "lib/phase2/CMakeLists.txt",
    "symbol": "P2_ENABLE_OPENMP (option + target_link_libraries)",
    "change_kind": "config_key_present_but_macro_never_defined",
    "science_effect": "none_direct",
    "configuration_effect": "VERIFIED: option default OFF (L18); when ON and OpenMP found, only target_link_libraries(OpenMP::OpenMP_CXX) at L56-58; no target_compile_definitions(P2_ENABLE_OPENMP) anywhere",
    "execution_effect": "VERIFIED consequence: source guard '#if defined(P2_ENABLE_OPENMP) && defined(_OPENMP)' (src/sampler.cpp:30) can never activate because macro P2_ENABLE_OPENMP is not defined for the target; linking alone cannot enable any parallel region",
    "candidate_regression": "NO_SEAM_EFFECT_BUT_EXECUTION_CONTRACT_FALSE",
    "reason": "docs claiming OpenMP-parallel sampler are contradicted by build graph",
    "evidence": "lib/phase2/CMakeLists.txt L16-30,L56-58 at " + C + "; lib/phase2/src/sampler.cpp L30-32 at " + C,
})

rows.append({
    "commit_from": B, "commit_to": C, "module": "lib/phase2", "file": "lib/phase2/(all sources)",
    "symbol": "#pragma omp",
    "change_kind": "absent",
    "science_effect": "none_direct",
    "configuration_effect": "n/a",
    "execution_effect": "VERIFIED: git grep -n '#pragma omp' HEAD -- lib/ returns zero matches at current main; union-cell sampler loop and stage2 tile/chunk/pixel loops are serial",
    "candidate_regression": NA,
    "reason": "static fact recorded for section 11.1",
    "evidence": "command output captured in audit session log; rerunnable via git grep at " + C,
})

rows.append({
    "commit_from": B, "commit_to": C, "module": "lib/phase2", "file": "lib/phase2/tools/stage2.cpp",
    "symbol": "use_acr_block (L718-724)",
    "change_kind": "gate_condition_confirmed_at_C",
    "science_effect": "ACR block path only reachable for rplan.method==P2_REJECT_SIGMA && weight_mode!=2(ivar) && !large_scale_active && acr_route!=cpu",
    "configuration_effect": "default weight_mode=auto/ivar and GC configs set acr_route=cpu -> production ACR block disabled",
    "execution_effect": "VERIFIED reading: CPU canonical pixel loop executes instead; Dispatcher-based Mixed scheduling not on this call path",
    "candidate_regression": NA,
    "reason": "section 11 static wiring fact; runtime proof still required before final verdict",
    "evidence": "lib/phase2/tools/stage2.cpp L715-737 at " + C,
})

rows.append({
    "commit_from": A, "commit_to": B, "module": "lib/phase2", "file": "lib/phase2/src/upm.cpp",
    "symbol": "huber weighting z=r/sigma_eff, delta=1.345 (upm.cpp L518-528,L602-605 at B)",
    "change_kind": "algorithm_change_standardized_huber",
    "science_effect": "VERIFIED_AT_SOURCE: Huber acts on standardized residual z=r/sigma_eff with sigma_eff=max(observation uncertainty, sigma_floor); delta stays dimensionless 1.345",
    "configuration_effect": "cfg.huber_delta serialized in model JSON (L684,L765 at B), default 1.345",
    "execution_effect": NA,
    "candidate_regression": "NOT_A_REGRESSION_BY_ITSELF; conflicts with algorithm-doc formula delta=1.345*median_abs_r (doc-source contradiction tracked separately)",
    "reason": "anchor B introduced standardized Huber per source comments (V13 R1)",
    "evidence": "git grep at " + B + " -- lib/phase2/src/upm.cpp lines 204,215,518-528,602-605,684,765",
})

# --- hunk-derived placeholder rows ---
with open(os.path.join(HS, "hunk_symbol_index.csv")) as f:
    for r in csv.DictReader(f):
        kind = r["change_kind_raw"]
        if kind == "code_hunk":
            sym = r["hunk_header_or_config_key"]
            ck = "code_hunk_unanalyzed"
        else:
            sym = r["hunk_header_or_config_key"]
            ck = kind
        rows.append({
            "commit_from": r["commit_from"], "commit_to": r["commit_to"],
            "module": r["module"], "file": r["file"], "symbol": sym,
            "change_kind": ck,
            "science_effect": NA, "configuration_effect": NA, "execution_effect": NA,
            "candidate_regression": NA,
            "reason": "PARTIAL: hunk-level index only; per-symbol semantic analysis pending in later rounds",
            "evidence": "derived from git diff --unified=0 between listed commits; raw patch: seam_paths_diff_*.patch",
        })

out_csv = os.path.join(HS, "seam_semantic_diff.csv")
with open(out_csv, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=fields)
    w.writeheader()
    w.writerows(rows)
print(f"seam_semantic_diff.csv rows={len(rows)} (verified_fact_rows=4)")
