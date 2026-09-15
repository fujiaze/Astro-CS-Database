import csv, os, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
rows = list(csv.DictReader(open(os.path.join(HERE, "..", "账本", "FIX_LEDGER.csv"), encoding="utf-8")))
print("账本行数:", len(rows), "| 列:", [k for k in rows[0].keys()][:14])
want = ["V11-N-01","V11-N-02","V11-N-03","V11-N-04","V19-N-05","C-14","V11-N-07","V11-N-09","V11-N-10"]
for r in rows:
    if r.get("id") in want:
        print("  ", r.get("id"), "|", r.get("priority"), "|", r.get("status"), "|", (r.get("title") or "")[:90])
print()
print("=== 状态值分布 ===")
import collections
print(collections.Counter(r.get("status") for r in rows))