
import re, json, os, collections
# --- robust multi-line signature parser for .c/.cpp files ---
def strip_comments(text):
    text=re.sub(r'/\*.*?\*/','',text,flags=re.S)
    text=re.sub(r'//[^\n]*','',text)
    return text
def split_params(s):
    parts=[];depth=0;cur=''
    for ch in s:
        if ch in '([{<': depth+=1
        if ch in ')]}>': depth-=1
        if ch==',' and depth==0: parts.append(cur);cur=''
        else: cur+=ch
    if cur.strip(): parts.append(cur)
    names=[]
    for p in parts:
        p=p.strip()
        if not p or p=='void': continue
        m=re.search(r'([A-Za-z_][A-Za-z0-9_]*)\s*(\[[^\]]*\])*\s*$',p)
        if m: names.append(m.group(1))
    return names

roots=['lib','providers','runtime','cli']
files=[]
for root in roots:
    for dp,dn,fn in os.walk(root):
        for f in fn:
            if f.endswith(('.c','.cpp','.cc')): files.append(os.path.join(dp,f))

# exported-name sets from headers: names declared inside extern "C" blocks or with EXPORT macros
hdr_names=set()
for dp,dn,fn in os.walk('include'):
    for f in fn:
        if f.endswith(('.h','.hpp')):
            t=strip_comments(open(os.path.join(dp,f),encoding='utf-8',errors='replace').read())
            for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', t): hdr_names.add(m.group(1))
for dp,dn,fn in os.walk('lib'):
    for f in fn:
        if f.endswith(('.h','.hpp')):
            t=strip_comments(open(os.path.join(dp,f),encoding='utf-8',errors='replace').read())
            for m in re.finditer(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(', t): hdr_names.add(m.group(1))
print('header-declared identifiers followed by (:', len(hdr_names))

sigre=re.compile(r'^[A-Za-z_][\w:\s,<>&*\-\)]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(([^;{]*)\)\s*(?:const|noexcept|override|final)?\s*\{', re.M|re.S)
out=[]
for path in files:
    raw=open(path,encoding='utf-8',errors='replace').read()
    t=strip_comments(raw)
    for m in sigre.finditer(t):
        name=m.group(1); params=m.group(2)
        if '\\n' in params and params.count('(')>0: pass
        # find definition body
        start=m.end()-1
        depth=0; j=start
        while j < len(t):
            if t[j]=='{': depth+=1
            elif t[j]=='}':
                depth-=1
                if depth==0: break
            j+=1
        body=t[start:j+1]
        lineno=raw[:m.start()].count('\n')+1
        names=split_params(params)
        if not names: continue
        voids=set(re.findall(r'\(\s*void\s*\)\s*([A-Za-z_][A-Za-z0-9_]*)', body))
        unused=[]
        for n in names:
            occ=len(re.findall(r'(?<![A-Za-z0-9_.>])'+re.escape(n)+r'(?![A-Za-z0-9_])', body))
            # declaration occurrence: params text is in the matched region? we used body from '{', so decl not counted
            if occ==0:
                unused.append((n,'void-cast' if n in voids else 'unref'))
            elif occ==len(re.findall(r'\(\s*void\s*\)\s*'+re.escape(n)+r'(?![A-Za-z0-9_])', body)):
                unused.append((n,'void-cast'))
        if unused:
            out.append({'path':path,'line':lineno,'fn':name,'params':re.sub(r'\s+',' ',params)[:200],'unused':unused})
print('defs with unreferenced params:', len(out))
c=collections.Counter()
for o in out:
    for n,k in o['unused']: c[k]+=1
print('totals:', dict(c))
json.dump(out, open('问题扫描/_cache/v20_params3.json','w'), indent=0)
byfile=collections.Counter(os.path.basename(o['path']) for o in out)
for k,v in byfile.most_common(40): print(' ',k,v)
