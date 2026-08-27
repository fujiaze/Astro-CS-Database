import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
hdr = rows[0]
for r in rows[1:]:
    if r[0] == "P2-01":
        # columns: id,severity,status,category,module,path,line,symbol,claim,observed,expected,reproduction,impact,proposed_fix,verification,evidence
        r[2] = "CONFIRMED(REVISED)"   # status
        r[9] = "src_idx(depth) per-pixel vector (L1085) - RUNTIME-VERIFIED: -O2 production binary stack-elides it (operator-new=0, glibc malloc=31 for reject path); churn overstated in static estimate"
        r[11] = "runtime allocator verification 12_performance/STAGE2_ALLOC_RUNTIME_VERIFY.md: LD_PRELOAD versioned operator-new interposer (validated) = 0 hits; glibc malloc = 31 for whole reject run; static stage2_alloc_churn_estimate.json over-estimates"
        r[12] = "low in -O2 (stack-elided); code-style maintainability: hoist buffer / document dependency on compiler SRA; -O0/debug builds DO allocate per pixel"
        r[15] = "runtime LD_PRELOAD allocator counts (round 126) + static stage2.cpp L1085"
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
print("P2-01 updated to:", rows[0][0] and [x for x in rows if x[0]=="P2-01"][0][2])
