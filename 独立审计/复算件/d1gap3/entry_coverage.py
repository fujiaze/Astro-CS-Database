#!/usr/bin/env python3
"""Entry-position coverage: does each universe path have a real 14-field entry in some ledger?"""
import sys, os, io, re

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
BASE = r"产出/"
INV, RAW = os.path.join(BASE, "inventory"), os.path.join(BASE, "raw")
WORK = os.path.join(BASE, "复算", "d1gap3")

def rd(p):
    with open(p, encoding="utf-8", errors="replace") as f:
        return f.read()

def lst(name):
    return [x.strip().replace("\\", "/") for x in rd(os.path.join(INV, name)).splitlines() if x.strip()]

universe = lst("DA-all.txt")

# ledger -> its assigned batch inventory files
BATCH = {
    "AUD-101-DA01-根规范与科学.md": ["DA-01.txt"],
    "AUD-101-DA02-算法推导.md": ["DA-02.txt"],
    "AUD-101-D1残余.md": ["DA-02-missing.txt", "D1-residual.txt"],
    "AUD-101-DB01.md": ["DB-01.txt"],
    "AUD-101-DB02.md": ["DB-02.txt"],
    "AUD-101-DB-03.md": ["DB-03.txt"],
    "AUD-101-DB-04.md": ["DB-04.txt"],
    "AUD-101-DB-05.md": ["DB-05.txt"],
    "AUD-101-DB-06-07.md": ["DB-06.txt", "DB-07.txt"],
    "AUD-101-DB-08.md": ["DB-08.txt"],
    "AUD-101-DB-09.md": ["DB-09.txt"],
    "AUD-101-DB-10.md": ["DB-10.txt"],
    "AUD-101-DB-10-补.md": ["DB-10.txt"],
    "AUD-101-DB-11.md": ["DB-11.txt"],
    "AUD-101-DB-12.md": ["DB-12.txt"],
    "AUD-101-DB-13.md": ["DB-13.txt"],
    "AUD-101-DB-14.md": ["DB-14.txt"],
    "AUD-101-DB-15.md": ["DB-15.txt"],
    "AUD-101-DB-16.md": ["DB-16.txt"],
    "AUD-101-DB-17.md": ["DB-17.txt"],
    "AUD-101-DB-18.md": ["DB-18.txt"],
    "AUD-101-DB-19.md": ["DB-19.txt"],
    "AUD-101-DB-20.md": ["DB-20.txt"],
}

EXT = r"(?:md|py|yaml|yml|csv|json|txt)"
# entry position 1: heading line containing a filename token
HEADING = re.compile(r"^#{2,6}[^\n]*?([^\s|`]+?\." + EXT + r")")
# entry position 2: markdown table row whose first cell is a filename token
TBLROW = re.compile(r"^\s*\|\s*`?([^\s|`]+?\." + EXT + r")`?\s*\|")

ledgers = sorted(n for n in os.listdir(RAW) if n.startswith("AUD-101-") and n.endswith(".md"))
assert set(ledgers) == set(BATCH), (set(ledgers) ^ set(BATCH))

reg = {}          # universe path -> list of (ledger, token)
unmatched_tok = {}
for n in ledgers:
    text = rd(os.path.join(RAW, n))
    own = lst(BATCH[n][0])
    for extra in BATCH[n][1:]:
        own += lst(extra)
    own = [p for p in dict.fromkeys(own)]
    toks = set()
    for line in text.splitlines():
        for pat in (HEADING, TBLROW):
            m = pat.match(line)
            if m:
                toks.add(m.group(1).strip().strip("`").strip())
    matched = set()
    # unique-basename index prevents one "README.md" entry from covering a whole batch
    byname = {}
    for p in own:
        byname.setdefault(os.path.basename(p), []).append(p)
    for p in own:
        base = os.path.basename(p)
        uniq_base = len(byname.get(base, [])) == 1
        for tok in toks:
            t = tok.replace("\\", "/").lstrip("./")
            suffix_hits = [q for q in own if q == t or q.endswith("/" + t)]
            if t == p or (len(suffix_hits) == 1 and suffix_hits[0] == p) or (uniq_base and t == base):
                matched.add(p)
                reg.setdefault(p, []).append((n, tok))
                break
    unmatched_tok[n] = [t for t in toks if t not in own and not any(t.endswith("/" + p) or p.endswith("/" + t) for p in own)]
    print(f"{n}: 分配 {len(own)}  条目位 token {len(toks)}  条目命中 {len(matched)}  缺 {len(own)-len(matched)}")
    for p in own:
        if p not in matched:
            print("     MISS:", p)

covered = set(reg)
gap = [p for p in universe if p not in covered]
print("\n=== universe", len(universe), " entry-covered", len(covered & set(universe)), " GAP", len(gap))
for p in gap:
    print("   GAP:", p, "| in db_u:", p in set(lst("db_u.txt")))

with open(os.path.join(WORK, "entry_gap.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(gap) + "\n")

# per-batch detail for the three candidates
print("\n--- candidate entry evidence ---")
for p in ["docs/standards/TEST_STANDARD.md",
          "docs/algorithms/anchors/check_doc_line_anchors.py",
          "docs/standards/checks/check_standards_registry.py"]:
    print(p, "=>", reg.get(p, "NO ENTRY"))
