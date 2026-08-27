import csv, subprocess, json, sys
# Smoke-test the shipped seam_metric_tool.py: synthetic boundary CSV with a known
# offset+scale relation between A and B, then verify the tool recovers it.
import os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
tool = os.path.join(ROOT, "package", "11_seam", "seam_metric_tool.py")
rows = []
import random
random.seed(7)
for i in range(200):
    pos = (i % 20) - 9
    x = random.gauss(0, 1)
    rows.append({"side": "A", "pos": pos, "value": "%.6f" % (x * 1.0)})
    rows.append({"side": "B", "pos": pos, "value": "%.6f" % (x * 1.1 + 5.0)})
csvpath = os.path.join(ROOT, "builds", "seam_smoke.csv")
with open(csvpath, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=["side", "pos", "value"]); w.writeheader(); w.writerows(rows)
r = subprocess.run([sys.executable, tool, csvpath, "--strip_width", "8"], capture_output=True, text=True)
print("rc=", r.returncode)
print("STDOUT:", r.stdout[-1500:])
print("STDERR:", r.stderr[-500:])
