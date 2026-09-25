import io, os, re, sys

pat = re.compile(sys.argv[1], re.I)
roots = sys.argv[2].split(";")
maxhits = int(sys.argv[3]) if len(sys.argv) > 3 else 60
n = 0
for root in roots:
    for dp, dn, fn in os.walk(root):
        if "__pycache__" in dp:
            continue
        for f in fn:
            if not f.endswith((".py", ".md", ".sh", ".txt", ".tsv", ".csv")):
                continue
            p = os.path.join(dp, f)
            try:
                with io.open(p, encoding="utf-8", errors="replace") as fh:
                    for i, line in enumerate(fh, 1):
                        if pat.search(line):
                            print("%s:%d: %s" % (p.replace(os.getcwd(), "."), i, line.rstrip()[:220]))
                            n += 1
                            if n >= maxhits:
                                sys.exit(0)
            except Exception:
                pass
