import os, re, json
ROOT = "/workspace/Astro CS Database"
hits = json.load(open(os.path.join(ROOT, "问题扫描/_verify/scripts_v17_censusA.json")))
from collections import defaultdict
files = defaultdict(lambda: {"read": [], "npos": []})
for h in hits["cpp_read"]:
    p, n, s = h.split(":", 2)
    files[p]["read"].append((int(n), s))
for h in hits["cpp_npos"]:
    p, n, s = h.split(":", 2)
    files[p]["npos"].append((int(n), s))
# negative-only assertion sites: find(...) == npos used inside CHECK/ASSERT (green when empty)
for p in sorted(files):
    if not (p.startswith("tests/") or "/tests/" in p or "selftest" in p):
        continue
    neg = [(n, s) for (n, s) in files[p]["npos"] if re.search(r"CHECK\(|ASSERT\(|if .*return|EXPECT", s)]
    negcheck = [(n, s) for (n, s) in files[p]["npos"] if "CHECK(" in s or "assert" in s.lower()]
    if files[p]["read"] and negcheck:
        print("==", p)
        for n, s in files[p]["read"]: print("   R", n, s[:110])
        for n, s in negcheck[:8]: print("   N", n, s[:110])