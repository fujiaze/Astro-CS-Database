#!/usr/bin/env python3
"""ROOT-004: condensed digest of the 93 P0 sections for controller adjudication (read-only)."""
import os, re, json
GEN = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..')) + '/reports/PROJECT-GOVERNANCE-01/root-scan/_gen'
dig = json.load(open(os.path.join(GEN,'p0_digest.json'), encoding='utf-8'))
import sys
lo, hi = int(sys.argv[1]), int(sys.argv[2])
KEY = ['- 位置','位置:','- 证据摘录','证据摘录','- 权威依据','权威依据','- 影响','影响:','- 建议处置','建议处置','- 类别','- 优先级','- 复核时点结论','- 置信度','- related','判据说明','**判据']
for d in dig[lo:hi]:
    p = os.path.join(GEN,'p0_sections', d['id']+'.md')
    txt = open(p, encoding='utf-8').read()
    print('='*100)
    print(d['id'], '|', d['cat'], '|', d['file']+':'+str(d['line']))
    print('TITLE:', d['heading'][:200])
    keep=[]; inkept=False
    for ln in txt.split('\n')[1:]:
        s=ln.strip()
        if not s: continue
        hit = any(s.startswith(k) or k in s[:14] for k in KEY)
        if hit: inkept=True; keep.append(s[:420])
        elif inkept and (s.startswith('>') or s.startswith('-') or s.startswith('①')):
            keep.append(s[:380])
        elif inkept and not s.startswith('>'):
            inkept=False
    print('\n'.join(keep[:26]))

