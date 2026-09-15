
import os, re, json, subprocess, collections
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]

print("=== A) 关键构建面文件的 changed_paths 覆盖门数 ===")
keys = ["CMakePresets.json", "cmake/install_layout.cmake", "packaging/**", "include/**", "lib/**/*.def", ".def", ".map", "tests/unit/CMakeLists.txt", "CMakeLists.txt"]
probe = ["CMakePresets.json", "cmake/install_layout.cmake", "packaging/install-tree.contract.json", "include/astrocs/abi/module_api_v1.h", "lib/drizzle/src/module_exports.map", "lib/drizzle/src/astrocs_p1_drizzle.def"]
def glob_hit(pat, path):
    import fnmatch
    if pat.endswith("/**"):
        return path.startswith(pat[:-3] + "/") or path == pat[:-3]
    return fnmatch.fnmatch(path, pat) or (("**" in pat) and fnmatch.fnmatch(path, pat.replace("/**", "/*")))
for p in probe:
    hits = []
    for c in checks:
        for cp in (c.get("changed_paths") or []):
            if glob_hit(cp, p) or cp == p:
                hits.append(c["id"]); break
    print("   %-52s -> %d 门: %s" % (p, len(hits), hits[:8]))

print()
print("=== B) cmake/toolchain/verify_toolchain.py 是否被任何门引用 ===")
n = [c["id"] for c in checks if any("cmake/toolchain" in str(a) for a in c["command"])]
print("   引用门:", n)
print("   changed_paths 含 CMakePresets.json 的门:", [c["id"] for c in checks if any("CMakePresets" in p for p in (c.get("changed_paths") or []))])
print("   changed_paths 含 packaging 的门:", [c["id"] for c in checks if any("packaging" in p for p in (c.get("changed_paths") or []))])

print()
print("=== C) product.json units vs install-tree.contract units 差集 ===")
con = json.load(open("packaging/install-tree.contract.json", encoding="utf-8"))
prod = json.load(open("packaging/astrocs.product.json", encoding="utf-8"))
cu = {u["unit_id"]: u for u in con["units"]}
pu = {u["unit_id"]: u for u in prod["units"]}
print("   contract units:", len(cu), "product units:", len(pu))
print("   contract-only:", sorted(set(cu) - set(pu)))
print("   product-only:", sorted(set(pu) - set(cu)))
mism = []
for k in set(cu) & set(pu):
    ip = cu[k]["install_path"]; rp = pu[k]["rel_path"]
    if ip != rp: mism.append((k, ip, rp))
print("   路径不一致 unit:", mism)
print("   contract target_version:", con["target_version"], "| product product_version:", prod["product_version"], "| 根 VERSION:", open("VERSION").read().strip())
print("   product source_commit:", prod["source_commit"][:12], "| HEAD:", subprocess.run(["git","--no-optional-locks","rev-parse","HEAD"],capture_output=True,text=True).stdout.strip()[:12])

print()
print("=== D) astrocs_p1_noise target 是否存在于任何受跟踪 CMake 源 ===")
out = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n","add_library(astrocs_p1_noise","--","*CMakeLists.txt"], capture_output=True, text=True)
print("   add_library(astrocs_p1_noise 命中:", out.stdout.strip() or "0")
out2 = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n","p1_noise_adapter","--","ci","tests"], capture_output=True, text=True)
print("   p1_noise_adapter 引用点:", out2.stdout.strip()[:600] or "0")

print()
print("=== E) echo 模块的构建面 ===")
for pat in ["add_library(astrocs_echo", "add_subdirectory(", "echo_module.c"]:
    o = subprocess.run(["git","--no-optional-locks","-c","core.quotepath=false","grep","-n",pat], capture_output=True, text=True)
    ls = [l for l in o.stdout.splitlines() if not l.startswith(("问题扫描/","evidence/","engineering/","设计大纲/","reports/"))]
    print("   ##", pat, "->", len(ls))
    for l in ls[:8]: print("      ", l[:130])
