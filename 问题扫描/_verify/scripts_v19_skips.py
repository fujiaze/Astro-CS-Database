
import json, os, re, ast, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
def roots_for(c):
    cmd=c['command']; roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    return roots
rows=[]
for c in cs:
    roots=roots_for(c)
    files=[f for f in sorted(tracked) if f.endswith('.py') and any(f==r or f.startswith(r+'/') for r in roots)]
    for f in files:
        src=open(f,encoding='utf-8',errors='replace').read()
        lines=src.splitlines()
        for i,l in enumerate(lines,1):
            if re.search(r'skipUnless|skipIf', l):
                cond=l.strip()
                rows.append((c['id'], ','.join(c['profiles']), f, i, cond[:150]))
print('=== skipUnless/skipIf guards in CI-loaded python (per check) ===')
from collections import Counter
cnt=Counter(r[0] for r in rows)
for cid in [c['id'] for c in cs]:
    if cnt.get(cid):
        print('### %s [%s] n=%d' % (cid, [c for c in cs if c['id']==cid][0]['profiles'] and ','.join([c for c in cs if c['id']==cid][0]['profiles']), cnt[cid]))
        for r in rows:
            if r[0]==cid: print('    %-52s:%-5d %s' % (r[2], r[4] if False else r[3], r[4]))
print()
print('total guards:', len(rows), 'in', len(cnt), 'checks')
print()
print('=== which guards key off build artifacts / tools / hardware ===')
CLS={'build-artifact':r'isfile|exists|is_dir|build/', 'tool':r'shutil\.which|which\(', 'hardware':r'cpu|avx|isa|os\.cpu_count|platform', 'env':r'os\.environ|getenv'}
for cls,rx in CLS.items():
    hits=[r for r in rows if re.search(rx, r[4])]
    print('  %-14s %d' % (cls, len(hits)))
    for h in hits[:14]: print('      %-26s %-50s:%d %s' % (h[0], h[2], h[3], h[4][:100]))
