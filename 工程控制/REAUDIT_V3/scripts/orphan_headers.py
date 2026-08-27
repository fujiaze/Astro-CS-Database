#!/usr/bin/env python3
"""§13 orphan-header scan: .h/.hpp files in lib/ never #include'd by any other source."""
import os, re, collections

REPO = "/home/lighthouse/Astro CS Database"
headers = {}
sources = []
for dp, _, fns in os.walk(os.path.join(REPO, "lib")):
    if "/third_party/" in dp or "/archive/" in dp or "/_deps/" in dp or "/build/" in dp:
        continue
    for fn in fns:
        rel = os.path.relpath(os.path.join(dp, fn), REPO)
        if fn.endswith((".h", ".hpp")):
            headers[rel] = fn
        elif fn.endswith((".cpp", ".c", ".cc")):
            sources.append(rel)

# find all include targets (basename)
included = set()
for s in sources:
    p = os.path.join(REPO, s)
    try:
        txt = open(p, encoding="utf-8", errors="ignore").read()
    except OSError:
        continue
    for m in re.finditer(r'#include\s*[<"]([^>"]+)[>"]', txt):
        t = m.group(1)
        included.add(t)  # could be path or basename
        included.add(os.path.basename(t))

# a header is orphaned if no source includes it by basename or path
orphans = []
for rel, fn in headers.items():
    # check path variants
    if fn in included or rel in included or "./" + rel in included:
        continue
    # also check relative path without lib/ prefix
    rel2 = rel.replace("lib/", "", 1)
    if rel2 in included:
        continue
    orphans.append(rel)

# exclude module-internal headers included by their own src (same-module) - report raw orphans
print("headers:", len(headers), "sources:", len(sources))
print("orphan headers (no source includes them by basename or path):", len(orphans))
for o in sorted(orphans):
    print("  ", o)
