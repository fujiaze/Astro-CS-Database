# -*- coding: utf-8 -*-
"""V15 普查器 B：定位「读源文件/配置文本 → 子串或正则命中即通过」的守卫（①类）。
   对每个含文件读取的测试函数，抽取其后 assert 的字面量参数，标出被读取对象的类型。"""
import os, re, json
ROOT = "/workspace/Astro CS Database"
SKIP_DIRS = {"run","build","out","worktrees","Testing","logs","artifacts","evidence","reports",
             "工程控制","GaiaDR3","GaiaDR3SP","BASS DR3","AstroCS.wiki","__pycache__","问题扫描",
             "third_party","testdata","CS","Database","graph",".git","astrocs_p1sess_neg",
             "astrocs_p1sess_perf","astrocs_p1sess_props","astrocs_p1sess_test"}

READ = re.compile(r'(read_text\(|\.read\(\)|open\(|readfile|read_file\()')
STR_ASSERT = re.compile(r'(assertIn\(|assertNotIn\(|assertRegex\(|assertNotRegex\(|re\.search\(|re\.match\(|re\.findall\(|\.find\(|in\s+\w+_?(text|src|source|body|content|out|log|line|data|txt)\b)')
PATHISH = re.compile(r'(CMakeLists|\.py\b|\.cpp\b|\.c\b|\.h\b|\.json\b|\.md\b|\.csv\b|\.yaml\b|\.yml\b|\.txt\b|\.cmake\b|\.ps1\b|\.sh\b|checks\.json|known_failures|workflow|README|\.inc\b|\.csv\b|schema)')

hits = []
for dirpath, dirnames, filenames in os.walk(ROOT):
    dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
    for fn in filenames:
        if not fn.endswith(".py"): continue
        p = os.path.join(dirpath, fn)
        r = "./" + os.path.relpath(p, ROOT).replace(os.sep, "/")
        if not (r.startswith("./tests/") or r.startswith("./ci/") or r.startswith("./tools/")): continue
        try:
            lines = open(p, encoding="utf-8", errors="replace").read().split("\n")
        except Exception: continue
        # window: 14 lines back from an assert that looks like a text assertion, require a read in the window
        for i, ln in enumerate(lines):
            if not STR_ASSERT.search(ln): continue
            win = lines[max(0,i-14):i+1]
            joined = "\n".join(win)
            if not READ.search(joined): continue
            if not PATHISH.search(joined): continue
            hits.append({"f": r, "l": i+1, "s": ln.strip()[:180]})

print("candidate ① (py) =", len(hits))
byfile = {}
for h in hits: byfile.setdefault(h["f"], []).append(h)
for f in sorted(byfile, key=lambda x: -len(byfile[x])):
    print(f"{len(byfile[f]):4d}  {f}")
json.dump(hits, open(os.path.join(ROOT,"问题扫描/_verify/scripts_v15_py_text.json"),"w",encoding="utf-8"), ensure_ascii=False, indent=1)
