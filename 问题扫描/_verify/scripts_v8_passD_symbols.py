
import re, json, subprocess, collections
recs = json.load(open('问题扫描/_verify/_v8_comment_lines.json', encoding='utf-8'))
prose = [r for r in recs if r['kind'] in ('cmt-line','cmt-trailing','doc-line')]
# path::symbol anchors  e.g. lib/x/y.cpp::foo  or module::func
SY = re.compile(r'([A-Za-z0-9_./\-]+\.(?:cpp|c|h|hpp|py|md))::([A-Za-z_][A-Za-z0-9_]*)')
BARE = re.compile(r'(?<![\w:>):.])([a-z_][a-z0-9_]{6,})\s*[::]\s*([A-Za-z_~][A-Za-z0-9_]{2,})')
tracked = subprocess.run(['git','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.split('\n')
tracked = [t for t in tracked if t.endswith(('.cpp','.c','.h','.hpp','.py'))]
cache = {}
def filetext(p):
    if p not in cache:
        try: cache[p]=open(p,encoding='utf-8',errors='replace').read()
        except Exception: cache[p]=None
    return cache[p]
whole = subprocess.run(['git','--no-optional-locks','grep','-c','-e','x','--name-only','HEAD'] ,capture_output=True,text=True)
missing=[]
seen=set()
for r in prose:
    t=r['t']
    for m in SY.finditer(t):
        f,sym=m.group(1),m.group(2)
        cands=[c for c in tracked if c==f or c.endswith('/'+f) or c.endswith(f)]
        key=(r['f'],r['n'],f,sym)
        if key in seen: continue
        seen.add(key)
        ok=False; note=''
        if cands:
            txt=''
            for c in cands[:3]:
                tx=filetext(c)
                if tx: txt+=tx
            if re.search(r'\b'+re.escape(sym)+r'\b', txt): ok=True
            else: note='file-exists-symbol-absent'
        else:
            note='file-absent'
        if not ok:
            missing.append((r['c'], r['f'], r['n'], f, sym, note))
    for m in BARE.finditer(t):
        a,b=m.group(1),m.group(2)
        if a in ('run','lib','docs','tests','tools','http','https','std'): continue
        # search repo-wide for the symbol b
        g=subprocess.run(['git','--no-optional-locks','grep','-l','-e',b,'--','lib','cli','include','tools','tests','docs','ci'],capture_output=True,text=True).stdout.strip()
        if not g:
            missing.append((r['c'], r['f'], r['n'], a+'::'+b, '(bare symbol)', 'symbol-nowhere'))
print('path::symbol / a::b anchor failures:', len(missing))
for c,f,n,p,s,note in missing:
    print('  %s %s:%d -> %s::%s  [%s]' % (c,f,n,p,s,note))
