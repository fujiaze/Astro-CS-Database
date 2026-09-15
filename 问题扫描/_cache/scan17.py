
import ast
files=[l.rstrip("\n") for l in open("问题扫描/_cache/scope_py.txt",encoding="utf-8") if l.strip()]
rep=[]
for f in files:
    if "engineering/control" in f or "/archive/" in f: continue
    try: s=open(f,encoding="utf-8",errors="replace").read()
    except Exception: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    for fn in [n for n in ast.walk(tree) if isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef))]:
        events=[]
        for node in ast.walk(fn):
            if isinstance(node,ast.Assign):
                for t in node.targets:
                    if isinstance(t,ast.Name) and t.id in ("rc","r","out","res","ret","proc","result","code"):
                        events.append((t.lineno,'W',t.id))
            if isinstance(node,ast.Name) and node.id in ("rc","r","out","res","ret","proc","result","code") and isinstance(node.ctx,ast.Load):
                events.append((node.lineno,'R',node.id))
        events.sort()
        prev=None
        for ln,kind,nm in events:
            if kind=='W':
                if prev and prev[1]==nm and prev[0]!=ln:
                    rep.append('%s:%d  %s overwritten at %d without any read since line %d (fn=%s)'%(f,ln,nm,ln,prev[0],fn.name))
                prev=(ln,nm)
            else:
                prev=None
rep.sort()
print('\n'.join(rep[:60]))
