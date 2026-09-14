
import re
def strip_comments(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s); return s
def block_at(txt, open_idx):
    d=0
    for i in range(open_idx, len(txt)):
        if txt[i]=='{': d+=1
        elif txt[i]=='}':
            d-=1
            if d==0: return i
    return -1
TY=re.compile(r'typedef\s+struct[^{;]*\{')
def find_typedef(path, name):
    raw=open(path,encoding='utf-8',errors='replace').read()
    txt=strip_comments(raw)
    for m in TY.finditer(txt):
        oi=m.end()-1; ci=block_at(txt,oi)
        if ci<0: continue
        nm=re.match(r'\}\s*(\w+)\s*;', txt[ci:ci+80])
        if nm and nm.group(1)==name:
            return parse_body(txt[oi+1:ci]), raw[:m.start()].count('\n')+1
    return None,None
def parse_body(body):
    out=[]
    for stmt in body.split(';'):
        s=re.sub(r'\s+',' ',strip_comments(stmt)).strip()
        if not s: continue
        am=re.match(r'^(.*?)\b(\w+)\s*\[\s*(\d+)\s*\]$', s)
        if am: out.append((am.group(2), re.sub(r'\s+',' ',am.group(1).strip())+'['+am.group(3)+']')); continue
        mm=re.match(r'^(.*?)\b(\w+)$', s)
        if mm: out.append((mm.group(2), re.sub(r'\s+',' ',mm.group(1).strip())))
    return out
def find_class(path, name):
    raw=open(path,encoding='utf-8',errors='replace').read()
    txt=strip_comments(raw)
    for m in re.finditer(r'\b(?:class|struct)\s+'+re.escape(name)+r'\b[^{;]*\{', txt):
        oi=m.end()-1; ci=block_at(txt,oi)
        body=txt[oi+1:ci]
        out=[]
        depth=0
        for stmt in body.split(';'):
            s=re.sub(r'\s+',' ',stmt).strip()
            if not s: continue
            if '{' in s or '}' in s: depth=0; continue
            if re.match(r'^(public|private|protected)\b', s): continue
            if '(' in s: continue
            if s.startswith(('#','template','friend','using','static_assert','return')): continue
            mm=re.match(r'^(.*?)\b(\w+)\s*(?:=.*)?$', s)
            if not mm: continue
            base=re.sub(r'\s+',' ',mm.group(1).strip())
            if base in ('','static','const'): continue
            out.append((mm.group(2), base))
        return out, raw[:m.start()].count('\n')+1
    return None,None
A,la=find_typedef('lib/plate_solve/cpp/ipv/include/ipv_api.h','IpvParams')
B,lb=find_class('lib/plate_solve/cpp/ipv/include/ipv_types.h','IPVSolverParams')
print(f'C ABI IpvParams @ipv_api.h:{la} n={len(A)}   |   C++ IPVSolverParams @ipv_types.h:{lb} n={len(B)}')
fa=[f for f,_ in A]; fb=[f for f,_ in B]
print('只在 C ABI :', [x for x in fa if x not in fb])
print('只在 C++侧:', [x for x in fb if x not in fa])
print('共有部分顺序一致:', [x for x in fa if x in fb]==[x for x in fb if x in fa])
print()
for i in range(max(len(A),len(B))):
    a=f'{A[i][0]} : {A[i][1]}' if i<len(A) else ''
    b=f'{B[i][0]} : {B[i][1]}' if i<len(B) else ''
    flag='OK' if i<len(A) and i<len(B) and A[i][0]==B[i][0] else '<<<'
    print(f'{i:2d} {flag:4s} C ={a:48s} CPP={b}')
