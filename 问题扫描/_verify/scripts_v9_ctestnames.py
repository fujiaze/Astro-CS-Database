import json, os, re, glob, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
ch = {c['id']:c for c in d['checks']}
# collect add_test(NAME ...) across repo (exclude build/, run/, worktrees, Third*)
names = {}
pat = re.compile(r'add_test\s*\(\s*(?:NAME\s+)?([A-Za-z0-9_.\-]+)')
for root, dirs, files in os.walk('.'):
    rp = root.replace(os.sep,'/')
    if any(rp.startswith(p) for p in ['./build','./run','./worktrees','./Testing','./.git','./out','./GaiaDR3','./GaiaDR3SP','./third_party','./BASS DR3','./问题扫描','./_cache','./artifacts','./evidence','./reports','./工程控制','./设计大纲','./logs','./astros_cs']):
        dirs[:] = []
        continue
    for f in files:
        if f == 'CMakeLists.txt' or f.endswith('.cmake'):
            p = os.path.join(root,f)
            try: txt = open(p, encoding='utf-8', errors='replace').read()
            except Exception: continue
            for m in pat.finditer(txt):
                names.setdefault(m.group(1), []).append(p)
print('REGISTERED ctest NAME count:', len(names))
reg = set(names)
print()
print('== ctest_targets existence check ==')
bad=[]
for cid,c in ch.items():
    for t in c.get('ctest_targets',[]):
        ok = t in reg
        if not ok: bad.append((cid,t))
        print(('  OK  ' if ok else '  NO  '), cid, '->', t, ('' if ok else '  | near: %s' % [n for n in reg if t.split('_')[0] in n][:5]))
print('MISSING_TARGETS', len(bad))
