import subprocess, re, os, glob
DD='/workspace/Astro CS Database/设计大纲'
names=[l.strip() for l in subprocess.run(['git','-c','core.quotePath=false','show','a3a343a4:设计大纲/reports/history/slices'],capture_output=True,text=True).stdout.splitlines() if l.strip().endswith('.md')]
diffs=[]
for n in sorted(names):
    old=subprocess.run(['git','show','a3a343a4:设计大纲/reports/history/slices/'+n],capture_output=True,text=True,errors='replace').stdout
    sid=n.replace('H-','').replace('.md','')
    ev=os.path.join(DD,'_evidence/commits/逐条详析/'+n)
    rp=os.path.join(DD,'reports/history/slices/'+n)
    co=len(re.findall('(?m)^### ',old))
    ce=len(re.findall('(?m)^### ',open(ev,encoding='utf-8').read())) if os.path.exists(ev) else 0
    cr=len(re.findall('(?m)^### ',open(rp,encoding='utf-8').read()))
    if co!=ce+cr: diffs.append((n,co,ce,cr,co-ce-cr))
print('不一致片数 %d' % len(diffs))
for d in diffs[:12]: print('  %s 旧=%d 证据=%d 报告=%d 差=%d' % d)
if diffs:
    n=diffs[0][0]
    old=subprocess.run(['git','show','a3a343a4:设计大纲/reports/history/slices/'+n],capture_output=True,text=True,errors='replace').stdout
    sid=n[:-3]
    ev=open(os.path.join(DD,'_evidence/commits/逐条详析/'+n),encoding='utf-8').read()
    rp=open(os.path.join(DD,'reports/history/slices/'+n),encoding='utf-8').read()
    ho=[l for l in old.split(chr(10)) if l.startswith('### ')]
    hn=[l for l in (ev+chr(10)+rp).split(chr(10)) if l.startswith('### ')]
    from collections import Counter
    a,b=Counter(ho),Counter(hn)
    lost=[k for k in a if a[k]>b.get(k,0)]
    print('  %s 丢失/未搬走的三级标题样本: %s' % (n, [l[:70] for l in lost][:6]))
