
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
TARGET={'run','check_output','check_call','call','Popen','system','popen'}
hits=[]
for f in files:
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    for st in ast.walk(tree):
        if isinstance(st, ast.Expr) and isinstance(st.value, ast.Call):
            fn=st.value.func
            m = fn.attr if isinstance(fn, ast.Attribute) else (fn.id if isinstance(fn,ast.Name) else None)
            if m in TARGET and isinstance(fn, ast.Attribute) and (ast.unparse(fn.value) in ('subprocess','os')):
                kw=[ast.unparse(k) for k in st.value.keywords]
                has_check = any(k.startswith('check=') for k in kw)
                if not has_check:
                    seg=ast.get_source_segment(s, st) or ''
                    hits.append((f,st.lineno,seg.replace('\n',' ')[:200]))
print('=== DISCARDED, NO check= (%d) ==='%len(hits))
for h in hits: print('%s:%d | %s'%h)
