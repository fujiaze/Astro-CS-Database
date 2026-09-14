# -*- coding: utf-8 -*-
"""V15 只读普查器 A：四类弱断言 + 注入式反向锁的机械命中统计（仅读取，不改仓库真源）。"""
import os, re, json

ROOT = "/workspace/Astro CS Database"
OUT = os.path.join(ROOT, "问题扫描/_verify/scripts_v15_cat.json")

SKIP_DIRS = {"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports",
             "工程控制","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描",
             "third_party","testdata","CS","Database","graph",".git","astrocs_p1sess_neg",
             "astrocs_p1sess_perf","astrocs_p1sess_props","astrocs_p1sess_test"}

cat = {"src_text_guard": [], "diagtext_pin": [], "positive_exist": [], "inject_reverse_lock": [], "selfwitness_cand": []}

FILE_READ_PY = re.compile(r'read_text\(|\.read\(\)|open\(')
NPOS = re.compile(r'find\(\s*"([^"]*)"\s*\)\s*(!=|==)\s*std::string::npos')

scanned = 0
for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
    for fn in filenames:
        p = os.path.join(dirpath, fn)
        r = "./" + os.path.relpath(p, ROOT).replace(os.sep, "/")
        ext = os.path.splitext(fn)[1]
        if ext not in (".py", ".cpp", ".c", ".h", ".hpp"):
            continue
        if not (r.startswith("./tests/") or r.startswith("./tools/") or r.startswith("./ci/")):
            if not (r.startswith("./lib/") and (("/tests/" in r) or "_test." in r or "selftest" in r)):
                continue
        scanned += 1
        try:
            txt = open(p, encoding="utf-8", errors="replace").read()
        except Exception:
            continue
        lines = txt.split("\n")
        for i, ln in enumerate(lines):
            s = ln.strip()
            if not s or s.startswith(("//", "#", "*")):
                continue
            m = NPOS.search(s)
            if m and (m.group(1).endswith((".inc", ".cpp", ".h", ".hpp", ".json", ".csv", ".fits", ".yaml"))) or (m and re.search(r'\.find\("[^"]*(\.inc|\.cpp|\.h|\.py|CMakeLists|\.json|\.yaml|\.csv)', s)):
                cat["src_text_guard"].append({"f": r, "l": i+1, "s": s[:200]})
            if re.search(r'(gate_diag_name|diag_message|error\(\)\.message\(\)|\.reason|fallback_reason|stale_reason|warning_text|failure_reason|["\']msg["\']|\["message"\]|stderr)', s) and re.search(r'assertIn\(|assertNotIn\(|\.find\("[^"]*"\)[^;]*npos|in \(|in text|in out\b|in js\b|in src\b|assertRegex\(', s):
                cat["diagtext_pin"].append({"f": r, "l": i+1, "s": s[:200]})
            if re.search(r'assertGreaterEqual\([^,]+,\s*1\s*\)|assertGreater\([^,]+,\s*0\s*\)|assertTrue\(os\.path\.exists|assertTrue\(\s*\w+\.exists\(\)|assertIsNotNone\(|len\([^)]*\)\s*[<>]=?\s*\d', s):
                cat["positive_exist"].append({"f": r, "l": i+1, "s": s[:200]})
            if re.search(r'FAULT|fault_env|ASTROCS_[A-Z0-9_]*FAULT|setenv\(|_inject|inject_|注入|tamper|corrupt', s):
                cat["inject_reverse_lock"].append({"f": r, "l": i+1, "s": s[:200]})

print("scanned files:", scanned)
for k, v in cat.items():
    print("###", k, len(v))
json.dump(cat, open(OUT, "w", encoding="utf-8"), ensure_ascii=False, indent=1)
