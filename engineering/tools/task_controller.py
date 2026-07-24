#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTER = ROOT / "control" / "MASTER_TASK_REGISTER.csv"
STATE = ROOT / "control" / "PROJECT_STATE.yaml"
CURRENT = ROOT / "control" / "CURRENT_WORK.md"

PHASES = {
    "P00": ("基线冻结与仓库完整性恢复", "G0", "tasks/P00_BASELINE_AND_REPOSITORY_INTEGRITY.md"),
    "P01": ("可复现构建", "G1", "tasks/P01_BUILD_REPRODUCIBILITY.md"),
    "P02": ("数据契约冻结", "G2", "tasks/P02_DATA_CONTRACT_FREEZE.md"),
    "P03": ("接口契约冻结", "G3", "tasks/P03_INTERFACE_CONTRACT_FREEZE.md"),
    "P04": ("测试基础设施", "G4", "tasks/P04_TEST_INFRASTRUCTURE.md"),
    "P05": ("Stage 1 系统验证", "G5", "tasks/P05_STAGE1_VERIFICATION.md"),
    "P06": ("Stage 2 系统调试", "G6", "tasks/P06_STAGE2_SYSTEMATIC_DEBUG.md"),
    "P07": ("性能与稳定性", "G7", "tasks/P07_PERFORMANCE_AND_STABILITY.md"),
    "P08": ("发布与后续演进", "G8", "tasks/P08_RELEASE_AND_FUTURE.md"),
}


def read_rows() -> tuple[list[dict[str, str]], list[str]]:
    with REGISTER.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames:
            raise RuntimeError("任务注册表没有表头")
        return list(reader), list(reader.fieldnames)


def write_rows(rows: list[dict[str, str]], fields: list[str]) -> None:
    temporary = REGISTER.with_suffix(".csv.tmp")
    with temporary.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(REGISTER)


def by_id(rows: list[dict[str, str]]) -> dict[str, dict[str, str]]:
    return {row["task_id"]: row for row in rows}


def dependencies_done(row: dict[str, str], lookup: dict[str, dict[str, str]]) -> bool:
    deps = [item.strip() for item in row.get("depends_on", "").split(";") if item.strip()]
    return all(dep in lookup and lookup[dep]["status"] == "DONE" for dep in deps)


def update_scalar(text: str, key: str, value: str, indent: int = 0) -> str:
    pattern = re.compile(rf"(?m)^{' ' * indent}{re.escape(key)}:\s*.*$")
    replacement = f"{' ' * indent}{key}: {value}"
    if pattern.search(text):
        return pattern.sub(replacement, text, count=1)
    return text.rstrip() + "\n" + replacement + "\n"


def update_state(task: dict[str, str] | None, project_status: str) -> None:
    text = STATE.read_text(encoding="utf-8")
    if task:
        phase = task["phase"]
        phase_name, gate, _ = PHASES[phase]
        text = update_scalar(text, "current", phase, indent=2)
        text = update_scalar(text, "name", phase_name, indent=2)
        text = update_scalar(text, "gate", gate, indent=2)
        text = update_scalar(text, "gate_status", "NOT_PASSED", indent=2)
        text = update_scalar(text, "current_task", task["task_id"])
    else:
        text = update_scalar(text, "current_task", "null")
    text = update_scalar(text, "project_status", project_status)
    text = update_scalar(text, "last_updated", date.today().isoformat())
    STATE.write_text(text, encoding="utf-8")


