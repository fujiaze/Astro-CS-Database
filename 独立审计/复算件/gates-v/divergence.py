#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-502 独立复算 ②：两入口在同一情形下的判定分叉面 + 嵌套派发面。"""
import io
import json
import sys
from collections import Counter, defaultdict

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = r"F:\Astro dev\Astro CS Normalization Database"
reg = json.loads(io.open(REPO + r"\eng\ci\checks.json", encoding="utf-8").read())
checks = reg["checks"]

INHERIT = ("command", "profiles", "platform", "timeout_seconds", "heavy",
           "mutates_workspace", "outputs", "waivable", "requires_monitor",
           "prerequisite_tools", "changed_paths", "dirty_ignore_exact",
           "dirty_ignore_prefixes", "fingerprint", "inputs", "optional_inputs")

units = []            # run_checks 口径（含继承）
for c in checks:
    raw = c.get("steps") or [{}]
    for s in raw:
        m = {}
        for f in INHERIT:
            if f in s:
                m[f] = s[f]
            elif f in c:
                m[f] = c[f]
        m["id"] = s.get("id", c["id"])
        m.setdefault("platform", "any")
        units.append((c["id"], m["id"], m, c))

print("=== 1. 嵌套派发：哪些注册项的 command 调 run_checks.py（run.py 把它当"
      "一个单元执行）===")
nested = [c["id"] for c in checks if "run_checks.py" in " ".join(
    [str(x) for x in (c.get("command") or [])])]
nested_units = [c["id"] for c in checks if any(
    "run_checks.py" in " ".join(str(x) for x in (s.get("command") or []))
    for s in (c.get("steps") or []))]
print(f"  顶层 command 含 run_checks.py 的注册项 = {len(nested)} / {len(checks)}")
print(f"  其中 P0 文档化的聚合项样例：{nested[:12]}")
print(f"  仅 step 级 command 含 run_checks.py 的注册项 = {len(set(nested_units) - set(nested))}")

print("\n=== 2. Windows 本机：注册项整体被 platform 跳过（run_checks --check ⇒ PASS/rc0）===")
by_entry = defaultdict(list)
for pid, uid, m, c in units:
    by_entry[pid].append((m, c))
def sk(p, plat):
    return p != "any" and p != plat
w_all = [pid for pid, ms in by_entry.items() if all(sk(m['platform'], 'windows') for m, _ in ms)]
print(f"  全部单元 platform 非 any/windows 的注册项 = {len(w_all)}")
lv = Counter()
for pid in w_all:
    c = by_entry[pid][0][1]
    lv[bool(c.get("waivable"))] += 1
print(f"  这些项的顶层 waivable 分布 = 非 waivable {lv[False]} / waivable {lv[True]}"
      f"（run_checks:603 在 waivable 判定之前，故 waivable 与否都记 SKIP → rc0）")
print(f"  run.py 侧同一情形（顶层 platform 不匹配）：非 waivable → FAIL(prerequisite) rc1；"
      f"waivable → SKIPPED(waivable)，若整机只有它 → no_checks_selected FAIL rc1")

print("\n=== 3. Linux CI 本机镜像情形：注册项整体被 platform 跳过 ===")
l_all = [pid for pid, ms in by_entry.items() if all(sk(m['platform'], 'linux') for m, _ in ms)]
print(f"  全部单元 platform 非 any/linux 的注册项 = {len(l_all)} → {l_all}")
for pid in l_all:
    ms = by_entry[pid]
    print(f"    {pid}: profiles={sorted({p for m,_ in ms for p in (m.get('profiles') or [])})} "
          f"waivable={[(m['id'], bool(m.get('waivable'))) for m,_ in ms]}")

