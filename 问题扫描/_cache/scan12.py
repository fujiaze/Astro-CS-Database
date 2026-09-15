
import ast, re
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
NAME=re.compile(r'^(_?(bad|missing|n_fail|nfail|fail_cnt|failed|drift|violations|violation|offenders|errors|err_cnt|problems|findings)_?s?|n_bad|bad_count|missing_count)$')
rep=[]
for f in files:
    if f.startswith('engineering/control/archive') or '/archive/' in f: continue
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    # find function scope containing assignment to NAME and count usages
    for fn in [n for n in ast.walk(tree) if isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef))]:
        targets=[]
        for st in ast.walk(fn):
            if isinstance(st,(ast.Assign,ast.AugAssign)):
                ts = st.targets if isinstance(st,ast.Assign) else [st.target]
                for t in ts:
                    if isinstance(t,ast.Name) and NAME.match(t.id):
                        targets.append((t.id,st.lineno))
        if not targets: continue
        names={n for n,_ in targets}
        uses=[n for n in ast.walk(fn) if isinstance(n,ast.Name) and n.id in names and isinstance(n.ctx,ast.Load)]
        # decide if used in a decision: inside If test / boolop / return / assert / comparison
        dec=set()
        for node in ast.walk(fn):
            ctxs=[]
            if isinstance(node,ast.If): ctxs.append(node.test)
            elif isinstance(node,ast.While): ctxs.append(node.test)
            elif isinstance(node,ast.Assert): ctxs.append(node.test)
            elif isinstance(node,ast.Return): ctxs.append(node.value)
            elif isinstance(node,(ast.UnaryOp,ast.BoolOp)): ctxs.append(node)
            for c in ctxs:
                if c is None: continue
                for n2 in ast.walk(c):
                    if isinstance(n2,ast.Name) and n2.id in names: dec.add(n2.id)
        for nm in sorted(names):
            if len([u for u in uses if u.id==nm])==0 or nm not in dec:
                rep.append('%s:%d  %s=%s  loads=%d in_decision=%s  fn=%s'%(f,[l for n,l in targets if n==nm][0],nm,(lines[[l for n,l in targets if n==nm][0]-1].strip())[:80],len([u for u in uses if u.id==nm]),nm in dec,fn.name))
rep.sort()
print('=== counters with no decision use (%d) ==='%len(rep))
print('\n'.join(rep))
