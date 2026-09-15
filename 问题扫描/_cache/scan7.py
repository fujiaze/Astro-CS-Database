
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
rep=[]
for f in files:
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    for node in ast.walk(tree):
        if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)) and node.name in ('main','run','check','__main__'):
            rets=[r for r in ast.walk(node) if isinstance(r,ast.Return)]
            consts=set()
            for r in rets:
                if isinstance(r.value,ast.Constant): consts.add(r.value.value)
                elif r.value is None: consts.add(None)
            has_nonzero = any(isinstance(c,int) and not isinstance(c,bool) and c!=0 for c in consts)
            # also SystemExit raised with nonzero?
            exits=[n for n in ast.walk(node) if isinstance(n,ast.Call) and ((isinstance(n.func,ast.Attribute) and ast.unparse(n.func)=='sys.exit') or (isinstance(n.func,ast.Name) and n.func.id in ('exit','quit','sys.exit')))]
            ex_vals=set()
            for e in exits:
                if e.args and isinstance(e.args[0],ast.Constant): ex_vals.add(e.args[0].value)
            has_nonzero_exit = any(isinstance(v,int) and v!=0 for v in ex_vals)
            if not has_nonzero and not has_nonzero_exit:
                # does it end with return 0 / return?
                last=[r for r in rets if r.lineno>=max(x.lineno for x in rets)]
                rep.append((f,node.lineno,node.name,sorted([str(c) for c in consts]),sorted(str(v) for v in ex_vals),len(exits)))
print('=== main/run/check funcs with NO nonzero return/exit constant (%d) ==='%len(rep))
for r in rep: print('%s:%d %s returns=%s exits=%s n_exit_calls=%d'%r)
