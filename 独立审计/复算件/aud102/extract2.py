#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102: D1 台账 -> 归一记录表 (表格型 + 详情块型两种登记)"""
import re, sys, io, os, glob
from collections import Counter

sys.stdout.reconfigure(encoding='utf-8')
RAW = r"独立审计/证据"
OUT = r"独立审计/复算件/aud102"

FILES = sorted(glob.glob(os.path.join(RAW, "AUD-101-*.md")))

COLMAP = {
    '路径': 'path', '标题': 'title', '行数': 'lines', '角色': 'role', '现角色': 'role',
    '主题': 'topic', '主题（一句话）': 'topic', '上游': 'up', '下游': 'down',
    '是否正本': 'auth', '正本': 'auth', '重复或重叠对象': 'dup', '重叠对象': 'dup',
    '元信息块': 'meta', '写法违规': 'viol', '悬空引用': 'dangle', '与上位冲突': 'conflict',
    '处置建议': 'disp', '处置': 'disp',
}

def split_row(ln):
    s = ln.strip()
    if s.startswith('|'):
        s = s[1:]
    if s.endswith('|'):
        s = s[:-1]
    return [c.strip() for c in s.split('|')]

def parse_tables(text):
    """yield dict per table row where header has 路径 & 处置"""
    out = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        if lines[i].strip().startswith('|') and i + 1 < len(lines) and re.match(r'^\|[\s:\-|]+\|$', lines[i+1].strip()):
            hdr = split_row(lines[i])
            keys = [COLMAP.get(h.replace(' ', '').strip('*'), None) for h in hdr]
            if 'path' in keys and any(k == 'disp' for k in keys):
                j = i + 2
                while j < len(lines) and lines[j].strip().startswith('|'):
                    cells = split_row(lines[j])
                    rec = {}
                    for idx, k in enumerate(keys):
                        if k and idx < len(cells):
                            rec[k] = cells[idx]
                    if rec.get('path') and set(rec['path'].strip('-: ')) != set():
                        rec['_line'] = j + 1
                        out.append(rec)
                    j += 1
                i = j
                continue
        i += 1
    return out

PATHISH = re.compile(r'[\w\u4e00-\u9fff.\-]+(?:/[\w\u4e00-\u9fff.\-]+)+\.(md|yaml|csv|json|txt)')
BULLET = re.compile(r'^\s*[-*:]*\s*\*\*(.+?)\*\*\s*[:：]\s*(.*)$')

def parse_blocks(text, start_marker=None):
    out = []
    lines = text.splitlines()
    cur = None
    for i, ln in enumerate(lines, 1):
        h = re.match(r'^###\s+(?:(\d+(?:\.\d+)*)[\.、\s]+)?(.*)$', ln)
        if h:
            title = h.group(2)
            m = PATHISH.search(title.replace('`', ' '))
            if not m and title.strip().endswith('.md'):
                p = title.strip()
            else:
                p = m.group(0) if m else None
            if p:
                if cur:
                    out.append(cur)
                cur = {'path': p, '_line': i, 'title': title.strip()[:80]}
                continue
        if cur is None:
            continue
        b = BULLET.match(ln)
        if b:
            k = b.group(1).replace(' ', '').strip('*')
            kk = COLMAP.get(k, k)
            if kk in ('path', 'role', 'topic', 'up', 'down', 'auth', 'dup', 'disp', 'title', 'lines', 'meta', 'viol', 'dangle', 'conflict'):
                cur.setdefault(kk, '')
                if kk != 'path':
                    cur[kk] = (cur[kk] + ' ' + b.group(2).strip()).strip()[:1500]
    if cur:
        out.append(cur)
    return out

allrec = []
for f in FILES:
    base = os.path.basename(f)
    text = io.open(f, encoding='utf-8', errors='replace').read()
    t = parse_tables(text)
    bl = parse_blocks(text)
    seen = set()
    for r in t + bl:
        p = r.get('path', '').strip().strip('`').lstrip('./')
        if not p or len(p) < 4:
            continue
        key = (base, p)
        if key in seen:
            continue
        seen.add(key)
        r['batch'] = base
        r['path'] = p
        allrec.append(r)

FIELDS = ['batch', 'path', 'lines', 'role', 'topic', 'up', 'auth', 'dup', 'disp', 'down']
with io.open(os.path.join(OUT, 'records2.tsv'), 'w', encoding='utf-8', newline='') as w:
    for r in allrec:
        w.write('\t'.join([re.sub(r'\s+', ' ', str(r.get(f, '') or ''))[:600] for f in FIELDS]) + '\n')

print('total records:', len(allrec))
for k, v in sorted(Counter(r['batch'] for r in allrec).items()):
    print('  %-38s %d' % (k, v))
paths = sorted({r['path'] for r in allrec})
print('distinct paths:', len(paths))

# 台账清单文件（应审全集）
inv = r"独立审计/批次清单"
uni = io.open(os.path.join(inv, '_union.txt'), encoding='utf-8', errors='replace').read().split()
print('_union.txt count:', len(uni))
mp = {os.path.basename(p) for p in paths}
missing = [p for p in uni if p not in mp and os.path.basename(p) not in mp]
print('in universe but no ledger record (by basename):', len(missing))
io.open(os.path.join(OUT, 'uncovered.txt'), 'w', encoding='utf-8').write('\n'.join(missing) + '\n')
extra = [p for p in mp if os.path.basename(p) not in {os.path.basename(x) for x in uni}]
print('ledger paths not in universe:', len(extra))
io.open(os.path.join(OUT, 'extra.txt'), 'w', encoding='utf-8').write('\n'.join(sorted(extra)) + '\n')

# 处置动作分布
def act(s):
    s = (s or '')
    hits = [a for a in ('保留', '合并', '删除', '迁移', '拆分', '待定', '下沉', '改指针', '订正') if a in s]
    return '+'.join(hits) if hits else '无动作词'
print('disposition keyword distribution:', Counter(act(r.get('disp')) for r in allrec).most_common(20))
