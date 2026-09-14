import re,subprocess,collections
TR=[t for t in subprocess.run(["git","ls-files"],capture_output=True,text=True).stdout.split("\n") if t]
RUN=re.compile(r"(?<![\w/])run/[A-Za-z0-9_./\-]+")
AUTH=re.compile(r"证据|依据|推导|实测|权威|锚|oracle|ORACLE|REPORT|report|manifest|MANIFEST|EVID|期望值|基准|baseline|见\s*`|参见|对照")
OUTP=re.compile(r"--outdir|-o\s|落\s*run|写入|输出|产出|dirty_ignore|gitignore|归\s*run|artifact|samples\.csv|summary\.json")
dan=[];outp=[];other=[]
for t in TR:
    if t.startswith(("evidence/","reports/","artifacts/","问题扫描/","docs/archive/")): continue
    try: lines=open(t,encoding="utf-8",errors="ignore").read().split("\n")
    except Exception: continue
    for i,ln in enumerate(lines,1):
        if not RUN.search(ln): continue
        if OUTP.search(ln): outp.append((t,i)); continue
        if AUTH.search(ln): dan.append((t,i,ln.strip()[:110]))
        else: other.append((t,i))
print("分类结果(排除 evidence/reports/artifacts/问题扫描/docs/archive):")
print("  A 危险(以 run/** 为证据/权威/期望值锚) =",len(dan))
print("  B 合法(声明输出/落位/ignore) =",len(outp))
print("  C 中性待人工 =",len(other))
c=collections.Counter(x[0] for x in dan)
print("  A 类按文件 top:")
for k,v in c.most_common(14): print("    %3d %s"%(v,k))
print("  A 类样本(前 12 条, 带行号与原文):")
for t,i,s in dan[:12]: print("    - %s:%d  %s"%(t,i,s))
