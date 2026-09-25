#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102: 非 E1 的合并/删除行 -> 回到台账全文（含举证节）二次判级；输出降级名单"""
import io, os, re, sys, glob
from collections import Counter, defaultdict

sys.stdout.reconfigure(encoding='utf-8')
OUT = F = r"独立审计/复算件/aud102"
RAW = r"独立审计/证据"
texts = {os.path.basename(f): io.open(f, encoding='utf-8', errors='replace').read()
         for f in glob.glob(os.path.join(RAW, 'AUD-101-*.md'))}
for k in texts:
    texts[k] = re.sub(r'\s+', ' ', texts[k])

COUNTERPART = re.compile(r'[\w\u4e00-\u9fff.\-\*]+/[\w\u4e00-\u9fff.\-\*]+\.(?:md|ya?ml|csv|json|txt)|\b[A-Z][A-Z0-9_\-]{4,}\.md\b')
QUOTE = re.compile(r'「[^」]{6,}」|”[^“”]{8,}“|"[^"]{12,}"|~~[^~]{3,}~~')
ANCH = re.compile(r':\d{1,4}\b|\bL\d{1,4}\b|\[[DQEGH]-\d+\]|第\s*\d{1,4}\s*行')

rows = [l.rstrip('\n').split('\t') for l in io.open(os.path.join(OUT, 'action_table2.tsv'), encoding='utf-8')]
res = []
for t in rows:
    p, act, grade, batch = t[0], t[1], t[2], t[5] if len(t) > 5 else ''
    if not ({'MERGE', 'DEL'} & set(act.split('/'))):
        continue
    base = os.path.basename(p)
    blob = ''
    for k, txt in texts.items():
        idxs = [m.start() for m in re.finditer(re.escape(base), txt)]
        for i in idxs[:12]:
            blob += ' ' + txt[max(0, i - 900):i + 900]
    named = bool(COUNTERPART.search(blob))
    quoted = bool(QUOTE.search(blob))
    anchored = bool(ANCH.search(blob))
    final = 'E1' if (named and quoted) else ('E2' if (named and anchored) else ('E3' if named else ('E4' if anchored else 'E5')))
    res.append((p, act, grade, final, base in ('',), len(idxs)))

print('MERGE/DEL rows:', len(res))
print('final grade dist:', Counter(r[3] for r in res).most_common())
with io.open(os.path.join(OUT, 'evidence_recheck.tsv'), 'w', encoding='utf-8') as w:
    for r in res:
        w.write('\t'.join(map(str, r)) + '\n')
print('--- non-E1 (需降级为待定案) ---')
for r in res:
    if r[3] != 'E1':
        print('%-6s %-58s %s' % (r[3], r[0][:58], r[1]))
