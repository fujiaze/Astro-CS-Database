#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102 D1 台账归一抽取 v3：支持宽表 / 二列键值表 / bullet 详情块 三种登记样式"""
import re, sys, io, os, glob, json
from collections import Counter, OrderedDict

sys.stdout.reconfigure(encoding='utf-8')
RAW = r"独立审计/证据"
OUT = r"独立审计/复算件/aud102"
FILES = sorted(glob.glob(os.path.join(RAW, "AUD-101-*.md")))

def classify(k):
    k = k.replace(' ', '')
    if k.startswith('路径'): return 'path'
    if '角色' in k: return 'role'
    if k.startswith('主题'): return 'topic'
    if k.startswith('上游'): return 'up'
    if k.startswith('下游'): return 'down'
    if '处置' in k: return 'disp'
    if '正本' in k: return 'auth'
    if '重复' in k or '重叠' in k: return 'dup'
    if '元信息' in k: return 'meta'
    if '违规' in k or '历史叙事' in k: return 'viol'
    if '悬空' in k: return 'dangle'
    if '冲突' in k: return 'conflict'
    if k.startswith('标题'): return 'title'
    if k.startswith('行数'): return 'lines'
    return None

def split_row(ln):
    s = ln.strip().strip('|')
    return [c.strip() for c in re.split(r'(?<!\\)\|', s)]

PATH1 = re.compile(r'`?([\w\u4e00-\u9fff.\-\*]+(?:/[\w\u4e00-\u9fff.\-\*]+)+\.(?:md|ya?ml|csv|json|txt))`?')
PATH0 = re.compile(r'^`?([\w\u4e00-\u9fff.\-]+\.(?:md|ya?ml|csv|json|txt))`?(?:\s|$|（|\()')

def only_path(cell):
    c = cell.strip().strip('`')
    m = PATH0.match(c)
    if m and '、' not in c and len(c.split()) <= 2:
        return m.group(1).lstrip('./')
    ms = PATH1.findall(cell)
    if len(ms) == 1 and len(cell) < 160:
        return ms[0].lstrip('./')
    return None

def parse(text, base):
    recs = []
    lines = text.splitlines()
    i = 0
    cur = None  # dict for style B / C
    while i < len(lines):
        ln = lines[i]
        st = ln.strip()
        # 表格起始
        if st.startswith('|') and i + 1 < len(lines) and re.match(r'^\|[\s:\-|]+\|$', lines[i+1].strip()):
            hdr = split_row(ln)
            keys = [classify(h) for h in hdr]
            is_sep = i + 1
            j = is_sep + 1
            if len([k for k in keys if k]) >= 4 and 'path' in keys:
                # 宽表
                while j < len(lines) and lines[j].strip().startswith('|'):
                    cells = split_row(lines[j])
                    r = {'batch': base, '_line': j + 1, '_src': 'wide'}
                    for idx, k in enumerate(keys):
                        if k and idx < len(cells):
                            r[k] = cells[idx]
                    p = only_path(r.get('path', '')) if r.get('path') else None
                    if p:
                        r['path'] = p
                        recs.append(r)
                    j += 1
                i = j
                continue
            if len(keys) == 2 and keys[0] in (None, 'field'):
                # 二列键值表：字段 | 判定
                r = {'batch': base, '_line': i + 1, '_src': 'kv'}
                while j < len(lines) and lines[j].strip().startswith('|'):
                    cells = split_row(lines[j])
                    if len(cells) >= 2:
                        k = classify(cells[0].strip('* `'))
                        if k:
                            v = cells[1]
                            if k == 'path':
                                p = only_path(v)
                                if p:
                                    r['path'] = p
                            else:
                                r[k] = (r.get(k, '') + ' ' + v).strip()[:2000]
                    j += 1
                if r.get('path'):
                    recs.append(r)
                i = j
                continue
        m = re.match(r'^#{3,4}\s+(?:(?:\d+(?:\.\d+)*)[\.,、\s]+)?(.*)$', ln)
        if m:
            title = m.group(1).strip()
            p = None
            if title.endswith('.md') or title.endswith('.yaml') or title.endswith('.csv') or title.endswith('.json'):
                p = only_path('`%s`' % title)
            if not p:
                ms = PATH1.findall(title)
                if len(ms) == 1:
                    p = ms[0]
            if p:
                if cur and cur.get('path'):
                    recs.append(cur)
                cur = {'batch': base, 'path': p, '_line': i + 1, '_src': 'block', 'title': title[:80]}
                i += 1
                continue
        if cur is not None:
            b = re.match(r'^\s*[-\*]\s+\*\*(.+?)\*\*\s*[:：]\s*(.*)$', ln)
            if b:
                k = classify(b.group(1))
                if k and k != 'path':
                    cur[k] = (cur.get(k, '') + ' ' + b.group(2).strip()).strip()[:2000]
        i += 1
    if cur and cur.get('path'):
        recs.append(cur)
    return recs

merged = OrderedDict()
for f in FILES:
    base = os.path.basename(f)
    text = io.open(f, encoding='utf-8', errors='replace').read()
    for r in parse(text, base):
        key = (base, r['path'])
        if key not in merged:
            merged[key] = r
        else:
            m = merged[key]
            for k, v in r.items():
                if k not in m or (not m[k] and v):
                    m[k] = v
                elif isinstance(v, str) and k in ('disp', 'role', 'topic', 'auth', 'dup', 'up') and len(v) > len(str(m[k])):
                    pass  # 保留首个非空
            m['_multi'] = m.get('_multi', 0) + 1

FIELDS = ['batch', 'path', 'lines', 'title', 'role', 'topic', 'up', 'down', 'auth', 'dup', 'meta', 'viol', 'dangle', 'conflict', 'disp']
CLIP = {'batch': 40, 'path': 110, 'lines': 10, 'title': 90, 'role': 300, 'topic': 200, 'up': 260,
        'down': 140, 'auth': 200, 'dup': 400, 'meta': 60, 'viol': 60, 'dangle': 60, 'conflict': 160, 'disp': 700}
with io.open(os.path.join(OUT, 'records3.tsv'), 'w', encoding='utf-8', newline='') as w:
    for (b, p), r in merged.items():
        row = []
        for f in FIELDS:
            v = re.sub(r'\s+', ' ', str(r.get(f, '') or '')).replace('\t', ' ')
            row.append(v[:CLIP[f]])
        w.write('\t'.join(row) + '\n')

paths = sorted({p for (_, p) in merged})
inv = r"独立审计/批次清单"
uni = [l.strip() for l in io.open(os.path.join(inv, '_union.txt'), encoding='utf-8', errors='replace') if l.strip()]
bn = {os.path.basename(p) for p in paths}
missing = [p for p in uni if os.path.basename(p) not in bn]
print('records:', len(merged), 'distinct paths:', len(paths), 'universe:', len(uni))
print('uncovered:', len(missing))
io.open(os.path.join(OUT, 'uncovered3.txt'), 'w', encoding='utf-8').write('\n'.join(missing) + '\n')
print('no-disp records:', sum(1 for r in merged.values() if not str(r.get('disp', '')).strip()))
print('no-role records:', sum(1 for r in merged.values() if not str(r.get('role', '')).strip()))
print(Counter(r['batch'] for r in merged.values()).most_common())
