import json, os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
for key in ["verify_install_tree", "product.json", "install-tree", "install_tree"]:
    ids = [c["id"] for c in checks if key in json.dumps(c)]
    print("KEY", key, "->", ids)
print()
print("=== 引用 packaging/ 的门 ===")
for c in checks:
    j = json.dumps(c, ensure_ascii=False)
    if "packaging" in j:
        print("  ", c["id"], c["profiles"], [a for a in c["command"]][:6])
print()
print("=== 全仓引用 verify_install_tree.py 的位置 ===")
