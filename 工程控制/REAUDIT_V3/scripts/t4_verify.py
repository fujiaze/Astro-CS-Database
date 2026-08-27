#!/usr/bin/env python3
"""Verify extracted T4 Red lights + masters against remote-side SHA-256 manifest.
Read-only w.r.t. the AstroCS repo; operates inside the audit root only."""
import csv, hashlib, os, sys

ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
PKG = os.path.join(ROOT, "package", "03_testdata")
remote_csv = os.path.join(PKG, "t4_manifest_remote.csv")

# local path mapping: manifest full_path is Windows F:\...; local extraction root is ROOT
def local_path(full_path: str) -> str:
    p = full_path.replace("\\", "/")
    marker = "Normalization Database/"
    i = p.find(marker)
    if i < 0:
        return None
    rel = p[i + len(marker):]
    return os.path.join(ROOT, rel)

rows = list(csv.DictReader(open(remote_csv)))
out_rows = []
ok = mismatch = missing = 0
for r in rows:
    lp = local_path(r["full_path"])
    status = "MISSING_LOCAL"
    sha = ""
    size = -1
    if lp and os.path.isfile(lp):
        size = os.path.getsize(lp)
        h = hashlib.sha256()
        with open(lp, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        sha = h.hexdigest().upper()
        if size == int(r["size_bytes"]) and sha == r["sha256"]:
            status = "OK"
            ok += 1
        else:
            status = "HASH_OR_SIZE_MISMATCH"
            mismatch += 1
    else:
        missing += 1
    out_rows.append({
        "dataset": "Galaxy_Center_T4", "panel": r["panel"], "filter": r["filter"],
        "index": r["index"], "filename": r["filename"],
        "relative_path": (lp.replace(ROOT + "/", "") if lp else ""),
        "size_bytes": size, "sha256": sha,
        "remote_size_bytes": r["size_bytes"], "remote_sha256": r["sha256"],
        "source_host": "fujia@fatduck (Windows)", "transfer_status": status,
    })

fields = ["dataset","panel","filter","index","filename","relative_path","size_bytes","sha256",
          "remote_size_bytes","remote_sha256","source_host","transfer_status"]
with open(os.path.join(PKG, "testdata_manifest.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=fields)
    w.writeheader()
    w.writerows(out_rows)

red_ok = sum(1 for x in out_rows if x["filter"] == "Red" and x["transfer_status"] == "OK")
print(f"verify_result total={len(out_rows)} ok={ok} mismatch={mismatch} missing={missing} red_ok={red_ok}")
sys.exit(0 if (mismatch == 0 and missing == 0 and red_ok == 32) else 3)
