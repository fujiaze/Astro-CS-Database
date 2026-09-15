
import re, json, os, collections
defs=json.load(open('问题扫描/_cache/v20_defs.json'))
# For each def, extract body by brace matching from the '{' at/after the sig line.
def body_lines(path, lineno):
    src=open(path,encoding='utf-8',errors='replace').read()
    lines=src.split('\n')
    i=lineno-1
    # find opening brace (may be on following lines)
    j=i
    depth=0; started=False; buf=[]
    while j < len(lines) and j < i+4000:
        l=lines[j]
        # crude comment strip for // only
        code=re.sub(r'//.*$','',l)
        for ch in code:
            if ch=='{':
                depth+=1; started=True
            elif ch=='}':
                depth-=1
        if started:
            buf.append(code)
            if depth<=0 and j>i:
                break
        elif j>i and '{' not in code and depth==0:
            # signature continuation without body -> abort
            if re.search(r';\s*$', code): return None, lines
        j+=1
    return '\n'.join(buf), lines

def parse_params(s):
    # split top-level commas
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
        if not p: continue
        m=re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])?\s*$', p)
        if m: names.append(m.group(1))
        else: names.append(None)
    return names

results=[]
for path,lineno,name,params in defs:
    body, lines = body_lines(path, lineno)
    if body is None: continue
    names=parse_params(params)
    # (void)x casts
    voids=set(re.findall(r'\(void\)\s*([A-Za-z_][A-Za-z0-9_]*)', body))
    unused=[]
    for n in names:
        if not n: continue
        # count references excluding the signature line itself
        refs=len(re.findall(r'\b'+re.escape(n)+r'\b', body))
        # signature text is included in body if '{' was on sig line -> subtract 1 for decl occurrence
        if refs<=1 and n in voids:
            unused.append((n,'void-cast'))
        elif refs<=1:
            unused.append((n,'unref'))
    if unused:
        results.append((path,lineno,name,unused))
print('defs with >=1 unreferenced param:', len(results))
cnt=collections.Counter()
for path,lineno,name,unused in results:
    for n,kind in unused: cnt[kind]+=1
print('totals:', dict(cnt))
json.dump(results, open('问题扫描/_cache/v20_unused_params.json','w'), indent=0)
byfile=collections.Counter(os.path.basename(r[0]) for r in results)
for k,v in byfile.most_common(30): print(' ',k,v)
