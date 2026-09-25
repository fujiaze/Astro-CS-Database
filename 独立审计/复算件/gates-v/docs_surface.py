#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-502 独立复算 ③：agent 入口在 Windows 本机的"跳过即记 PASS"规模 + 文档处方面。"""
import io
import json
import re
import sys
from collections import defaultdict

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
REPO = r"F:\Astro dev\Astro CS Normalization Database"
reg = json.loads(io.open(REPO + r"\eng\ci\checks.json", encoding="utf-8").read())
checks = reg["checks"]
INHERIT = ("command", "profiles", "platform", "timeout_seconds", "heavy",
           "mutates_workspace", "outputs", "waivable", "requires_monitor",
           "prerequisite_tools", "changed_paths", "dirty_ignore_exact",
           "dirty_ignore_prefixes", "fingerprint", "inputs", "optional_inputs")
units = []
for c in checks:
    for s in (c.get("steps") or [{}]):
        m = {}
        for f in INHERIT:
            if f in s:
                m[f] = s[f]
            elif f in c:
                m[f] = c[f]
        m["id"] = s.get("id", c["id"])
        m.setdefault("platform", "any")
        units.append((c["id"], m["id"], m))

def sk(p, plat):
    return p != "any" and p != plat

by_entry = defaultdict(list)
for pid, uid, m in units:
    by_entry[pid].append(m)

# 1. 注册项（entry）粒度的 profile 成员数 —— 与既往"101 条里 38 条非绿"同口径
print("=== 注册项粒度的 profile 成员数（run.py / run_checks 各自的'选中集'规模）===")
for p in ("fast", "linux-main", "windows-main", "prerelease", "linux-deep", "integration", "fatduck"):
    ent = [c["id"] for c in checks if p in (c.get("profiles") or [])]
    ent_u = [u for _, u, m in units if p in (m.get("profiles") or [])]
    skw = [u for _, u, m in units if p in (m.get("profiles") or []) and sk(m["platform"], "windows")]
    skl = [u for _, u, m in units if p in (m.get("profiles") or []) and sk(m["platform"], "linux")]
    print(f"  {p:13s} 注册项={len(ent):4d} 单元={len(ent_u):4d} "
          f"Win 跳过={len(skw):4d} Win 可执行={len(ent_u)-len(skw):4d} "
          f"Linux 跳过={len(skl):4d} Linux 可执行={len(ent_u)-len(skl):4d}")

# 2. 文档处方面：01_CHECKS.md 里有多少行命令是 run_checks.py --check
doc = io.open(REPO + r"\docs\ci\01_CHECKS.md", encoding="utf-8").read()
rows = [l for l in doc.splitlines() if l.strip().startswith("|")]
pres = [l for l in rows if "run_checks.py --check" in l]
pres_run = [l for l in rows if "eng/ci/run.py" in l]
print(f"\n=== 01_CHECKS.md 处方面 ===")
print(f"  含 run_checks.py --check 的表行 = {len(pres)}")
print(f"  含 eng/ci/run.py 的表行 = {len(pres_run)}")
ids = set()
for l in pres:
    ids |= set(re.findall(r"--check\s+([A-Z0-9][A-Za-z0-9._-]*)", l))
all_linux = {pid for pid, ms in by_entry.items() if ms and all(sk(m["platform"], "windows") for m in ms)}
hit = sorted(ids & all_linux)
print(f"  文档点名的 ID 中、在 Windows 本机整体被 platform 跳过的 = {len(hit)}")
for h in hit:
    lvl = [l for l in rows if l.strip().startswith("|") and h in l.split("|")[1]]
    grade = re.findall(r"\bP[0-2]\b", lvl[0]) if lvl else []
    print(f"     {h:34s} 文档级别标注={grade}")

# 3. 三条"门级"事实核对
print("\n=== 3. 附加核对：fast 档在 Windows 的可执行单元占比 ===")
fu = [m for _, _, m in units if "fast" in (m.get("profiles") or [])]
print(f"  fast 档单元 {len(fu)} / 全注册表单元 {len(units)} = {len(fu)/len(units)*100:.1f}%"
      f" ⇒ 既往『全量扫描 = run_checks.py --all --profile fast』的口径覆盖 "
      f"{len(fu)}/{len(units)} 个执行单元")
print(f"  windows-main 档单元 {len([m for _,_,m in units if 'windows-main' in (m.get('profiles') or [])])}"
      f" / linux-main 档单元 {len([m for _,_,m in units if 'linux-main' in (m.get('profiles') or [])])}")

# 4. "345" 这个数的真实身份
lm = [m for _, _, m in units if "linux-main" in (m.get("profiles") or [])]
print(f"\n=== 4. '345' 口径核对 ===")
print(f"  profiles 含 linux-main 的单元 = {len(lm)}")
print(f"  其中 platform=linux = {len([m for m in lm if m['platform']=='linux'])}"
      f" / platform=any = {len([m for m in lm if m['platform']=='any'])}"
      f" / platform=windows = {len([m for m in lm if m['platform']=='windows'])}")
print(f"  全注册表 platform=linux 的单元 = {len([m for _,_,m in units if m['platform']=='linux'])}")
