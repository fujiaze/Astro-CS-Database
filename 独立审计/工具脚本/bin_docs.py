"""把 DA-03..06 按（份数≤25 且 行数≤3500）确定性装箱成精读批次，并自证不相交、并集覆盖。"""

import sys
from pathlib import Path

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

REPO = Path(sys.argv[1]).resolve()
INV = Path(sys.argv[2]).resolve()
MAX_FILES, MAX_LINES = 25, 3500

src = [f"DA-{n}.txt" for n in ("03", "04", "05", "06")]
items = []
for s in src:
    p = INV / s
    if not p.is_file():
        print(f"FAIL-CLOSED: 缺清单 {p}")
        sys.exit(2)
    for line in p.read_text(encoding="utf-8").splitlines():
        f = line.strip().replace("\\", "/")
        if f:
            items.append((s[:5], f))
if not items:
    print("FAIL-CLOSED: 输入为空")
    sys.exit(2)

sized = []
for batch, f in items:
    fp = REPO / f
    if not fp.is_file():
        print(f"FAIL-CLOSED: {f} 不在树里")
        sys.exit(2)
    sized.append((batch, f, sum(1 for _ in fp.open(encoding="utf-8", errors="replace"))))

bins, cur, curl = [], [], 0
for it in sized:
    if cur and (len(cur) >= MAX_FILES or curl + it[2] > MAX_LINES):
        bins.append(cur)
        cur, curl = [], 0
    cur.append(it)
    curl += it[2]
if cur:
    bins.append(cur)

assigned = [it[1] for b in bins for it in b]
if len(assigned) != len(set(assigned)) or set(assigned) != {i[1] for i in sized}:
    print("FAIL: 装箱不闭合（相交或漏项）")
    sys.exit(1)

old = list(INV.glob("DB-*.txt"))
for o in old:
    o.unlink()
for i, b in enumerate(bins, 1):
    out = INV / f"DB-{i:02d}.txt"
    out.write_text("".join(f"{it[1]}\n" for it in b), encoding="utf-8")
    print(f"DB-{i:02d}  {len(b):2d} 份  {sum(x[2] for x in b):5d} 行  源批次 {sorted({x[0] for x in b})}  首行: {b[0][1]}")
print(f"合计 {len(bins)} 批 / {len(assigned)} 份 / {sum(x[2] for b in bins for x in b)} 行（自证：不相交且并集=全集）")
