#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-502 独立复算 ①：注册表只读普查（不执行任何 eng/ci 脚本、不 import 仓库 Python）。

复刻三条口径各自的"单元展开/字段解析"规则，逐一数出：
  A. run_checks.py 展开口径（expand_steps + INHERIT_FIELDS 继承）
  B. failclosed_survey.py 展开口径（units()：step.get("outputs") 原样，无继承）
  C. checks.schema.json / validate_registry.py 合同口径（STEP_REQUIRED 必须显式）
并模拟 Windows 本机节点上 run_checks.py 的 SKIPPED(platform) 归属。
"""
import io
import json
import sys
from collections import Counter, defaultdict

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = r"F:\Astro dev\Astro CS Normalization Database"
REG = REPO + r"\eng\ci\checks.json"
# 与 run_checks.py:155 逐字一致的继承字段表
INHERIT_FIELDS = ("command", "profiles", "platform", "timeout_seconds", "heavy",
                  "mutates_workspace", "outputs", "waivable",
                  "requires_monitor", "prerequisite_tools", "changed_paths",
                  "dirty_ignore_exact", "dirty_ignore_prefixes", "fingerprint",
                  "inputs", "optional_inputs")
# 与 validate_registry.py:183 逐字一致
STEP_REQUIRED = ("id", "command", "timeout_seconds", "profiles", "platform")
PROFILES = ("fast", "integration", "linux-main", "windows-main",
            "linux-deep", "prerelease", "fatduck")

reg = json.loads(io.open(REG, encoding="utf-8").read())
checks = reg["checks"]
print(f"registry_top_keys={sorted(reg.keys())}")
print(f"entries(checks 数组长度)={len(checks)}")

# ── A：run_checks.py expand_steps 复刻 ────────────────────────────────
unitsA = []            # (parent_id, unit_id, merged_step)
inherited_fields = Counter()
units_missing_platform_entirely = []
for c in checks:
    raw = c.get("steps")
    raw = [{}] if raw is None else raw
    for j, s in enumerate(raw):
        merged = {}
        for f in INHERIT_FIELDS:
            if f in s:
                merged[f] = s[f]
            elif f in c:
                merged[f] = c[f]
                inherited_fields[f] += 1
        sid = s.get("id", c["id"])
        merged["id"] = sid
        merged["platform"] = merged.get("platform", "any")
        merged["_from_parent"] = (f"platform" if "platform" in c and "platform" not in s else "")
        unitsA.append((c["id"], sid, merged))
        if "platform" not in s and "platform" not in c:
            units_missing_platform_entirely.append((c["id"], sid))

print(f"\n[A] run_checks 展开后执行单元数 = {len(unitsA)}")
print(f"[A] 有效 platform 分布 = {Counter(u[2]['platform'] for u in unitsA)}")
print(f"[A] step 靠父项继承得到的字段计数 = {dict(inherited_fields)}")
print(f"[A] step 与父项均无 platform（run_checks 兜底 any）= {len(units_missing_platform_entirely)}"
      f" {units_missing_platform_entirely[:8]}")

# ── B：failclosed_survey.units() 口径 ────────────────────────────────
unitsB = []
for c in checks:
    steps = c.get("steps")
    if isinstance(steps, list) and steps:
        for s in steps:
            unitsB.append((c["id"], s.get("id"), s))
    else:
        unitsB.append((c["id"], c["id"], c))
print(f"\n[B] failclosed_survey 口径单元数 = {len(unitsB)}")
has_out = [u for u in unitsB if u[2].get("outputs")]
no_out_nonwaiv = [u for u in unitsB if not u[2].get("outputs") and not u[2].get("waivable")]
no_out_waiv = [u for u in unitsB if not u[2].get("outputs") and u[2].get("waivable")]
req_mon = [u for u in unitsB if u[2].get("requires_monitor")]
glob_out = [u for u in unitsB if any(ch in str(x) for x in (u[2].get("outputs") or []) for ch in "*?[")]
print(f"[B] 声明 outputs（→ 面 A 适用）的单元 = {len(has_out)}")
print(f"[B] outputs 空且非 waivable（→ 面 C 适用）= {len(no_out_nonwaiv)}")
print(f"[B] outputs 空且 waivable（→ 无适用面，除非 requires_monitor）= {len(no_out_waiv)}")
print(f"[B] requires_monitor=true 的单元 = {len(req_mon)}")
print(f"[B] outputs 含 glob 的单元 = {len(glob_out)}")
print(f"[B] requires_monitor 且 outputs 含 glob = "
      f"{[u[1] for u in req_mon if any(ch in str(x) for x in (u[2].get('outputs') or []) for ch in '*?[')]}")
print(f"[B] requires_monitor 但无 .json 声明输出（面 B 记 N/A）= "
      f"{[u[1] for u in req_mon if not any(str(x).endswith('.json') for x in (u[2].get('outputs') or []))]}")

# ── C：合同口径（STEP_REQUIRED 显式） ────────────────────────────────
agg_missing = defaultdict(list)
for c in checks:
    steps = c.get("steps")
    if isinstance(steps, list) and steps:
        for s in steps:
            for f in STEP_REQUIRED:
                if f not in s:
                    agg_missing[f].append(f"{c['id']}/{s.get('id')}")
print(f"\n[C] 未显式声明 STEP_REQUIRED 字段的 step 数（按字段）= "
      f"{ {k: len(v) for k, v in agg_missing.items()} }")
for k, v in agg_missing.items():
    print(f"    {k}: {v[:10]}{' ...' if len(v) > 10 else ''}")

# ── Windows 本机：run_checks.py SKIPPED(platform) 归属 ───────────────
RUN_PLATFORM = "windows"        # current_platform('auto') on win32
def skipped(p):                 # run_checks.py:603
    return p != "any" and p != RUN_PLATFORM

print(f"\n=== 本机 platform 判定 = {RUN_PLATFORM}（run_checks.py:192-197）===")
allA = len(unitsA)
sk_all = [u for u in unitsA if skipped(u[2]["platform"])]
print(f"全注册表：单元 {allA} 个，落进 SKIPPED(platform) 的 = {len(sk_all)}"
      f"（{len(sk_all)/allA*100:.1f}%）")
print(f"  其中 platform=linux {sum(1 for u in sk_all if u[2]['platform']=='linux')} / "
      f"fatduck {sum(1 for u in sk_all if u[2]['platform']=='fatduck')} / "
      f"windows {sum(1 for u in sk_all if u[2]['platform']=='windows')}")

# 按 profile 的"选中集"（--all --profile P，select() :1206）
print("\n--all --profile P 在 Windows 本机的选中/跳过：")
for p in PROFILES:
    sel = [u for u in unitsA if p in (u[2].get("profiles") or [])]
    s = [u for u in sel if skipped(u[2]["platform"])]
    allskipped = bool(sel) and len(s) == len(sel)
    print(f"  profile={p:14s} selected={len(sel):4d} skipped={len(s):4d} "
          f"executed={len(sel)-len(s):4d}  整机零执行仍报 PASS(rc=0)? {'是' if allskipped else '否'}")

# 按注册项（--check <ID> 不做 profile 过滤，select() :1193-1204）
by_entry = defaultdict(list)
for pid, uid, m in unitsA:
    by_entry[pid].append(m)
entries_all_skipped = [pid for pid, ms in by_entry.items()
                       if ms and all(skipped(m["platform"]) for m in ms)]
print(f"\n--check <ID>（显式点名，不受 profile 限定）在 Windows 本机：")
print(f"  全部 step 都被 platform 跳过的注册项数 = {len(entries_all_skipped)} / {len(by_entry)}")
print(f"  这些项在本机跑 run_checks.py --check ⇒ failures=[] → verdict=PASS → rc=0")
for pid in sorted(entries_all_skipped):
    plats = sorted({m['platform'] for m in by_entry[pid]})
    profs = sorted({p for m in by_entry[pid] for p in (m.get('profiles') or [])})
    print(f"    {pid:38s} platform={plats} profiles={profs} steps={len(by_entry[pid])}")
