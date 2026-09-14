import json, os, shutil, pathlib
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
repo=pathlib.Path('.').resolve()
def exists(rel): 
    p=repo/rel
    return p.exists()
problems=[]
for c in reg:
    cmd=list(map(str,c['command']))
    outputs=set(c.get('outputs',[]))
    for idx,arg in enumerate(cmd):
        if idx==0: continue
        if arg.startswith('-'): continue
        if idx>=1 and cmd[idx-1]=='-m': continue
        if ' ' in arg or '\t' in arg: continue
        if '/' not in arg and '\\' not in arg: continue
        if arg in outputs or arg.startswith('/'): continue
        if arg=='run' or arg.startswith('run/') or arg.startswith('run\\'): continue
        if not exists(arg):
            problems.append((c['id'], arg, c['waivable'], c['platform'], c['profiles']))
print('== run.py probe_prerequisite path-existence failures (=> FAIL(prerequisite) for non-waivable) ==')
for p in problems: print('  ', p)
print('COUNT', len(problems))
print()
print('== command[0] exe availability ==')
for c in reg:
    exe=c['command'][0]
    if '/' in exe or '\\' in exe:
        ok=exists(exe)
    else:
        ok = shutil.which(exe) is not None
    if not ok: print('   EXE-MISSING', c['id'], exe)
print()
print('== prerequisite_tools declared ==')
for c in reg:
    if c.get('prerequisite_tools'): print('  ', c['id'], c['prerequisite_tools'], 'waivable=',c['waivable'])
print()
print('== outputs declared but path parent==run/? sample ==')
n=0
for c in reg:
    for o in c.get('outputs',[]):
        if not o.startswith('run/'):
            print('   NON-run OUTPUT', c['id'], o, 'waivable=',c['waivable']); n+=1
print('non-run outputs count', n)
