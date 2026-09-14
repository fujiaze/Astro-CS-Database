#!/usr/bin/env python3
# V10 只读普查脚本 v2（订正口径）：
#  - 排除任何路径段含 third_party（vendored，SUMMARY「未覆盖面」已声明不展开）
#  - 排除 build/ run/ out/ Testing_archive/ astrocs_p1sess_*（影子树）
#  - git -c core.quotepath=off 取路径（v1 因转义引号 READFAIL，已修）
#  - 分组：生产面 = lib/ cli/ runtime/ modules/ providers/ include/
import subprocess, re, collections, sys

def gitls():
    out = subprocess.run(["git","-c","core.quotepath=off","--no-optional-locks","ls-files",
                          "*.c","*.cpp","*.cc","*.h","*.hpp"],
                         capture_output=True, text=True).stdout
    return out.splitlines()

BAD_SEG = {"third_party","build","run","out","Testing_archive"}
def excluded(f):
    segs = f.split("/")
    if any(s in BAD_SEG for s in segs): return True
    if segs[0].startswith("astrocs_p1sess_"): return True
    if segs[0] == "engineering": return True
    return False

files = [f for f in gitls() if not excluded(f)]

PAT = re.compile(r"(?<![\w.])(atof|atol|atoi|strtof|strtod|strtold|strtol|strtoll|strtoul|strtoull|scanf|fscanf|sscanf|vsscanf|std::stod|std::stof|std::stoi|std::stoll|std::stoul|std::stoull|\bstod\b|\bstof\b|\bstoi\b|\bstoll\b|\bstoul\b|\bstoull\b)(?![\w.])")
CMT = re.compile(r"^\s*(//|/\*|\*)")

PROD_PREFIX = ("lib/","cli/","runtime/","modules/","providers/","include/")

rows=[]; cmt_hits=0
for f in files:
    try:
        with open(f,"r",encoding="utf-8",errors="replace") as fh:
            for i,line in enumerate(fh,1):
                if not PAT.search(line): continue
                if CMT.match(line):
                    cmt_hits+=1; continue
                code = PAT.search(line.split("//",1)[0])
                if not code:
                    cmt_hits+=1; continue
                rows.append((f,i,code.group(1).replace("std::",""),line.rstrip()))
    except Exception as e:
        print("READFAIL",f,e,file=sys.stderr)

prod=[r for r in rows if r[0].startswith(PROD_PREFIX)]
test=[r for r in rows if not r[0].startswith(PROD_PREFIX)]
print("== 口径 v2：受跟踪 .c/.cpp/.cc/.h/.hpp，排除 third_party 段与影子树，剔除纯注释行 ==")
print("候选文件数:",len(files),"  命中站点:",len(rows),"  (纯注释命中已剔:",cmt_hits,")")
print("生产面(lib/cli/runtime/modules/providers/include):",len(prod),"  测试/工具面:",len(test))
print("-- 生产面按函数 --")
for k,v in collections.Counter(r[2] for r in prod).most_common(): print("  ",k,v)
print("-- 生产面按文件 --")
for k,v in collections.Counter(r[0] for r in prod).most_common(40): print("  %3d %s"%(v,k))
