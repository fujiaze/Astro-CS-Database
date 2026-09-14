import csv, json
p = "/workspace/Astro CS Database/问题扫描/账本/FIX_LEDGER.csv"
with open(p, newline='', encoding='utf-8') as f:
    rows = list(csv.DictReader(f))
for key in ["c1959436","dbaa2e18","80c32b19"]:
    print("### search", key, "->", [r.get('\ufeffid') or r.get('id') for r in rows if key in json.dumps(r,ensure_ascii=False)])
# F-14 rows
print("### ids containing F-14 / reclaim:")
for r in rows:
    i = (r.get('\ufeffid') or r.get('id') or '')
    t = (r.get('title') or '')
    if 'F-14' in i or 'F-14' in t or 'reclaim' in json.dumps(r,ensure_ascii=False).lower():
        print(' -', i, '|', r.get('fix_state'), '|', r.get('fix_commit'), '|', t[:120])
# summary of fix_state distribution
import collections
print("### fix_state:", collections.Counter(r.get('fix_state') for r in rows))
print("### filled rows (fix_state non-empty):")
for r in rows:
    if (r.get('fix_state') or '').strip():
        print('  ', (r.get('\ufeffid') or r.get('id')), '|', r.get('fix_state'), '|', r.get('fix_commit'), '|', (r.get('regression_test') or '')[:80])
