
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
def tuple_returns(f, tree, src):
    out={}
    for node in ast.walk(tree):
        if isinstance(node,(ast.FunctionDef,ast.AsyncFunctionDef)):
            kinds=set()
            for sub in ast.walk(node):
                if isinstance(sub,ast.Return) and isinstance(sub.value,ast.Tuple):
                    n=len(sub.value.elts)
                    if n in (2,3): kinds.add(n)
            if kinds:
                out[node.name]=(node.lineno,sorted(kinds),[ (s.lineno, ast.unparse(s.value)[:70]) for s in ast.walk(node) if isinstance(s,ast.Return) and isinstance(s.value,ast.Tuple)])
    return out
def main():
    rep=[]
    for f in files:
        try: s=open(f,encoding='utf-8',errors='replace').read()
        except Exception: continue
        try: tree=ast.parse(s)
        except SyntaxError: continue
        tr=tuple_returns(f,tree,s)
        if not tr: continue
        # collect call sites
        calls=[]
        for node in ast.walk(tree):
            if isinstance(node,ast.Call):
                nm=node.func
                idn = nm.id if isinstance(nm,ast.Name) else (nm.attr if isinstance(nm,ast.Attribute) else None)
                if idn in tr:
                    parent_stmt=None
                    calls.append((node.lineno, idn, ast.unparse(node)[:60]))
        # determine which call sites are bare Expr statements (fully discarded)
        disc=set()
        for st in ast.walk(tree):
            if isinstance(st,ast.Expr) and isinstance(st.value,ast.Call):
                disc.add(st.value.lineno)
        for ln,idn,txt in calls:
            if ln in disc:
                rep.append((f,ln,idn,'DISCARDED',txt,tr[idn][1]))
    rep.sort()
    print('=== calls to tuple-returning funcs whose result is discarded (%d) ==='%len(rep))
    for r in rep: print('%s:%d %s%s %s tuple_arity=%s'%r)
main()
