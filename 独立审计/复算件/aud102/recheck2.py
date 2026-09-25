#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102: 台账记录区域级举证判级（只在被审文档自己的登记区间内取证，不做全文邻近窗口）
判级定义（写进成稿，可复核）：
  E1 双段贴出  : 区间内点名对方文档 + 出现 >=2 处带定位锚（:行号 / L行号 / 「原文」）的段落引证
  E2 单段+锚   : 点名对方 + 仅 1 处引证
  E3 只点名    : 点名对方，无任何原文/行号引证
  E4 有锚无对象 : 有引证但未点名对方文档
  E5 皆无      : 既无对方文档也无引证
"""
import io, os, re, sys, glob
from collections import Counter, OrderedDict

sys.stdout.reconfigure(encoding='utf-8')
RAW = r"独立审计/证据"
OUT = r"独立审计/复算件/aud102"
INV = r"独立审计/批次清单"

import extract3 as E   # 复用解析函数

def regions(text, base):
    """返回 [(path, region_text)]，region = 该文档登记区间（标题/表块 到下一个同级登记块之前）"""
    lines = text.splitlines()
    marks = []  # (lineno, path)
    for i, ln in enumerate(lines, 1):
        m = re.match(r'^#{3,4}\s+(?:(?:\d+(?:\.\d+)*)[\.,、\s]+)?(.*)$', ln)
        if m:
            t = m.group(1).strip()
            p = None
            if re.match(r'^[\w\u4e00-\u9fff.\-\*]+\.(md|ya?ml|csv|json|txt)$', t):
                p = t
            else:
                ms = E.PATH1.findall(t)
                if len(ms) == 1:
                    p = ms[0].lstrip('./')
            if p:
                marks.append((i, p))
    # 宽表 / 键值表：逐行成块
    tabmarks = []
    for i, ln in enumerate(lines, 1):
        s = ln.strip()
        if s.startswith('|') and i + 1 < len(lines) and re.match(r'^\|[\s:\-|]+\|$', lines[i+1].strip()):
            hdr = E.split_row(ln)
            keys = [E.classify(h) for h in hdr]
            if len(keys) >= 4 and 'path' in keys:
                j = i + 2
                while j < len(lines) and lines[j].strip().startswith('|'):
                    cells = E.split_row(lines[j])
                    for idx, k in enumerate(keys):
                        if k == 'path' and idx < len(cells):
                            p = E.only_path(cells[idx])
                            if p:
                                tabmarks.append((j + 1, p, 1))   # 1 行 = 1 区间
                    j += 1
    out = []
    allm = sorted([(i, p, 'h') for i, p in marks] + [(i, p, 't') for i, p, _ in tabmarks])
    for n, (i, p, kind) in enumerate(allm):
        if kind == 'h':
            nxt = None
            for j2, p2, k2 in allm[n+1:]:
                nxt = j2
                break
            reg = '\n'.join(lines[i-1:(nxt-1) if nxt else min(len(lines), i+120)])
        else:
            reg = lines[i-1]
        out.append((p, reg))
    return out

QUOTE = re.compile(r'「[^」]{4,}」|“[^”]{6,}”|~~[^~]{2,}~~|`[^`]{6,}`')
LOC = re.compile(r':\d{1,4}\b|\bL\d{1,4}\b|第\s*\d{1,4}\s*行|\[[DQEGH]-\d+\]|:\d{1,4}-\d{1,4}')
COUNTER = re.compile(r'[\w\u4e00-\u9fff.\-\*]+/[\w\u4e00-\u9fff.\-\*]+\.(?:md|ya?ml|csv|json|txt)|\b[A-Z][A-Z0-9_\-]{4,}\.md\b')

def acts(d):
    a = []
    for k, c in (('保留', 'KEEP'), ('合并', 'MERGE'), ('并入', 'MERGE'), ('删除', 'DEL'), ('移除', 'DEL'),
                 ('迁往', 'MOVE'), ('迁移', 'MOVE'), ('迁入', 'MOVE'), ('下沉', 'SINK'), ('拆分', 'SPLIT'),
                 ('待定', 'TBD'), ('待证', 'TBD'), ('上呈', 'ESC')):
        if k in (d or '') and c not in a:
            a.append(c)
    return a

recs = {}
for f in glob.glob(os.path.join(RAW, 'AUD-101-*.md')):
    base = os.path.basename(f)
    text = io.open(f, encoding='utf-8', errors='replace').read()
    regs = regions(text, base)
    regmap = {}
    for p, r in regs:
        regmap.setdefault(p.lstrip('./'), '')
        regmap[p.lstrip('./')] += '\n' + r
    for ln in io.open(os.path.join(OUT, 'records3.tsv'), encoding='utf-8'):
        parts = ln.rstrip('\n').split('\t')
        if parts[0] != base:
            continue
        p = parts[1]
        r = dict(zip(E.FIELDS, parts))
        if p.startswith('tasks/'):
            p = '工程控制/RELEASE-05/' + p
        key = (base, p)
        if key in recs:
            continue
        recs[key] = {'path': p, 'batch': base, 'disp': r.get('disp', ''), 'region': regmap.get(p, regmap.get(os.path.basename(p), ''))}

rows = []
for (b, p), r in recs.items():
    A = acts(r['disp'])
    if not ({'MERGE', 'DEL'} & set(A)):
        continue
    reg = r['region'] or ''
    named = bool(COUNTER.search(reg.replace(p, '')))
    nloc = len(set(LOC.findall(reg)))
    nq = len(QUOTE.findall(reg))
    if named and nq >= 2 and nloc >= 1:
        g = 'E1'
    elif named and (nq >= 1 or nloc >= 1):
        g = 'E2'
    elif named:
        g = 'E3'
    elif nq or nloc:
        g = 'E4'
    else:
        g = 'E5'
    rows.append((p, '/'.join(A), g, nq, nloc, len(reg), b))

rows.sort()
print('MERGE/DEL/records with region:', len(rows))
print('grade dist:', Counter(x[2] for x in rows).most_common())
print('no-region (region len<80):', sum(1 for x in rows if x[5] < 80))
with io.open(os.path.join(OUT, 'evidence_regions.tsv'), 'w', encoding='utf-8') as w:
    w.write('path\tacts\tgrade\tnquote\tnanchor\tregionlen\tbatch\n')
    for x in rows:
        w.write('\t'.join(map(str, x)) + '\n')
for x in rows:
    if x[2] != 'E1':
        print('%-3s %-58s %-16s q=%d a=%d len=%d' % (x[2], x[0][:58], x[1], x[3], x[4], x[5]))
