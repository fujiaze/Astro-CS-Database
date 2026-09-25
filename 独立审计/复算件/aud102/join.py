#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102: 把 D1 台账记录与被审文档全集 _union.txt 连接，产出逐份动作底表"""
import io, os, re, sys, csv
from collections import Counter, OrderedDict

sys.stdout.reconfigure(encoding='utf-8')
OUT = r"独立审计/复算件/aud102"
INV = r"独立审计/批次清单"

FIELDS = ['batch', 'path', 'lines', 'title', 'role', 'topic', 'up', 'down', 'auth', 'dup', 'meta', 'viol', 'dangle', 'conflict', 'disp']
recs = []
for ln in io.open(os.path.join(OUT, 'records3.tsv'), encoding='utf-8'):
    parts = ln.rstrip('\n').split('\t')
    if len(parts) < len(FIELDS):
        parts += [''] * (len(FIELDS) - len(parts))
    recs.append(dict(zip(FIELDS, parts)))

uni = [l.strip() for l in io.open(os.path.join(INV, '_union.txt'), encoding='utf-8', errors='replace') if l.strip()]
print('universe:', len(uni))
print('universe top-dir counts:')
for k, v in sorted(Counter('/'.join(p.split('/')[:2]) if '/' in p else '(root)' + p for p in uni).items()):
    if k.startswith('(root)') or k.count('/') <= 1:
        print('   %-40s %d' % (k, v))

bybase = {}
for r in recs:
    bybase.setdefault(os.path.basename(r['path']), []).append(r)

rows = []
uncovered = []
for p in uni:
    cands = bybase.get(os.path.basename(p), [])
    exact = [c for c in cands if c['path'] == p]
    r = exact[0] if exact else (cands[0] if len(cands) == 1 else None)
    if r is None:
        uncovered.append((p, len(cands)))
        r = {'batch': '?', 'role': '', 'topic': '', 'disp': '', 'lines': '', 'auth': '', 'up': '', 'dup': ''}
    rows.append((p, r))

def act(d):
    d = d or ''
    a = []
    for k, code in (('保留', 'KEEP'), ('合并', 'MERGE'), ('并入', 'MERGE'), ('删除', 'DEL'), ('移除', 'DEL'),
                    ('迁移', 'MOVE'), ('迁往', 'MOVE'), ('迁出', 'MOVE'), ('下沉', 'SINK'), ('拆分', 'SPLIT'),
                    ('待定', 'TBD'), ('待证', 'TBD'), ('上呈', 'ESC')):
        if k in d and code not in a:
            a.append(code)
    return '/'.join(a) or '-'

with io.open(os.path.join(OUT, 'doc_table.tsv'), 'w', encoding='utf-8', newline='') as w:
    w.write('path\tlines\tact\tbatch\trole\ttopic\tup\tauth\tdisp\n')
    for p, r in rows:
        d = re.sub(r'\s+', ' ', r['disp'])
        w.write('\t'.join([p, r['lines'][:6], act(d), r['batch'][8:20], r['role'][:110], r['topic'][:80],
                           r['up'][:70], r['auth'][:70], d[:420]]) + '\n')

print('rows:', len(rows), 'uncovered:', len(uncovered))
for p, n in uncovered:
    print('   UNC:', p, 'cands=', n)
print('act distribution:', Counter(act(r['disp']) for _, r in rows).most_common(14))
print('no batch:', sum(1 for _, r in rows if r['batch'] == '?'))
