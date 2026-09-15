import glob, re, os, subprocess
REPO='/workspace/Astro CS Database'
DD=os.path.join(REPO,'设计大纲')
out=subprocess.run(['git','-c','core.quotePath=false','log','--all','--format=','--name-only','-M'],cwd=REPO,capture_output=True,text=True,errors='replace').stdout
hist=set(x.strip() for x in out.splitlines() if x.strip())
disk=set()
for root,dirs,files in os.walk(DD):
    for f in files: disk.add(os.path.relpath(os.path.join(root,f),REPO))
for root,dirs,files in os.walk(REPO):
    if any(x in root for x in ('/.git','/run','/build','/out','GaiaDR3','BASS DR3','/node_modules')): continue
    for f in files: disk.add(os.path.relpath(os.path.join(root,f),REPO))
PRE=['lib','cli','include','tests','ci','tools','docs','engineering','evidence','artifacts','reports','contracts','runtime','providers','modules','packaging','工程控制','问题扫描','设计大纲']
PAT=re.compile('(?:'+'|'.join(PRE)+'/[A-Za-z0-9_./一-鿿-]{1,120})+')
susp={}; checked=0
for p in sorted(glob.glob(os.path.join(DD,'reports/**/*.md'), recursive=True)):
    t=open(p,encoding='utf-8').read()
    for m in PAT.finditer(t):
        a=m.start()
        if a>0 and (t[a-1].isalnum() or t[a-1] in '_/-'): continue
        c=m.group(0).rstrip('.,;:、）) ')
        if '*' in c or '<' in c or ' ' in c or c.endswith('/') or '/' not in c[4:]: continue
        checked+=1
        if c not in hist and c not in disk:
            susp.setdefault(os.path.basename(p),set()).add(c)
print('历史+磁盘路径全集 %d；多段路径引用核对 %d 次；既不在磁盘也不在任何历史提交中 %d 种' % (len(hist|disk), checked, sum(len(v) for v in susp.values())))
for k in sorted(susp):
    print('  %-30s %s' % (k, '; '.join(sorted(susp[k]))[:170]))
