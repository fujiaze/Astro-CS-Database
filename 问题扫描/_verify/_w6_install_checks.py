import json, os, re, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
os.chdir(ROOT)
checks = json.load(open("ci/checks.json", encoding="utf-8"))["checks"]
print("=== 命令或参数含 install 的门 ===")
for c in checks:
    j = json.dumps(c, ensure_ascii=False)
    if re.search(r"install", j, re.I):
        print("  ", c["id"], c["profiles"], c["command"][:8])
print()
print("=== tests/abi 目录清单 ===")
for f in sorted(os.listdir("tests/abi")):
    print("   ", f, os.path.getsize(os.path.join("tests/abi", f)))
print()
print("=== UT-ABI 门命令 ===")
for c in checks:
    if "ABI" in c["id"]:
        print("  ", c["id"], c["profiles"], c["command"])