
import json, os, re, ast, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
tracked={t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
def guard_scope(src, tree, deco_line):
    # find ClassDef/FunctionDef whose decorator line == deco_line
    for n in ast.walk(tree):
        if isinstance(n,(ast.ClassDef, ast.FunctionDef)):
            for dl in [dec.lineno for dec in n.decorator_list]:
                if dl==deco_line:
                    return n
    return None
print('=== quantified silent-skip surface per UT check (test functions behind tool/artifact guards) ===')
for c in cs:
    cmd=c['command']
    if 'discover' not in cmd: continue
    s=cmd[cmd.index('-s')+1]
    files=[f for f in sorted(tracked) if f.startswith(s+'/') and f.endswith('.py')]
    tot=0; guarded=0; gd=[]
    for f in files:
        src=open(f,encoding='utf-8',errors='replace').read()
        try: tree=ast.parse(src)
        except SyntaxError: continue
        # all test funcs
        allf=[n for n in ast.walk(tree) if isinstance(n,ast.FunctionDef) and n.name.startswith('test')]
        tot+=len(allf)
        for n in ast.walk(tree):
            if isinstance(n,(ast.ClassDef,ast.FunctionDef)):
                for dec in n.decorator_list:
                    dn = getattr(dec,'func',None)
                    nm = (getattr(dn,'attr',None) or getattr(dn,'id','')) if dn is not None else getattr(dec,'id','')
                    if nm in ('skipUnless','skipIf'):
                        cond = ast.get_source_segment(src, dec) or ''
                        if not re.search(r'shutil\.which|isfile|exists|cpuinfo|HAS_LINUX|/proc', cond): continue
                        subs=[x for x in ast.walk(n) if isinstance(x,ast.FunctionDef) and x.name.startswith('test')]
                        if isinstance(n,ast.FunctionDef): subs=[n]
                        guarded+=len(subs)
                        gd.append((f,n.lineno,len(subs),cond.replace('\n',' ')[:110]))
    if tot:
        print('  %-22s prof=%-26s tests=%-4d guarded_by_env=%-4d (%.0f%%)' % (c['id'], ','.join(c['profiles']), tot, guarded, 100.0*guarded/tot))
        for f,l,n,cond in gd: print('       %-52s:%-5d n=%-3d %s' % (f,l,n,cond))
