
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
rep=[]
for f in files:
    if f.startswith('engineering/control/archive') or '/archive/' in f: continue
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    if 'subprocess' not in s: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    for node in ast.walk(tree):
        if isinstance(node,ast.Try):
            has_sub=any(isinstance(n,ast.Call) and 'subprocess' in ast.unparse(n.func) for n in ast.walk(node) if isinstance(n,ast.Call))
            if not has_sub: continue
            for h in node.handlers:
                t=ast.unparse(h.type) if h.type else 'bare'
                kinds={type(b).__name__ for b in h.body}
                # swallow: pass / continue / only print
                sw = ('Pass' in kinds and len(h.body)<=2) or (kinds<={'Continue'} ) or (kinds<={'Expr'} and all(isinstance(b.value,ast.Call) and getattr(b.value.func,'id',None) in ('print','pprint','log') for b in h.body if isinstance(b,ast.Expr)))
                if sw:
                    seg=' '.join(l.strip() for l in lines[node.lineno-1:h.lineno+len(h.body)+1])[:220]
                    rep.append('%s:%d  try-subprocess except %s -> %s'%(f,node.lineno,t,seg))
rep.sort()
print('=== subprocess in try with swallowing handler (%d) ==='%len(rep))
print('\n'.join(rep))
