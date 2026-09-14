#!/usr/bin/env python3
# V10 只读：导出生产面全部解析站点（含函数名+该行），按文件分组，供人工判定外部输入归属
import subprocess, re, collections
BAD_SEG={"third_party","build","run","out","Testing_archive"}
def excluded(f):
    segs=f.split("/")
    return any(s in BAD_SEG for s in segs) or segs[0].startswith("astrocs_p1sess_") or segs[0]=="engineering"
files=[f for f in subprocess.run(["git","-c","core.quotepath=off","--no-optional-locks","ls-files","*.c","*.cpp","*.cc","*.h","*.hpp"],capture_output=True,text=True).stdout.splitlines() if not excluded(f)]
PAT=re.compile(r"(?<![\w.])(atof|atol|atoi|strtof|strtod|strtold|strtol|strtoll|strtoul|strtoull|scanf|fscanf|sscanf|vsscanf|stod|stof|stoi|stoll|stoul|stoull)(?![\w.])")
CMT=re.compile(r"^\s*(//|/\*|\*)")
PROD=("lib/","cli/","runtime/","modules/","providers/","include/")
out=collections.defaultdict(list)
for f in files:
    if not f.startswith(PROD): continue
    if "/tests/" in f or f.split("/")[-1].startswith("test_") or "_test" in f or "/example" in f or "/tools/" in f or "/bench" in f: 
        tag="TESTTOOL"
    else: tag="PROD"
    try: lines=open(f,encoding="utf-8",errors="replace").read().splitlines()
    except Exception: continue
    for i,line in enumerate(lines,1):
        if CMT.match(line): continue
        code=line.split("//",1)[0]
        m=PAT.search(code)
        if not m: continue
        out[(tag,f)].append((i,m.group(1),line.strip()[:150]))
for (tag,f),v in sorted(out.items(), key=lambda x:(-len(x[1]),x[0][1])):
    if tag!="PROD": continue
    print("### %s (%d)"%(f,len(v)))
    for i,fn,txt in v: print("  :%d [%s] %s"%(i,fn,txt))
