import json, csv, io, os
# replicate constants from the checker
src = open('tools/traceability/check_traceability_matrix.py', encoding='utf-8').read()
import re
m = re.search(r'CSV_COLS\s*=\s*\[(.*?)\]', src, re.S)
CSV_COLS = json.loads('['+m.group(1).replace(chr(39),'"')+']')
print('CSV_COLS len', len(CSV_COLS))
MM='docs/traceability/TRACEABILITY_MATRIX.json'; MC='docs/traceability/TRACEABILITY_MATRIX.csv'
raw = open(MC, 'rb').read()
hdr_bom = raw[:3]==b'\xef\xbb\xbf'
print('CSV has BOM:', hdr_bom)
with open(MC, encoding='utf-8', newline='') as f:
    rd = csv.reader(f); header = next(rd); rows=[r for r in rd if r and any(c.strip() for c in r)]
print('header[0] repr', repr(header[0]))
print('header == CSV_COLS ?', header==CSV_COLS)
if header!=CSV_COLS:
    print('  => ERROR SCHEMA_VIOLATION 表头不一致 缺=', set(CSV_COLS)-set(header), '多=', set(header)-set(CSV_COLS))
mods = json.load(open(MM, encoding='utf-8'))['modules']
jmap = {r['module_id']: [str(r.get(c,'')) for c in CSV_COLS] for r in sorted(mods, key=lambda x:x['module_id'])}
print('csv rows', len(rows), 'json modules', len(jmap))
# parity on BOM-stripped rows: if header mismatched, checker returns early. Simulate post-fix parity:
if header != CSV_COLS:
    # after removing BOM, header should equal CSV_COLS; then parity check runs
    fixed = [ [ (c[1:] if i==0 and c.startswith('\ufeff') else c) for i,c in enumerate(r)] for r in rows]
else:
    fixed = rows
diffs=[]
for i,r in enumerate(fixed):
    mid=r[0]
    if mid not in jmap: diffs.append((i,mid,'NOT-IN-JSON',None)); continue
    if r != jmap[mid]:
        d=[CSV_COLS[j] for j in range(len(CSV_COLS)) if j<len(r) and r[j]!=jmap[mid][j]]
        diffs.append((i,mid,'DIFF',d))
print('POST-BOM-FIX PARITY DIFFS:', len(diffs))
for dd in diffs: print('   ', dd)
