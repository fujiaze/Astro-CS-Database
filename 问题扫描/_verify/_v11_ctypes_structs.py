
import os, re, json, sys
ROOT = os.getcwd()
EXCLUDE = {'.git','build','out','run','worktrees','artifacts','evidence','GaiaDR3','GaiaDR3SP','BASS DR3','AstroCS.wiki','logs','__pycache__','.pytest_cache','node_modules','third_party','_v11_scratch'}
rows=[]
def walk():
    for dirpath, dirnames, filenames in os.walk(ROOT):
        rel = os.path.relpath(dirpath, ROOT)
        parts = set(rel.split(os.sep))
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE and not (d.endswith('.egg-info')) and d not in {'Testing','Testing_archive'}]
        if parts & EXCLUDE: continue
        for fn in filenames:
            yield os.path.join(rel, fn) if rel!='.' else fn
CT = re.compile(r'class\s+(\w+)\s*\(\s*ctypes\.Structure\s*\)')
for p in walk():
    if not p.endswith('.py'): continue
    try:
        txt = open(p, encoding='utf-8', errors='replace').read()
    except Exception: continue
    if 'ctypes.Structure' not in txt: continue
    for m in CT.finditer(txt):
        ln = txt[:m.start()].count('\n')+1
        rows.append((m.group(1), p, ln))
print('### ctypes.Structure 类定义全量:', len(rows))
for n,p,l in sorted(rows):
    print(f'{n}\t{p}:{l}')
