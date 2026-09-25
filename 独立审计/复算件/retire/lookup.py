# -*- coding: utf-8 -*-
"""AUD-404 复算 3：对象级四档定位 + 四面引用计数。

用法：python lookup.py <pattern> [<pattern> ...]
 - bucket 列取自 AUD-401 机器表 source_prod_membership_v2.tsv（PROD/TEST/ALONE/ORPHAN），
   本脚本另用 git grep 独立复核代码面命中，不直接采信该表。
四面：生产(lib/eng 非测试路径) / 测试(tests 路径) / 文档(docs/** + 根 *.md + lib/**.md) /
      登记面(eng/ci, eng/contracts, eng/tools/quality, */module.yaml, module_ports.registry.json,
             docs/architecture/api_inventory.csv, docs/contracts/API_CONTRACTS.csv, docs/TRACEABILITY.csv)
"""
import csv
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")
REPO = r"F:\Astro dev\Astro CS Normalization Database"
TSV = (r"独立审计/批次清单/AUD-401"
       r"\source_prod_membership_v2.tsv")

MEMB = {}
with open(TSV, encoding="utf-8") as f:
    for row in csv.DictReader(f, delimiter="\t"):
        MEMB[row["path"]] = (row["bucket"], row["targets"])

TEST_RE = re.compile(r"(^|/)tests?/|_test\.|_selftest|/test_|testkit|fixtures/")
DOC_RE = re.compile(r"^docs/|\.md$|^README")
REG_RE = re.compile(r"^eng/ci/|^eng/contracts/|^eng/tools/quality/|"
                    r"module\.yaml$|module_ports\.registry\.json$|"
                    r"api_inventory\.csv$|API_CONTRACTS\.csv$|TRACEABILITY\.csv$|"
                    r"^eng/tools/")
CODE_RE = re.compile(r"^lib/|^eng/")


def hits(pattern, pathspec):
    r = subprocess.run(
        ["git", "-C", REPO, "-c", "core.quotepath=false", "grep", "-n", "-E",
         "--", pattern] + pathspec,
        capture_output=True, text=True, encoding="utf-8", errors="replace")
    return [l for l in (r.stdout or "").split("\n") if l]


def split(hs):
    prod = test = doc = reg = 0
    for h in hs:
        fp = h.split(":", 1)[0]
        if REG_RE.search(fp):
            reg += 1
        elif DOC_RE.search(fp):
            doc += 1
        elif TEST_RE.search(fp):
            test += 1
        elif CODE_RE.search(fp):
            prod += 1
    return prod, test, doc, reg


for pat in sys.argv[1:]:
    print("=" * 70)
    print("PATTERN:", pat)
    base = hits(r"\b%s\b" % re.escape(pat), ["--", "lib", "docs", "eng", "CMakeLists.txt"])
    print("git grep -n -E '\\b%s\\b' -- lib docs eng CMakeLists.txt  => %d 行" % (pat, len(base)))
    print("  四面计数 生产/测试/文档/登记 = %s" % (split(base),))
    for h in base[:14]:
        print("   ", h[:175])
    if len(base) > 14:
        print("    ... 另 %d 行" % (len(base) - 14))
    for p, (b, t) in MEMB.items():
        if pat in p:
            print("  membership: %-72s %s | %s" % (p[:72], b, t[:90]))
