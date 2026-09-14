
import re, sys
def strip_comments(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s); return s
def fields_of(path, name, kind='struct'):
    raw=open(path,encoding='utf-8',errors='replace').read()
    txt=strip_comments(raw)
    if kind=='struct':
        pat=r'typedef\s+struct(?:\s+\w+)?\s*\{(.*?)\}\s*'+name+r'\s*;'
    else:
        pat=r'(?:class|struct)\s+'+name+r'\s*\{(.*?)\n\s*\};'
    m=re.search(pat, txt, re.S)
    if not m: return None, None
    line=raw[:m.start()].count('\n')+1
    out=[]
    for stmt in m.group(1).split(';'):
        s=re.sub(r'\s+',' ',stmt).strip()
        if not s or s.startswith(('public','private','protected')): continue
        if '(' in s and ')' in s.split('=')[0]: continue      # 方法
        if s.startswith(('//','template','#','static_assert','friend','using')): continue
        mm=re.match(r'^(.*?)\b(\w+)\s*(?:=\s*[^;]*)?$', s)
        if not mm: continue
        base=mm.group(1).strip(); fld=mm.group(2)
        if base in ('return',): continue
        out.append((fld, re.sub(r'\s+',' ',base)))
    return out, line

A,la = fields_of('lib/plate_solve/cpp/ipv/include/ipv_api.h','IpvParams','struct')
B,lb = fields_of('lib/plate_solve/cpp/ipv/include/ipv_types.h','IPVSolverParams','class')
if B is None: A2,la2=fields_of('lib/plate_solve/cpp/ipv/include/ipv_types.h','IPVSolverParams','struct'); print('retry',A2 is not None)
print(f'C ABI  IpvParams        @ ipv_api.h:{la}  字段 {len(A)}')
print(f'C++ 内 IPVSolverParams  @ ipv_types.h:{lb} 字段 {len(B) if B else 0}')
fa=[f for f,_ in A]; fb=[f for f,_ in (B or [])]
print('\n只在 C ABI:', [x for x in fa if x not in fb])
print('只在 C++ 侧:', [x for x in fb if x not in fa])
print('顺序是否一致:', fa==fb)
print('\n逐字段并排（前 30）')
for i in range(max(len(fa),len(fb))):
    a = f'{fa[i]:32s} {A[i][1]:14s}' if i<len(fa) else ' '*47
    b = f'{fb[i]:32s} {B[i][1]:14s}' if i<len(fb) else ''
    mark = '' if (i<len(fa) and i<len(fb) and fa[i]==fb[i]) else '   <<< 错位'
    print(f'  {i:2d} | {a} | {b}{mark}')
