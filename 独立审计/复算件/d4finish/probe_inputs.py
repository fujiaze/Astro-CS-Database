# -*- coding: utf-8 -*-
"""Structure probe of the four judgement CSVs + mechanical inventory CSV."""
import csv, io, sys, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'独立审计/证据'
for name in ('AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
             'AUD-402-判读-BD1.csv', 'AUD-402-常数台账.csv',
             'AUD-402-常数台账.旁表-无名值与测试钉值.csv',
             'D4-多侧默认值全表.csv', 'D4-多侧默认值候选.csv'):
    p = ROOT + '\\' + name
    try:
        with io.open(p, encoding='utf-8-sig', newline='') as f:
            rows = list(csv.reader(f))
    except Exception as e:
        print('!!', name, e)
        continue
    hdr = rows[0]
    body = [r for r in rows[1:] if len(r) > 1]
    print('=== %s  rows=%d cols=%d' % (name, len(body), len(hdr)))
    for i, h in enumerate(hdr):
        print('   %2d %s' % (i, h))
    print('   widths:', dict(collections.Counter(len(r) for r in rows[1:])))
    print()
