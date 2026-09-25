"""W3c: replicate the two anchor-liveness assertions by reading.

1) test_cfg001_contracts.KEY_ANCHORS  -> grep_line(path, line, token)
2) defaults.json fields[].source_ref   -> path exists / line in range / line non-empty
   plus: does the registered source_ref agree with the CFG002 token_anchors?
3) which config gates are registered in eng/ci/checks.json (the doc claims none).
"""
import io
import json
import os
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"

_src = io.open(os.path.join(REPO, "eng/tests/config/test_cfg001_contracts.py"),
               encoding="utf-8").read()
_block = _src.split("KEY_ANCHORS = [", 1)[1].split("]", 1)[0]
KEY_ANCHORS = [(a, b, int(c), t) for a, b, c, t in re.findall(
    r'\("([^"]+)",\s*"([^"]+)",\s*(\d+),\s*"([^"]+)"\)', _block)]


def lines(rel):
    return io.open(os.path.join(REPO, rel.replace("/", os.sep)), encoding="utf-8",
                   errors="replace").read().split("\n")


print("== 1) KEY_ANCHORS (len=%d) recomputed ==" % len(KEY_ANCHORS))
d = json.load(io.open(os.path.join(REPO, "eng/packaging/config/defaults.json"), encoding="utf-8"))
byk = {f["key"]: f for f in d["fields"]}
fails = []
for key, path, line, token in KEY_ANCHORS:
    ls = lines(path)
    hold = line <= len(ls) and token in ls[line - 1]
    reg = byk.get(key, {}).get("source_ref") or {}
    agree = (reg.get("path") == path and reg.get("line") == line)
    where = [i + 1 for i, l in enumerate(ls) if token in l][:6]
    tag = "OK " if (hold and agree) else "FAIL"
    if tag == "FAIL":
        fails.append(key)
    print("  %s %-32s %s:%-4d token=%-12r in_line=%-5s registered_same=%-5s token_actually_at=%s"
          % (tag, key, path, line, token, hold, agree, where))
print("  failing:", len(fails), fails)

print()
print("== 2) defaults.json source_ref liveness (CFG002-09 face) ==")
bad = []
n_ref = 0
for f in d["fields"]:
    ref = f.get("source_ref")
    if not ref:
        continue
    n_ref += 1
    p, ln = ref.get("path"), ref.get("line")
    fp = os.path.join(REPO, str(p).replace("/", os.sep))
    if not os.path.isfile(fp):
        bad.append((f["key"], p, ln, "文件不存在"))
        continue
    ls = io.open(fp, encoding="utf-8", errors="replace").read().split("\n")
    if not isinstance(ln, int) or not (1 <= ln <= len(ls)):
        bad.append((f["key"], p, ln, "行越界(%d)" % len(ls)))
    elif not ls[ln - 1].strip():
        bad.append((f["key"], p, ln, "空行"))
print("  source_ref entries: %d ; problems: %d" % (n_ref, len(bad)))
for x in bad:
    print("    ", x)

print()
print("== 3) are the config gates registered in eng/ci/checks.json? ==")
c = json.load(io.open(os.path.join(REPO, "eng/ci/checks.json"), encoding="utf-8"))
blob = json.dumps(c, ensure_ascii=False)
for pat in ("eng/tests/config", "check_cfg002_registry", "test_cfg001", "CONFIG-CONTRACT",
            "CON-CONFIG-CONTRACTS", "UT-CONFIG"):
    print("  %-26s present=%s  occurrences=%d"
          % (pat, pat in blob, blob.count(pat)))
hits = re.findall(r'"id"\s*:\s*"([^"]*(?:CONFIG|CFG)[^"]*)"', blob)
print("  ids containing CONFIG/CFG:", sorted(set(hits)))
