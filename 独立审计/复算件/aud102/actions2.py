#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102: 举证分级 v2（严格：合并/删除须"点名对方文档 + 贴出可定位的段落锚"）"""
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
for r in list(recs):
    if r['path'].startswith('tasks/'):
        newp = '工程控制/RELEASE-05/' + r['path']
        r['path'] = newp
        bypath.setdefault(newp, r)

uni = [l.strip() for l in io.open(os.path.join(INV, '_union.txt'), encoding='utf-8') if l.strip()]

def acts(d):
    a = []
    for k, c in (('保留', 'KEEP'), ('合并', 'MERGE'), ('并入', 'MERGE'), ('删除', 'DEL'), ('移除', 'DEL'),
                 ('迁往', 'MOVE'), ('迁移', 'MOVE'), ('迁入', 'MOVE'), ('下沉', 'SINK'), ('拆分', 'SPLIT'),
                 ('待定', 'TBD'), ('待证', 'TBD'), ('上呈', 'ESC')):
        if k in (d or '') and c not in a:
            a.append(c)
    return a

# 点名对方文档：路径样式（含目录）或 "同主题正本 = X.md" 式具名
COUNTERPART = re.compile(r'[\w\u4e00-\u9fff.\-\*]+/[\w\u4e00-\u9fff.\-\*]+\.(?:md|ya?ml|csv|json|txt)|\b[A-Z][A-Z0-9_\-]{4,}\.md\b')
# 可定位锚：行号 :NN / L NN / 第 NN 行 / 引号内原文「…」/ 『…』/ §号带节名 / [D-NN] 举证编号 / 表格单元引用
ANCH = re.compile(r':\d{1,4}\b|\bL\d{1,4}\b|第\s*\d{1,4}\s*行|\[[DQEG]-\d+\]|「|『|line\s*\d+')

def grade(r, A):
    if not ({'MERGE', 'DEL', 'MOVE', 'SPLIT'} & set(A)):
        return ''
    dup, conf, disp = r.get('dup', ''), r.get('conflict', ''), r.get('disp', '')
    blob = dup + ' ' + conf + ' ' + disp
    named = bool(COUNTERPART.search(blob))
    anchored = bool(ANCH.search(blob))
    quoted = bool(re.search(r'「|『|"', blob)) and anchored
    if named and quoted:
        return 'E1 贴段'
    if named and anchored:
        return 'E2 有锚无原文'
    if named:
        return 'E3 只点名'
    if anchored:
        return 'E4 有锚未点名对方'
    return 'E5 两者皆无'

rows = []
for p in uni:
    r = bypath.get(p)
    if r is None:
        rows.append((p, '-', 'E0 台账无记录', '', 'NO-LEDGER-RECORD', '', '', ''))
        continue
    A = acts(r['disp'])
    rows.append((p, '/'.join(A) or 'NOP', grade(r, A), r['role'][:60], r['disp'][:200], r['batch'][8:18],
                 r['topic'][:60], r['up'][:60]))

with io.open(os.path.join(OUT, 'action_table2.tsv'), 'w', encoding='utf-8') as w:
    for t in rows:
        w.write('\t'.join(re.sub(r'\s+', ' ', x).replace('\t', ' ') for x in t) + '\n')

print('act dist:', Counter(t[1] for t in rows).most_common(18))
print('grade dist:', Counter(t[2] for t in rows if t[2]).most_common())
mm = [t for t in rows if {'MERGE', 'DEL'} & set(t[1].split('/'))]
print('MERGE/DEL rows:', len(mm))
print('  by grade:', Counter(t[2] for t in mm).most_common())
print('  need 待定案 (not E1):', sum(1 for t in mm if t[2] != 'E1 贴段'))
