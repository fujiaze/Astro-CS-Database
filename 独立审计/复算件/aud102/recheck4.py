#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102 举证判级（只在该文档自己登记的字段内取证：重复或重叠对象 / 与上位冲突 / 处置建议）
规则 E-MERGE-DEL-2：
  E1 = 字段内点名对方文档（另一份文档的路径或裸文件名）且给出 >=2 处可定位引证（:行号 / L行号 / 「原文」/ ~~划掉~~ / `代码原文`）
  E2 = 点名对方 + <=1 处引证
  E3 = 只点名，无引证
  E4 = 有引证，未点名对方
  E5 = 两者皆无
"""
import io, os, re, sys
from collections import Counter

sys.stdout.reconfigure(encoding='utf-8')
import extract3 as E
OUT = r"独立审计/复算件/aud102"
RAW = r"独立审计/证据"
import glob
recs = {}
for f in glob.glob(os.path.join(RAW, 'AUD-101-*.md')):
    base = os.path.basename(f)
    text = io.open(f, encoding='utf-8', errors='replace').read()
    for r in E.parse(text, base):
        p = r['path']
        if p.startswith('tasks/'):
            p = '工程控制/RELEASE-05/' + p
        key = (base, p)
        if key in recs:
            continue
        r['path'] = p
        prev = recs.get(key)
        if prev is None:
            recs[key] = r
        else:
            for k, v in r.items():
                if isinstance(v, str) and (not isinstance(prev.get(k), str) or len(v) > len(prev.get(k) or '')):
                    prev[k] = v

def acts(d):
    a = []
    for k, c in (('保留', 'KEEP'), ('合并', 'MERGE'), ('并入', 'MERGE'), ('删除', 'DEL'), ('移除', 'DEL'),
                 ('迁往', 'MOVE'), ('迁移', 'MOVE'), ('迁入', 'MOVE'), ('下沉', 'SINK'), ('拆分', 'SPLIT'),
                 ('待定', 'TBD'), ('待证', 'TBD'), ('上呈', 'ESC')):
        if k in (d or '') and c not in a:
            a.append(c)
    return a

QUOTE = re.compile(r'「[^」]{4,}」|“[^”]{6,}”|~~[^~]{2,}~~|`[^`]{6,}`|:\d{1,4}[–\-:]\d{1,4}')
COUNTER = re.compile(r'[\w\u4e00-\u9fff.\-\*]+/[\w\u4e00-\u9fff.\-\*]+\.(?:md|ya?ml|csv|json|txt)|\b[A-Z][A-Z0-9_\-]{4,}\.(?:md|ya?ml|csv)\b')
out = []
for (b, p), r in sorted(recs.items(), key=lambda kv: kv[0][1]):
    A = acts(r.get('disp', ''))
    if not ({'MERGE', 'DEL', 'SPLIT'} & set(A)):
        continue
    fields = ' '.join([str(r.get(k, '')) for k in ('dup', 'conflict', 'disp', 'auth')])
    counters = set(x for x in COUNTER.findall(fields))
    cnt_names = {c for c in re.findall(r'[\w\.\-/]+\.(?:md|ya?ml|csv|json|txt)', fields)
                 if os.path.basename(c) != os.path.basename(p)}
    named = len(cnt_names) >= 1
    nq = len(QUOTE.findall(fields))
    g = 'E1' if (named and nq >= 2) else ('E2' if (named and nq == 1) else ('E3' if named else ('E4' if nq else 'E5')))
    out.append((p, '/'.join(A), g, nq, len(cnt_names), b, fields[:0]))

print('actionable rows:', len(out))
print('grade dist:', Counter(x[2] for x in out).most_common())
with io.open(os.path.join(OUT, 'evidence_field.tsv'), 'w', encoding='utf-8') as w:
    w.write('path\tacts\tgrade\tnquote\tnCounterpartDocs\tbatch\n')
    for x in out:
        w.write('\t'.join(map(str, x[:6])) + '\n')
for x in out:
    if x[2] != 'E1':
        print('%-3s %-58s %-16s q=%-3s dups=%s  %s' % (x[2], x[0][:58], x[1], x[3], x[4], x[5]))
