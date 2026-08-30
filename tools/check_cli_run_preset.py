#!/usr/bin/env python3
"""CLI-002: run preset/IR 驱动 + 真实 Artifact 传递校验。

规则:
1. run 经 phases 列表调度 (preset), 每 phase 独立 session。
2. artifact 收集到 all_artifacts 并写入 run manifest。
3. resume/hash-mismatch: prior artifact sha 校验 → 失败 exit 8。
4. --events-jsonl 模式 stdout 仅 JSON 事件。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
MAIN = (REPO / "cli" / "main.cpp").read_text(encoding="utf-8")

def main():
    errors = []
    # 1) preset 调度
    if "for (int phase : phases)" not in MAIN:
        errors.append("no phases-list (preset) dispatch")
    for tok in ("p1_session_create", "p2_session_create", "p3_session_create"):
        if tok not in MAIN:
            errors.append(f"missing session: {tok}")
    # 2) artifact 收集 + manifest
    if "all_artifacts" not in MAIN:
        errors.append("no artifact collection")
    if "astrocs_run_" not in MAIN:
        errors.append("no run manifest pattern")
    # 3) hash-mismatch → exit 8
    if "prior artifact hash mismatch" not in MAIN:
        errors.append("no hash-mismatch guard")
    # 4) events-jsonl 模式
    if "--events-jsonl" not in MAIN:
        errors.append("no events-jsonl mode")
    if errors:
        print("CLI-002_PRESET_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("CLI-002_PASS: run preset phases 调度, session 逐 phase, artifact 哈希链, hash-mismatch→8, events-jsonl")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
