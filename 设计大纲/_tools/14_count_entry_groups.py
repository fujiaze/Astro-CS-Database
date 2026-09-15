import glob, re, os
DD='/workspace/Astro CS Database/设计大纲'
ev=sorted(glob.glob(os.path.join(DD,'_evidence/commits/逐条详析/H-S*.md')))
rp=sorted(glob.glob(os.path.join(DD,'reports/history/slices/H-S*.md')))
def groups(t): return len(re.findall('(?m)^### ', t))
def commit_groups(t): return len(re.findall('(?m)^### (?:seq |G\\d)', t))
tot_ev=sum(groups(open(p,encoding='utf-8').read()) for p in ev)
tot_ev_c=sum(commit_groups(open(p,encoding='utf-8').read()) for p in ev)
hdr=[]
for p in ev:
    t=open(p,encoding='utf-8').read()
    m=re.search('条目组 (\\d+) 个', t)
    hdr.append(int(m.group(1)) if m else -1)
print('证据层文件 %d；### 标题总数 %d；其中逐条提交组 %d；篇首标注合计 %d；缺标注文件 %d' % (len(ev), tot_ev, tot_ev_c, sum(x for x in hdr if x>0), hdr.count(-1)))
print('逐片差值(篇首-实际) 非零的:', [(os.path.basename(p).replace('.md',''), h, groups(open(p,encoding='utf-8').read())) for p,h in zip(ev,hdr) if h!=groups(open(p,encoding='utf-8').read())][:8])
tot_rp_c=sum(commit_groups(open(p,encoding='utf-8').read()) for p in rp)
tot_rp=sum(groups(open(p,encoding='utf-8').read()) for p in rp)
print('报告树 %d 份；### 标题总数 %d；其中逐条提交组 %d（应 0）' % (len(rp), tot_rp, tot_rp_c))
print('=> 逐条提交组权威数 = 证据层 %d + 报告树 %d = %d' % (tot_ev_c, tot_rp_c, tot_ev_c+tot_rp_c))
import csv
n=0
for mf in glob.glob(os.path.join(DD,'_evidence/commits/slices/S*/manifest.csv')):
    n+=sum(1 for r in csv.DictReader(open(mf,encoding='utf-8')) if r['角色']=='主责')
print('manifest 主责提交总数 %d（每个条目组含 1..n 条，故条目组数 < 提交数属正常）' % n)
