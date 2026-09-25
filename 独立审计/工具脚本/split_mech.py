"""把机械列 CSV 按批次切片，供各文档审计代理当线索用；并对未分派残余做归属判定。

只读被审仓库；输出全部落审查工作区。
"""

import csv
import sys
from collections import Counter
from pathlib import Path

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

ROOT = Path(__file__).resolve().parent.parent          # 产出/
INV = ROOT / "inventory"
MECH = INV / "doc_mechanical.csv"

if not MECH.is_file():
    print(f"FAIL-CLOSED: 缺 {MECH}")
    sys.exit(2)

rows = list(csv.DictReader(MECH.open(encoding="utf-8", newline="")))
if not rows:
    print("FAIL-CLOSED: 机械台账为空")
    sys.exit(2)
cols = rows[0].keys()

by_path = {r["path"]: r for r in rows}
if len(by_path) != len(rows):
    print(f"FAIL-CLOSED: 机械台账有重复路径 {len(rows)} 行 / {len(by_path)} 唯一")
    sys.exit(2)

made = []
for lp in sorted(INV.glob("DA-0*.txt")):
    if lp.name == "DA-all.txt":
        continue
    batch = lp.stem                                        # DA-01
    paths = [x.strip().replace("\\", "/") for x in lp.read_text(encoding="utf-8").splitlines() if x.strip()]
    hit = [by_path[p] for p in paths if p in by_path]
    miss = [p for p in paths if p not in by_path]
    if miss:
        print(f"WARN {batch}: {len(miss)} 项不在机械台账，例 {miss[:3]}")
    out = INV / f"mech-{batch}.csv"
    with out.open("w", encoding="utf-8", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(cols))
        w.writeheader()
        w.writerows(hit)
    flagged = sum(1 for r in hit if r["dangling_links"] or r["dates"] or r["task_ids"]
                  or r["commit_sha"] or r["history_narrative"] or r["unregistered_index"] == "True")
    made.append(f"{batch}: {len(hit)} 份 → {out.name}，其中 {flagged} 份有机械线索")
print("\n".join(made))

# ---- 残余归属 ----
res = INV / "_residual.txt"
if res.is_file():
    items = [x.strip() for x in res.read_text(encoding="utf-8").splitlines() if x.strip()]
    def owner(p):
        top = p.split("/")[0]
        if top in ("lib", "eng"):
            return "AUD-403", "库与工具的模块 README / 注释面，属代码梳理标准 §6"
        if top == "artifacts":
            return "本轮非目标", "证据与档案层（标准01 §2：台账落 artifacts/evidence 并由正本引用），不属正式文档层"
        if top == "testdata":
            return "AUD-401", "数据集索引与只读外部数据说明，随架构面一起核其登记一致性"
        return "待指派", "未匹配任何已定分片"
    out = INV / "residual_triage.csv"
    with out.open("w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["path", "归属", "理由"])
        w.writerows([(p, *owner(p)) for p in items])
    c = Counter(owner(p)[0] for p in items)
    print(f"残余 {len(items)} 项 → {out.name}: " + " / ".join(f"{k} {v}" for k, v in c.most_common()))
    unc = [k for k in c if k == "待指派"]
    if unc:
        print(f"FAIL: {c['待指派']} 项无归属，须显式登记为本轮非目标或补分片")
        sys.exit(1)
else:
    print(f"WARN: 无 {res.name}，跳过残余归属")
