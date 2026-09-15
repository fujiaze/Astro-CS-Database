import json, os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
print("=== changed_paths 覆盖 packaging/ 或 cmake/ 的门 ===")
for c in checks:
    cp = c.get("changed_paths") or []
    if any(p.startswith("packaging") or p.startswith("cmake") or "install" in p for p in cp):
        print("  ", c["id"], [p for p in cp if p.startswith("packaging") or p.startswith("cmake") or "install" in p])
print()
print("=== impact_map.json 是否含 packaging/cmake ===")
im = json.load(open("ci/impact_map.json", encoding="utf-8"))
print("keys:", list(im.keys())[:8])
s = json.dumps(im, ensure_ascii=False)
print("impact_map 含 packaging:", s.count("packaging"), "| 含 cmake/:", s.count("cmake/"), "| install_layout:", s.count("install_layout"))
print()
print("=== tests/abi 各文件的 TestCase / main 结构 ===")
for f in sorted(os.listdir("tests/abi")):
    if not f.endswith(".py"): continue
    txt = open(os.path.join("tests/abi", f), encoding="utf-8", errors="replace").read()
    n_class = len(re.findall(r"class\s+\w+\s*\(\s*(unittest\.)?TestCase", txt))
    n_def = len(re.findall(r"^\s*def test_", txt, re.M))
    has_main = bool(re.search(r"if __name__", txt))
    print(f"   {f}: TestCase子类={n_class} test_= {n_def} __main__={has_main}")