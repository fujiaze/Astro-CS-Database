# -*- coding: utf-8 -*-
"""AUD-404 复算 6：eng/ 面翻译单元的"是否被任何 CMake 源清单命名"独立普查。

AUD-401 机器表只覆盖 lib/**（621 TU），eng/tests 与 eng/tools 的 TU 未在表内。
本脚本纯读 CMake 源文本（不配置、不编译）：凡文件名出现在任一跟踪
CMakeLists.txt / *.cmake 中即视为"被命名"。
"""
import collections
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"


def ls(*args):
    return [x for x in subprocess.run(
        ["git", "-C", REPO, "-c", "core.quotepath=false", "ls-files"] + list(args),
        capture_output=True, text=True, encoding="utf-8",
        errors="replace").stdout.split("\n") if x]


tu = [p for p in ls("eng") if p.endswith((".cpp", ".c", ".cc", ".cxx"))]
cmake = [p for p in ls() if p.endswith(("CMakeLists.txt", ".cmake"))]
blob = "\n".join(open(REPO + "/" + c, encoding="utf-8", errors="replace").read()
                 for c in cmake)

named, unnamed = [], []
for p in tu:
    base = p.rsplit("/", 1)[1]
    # 文件名或去扩展名后出现在任一 CMake 文本中即算命名
    if base in blob or base.rsplit(".", 1)[0] in blob:
        named.append(p)
    else:
        unnamed.append(p)

print("eng/ 面跟踪 TU:", len(tu))
print("  被某 CMake 文本命名:", len(named))
print("  未被任何 CMake 文本命名:", len(unnamed))
print("扫描的 CMake 文本数:", len(cmake))
d = collections.Counter("/".join(p.split("/")[:3]) for p in unnamed)
print("\n未命名 TU 按目录:")
for k, v in d.most_common(20):
    print(f"  {v:4d}  {k}")
print("\n样例 25 条:")
for p in unnamed[:25]:
    print("  ", p)
open(r"独立审计/复算件/retire\eng_unnamed.txt",
     "w", encoding="utf-8").write("\n".join(unnamed) + "\n")
