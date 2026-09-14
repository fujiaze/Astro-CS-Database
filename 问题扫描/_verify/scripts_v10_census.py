#!/usr/bin/env python3
# V10 只读普查脚本：外部输入解析函数站点清点（不写任何生产文件，不改任何东西）
# 口径：git ls-files 受跟踪源文件（.c/.cpp/.cc/.h/.hpp），排除 third_party/、build/、run/、out/、Testing_archive/
import subprocess, re, collections, os, sys

files = subprocess.run(["git","--no-optional-locks","ls-files",
                        "*.c","*.cpp","*.cc","*.h","*.hpp"],
                       capture_output=True, text=True).stdout.splitlines()

EXCL = ("third_party/", "build/", "run/", "out/", "Testing_archive/", "astrocs_p1sess_")
files = [f for f in files if not f.startswith(EXCL)]

PAT = re.compile(r"\b(ato[fild]|strto[dfil]+|scanf|fscanf|sscanf|vsscanf|stod|stof|stoi|stoll|stoul|stoull|unserialize|strto[u]l)\b")

rows = []
for f in files:
    try:
        with open(f, "r", encoding="utf-8", errors="replace") as fh:
            for i, line in enumerate(fh, 1):
                m = PAT.search(line)
                if m:
                    fn = m.group(1)
                    code = line.split("//", 1)[0]
                    if not PAT.search(code):
                        continue  # 纯注释命中，单列计数
                    rows.append((f, i, fn, line.rstrip()[:160]))
    except Exception as e:
        print("READFAIL", f, e, file=sys.stderr)

by_fn = collections.Counter(r[2] for r in rows)
by_dir = collections.Counter("/".join(r[0].split("/")[:2]) for r in rows)
print("== 口径：受跟踪 C/C++ 源文件（排除 %s），代码行（剔除纯注释行）==" % ",".join(EXCL))
print("文件总数(git ls-files 口径):", len(files))
print("命中站点总数:", len(rows))
print("-- 按函数 --")
for k, v in by_fn.most_common(): print(" ", k, v)
print("-- 按目录（前 2 段）--")
for k, v in by_dir.most_common(): print(" ", k, v)
print("-- 按文件 top30 --")
for k, v in collections.Counter(r[0] for r in rows).most_common(30): print(" %4d %s" % (v, k))
