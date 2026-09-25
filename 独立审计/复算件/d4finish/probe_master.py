# -*- coding: utf-8 -*-
"""D4 master-table structural read-only probe (AUDIT-06 D4 finish)."""
import csv, io, sys, collections, json

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'

with io.open(MASTER, encoding='utf-8-sig', newline='') as f:
    rows = list(csv.reader(f))
hdr, body = rows[0], rows[1:]
print('header cols =', len(hdr))
print('data rows   =', len(body))
print('row-width distribution =', dict(collections.Counter(len(r) for r in body)))
bad = [(i + 2, len(r)) for i, r in enumerate(body) if len(r) != len(hdr)]
print('ragged rows (file line no, width) =', bad[:20])

# key column stats
keys = [r[0] for r in body]
print('distinct keys =', len(set(keys)))
dup = [k for k, n in collections.Counter(keys).items() if n > 1]
print('duplicate keys =', len(dup), dup[:10])

for idx, name in ((1, '类别'), (10, '处置'), (12, '冲突标记'), (13, '是否已复核')):
    cnt = collections.Counter((r[idx] if len(r) > idx else '<MISSING>') for r in body)
    print('---', name, 'distinct =', len(cnt))
    for k, v in cnt.most_common(12):
        print('   %5d  %s' % (v, (k[:110]).replace('\n', ' ')))
    if len(cnt) > 12:
        print('   ... (%d more)' % (len(cnt) - 12))

# empty cells per column
print('--- empty cell counts per column')
for i, h in enumerate(hdr):
    e = sum(1 for r in body if len(r) > i and r[i].strip() == '')
    print('  %2d %-24s empty=%d' % (i, h[:24], e))
