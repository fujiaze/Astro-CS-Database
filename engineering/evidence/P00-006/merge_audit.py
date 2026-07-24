#!/usr/bin/env python3
"""Merge P0P1/P2/P3 audit reconciliation JSONs into unified audit_reconciliation.json and .md"""
import json
from pathlib import Path
from collections import defaultdict

evidence_dir = Path(__file__).parent
groups = ["P0P1", "P2", "P3"]
all_items = []
summary = {"OPEN": 0, "CLOSED": 0, "STALE": 0, "UNVERIFIED": 0, "REJECTED": 0}
module_stats = defaultdict(lambda: {"OPEN": 0, "CLOSED": 0, "STALE": 0, "UNVERIFIED": 0, "REJECTED": 0, "total": 0})

for g in groups:
    data = json.loads((evidence_dir / f"audit_reconciliation_{g}.json").read_text(encoding="utf-8"))
    for item in data["items"]:
        all_items.append(item)
        s = item["status"]
        summary[s] = summary.get(s, 0) + 1
        m = item.get("module", "unknown")
        module_stats[m][s] = module_stats[m].get(s, 0) + 1
        module_stats[m]["total"] += 1

unified = {
    "project": "AstroCS",
    "task_id": "P00-006",
    "generated_at": "2026-07-24",
    "source_audit": "docs/superpowers/specs/2026-07-18-code-audit-report.md (2026-07-18, 9 子代理扫描)",
    "total_items": len(all_items),
    "summary": summary,
    "module_stats": dict(module_stats),
    "items": all_items,
}

(evidence_dir / "audit_reconciliation.json").write_text(
    json.dumps(unified, ensure_ascii=False, indent=2), encoding="utf-8")

# Markdown
lines = [
    "# AstroCS 旧审计 163 项复核报告",
    "",
    "- **Task ID**: P00-006",
    "- **生成时间**: 2026-07-24",
    "- **审计来源**: docs/superpowers/specs/2026-07-18-code-audit-report.md (2026-07-18, 9 子代理扫描)",
    f"- **复核总数**: {len(all_items)}",
    "",
    "## 总体统计",
    "",
    "| 状态 | 数量 | 占比 | 含义 |",
    "|---|---|---|---|",
    f"| OPEN | {summary['OPEN']} | {summary['OPEN']*100//len(all_items)}% | 当前源码仍存在 |",
    f"| CLOSED | {summary['CLOSED']} | {summary['CLOSED']*100//len(all_items)}% | 已有代码证据已解决 |",
    f"| STALE | {summary['STALE']} | {summary['STALE']*100//len(all_items)}% | 路径/架构已变化 |",
    f"| UNVERIFIED | {summary['UNVERIFIED']} | {summary['UNVERIFIED']*100//len(all_items)}% | 无法验证 |",
    f"| REJECTED | {summary['REJECTED']} | {summary['REJECTED']*100//len(all_items)}% | 硬约束无有效来源 |",
    f"| **合计** | **{len(all_items)}** | **100%** | |",
    "",
    "## 按优先级分布",
    "",
    "| 优先级 | 总数 | OPEN | CLOSED | STALE | UNVERIFIED | REJECTED |",
    "|---|---|---|---|---|---|---|",
]
for g in groups:
    data = json.loads((evidence_dir / f"audit_reconciliation_{g}.json").read_text(encoding="utf-8"))
    s = data["summary"]
    lines.append(f"| {g} | {data['total']} | {s.get('OPEN',0)} | {s.get('CLOSED',0)} | {s.get('STALE',0)} | {s.get('UNVERIFIED',0)} | {s.get('REJECTED',0)} |")

lines += ["", "## 按模块分布", "", "| 模块 | 总数 | OPEN | CLOSED | STALE | UNVERIFIED | REJECTED |", "|---|---|---|---|---|---|---|"]
for m in sorted(module_stats.keys()):
    s = module_stats[m]
    lines.append(f"| {m} | {s['total']} | {s['OPEN']} | {s['CLOSED']} | {s['STALE']} | {s['UNVERIFIED']} | {s['REJECTED']} |")

lines += ["", "## OPEN 项按优先级（P01+ 修复输入）", ""]
for prio in ["Critical", "High", "Medium", "Low"]:
    prio_items = [i for i in all_items if i.get("severity") == prio and i["status"] == "OPEN"]
    if prio_items:
        lines.append(f"### {prio} ({len(prio_items)} 项 OPEN)")
        lines.append("")
        lines.append("| ID | 模块 | 标题 | 证据 |")
        lines.append("|---|---|---|---|")
        for item in prio_items:
            lines.append(f"| {item['id']} | {item.get('module','')} | {item['title'][:40]} | {item.get('evidence','')[:60]} |")
        lines.append("")

lines += ["## CLOSED 项（已解决）", ""]
lines.append("| ID | 模块 | 标题 | 证据 |")
lines.append("|---|---|---|---|")
for item in all_items:
    if item["status"] == "CLOSED":
        lines.append(f"| {item['id']} | {item.get('module','')} | {item['title'][:40]} | {item.get('evidence','')[:60]} |")

lines += ["", "## REJECTED 项", ""]
for item in all_items:
    if item["status"] == "REJECTED":
        lines.append(f"- **{item['id']}** ({item.get('module','')}): {item['title']} — {item.get('evidence','')}")

lines += ["", "## 复核方法", "",
    "1. 读取 2026-07-18 代码审计文档（P0P1/P2/P3 三个 findings 文件）",
    "2. 对每项提取文件定位与问题描述",
    "3. 使用 Read/Grep 工具读取当前源码对应位置",
    "4. 对照问题描述判断当前状态（OPEN/CLOSED/STALE/UNVERIFIED/REJECTED）",
    "5. 记录证据（文件:行号 或说明）",
    "",
    "## 详细复核清单",
    "- P0P1 (50 项): audit_reconciliation_P0P1.json / .md",
    "- P2 (54 项): audit_reconciliation_P2.json / .md",
    "- P3 (59 项): audit_reconciliation_P3.json / .md",
]

(evidence_dir / "audit_reconciliation.md").write_text("\n".join(lines), encoding="utf-8")
print(f"OK: {len(all_items)} items unified")
print(f"  OPEN={summary['OPEN']} CLOSED={summary['CLOSED']} STALE={summary['STALE']} UNVERIFIED={summary['UNVERIFIED']} REJECTED={summary['REJECTED']}")
