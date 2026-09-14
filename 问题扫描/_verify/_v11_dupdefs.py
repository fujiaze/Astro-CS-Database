
import os, re, collections, sys
ROOT=os.getcwd()
EXCL=re.compile(r'(^|/)(build|out|run|worktrees|artifacts|evidence|GaiaDR3|GaiaDR3SP|BASS DR3|AstroCS\.wiki|logs|third_party|\.git|__pycache__|engineering|reports|docs|问题扫描|Testing)(/|$)')
def strip_cmt(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s); return s
def block_at(t,i):
    d=0
    for k in range(i,len(t)):
        if t[k]=='{': d+=1
        elif t[k]=='}':
            d-=1
            if d==0: return k
    return -1
TY=re.compile(r'typedef\s+struct(?:\s+\w+)?\s*\{')
recs=collections.defaultdict(list)
nf=0
for dp,dn,fn in os.walk(ROOT):
    rel=os.path.relpath(dp,ROOT)
    if EXCL.search(rel+'/'): continue
    dn[:] = [d for d in dn if not EXCL.search(d+'/')]
    for f in fn:
        if not f.endswith(('.h','.hpp','.c','.cpp')): continue
        p=os.path.join(rel,f)
        try: raw=open(p,encoding='utf-8',errors='replace').read()
        except Exception: continue
        nf+=1
        txt=strip_cmt(raw)
        for m in TY.finditer(txt):
            oi=m.end()-1; ci=block_at(txt,oi)
            if ci<0: continue
            nm=re.match(r'\}\s*(\w+)\s*;', txt[ci:ci+90])
            if not nm: continue
            body=txt[oi+1:ci]
            fields=[]
            for stmt in body.split(';'):
                s=re.sub(r'\s+',' ',strip_cmt(stmt)).strip()
                if not s: continue
                mm=re.match(r'^(.*?)\b(\w+)\s*(?:\[[^\]]*\])?(?:=.*)?$', s)
                if not mm: continue
                base=re.sub(r'\b(const|volatile|struct|unsigned|signed|long)\b',' ',mm.group(1))
                base=re.sub(r'\s+',' ',base).strip()
                fields.append((mm.group(2), base))
            if fields: recs[nm.group(1)].append((p, raw[:m.start()].count('\n')+1, tuple(fields)))
print(f'### 扫描源文件 {nf} 个；同名 typedef struct 组 {sum(1 for k,v in recs.items() if len(v)>1)} 个')
bad=[]
for name, lst in sorted(recs.items()):
    if len(lst)<2: continue
    sigs={f for _,_,f in lst}
    same = len(sigs)==1
    tag = '同字段表' if same else '★字段表不一致'
    print(f'\n-- {name}: {len(lst)} 份定义 [{tag}]')
    for p,l,f in lst: print(f'     {p}:{l}  n={len(f)}')
    if not same:
        base=lst[0][2]
        for p,l,f in lst[1:]:
            only_a=[x for x,_ in base if x not in [y for y,_ in f]]
            only_b=[y for y,_ in f if y not in [x for x,_ in base]]
            order_same = [x for x,_ in base]==[y for y,_ in f]
            print(f'     差异 vs {lst[0][0]}: 只在首份={only_a}  只在{p}={only_b}  顺序一致={order_same}')
        bad.append(name)
print('\n### 字段表不一致的同名结构体总数:', len(bad), bad)
