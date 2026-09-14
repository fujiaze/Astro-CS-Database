import json, os, subprocess, collections
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
def git(*a): return subprocess.run(['git','-c','core.quotepath=off','--no-optional-locks']+list(a),capture_output=True,text=True,errors='replace').stdout
files = git('diff','--name-only','48ceee59','HEAD').splitlines()
print('files changed since last CI baseline run (48ceee59):', len(files))
# which checkers read them (by grep of path or dir in script text)
import re
paths=[]
for c in reg:
    cmd=list(map(str,c['command']))
    if cmd[0]=='python3' and len(cmd)>1 and not cmd[1].startswith('-'):
        s=cmd[1]
        if os.path.exists(s): paths.append((c['id'], s, open(s,encoding='utf-8',errors='replace').read()))
    elif 'discover' in cmd:
        paths.append((c['id'], cmd[cmd.index('-s')+1], ''))
changed_dirs = sorted({os.path.dirname(f) for f in files})
print()
print('== changed files (non-audit) ==')
nf=[f for f in files if not f.startswith('问题扫描/')]
for f in nf: print('   ', f)
print()
print('== gates whose script text mentions a changed path ==')
for cid, s, txt in paths:
    if not txt: continue
    hits=[f for f in nf if f.split('/')[-1] in txt or f in txt]
    if hits: print('  %-28s %s' % (cid, sorted(set(hits))[:8]))
