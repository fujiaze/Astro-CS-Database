"""按"条目是否出现在成稿里"算补派缺口（不看 PROGRESS 头，它会被并发 bump 过头）。

用法：python -B scripts/gap_check.py <清单.txt> <成稿.md> [输出前缀]
清单每行一个仓库相对路径；缺口写入 <清单名>-missing.txt，并在 stdout 打印计数与样例。
"""

import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if len(sys.argv) < 3:
    print("FAIL-CLOSED: 需要 <清单> <成稿>")
    sys.exit(2)

lst, out_md = Path(sys.argv[1]), Path(sys.argv[2])
if not lst.is_file():
    print(f"FAIL-CLOSED: 清单不存在 {lst}")
    sys.exit(2)
if not out_md.is_file():
    print(f"FAIL-CLOSED: 成稿不存在 {out_md}（无成稿＝整批未开工，按全量缺口处理前先确认）")
    sys.exit(2)

items = [x.strip().replace("\\", "/") for x in lst.read_text(encoding="utf-8").splitlines() if x.strip()]
if not items:
    print("FAIL-CLOSED: 清单为空")
    sys.exit(2)
text = out_md.read_text(encoding="utf-8", errors="replace")
missing = [p for p in items if p not in text]

# 自证：拿一个应命中的项在同一条判定上跑通（防"字符串永远不匹配"式假缺口）
hit = [p for p in items if p in text]
if not hit:
    print("FAIL: 成稿里一个路径都没出现 ⇒ 判定式或成稿格式有问题，不当作'全部未读'")
    sys.exit(1)

tag = sys.argv[3] if len(sys.argv) > 3 else lst.stem
dst = lst.parent / f"{tag}-missing.txt"
dst.write_text("".join(p + "\n" for p in missing), encoding="utf-8")
print(f"{lst.name}: 分配 {len(items)}  成稿已出现 {len(hit)}  缺口 {len(missing)} → {dst.name}")
for p in missing[:8]:
    print("   缺:", p)
