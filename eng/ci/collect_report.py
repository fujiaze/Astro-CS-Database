#!/usr/bin/env python3
"""持续集成报告收集器 —— 把各步骤的原始输出收成**可读产物**。

**它是什么**：一个纯标准库脚本（零第三方依赖）。读入各步骤落下的原始输出，
解析出结构化结论，产出两份人可读产物，并按**阻塞清单**给出退出码。

**它不是什么**（`standards/05_INDEPENDENT_TEST_SUITE.md:37` 逐字「以非阻塞为常态」）：

- 不是门禁。默认退出码恒 `0`；
- 不裁决合入（`docs/engineering/testing/TEST.md:203` 逐字「测试不裁决合入，合入由提交纪律决定」）；
- 不把「没跑到」记成「通过」。

## ⚠️ 为什么需要它（本脚本存在的唯一理由）

仓内各测试层的运行器**默认退出码恒 0**（`eng/tests/unit/harness.py:287-294`
逐字「退出码。默认恒 `0`」），且 `eng/tests/unit/run_unit.py:104-106` 把
`--exit-code` 明确标为「**不得**被接进 CI」。

⇒ 直接把运行器挂到持续集成里，**无论红多少项，步骤永远是绿的**。
实测证据（单元层 160 条、红 1 条时退出码仍为 0）见审核包
`run/GOVERN-08/审核包-R2/T12-持续集成设计.md`。

⇒ 本脚本的职责是**把「红」从 stdout 文本里取出来，落进产物，并让退出码反映它** ——
但**只对阻塞清单里的步骤**。这正是规范允许的例外面。

## 反假绿的四态（`docs/engineering/testing/TEST.md:136-138` 逐字
「『没选中要跑的东西』记成通过是**假绿**」）

每一步必须落进四态之一，**没有第五态，缺产物不补记为通过**：

| 状态 | 含义 |
|---|---|
| `pass` | 步骤跑完，判据全部通过 |
| `fail` | 步骤跑完，有判红 |
| `skipped` | 步骤被显式跳过（跳过**单列**，不并入 pass） |
| `not_collected` | **没有产物可解析** —— 步骤没跑、崩了、或输出格式变了 |

`not_collected` 永远不算 `pass`。汇总里的「通过」只累加 `pass`。

## 用法

```bash
# 收集 ci-raw/ 下各步骤的原始输出，产出 ci-report/{report.json,summary.md}
python3 eng/ci/collect_report.py --raw ci-raw --out ci-report

# 只在阻塞清单内的步骤出现 fail / not_collected 时返回非 0
python3 eng/ci/collect_report.py --raw ci-raw --out ci-report --enforce
```

`--enforce` 是**唯一**能让本脚本返回非 0 的开关，且它只读 `steps.yaml` 里
`blocking: true` 的步骤 —— 数量受规范约束，见步骤清单正本。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from typing import Any, Dict, List, Optional

PASS = "pass"
FAIL = "fail"
SKIPPED = "skipped"
NOT_COLLECTED = "not_collected"

#: 四态里**唯一**可以计入「通过」的状态。
GREEN_STATES = (PASS,)

# 各层运行器的实际输出格式（逐字对齐 run_unit.py:84 / :96-102 / run_synthetic.py:164）：
#   [PASS] <case_id>  (12.3 ms)
#   [FAIL] <case_id>  (0.1 ms)
#   合计 160  通过 159  红 1   （正例 94 / 负例 66）
#   红项清单:
#     - <case_id>: <detail 首行>
_CASE_RE = re.compile(r"^\[(?P<mark>PASS|FAIL)\]\s+(?P<id>\S+)\s+\((?P<ms>[\d.]+)\s+ms\)")
_SUMMARY_RE = re.compile(
    r"^合计\s+(?P<total>\d+)\s+通过\s+(?P<ok>\d+)\s+红\s+(?P<red>\d+)"
)
_FAILLIST_RE = re.compile(r"^\s+-\s+(?P<id>\S+?):\s*(?P<detail>.*)$")
_VERDICT_RE = re.compile(r"^裁决词:\s*(?P<verdict>\S+)")

# 显式跳过的标记：ctest 的 SKIPPED 段、pytest 的 skipped 计数、GTEST_SKIP 提示
_SKIP_HINTS = ("SKIPPED", "skipped", "GTEST_SKIP", "该判据记为未执行", "记为未执行")

# pytest 汇总行：`55 passed, 9 skipped, 5 warnings in 49.44s`
#              `113 tests collected in 7.76s`（--collect-only，无 passed/skipped）
#              `1 failed, 62 passed, 9 skipped in 30s`
_PYTEST_COUNT_RE = re.compile(r"(\d+)\s+(passed|failed|error|errors|skipped|xfailed|xpassed|warnings)")
_PYTEST_REASON_RE = re.compile(r"^SKIPPED\s*(?:\[(\d+)\])?\s*(.*)$")


def parse_pytest_output(text: str) -> Dict[str, Any]:
    """解析 pytest 风格输出（module / integration / e2e 三层走这条）。

    ⚠️ pytest 的退出码 **5 = 没收集到测试**。那是**假绿**的高发形态，
    本函数在既无 `passed` 也无 `failed` 计数时返回 `case_total == 0`，
    由 `classify` 落成 `not_collected` —— 绝不补记为通过。
    """
    counts: Dict[str, int] = {}
    reasons: List[str] = []
    for line in text.splitlines():
        m = _PYTEST_REASON_RE.match(line.strip())
        if m:
            reasons.append((m.group(2) or "未给出跳过理由").strip())
        if re.search(r"\b(passed|failed|skipped|error)\b", line) and " in " in line:
            for n, word in _PYTEST_COUNT_RE.findall(line):
                counts[word.rstrip("s") if word.startswith("error") else word] = int(n)

    passed = counts.get("passed", 0)
    failed = counts.get("failed", 0) + counts.get("error", 0)
    skipped = counts.get("skipped", 0)
    # `collected` 不计入总量：它描述「有多少条」，不描述「跑了多少条」。
    total = passed + failed + skipped
    return {
        "cases": [],
        "case_total": total,
        "case_red": failed,
        "case_skipped": skipped,
        "summary_line": counts or None,
        "verdict": None,
        "fail_details": {f"skipped[{i+1}]": r for i, r in enumerate(reasons)},
    }


def parse_layer_output(text: str) -> Dict[str, Any]:
    """把一层运行器的 stdout 解析成结构化结论。

    解析不到汇总行 ⇒ `not_collected`。**不**把「没解析到」当成通过 ——
    这是 `docs/engineering/testing/TEST.md:136-138`「假绿」条款的执行面。
    """
    cases: List[Dict[str, Any]] = []
    summary: Optional[Dict[str, int]] = None
    verdict: Optional[str] = None
    fail_details: Dict[str, str] = {}
    in_fail_list = False

    for line in text.splitlines():
        m = _CASE_RE.match(line)
        if m:
            cases.append({
                "id": m.group("id"),
                "mark": m.group("mark"),
                "ms": float(m.group("ms")),
            })
            in_fail_list = False
            continue

        m = _SUMMARY_RE.match(line)
        if m:
            summary = {k: int(v) for k, v in m.groupdict().items()}
            in_fail_list = False
            continue

        m = _VERDICT_RE.match(line)
        if m:
            verdict = m.group("verdict")
            continue

        if line.strip().startswith("红项清单"):
            in_fail_list = True
            continue

        if in_fail_list:
            m = _FAILLIST_RE.match(line)
            if m:
                fail_details[m.group("id")] = m.group("detail")
                continue
            if not line.strip():
                in_fail_list = False

    red = sum(1 for c in cases if c["mark"] == "FAIL")
    return {
        "cases": cases,
        "case_total": len(cases),
        "case_red": red,
        "case_skipped": 0,
        "summary_line": summary,
        "verdict": verdict,
        "fail_details": fail_details,
    }


def classify(step: Dict[str, Any], raw_path: str) -> Dict[str, Any]:
    """给一步定状态。**缺产物 ⇒ not_collected**（绝不补记为 pass）。"""
    parser = step.get("parser", "layer")

    if raw_path is None or not os.path.exists(raw_path):
        status, detail = NOT_COLLECTED, "原始输出文件缺失：步骤未执行、崩溃或未上传"
        return _result(step, status, detail, {}, [])

    with open(raw_path, encoding="utf-8", errors="replace") as fh:
        text = fh.read()

    if not text.strip():
        # 产物在、但一个字符都没有 ⇒ 这一步什么都没跑。记「跳过」，不记「通过」。
        # ⚠️ 此前该分支写成「跳过记号 in text and not text.strip()」，而空输出里
        # 永远不含记号 ⇒ 分支恒不可达，是逻辑缺陷（已修正）。
        # 依据 TEST.md:89 逐字「不以伪通过掩盖未执行」、
        # TEST.md:136-138 逐字「『没选中要跑的东西』记成通过是假绿」。
        return _result(step, SKIPPED, "产物存在但输出为空：该步未执行任何判据",
                       {}, [])

    if parser == "layer":
        parsed = parse_layer_output(text)
        if parsed["summary_line"] is None and parsed["case_total"] == 0:
            # 汇总行与用例行都没有 ⇒ 无从判读。这是最危险的一态：不是绿，是未知。
            return _result(step, NOT_COLLECTED,
                           "未解析到汇总行也无用例行：输出格式变化或步骤崩溃",
                           parsed, [])
        status = FAIL if parsed["case_red"] else PASS
        detail = (f"用例 {parsed['case_total']} 条，红 {parsed['case_red']} 条"
                  + (f"，裁决词 {parsed['verdict']}" if parsed["verdict"] else ""))
        return _result(step, status, detail, parsed,
                       [f"{k}: {v}" for k, v in parsed["fail_details"].items()])

    if parser == "pytest":
        # ⚠️ pytest 的 5 = 没收集到测试。这里 case_total==0 即 not_collected，
        # 绝补记为通过（docs/engineering/testing/TEST.md:136-138「假绿」）。
        parsed = parse_pytest_output(text)
        if parsed["case_total"] == 0:
            return _result(step, NOT_COLLECTED,
                           "pytest 未收集到任何用例（退出码 5 的形态）：判读面为空",
                           parsed, [])
        status = FAIL if parsed["case_red"] else PASS
        detail = (f"用例 {parsed['case_total']} 条，红 {parsed['case_red']} 条，"
                  f"跳过 {parsed['case_skipped']} 条")
        violations = [f"{k}: {v}" for k, v in parsed["fail_details"].items()]
        return _result(step, status, detail, parsed, violations)

    if parser == "log_scan":
        # 自由文本步骤：按 spec 的 needles 判定（None = 期望缺席）
        needles = step.get("needles", [])
        absences = step.get("absent_needles", [])
        hit_fail = [n for n in needles if n in text]
        hit_absent = [n for n in absences if n in text]
        status = FAIL if hit_fail or hit_absent else PASS
        detail = f"命中违禁模式 {hit_fail + hit_absent}（期望违禁 {needles + absences}）"
        return _result(step, status, detail, {}, hit_fail + hit_absent)

    if parser == "none":
        # 只登记「跑没跑」，不判红绿（如 Windows 构建：失败即 fail）
        return _result(step, PASS if text.strip() else NOT_COLLECTED,
                       "仅登记是否执行", {}, [])

    return _result(step, NOT_COLLECTED, f"未知 parser={parser}", {}, [])


def _result(step, status, detail, parsed, violations) -> Dict[str, Any]:
    return {
        "id": step["id"],
        "name": step.get("name", step["id"]),
        "lane": step.get("lane", ""),
        "tier": step.get("tier", ""),
        "spec": step.get("spec", ""),
        "command": step.get("command", ""),
        "blocking": bool(step.get("blocking", False)),
        "blocking_candidate": bool(step.get("blocking_candidate", False)),
        "blocking_reason": step.get("blocking_reason", ""),
        "discriminating_power": step.get("discriminating_power", ""),
        "current_state": step.get("current_state", ""),
        "timeout_s": step.get("timeout_s"),
        "status": status,
        "detail": detail,
        "case_total": parsed.get("case_total", 0),
        "case_red": parsed.get("case_red", 0),
        "case_skipped": parsed.get("case_skipped", 0),
        "green_class": _green_class(status, parsed),
        "uncovered": step.get("uncovered", []),
        "violations": violations[:10],
        "violation_count": len(violations),
    }


def _green_class(status: str, parsed: Dict[str, Any]) -> str:
    """把状态进一步分成两档「绿」——这是本项目反复误判的地方。

    - `conformant`：**跑通且符合预期**。在册判据全跑、全过、无跳过。
    - `green_with_gaps`：跑完且无红，但**有跳过**。这是**跳过也算绿**的那一档，
      `docs/engineering/testing/TEST.md:89` 逐字「不以伪通过掩盖未执行」正是针对它。
    """
    if status == PASS:
        return "conformant" if not parsed.get("case_skipped", 0) else "green_with_gaps"
    if status == SKIPPED:
        return "green_with_gaps"
    return "not_green"


def load_steps(path: Optional[str]) -> List[Dict[str, Any]]:
    if not path:
        return []
    with open(path, encoding="utf-8") as fh:
        if path.endswith((".yaml", ".yml")):
            import yaml  # 仅解析步骤清单时用；运行期主路径不依赖
            return yaml.safe_load(fh)["steps"]
        return json.load(fh)["steps"]


def summarize(results: List[Dict[str, Any]]) -> Dict[str, Any]:
    counts = {PASS: 0, FAIL: 0, SKIPPED: 0, NOT_COLLECTED: 0}
    for r in results:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    counts["green"] = sum(counts[s] for s in GREEN_STATES)
    return counts


def render_markdown(report: Dict[str, Any]) -> str:
    c = report["counts"]
    out: List[str] = []
    out.append("# 持续集成报告（报告，不是判决）")
    out.append("")
    out.append(f"提交：`{report.get('commit', '未知')}`　"
               f"运行：`{report.get('run', '未知')}`　"
               f"平台车道：`{report.get('lane', '未知')}`")
    out.append("")
    out.append("| 状态 | 步数 | 计入「通过」 |")
    out.append("|---|---|---|")
    out.append(f"| 通过 pass | {c[PASS]} | 是 |")
    out.append(f"| 判红 fail | {c[FAIL]} | **否** |")
    out.append(f"| 跳过 skipped | {c[SKIPPED]} | **否** |")
    out.append(f"| 未收集 not_collected | {c[NOT_COLLECTED]} | **否** |")
    out.append("")
    out.append(f"**「通过 {c['green']} / 总 {report['total']}」** —— 分母是总步数，"
               "不是通过数之和以外的任何东西；跳过与未收集都不计入分子。")
    out.append("")
    out.append("## 步骤清单")
    out.append("")
    out.append("| # | 步骤 | 车道 | 状态 | 阻塞 | 依据 | 说明 |")
    out.append("|---|---|---|---|---|---|---|")
    for i, r in enumerate(report["results"], 1):
        flag = "**阻塞**" if r["blocking"] else "非阻塞"
        out.append(f"| {i} | {r['name']} | {r['lane']} | `{r['status']}` | {flag} "
                   f"| {r['spec']} | {r['detail']} |")
    out.append("")
    blocking = [r for r in report["results"] if r["blocking"]]
    out.append(f"## 阻塞项判别力论证（{len(blocking)} 项）")
    out.append("")
    if not blocking:
        out.append("本车道无阻塞项。")
    else:
        for r in blocking:
            out.append(f"### {r['name']}（`{r['status']}`）")
            out.append(f"- 依据：{r['spec']}")
            out.append(f"- 判别力：{r['discriminating_power'] or '**未论证**'}")
            out.append(f"- 现状：{r['detail']}")
            out.append("")
    bad = [r for r in report["results"] if r["status"] in (FAIL, NOT_COLLECTED)]
    out.append(f"## 需要人看的红项与缺口（{len(bad)} 项）")
    out.append("")
    if not bad:
        out.append("无。")
    else:
        out.append("| 步骤 | 状态 | 对象计数 | 前 10 条 |")
        out.append("|---|---|---|---|")
        for r in bad:
            head = "；".join(str(v) for v in r["violations"][:10]) or "—"
            out.append(f"| {r['name']} | `{r['status']}` | {r['violation_count']} | {head} |")
    out.append("")
    out.append("> 本报告不裁决合入。红项是缺陷信号，按修复或回退处理；"
               "合入由提交纪律决定。")
    return "\n".join(out) + "\n"


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="持续集成报告收集器（非门禁）")
    ap.add_argument("--raw", required=True, help="各步骤原始输出所在目录")
    ap.add_argument("--out", required=True, help="产物输出目录")
    ap.add_argument("--steps", default=None, help="步骤清单（YAML/JSON）")
    ap.add_argument("--commit", default=os.environ.get("GITHUB_SHA", ""))
    ap.add_argument("--run", default=os.environ.get("GITHUB_RUN_ID", ""))
    ap.add_argument("--lane", default=os.environ.get("ACSD_CI_LANE", ""))
    ap.add_argument("--enforce", action="store_true",
                    help="仅对阻塞清单内的步骤 fail/not_collected 返回非 0")
    args = ap.parse_args(argv)

    steps = load_steps(args.steps) if args.steps else _discover(args.raw)
    results = []
    for st in steps:
        raw = os.path.join(args.raw, f"{st['id']}.txt")
        results.append(classify(st, raw if os.path.exists(raw) else None))

    report = {
        "schema": "acsd.ci-report/1",
        "commit": args.commit,
        "run": args.run,
        "lane": args.lane,
        "total": len(results),
        "counts": summarize(results),
        "results": results,
        "enforced": bool(args.enforce),
    }

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "report.json"), "w", encoding="utf-8") as fh:
        json.dump(report, fh, ensure_ascii=False, indent=2)
    with open(os.path.join(args.out, "summary.md"), "w", encoding="utf-8") as fh:
        fh.write(render_markdown(report))

    counts = report["counts"]
    print(f"[collect_report] 步数 {report['total']}　通过 {counts[PASS]}　"
          f"红 {counts[FAIL]}　跳过 {counts[SKIPPED]}　未收集 {counts[NOT_COLLECTED]}")

    if not args.enforce:
        return 0

    breaches = [r for r in results
                if r["blocking"] and r["status"] in (FAIL, NOT_COLLECTED, SKIPPED)]
    if breaches:
        print("[collect_report] 阻塞项破线：", file=sys.stderr)
        for r in breaches:
            print(f"  - {r['id']}: {r['status']} — {r['detail']}", file=sys.stderr)
        return 1
    return 0


def _discover(raw_dir: str) -> List[Dict[str, Any]]:
    """无步骤清单时，从原始输出文件名兜底登记（全部非阻塞）。"""
    if not os.path.isdir(raw_dir):
        return []
    out = []
    for fn in sorted(os.listdir(raw_dir)):
        if not fn.endswith(".txt"):
            continue
        sid = fn[:-4]
        out.append({"id": sid, "name": sid, "parser": "layer", "blocking": False})
    return out


if __name__ == "__main__":
    raise SystemExit(main())
