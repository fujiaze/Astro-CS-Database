import collections
import csv
import re
import sys

# Frequency of floating-point literals in the production code surface, so the
# judgment families can be ordered by blast radius (sites affected).
SRC = r"独立审计/证据/scan402\cpp_hits.tsv"
DROP = ("/tests/", "/test/", "/tools/", "memory.md", "README", ".md", "test_", "_test",
        "oracle", "bench", "probe", "fixture")
cnt = collections.Counter()
per = collections.defaultdict(set)
lines = 0
for rec in csv.DictReader(open(SRC, encoding="utf-8"), delimiter="\t"):
    f = rec["file"]
    if not f.startswith("lib/"):
        continue
    if any(d in f for d in DROP):
        continue
    lines += 1
    for t in re.findall(r"[-+]?\d*\.\d+(?:[eE][-+]?\d+)?f?", rec["text"]):
        t = t.rstrip("fF")
        cnt[t] += 1
        per[t].add(f)
print("production hit-lines scanned:", lines, "| distinct float tokens:", len(cnt))
out = open(r"独立审计/证据/scan402\aud402_float_freq.tsv",
           "w", encoding="utf-8", newline="\n")
out.write("token\tsites\tfiles\n")
for t, c in cnt.most_common():
    out.write(f"{t}\t{c}\t{len(per[t])}\n")
out.close()
for t, c in cnt.most_common(70):
    print(f"{t:>14} sites={c:<5} files={len(per[t])}")
