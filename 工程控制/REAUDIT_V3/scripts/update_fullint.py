import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "06_checker_truthfulness", "checker_truthfulness.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
for r in rows:
    if r and r[0] == "check_full_integration":
        # append the real-repo confirmation to evidence (last col)
        r[-1] = r[-1] + " + REAL_REPO run round 25: exit=0 status=PASS passed=True findings=0 (10/10 sub-checkers pass due to their own false negatives); DELIVERED/INTEG-P1-DEBT path only triggers if check_forbidden_patterns reports FAIL, which never happens on HEAD"
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
print("check_full_integration row updated")
