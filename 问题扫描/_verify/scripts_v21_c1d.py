
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
pat = re.compile(r'\bassert\s*\(([^)]*\b\w+\s*\([^)]*\)[^)]*)\)\s*;')
rows=[]
for f in prod:
    try: lines = open(f, encoding='utf-8', errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines,1):
        s=L.strip()
        if s.startswith('//') or s.startswith('*'): continue
        m = pat.search(L)
        if not m: continue
        inner = m.group(1)
        # has a comparison AND a function call inside -> side-effect inside assert
        if re.search(r'(==|!=|>=|<=|>|<)', inner) and re.search(r'\w+\s*\(', inner):
            rows.append((f,i,s[:150]))
print('assert-with-call-in-comparison sites (prod):', len(rows))
for f,i,s in rows: print(f+':'+str(i)+'  '+s)
