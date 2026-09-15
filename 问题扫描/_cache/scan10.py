
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
rep=[]
for f in files:
    if f.startswith('engineering/control/archive') or '/archive/' in f: continue
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    for node in ast.walk(tree):
        if isinstance(node,ast.ExceptHandler):
            body=node.body
            # all-const/pass/continue-only body
            kinds={type(b).__name__ for b in body}
            only_noop = kinds <= {'Pass','Continue','Expr'}
            expr_only_print = kinds=={'Expr'} and all(isinstance(b,ast.Expr) and isinstance(b.value,ast.Call) and getattr(b.value.func,'id',None) in ('print','pprint') for b in body)
            typ = ast.unparse(node.type) if node.type else 'bare'
            if only_noop:
                seg=' '.join(l.strip() for l in lines[node.lineno-1:node.lineno+len(body)+1])[:130]
                rep.append((f,node.lineno,typ,expr_only_print,seg))
rep.sort()
print('=== except handlers whose body is pass/continue/print-only (%d) ==='%len(rep))
for f,ln,typ,p,seg in rep:
    print('%s:%d  except %s  %s'%(f,ln,typ,seg))
