import glob, re, os, subprocess, datetime
REPO='/workspace/Astro CS Database'
DD=os.path.join(REPO,'设计大纲')
SIN=os.path.join(DD,'_evidence/commits/逐条详析')
RPT=os.path.join(DD,'reports/history/slices')
os.makedirs(SIN,exist_ok=True)
BASE='a3a343a4'
MOVE=re.compile('主责条目|主责区|正文条目|重叠复核|逐条详析|逐条')
KEEP=re.compile('主题归纳|重叠区核对|遗留|缺口|开篇声明|衔接|实际关系|拓扑序|覆盖|声明|边界信号|日期')
def old_text(n):
    r=subprocess.run(['git','show','a3a343a4:设计大纲/reports/history/slices/'+n],cwd=REPO,capture_output=True,text=True,errors='replace')
    return r.stdout if r.returncode==0 else ''
def split(t):
    lines=t.split(chr(10))
    idx=[i for i,l in enumerate(lines) if l.startswith('## ')]
    if not idx: return lines, [], []
    pre=lines[:idx[0]]; body=[]; keep=[]
    for k,s in enumerate(idx):
        e=idx[k+1] if k+1<len(idx) else len(lines)
        h=lines[s]; blk=lines[s:e]
        n=sum(1 for l in blk if l.startswith('### '))
        moved = (MOVE.search(h) and not KEEP.search(h)) or (n>=10 and not KEEP.search(h))
        if moved: body+=blk
        else: keep.append(blk)
    return pre, body, keep
tot_e=0; tot_r=0; done=0
for p in sorted(glob.glob(os.path.join(RPT,'H-S*.md'))):
    n=os.path.basename(p)
    sid=n[2:-6]
    cur=open(p,encoding='utf-8').read()
    old=old_text(n)
    src = cur if len(re.findall('(?m)^### ',cur))>5 and BASE not in cur else (old or cur)
    if len(re.findall('(?m)^### ',src))<=5: src=old or src
    pre, body, keep = split(src)
    if not body:
        print('跳过（无可下沉段）', n); continue
    g=len([1 for l in body if l.startswith('### ')])
    hdr=['# 逐条详析 '+sid+'（证据层过程件）','',
         '> 本文件是 设计大纲/reports/history/slices/'+n+' 的逐条正文；最终交付只保留两份大报告，逐条正文下沉到证据层（条目文本零改动，仅移动）。',
         '> 由 _tools/11_split_slice_reports.py 从入库基线 '+BASE+' 重切，日期 '+datetime.date.today().isoformat()+'；条目组 '+str(g)+' 个；报告树保留段（主题归纳/重叠区核对/遗留缺口）见原文件。','']
    open(os.path.join(SIN,n),'w',encoding='utf-8').write(chr(10).join(hdr+body).rstrip()+chr(10))
    note=['','---','','> 逐条正文（%d 个条目组）已下沉至 设计大纲/_evidence/commits/逐条详析/%s（过程件，非最终交付内容）。本文件仅保留主题归纳、重叠区核对与遗留缺口。' % (g,sid),'']
    body_keep=[l for blk in keep for l in blk]
    open(p,'w',encoding='utf-8').write((chr(10).join(pre+note)+chr(10)+chr(10).join(body_keep)).rstrip()+chr(10))
    tot_e+=g; tot_r+=len([1 for l in body_keep if l.startswith('### ')]); done+=1
print('重切 %d 份；下沉条目组 %d；报告树保留三级标题 %d' % (done,tot_e,tot_r))