def write_current(task: dict[str, str] | None, status: str, note: str = "") -> None:
    if task is None:
        content = """# 当前工作\n\n全部注册任务已完成。下一步执行 G0–G8 最终一致性检查、干净 clone 重建、发布包生成和最终工程报告。\n"""
    else:
        _, _, task_doc = PHASES[task["phase"]]
        content = f"""# 当前唯一工作\n\n## Task ID\n\n`{task['task_id']}` — {task['title']}\n\n## 当前状态\n\n`{status}`\n\n## 任务规范\n\n读取 `engineering/{task_doc}` 中 `{task['task_id']}` 对应条目，并同时读取该条目引用的 spec、checklist 和注册表。\n\n## 交付物\n\n{task['deliverable']}\n\n## 验收条件\n\n{task['acceptance']}\n\n## 自治规则\n\n完成实现、测试、证据归档和独立复核。复核 PASS 后使用 `engineering/tools/task_controller.py approve` 自动推进，不等待用户发送“继续”。\n"""
        if note:
            content += f"\n## 状态说明\n\n{note}\n"
    CURRENT.write_text(content, encoding="utf-8")


def require_task(rows: list[dict[str, str]], task_id: str) -> dict[str, str]:
    lookup = by_id(rows)
    if task_id not in lookup:
        raise RuntimeError(f"未知任务：{task_id}")
    return lookup[task_id]


def status_command(rows: list[dict[str, str]]) -> int:
    active = [row for row in rows if row["status"] in {"IN_PROGRESS", "IN_REVIEW", "BLOCKED"}]
    print(f"active_count={len(active)}")
    for row in active:
        print(f"{row['task_id']}\t{row['status']}\t{row['title']}")
    lookup = by_id(rows)
    eligible = [row for row in rows if row["status"] == "BACKLOG" and dependencies_done(row, lookup)]
    print("eligible=" + ",".join(row["task_id"] for row in eligible))
    return 0


def submit_command(rows: list[dict[str, str]], fields: list[str], task_id: str, evidence_dir: Path) -> int:
    task = require_task(rows, task_id)
    if task["status"] != "IN_PROGRESS":
        raise RuntimeError(f"{task_id} 当前状态不是 IN_PROGRESS，而是 {task['status']}")
    required = ["TASK_REPORT.md", "TEST_REPORT.md", "EVIDENCE_INDEX.md"]
    missing = [name for name in required if not (evidence_dir / name).is_file()]
    if missing:
        raise RuntimeError("提交复核前缺少证据文件：" + ", ".join(missing))
    task["status"] = "IN_REVIEW"
    write_rows(rows, fields)
    update_state(task, "ACTIVE_IN_REVIEW")
    write_current(task, "IN_REVIEW", "等待独立复核；不得直接标记 DONE。")
    print(f"SUBMITTED={task_id}")
    return 0


def approve_command(rows: list[dict[str, str]], fields: list[str], task_id: str, review_report: Path) -> int:
    task = require_task(rows, task_id)
    if task["status"] != "IN_REVIEW":
        raise RuntimeError(f"{task_id} 当前状态不是 IN_REVIEW，而是 {task['status']}")
    if not review_report.is_file():
        raise RuntimeError(f"复核报告不存在：{review_report}")
    review_text = review_report.read_text(encoding="utf-8", errors="replace")
    if not re.search(r"(?mi)^VERDICT:\s*PASS\s*$", review_text):
        raise RuntimeError("复核报告没有独立一行 `VERDICT: PASS`")

    task["status"] = "DONE"
    lookup = by_id(rows)
    eligible = [row for row in rows if row["status"] == "BACKLOG" and dependencies_done(row, lookup)]
    next_task = eligible[0] if eligible else None
    if next_task:
        next_task["status"] = "IN_PROGRESS"
    write_rows(rows, fields)

    if next_task:
        update_state(next_task, "ACTIVE")
        write_current(next_task, "IN_PROGRESS")
        print(f"COMPLETED={task_id}")
        print(f"NEXT_TASK={next_task['task_id']}")
    else:
        remaining = [row for row in rows if row["status"] != "DONE"]
        if remaining:
            update_state(None, "BLOCKED_ON_DEPENDENCY_GRAPH")
            write_current(None, "BLOCKED", "没有依赖已满足的 BACKLOG，但仍有未完成任务；请检查循环依赖或错误状态。")
            print(f"COMPLETED={task_id}")
            print("NEXT_TASK=NONE_DEPENDENCY_BLOCK")
            return 2
        update_state(None, "ALL_TASKS_DONE_PENDING_FINAL_GATE")
        write_current(None, "DONE")
        print(f"COMPLETED={task_id}")
        print("NEXT_TASK=FINAL_GATE")
    return 0


