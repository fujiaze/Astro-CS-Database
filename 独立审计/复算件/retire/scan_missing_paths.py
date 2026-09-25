# -*- coding: utf-8 -*-
"""AUD-404 复算 4：登记面/合同/注释中的路径名存活核对（未跟踪产物名）。

从登记面与 lib/ 退役注释里抽 path-like token，判其是否在 git 跟踪集中。
run/ 是 gitignore 的产物根 => 命中即"未跟踪产物名"。
"""
import collections
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"

tracked = set(subprocess.run(
    ["git", "-C", REPO, "-c", "core.quotepath=false", "ls-files"],
    capture_output=True, text=True, encoding="utf-8", errors="replace"
).stdout.split("\n"))
tracked.discard("")

PREFIX = r"(?:lib|docs|eng|run|artifacts|testdata|实验|reports|gaia)"
PATH_RE = re.compile(
    r"(?<![\w./-])(" + PREFIX + r"/[\w./\*\-一-鿿,]+)")

SURFACES = [
    "eng/ci/prod_wiring_baseline.json",
    "eng/ci/retired_code_allowlist.json",
    "eng/ci/checks.json",
    "eng/ci/known_failures_baseline.json",
    "eng/ci/id_migration_map.json",
    "eng/ci/ledgers/registry_ir_parity.json",
    "eng/tools/doccheck/dangling_ledger.json",
    "docs/architecture/api_inventory.csv",
    "docs/contracts/API_CONTRACTS.csv",
    "docs/TRACEABILITY.csv",
]

surfaces = list(SURFACES)
for extra in ("eng/ci", "lib"):
    r = subprocess.run(["git", "-C", REPO, "ls-files", extra],
                       capture_output=True, text=True, encoding="utf-8",
                       errors="replace").stdout.split("\n")
    surfaces += [f for f in r if f.endswith((".py", ".yaml", ".json", ".h",
                                             ".cpp", ".c"))]

miss = collections.defaultdict(set)
for s in sorted(set(surfaces)):
    if s not in tracked:
        continue
    try:
        text = open(REPO + "/" + s, encoding="utf-8", errors="replace").read()
    except OSError:
        continue
    for tok in PATH_RE.findall(text):
        t = tok.rstrip(".,;:：)、）\"'`")
        if not t or t.endswith(("/.", "*")) or "//" in t:
            continue
        if t in tracked:
            continue
        # 目录前缀命中跟踪集 => 视为存在
        if any(x.startswith(t.rstrip("/") + "/") for x in tracked):
            continue
        miss[t].add(s)

print("缺失/未跟踪路径名种类:", len(miss))
byfam = collections.Counter()
for t in miss:
    byfam[t.split("/")[0]] += 1
print("按首段:", byfam.most_common())
print()
for t in sorted(miss, key=lambda x: (x.split("/")[0], x)):
    src = sorted(miss[t])
    print(f"MISSING {t}   <- {len(src)} 处")
    for s in src[:4]:
        print(f"      {s}")
    if len(src) > 4:
        print(f"      ... 另 {len(src)-4} 个来源")
