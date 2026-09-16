#!/usr/bin/env python3
"""ROOT-004: ultra-tight digest for P0 adjudication."""
import os, re, json, sys
GEN = os.path.abspath(os.path.join(os.path.dirname(__file__), '..','..','..','..')) + '/reports/PROJECT-GOVERNANCE-01/root-scan/_gen'
dig = json.load(open(os.path.join(GEN,'p0_digest.json'), encoding='utf-8'))
lo, hi = int(sys.argv[1]), int(sys.argv[2])
for d in dig[lo:hi]:
    txt = open(os.path.join(GEN,'p0_sections', d['id']+'.md'), encoding='utf-8').read().split('\n')
    pos=[]; ev=[]; auth=[]; mode=None
    for ln in txt:
        s=ln.strip()
        if not s or s.startswith('#'): continue
        if re.match(r'^- 位置', s): mode='pos'; pos.append(s[4:].strip()); continue
        if re.match(r'^- 证据摘录', s): mode='ev'; continue
        if re.match(r'^- 权威依据', s): mode='auth'; auth.append(re.sub(r'^[^:：]*[:：]\s*','',s)); continue
        if re.match(r'^- (类别|优先级|建议优先级|来源|问题说明|影响|建议处置|置信度|related|复核时点结论)', s): mode=None; continue
        if mode=='pos' and s.startswith('-'): pos.append(s.lstrip('- '))
        elif mode=='ev' and s.startswith('>') and len(ev)<3: ev.append(re.sub(r'\s+',' ',s)[:260])
        elif mode=='auth' and s.startswith('>') and len(auth)<2: auth.append(re.sub(r'\s+',' ',s)[:200])
    print('## %s | %s | %s:%s' % (d['id'], d['cat'], d['file'].split('/')[-1], d['line']))
    print('  T: '+d['heading'].split(' ',1)[-1][:150])
    print('  P: '+' / '.join(x[:200] for x in pos[:2]))
    for e in ev[:2]: print('  E: '+e)
    for a in auth[:1]: print('  A: '+a)

