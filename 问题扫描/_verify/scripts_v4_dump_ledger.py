import csv, json, sys, collections
p = "/workspace/Astro CS Database/问题扫描/账本/FIX_LEDGER.csv"
with open(p, newline='', encoding='utf-8') as f:
    rd = csv.DictReader(f)
    rows = list(rd)
print("COLS:", rd.fieldnames)
print("NROWS:", len(rows))
want = {"adaeb531","c1959436","dbaa2e18"}
hits = []
for r in rows:
    blob = json.dumps(r, ensure_ascii=False)
    for w in want:
        if w in blob:
            hits.append((w, r))
            break
print("HITS:", len(hits))
for w, r in hits:
    print("="*100)
    for k,v in r.items():
        if v and v.strip():
            print(f"  {k}: {str(v)[:1200]}")
