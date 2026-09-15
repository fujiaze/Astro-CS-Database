import os, re, subprocess, json
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
print("=== 1) astrocs_provider_query_v1 的仓内出现点 ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n","astrocs_provider_query_v1"], capture_output=True, text=True)
for ln in out.stdout.splitlines():
    if ln.startswith("问题扫描/") or ln.startswith("evidence/") or ln.startswith("engineering/") or ln.startswith("设计大纲/") or ln.startswith("reports/"): continue
    print("   ", ln[:150])
print()
print("=== 2) lib/backend_host/baseline_backend.cpp 的导出符号定义（顶层 extern C 名） ===")
t = open("lib/backend_host/baseline_backend.cpp", encoding="utf-8", errors="replace").read()
names = re.findall(r"^(?:extern\s+\"c\"\s+)?[A-Za-z_][\w \*&:]*?\b(astrocs_[a-z0-9_]+|acs_[a-z0-9_]+)\s*\(", t, re.M)
print("   baseline_backend.cpp 顶层符号:", sorted(set(names)))
t2 = open("providers/cpu/baseline/src/baseline_provider.cpp", encoding="utf-8", errors="replace").read()
names2 = re.findall(r"\b(astrocs_[a-z0-9_]+)\s*\(", t2, re.M)
print("   providers/cpu/baseline/src/baseline_provider.cpp 符号:", sorted(set(names2)))
print()
print("=== 3) providers/ 是否出现在任何 CMakeLists（构建图） ===")
for f in sorted(subprocess.run(["git","--no-optional-locks","ls-files","*CMakeLists.txt","*.cmake"], capture_output=True, text=True).stdout.split()):
    txt = open(f, encoding="utf-8", errors="replace").read()
    if "providers/" in txt:
        print("   ", f, [l.strip()[:100] for l in txt.splitlines() if "providers/" in l][:4])
print()
print("=== 4) tests/cpu 脚本编译的源路径 ===")
for f in ["tests/cpu/baseline/run_provider_oracle_checks.py","tests/cpu/avx2/run_provider_avx2_checks.py","tests/cpu/avx512/run_provider_avx512_checks.py","tests/cpu/dispatch/run_cpu_capability_checks.py"]:
    txt = open(f, encoding="utf-8", errors="replace").read()
    srcs = sorted(set(re.findall(r"providers/[A-Za-z0-9_/.-]+|lib/backend_host/[A-Za-z0-9_/.-]+", txt)))
    print("   ", f, "->", srcs[:8])