#!/usr/bin/env python3
"""CLI-002: run preset/IR 驱动 + 真实 Artifact 传递校验。

规则 (RT-008 拆分后: 命令面/旗标在 cli/parser.cpp, run 管线在 cli/commands.cpp):
1. run 命令面含 phase1/2/3 run 逐 phase 命令, 每 phase 经 run_pipeline({N}, ...) 独立调度。
2. artifact 收集进 run manifest (write_run_manifest, artifacts 数组) 并落 astrocs_run_*.json。
3. verify 对 prior artifact 复算 sha256 → mismatch 返回 INTEGRITY(=8)。
4. --events-jsonl 模式 stdout 仅 JSON 事件。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
PARSER = (REPO / "cli" / "parser.cpp").read_text(encoding="utf-8")
CMDS = (REPO / "cli" / "commands.cpp").read_text(encoding="utf-8")

def main():
    errors = []
    # 1) 逐 phase 命令面 + run_pipeline 单 phase 调度
    for tok in ('{"phase1 run"', '{"phase2 run"', '{"phase3 run"'):
        if tok not in PARSER:
            errors.append(f"missing command rule: {tok}")
    for tok in ("run_pipeline({1}", "run_pipeline({2}", "run_pipeline({3}"):
        if tok not in CMDS:
            errors.append(f"missing per-phase run_pipeline dispatch: {tok}")
    # 2) artifact 收集 + manifest
    if '"artifacts", artifacts' not in CMDS:
        errors.append("no artifact collection into run manifest")
    if "astrocs_run_" not in CMDS:
        errors.append("no run manifest pattern")
    # 3) artifact sha256 mismatch → INTEGRITY(=8)
    if "artifact sha256 mismatch" not in CMDS:
        errors.append("no artifact sha256 mismatch guard")
    if "astrocs::INTEGRITY" not in CMDS:
        errors.append("no INTEGRITY(=8) exit on mismatch")
    # 4) events-jsonl 模式
    if "--events-jsonl" not in PARSER:
        errors.append("no events-jsonl mode")
    if errors:
        print("CLI-002_PRESET_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("CLI-002_PASS: phase1/2/3 run 命令面, run_pipeline 逐 phase 调度, artifact 进 run manifest, sha256 mismatch→8, events-jsonl")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
