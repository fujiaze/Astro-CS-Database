"""W4 evidence: how do the 11 doc layers actually satisfy `must_contain`?

For every layer registered in
eng/contracts/schemas/hips_storage_form.schema.json#x-astrocs-field-vocabulary.layers:
  * split the file into fenced code blocks vs prose (GFM ``` / ~~~ fences);
  * count occurrences of each must_contain token in prose and in code separately;
  * detect a vocabulary pointer (reference to hips_storage_form.schema.json or the
    phrase 唯一词表 / 唯一源) and whether it sits in prose;
  * classify the layer:
      POINTER_ONLY  - has pointer, no token satisfied solely by prose re-listing
      RESTATEMENT   - at least one required token is satisfied ONLY by prose that
                      re-lists >= 2 vocabulary tokens on one line (a field list)
      CODE_ONLY     - a required token appears only inside a fenced block
A token satisfied by zero occurrences = the gate is red for that layer today.
"""
import io
import json
import os
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"
SCHEMA = "eng/contracts/schemas/hips_storage_form.schema.json"

vocab = json.loads(io.open(os.path.join(REPO, SCHEMA), encoding="utf-8").read())
layers = vocab["x-astrocs-field-vocabulary"]["layers"]
all_tokens = set()
for ent in layers:
    all_tokens |= set(ent["must_contain"])
forbid = set(vocab["x-astrocs-field-vocabulary"].get("forbidden_synonyms", []))

FENCE = re.compile(r"^(\s*)(```|~~~)")


def split_fenced(text):
    """Return list of (kind, line) with kind in {'prose','code'}; fence lines are code."""
    out = []
    in_code = False
    for ln in text.split("\n"):
        if FENCE.match(ln):
            in_code = not in_code
            out.append(("code", ln))
            continue
        out.append(("code" if in_code else "prose", ln))
    return out


POINTER_RE = re.compile(r"(hips_storage_form\.schema\.json|x-astrocs-field-vocabulary|唯一词表)")

print("layers:", len(layers))
print()
hdr = "%-16s %-46s %5s %6s %6s %8s %s"
print(hdr % ("layer", "file", "tokens", "prose", "code", "pointer", "classification"))
summary = {}
for ent in layers:
    rel = ent["file"]
    text = io.open(os.path.join(REPO, rel.replace("/", os.sep)), encoding="utf-8",
                   errors="replace").read()
    kinds = split_fenced(text)
    prose = "\n".join(l for k, l in kinds if k == "prose")
    code = "\n".join(l for k, l in kinds if k == "code")
    missing = [t for t in ent["must_contain"] if t not in text]
    code_only = [t for t in ent["must_contain"] if t in code and t not in prose]
    # a "restatement line" = prose line carrying >=2 distinct vocabulary tokens
    rest_lines = 0
    for line in prose.split("\n"):
        found = {t for t in all_tokens if t in line}
        if len(found) >= 2:
            rest_lines += 1
    ptr = len(POINTER_RE.findall(prose))
    cls = []
    if missing:
        cls.append("MISSING(%s)" % ",".join(missing))
    if code_only:
        cls.append("CODE_ONLY(%s)" % ",".join(code_only))
    if ptr:
        cls.append("HAS_POINTER")
    if rest_lines:
        cls.append("RESTATE_LINES=%d" % rest_lines)
    n_prose = sum(prose.count(t) for t in ent["must_contain"])
    n_code = sum(code.count(t) for t in ent["must_contain"])
    summary[ent["layer"]] = (len(ent["must_contain"]), n_prose, n_code, ptr, rest_lines, missing)
    print(hdr % (ent["layer"], rel, len(ent["must_contain"]), n_prose, n_code, ptr,
                 " ".join(cls) or "-"))

print()
print("== 汇总 ==")
with_ptr = [k for k, v in summary.items() if v[3]]
restating = [k for k, v in summary.items() if v[4]]
pure_pointer = [k for k, v in summary.items() if v[3] and not v[4]]
print("  含词表指针的层        :", len(with_ptr), with_ptr)
print("  含清单式复述行的层    :", len(restating), restating)
print("  只靠指针、无复述行的层:", len(pure_pointer), pure_pointer)
print("  must_contain 有缺词的层:", [k for k, v in summary.items() if v[5]])

print()
print("== forbidden_synonyms 现况（D2 判据的正面读数）==")
for s in sorted(forbid):
    hits = []
    for ent in layers:
        t = io.open(os.path.join(REPO, ent["file"].replace("/", os.sep)), encoding="utf-8",
                    errors="replace").read()
        if s in t:
            hits.append(ent["file"])
    print("  %-22s 出现于 %d 层 %s" % (s, len(hits), hits[:3]))

print()
print("== 每个 token 在多少层里被字面复述（口径词分布）==")
cnt = {}
for ent in layers:
    for t in ent["must_contain"]:
        cnt[t] = cnt.get(t, 0) + 1
for t, n in sorted(cnt.items(), key=lambda x: -x[1]):
    print("  %-22s 需求它的层数=%d" % (t, n))
