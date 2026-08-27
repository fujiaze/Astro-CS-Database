#!/usr/bin/env python3
"""§13 duplicate-code scan: exact-duplicate non-trivial function definitions across lib/ (Control §13)."""
import os, re, collections, hashlib

REPO = "/home/lighthouse/Astro CS Database"

# collect function bodies (crude: signature line + next balanced-brace block)
func_pat = re.compile(
    r'^([A-Za-z_][A-Za-z0-9_:<>,\s*&]*?)\s*\n?\([^;{]*\)\s*(?:const\s*)?\{',
    re.M)

bodies = []  # (file, name, hash, length, text)
for dp, _, fns in os.walk(os.path.join(REPO, "lib")):
    if "/third_party/" in dp or "/archive/" in dp or "/_deps/" in dp or "/build/" in dp:
        continue
    for fn in fns:
        if not fn.endswith((".cpp", ".cc")):
            continue
        p = os.path.join(dp, fn)
        rel = os.path.relpath(p, REPO)
        txt = open(p, encoding="utf-8", errors="ignore").read()
        # find function definitions: "ret name(...) {"
        for m in re.finditer(r'(?m)^\s*([A-Za-z_][\w:<>,\s*&]*?)\s+([A-Za-z_]\w*)\s*\(([^;{]*?)\)\s*(?:const\s*)?\{\s*(.+?)\n\}', txt, re.S):
            ret, name, args, body = m.group(1), m.group(2), m.group(3), m.group(4)
            body_n = re.sub(r'\s+', ' ', body).strip()
            if len(body_n) < 200:
                continue  # skip tiny bodies (accessors etc.)
            h = hashlib.sha256(body_n.encode()).hexdigest()
            bodies.append((rel, name, h, len(body_n), body_n[:80]))

dups = collections.defaultdict(list)
for rel, name, h, ln, snip in bodies:
    dups[h].append((rel, name, ln))

dup_groups = [(h, v) for h, v in dups.items() if len(v) > 1]
dup_groups.sort(key=lambda x: -sum(e[2] for e in x[1]))
print("functions scanned:", len(bodies))
print("duplicate groups (>=2 identical bodies, >=200 chars):", len(dup_groups))
for h, v in dup_groups[:8]:
    total = sum(e[2] for e in v)
    print("  %d bytes duplicated across %d files: %s" % (total, len(v), " | ".join("%s:%s" % (a, b) for a, b, _ in v)))
