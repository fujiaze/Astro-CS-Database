"""把 AUD-402 的机械穷举重排成**可判读队列**：配置面全量、具名常数按语义筛、阈值容差单列。

无名行内字面量不逐条判读（那会淹掉真信号），只按簇出统计与抽样计划。
"""

import csv, re, sys, collections
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
OUT = Path(__file__).resolve().parent.parent
INV, RAW = OUT / "inventory", OUT / "raw"

NAMED = re.compile(r"(?i)(tol|epsilon|eps|thresh|window|max_|min_|limit|sigma|scale|factor|power|exponent|bunit|zp|snr|fwhm|gain|read_?noise|dark|sky|iter|converg|clip|percent|quant|dex|margin|guard|cap|threshold)")
CONFIGISH = re.compile(r"^(eng/packaging|eng/contracts|lib/infrastructure/cli|.*defaults\.json|.*filters\.json|.*schema)")

src = RAW / "AUD-402-常数台账.csv"
with src.open(encoding="utf-8-sig", newline="") as fh:
    rows = list(csv.DictReader(fh))
cols = list(rows[0].keys())
if not rows:
    print("FAIL-CLOSED: 输入为空"); sys.exit(2)

def pos(r):
    return (r.get("位置(路径:行)") or r.get("位置") or "").replace("\\", "/")

print("样例 3 行位置字段:", [pos(r) for r in rows[:3]])

buckets = collections.defaultdict(list)
for r in rows:
    p = pos(r)
    sym = r.get("符号/键") or ""
    if CONFIGISH.search(p) or p.endswith(".json") or p.endswith(".yaml"):
        buckets["A-配置与合同面"].append(r)
    elif sym and not re.fullmatch(r"[-+0-9.eEx_]+", sym):
        (buckets["B-具名常数(语义筛)"] if NAMED.search(sym) else buckets["C-具名常数(其余)"]).append(r)
    else:
        buckets["D-无名行内字面量"].append(r)

for name in sorted(buckets):
    rs = buckets[name]
    cap = {"A-配置与合同面": len(rs), "B-具名常数(语义筛)": len(rs),
           "C-具名常数(其余)": 0, "D-无名行内字面量": 0}[name]
    tag = name.split("-")[0]
    if tag in ("A", "B"):
        # 按文件切批，每批 ≤30 行
        rs.sort(key=lambda r: pos(r))
        n = max(1, (len(rs) + 29) // 30)
        for i in range(n):
            chunk = rs[i * 30:(i + 1) * 30]
            out = INV / f"P402-{tag}-{i + 1:02d}.csv"
            with out.open("w", encoding="utf-8", newline="") as fh:
                w = csv.DictWriter(fh, fieldnames=cols)
                w.writeheader(); w.writerows(chunk)
            print(f"{out.name}: {len(chunk)} 行")
    else:
        stat = collections.Counter(pos(r).rsplit("/", 1)[0] for r in rs)
        out = INV / f"P402-{tag}-统计.md"
        lines = [f"# {name} 统计（不逐条判读）", "", f"总数 {len(rs)}。按目录前 30：", ""]
        lines += [f"- `{d or '(根)'}` {c}" for d, c in stat.most_common(30)]
        lines += ["", "## 抽样计划", "",
                  f"按目录分层抽 {min(60, max(20, len(rs) // 200))} 行人工判读（每目录至多 4 行），"
                  "用途仅限：估计该簇中'真科学量'占比与给出反例；不外推为'全量已判读'。", ""]
        out.write_text("\n".join(lines), encoding="utf-8")
        print(f"{out.name}: {len(rs)} 行（只出统计）")

tot = sum(len(v) for v in buckets.values())
assert tot == len(rows), f"分桶不闭合 {tot} vs {len(rows)}"
print(f"闭合自证：{tot} = 总行 {len(rows)}，四桶 {dict((k, len(v)) for k, v in buckets.items())}")
