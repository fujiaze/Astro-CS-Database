
import re, json, os, collections
defs=json.load(open('问题扫描/_cache/v20_defs.json'))
def get_body(path, lineno):
    src=open(path,encoding='utf-8',errors='replace').read()
    lines=src.split('\n')
    i=lineno-1
    depth=0; started=False; buf=[]
    sig_in_body=False
    j=i
    while j < len(lines) and j < i+6000:
        raw=lines[j]
        code=re.sub(r'//.*$','',raw)
        code=re.sub(r'/\*.*?\*/','',code)
        if not started:
            if '{' in code:
                started=True; sig_in_body=(j==i)
        if started:
            buf.append(code)
            for ch in code:
                if ch=='{': depth+=1
                elif ch=='}': depth-=1
            if depth<=0: break
        else:
            if re.search(r';\s*$', code) or j>i+3: return None, False
        j+=1
    return '\n'.join(buf), sig_in_body

def parse_params(s):
    parts=[]; depth=0; cur=''
    for ch in s:
        if ch in '([{': depth+=1
        if ch in ')]}': depth-=1
        if ch==',' and depth==0: parts.append(cur); cur=''
        else: cur+=ch
    if cur.strip(): parts.append(cur)
    names=[]
    for p in parts:
        p=p.strip()
        if not p or p=='void': continue
        m=re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])*\s*$', p)
        names.append(m.group(1) if m else None)
    return names

CKEY={'void','const','unsigned','signed','int','char','float','double','long','short','struct','class','typename'}
results=[]
for path,lineno,name,params in defs:
    if name in CKEY: continue
    body,sib=get_body(path,lineno)
    if body is None: continue
    names=parse_params(params)
    voids=set(re.findall(r'\(\s*void\s*\)\s*([A-Za-z_][A-Za-z0-9_]*)', body))
    unused=[]
    for n in names:
        if not n or n in CKEY: continue
        occ=len(re.findall(r'(?<![A-Za-z0-9_.>])'+re.escape(n)+r'\b', body))
        decl=1 if sib else 0
        if occ-decl<=0:
            unused.append((n, 'void-cast' if n in voids else 'unref'))
    if unused:
        results.append((path,lineno,name,params[:120],unused))
print('defs with unreferenced params:', len(results))
c=collections.Counter()
for r_ in results:
    for n,k in r_[4]: c[k]+=1
print('totals:', dict(c))
json.dump(results, open('问题扫描/_cache/v20_unused_params2.json','w'), indent=0)
for r_ in results:
    print(f'{r_[0]}:{r_[1]} {r_[2]} -> {r_[4]}')
