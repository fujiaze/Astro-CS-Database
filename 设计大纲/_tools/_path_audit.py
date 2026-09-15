import glob, re, os
REPO='/workspace/Astro CS Database'
DD=os.path.join(REPO,'设计大纲')
BT=chr(96)
PRE=['lib','cli','include','tests','ci','tools','docs','engineering','evidence','artifacts','reports','contracts','runtime','providers','modules','packaging','工程控制','问题扫描','设计大纲']
PAT=re.compile('(?:'+'|'.join(PRE)+'/[A-Za-z0-9_./一-鿿-]{2,140})')
def toks(t):
    out=set()
    segs=t.split(BT)
    for i in range(1,len(segs),2): out.add(segs[i].strip())
    for m in PAT.finditer(t):
        a=m.start()
        if a>0 and (t[a-1].isalnum() or t[a-1] in '_/-'): continue
        out.add(m.group(0))
    return out
skip=('git ','python3 ','bash ','unzip ','find ','grep ','ls ','cd ','sha256sum ','echo ','head ','sed ','wc ','sort ','tr ','cat ','-')
missing={}; checked=0; files=0
for p in sorted(glob.glob(os.path.join(DD,'reports/**/*.md'), recursive=True)):
    files+=1
    t=open(p,encoding='utf-8').read()
    for c in toks(t):
        if any(c.startswith(s) for s in skip): continue
        if ' ' in c or '*' in c or '<' in c or '…' in c or c.endswith('/') or len(c)<5: continue
        c2=c.rstrip('.,;:')
        checked+=1
        if not os.path.exists(os.path.join(REPO,c2)) and not os.path.exists(os.path.join(DD,c2)):
            missing.setdefault(os.path.basename(p),[]).append(c2)
print('报告 %d 份；路径引用核对 %d 次；树中找不到 %d 种' % (files,checked,sum(len(v) for v in missing.values())))
for k in sorted(missing):
    print('  %-46s %s' % (k, '; '.join(missing[k][:3])[:140]))
