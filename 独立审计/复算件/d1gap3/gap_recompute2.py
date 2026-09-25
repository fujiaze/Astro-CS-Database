#!/usr/bin/env python3
"""D1 gap recompute, three graded tests.

T1 full-path containment in concatenated ledgers (= assembly's gap_check.py rule)
T2 basename containment (loose, shows how many are mentioned-but-not-registered)
T3 registered-entry extraction: only paths that appear as a 台账 entry (table row /
   entry heading) count as registered.
"""
import sys, os, io, re

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

BASE = r"产出/"
INV, RAW = os.path.join(BASE, "inventory"), os.path.join(BASE, "raw")
WORK = os.path.join(BASE, "复算", "d1gap3")

with open(os.path.join(INV, "DA-all.txt"), encoding="utf-8") as f:
    universe = [l.strip().replace("\\", "/") for l in f if l.strip()]
ledgers = sorted(n for n in os.listdir(RAW) if n.startswith("AUD-101-") and n.endswith(".md"))
texts = {}
for n in ledgers:
    with open(os.path.join(RAW, n), encoding="utf-8", errors="replace") as fh:
        texts[n] = fh.read()
blob = "\n".join(texts.values())
print("universe:", len(universe), " ledgers:", len(ledgers), " chars:", len(blob))

# ---- T1 full-path containment ----
t1 = [p for p in universe if p not in blob]
print("\n[T1 full-path missing]", len(t1))
for p in t1:
    print("   ", p)

# self-proving control: a path known to be registered must hit
ctrl = "ASTROCS_DESIGN.md"
print("   control hit for", ctrl, ":", ctrl in blob)

# ---- T2 basename containment ----
t2 = [p for p in universe if os.path.basename(p) not in blob]
print("\n[T2 basename missing]", len(t2))
for p in t2:
    print("   ", p)

# ---- T3 registered-entry extraction ----
# an entry row: markdown table line whose first cell holds a repo path, or a
# heading/bold line naming the file under audit.
pat_row = re.compile(r"^\s*\|\s*([^|]*?\.(?:md|py|yaml|yml|csv|json|txt|sh|cmake))\s*\|", re.I)
pat_head = re.compile(r"^(?:#{1,6}\s*|\*\*)([^#\n]*?\.(?:md|py|yaml|yml|csv|json|txt|sh|cmake))(?:\*\*)?[：:\s]", )
reg_tokens = set()
for n, t in texts.items():
    for line in t.splitlines():
        for pat in (pat_row, pat_head):
            m = pat.match(line)
            if m:
                reg_tokens.add(m.group(1).strip().strip("`*").strip())

def forms(tok):
    tok = tok.replace("\\", "/").lstrip("./")
    out = {tok, os.path.basename(tok)}
    segs = tok.split("/")
    for k in range(1, len(segs)):
        out.add("/".join(segs[k:]))
    return out

reg_full, reg_base, reg_suffix = set(), set(), set()
for tok in reg_tokens:
    reg_full.add(tok)
    reg_base.add(os.path.basename(tok))
    reg_suffix |= forms(tok)

t3 = [p for p in universe if p not in reg_full and p not in reg_suffix]
print("\n[T3 entry-registered missing]", len(t3))
for p in t3:
    print("   ", p)
print("\n[T3 entry-registered missing, basename-loose too]",
      len([p for p in t3 if os.path.basename(p) not in reg_base]))

with open(os.path.join(WORK, "t1_fullpath_missing.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(t1) + "\n")
with open(os.path.join(WORK, "t3_entry_missing.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(t3) + "\n")
