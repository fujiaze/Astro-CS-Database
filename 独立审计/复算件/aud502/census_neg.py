"""AUD-502 只读普查 3：负例面的两种口径（自身带注入入口 / 有独立配对项 / 两者皆无）。"""
import json
import pathlib
import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = pathlib.Path(r"F:/Astro dev/Astro CS Normalization Database")
checks = json.loads((REPO / "eng/ci/checks.json").read_text(encoding="utf-8"))["checks"]
INHERIT = ("command", "profiles", "platform", "timeout_seconds", "heavy",
           "mutates_workspace", "outputs", "waivable", "requires_monitor",
           "prerequisite_tools", "changed_paths", "dirty_ignore_exact",
           "dirty_ignore_prefixes", "fingerprint", "inputs", "optional_inputs")

NEG_RE = re.compile(r"--self[-_]?test|--fault-inject|--negative|--inject|FAULT")
PAIR_RE = re.compile(r"SELFTEST|SELFTEST|-NEG|NEGATIVE|REPLAY")

top_ids = [e["id"] for e in checks]
units_of = {}
for e in checks:
    us = []
    for s in (e.get("steps") or [None]):
        if s is None:
            us.append(e)
        else:
            m = {f: (s[f] if f in s else e.get(f)) for f in INHERIT}
            m["id"] = s.get("id", e["id"])
            us.append(m)
    units_of[e["id"]] = us

own, paired, none = [], [], []
for tid in top_ids:
    cmds = " ".join(" ".join(map(str, u.get("command") or [])) for u in units_of[tid])
    if NEG_RE.search(cmds) or NEG_RE.search(tid):
        own.append(tid)
        continue
    core = re.sub(r"^(CHK-|AIO-)", "", tid)
    sib = [o for o in top_ids if o != tid and PAIR_RE.search(o)
           and core[:14] in o]
    (paired if sib else none).append(tid)

print("A 自身带可执行注入/自检面（command 或 ID 命中）= %d" % len(own))
print("B 无自身面、但有独立配对负例项 = %d" % len(paired))
print("C 两者皆无 = %d" % len(none))
print("\nC 清单：")
print(" ".join(none))
print("\nC 中注册文档标 P0 的（按 docs/ci/01_CHECKS.md §2 表第 5 列粗取）：")
doc = (REPO / "docs/ci/01_CHECKS.md").read_text(encoding="utf-8")
p0 = set()
for line in doc.splitlines():
    if line.startswith("| ") and "P0" in line:
        m = re.match(r"\|\s*([A-Z0-9_.-]+)\s*\|", line)
        if m:
            p0.add(m.group(1))
print("文档 §2 表首列可解析出的 ID 数 =", len(p0))
print("C∩P0 =", " ".join(sorted(set(none) & p0)))
