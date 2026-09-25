"""AUD-502 只读普查 4：两种单元字段口径的精确对照（复现"普查对象≠被强制对象"面）。

runner 语义（run_checks.py expand_steps）：字段在 step 中显式出现即用 step 的（含空列表），
否则取父项。旧普查语义（failclosed_survey.units）：只读 step 自身字段，不继承。
"""
import json
import pathlib
import sys
from collections import Counter

sys.stdout.reconfigure(encoding="utf-8")
REPO = pathlib.Path(r"F:/Astro dev/Astro CS Normalization Database")
checks = json.loads((REPO / "eng/ci/checks.json").read_text(encoding="utf-8"))["checks"]
IN = ("command", "profiles", "platform", "timeout_seconds", "heavy", "mutates_workspace",
      "outputs", "waivable", "requires_monitor", "prerequisite_tools", "changed_paths",
      "dirty_ignore_exact", "dirty_ignore_prefixes", "fingerprint", "inputs", "optional_inputs")
SILENT_OK = {"API-DOCS", "UNIT-CLOSURE"}

units = []          # (top, uid, merged, own)
for e in checks:
    if not e.get("steps"):
        units.append((e["id"], e["id"], e, e))
        continue
    for s in e["steps"]:
        merged = {f: (s[f] if f in s else e.get(f)) for f in IN}
        units.append((e["id"], s.get("id", "?"), merged, s))

n_top, n_units, n_agg, n_single = len(checks), len(units), sum(1 for e in checks if e.get("steps")), sum(1 for e in checks if not e.get("steps"))
print("顶层=%d 单元=%d 聚合=%d 单步=%d" % (n_top, n_units, n_agg, n_single))


def face(merged_own_outputs, requires_monitor, waivable, uid):
    """旧普查的面归属；返回适用面集合。"""
    f = []
    if merged_own_outputs:
        f.append("A")
    if requires_monitor:
        f.append("B")
    if (not merged_own_outputs) and (not waivable) and uid not in SILENT_OK:
        f.append("C")
    return frozenset(f)


diff = []
own_a = merged_a = own_c = merged_c = own_b = merged_b = 0
for top, uid, m, own in units:
    mo = bool(m.get("outputs"))
    oo = bool(own.get("outputs"))
    mm = bool(m.get("requires_monitor"))
    om = bool(own.get("requires_monitor"))
    mw = bool(m.get("waivable"))
    ow = bool(own.get("waivable"))
    fa = face(oo, om, ow, uid)
    fm = face(mo, mm, mw, uid)
    own_a += "A" in fa
    merged_a += "A" in fm
    own_b += "B" in fa
    merged_b += "B" in fm
    own_c += "C" in fa
    merged_c += "C" in fm
    if fa != fm:
        diff.append((top, uid, sorted(fa) or ["无"], sorted(fm) or ["无"]))

print("旧普查口径（读 step 自身字段）：A=%d B=%d C=%d" % (own_a, own_b, own_c))
print("runner 强制口径（继承解析后）：A=%d B=%d C=%d" % (merged_a, merged_b, merged_c))
print("面归属不一致的单元数 = %d / %d" % (len(diff), n_units))
for row in diff[:10]:
    print("   ", row)
print("...其余 %d 条未打印" % max(0, len(diff) - 10))

# 显式空列表覆盖父项非空的单元（这就是两口径分叉的主因）
over = [(t, u) for t, u, m, o in units if o.get("outputs") == [] and m.get("outputs")]
print("\nstep 显式登记 outputs:[] 而父项非空的单元数 = %d" % len(over))
print("样例:", over[:6])
own_none = sum(1 for _, _, m, o in units if not o.get("outputs"))
print("自身字段无 outputs 的单元 = %d ；继承后无 outputs = %d" % (own_none, sum(1 for _, _, m, _ in units if not m.get("outputs"))))

# 无适用面的单元（两口径并列给）
no_face_own = [u for _, u, _, o in units if not face(bool(o.get("outputs")), bool(o.get("requires_monitor")), bool(o.get("waivable")), u)]
no_face_merged = [u for _, u, m, _ in units if not face(bool(m.get("outputs")), bool(m.get("requires_monitor")), bool(m.get("waivable")), u)]
print("无适用面（旧口径）=%d %s" % (len(no_face_own), no_face_own))
print("无适用面（继承后）=%d %s" % (len(no_face_merged), no_face_merged))
