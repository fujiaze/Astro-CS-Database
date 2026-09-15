import os, re, json, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
print("=== (1) ci/tests 文件与 CI-BINDING-TESTS 的 -p 模式 ===")
allf = sorted(f for f in os.listdir("ci/tests") if f.endswith(".py"))
import fnmatch
pat = [c for c in checks if c["id"]=="CI-BINDING-TESTS"][0]["command"]
p = pat[pat.index("-p")+1] if "-p" in pat else "*"
match = [f for f in allf if fnmatch.fnmatch(f, p)]
print("   模式:", p, "| ci/tests .py 总数:", len(allf), "| 命中:", len(match), match)
print("   未命中:", [f for f in allf if f not in match])
print("   其它 discover 到 ci/tests 的门:", [c["id"] for c in checks if "ci/tests" in json.dumps(c) and c["id"]!="CI-BINDING-TESTS"])
print()
print("=== (2) 有没有门检查 install() 站点唯一性 ===")
cands = [c["id"] for c in checks if re.search(r"install", json.dumps(c), re.I)]
print("   command/参数含 install 的门:", cands)
for f in ["tools/check_build_hygiene.py","tools/quality/check_install_rules.py"]:
    print("   ", f, os.path.exists(f))
o = subprocess.run(["git","--no-optional-locks","ls-files","tools/*"], capture_output=True, text=True).stdout.split()
print("   tools/ 里名字含 install/build/cmake 的脚本:", [x for x in o if re.search(r"(install|build|cmake)", x, re.I)])
print("   这些脚本是否被门引用:", [(x, [c["id"] for c in checks if x in json.dumps(c)]) for x in o if re.search(r"(install|build_graph|cmake)", x, re.I)])
print()
print("=== (3) impact_map: 构建面路径的规则覆盖 ===")
im = json.load(open("ci/impact_map.json", encoding="utf-8"))
rules = im["rules"]
print("   rules 数:", len(rules), "| fallback:", json.dumps(im.get("fallback"), ensure_ascii=False)[:200])
for probe in ["CMakePresets.json", "cmake/install_layout.cmake", "packaging/install-tree.contract.json", "build.sh", "include/astrocs/abi/module_api_v1.h", "lib/drizzle/src/module_exports.map"]:
    hit = []
    for r in rules:
        pats = r.get("paths") or r.get("patterns") or r.get("glob") or []
        if isinstance(pats, str): pats = [pats]
        for pp in pats:
            import fnmatch as fm
            if pp.endswith("/**"):
                if probe.startswith(pp[:-3]): hit.append((r.get("id") or r.get("check") or str(r)[:40], pp))
            elif fm.fnmatch(probe, pp): hit.append((r.get("id") or r.get("check") or str(r)[:40], pp))
    print("   %-46s -> %s" % (probe, hit[:3] or "无规则"))
print()
print("=== (4) make_linux_release / LNX 交付面门 ===")
print("   引用 tools/make_linux_release.py 的门:", [c["id"] for c in checks if "make_linux_release" in json.dumps(c)])
print("   lnx_v5_clean_rel 在 CI 面的生产者:", subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-ln","lnx_v5_clean_rel","--","ci",".github","CMakePresets.json","CMakeLists.txt","build.sh"], capture_output=True, text=True).stdout.split() or "0")