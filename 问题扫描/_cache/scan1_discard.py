
import ast, sys, io, os
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
MOD=('subprocess.run','subprocess.check_output','subprocess.check_call','subprocess.call','subprocess.Popen','os.system','os.popen')
def name_of(node):
    return ast.unparse(node) if hasattr(ast,'unparse') else '?'
discarded=[]
nocheck=[]
for f in files:
    try: src_txt=open(f,encoding='utf-8',errors='replace').read()
    except Exception as e: continue
    try: tree=ast.parse(src_txt, filename=f)
    except SyntaxError as e:
        print('PARSEFAIL', f, e); continue
    lines=src_txt.splitlines()
    for st in ast.walk(tree):
        if isinstance(st, ast.Expr) and isinstance(st.value, ast.Call):
            n=name_of(st.value.func)
            if n in MOD or any(n.endswith('.'+m.split('.')[1]) for m in MOD):
                discarded.append((f, st.lineno, n, lines[st.lineno-1].strip() if st.lineno-1<len(lines) else ''))
print('=== DISCARDED SUBPROCESS CALLS (%d) ===' % len(discarded))
for d in discarded:
    print('%s:%d  [%s]  %s' % (d[0], d[1], d[2], d[3][:150]))
