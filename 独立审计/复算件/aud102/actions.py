#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102: 逐份动作底表 + 举证分级（合并/删除是否贴了两处实际段落）"""
import io, os, re, sys
from collections import Counter, defaultdict

sys.stdout.reconfigure(encoding='utf-8')
OUT = r"独立审计/复算件/aud102"
INV = r"独立审计/批次清单"
FIELDS = ['batch', 'path', 'lines', 'title', 'role', 'topic', 'up', 'down', 'auth', 'dup',
          'meta', 'viol', 'dangle', 'conflict', 'disp']

recs, bypath = [], {}
for ln in io.open(os.path.join(OUT, 'records3.tsv'), encoding='utf-8'):
    p = ln.rstrip('\n').split('\t')
    p += [''] * (len(FIELDS) - len(p))
    r = dict(zip(FIELDS, p))
    recs.append(r)
    bypath.setdefault(r['path'], r)

# 修正：DB-19 用 tasks/XXX.md 短路径登记，还原为 工程控制/RELEASE-05/tasks/XXX.md
fixes = 0
for r in list(recs):
    if r['path'].startswith('tasks/') and r['batch'].startswith('AUD-101-DB-19'):
        newp = '工程控制/RELEASE-05/' + r['path']
        if newp not in bypath:
            r['path'] = newp
            bypath[newp] = r
            fixes += 1
print('path fixes:', fixes)

uni = [l.strip() for l in io.open(os.path.join(INV, '_union.txt'), encoding='utf-8') if l.strip()]

def acts(d):
    a = []
    for k, c in (('保留', 'KEEP'), ('合并', 'MERGE'), ('并入', 'MERGE'), ('删除', 'DEL'), ('移除', 'DEL'),
                 ('迁往', 'MOVE'), ('迁移', 'MOVE'), ('迁入', 'MOVE'), ('下沉', 'SINK'), ('拆分', 'SPLIT'),
                 ('待定', 'TBD'), ('待证', 'TBD'), ('上呈', 'ESC')):
        if k in (d or '') and c not in a:
            a.append(c)
    return a

PATH_M = re.compile(r'[\w\u4e00-\u9fff.\-\*]+(?:/[\w\u4e00-\u9fff.\-\*]+)+\.(?:md|ya?ml|csv|json|txt)')
ANCHOR = re.compile(r'(:\d|[:：]\s*\d|\[D-|\(?L\d+\)|§\d)')

def grade(r, A):
    """举证分级：STRONG 点名对方路径且给段落/行号锚；WEAK 只点名；NONE 无动作"""
    if not ({'MERGE', 'DEL', 'MOVE', 'SPLIT'} & set(A)):
        return ''
    blob = ' '.join([r.get('dup', ''), r.get('conflict', ''), r.get('disp', '')])
    named = bool(PATH_M.search(blob)) or bool(re.search(r'§\s*\d', blob))
    anchored = bool(ANCHOR.search(blob))
    if named and anchored:
        return 'STRONG'
    if named:
        return 'WEAK'
    return 'NONE'

rows = []
for p in uni:
    r = bypath.get(p)
    if r is None:
        rows.append((p, '-', '', '', 'NO-LEDGER-RECORD', ''))
        continue
    A = acts(r['disp'])
    rows.append((p, '/'.join(A) or 'NOP', grade(r, A), r['role'][:70], r['disp'][:170], r['batch'][8:18]))

with io.open(os.path.join(OUT, 'action_table.tsv'), 'w', encoding='utf-8') as w:
    for t in rows:
        w.write('\t'.join(x.replace('\t', ' ') for x in t) + '\n')

print('act dist:', Counter(t[1] for t in rows).most_common(16))
print('grade dist:', Counter(t[2] for t in rows if t[2]).most_common())
# 需要降级为"待定案"的：MERGE/DEL 且举证非 STRONG
susp = [t for t in rows if ({'MERGE', 'DEL'} & set(t[1].split('/'))) and t[2] != 'STRONG']
print('merge/del rows lacking strong evidence:', len(susp))
print('no-ledger:', sum(1 for t in rows if t[4] == 'NO-LEDGER-RECORD'))
byd = defaultdict(lambda: [0, 0])
for t in rows:
    k = '/'.join(t[0].split('/')[:-1]) or '(root)'
    byd[k][1] += 1
    if 'MERGE' in t[1] or 'DEL' in t[1] or 'MOVE' in t[1] or 'SPLIT' in t[1]:
        byd[k][0] += 1
print('%-34s %s' % ('dir', 'actionable/total'))
for k, (a, b) in sorted(byd.items()):
    print('%-34s %3d/%3d' % (k, a, b))
