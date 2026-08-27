import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
P = os.path.join(ROOT, "package", "14_findings", "findings.csv")
rows = list(csv.reader(open(P, encoding="utf-8")))
seen = set(r[0] for r in rows)
new = ["P2-10","P2","CONFIRMED","data_artifact","BASS DR3/","BASS DR3/SHA256SUMS.txt","369 entries; 4 OK; 364 unusable","BASS bundled catalog checksum file integrity",
 "BASS DR3 bundled index/coords checksums are valid on Linux",
 "committed SHA256SUMS.txt (BASS DR3/SHA256SUMS.txt) verifies only 4/369 entries: coords.csv.gz, index.csv.gz, index.json, README.md OK; constellation_coverage.csv FAILS (actual 000a1ad3... vs listed E295A8BF...); ~364 entries use Windows backslash paths (dates\\...json) or reference uncompressed coords.csv/index.csv that are not committed -> unusable on Linux",
 "all bundled artifacts verify",
 "sha256sum -c BASS DR3/SHA256SUMS.txt",
 "bundled dataset checksum ledger is partially stale/cross-platform-broken; constellation_coverage.csv mismatch not caught",
 "regenerate SHA256SUMS.txt with forward-slash paths for committed files only",
 "regenerate + re-verify",
 "15_large_artifact_manifest/BASS_DR3_NOTE.md + large_artifact_manifest.json"]
if new[0] not in seen:
    rows.append(new)
with open(P, "w", newline="", encoding="utf-8") as f:
    csv.writer(f).writerows(rows)
from collections import Counter
c = Counter(r[1] for r in rows[1:])
print("findings rows=" + str(len(rows)-1) + " by_severity=" + str(dict(c)))
