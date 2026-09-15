
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
rep=[]
for f in files:
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    # module-level: does main return any non-constant expr?
    for node in tree.body:
        if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)) and node.name in ('main','run','run_checks','check_all'):
            rets=[r for r in ast.walk(node) if isinstance(r,ast.Return)]
            if not rets: continue
            consts=set(); nonconst=0
            for r in rets:
                if isinstance(r.value,ast.Constant): consts.add(r.value.value)
                elif r.value is None: consts.add('NONE')
                else: nonconst+=1
            if nonconst==0 and all((isinstance(c,int) and c==0) or c in ('NONE',None) for c in consts):
                # check __main__ guard
                guard=[]
                for st in tree.body:
                    if isinstance(st,ast.If) and 'name' in ast.unparse(st.test) and '__main__' in ast.unparse(st.test):
                        guard.append(ast.unparse(st)[:200].replace('\n',' '))
                rep.append('%s:%d %s consts=%s  guard=%s'%(f,node.lineno,node.name,sorted(map(str,consts)), guard))
print('=== funcs whose only returns are 0/None (%d) ==='%len(rep))
print('\n'.join(rep))
