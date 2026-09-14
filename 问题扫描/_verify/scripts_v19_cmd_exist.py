
import json, os, subprocess, shlex
d = json.load(open('ci/checks.json', encoding='utf-8'))
cs = d['checks']
tracked = set(subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split('\0'))
tracked = {t for t in tracked if t}
tracked_dirs = set()
for t in tracked:
    parts = t.split('/')
    for i in range(1,len(parts)): tracked_dirs.add('/'.join(parts[:i]))
def exists_tracked(p):
    return ('L-FILE' if p in tracked else ('L-DIR' if p in tracked_dirs else ('W-FILE-UNTRACKED' if os.path.isfile(p) else ('W-DIR-UNTRACKED' if os.path.isdir(p) else 'ABSENT'))))
# collect referenced tokens from commands
import re
missing=[]
for c in cs:
    cmd=c['command']
    for tok in cmd[1:]:
        if tok.startswith('-'): continue
        if ':' in tok and not tok.endswith('.py'): continue
        if not re.search(r'[/.]', tok): continue
        st = exists_tracked(tok)
        if st!='L-FILE' and st!='L-DIR':
            missing.append((c['id'], tok, st))
print('=== non-tracked referenced tokens in command (exe/script/data/dir) ===')
for m in missing: print('  %-34s %-46s %s' % m)
print('total flags:', len(missing))
print()
print('=== argv[0] executables used ===')
import collections
ex=collections.Counter(c['command'][0] for c in cs)
print(dict(ex))
print()
print('=== python3 script targets (argv[1]) existence ===')
tg=collections.Counter()
for c in cs:
    if c['command'][0]=='python3':
        for j,t in enumerate(c['command'][1:],1):
            if not t.startswith('-'):
                tg[t]+=1; break
for t,n in sorted(tg.items()):
    print('  %-58s n=%-3d %s' % (t,n,exists_tracked(t)))
