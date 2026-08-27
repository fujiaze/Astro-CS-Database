import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
for r in rows:
    if r and r[0] == "P2-01":
        r[11] = ("static reading + 12_performance/stage2_alloc_churn_estimate.json: L1085 per-PIXEL "
                 "src_idx(depth) heap alloc -> ~1M allocs per 1M output px (~16M per full-frame mosaic), "
                 "128 B each; cal/supv per chunk + frames per tile minor. Runtime profiling still pending "
                 "32R (Gaia-blocked); estimate is static, not measured")
        r[12] = "high allocation churn; per-pixel vector in innermost loop (confirmed at L1085)"
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
print("P2-01 updated")
