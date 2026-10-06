#!/usr/bin/env python3
"""把 `collect_report.py` 的 JSON 报告翻译成一条**显式裁决行**，写进 CI job summary。

**为什么需要它**：工作流本身的绿/红**不能**用来回答「跑通了吗」。

- 工作流按 `standards/05_INDEPENDENT_TEST_SUITE.md:37`（非阻塞为常态）刻意不失败；
- 但 `docs/engineering/testing/TEST.md:136-138` 逐字把「没选中要跑的东西记成通过」
  命名为**假绿**。

⇒ 二者叠加会让人把「工作流绿」误读成「判据全过」。本脚本专门消除这个歧义：
它读报告，只输出一条**口径明确**的裁决，并附四项计数。

## 裁决口径（三档，逐字可核）

| 裁决 | 含义 | 条件 |
|---|---|---|
| `PASS_CONFORMANT` | **跑通且符合预期** | 每一步都是 `conformant`（全跑、全过、无跳过、无未收集） |
| `PASS_WITH_GAPS` | **不是绿灯**：有跳过或未覆盖 | 无红/未收集，但存在 `green_with_gaps`、`skipped` 或未覆盖路径 |
| `NOT_GREEN` | **不是绿灯**：有判红或有判据缺读数 | 存在 `fail` 或 `not_collected` |

`PASS_WITH_GAPS` 刻意**不叫 PASS**。它对应 `TEST.md:89` 逐字
「不以伪通过掩盖未执行」所防的那种形态。
"""

from __future__ import annotations

import json
import sys
from typing import Any, Dict, List

PASS_CONFORMANT = "PASS_CONFORMANT"
PASS_WITH_GAPS = "PASS_WITH_GAPS"
NOT_GREEN = "NOT_GREEN"

_LABELS = {
    PASS_CONFORMANT: "绿灯（跑通且符合预期）",
    PASS_WITH_GAPS: "**不是绿灯**（跳过也算绿的那一档）",
    NOT_GREEN: "**不是绿灯**（有判红，或有判据根本没读到读数）",
}


def judge(report: Dict[str, Any]) -> str:
    results: List[Dict[str, Any]] = report.get("results", [])
    if not results:
        return NOT_GREEN

    # 1) 有红，或有判据缺读数 —— 最硬的「不是绿灯」。
    if any(r["status"] in ("fail", "not_collected") for r in results):
        return NOT_GREEN
    # 2) 有跳过，或登记了未覆盖路径 —— 也不能叫绿灯。
    if any(r["green_class"] == "green_with_gaps" or r["status"] == "skipped"
           for r in results):
        return PASS_WITH_GAPS
    if report.get("uncovered_paths"):
        return PASS_WITH_GAPS
    # 3) 阻塞清单为空时不算缺口 ——「无阻塞项」≠「无缺口」，但也不是红。
    if all(r["green_class"] == "conformant" for r in results):
        return PASS_CONFORMANT
    return PASS_WITH_GAPS


def render(report: Dict[str, Any]) -> str:
    c = report.get("counts", {})
    verdict = judge(report)
    out: List[str] = []
    out.append("## 持续集成裁决")
    out.append("")
    out.append(f"**ACSD-CI-VERDICT: `{verdict}`** —— {_LABELS[verdict]}")
    out.append("")
    out.append("| 计数 | 值 |")
    out.append("|---|---|")
    out.append(f"| 总步数（分母） | {report.get('total', 0)} |")
    out.append(f"| 跑通且符合预期 | {c.get('pass', 0)} |")
    out.append(f"| 判红 fail | {c.get('fail', 0)} |")
    out.append(f"| 跳过 skipped | {c.get('skipped', 0)} |")
    out.append(f"| 未收集 not_collected | {c.get('not_collected', 0)} |")
    out.append(f"| 未覆盖路径 | {len(report.get('uncovered_paths', []))} |")
    out.append(f"| 阻塞清单项数 | {sum(1 for r in report.get('results', []) if r.get('blocking'))} |")
    out.append("")
    out.append("> 分母是**总步数**。跳过与未收集**不进分子**——"
               "这正是 `docs/engineering/testing/TEST.md:136-138` 逐字"
               "「『没选中要跑的东西』记成通过是**假绿**」的执行面。")
    out.append("")

    red = [r for r in report.get("results", []) if r["status"] in ("fail", "not_collected")]
    if red:
        out.append("### 需要人看的红项与缺口")
        out.append("")
        out.append("| 步骤 | 状态 | 说明 |")
        out.append("|---|---|---|")
        for r in red:
            out.append(f"| {r['name']} | `{r['status']}` | {r['detail']} |")
        out.append("")

    unc = report.get("uncovered_paths", [])
    if unc:
        out.append("### 未覆盖路径（`TEST.md:231-233`：不得按『已执行』计入绿）")
        out.append("")
        for u in unc:
            out.append(f"- {u}")
        out.append("")

    out.append("> 本报告不裁决合入"
               "（`docs/engineering/testing/TEST.md:203` 逐字「测试不裁决合入，"
               "合入由提交纪律决定」）。")
    return "\n".join(out) + "\n"


def main(argv: List[str]) -> int:
    if len(argv) < 2:
        print("用法：summarize_verdict.py <report.json>", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8") as fh:
        report = json.load(fh)
    sys.stdout.write(render(report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
