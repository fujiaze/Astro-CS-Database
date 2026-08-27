#!/usr/bin/env python3
"""Refresh large-artifact manifest (Control §15): largest tracked files in repo."""
import os, subprocess, json

REPO = "/home/lighthouse/Astro CS Database"
OUT = os.path.join(open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip(),
                   "package", "15_large_artifact_manifest")

# use git ls-files to list tracked files and stat them
files = subprocess.run(["git", "-C", REPO, "ls-files"], capture_output=True, text=True).stdout.splitlines()
sizes = []
for f in files:
    p = os.path.join(REPO, f)
    if os.path.isfile(p):
        try:
            sizes.append((os.path.getsize(p), f))
        except OSError:
            pass
sizes.sort(reverse=True)
top50 = [{"size_bytes": s, "size_mib": round(s/1048576, 2), "path": p} for s, p in sizes[:50]]
total_bytes = sum(s for s, _ in sizes)
result = {
    "tracked_files": len(sizes),
    "total_bytes": total_bytes,
    "total_mib": round(total_bytes/1048576, 2),
    "files_gt_10MiB": sum(1 for s, _ in sizes if s > 10*1048576),
    "top50": top50,
}
os.makedirs(OUT, exist_ok=True)
open(os.path.join(OUT, "large_artifact_manifest.json"), "w").write(
    json.dumps(result, indent=2, ensure_ascii=False))
print("tracked files:", len(sizes), "total MiB:", round(total_bytes/1048576, 2))
print("files > 10 MiB:", sum(1 for s, _ in sizes if s > 10*1048576))
print("top 15:")
for s, p in sizes[:15]:
    print("  %8d  %s" % (s, p))
