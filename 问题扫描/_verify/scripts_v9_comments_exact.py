import re, pathlib, json
repo=pathlib.Path('.')
srcs=list((repo/'lib').rglob('*.cpp'))+list((repo/'lib').rglob('*.h'))+list((repo/'lib').rglob('*.hpp'))
srcs=[p for p in srcs if 'third_party' not in str(p) and 'archive' not in str(p)]
findings=[]
for src in srcs:
    text=src.read_text(encoding='utf-8',errors='ignore')
    comments=re.findall(r'//.*|/\*.*?\*/', text, re.S)   # 与 checker 逐字一致
    for c in comments:
        if 'V19R2' in c or 'V19R3' in c:
            if '冻结' not in c and 'history' not in str(src).lower():
                findings.append(('COMMENT-STALE', str(src.relative_to(repo)), len(c)))
                break
inv=[]
for src in srcs:
    text=src.read_text(encoding='utf-8',errors='ignore')
    tl=text.lower()
    if 'false_negative' in tl or '不变量' in text:
        if not re.search(r'SCI-|ALG-|TRACEABILITY', text):
            inv.append(('CON-INVARIANT-NOID', str(src.relative_to(repo))))
print('files scanned:', len(srcs))
print('COMMENT-STALE (真·checker 逻辑):', len(findings))
for f in findings[:10]: print('   ', f)
print('INVARIANT-NOID:', len(inv), inv[:6])
print('=> CON-COMMENTS verdict:', 'FAIL' if (findings or inv) else 'PASS')
print()
p=repo/'lib/phase2/tests/synthetic_gate.cpp'
t=p.read_text(encoding='utf-8',errors='ignore')
cs=re.findall(r'//.*|/\*.*?\*/', t, re.S)
big=[c for c in cs if 'V19R2' in c]
print('synthetic_gate.cpp: first // comment spans %d chars; V19R2-containing comment len=%d; contains 冻结? %s' % (len(cs[0]) if cs else 0, len(big[0]) if big else 0, ('冻结' in big[0]) if big else None))
print('line of first //:', t.index('//')+1 if '//' in t else None, 'file lines:', t.count(chr(10)))
