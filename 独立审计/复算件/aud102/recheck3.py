#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102 举证判级 pass2：限定在登记该文档的那一分片文件内取窗口，再判级"""
import io, os, re, sys, glob

sys.stdout.reconfigure(encoding='utf-8')
RAW = r"独立审计/证据"
OUT = r"独立审计/复算件/aud102"

QUOTE = re.compile(r'「[^」]{4,}」|“[^”]{6,}”|~~[^~]{2,}~~|`[^`]{6,}`|:\d{1,3}-\d{1,3}')
LOC = re.compile(r':\d{1,4}\b|\bL\d{1,4}\b|第\s*\d{1,4}\s*行')
COUNTER = re.compile(r'[\w\u4e00-\u9fff.\-\*]+/[\w\u4e00-\u9fff.\-\*]+\.(?:md|ya?ml|csv|json|txt)|\b[A-Z][A-Z0-9_\-]{4,}\.md\b')

txts = {}
rows = [l.rstrip('\n').split('\t') for l in io.open(os.path.join(OUT, 'evidence_regions.tsv'), encoding='utf-8')]
out = []
for p, act, g, nq, na, rl, b in rows[1:]:
    if g == 'E1':
        out.append((p, act, 'E1', nq, na, b)); continue
    f = os.path.join(RAW, b)
    if not os.path.exists(f):
        out.append((p, act, 'E?-文件缺失', 0, 0, b)); continue
    if b not in txts:
        txts[b] = io.open(f, encoding='utf-8', errors='replace').read()
    t = txts[b]
    key = os.path.basename(p)
    idxs = [m.start() for m in re.finditer(re.escape(key), t)]
    blob = ''
    for i in idxs[:8]:
        blob += '\n' + t[max(0, i - 1500):i + 1500]
    named = len(set(COUNTER.findall(blob)) - {p, key}) >= 1
    nq = len(QUOTE.findall(blob)); na = len(set(LOC.findall(blob)))
    if named and nq >= 2 and na >= 1: g = 'E1'
    elif named and (nq or na): g = 'E2'
    elif named: g = 'E3'
    elif nq or na: g = 'E4'
    else: g = 'E5'
    out.append((p, act, g, nq, na, b, len(idxs)))

from collections import Counter
print('pass2 grade dist:', Counter(x[2] for x in out).most_common())
with io.open(os.path.join(OUT, 'evidence_final.tsv'), 'w', encoding='utf-8') as w:
    w.write('path\tacts\tgrade\tnquote\tnanchor\tbatch\n')
    for x in out:
        w.write('\t'.join(map(str, x[:6])) + '\n')
print('--- non-E1 ---')
for x in out:
    if x[2] != 'E1':
        print('%-3s %-60s %-15s q=%-3s a=%-3s hits=%s %s' % (x[2], x[0][:60], x[1], x[3], x[4], x[6] if len(x) > 6 else '-', x[5]))
