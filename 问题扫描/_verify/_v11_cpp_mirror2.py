
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

def find_struct_fields(path, name, mode='typedef'):
    raw=open(path,encoding='utf-8',errors='replace').read()
    txt=strip_comments(raw)
    if mode=='typedef':
        m=re.search(r'typedef\s+struct[^{;]*\{', txt)
        while m:
            oi=m.end()-1; ci=block_at(txt,oi)
            tail=txt[ci:ci+80]
            nm=re.match(r'\}\s*(\w+)\s*;', tail)
            if nm and nm.group(1)==name:
                return parse_body(txt[oi+1:ci], raw, m.start()), raw[:m.start()].count('\n')+1
            m=re.search(r'typedef\s+struct[^{;]*\{', txt, m.end())
        return None,None
    else:
        for m in re.finditer(r'\b(class|struct)\s+'+re.escape(name)+r'\b[^{;]*\{', txt):
            oi=m.end()-1; ci=block_at(txt,oi)
            body=txt[oi+1:ci]
            fields=[]
            for stmt in re.split(r';', body):
                s=re.sub(r'\s+',' ',stmt).strip()
                if not s: continue
                if re.match(r'^(public|private|protected)\b', s): continue
                if '{' in s or '}' in s: continue
                if '(' in s: continue
                if s.startswith(('#','template','friend','using','static_assert','return','//')): continue
                mm=re.match(r'^(.*?)\b(\w+)\s*(?:=.*)?$', s)
                if not mm: continue
                base=re.sub(r'\s+',' ',mm.group(1).strip())
                if base in ('', 'const', 'static'): continue
                fields.append((mm.group(2), base))
            return fields, raw[:m.start()].count('\n')+1
        return None,None

def parse_body(body, raw, start):
    out=[]
    for stmt in body.split(';'):
        s=re.sub(r'\s+',' ',strip_comments(stmt)).strip()
        if not s: continue
        am=re.match(r'^(.*?)\b(\w+)\s*\[\s*(\d+)\s*\]$', s)
        if am: out.append((am.group(2), am.group(1).strip()+'['+am.group(3)+']')); continue
        mm=re.match(r'^(.*?)\b(\w+)$', s)
        if mm: out.append((mm.group(2), mm.group(1).strip()))
    return out

A,la=find_struct_fields('lib/plate_solve/cpp/ipv/include/ipv_api.h','IpvParams','typedef')
B,lb=find_struct_fields('lib/plate_solve/cpp/ipv/include/ipv_types.h','IPVSolverParams','class')
print(f'C ABI  IpvParams @ipv_api.h:{la}  n={len(A)}')
print(f'C++    IPVSolverParams @ipv_types.h:{lb} n={len(B)}')
fa=[f for f,_ in A]; fb=[f for f,_ in B]
print('\n只在 C ABI :', [x for x in fa if x not in fb])
print('只在 C++侧:', [x for x in fb if x not in fa])
print('共有字段顺序是否一致(去掉只在一侧的):', [x for x in fa if x in fb]==[x for x in fb if x in fa])
print('\n并排:')
for i in range(max(len(A),len(B))):
    a=f'{A[i][0]}:{A[i][1]}' if i<len(A) else ''
    b=f'{B[i][0]}:{B[i][1]}' if i<len(B) else ''
    flag = 'OK' if i<len(A) and i<len(B) and A[i][0]==B[i][0] else '<<<'
    print(f'  {i:2d} {flag:4s} C={a:46s} CPP={b}')
print('\n### C++ 侧独有字段是否被 to_c_result/from_c 映射丢弃')
src=open('lib/plate_solve/cpp/ipv/src/ipv_entry.cpp',encoding='utf-8',errors='replace').read()
for ln,f in enumerate(src.split('\n'),1):
    if 'good_rms_threshold' in f or ('to_solver_params' in f) or ('from_c_params' in f) or ('to_c_params' in f):
        print(f'  ipv_entry.cpp:{ln}: {f.strip()[:150]}')
