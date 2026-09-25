"""AUD-502 只读普查 2：入口唯一化与档位设计所需的分档面。口径全部写死并在输出里标注。"""
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


def units(entry=None):
    out = []
    for e in (checks if entry is None else [entry]):
        steps = e.get("steps")
        if not steps:
            out.append((e["id"], e["id"], e, False))
            continue
        for s in steps:
            m = {f: (s[f] if f in s else e.get(f)) for f in INHERIT}
            out.append((e["id"], s.get("id", "?"), m, True))
    return out


U = units()


def kind(m):
    j = " ".join(str(x) for x in (m.get("command") or []))
    if "run_checks.py" in j:
        return "A_run_checks派发"
    if "unittest" in j:
        return "B_unittest_discover"
    if "deep_ci_driver.py ctest-target" in j:
        return "C_ctest单目标"
    if "ctest" in j:
        return "D_ctest其它"
    if "resource_monitor.py" in j:
        return "E_monitor包装"
    if "wf_step.py" in j:
        return "F_wf_step"
    if re.search(r"cmake|ninja", j):
        return "G_构建命令"
    if re.search(r"eng/\S+\.py", j):
        return "H_单脚本直跑"
    return "I_其它"


def skipped(m, host):
    p = m.get("platform", "any")
    return p != "any" and p != host


for host in ("windows", "linux"):
    pu, ps = Counter(), Counter()
    for _, _, m, _ in U:
        for pr in (m.get("profiles") or []):
            pu[pr] += 1
            if skipped(m, host):
                ps[pr] += 1
    print("host=%s 档级：选中 / 被 platform 跳过" % host)
    for pr in ("fast", "integration", "linux-main", "windows-main",
               "linux-deep", "prerelease", "fatduck"):
        print("   %-13s %3d / %3d" % (pr, pu[pr], ps[pr]))
    zero = sorted(e["id"] for e in checks
                  if units(e) and all(skipped(m, host) for _, _, m, _ in units(e)))
    print("  %s 上「整项全跳」注册项 = %d / %d" % (host, len(zero), len(checks)))
    if host == "windows":
        print("  清单:", " ".join(zero))
    print()

print("--- 命令面形态 × 证据/输入/豁免面 ---")
tot = Counter()
for _, _, m, _ in U:
    k = kind(m)
    tot[k] += 1
    print("  %-18s %-28s out=%d in=%d waiv=%s heavy=%s mon=%s plat=%s prof=%s" % (
        k, _, bool(m.get("outputs")), bool(m.get("inputs")), bool(m.get("waivable")),
        bool(m.get("heavy")), bool(m.get("requires_monitor")),
        m.get("platform", "any"), ",".join(m.get("profiles") or []))) if tot[k] == 1 else None
print()
agg = {}
for top, uid, m, _ in U:
    k = kind(m)
    a = agg.setdefault(k, [0, 0, 0, 0, 0])
    a[0] += 1
    a[1] += 1 if m.get("outputs") else 0
    a[2] += 1 if m.get("inputs") else 0
    a[3] += 1 if m.get("waivable") else 0
    a[4] += 1 if m.get("heavy") else 0
for k in sorted(agg):
    n, o, i, w, h = agg[k]
    print("%-18s n=%3d outputs=%3d inputs=%2d waivable=%2d heavy=%2d" % (k, n, o, i, w, h))

print("\n--- 档位登记 timeout 分布（合并继承后的单元面）---")
for pr in ("fast", "integration", "linux-main", "windows-main", "linux-deep",
           "prerelease", "fatduck"):
    ts = sorted(int(m.get("timeout_seconds") or 0) for _, _, m, _ in U
                if pr in (m.get("profiles") or []))
    if not ts:
        continue
    print("档 %-13s n=%3d 合计=%7ds 最大=%5d >120s=%3d >300s=%3d 中位=%5d" % (
        pr, len(ts), sum(ts), ts[-1], sum(1 for t in ts if t > 120),
        sum(1 for t in ts if t > 300), ts[len(ts) // 2]))

# 每单元是否有任何「可执行负例面」：命令含 self-test/fault-inject，或同项另有 NEG/SELFTEST 单元
selftest_ids = set()
for top, uid, m, _ in U:
    j = " ".join(str(x) for x in (m.get("command") or []))
    if re.search(r"--self[-_]?test|--fault-inject|FAULT|SELFTEST|-NEG|NEGATIVE", j, re.I) \
            or re.search(r"SELFTEST|-NEG|NEGATIVE", uid, re.I) or re.search(r"SELFTEST|-NEG|NEGATIVE", top, re.I):
        selftest_ids.add(top)
tops = {e["id"] for e in checks}
print("\n顶层项中含任一可执行负例面的 = %d / %d；完全无负例面的项 = %d" % (
    len(selftest_ids & tops), len(tops), len(tops - selftest_ids)))
print("完全无负例面清单:", " ".join(sorted(tops - selftest_ids)))
