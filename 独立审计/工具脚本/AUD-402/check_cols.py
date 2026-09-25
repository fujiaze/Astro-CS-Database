import glob
import sys

# report column counts per line of every authored batch
for p in sorted(glob.glob(r"独立审计/工具脚本/AUD-402\rows_b*.tsv")):
    bad = 0
    for i, line in enumerate(open(p, encoding="utf-8"), 1):
        line = line.rstrip("\n")
        if not line.strip() or line.startswith("#"):
            continue
        n = len(line.split("\t"))
        if n != 12:
            bad += 1
            print(p.split("\\")[-1], "line", i, "cols", n, "|", line[:60])
    if not bad:
        print(p.split("\\")[-1], "OK")
