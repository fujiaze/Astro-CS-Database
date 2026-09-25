"""把各分片成稿抽成一张底表，供跨片去重与实施任务书装配。

只读 raw/*.md；输出 CSV + 覆盖率自证。输入清单为空即非零退出（不判绿）。
"""

import csv, re, sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
ROOT = Path(__file__).resolve().parent.parent
RAW, INV = ROOT / "raw", ROOT / "inventory"
OUT = INV / "findings_base.csv"

REPO_PATH = re.compile(r"(?:docs|lib|eng|实验|工程控制|artifacts|testdata|reports)/[^\s`、，。;；:：)\]|*'\"<>]*")
NUM = re.compile(r"^\s*(?:[#|>\-*]\s*)?(?:\**([A-Z]{1,4}[-_]?(?:\d{1,4}|[A-Z]\d{1,3}))\**)\s*[（(]?([^\n]{0,40}?)\)?[\s]*[\|:：—\-]")
CONF = re.compile(r"CONFIRMED|PARTIAL|REFUTED|UNPROVEN|确认|降级|推翻|待证|成立|不成立|证据不足")
SEV = re.compile(r"\b(P0|P1|P2|S1|S2)\b")

files = sorted(p for p in RAW.glob("*.md") if not p.name.startswith("_"))
if not files:
    print("FAIL-CLOSED: raw/ 下没有成稿"); sys.exit(2)

rows = []
for f in files:
    src = f.name
    for ln, line in enumerate(f.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if len(line.strip()) < 24:
            continue
        paths = [p for p in REPO_PATH.findall(line) if len(p) > 4]
        if not paths:
            continue                       # 无被点名对象 ⇒ 不是可派工条目
        m = NUM.match(line)
        rows.append({
            "来源成稿": src, "行": ln,
            "条目": (m.group(1) if m else ""),
            "短标题": ((m.group(2) if m else "") or line.strip())[:60],
            "被点名对象": paths[0],
            "全部对象": " ".join(dict.fromkeys(paths))[:300],
            "对象数": len(set(paths)),
            "定级": (SEV.search(line).group(0) if SEV.search(line) else ""),
            "置信": (CONF.search(line).group(0) if CONF.search(line) else ""),
            "原文": line.strip()[:400],
        })

if not rows:
    print("FAIL-CLOSED: 未抽出任何带被点名对象的条目（抽取口径失效，不当作'无缺陷'）"); sys.exit(2)

with OUT.open("w", encoding="utf-8-sig", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
    w.writeheader(); w.writerows(rows)

per = {}
for r in rows:
    per[r["来源成稿"]] = per.get(r["来源成稿"], 0) + 1
print(f"成稿 {len(files)} 份 → 条目 {len(rows)} 行 → {OUT.name}")
for k, v in sorted(per.items(), key=lambda kv: -kv[1]):
    print(f"  {v:4d}  {k}")
zero = [k for k in (f.name for f in files) if k not in per]
print(f"零条目成稿 {len(zero)}: {', '.join(zero) if zero else '（无）'}")
obj = {}
for r in rows:
    obj[r["被点名对象"]] = obj.get(r["被点名对象"], 0) + 1
top = sorted(obj.items(), key=lambda kv: -kv[1])[:12]
print("被点名最多（跨片重复候选）：")
for o, n in top:
    print(f"  {n:3d}× {o}")
