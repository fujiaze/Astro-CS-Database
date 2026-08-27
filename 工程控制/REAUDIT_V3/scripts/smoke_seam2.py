import subprocess, sys, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
tool = os.path.join(ROOT, "package", "11_seam", "seam_metric_tool.py")
csvpath = os.path.join(ROOT, "builds", "seam_smoke.csv")
r = subprocess.run([sys.executable, tool, csvpath, "8"], capture_output=True, text=True)
print("rc=", r.returncode)
print("STDOUT:", r.stdout[-1800:])
print("STDERR:", r.stderr[-400:])
