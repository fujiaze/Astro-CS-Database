
import json, os, re, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
tracked={t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
def roots_for(c):
    cmd=c['command']; roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    return roots
TOP=('docs/','contracts/','ci/','runtime/','modules/','include/','lib/','cli/','tools/','tests/','packaging/','engineering/','scripts/','testdata/','cmake/','providers/','third_party/','VERSION','CMakeLists.txt','CMakePresets.json')
print('=== axis(1): literal repo paths referenced by each checker, existence in tracked tree ===')
bad=[]
for c in cs:
    roots=roots_for(c)
    seen=set()
    for f in [x for x in sorted(tracked) if x.endswith('.py') and any(x==r or x.startswith(r+'/') for r in roots)]:
        src=open(f,encoding='utf-8',errors='replace').read()
        for m in re.finditer(r'''["']((?:''' + '|'.join(re.escape(t) for t in TOP) + r''')[^"']{1,80})["']''', src):
            p=m.group(1)
            if any(ch in p for ch in '%{}*$') or p.endswith(('/','.','\\')): continue
            if p in seen: continue
            seen.add(p)
            if p not in tracked:
                ign = subprocess.run(['git','--no-optional-locks','check-ignore','-q','--stdin'],input=p.encode(),capture_output=True).returncode==0
                bad.append((c['id'], f, p, 'EXISTS-UNTRACKED' if os.path.exists(p) else ('GITIGNORED' if ign else 'ABSENT')))
from collections import Counter
print('total dangling literal repo-path references:', len(bad))
cnt=Counter(b[0] for b in bad)
for cid,n in cnt.most_common(30):
    rows=[b for b in bad if b[0]==cid]
    print('  %-26s n=%-3d %s' % (cid, n, [(r[2],r[3]) for r in rows[:4]]))
print()
print('=== ABSENT-only (真悬空) list ===')
for b in bad:
    if b[3]=='ABSENT': print('   %-24s %-52s %s' % (b[0], b[2], b[1]))
