# -*- coding: utf-8 -*-
"""统计「测试 TU 受跟踪但不在任何受跟踪 CMakeLists 非注释行里」的当前数量（ROW 74 取证）。"""
import os, re, subprocess, sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"

def git(*a):
    return subprocess.run(["git", "-C", REPO] + list(a), capture_output=True, text=True, encoding="utf-8").stdout

files = [p.replace("\\", "/") for p in git("ls-files").splitlines()]
cmake = [f for f in files if f.endswith("CMakeLists.txt") or f.endswith(".cmake")]
blob = []
for c in cmake:
    with open(os.path.join(REPO, c), encoding="utf-8", errors="replace") as fh:
        for ln in fh:
            s = ln.strip()
            if s.startswith("#"):
                continue
            blob.append(s)
hay = "\n".join(blob)

pat = re.compile(r"(_test|_oracle|_gate|_check)\.(cpp|c)$|test_.*\.(cpp|c)$")
tu = [f for f in files if (f.startswith("lib/") or f.startswith("eng/tests/"))
      and f.endswith((".cpp", ".c"))
      and pat.search(os.path.basename(f))]
unbuilt = [f for f in tu if os.path.basename(f).split(".")[0] not in hay]
print("受跟踪 CMake 文件数:", len(cmake))
print("测试类 TU（lib/ + eng/tests/，按文件名启发式）:", len(tu))
print("其中不在任何 CMake 非注释行的（tracked-but-unbuilt）:", len(unbuilt))
for f in sorted(unbuilt)[:40]:
    print("   ", f)
