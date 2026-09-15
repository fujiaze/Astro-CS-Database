
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
# find expressions combining support/coverage with weights/ivar/snr
pat = re.compile(r'(support|coverage|n_contrib|nContrib|sum_area|sumArea)[^;]{0,80}(\*|/)[^;]{0,40}(ivar|weight|snr|variance)|(ivar|weight|inv_var|snr)[^;]{0,40}(\*|/)[^;]{0,40}(support|coverage|nContrib)', re.I)
rows=[]
for f in prod:
    try: lines=open(f,encoding='utf-8',errors='replace').read().split('\n')
    except Exception: continue
    if 'acr/' in f: continue
    for i,L in enumerate(lines,1):
        s=L.strip()
        if s.startswith('//') or s.startswith('*'): continue
        if pat.search(L): rows.append((f,i,s[:160]))
print('support/coverage-as-weight candidates:', len(rows))
for f,i,s in rows[:40]: print(f+':'+str(i), s)
