
import ast
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
out=[]
for f in files:
    try: s=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    if 'returncode' not in s and 'subprocess' not in s: continue
    try: tree=ast.parse(s)
    except SyntaxError: continue
    lines=s.splitlines()
    # find names bound to subprocess calls
    bound={}
    class V(ast.NodeVisitor):
        def visit_Assign(self,node):
            val=node.value
            src_txt=ast.unparse(val)
            if isinstance(val,ast.Call):
                fn=val.func
                nm=ast.unparse(fn)
                if 'subprocess' in nm or 'Popen' in nm or 'run(' in nm:
                    for t in node.targets:
                        if isinstance(t,ast.Name): bound[t.id]=(node.lineno, src_txt[:80])
            self.generic_visit(node)
        def visit_For(self,node):
            self.generic_visit(node)
    V().visit(tree)
    # collect all read contexts of returncode and names
    reads={}
    for node in ast.walk(tree):
        if isinstance(node,ast.Attribute) and node.attr=='returncode':
            base=ast.unparse(node.value)
            reads.setdefault(base,[]).append(node.lineno)
        if isinstance(node,ast.Name) and node.id in bound and isinstance(node.ctx,ast.Load):
            reads.setdefault(node.id,[]).append(node.lineno)
    for nm,(ln,srcx) in sorted(bound.items(), key=lambda x:x[1][0]):
        r=reads.get(nm,[])
        rc_reads=[l for l in r if l!=ln]
        # detect multiple assignment (overwrite)
        multi=[ (ln2) for n2,(ln2,_) in bound.items() if n2!=nm ]
        out.append((f,ln,nm,srcx,len(rc_reads),sorted(set(rc_reads))))
print('=== subprocess result bindings and #reads ===')
for f,ln,nm,sx,c,rd in out:
    if c<=2:
        print('%s:%d %s = %s | reads=%d %s'%(f,ln,nm,sx,c,rd))
