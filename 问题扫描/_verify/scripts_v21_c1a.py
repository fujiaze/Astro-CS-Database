
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
print('prod files:', len(prod))

call_stmt = re.compile(r'^\s{0,8}([A-Za-z_][A-Za-z0-9_:>.\-]*)\s*\(([^;]*)\)\s*;\s*$')
status_ret = set()
for f in files:
    try: txt = open(f, encoding='utf-8', errors='replace').read()
    except Exception: continue
    for m in re.finditer(r'\b(acs_status|aio_status|acs_fio_status|c_status)\s*\*?\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(', txt):
        status_ret.add(m.group(2))
print('status-returning fn names:', len(status_ret))
rows=[]
for f in prod:
    try: lines = open(f, encoding='utf-8', errors='replace').read().split('\n')
    except Exception: continue
    for i, L in enumerate(lines, 1):
        s = L.strip()
        if not s or s.startswith('//') or s.startswith('*') or s.startswith('/*'): continue
        m = call_stmt.match(L)
        if not m: continue
        name = m.group(1).split('::')[-1].split('.')[-1]
        if name in status_ret:
            rows.append((f,i,s[:140],name))
print('DISCARD CANDIDATES:', len(rows))
for f,i,s,n in rows[:80]:
    print(f+':'+str(i)+'  ['+n+']  '+s)
