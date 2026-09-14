
import json, os, re, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
PAT = re.compile(r'''(?:"|')(build)(/|"\s*,\s*"|'\s*,\s*')''')
def ctx(fn, n=6):
    L = open(fn, encoding='utf-8', errors='replace').read().splitlines()
    return L
cands = {}
for c in cs:
    cmd = c['command']
    roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    hits=[]
    for f in sorted(tracked):
        if not f.endswith('.py'): continue
        if not any(f==r or f.startswith(r+'/') for r in roots): continue
        src = open(f, encoding='utf-8', errors='replace').read()
        lines = src.splitlines()
        for i,l in enumerate(lines):
            if re.search(r'''["']build["']\s*,|["']build/''', l):
                window = '\n'.join(lines[max(0,i-3):i+7])
                guard = 'SkipTest' if re.search(r'SkipTest|skipIf|skipUnless', window) else ('exit0' if re.search(r'sys\.exit\(0\)|exit\(0\)', window) else 'NO-GUARD')
                hits.append((f, i+1, guard, l.strip()[:90]))
    if hits:
        print('### %s [%s]' % (c['id'], ','.join(c['profiles'])))
        for h in hits[:10]:
            print('    %-56s:%-5d %-9s %s' % h)
        if len(hits)>10: print('    ...+%d more build-ref lines' % (len(hits)-10))
