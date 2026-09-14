
import ast, os, re, collections
ROOT=os.getcwd()
EXCL=re.compile(r'(^|/)(build|out|run|worktrees|artifacts|evidence|GaiaDR3|GaiaDR3SP|BASS DR3|AstroCS\.wiki|logs|third_party|\.git|__pycache__|engineering|reports|docs|问题扫描|Testing)(/|$)')
hits=collections.defaultdict(list)
for dp,dn,fn in os.walk(ROOT):
    rel=os.path.relpath(dp,ROOT)
    if EXCL.search(rel+'/'): continue
    for f in fn:
        if not f.endswith('.py'): continue
        p=os.path.join(rel,f)
        try: tree=ast.parse(open(p,encoding='utf-8',errors='replace').read())
        except Exception: continue
        for nd in ast.walk(tree):
            if isinstance(nd, ast.ClassDef) and any('Structure' in ast.unparse(b) for b in nd.bases):
                hits[nd.name].append((p, nd.lineno))
tot=sum(len(v) for v in hits.values())
print('### 按当前树（HEAD 2f03dd89，排除 问题扫描/ 与影子树）重算：')
print('   ctypes.Structure 定义 =', tot, '处 / 不同名 =', len(hits), '个 / 文件 =', len({p for v in hits.values() for p,_ in v}), '个')
for n,v in sorted(hits.items()):
    print(f'   {n:22s} x{len(v)}  ' + ', '.join(f'{p}:{l}' for p,l in sorted(v)))
