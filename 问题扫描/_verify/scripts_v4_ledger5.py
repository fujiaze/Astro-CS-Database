import csv, json
p="/workspace/Astro CS Database/问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(p,newline='',encoding='utf-8')))
ids={'M5a-G-001','M5a-G-002','M8-F-001','M8-F-002','M8-F-003'}
for r in rows:
    i=(r.get('\ufeffid') or r.get('id') or '').strip()
    if i in ids:
        print("="*70); print(i, "|", r['priority'], "|", r['release_blocker'])
        print("title:", r['title'][:200])
        print("position:", r['position'][:300])
        print("clause:", r['clause'][:200])
        for k in ('fix_state','fix_commit','fix_date','regression_test','fixed_by','fix_note','verified_state','verified_by'):
            print(f"  {k}: {r.get(k,'')[:400]}")
print("#"*70)
print("== 含 p1_noise_adapter / known_failures / FD-B2 的行 ==")
for r in rows:
    blob=json.dumps(r,ensure_ascii=False)
    if 'p1_noise_adapter' in blob or 'known_failures' in blob or 'FD-B2' in blob or 'fa5581a0' in blob or 'f849860a' in blob:
        print((r.get('\ufeffid') or r.get('id')), '|', r['fix_state'], '|', r['fix_commit'], '|', r['title'][:90])
