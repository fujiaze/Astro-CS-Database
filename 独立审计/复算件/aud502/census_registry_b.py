"""AUD-502 只读普查：从 eng/ci/checks.json 现算门禁清单落地所需的分组面。

只以文本/json 读仓库文件，不 import 仓库 Python，不执行任何 eng/** 脚本。
"""
import json
import pathlib
import re
import sys
from collections import Counter

sys.stdout.reconfigure(encoding="utf-8")

REPO = pathlib.Path(r"F:/Astro dev/Astro CS Normalization Database")
REG = json.loads((REPO / "eng/ci/checks.json").read_text(encoding="utf-8"))
checks = REG["checks"]

INHERIT = ("command", "profiles", "platform", "timeout_seconds", "heavy",
           "mutates_workspace", "outputs", "waivable", "requires_monitor",
           "prerequisite_tools", "changed_paths", "dirty_ignore_exact",
           "dirty_ignore_prefixes", "fingerprint", "inputs", "optional_inputs")


def units():
    out = []
    for e in checks:
        steps = e.get("steps")
        if not steps:
            out.append((e["id"], e["id"], e, False))
            continue
        for s in steps:
            m = {}
            for f in INHERIT:
                if f in s:
                    m[f] = s[f]
                elif f in e:
                    m[f] = e[f]
            out.append((e["id"], s.get("id", "?"), m, True))
    return out


U = units()
print("TOP=%d UNITS=%d AGG=%d SINGLE=%d" % (
    len(checks), len(U), sum(1 for e in checks if e.get("steps")),
    sum(1 for e in checks if not e.get("steps"))))

# 类别：用 ID 前缀与 command 目标脚本粗略归族，只为批量规则的分组面服务
pat = Counter()
for top, uid, m, is_step in U:
    cmd = " ".join(str(x) for x in (m.get("command") or []))
    tgt = ""
    mm = re.search(r"(eng/[^\s\"]+?\.py)", cmd)
    if mm:
        tgt = mm.group(1)
    pat[tgt.split("/")[-1] if tgt else "(no-py-target)"] += 1
print("\n--- TOP COMMAND TARGETS (units) ---")
for k, v in pat.most_common(18):
    print("%4d  %s" % (v, k))

prof = Counter()
for top, uid, m, is_step in U:
    for p in (m.get("profiles") or []):
        prof[p] += 1
print("\n--- PROFILE UNITS ---", dict(prof))

plat = Counter((m.get("platform") or "any") for _, _, m, _ in U)
print("--- PLATFORM UNITS ---", dict(plat))

# 顶层项 platform x profiles 矛盾面
print("\n--- platform/profiles 矛盾顶层项 ---")
for e in checks:
    p = e.get("platform", "any")
    for pr in (e.get("profiles") or []):
        if p == "linux" and pr == "windows-main":
            print("  L-in-win", e["id"])
        if p == "windows" and pr == "linux-main":
            print("  W-in-lin", e["id"])
        if p == "fatduck" and pr != "fatduck":
            print("  F-other", e["id"], pr)

# 无 changed_paths 的单元（增量档选不到 ⇒ 只在 --all 面跑）
no_cp = [u[1] for u in U if not u[2].get("changed_paths")]
print("\n--- 无 changed_paths 单元数 = %d / %d ---" % (len(no_cp), len(U)))
top_no_cp = sorted({u[0] for u in U if not u[2].get("changed_paths")})
print("顶层项完全无 changed_paths 的项数 =", len(top_no_cp))
print("样例:", top_no_cp[:12])

# 没有 inputs 声明但 command 目标为脚本的单元
no_inputs = [u for u in U if not u[2].get("inputs")]
print("\n无 inputs 单元 = %d" % len(no_inputs))
with_out = [u for u in U if u[2].get("outputs")]
print("有 outputs 单元 = %d ；无 outputs = %d" % (len(with_out), len(U) - len(with_out)))

# self-test / fault-inject 面：command 内含 --self-test / --selftest / --fault-inject / -NEG / SELFTEST
st = [u for u in U if re.search(r"--self[-_]?test|--fault-inject|FAULT",
                                " ".join(map(str, u[2].get("command") or [])), re.I)]
stid = [u for u in U if re.search(r"SELFTEST|-NEG|NEGATIVE|_NEG\b", u[1], re.I)]
print("\ncommand 内含 self-test/fault-inject 的单元 = %d" % len(st))
print("ID 含 SELFTEST/NEG 的单元 = %d" % len(stid))
print("既无 self-test 命令面也无 NEG ID 的单元 = %d" % len(
    [u for u in U if u not in st and u not in stid]))

# 输出全量顶层表（供 CSV 点名用）
with open(pathlib.Path(__file__).with_name("top_table.txt"), "w", encoding="utf-8") as f:
    for e in checks:
        steps = e.get("steps") or []
        f.write("%s\tsteps=%d\tplatform=%s\tprofiles=%s\twaivable=%s\theavy=%s\tmonitor=%s\tinputs=%d\toutputs=%d\ttimeout=%s\tcmd=%s\n" % (
            e["id"], len(steps), e.get("platform", "-"), ",".join(e.get("profiles") or []),
            e.get("waivable"), e.get("heavy"), e.get("requires_monitor"),
            len(e.get("inputs") or []), len(e.get("outputs") or []), e.get("timeout_seconds"),
            " ".join(map(str, (e.get("command") or [""])[0:3]))))
print("\nwrote top_table.txt")