def fail_review_command(rows: list[dict[str, str]], fields: list[str], task_id: str, review_report: Path) -> int:
    task = require_task(rows, task_id)
    if task["status"] != "IN_REVIEW":
        raise RuntimeError(f"{task_id} 当前状态不是 IN_REVIEW，而是 {task['status']}")
    if not review_report.is_file():
        raise RuntimeError(f"复核报告不存在：{review_report}")
    text = review_report.read_text(encoding="utf-8", errors="replace")
    if not re.search(r"(?mi)^VERDICT:\s*FAIL\s*$", text):
        raise RuntimeError("复核报告没有独立一行 `VERDICT: FAIL`")
    task["status"] = "IN_PROGRESS"
    write_rows(rows, fields)
    update_state(task, "ACTIVE_REWORK")
    write_current(task, "IN_PROGRESS", "独立复核失败；按 REVIEW_REPORT.md 的 Required corrections 修正后重新提交。")
    print(f"RETURNED_FOR_REWORK={task_id}")
    return 0


def block_command(rows: list[dict[str, str]], fields: list[str], task_id: str, blocked_report: Path) -> int:
    task = require_task(rows, task_id)
    if task["status"] not in {"IN_PROGRESS", "IN_REVIEW"}:
        raise RuntimeError(f"{task_id} 不能从状态 {task['status']} 转为 BLOCKED")
    if not blocked_report.is_file():
        raise RuntimeError(f"阻塞报告不存在：{blocked_report}")
    task["status"] = "BLOCKED"
    write_rows(rows, fields)
    update_state(task, "BLOCKED")
    write_current(task, "BLOCKED", f"阻塞证据：`{blocked_report}`。阻塞解除后将任务状态恢复为 IN_PROGRESS。")
    print(f"BLOCKED={task_id}")
    return 0


def resume_command(rows: list[dict[str, str]], fields: list[str], task_id: str) -> int:
    task = require_task(rows, task_id)
    if task["status"] != "BLOCKED":
        raise RuntimeError(f"{task_id} 当前状态不是 BLOCKED，而是 {task['status']}")
    task["status"] = "IN_PROGRESS"
    write_rows(rows, fields)
    update_state(task, "ACTIVE")
    write_current(task, "IN_PROGRESS", "阻塞条件已解除，按 BLOCKED_REPORT.md 的恢复步骤继续。")
    print(f"RESUMED={task_id}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="AstroCS 自治任务状态控制器")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("status")

    p_submit = sub.add_parser("submit")
    p_submit.add_argument("--task", required=True)
    p_submit.add_argument("--evidence-dir", required=True)

    p_approve = sub.add_parser("approve")
    p_approve.add_argument("--task", required=True)
    p_approve.add_argument("--review-report", required=True)

    p_fail = sub.add_parser("fail-review")
    p_fail.add_argument("--task", required=True)
    p_fail.add_argument("--review-report", required=True)

    p_block = sub.add_parser("block")
    p_block.add_argument("--task", required=True)
    p_block.add_argument("--blocked-report", required=True)

    p_resume = sub.add_parser("resume")
    p_resume.add_argument("--task", required=True)

    args = parser.parse_args()
    rows, fields = read_rows()
    try:
        if args.command == "status":
            return status_command(rows)
        if args.command == "submit":
            return submit_command(rows, fields, args.task, Path(args.evidence_dir))
        if args.command == "approve":
            return approve_command(rows, fields, args.task, Path(args.review_report))
        if args.command == "fail-review":
            return fail_review_command(rows, fields, args.task, Path(args.review_report))
        if args.command == "block":
            return block_command(rows, fields, args.task, Path(args.blocked_report))
        if args.command == "resume":
            return resume_command(rows, fields, args.task)
        return 1
    except Exception as exc:
        print(f"TASK_CONTROLLER_ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
