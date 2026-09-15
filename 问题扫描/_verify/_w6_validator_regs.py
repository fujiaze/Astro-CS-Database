import json, os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
import collections
entries = collections.defaultdict(list)
for c in checks:
    for a in c["command"]:
        if isinstance(a, str) and a.endswith(".py"):
            entries[a].append(c["id"])
print("checks.json 里被引用的 .py 入口脚本数:", len(entries))
import importlib
for name in ["ci/validate_registry.py", "ci/validate_workflow_binding.py", "packaging/verify_install_tree.py",
             "ci/verify_actions_lock.py", "cmake/toolchain/verify_toolchain.py", "ci/verify_candidate.py",
             "tools/quality/verify_candidate.py", "ci/check_version.py", "tools/check_abi_boundary.py",
             "tools/quality/check_standards_registry.py"]:
    print("  %-45s -> 注册门: %s | 文件存在: %s" % (name, entries.get(name, "无"), os.path.exists(name)))
print()
print("=== 全仓 validate_registry.py 的调用点（受跟踪文件） ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln","validate_registry"], capture_output=True, text=True)
print(out.stdout)
print("=== verify_candidate 路径 ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","ls-files","*verify_candidate*"], capture_output=True, text=True)
print(out.stdout)
print("=== mod001_install_load_check.py 的结构（是否直跑验收脚本） ===")
txt = open("tests/abi/mod001_install_load_check.py", encoding="utf-8", errors="replace").read()
for pat in ["def main", "sys.exit(main(", "class .*(TestCase)", "if __name__", "skipUnless", "skipIf"]:
    ms = re.findall(pat, txt)
    print("   %-24s 命中 %d" % (pat, len(ms)))
