
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
# collapse pattern: rc != 0 -> return single generic error
pats = [
  (re.compile(r'if\s*\(\s*!?([a-zA-Z_][\w:->.]*)\s*(?:!=\s*0|!=\s*ACS_[A-Z_]*OK|>\s*0)\s*\)\s*(?:\{\s*)?return\s+(?:efill\([^)]*?)?ACS_ERR_(?:INTERNAL|IO|STATE|UNSUPPORTED)'), 'collapse-to-generic'),
  (re.compile(r'return\s+\w+\s*!=\s*0\s*\?\s*ACS_ERR_([A-Z_]+)'), 'ternary-collapse'),
]
rows=[]
for f in prod:
    try: txt = open(f, encoding='utf-8', errors='replace').read()
    except Exception: continue
    lines = txt.split('\n')
    for i,L in enumerate(lines,1):
        for p,tag in pats:
            if p.search(L):
                rows.append((f,i,tag,L.strip()[:150]))
print('collapse sites:', len(rows))
for f,i,t,s in rows: print(f+':'+str(i)+' ['+t+'] '+s)
print()
print('=== pattern: any-nonzero mapped to one code, via switch/if chains on status ===')
# find where an acs_status-ish var is tested only against != 0 and then replaced by constant
for f in prod:
    try: lines = open(f, encoding='utf-8', errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines,1):
        m = re.search(r'\b(rc|st|status|ret|err_code|rv)\b\s*!=\s*0\s*\)\s*\{?\s*(?:return|throw)', L)
        if m and 'OK' not in L:
            print('ANY-NONZERO-BRANCH', f+':'+str(i), L.strip()[:150])
