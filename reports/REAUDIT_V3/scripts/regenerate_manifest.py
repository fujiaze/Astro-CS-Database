import json, os, hashlib, datetime
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package")
files = []
for dirpath, _, fns in os.walk(P):
    for fn in fns:
        full = os.path.join(dirpath, fn)
        if os.path.basename(full) in ("SHA256SUMS", "MANIFEST.json"):
            continue
        rel = os.path.relpath(full, ROOT)
        files.append({"path": rel, "size_bytes": os.path.getsize(full),
                      "sha256": hashlib.sha256(open(full,"rb").read()).hexdigest()})
files.sort(key=lambda x: x["path"])
m = {
    "tool": "AstroCS_MAIN_AUDIT_SUPPLEMENT_V2",
    "collected_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    "verdict": "EVIDENCE_INCOMPLETE",
    "note": "MANIFEST.json and SHA256SUMS excluded from their own listings (self-reference).",
    "file_count": len(files),
    "total_bytes": sum(f["size_bytes"] for f in files),
    "files": files,
}
open(os.path.join(P, "MANIFEST.json"), "w", encoding="utf-8").write(json.dumps(m, indent=2, ensure_ascii=False))
print("MANIFEST files:", len(files))
# verify
mm = json.load(open(os.path.join(P, "MANIFEST.json"), encoding="utf-8"))
miss = mis = 0
for f in mm["files"]:
    full = os.path.join(ROOT, f["path"])
    if not os.path.isfile(full): miss += 1; continue
    if hashlib.sha256(open(full,"rb").read()).hexdigest() != f["sha256"]: mis += 1
print("verify: entries=" + str(len(mm["files"])) + " missing=" + str(miss) + " mismatch=" + str(mis))
