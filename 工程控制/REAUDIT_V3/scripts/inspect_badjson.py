import os, json
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
targets = []
for dp, dn, fn in os.walk(os.path.join(ROOT, "current")):
    for f in fn:
        if f in ("browser_cli_fp64.json", "browser_cli_fp32.json"):
            targets.append(os.path.join(dp, f))
for p in targets:
    print("---", p)
    print("size:", os.path.getsize(p))
    with open(p, "rb") as fh:
        print("head bytes:", fh.read(120))
# are they git-tracked?
import subprocess
for p in targets:
    rel = os.path.relpath(p, os.path.join(ROOT, "current"))
    r = subprocess.run(["git", "-C", os.path.join(ROOT, "current"), "ls-files", "--", rel],
                       capture_output=True, text=True)
    print("tracked?", rel, "->", repr(r.stdout.strip()))
