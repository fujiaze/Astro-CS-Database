"""W3b: re-implement (by reading, not running) the two citation sub-checks of
CFG002-09 as written in eng/tests/config/check_cfg002_registry.py:778-822, for
CONFIG_CONTRACT.md, plus the defaults/registry count reconciliation the doc claims.

Reads only. Prints a per-anchor verdict table.
"""
import io
import json
import os
import re
import sys
from collections import Counter

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"
DOC = "docs/contracts/CONFIG_CONTRACT.md"


def read(rel):
    return io.open(os.path.join(REPO, rel.replace("/", os.sep)), encoding="utf-8",
                   errors="replace").read()


text = read(DOC)
reg = json.loads(read("eng/packaging/config/config_registry.json"))
anchors = reg["contract_doc_citations"]["token_anchors"]

# _find_citation in the checker falls back to a basename search; replicate the
# "resolve by suffix anywhere in the tracked tree" behaviour with a cheap walk.
print("== CFG002-09 token_anchors recomputation ==")
bad = 0
for item in anchors:
    ref, token = item["ref"], item["token"]
    in_doc = ref in text
    path, n = ref.rsplit(":", 1)
    n = n.split("-")[0]
    line_no = int(item.get("token_line", n))
    lines = read(path).split("\n")
    holds = line_no <= len(lines) and token in lines[line_no - 1]
    flag = "OK  " if (in_doc and holds) else "FAIL"
    if flag == "FAIL":
        bad += 1
    print("  %s %-42s doc_cites=%-5s line=%-4d token=%-24r token_in_line=%s"
          % (flag, ref, in_doc, line_no, token, holds))
    if not holds and line_no <= len(lines):
        print("        actual line %d: %s" % (line_no, lines[line_no - 1][:90]))
print("  failing anchors: %d / %d" % (bad, len(anchors)))

print()
print("== CFG002-09 structural rule: every `file.ext:line[-line]` citation ==")
cites = set()
for m in re.finditer(r"([A-Za-z0-9_./\u4e00-\u9fff\-]+\.(?:md|json|jsonc|yaml|csv|py|cpp|h)):(\d+)(?:-(\d+))?",
                     text):
    cites.add((m.group(1), int(m.group(2)), int(m.group(3)) if m.group(3) else None))
prob = []
for path, a, b in sorted(cites):
    p = os.path.join(REPO, path.replace("/", os.sep))
    if not os.path.isfile(p):
        prob.append("%s:%d 文件不存在" % (path, a))
        continue
    lines = read(path).split("\n")
    if not (1 <= a <= len(lines)):
        prob.append("%s:%d 越界（%d 行）" % (path, a, len(lines)))
    elif not lines[a - 1].strip():
        prob.append("%s:%d 指向空行" % (path, a))
    if b is not None and (b < a or b > len(lines)):
        prob.append("%s:%d-%d 范围非法（%d 行）" % (path, a, b, len(lines)))
print("  citations parsed: %d ; structural problems: %d" % (len(cites), len(prob)))
for x in prob:
    print("    ", x)

print()
print("== citations the structural gate CANNOT see (form '§N.M（:a-b）' / 'md` §x:NN') ==")
blind = re.findall(r"([A-Za-z0-9_./\u4e00-\u9fff\-]+\.md)[`\s]*§[0-9.]+[（(]?[:：]?\s*(\d+)(?:\s*[-–]\s*(\d+))?",
                   text)
for x in sorted(set(blind)):
    print("   ", x)
print("  count:", len(set(blind)))

print()
print("== doc-stated counts vs machine facts ==")
d = json.loads(read("eng/packaging/config/defaults.json"))
fields = d["fields"]
groups = Counter(f["key"].split(".")[0] for f in fields)
doc_group_rows = {"calibration": 1, "detection": 1, "psf": 2, "noise": 14, "rejection": 18,
                  "photometry": 6, "weight": 1, "precision": 1, "upm": 1, "hips": 1,
                  "drizzle": 1, "sparse_snr+scalar_gate": 3}
print("  defaults field total      doc=50 (title) / gate-declared pointer=field_count  machine=%d"
      % d["field_count"])
for k, v in sorted(doc_group_rows.items()):
    if "+" in k:
        act = groups.get("sparse_snr", 0) + groups.get("scalar_gate", 0)
    else:
        act = groups.get(k, 0)
    mark = "==" if v == act else "!!"
    print("    %s %-22s doc=%-3d actual=%-3d" % (mark, k, v, act))
extra = sorted(set(groups) - {"calibration", "detection", "psf", "noise", "rejection",
                              "photometry", "weight", "precision", "upm", "hips",
                              "drizzle", "sparse_snr", "scalar_gate"})
print("    !! groups in defaults.json absent from the doc table:",
      [(g, groups[g]) for g in extra], "sum=", sum(groups[g] for g in extra))
print("  authority_status          doc={sourced, owner_adjudicated, pending_authority}"
      "  machine=%s" % sorted({f["authority_status"] for f in fields}))
print("  key anchors               doc §2 line39='12' / doc §7 row='11'  "
      "test KEY_ANCHORS len=? (see test_cfg001_contracts.py:35-50)")
print("  module.yaml               doc='20 份'   tracked=%d"
      % len([1 for p in os.listdir("lib") ]))
