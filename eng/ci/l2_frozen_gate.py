#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/l2_frozen_gate.py — L2 性能门冻结判据的 fail-closed 判定核心（GATE-501）。

背景（RELEASE-04 偏差 D-10 / GAP_AUDIT G2-1）
  L2 性能门的四条冻结判据（平均利用率 / p50 / 达标样本占比 / 无低利用窗）
  在 eng/tools/monitoring/run_monitored.py::evaluate_frozen_gate 里以
  enforcement=record_and_justify 落地：违规只写进 recorded，**不改变 verdict**。
  后果：artifacts/acceptance/l2_performance/gates/*_gate.json 里四条判据全部
  违规（平均 0.245<0.85、p50 0.039<0.90、达标占比 0.069<0.70、连续 142.0s
  低利用窗）而 frozen_gate.verdict="pass" —— 恒真门（AGENTS.md §9「恒真门
  没有证据资格」、§6「不用 waiver 盖红灯」）。

本模块的角色
  把「违规必红」落到 **CI 裁决面**：给定一份监控证据（run_monitored 结果 JSON），
  逐条重算四条冻结判据并 fail-closed 裁决。阈值**不写字面量**——唯一数值源是
  eng/contracts/resource_gate_v1.json（该文件的 *_enforcement=record_and_justify
  是**生产侧**记录语义，见 docs/ci/CI_SPEC.md §9：CI 裁决面按本模块判红）。

fail-closed 规则（ENGINEERING_SPEC §10）
  * 证据缺失 / 不可解析 / 非 dict            -> red（evidence_missing）
  * 无 CPU 采样且无 frozen_gate              -> red（evidence_missing）
  * 判据不可评估（无 metrics 且无法重算）      -> red（evidence_unevaluable）
  * require_applicable=True 且门不适用        -> red（gate_not_applicable：
    L2 验收证据必须落在门的适用域内，NOT_APPLICABLE 是分类不是豁免）
  * 分母（已分配容量）未声明                  -> require_evaluable=True 时 red
    （l2_denominator_undeclared）；False 时按契约 denominator.zero_denominator_effect
    记入 recorded、利用率类判据不参与裁决（**不改判据、不放松**）
  * 任一冻结判据违规                          -> red（逐条列出实测值与阈值）

用法（库）:
  from l2_frozen_gate import load_thresholds, adjudicate
  verdict = adjudicate(evidence, thresholds=load_thresholds(repo))
"""
from __future__ import annotations

import json
import statistics
from pathlib import Path
from typing import Any, Optional

CONTRACT_REL = "eng/contracts/resource_gate_v1.json"

V_PASS = "pass"
V_RED = "red"
V_NOT_APPLICABLE = "not_applicable"

# 四条冻结判据的稳定 ID（判定表/证据 JSON 的键；与任务书 GATE-501 步骤 1 一一对应）
C_AVG = "avg_utilization_ge_min"
C_P50 = "p50_utilization_ge_min"
C_FRACTION = "sample_pass_fraction_ge_min"
C_WINDOW = "no_low_utilization_window"


class ThresholdError(RuntimeError):
    """阈值合同不可用 -> 调用方必须 fail-closed（绝不回落字面量默认值）。"""


def load_thresholds(repo: Path | str) -> dict:
    """从唯一数值源读取冻结判据阈值（缺失/非法 -> ThresholdError）。

    绝不内置字面量兜底：合同不可用即「判据无据」，按 fail-closed 处理。
    """
    path = Path(repo) / CONTRACT_REL
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:  # noqa: BLE001 - 统一转合同错误
        raise ThresholdError(f"阈值合同不可读：{path}（{exc}）") from exc
    try:
        compute = data["compute"]
        applicability = data["applicability"]
        return {
            "min_effective_cpus": int(applicability["min_effective_cpus"]),
            "min_interval_seconds_exclusive": float(
                applicability["min_active_window_seconds_exclusive"]),
            "avg_min": float(compute["mean_utilization_min_percent"]) / 100.0,
            "p50_min": float(compute["p50_utilization_min_percent"]) / 100.0,
            "sample_min": float(compute["per_sample_utilization_min_percent"]) / 100.0,
            "sample_fraction_min": float(compute["per_sample_pass_fraction_min"]),
            "low_util_percent": float(compute["queue_low_utilization_percent"]) / 100.0,
            "low_window_seconds": float(compute["queue_low_window_seconds_min"]),
            "contract": CONTRACT_REL,
        }
    except (KeyError, TypeError, ValueError) as exc:
        raise ThresholdError(f"阈值合同字段缺失/非法：{path}（{exc!r}）") from exc


def _num(value: Any) -> Optional[float]:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


def _samples(evidence: dict) -> list:
    for key in ("cpu_samples", "samples"):
        value = evidence.get(key)
        if isinstance(value, list):
            return [s for s in value if isinstance(s, dict)]
    return []


def _recompute_from_samples(samples: list, allocated: float,
                            interval: float, poll: float,
                            thresholds: dict) -> dict:
    """无 frozen_gate.metrics 时的等价重算（同一算法，阈值同源）。

    仅当证据里没有生产侧 metrics 时使用；有 metrics 时以生产侧实测为准
    （单一实现点原则），本函数只做交叉核对。
    """
    denom = 100.0 * allocated
    valid = [v for v in (_num(s.get("cpu_percent")) for s in samples) if v is not None]
    metrics: dict = {"recomputed_from_samples": True, "interval_seconds": interval}
    if not valid:
        return metrics
    avg = sum(valid) / len(valid)
    metrics["avg_utilization"] = avg / denom
    utils = [v / denom for v in valid]
    metrics["p50_utilization"] = statistics.median(utils)
    metrics["sample_pass_fraction"] = (
        sum(1 for u in utils if u >= thresholds["sample_min"]) / len(utils))
    # 低利用窗：与生产侧同口径（连续样本 util < low_util，累计窗口时长）
    run_seconds, best, prev_t, prev_low = 0.0, 0.0, None, False
    for s in samples:
        v = _num(s.get("cpu_percent"))
        t = _num(s.get("t"))
        low = v is not None and (v / denom) < thresholds["low_util_percent"]
        if low and prev_low and t is not None and prev_t is not None:
            run_seconds += max(0.0, t - prev_t)
        elif low:
            run_seconds = poll
        else:
            run_seconds = 0.0
        best = max(best, run_seconds)
        prev_t = t if t is not None else prev_t
        prev_low = low
    metrics["max_low_window_seconds"] = best
    return metrics


def adjudicate(evidence: Any, *, thresholds: dict,
               require_applicable: bool = True,
               require_evaluable: bool = True,
               source: str = "") -> dict:
    """对一份监控证据做 L2 冻结判据 fail-closed 裁决。

    require_applicable  True = 门必须适用（L2 验收证据）；False = 允许
                        NOT_APPLICABLE（短区间/单核的常规监控检查）。
    require_evaluable   True = 分母未声明/利用率判据不成立即红（L2 验收证据）；
                        False = 按契约 denominator.zero_denominator_effect 记入
                        recorded，利用率类判据不参与裁决。
    返回 dict：verdict 属于 {pass, red, not_applicable}；violations 非空 <=> verdict=red；
    criteria 为四条判据的逐条实测表；recorded 为不参与裁决的记录项。
    """
    out: dict = {
        "verdict": V_RED,
        "violations": [],
        "recorded": [],
        "criteria": [],
        "metrics": {},
        "source": source,
        "thresholds_contract": thresholds.get("contract"),
    }

    def red(code: str, detail: str) -> dict:
        out["violations"].append(f"{code}: {detail}")
        out["verdict"] = V_RED
        return out

    if not isinstance(evidence, dict):
        return red("evidence_missing",
                   f"监控证据不是 JSON 对象（{type(evidence).__name__}）")
    gate = evidence.get("frozen_gate")
    samples = _samples(evidence)
    if not samples and not isinstance(gate, dict):
        return red("evidence_missing",
                   "既无 cpu_samples 采样也无 frozen_gate 判定（无实测证据面）")
    if gate is not None and not isinstance(gate, dict):
        return red("evidence_missing",
                   f"frozen_gate 非法（{type(gate).__name__}）-> 判定不可复核")

    metrics: dict = {}
    if isinstance(gate, dict):
        gm = gate.get("metrics")
        if isinstance(gm, dict):
            metrics = dict(gm)
        out["gate_verdict"] = gate.get("verdict")
    if not metrics:
        allocated_hint = _num(evidence.get("allocated_workers"))
        if allocated_hint is None or allocated_hint <= 0:
            return red("evidence_unevaluable",
                       "证据缺 frozen_gate.metrics 且无 allocated_workers 声明"
                       "（不可重算 -> fail-closed）")
        poll = _num(evidence.get("poll_interval")) or 0.2
        interval = _num(evidence.get("duration_seconds"))
        if interval is None:
            return red("evidence_unevaluable", "证据缺 duration_seconds，区间不可得")
        metrics = _recompute_from_samples(samples, allocated_hint, interval, poll,
                                          thresholds)

    effective = _num(metrics.get("effective_cpus"))
    allocated = _num(metrics.get("allocated"))
    interval = _num(metrics.get("interval_seconds"))
    out["metrics"] = {
        "effective_cpus": effective,
        "allocated": allocated,
        "interval_seconds": interval,
        "avg_utilization": _num(metrics.get("avg_utilization")),
        "p50_utilization": _num(metrics.get("p50_utilization")),
        "sample_pass_fraction": _num(metrics.get("sample_pass_fraction")),
        "max_low_window_seconds": _num(metrics.get("max_low_window_seconds")),
        "utilization_evaluated": bool(metrics.get("utilization_evaluated", False)),
    }

    # -- 适用性（先分类，再判据；NOT_APPLICABLE 是显式分类，不是豁免） --
    if effective is None or interval is None:
        return red("evidence_unevaluable",
                   "证据缺 effective_cpus / interval_seconds（适用性无法判定）")
    not_applicable_reasons = []
    if effective < thresholds["min_effective_cpus"]:
        not_applicable_reasons.append(
            f"effective_cpus={effective:g} < {thresholds['min_effective_cpus']}")
    if interval <= thresholds["min_interval_seconds_exclusive"]:
        not_applicable_reasons.append(
            f"interval={interval:g}s <= {thresholds['min_interval_seconds_exclusive']:g}s")
    if not_applicable_reasons:
        if require_applicable:
            return red("gate_not_applicable",
                       "L2 验收证据落在门适用域之外（NOT_APPLICABLE 非豁免）："
                       + "; ".join(not_applicable_reasons))
        out["verdict"] = V_NOT_APPLICABLE
        out["reason"] = "门不适用（NOT_APPLICABLE, 非豁免）: " + "; ".join(
            not_applicable_reasons)
        return out

    # -- 分母（已分配容量）：契约 denominator.zero_denominator_effect --
    if allocated is None or allocated <= 0:
        msg = ("l2_denominator_undeclared: 已分配容量未声明（granted_workers / "
               "selected_workers 皆哨兵 0）-> 利用率类判据不成立")
        if require_evaluable:
            return red("l2_denominator_undeclared",
                       msg + "；L2 验收证据必须声明分母（fail-closed）")
        out["recorded"].append(msg + "（契约 denominator.zero_denominator_effect："
                                    "记入 recorded，不参与裁决）")
        out["verdict"] = V_PASS
        return out

    # -- 四条冻结判据：任一违规 => red（fail-closed） --
    avg = _num(metrics.get("avg_utilization"))
    p50 = _num(metrics.get("p50_utilization"))
    frac = _num(metrics.get("sample_pass_fraction"))
    window = _num(metrics.get("max_low_window_seconds"))
    if avg is None or p50 is None or frac is None or window is None:
        return red("evidence_unevaluable",
                   "冻结判据实测值缺失（avg/p50/达标占比/低利用窗 需全部可得）")

    checks = [
        (C_AVG, "平均利用率", avg, thresholds["avg_min"], ">=",
         f"计算区间平均利用率 {avg:.3f} < {thresholds['avg_min']:.2f}"),
        (C_P50, "样本利用率中位数", p50, thresholds["p50_min"], ">=",
         f"样本利用率 p50 {p50:.3f} < {thresholds['p50_min']:.2f}"),
        (C_FRACTION, "达标样本占比", frac, thresholds["sample_fraction_min"], ">=",
         f"单样本利用率 >={thresholds['sample_min']:.0%} 的占比 {frac:.3f} "
         f"< {thresholds['sample_fraction_min']:.2f}"),
        (C_WINDOW, "无低利用窗", window, thresholds["low_window_seconds"], "<",
         f"存在连续 {window:.1f}s 利用率 <{thresholds['low_util_percent']:.0%} 的窗口"
         f"（阈值 {thresholds['low_window_seconds']:g}s；L2 冻结判据不允许低利用窗，"
         f"无就绪积压同样计违规——串行/停顿与 CPU 饥饿同属性能缺陷）"),
    ]
    for cid, name, value, threshold, op, detail in checks:
        ok = value >= threshold if op == ">=" else value < threshold
        out["criteria"].append({
            "id": cid, "name": name, "value": round(value, 6),
            "threshold": threshold, "op": op, "ok": ok,
        })
        if not ok:
            out["violations"].append(f"{cid}: {detail}")
    out["verdict"] = V_RED if out["violations"] else V_PASS
    return out
