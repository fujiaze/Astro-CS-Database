import csv, os, glob
ROOT = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662"
# find contract files
hits = []
for pat in ["**/API_CONTRACTS*", "**/*contract*.csv", "**/*CONTRACT*"]:
    hits += glob.glob(os.path.join(ROOT, "package", pat), recursive=True)
print("contract-like files in package:")
for h in sorted(set(hits)):
    n = sum(1 for _ in open(h, encoding="utf-8", errors="replace"))
    print(f"  {os.path.relpath(h, ROOT)}  rows={n}")
# also search the whole package for any 4xx-row csv
print()
print("CSV files with >= 100 data rows:")
for h in sorted(glob.glob(os.path.join(ROOT, "package", "**", "*.csv"), recursive=True)):
    try:
        rows = sum(1 for _ in open(h, encoding="utf-8", errors="replace"))
    except: continue
    if rows >= 100:
        print(f"  {os.path.relpath(h, ROOT)}  lines={rows}")
