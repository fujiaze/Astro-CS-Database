
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
rep=[]
for f in files:
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    tup={}
    for node in ast.walk(tree):
        if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)):
            ar=set()
            for sub in ast.walk(node):
                if isinstance(sub,ast.Return) and isinstance(sub.value,ast.Tuple): ar.add(len(sub.value.elts))
            if ar: tup[node.name]=(node.lineno,sorted(ar))
    if not tup: continue
    disc_ids=set()
    for st in ast.walk(tree):
        if isinstance(st,ast.Expr) and isinstance(st.value,ast.Call): disc_ids.add(id(st.value))
    # also: assigned but one element dropped e.g. a, _ = f()  -> record separately
    for node in ast.walk(tree):
        if isinstance(node,ast.Call) and id(node) in disc_ids:
            nm=node.func
            idn = nm.id if isinstance(nm,ast.Name) else (nm.attr if isinstance(nm,ast.Attribute) else None)
            if idn in tup:
                rep.append((f,node.lineno,idn,(ast.get_source_segment(s,node) or '')[:90],tup[idn]))
rep.sort()
print('=== TUPLE-RETURN func called as bare statement (%d) ==='%len(rep))
for r in rep: print('%s:%d %s%s  def@%d ar=%s'%r)
