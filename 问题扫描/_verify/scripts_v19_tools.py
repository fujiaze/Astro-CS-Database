
import json, os, re, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
# 1) which CI-loaded py files invoke a compiler / external tool, and what happens when missing
TOOLS = re.compile(r'\b(g\+\+|gcc|clang|clang\+\+|objdump|nm|dumpbin|make|cmake|ctest|taskset|zstd|tar)\b')
rows=[]
for c in cs:
    cmd=c['command']
    roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    files=[f for f in sorted(tracked) if f.endswith('.py') and any(f==r or f.startswith(r+'/') for r in roots)]
    agg={}
    for f in files:
        src=open(f,encoding='utf-8',errors='replace').read()
        for m in set(TOOLS.findall(src)):
            agg.setdefault(m,[]).append(f)
    decl = set()
    for t in c.get('prerequisite_tools',[]): decl.add(t.split(':')[-1] if ':' in t else t)
    used=set(agg)
    undecl = sorted(u for u in used if u not in decl and u not in ('python3',))
    if used:
        print('### %-24s declared_PT=%-28s used=%s' % (c['id'], sorted(decl) or '-', sorted(used)))
        print('     UNDECLARED:', undecl)
        for k,v in sorted(agg.items()): print('        %-10s in %d files e.g. %s' % (k,len(v),v[0]))
