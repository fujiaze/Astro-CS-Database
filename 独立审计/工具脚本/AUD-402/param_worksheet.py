import re

# Author-worksheet: production named constants with a *fractional or exponent*
# value (i.e. parameter-like), grouped by module, with the declaration comment.
SRC = r"独立审计/证据/scan402\named.tsv"
OUT = r"独立审计/证据/scan402\aud402_param_like.tsv"
DROP = ("/tests/", "/test/", "/tools/", "memory.md", "README", "test_", "_test", "oracle",
        "bench", "probe", "fixture", "/examples/", "/integration/")
FLOATISH = re.compile(r"^[-+]?\d[\d_]*\.\d|^[-+]?0?\.\d|^[-+]?\d[\d_]*[eE][-+]?\d|^[-+]?\d[\d_]*\.\d+[eE]")
rows = []
for line in open(SRC, encoding="utf-8").read().split("\n")[1:]:
    p = line.split("\t")
    if len(p) < 6:
        continue
    f, ln, cls, name, assigned, text = p[:6]
    if not f.startswith("lib/") or any(d in f for d in DROP):
        continue
    a = assigned.replace(" ", "")
    if not FLOATISH.match(a):
        continue
    rows.append((f, ln, name, a, text))
with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
    fh.write("file\tline\tname\tvalue\ttext\n")
    for r in rows:
        fh.write("\t".join(r) + "\n")
print("parameter-like production named constants:", len(rows))
import collections
c = collections.Counter(r[0].split("/")[2] if r[0].startswith("lib/algorithms") or
                        r[0].startswith("lib/infrastructure") else r[0].split("/")[1] for r in rows)
for k, v in c.most_common(30):
    print(f"  {k:24s} {v}")
