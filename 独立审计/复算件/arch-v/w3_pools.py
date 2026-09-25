"""AUDIT-06 arch-v W3: private thread-pool census + `#include "*.cpp"` text-include census.

Pools are classified by whether the declaring TU is compiled into a production target.
That membership decision is made ONLY from CMake source lists read as text (see
w3_targets.py); nothing here builds or configures anything.
"""
import io
import os
import re
import subprocess

REPO = r"F:\Astro dev\Astro CS Normalization Database"
POOL_RE = re.compile(r"std::vector<std::thread>\s+([A-Za-z_]\w*)")
TXT_INC_RE = re.compile(r'^\s*#\s*include\s*"([^"]+\.(?:cpp|cc|inc|h))"', re.M)


def git(*args):
    return subprocess.run(["git"] + list(args), cwd=REPO, capture_output=True,
                          text=True, encoding="utf-8", errors="replace").stdout


def is_test(p):
    return p.startswith("eng/tests/") or "/tests/" in p or p.startswith("eng/tools/")


# ---- build "compiled into production target" set from CMake text ----
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from w3_targets import production_source_set  # noqa: E402

PROD = production_source_set(REPO)

print("=== POOL SITES: std::vector<std::thread> <name> declarations in lib/** and app/** ===")
rows = []
for line in git("grep", "-n", "std::vector<std::thread>").splitlines():
    m = re.match(r"^(.+?):(\d+):(.*)$", line)
    if not m:
        continue
    f, ln, body = m.group(1), int(m.group(2)), m.group(3)
    if not (f.startswith("lib/") or f.startswith("app/")):
        continue
    dm = POOL_RE.search(body)
    kind = "DECL" if dm else "comment/other"
    if f.endswith(".md"):
        continue
    rows.append((f, ln, kind, dm.group(1) if dm else "-", body.strip()[:78]))

for f, ln, kind, name, body in sorted(rows):
    if kind != "DECL":
        continue
    tag = "PROD-COMPILED" if f in PROD else ("test" if is_test(f) else "NOT-in-any-cmake-src-list")
    algo = "算法模块" if f.startswith("lib/algorithms/") else "基建"
    print("  %-26s %-8s %-16s %s:%d  %s   [%s]" % (algo, name, tag, f, ln, body[:40], kind))

decl = [r for r in rows if r[2] == "DECL"]
print("\n  TOTAL DECL in lib/** = %d" % len(decl))
from collections import Counter
c = Counter()
for f, ln, _k, n, _b in decl:
    inprod = f in PROD
    dom = "算法模块" if f.startswith("lib/algorithms/") else ("基建" if f.startswith("lib/infrastructure/") else "其他lib")
    c[(dom, "生产编译" if inprod else ("测试" if is_test(f) else "未编译"))] += 1
for k, v in sorted(c.items()):
    print("   ", k, v)
print("    合计生产编译 =", sum(v for k, v in c.items() if k[1] == "生产编译"))
print("    其中算法模块内 =", sum(v for k, v in c.items() if k[1] == "生产编译" and k[0] == "算法模块"))
print("    其中基建内     =", sum(v for k, v in c.items() if k[1] == "生产编译" and k[0] == "基建"))

print("\n=== `#include \"*.cpp\"` / \"*.inc\" TEXT INCLUDES, whole tracked repo ===")
for line in git("grep", "-nE", r'#\s*include\s*"[^"]+\.(cpp|cc|inc)"', "--", "lib", "eng", "app").splitlines():
    m = re.match(r"^(.+?):(\d+):(.*)$", line)
    if not m:
        continue
    f, ln, body = m.group(1), m.group(2), m.group(3).strip()
    tinc = TXT_INC_RE.search(" " + body + "\n") or TXT_INC_RE.search(body + "\n")
    inc = tinc.group(1) if tinc else "?"
    print("  %-60s %-5s -> %-34s  %s" % (f, ln, inc, "PROD-COMPILED host" if f in PROD else ("test host" if is_test(f) else "not-compiled host")))
