#!/usr/bin/env python3
"""Static quality: TODO/FIXME/HACK/XXX density + hardcoded numeric constants in lib/ (Control §13)."""
import os, re, collections, json

REPO = "/home/lighthouse/Astro CS Database"
OUT = os.path.join(open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip(),
                   "package", "13_static_quality")

PATTERNS = {
    "TODO": r"\bTODO\b",
    "FIXME": r"\bFIXME\b",
    "HACK": r"\bHACK\b",
    "XXX": r"\bXXX\b",
    "TEMP": r"\bTEMP(ORARY)?\b",
    "WORKAROUND": r"\bworkaround\b",
    "HOTFIX": r"\bhotfix\b",
}

by_mod = collections.defaultdict(collections.Counter)
total = collections.Counter()
file_count = 0
worst = []
for dp, _, fns in os.walk(os.path.join(REPO, "lib")):
    if "/third_party/" in dp or "/archive/" in dp or "/_deps/" in dp or "/build/" in dp:
        continue
    for fn in fns:
        if not fn.endswith((".cpp", ".h", ".hpp", ".cc", ".c", ".py")):
            continue
        p = os.path.join(dp, fn)
        rel = os.path.relpath(p, REPO)
        parts = rel.split(os.sep)
        mod = parts[1] if len(parts) > 1 else "?"
        txt = open(p, encoding="utf-8", errors="ignore").read()
        file_count += 1
        cnt = 0
        for label, pat in PATTERNS.items():
            n = len(re.findall(pat, txt))
            if n:
                total[label] += n
                by_mod[mod][label] += n
                cnt += n
        if cnt:
            worst.append((cnt, rel))

worst.sort(reverse=True)
result = {
    "file_count": file_count,
    "pattern_totals": dict(total),
    "by_module": {k: dict(v) for k, v in sorted(by_mod.items())},
    "top_files_by_marker_density": [(c, p) for c, p in worst[:25]],
}
os.makedirs(OUT, exist_ok=True)
open(os.path.join(OUT, "static_marker_density.json"), "w").write(
    json.dumps(result, indent=2, ensure_ascii=False))
print("files scanned:", file_count)
print("pattern totals:", dict(total))
print("by module:")
for mod, c in sorted(by_mod.items()):
    print("  ", mod, dict(c))
print("top files:")
for c, p in worst[:15]:
    print("  ", c, p)
