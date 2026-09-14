
import re
def strip_comments(s):
    s=re.sub(r'/\*.*?\*/',' ',s,flags=re.S); s=re.sub(r'//[^\n]*','',s); return s
def block_at(txt, i):
    d=0
    for k in range(i,len(txt)):
        if txt[k]=='{': d+=1
        elif txt[k]=='}':
            d-=1
            if d==0: return k
    return -1
TY=re.compile(r'typedef\s+struct(?:\s+\w+)?\s*\{')
raw=open('lib/plate_solve/cpp/ipv/include/ipv_api.h',encoding='utf-8',errors='replace').read()
t=strip_comments(raw)
for m in TY.finditer(t):
    oi=m.end()-1; ci=block_at(t,oi)
    nm=re.match(r'\}\s*(\w+)\s*;', t[ci:ci+90])
    if not nm: continue
    body=t[oi+1:ci]
    print(nm.group(1), '| struct_size?', bool(re.search(r'\bstruct_size\b', body)), '| abi_version?', bool(re.search(r'\babi_version\b', body)))
    if nm.group(1)=='IpvParams':
        print('   body tail 160:', repr(body[-160:]))
        print('   分号段数:', len([s for s in body.split(';') if re.sub(r'\s+','',s)]))
