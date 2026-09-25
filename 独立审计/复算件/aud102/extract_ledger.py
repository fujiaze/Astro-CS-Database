#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102 取证脚本：从 D1 台账各分片中机械抽取"逐份登记"记录，归一为 TSV。
只读输入，不改仓库。输出: aud102/records.tsv + 统计
"""
import re, sys, io, os, csv, glob

sys.stdout.reconfigure(encoding='utf-8')
RAW = r"独立审计/证据"
OUT = r"独立审计/复算件/aud102"
os.makedirs(OUT, exist_ok=True)

FILES = sorted(glob.glob(os.path.join(RAW, "AUD-101-*.md")))
PATHRE = re.compile(r'(?:^|[\s>（(\[|])([\w./\-\u4e00-\u9fff\u3001\uff08\uff09]*(?:/[\w.\-\u4e00-\u9fff\u3001]+)+\.(?:md|yaml|csv|json|txt))\b')

def find_path(s):
    m = PATHRE.search(s)
    return m.group(1) if m else None

FIELD = re.compile(r'^\s*[-*:]*\s*\*\*(.+?)\*\*\s*[:：]\s*(.*)$')

records = []
for f in FILES:
    base = os.path.basename(f)
    lines = io.open(f, encoding='utf-8', errors='replace').read().splitlines()
    cur = None
    sec = None
    for i, ln in enumerate(lines, 1):
        h = re.match(r'^###\s+(\d+(?:\.\d+)*)[\.、\s]\s*(.*)$', ln)
        if h:
            if cur and cur.get('path'):
                records.append(cur)
            p = find_path(h.group(2)) or h.group(2).strip()
            cur = {'batch': base, 'item': h.group(1), 'path': p, 'heading': h.group(2).strip()[:90],
                   'lineno': i, 'role': '', 'topic': '', 'upstream': '', 'disp': '', 'dup': '', 'auth': '', 'down': ''}
            sec = 'record'
            continue
        if cur is None:
            continue
        m = FIELD.match(ln)
        if m:
            k, v = m.group(1).strip(), m.group(2).strip()
            kk = k.replace(' ', '')
            if kk in ('角色', '现角色', '角色判定'):
                cur['role'] = v
            elif kk in ('主题', '主题（一句话）'):
                cur['topic'] = v
            elif kk.startswith('上游'):
                cur['upstream'] = v
            elif kk.startswith('处置'):
                cur['disp'] = v
            elif kk.startswith('重复') or kk.startswith('重叠'):
                cur['dup'] = v
            elif kk.startswith('是否正本') or kk == '正本':
                cur['auth'] = v
            elif kk.startswith('下游'):
                cur['down'] = v
    if cur and cur.get('path'):
        records.append(cur)

# 表格型登记（DA01 一览表等）： | `path` | ... |
TABROW = re.compile(r'^\|\s*`?([^|`\n]{4,120}?\.(?:md|yaml|csv|json))`?\s*\|')
tabrows = []
for f in FILES:
    base = os.path.basename(f)
    for i, ln in enumerate(io.open(f, encoding='utf-8', errors='replace').read().splitlines(), 1):
        m = TABROW.match(ln)
        if m:
            cells = [c.strip() for c in ln.strip().strip('|').split('|')]
            tabrows.append({'batch': base, 'lineno': i, 'path': m.group(1).strip().strip('`'),
                            'ncells': len(cells), 'tail': ' | '.join(cells[-3:])})

def norm(p):
    p = p.strip().strip('`').lstrip('./')
    return p

with io.open(os.path.join(OUT, 'records.tsv'), 'w', encoding='utf-8', newline='') as w:
    for r in records:
        w.write('\t'.join([r['batch'], str(r['lineno']), norm(r['path']), r['role'][:200], r['disp'][:300], r['auth'][:120]]) + '\n')

with io.open(os.path.join(OUT, 'tabrows.tsv'), 'w', encoding='utf-8', newline='') as w:
    for r in tabrows:
        w.write('\t'.join([r['batch'], str(r['lineno']), norm(r['path']), str(r['ncells']), r['tail'][:220]]) + '\n')

print("record-blocks:", len(records), "tabrows:", len(tabrows))
print("per-batch blocks:")
from collections import Counter
for k, v in sorted(Counter(r['batch'] for r in records).items()):
    print("  %-40s %d" % (k, v))
paths = sorted({norm(r['path']) for r in records if '/' in r['path'] or r['path'].endswith('.md')})
print("distinct paths in blocks:", len(paths))
io.open(os.path.join(OUT, 'paths.tsv'), 'w', encoding='utf-8').write('\n'.join(paths))
