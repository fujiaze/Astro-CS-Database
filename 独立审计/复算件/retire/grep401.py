import re
import sys

sys.stdout.reconfigure(encoding="utf-8")

path = r"独立审计/证据/AUD-401-架构对齐.md"
pat = sys.argv[1]
lo = int(sys.argv[2]) if len(sys.argv) > 2 else 1
hi = int(sys.argv[3]) if len(sys.argv) > 3 else 10 ** 9
lines = open(path, encoding="utf-8").read().split("\n")
rx = re.compile(pat)
for i, l in enumerate(lines, 1):
    if lo <= i <= hi and rx.search(l):
        print(f"{i}: {l}")
