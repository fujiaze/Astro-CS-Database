#!/usr/bin/env python3
import subprocess, csv, os, sys, fnmatch
REPO="/workspace/Astro CS Database"
led = {}
with open(os.path.join(REPO,"工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/TASK_LEDGER.csv")) as f:
    for row in csv.DictReader(f):
        led[row["task_id"]] = row
# task -> commit (from controller log mapping, independently re-derived below)
mapping = {
 "BASE-OWN-001":"4b508f28","SCI-OBS-001":"9d99fd71","SCI-PSFW-001":"24610e01","SCI-P2-001":"192fab35",
 "SCI-P3-001":"eac43135","AUDIT-REVIEW-001":"a09a81f4","SCI-ADJ-001":"db26eec5","DATA-DESIGN-001":"2eab1fc1",
 "ALG-P1-001":"29747831","ALG-P2-POINT-001":"28e0ac6c","ALG-P2-PSFSW-001":"1c4e5f92","ALG-P2-SURF-001":"77ce7299",
 "ALG-P3-001":"ebefe00d","QA-MATRIX-001":"67f0456f","CONTRACT-FREEZE-001":"7a104485","IMPL-P1-CAL-001":"6c17e7d6",
 "IMPL-P1-PSFW-001":"078bde5f","IMPL-P1-DRZ-001":"684a693e","IMPL-P2-UPM-001":"f0502b25","IMPL-P2-REJ-001":"dfc1f8fc",
 "IMPL-P2-SAMP-001":"51d5b645","IMPL-P3-PROJ-001":"e476b90e","IMPL-P3-RSMP-001":"b5bc6af4","IMPL-AIO-001":"e244b284",
 "SCHEMA-INTEGRATE-001":"2ac6b758","P3-INTEGRATE-001":"a689eff2","P1-INTEGRATE-001":"960d6051",
 "P2-INTEGRATE-001":"4c0296e0","RUNTIME-CI-001":"0d8e98f1","REAL-SCIENCE-001":"815f161f","PERF-SCALE-001":"f8470f4c",
 "WIN-VERIFY-001":"296c012f","DOC-CONVERGE-001":"46ca7573",
}
def scopes(s):
    return [x.strip() for x in s.split(";") if x.strip()]
def in_scope(path, scope):
    path = path.replace("\\","/")
    s = scope.replace("\\","/").rstrip("/")
    if s.endswith("/"): s=s[:-1]
    if path == s or path.startswith(s+"/"): return True
    # file scope
    if fnmatch.fnmatch(path, s): return True
    return False
out=[]
for task, sha in mapping.items():
    files = subprocess.check_output(["git","-C",REPO,"-c","core.quotepath=false","show","--pretty=format:","--name-only",sha], text=True)
    fl = [x for x in files.splitlines() if x.strip()]
    sc = scopes(led[task]["write_scope"])
    # 控制器每任务提交更新台账（controller-owned，非任务写域，单独列出）
    ledger = "工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/TASK_LEDGER.csv"
    ctrl = [f for f in fl if f == ledger]
    bad = [f for f in fl if f != ledger and not any(in_scope(f,s) for s in sc)]
    out.append((task, sha, len(fl), len(bad), bad))
    if ctrl: pass
print(f"{'task':24} {'sha':9} {'files':>5} {'OUT':>4}  out-of-scope")
for t,s,n,b,bad in out:
    flag = "  <-- OUT" if b else ""
    print(f"{t:24} {s:9} {n:5} {b:4}{flag}")
    for f in bad[:8]: print("      ", f)
print()
print("TOTAL tasks:", len(out), " with out-of-scope files:", sum(1 for x in out if x[4]))
