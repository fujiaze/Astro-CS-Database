import json, os
from collections import Counter
d = json.load(open("/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662/builds/doc_ref_clean.json"))
# We only captured the sample (8 items). Re-run the full classification inline:
import re
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
SRC = os.path.join(ROOT, "current")
link_re = re.compile(r'\[[^\]]*\]\(([^)]+)\)')
c = Counter(); samples = []
for dp, dn, fn in os.walk(SRC):
    if "/.git" in dp: continue
    for f in fn:
        if not f.endswith(".md"): continue
        p = os.path.join(dp, f)
        for m in link_re.finditer(open(p, encoding="utf-8", errors="ignore").read()):
            tgt = m.group(1).strip()
            if tgt.startswith("http") or tgt.startswith("#") or tgt.startswith("mailto"): continue
            if re.search(r'\s', tgt): continue
            full = os.path.normpath(os.path.join(dp, tgt))
            if os.path.exists(full): continue
            rel = os.path.relpath(p, SRC)
            if tgt.startswith("file:///"):
                c["windows_file_url(original repo layout, target exists on Fatduck not this Linux copy)"] += 1
            elif re.search(r'[()*\d+,\s]', tgt) and not os.path.splitext(tgt)[1]:
                c["api_signature/param/inline-code misparse"] += 1
            else:
                c["genuine_broken"] += 1
                if len(samples) < 6: samples.append((rel, tgt))
print("classification:", dict(c))
print("genuine broken sample:", samples)
