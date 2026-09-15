import os, re, json, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
print("=== (a) cli/CMakeLists.txt 的 add_test ===")
txt = open("cli/CMakeLists.txt", encoding="utf-8", errors="replace").read()
print("   ", re.findall(r"add_test\s*\([^)]*", txt, re.S))
base = json.load(open("ci/ctest_baseline.json", encoding="utf-8"))
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
globs = []
for c in checks:
    for t in (c.get("ctest_targets") or []): globs.append(t)
import fnmatch
for name in ["cli_protocol_check","browser_backend","module:astrocs.conformance.echo","p1_noise_adapter"]:
    inb = name in base["targets"]
    ing = [g for g in globs if fnmatch.fnmatch(name, g)]
    print("   %-32s baseline=%-5s 门 glob 命中=%s" % (name, inb, ing))
print()
print("=== (b) run.py 是否调用 validate_registry ===")
rt = open("ci/run.py", encoding="utf-8", errors="replace").read()
print("   run.py 含 validate_registry:", "validate_registry" in rt)
for f in ["ci/validate_workflow_binding.py"]:
    print("   ", f, "含 validate_registry:", "validate_registry" in open(f, encoding="utf-8", errors="replace").read())
print("   checks.json 中 command 含 validate_registry 的门:", [c["id"] for c in checks if any("validate_registry" in str(a) for a in c["command"])])
print("   ci/tests 里调用 validate_registry 的文件:", [f for f in os.listdir("ci/tests") if f.endswith(".py") and "validate_registry" in open(os.path.join("ci/tests",f),encoding="utf-8",errors="replace").read()])
print("   CI-BINDING-TESTS 门命令:", [c["command"] for c in checks if c["id"]=="CI-BINDING-TESTS"])
print()
print("=== (c) taskset/tar 使用点 ===")
for key in ["taskset", "\"tar\"", "objdump"]:
    o = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln",key,"--","*.py","*.sh","CMakeLists.txt","*.cmake"], capture_output=True, text=True).stdout.splitlines()
    o = [x for x in o if not x.startswith(("问题扫描/","evidence/","engineering/","设计大纲/","reports/","run/"))]
    print("   %-10s ->" % key, o)
print()
print("=== (d) build.sh 是否在门/文档中被当入口 ===")
o = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln","build.sh"], capture_output=True, text=True).stdout.splitlines()
o = [x for x in o if not x.startswith(("问题扫描/","evidence/","engineering/","设计大纲/","reports/","run/","docs/archive/"))]
print("   引用 build.sh 的文件数:", len(o))
for x in o[:20]: print("      ", x)