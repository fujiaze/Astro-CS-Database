import csv, os, collections
HERE = os.path.dirname(os.path.abspath(__file__))
p = os.path.join(HERE, "..", "账本", "FIX_LEDGER.csv")
rows = list(csv.DictReader(open(p, encoding="utf-8-sig")))
print("行数:", len(rows))
cols = ["id","priority","source_line_state","fix_state","fix_commit","fix_date","regression_test","verified_state","verified_by"]
print(collections.Counter(r.get("fix_state") for r in rows))
print(collections.Counter(r.get("source_line_state") for r in rows))
print()
want = ["V11-N-01","V11-N-02","V11-N-03","V11-N-04","V11-N-05","V11-N-06","V11-N-07","V11-N-08","V11-N-09","V11-N-10","V19-N-05","V19-N-07","V2-N-01"]
for r in rows:
    if r.get("id") in want:
        print("  ", " | ".join(str(r.get(c) or "-")[:34] for c in cols))