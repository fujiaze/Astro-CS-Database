
import os, re, ast
ROOT=os.getcwd()
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
recs={}
EXCL=re.compile(r'(^|/)(build|out|run|worktrees|artifacts|evidence|GaiaDR3|GaiaDR3SP|BASS DR3|AstroCS\.wiki|logs|third_party|\.git|__pycache__|engineering|reports|docs|问题扫描|Testing)(/|$)')
pubstructs=[]
for dp,dn,fn in os.walk(ROOT):
    rel=os.path.relpath(dp,ROOT)
    if EXCL.search(rel+'/'): continue
    for f in fn:
        if not f.endswith('.h'): continue
        p=os.path.join(rel,f)
        if '/include/' not in '/'+p: continue
        raw=open(p,encoding='utf-8',errors='replace').read()
        t=strip_cmt(raw)
        for m in TY.finditer(t):
            oi=m.end()-1; ci=block_at(t,oi)
            if ci<0: continue
            nm=re.match(r'\}\s*(\w+)\s*;', t[ci:ci+90])
            if not nm: continue
            body=t[oi+1:ci]
            nf=len([s for s in body.split(';') if re.sub(r'\s+','',s)])
            ss = bool(re.search(r'\bstruct_size\b', body)) or bool(re.search(r'\bacs_head\b', body))
            pubstructs.append((p, raw[:m.start()].count('\n')+1, nm.group(1), nf, ss))
print('### 统计口径（V11-S1）：include/ 路径公共头里的 typedef struct')
print('   总数 =', len(pubstructs))
print('   带 struct_size 或 acs_head =', sum(1 for x in pubstructs if x[4]), f'({100*sum(1 for x in pubstructs if x[4])/len(pubstructs):.1f}%)')
print('   无自描述 =', sum(1 for x in pubstructs if not x[4]))
CONS = {'IpvParams','IpvWcsResult','SDetParams','DPSFFitParams','DPSFFitResult','AstroSphereTileView','AioHipsSnrPoint','GaiaSpectrumStar','acs_fio_header_v1','acs_fio_keyword_v1','acs_fio_trace_hooks_v1'}
print('\n### 其中【被跨语言消费】的（ctypes 镜像）：')
found=set()
for p,l,n,nf,ss in pubstructs:
    if n in CONS:
        print(f'   {n:24s} 字段{nf:3d} struct_size={"有" if ss else "无"}  {p}:{l}')
        found.add(n)
print('   未在 include 面找到的:', sorted(CONS-found), '（GaiaSpectrumStar 家族在 src/gaia_client.h —— 导出但头不在 include/）')
# gate1/gate2 调用的 f64 与 sdet_free_detect_ex arity
for hdr,nm in [('lib/dynamic_psf/include/dynamic_psf.h','dpsf_fit_batch_f64'),('lib/star_detector/include/star_detector.h','sdet_free_detect_ex'),('lib/star_detector/include/star_detector.h','sdet_detect_ex')]:
    raw=open(os.path.join(ROOT,hdr),encoding='utf-8',errors='replace').read()
    t=strip_cmt(raw)
    m=re.search(re.escape(nm)+r'\s*\((.*?)\)\s*;', t, re.S)
    if not m: m=re.search(re.escape(nm)+r'\s*\((.*?)\)\s*\w*\s*;', t, re.S)
    if m:
        a=[x for x in re.split(r',(?![^()]*\))', m.group(1)) if x.strip()]
        print(f'\n   {nm} C 参数个数 = {len(a)}  @ {hdr}:{raw[:m.start()].count(chr(10))+1}')
        for x in a: print('       -', re.sub(r"\s+",' ',x.strip()))
# tests/io 采集真实用例数（AST）
print('\n### tests/io 各脚本的 TestCase 与测试方法数（决定 UT-IO 实采数）')
for f in sorted(os.listdir('tests/io')):
    if not f.endswith('.py'): continue
    p=os.path.join('tests/io',f)
    try: tree=ast.parse(open(p,encoding='utf-8',errors='replace').read())
    except Exception as e: print('   parse fail',f,e); continue
    n_tc=0; n_m=0
    for nd in ast.walk(tree):
        if isinstance(nd, ast.ClassDef):
            bases=[ast.unparse(b) for b in nd.bases]
            if any('TestCase' in b for b in bases):
                n_tc+=1
                n_m+=sum(1 for s in nd.body if isinstance(s,(ast.FunctionDef,)) and s.name.startswith('test'))
    print(f'   {f:36s} TestCase={n_tc} test方法={n_m}')
