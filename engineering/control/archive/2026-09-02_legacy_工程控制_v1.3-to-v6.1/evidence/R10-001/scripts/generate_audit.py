"""R10 Python 审计 CSV 生成器 (NON_PRODUCTION_TOOL_ONLY 扫描)"""
import csv
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
LIB = os.path.join(ROOT, "lib")
OUT = os.path.join(ROOT, "工程控制", "evidence", "R10-001", "reports", "python_audit.csv")

HEADER = [
    "path", "category", "production_reachable", "imports_dll", "has_main",
    "action", "new_path", "nonproduction_marker", "build_reference",
    "documentation_reference", "evidence",
]


def classify(path):
    parts = path.replace("\\", "/").split("/")
    if "tests" in parts or "test" in parts:
        return "test"
    if "tools" in parts:
        return "research_tool"
    return "research"


rows = []
for dirpath, _dirs, files in os.walk(LIB):
    for fn in sorted(files):
        if not fn.endswith(".py"):
            continue
        full = os.path.join(dirpath, fn)
        rel = os.path.relpath(full, ROOT).replace("\\", "/")
        try:
            text = open(full, "r", encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        marker = "NON_PRODUCTION_TOOL_ONLY" in text
        imports_dll = ("ctypes" in text) or re.search(r"\b(dll)\b", text, re.I) is not None
        has_main = "__main__" in text
        evidence = ""
        for line in text.splitlines():
            if "NON_PRODUCTION_TOOL_ONLY" in line:
                evidence = line.strip()[:120]
                break
        rows.append([
            rel,
            classify(rel),
            "NO" if marker else "YES",
            "YES" if imports_dll else "NO",
            "YES" if has_main else "NO",
            "KEEP_NONPRODUCTION",
            rel,
            "YES" if marker else "NO",
            "",
            "",
            evidence,
        ])

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(HEADER)
    w.writerows(rows)
print(f"wrote {len(rows)} rows -> {OUT}")
