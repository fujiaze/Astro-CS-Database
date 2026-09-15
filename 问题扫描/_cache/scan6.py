
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
    bool_ctx=set()
    for st in ast.walk(tree):
        if isinstance(st,ast.If): bool_ctx.add(id(st.test))
        if isinstance(st,ast.Assert): bool_ctx.add(id(st.test))
        if isinstance(st,ast.UnaryOp) and isinstance(st.op,ast.Not): bool_ctx.add(id(st.operand))
        if isinstance(st,(ast.If,ast.While)) : pass
    for node in ast.walk(tree):
        if isinstance(node,ast.Call) and id(node) in bool_ctx:
            nm=node.func
            idn = nm.id if isinstance(nm,ast.Name) else (nm.attr if isinstance(nm,ast.Attribute) else None)
            if idn in tup:
                rep.append('%s:%d  if/assert(%s)   [def@%d ar=%s]'%(f,node.lineno,(ast.get_source_segment(s,node) or '')[:60],tup[idn][0],tup[idn][1]))
    # single-name assignment of tuple call
    for st in ast.walk(tree):
        if isinstance(st,ast.Assign) and len(st.targets)==1 and isinstance(st.targets[0],ast.Name):
            if isinstance(st.value,ast.Call):
                nm=st.value.func
                idn = nm.id if isinstance(nm,ast.Name) else (nm.attr if isinstance(nm,ast.Attribute) else None)
                if idn in tup and len(tup[idn][1])==2:
                    rep.append('%s:%d  %s = %s  (single-name assign of 2-tuple)'%(f,st.lineno,st.targets[0].id,(ast.get_source_segment(s,st.value) or '')[:70]))
rep.sort()
print('=== TUPLE-FUNC results used as truthiness / mis-unpacked (%d) ==='%len(rep))
print('\n'.join(rep))
