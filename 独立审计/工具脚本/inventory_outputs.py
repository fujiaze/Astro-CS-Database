"""盘点已落盘成稿的完成度：文件存在性、行数、PROGRESS 头、四节/判定词是否齐。

只读审查工作区自身，不碰被审仓库。
"""

import re, sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
RAW = Path(__file__).resolve().parent.parent / "raw"
if not RAW.is_dir():
    print(f"FAIL-CLOSED: {RAW} 不存在"); sys.exit(2)

NEED = {
    "AUD-101": ["同主题组", "索引与链接缺陷", "覆盖率自报", "我证伪"],
    "复核": ["确认", "降级", "推翻", "待证"],
    "AUD-2": ["链路口径与公式", "证据清单", "三类数据", "诚实边界"],
    "AUD-4": ["覆盖率自报", "我证伪"],
}

rows = []
for p in sorted(RAW.glob("*.md")):
    if p.name.startswith("_"):
        continue
    t = p.read_text(encoding="utf-8", errors="replace")
    prog = re.findall(r"PROGRESS:\s*([0-9]+)/([0-9]+)", t)
    key = next((k for k in NEED if k in p.name), None)
    miss = [s for s in NEED[key] if s not in t] if key else []
    rows.append((p.name, len(t.splitlines()), prog[-1] if prog else ("", ""),
                 ("缺节:" + ",".join(miss)) if miss else ("节齐" if key else "-")))

print(f"{'成稿':40} {'行数':>6} {'进度':>8}  节完整性")
for n, ln, pr, sec in rows:
    print(f"{n:40} {ln:>6} {pr[0] + '/' + pr[1] if pr[0] else '-':>8}  {sec}")
thin = [n for n, ln, pr, sec in rows if ln < 25]
print(f"\n薄件（<25 行，可能只落了头部）：{', '.join(thin) if thin else '（无）'}")
print(f"合计 {len(rows)} 份成稿")
