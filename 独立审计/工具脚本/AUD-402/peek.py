import sys
# usage: peek.py FILE:START-END [FILE:START-END ...]  -> writes raw/scan402/peek_aud402.txt
OUT = r"独立审计/证据/scan402\peek_aud402.txt"
buf = []
for a in sys.argv[1:]:
    f, rng = a.rsplit(":", 1)
    lo, hi = (int(x) for x in rng.split("-"))
    buf.append("=== " + a + " ===")
    for i, line in enumerate(open(f, encoding="utf-8", errors="replace"), 1):
        if lo <= i <= hi:
            buf.append(str(i) + ": " + line.rstrip()[:300])
open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(buf) + "\n")
print("wrote", OUT, len(buf), "lines")
