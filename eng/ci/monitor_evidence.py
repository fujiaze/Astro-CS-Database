#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/monitor_evidence.py — 监控证据的 fail-closed 判定（run.py / run_checks.py 共用）。

权威依据
  * docs/ci/CI_SPEC.md §9（监控字段语义：requires_monitor 真强制）；
  * ENGINEERING_SPEC.md §10（fail-closed：证据缺失判红）；
  * 阈值唯一数值源 eng/contracts/resource_gate_v1.json（经 l2_frozen_gate 读取）。

语义（GATE-501 / D-12 二选一落地，取「真强制」分支）
  1. requires_monitor=true ⇒ 该检查**必须**产出监控证据（登记 outputs 中存在含
     cpu_samples 的监控 JSON）；缺失/不可解析 ⇒ 判红 FAIL(monitor_gate_missing)。
  2. 命令显式请求资源门判定（监控参数区含 --gate-required/--gate-workers）⇒
     证据还必须含合法 frozen_gate.verdict ∈ {pass, not_applicable}。
  3. 证据含 frozen_gate ⇒ 四条 L2 冻结判据按 l2_frozen_gate fail-closed 复核：
     任一违规 ⇒ 判红（这是 D-10「恒真门」在 CI 裁决面的堵口）。
     分母未声明/门不适用按契约显式分类处理（不参与裁决，记录项），不放松判据。
"""
from __future__ import annotations

import glob as _glob
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import l2_frozen_gate as _l2  # noqa: E402  (同目录：判据单一实现点)

GATE_FLAGS = ("--gate-required", "--gate-workers")
MONITOR_SCRIPTS = ("resource_monitor.py", "run_monitored.py")
V_GATE_MISSING = "FAIL(monitor_gate_missing)"


def gate_requested(command: list) -> bool:
    """命令的**监控参数区**（-- 之前）是否请求资源门判定。"""
    cmd = list(command or [])
    head = cmd[:cmd.index("--")] if "--" in cmd else cmd
    return any(flag in head for flag in GATE_FLAGS)


def path_exists(repo: Path, rel: str) -> bool:
    """登记输出是否存在：精确路径 / 目录 / glob（与 run.py _script_exists 同口径）。"""
    if not rel:
        return False
    path = repo / rel
    if path.exists():
        return True
    if any(ch in rel for ch in "*?["):
        return bool(_glob.glob(str(path), recursive=True))
    return False


def monitor_evidence(repo: Path, outputs: list) -> list:
    """登记 outputs 中定位监控证据 JSON（含 cpu_samples 键），返回 [(rel, data)]。"""
    found = []
    for rel in outputs or []:
        path = repo / str(rel)
        if not path.is_file():
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        if isinstance(data, dict) and "cpu_samples" in data:
            found.append((str(rel), data))
    return found


def monitor_evidence_gap(check: dict, repo: Path,
                         require_frozen_gate: bool | None = None) -> str | None:
    """requires_monitor 证据面 fail-closed 判定。

    返回 None = 证据齐备；否则返回原因串（调用方判 FAIL(monitor_gate_missing)）。
    非 requires_monitor 检查恒返回 None（该字段不适用）。
    """
    if not check.get("requires_monitor"):
        return None
    outputs = list(check.get("outputs") or [])
    for step in check.get("steps") or []:
        if isinstance(step, dict):
            outputs += list(step.get("outputs") or [])
    found = monitor_evidence(repo, outputs)
    if not found:
        return ("登记 outputs 中未找到含 cpu_samples 的监控证据 JSON"
                "（requires_monitor=true 即必须监控：缺失证据判红, fail-closed）")
    if require_frozen_gate is None:
        # 请求判定 = 本检查自身命令或任一 step 命令的监控参数区含判定旗标
        require_frozen_gate = gate_requested(list(check.get("command") or [])) or any(
            gate_requested(list(s.get("command") or []))
            for s in (check.get("steps") or []) if isinstance(s, dict))
    thresholds = None
    for rel, data in found:
        gate = data.get("frozen_gate")
        if not isinstance(gate, dict):
            if require_frozen_gate:
                return (f"监控证据 {rel} 缺 frozen_gate 判定"
                        "（命令请求了资源门判定 → 必须兑现判定证据, fail-closed）")
            continue
        verdict = gate.get("verdict")
        if verdict not in ("pass", "not_applicable"):
            return (f"监控证据 {rel} 的 frozen_gate.verdict 非法或与退出码矛盾："
                    f"{verdict!r}（fail-closed）")
        # D-10 堵口：四条冻结判据 fail-closed 复核（违规即红）。
        # 阈值按需惰性加载：只有真的存在 frozen_gate 判定面时才要求合同可用
        # （无判定面的采样留证检查不因合同缺失而假红）。
        if thresholds is None:
            try:
                thresholds = _l2.load_thresholds(repo)
            except _l2.ThresholdError as exc:
                return f"L2 冻结判据阈值合同不可用（fail-closed）：{exc}"
        result = _l2.adjudicate(data, thresholds=thresholds,
                                require_applicable=False, require_evaluable=False,
                                source=rel)
        if result["verdict"] == _l2.V_RED:
            return (f"监控证据 {rel} 违反 L2 冻结判据（fail-closed）："
                    + "；".join(result["violations"]))
    return None
