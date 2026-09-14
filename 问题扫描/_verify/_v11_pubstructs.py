
import os, re, json
ROOT=os.getcwd()
EXCL = re.compile(r'(^|/)(build|out|run|worktrees|artifacts|evidence|GaiaDR3|GaiaDR3SP|BASS DR3|AstroCS\.wiki|logs|third_party|\.git|__pycache__|engineering|reports|docs|问题扫描)(/|$)')
def strip_comments(s):
    s = re.sub(r'/\*.*?\*/',' ',s,flags=re.S)
    s = re.sub(r'//[^\n]*','',s)
    return s
HDRDIRS = ['include','lib','cli','runtime','modules','providers','contracts','cmake']
pub=[]
for d in HDRDIRS:
    for dirpath,dirnames,filenames in os.walk(os.path.join(ROOT,d)):
        rel=os.path.relpath(dirpath,ROOT)
        if EXCL.search(rel+'/'): continue
        for fn in filenames:
            if fn.endswith(('.h','.hpp')): pub.append(os.path.join(rel,fn))
print('### 公共/模块头文件总数:', len(pub))
# 只看非 private 头（include/ 与 */include/ 视为公共面）
public_iface = [p for p in pub if re.search(r'(^|/)include/', p)]
print('### 其中位于 include/ 路径（公共接口面）:', len(public_iface))
STR = re.compile(r'typedef\s+struct(?:\s+\w+)?\s*\{(.*?)\}\s*(\w+)\s*;', re.S)
rows=[]
for p in public_iface:
    raw=open(os.path.join(ROOT,p),encoding='utf-8',errors='replace').read()
    txt=strip_comments(raw)
    for m in STR.finditer(txt):
        name=m.group(2); body=m.group(1)
        line = raw[:m.start()].count('\n')+1
        has_ss = bool(re.search(r'\bstruct_size\b', body))
        has_abi = bool(re.search(r'\abi_version\b', body))
        has_head = bool(re.search(r'\bacs_head\b', body))
        nf = len([s for s in body.split(';') if s.strip()])
        rows.append((p,line,name,nf,has_ss or has_head,has_abi))
print(f'\n### include 路径公共头里的 typedef struct 总数: {len(rows)}')
with_ss=[r for r in rows if r[4]]
print(f'### 其中带 struct_size/acs_head: {len(with_ss)}  ({100*len(with_ss)/max(1,len(rows)):.1f}%)')
print(f'### 不带: {len(rows)-len(with_ss)}')
# 跨语言消费集
PYCT = set("""IpvParams IpvWcsResult AioHipsSnrPoint AstroSphereTileView GaiaSpectrumStar SDetParams DPSFFitParams DPSFFitResult FioHeader FioKeyword TraceHooks""".split())
print('\n### 被 ctypes 消费的 C 结构体 struct_size 情况')
for p,line,name,nf,ss,abi in rows:
    if name in PYCT:
        print(f'  {name:24s} {"有" if ss else "无"}struct_size  字段{nf:3d}  {p}:{line}')
names_in_repo={r[2] for r in rows}
print('  !! 未在 include 公共头找到的镜像名:', sorted(PYCT - names_in_repo))
print('\n### 全部无 struct_size 的公共 typedef struct（清单，前 60）')
n=0
for p,line,name,nf,ss,abi in rows:
    if not ss:
        n+=1
        if n<=60: print(f'  {name:34s} 字段{nf:3d}  {p}:{line}')
print(f'  ... 共 {n} 个')
json.dump([{'path':p,'line':l,'name':nm,'fields':nf,'struct_size':ss} for p,l,nm,nf,ss,ab in rows], open(os.path.join(ROOT,'问题扫描/_verify/_v11_structs.json'),'w'), ensure_ascii=False, indent=1)
print('\n已写 问题扫描/_verify/_v11_structs.json')
