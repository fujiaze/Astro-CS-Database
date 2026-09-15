import glob, re, os
REPO='/workspace/Astro CS Database'
DD=os.path.join(REPO,'设计大纲')
BT=chr(96)
PRE=['lib','cli','include','tests','ci','tools','docs','engineering','evidence','artifacts','reports','contracts','runtime','providers','modules','packaging','工程控制','问题扫描','设计大纲']
PAT=re.compile('(?:'+'|'.join(PRE)+'/)+[A-Za-z0-9_./一-鿿-]{2,120}')
missing={}; checked=0; files=0
for p in sorted(glob.glob(os.path.join(DD,'reports/**/*.md'), recursive=True)):
    files+=1
    t=open(p,encoding='utf-8').read()
    for m in PAT.finditer(t):
        a=m.start()
        if a>0 and (t[a-1].isalnum() or t[a-1] in '_/-'): continue
        c=m.group(0).rstrip('.,;:、）)')
        if '*' in c or '<' in c or ' ' in c or c.endswith('/'): continue
        checked+=1
        if not (os.path.exists(os.path.join(REPO,c)) or os.path.exists(os.path.join(DD,c)) or os.path.exists(os.path.join(DD,'_evidence',c.split('/',1)[1] if c.startswith('设计大纲/') else c))):
            missing.setdefault(os.path.basename(p),set()).add(c)
print('多段路径核对 %d 次；树中找不到 %d 种' % (checked, sum(len(v) for v in missing.values())))
for k in sorted(missing):
    print('  %-34s %s' % (k, '; '.join(sorted(missing[k]))[:150]))
