
import json, os, re, ast, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
THIRD = {'numpy','astropy','yaml','scipy','pytest','h5py','matplotlib','pandas','requests'}
def roots_for(c):
    cmd=c['command']; roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    return roots
def try_lines(src, tree):
    s=set()
    for n in ast.walk(tree):
        if isinstance(n, ast.Try):
            for sub in n.body:
                for x in ast.walk(sub):
                    if isinstance(x,(ast.Import,ast.ImportFrom)): s.add(x.lineno)
    return s
res=[]
for c in cs:
    roots=roots_for(c)
    if not roots: continue
    files=[f for f in sorted(tracked) if f.endswith('.py') and any(f==r or f.startswith(r+'/') for r in roots)]
    mods={}
    for f in files:
        src=open(f,encoding='utf-8',errors='replace').read()
        try: tree=ast.parse(src)
        except SyntaxError: continue
        tl=try_lines(src,tree)
        for n in ast.walk(tree):
            names=[]
            if isinstance(n, ast.Import): names=[a.name.split('.')[0] for a in n.names]
            elif isinstance(n, ast.ImportFrom): names=[(n.module or '').split('.')[0]]
            for top in names:
                if top in THIRD:
                    mods.setdefault(top,{'bare':[], 'try':[]})['try' if n.lineno in tl else 'bare'].append(f)
    decl={t.split(':')[-1] for t in c.get('prerequisite_tools',[])} & THIRD
    if mods:
        und=sorted(set(mods)-decl)
        anybare=sorted(k for k,v in mods.items() if v['bare'])
        res.append((c['id'], ','.join(c['profiles']), mods, decl, und, anybare))
print('=== checks whose loaded code imports third-party modules ===')
print('%-24s %-26s %-34s %-14s %-22s %s' % ('id','profiles','third-party (bare/try)','declared PT','UNDECLARED','bare-only(no try)'))
for cid,prof,mods,decl,und,anybare in res:
    desc=' '.join('%s:%s' % (k, ('bare%d' % len(v['bare'])) + ('/try%d'%len(v['try']) if v['try'] else '')) for k,v in sorted(mods.items()))
    print('%-24s %-26s %-34s %-14s %-22s %s' % (cid, prof[:26], desc[:34], ','.join(sorted(decl)) or '-', ','.join(und)[:22], ','.join(anybare)))
print()
print('checks with >=1 third-party import:', len(res), '| with >=1 UNDECLARED:', sum(1 for r in res if r[4]), '| with bare(unguarded) undeclared:', sum(1 for r in res if [u for u in r[4] if r[2][u]['bare']]))
print()
print('=== detail: bare undeclared imports (file:line) for the top risk rows ===')
for cid,prof,mods,decl,und,anybare in res:
    rows=[]
    for k in und:
        for f in mods[k]['bare']: rows.append((k,f))
    if not rows: continue
    if set(p for p in prof.split(',')) & {'fast','windows-main'} or True:
        pass
    print('  %-22s [%s]' % (cid, prof))
    for k,f in sorted(rows)[:8]: print('      %-8s %s' % (k,f))