print("\n=== 4. profile 成员与 platform 的自相矛盾（跨平台档里挂着单平台单元）===")
bad = defaultdict(list)
for pid, uid, m, c in units:
    profs = m.get("profiles") or []
    p = m["platform"]
    if "windows-main" in profs and p == "linux":
        bad["linux 单元挂进 windows-main 档"].append((pid, uid, bool(m.get("waivable"))))
    if "linux-main" in profs and p == "windows":
        bad["windows 单元挂进 linux-main 档"].append((pid, uid, bool(m.get("waivable"))))
    if "fatduck" in profs and p != "fatduck":
        bad["fatduck 档但 platform!=fatduck"].append((pid, uid, bool(m.get("waivable"))))
    if p == "fatduck" and "fatduck" not in profs:
        bad["platform=fatduck 但不在 fatduck 档"].append((pid, uid, bool(m.get("waivable"))))
for k, v in bad.items():
    print(f"  {k}: {len(v)}")
    for row in v[:10]:
        print(f"     {row[0]:32s}/{row[1]:28s} waivable={row[2]}")

print("\n=== 5. run_checks 继承 vs survey 不继承：同一单元的字段取值不同 ===")
n_inherit_outputs = 0
n_inherit_waivable = 0
n_inherit_reqmon = 0
for c in checks:
    for s in (c.get("steps") or [{}]):
        if "outputs" not in s and "outputs" in c:
            n_inherit_outputs += 1
        if "waivable" not in s and "waivable" in c:
            n_inherit_waivable += 1
        if "requires_monitor" not in s and "requires_monitor" in c:
            n_inherit_reqmon += 1
print(f"  靠父项继承 outputs 的单元 = {n_inherit_outputs}（failclosed_survey 读到的是空/自身值）")
print(f"  靠父项继承 waivable 的单元 = {n_inherit_waivable}")
print(f"  靠父项继承 requires_monitor 的单元 = {n_inherit_reqmon}")
# 面归属差异：runner 执行时会带父 outputs；普查只看 step 自身
face_diff = []
for c in checks:
    for s in (c.get("steps") or [{}]):
        own = s.get("outputs") or []
        eff = s["outputs"] if "outputs" in s else (c.get("outputs") or [])
        if bool(own) != bool(eff):
            face_diff.append((c["id"], s.get("id", c["id"]), len(own), len(eff)))
print(f"  「普查面归属」与「runner 实际强制面」不一致的单元 = {len(face_diff)}")
for row in face_diff[:10]:
    print(f"     {row[0]}/{row[1]}: 普查见 outputs={row[2]} 条 → runner 强制 {row[3]} 条")

print("\n=== 6. 普查充要条件复算：单元是否有 applicable face ===")
SILENT_OK_UNITS = {"API-DOCS", "UNIT-CLOSURE"}
rows = []
for c in checks:
    steps = c.get("steps")
    its = [(s.get("id"), s) for s in steps] if (isinstance(steps, list) and steps) else [(c["id"], c)]
    for uid, s in its:
        outs = [str(x) for x in (s.get("outputs") or [])]
        waivable = bool(s.get("waivable"))
        fa = bool(outs)
        fb = bool(s.get("requires_monitor")) and any(o.endswith(".json") for o in outs)
        fc = (not outs) and (not waivable) and (uid not in SILENT_OK_UNITS)
        rows.append((c["id"], uid, fa, fb, fc))
applicable = [r for r in rows if r[2] or r[3] or r[4]]
no_face = [r for r in rows if not (r[2] or r[3] or r[4])]
print(f"  单元总数 = {len(rows)}；有 ≥1 适用面 = {len(applicable)}；无适用面 = {len(no_face)}")
print(f"  无适用面清单 = {[(r[0], r[1]) for r in no_face]}")
print(f"  仅因『登记了 outputs』就有适用面的单元 = {sum(1 for r in applicable if r[2])}")
print(f"  适用面全由 C 面（outputs 空且非 waivable）撑起 = {sum(1 for r in applicable if (not r[2]) and (not r[3]) and r[4])}")
only_a = [r for r in applicable if r[2] and not r[3] and not r[4]]
print(f"  只有 A 面（有 outputs 且 waivable=true，即运行时 outputs 缺失才会被 runner 强制）= {len(only_a)}")
