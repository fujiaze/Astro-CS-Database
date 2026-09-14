
import json, os, re, ast, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
TOOLS = ['g++','gcc','clang++','clang','cmake','ctest','make','nm','objdump','dumpbin','taskset','zstd','tar','llvm-profdata','llvm-cov','pytest','bash','sh','python','python3']
def roots_for(c):
    cmd=c['command']; roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    return roots
def argv0(node):
    # node is Call; try to extract first argv element literal
    try:
        args=node.args
    except Exception: return None
    cand=None
    if args and isinstance(args[0], (ast.List, ast.Tuple)):
        el=args[0].elts[0]
        if isinstance(el, ast.Constant) and isinstance(el.value,str): cand=el.value
    if cand is None:
        for kw in args[1:] if len(args)>1 else []:
            pass
        for kw in node.keywords or []:
            if kw.arg in ('argv','cmd','command'):
                v=kw.value
                if isinstance(v,(ast.List,ast.Tuple)) and v.elts and isinstance(v.elts[0],ast.Constant) and isinstance(v.elts[0].value,str):
                    cand=v.elts[0].value
    if cand is None and args and isinstance(args[0], ast.Constant) and isinstance(args[0].value,str):
        cand=args[0].value.split(' ')[0]
    return cand
FUNC={'run','Popen','check_output','check_call','call','run_cmd'}
out=[]
for c in cs:
    roots=roots_for(c)
    files=[f for f in sorted(tracked) if f.endswith('.py') and any(f==r or f.startswith(r+'/') for r in roots)]
    used={}
    for f in files:
        src=open(f,encoding='utf-8',errors='replace').read()
        try: tree=ast.parse(src)
        except SyntaxError: continue
        for n in ast.walk(tree):
            if isinstance(n, ast.Call):
                fn = n.func
                name = getattr(fn,'attr',None) or getattr(fn,'id',None)
                if name in FUNC:
                    a=argv0(n)
                    if a:
                        base=os.path.basename(a)
                        if base in TOOLS: used.setdefault(base,set()).add(f)
                    else:
                        # f-string / variable: check literal occurrence in the call source segment
                        try: seg=ast.get_source_segment(src,n) or ''
                        except Exception: seg=''
                        for t in TOOLS:
                            if re.search(r'["\x27]'+re.escape(t)+r'["\x27]', seg): used.setdefault(t,set()).add(f)
    decl=set()
    for t in c.get('prerequisite_tools',[]): decl.add(t.split(':')[-1])
    if used:
        und=sorted(t for t in used if t not in decl)
        print('### %-26s declared=%-24s invoked=%s' % (c['id'], sorted(decl) or '-', sorted(used)))
        for t in sorted(used):
            mark = 'DECLARED' if t in decl else 'UNDECLARED'
            print('    %-14s %-11s files=%d e.g. %s' % (t, mark, len(used[t]), sorted(used[t])[0]))
