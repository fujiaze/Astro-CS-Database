import sys, os, csv, io
sys.stdout.reconfigure(encoding='utf-8')
REPO = r"F:/Astro dev/Astro CS Normalization Database"
# usage: ctx.py file:line[:before,after] ...
for spec in sys.argv[1:]:
    f, loc = spec.rsplit(":", 1)
    ln = int(loc)
    path = os.path.join(REPO, f.replace("/", os.sep))
    lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    lo, hi = max(1, ln - 6), min(len(lines), ln + 6)
    print("=" * 100)
    print(f"### {f}:{ln}  (file has {len(lines)} lines)")
    for i in range(lo, hi + 1):
        mark = ">>" if i == ln else "  "
        print(f"{mark}{i:6d}| {lines[i-1]}")
