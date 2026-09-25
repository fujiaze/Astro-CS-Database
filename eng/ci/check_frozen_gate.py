#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/check_frozen_gate.py — L2 冻结判据裁决器 + 可执行正负例面（GATE-501）。

权威依据
  * docs/ci/CI_SPEC.md §4/§9（L2 性能门 = fail-closed 裁决面）；
  * docs/ci/03_GATES.md §L2（四条冻结判据与红/绿判据）；
  * ENGINEERING_SPEC.md §10（fail-closed：输入缺失/路径不存在/依赖不可用判红；
    每项检查提供机器可执行负例入口 --self-test）；
  * 阈值唯一数值源：eng/contracts/resource_gate_v1.json（本文件不含字面量阈值）。

三种模式
  1) --evidence <gate.json> [--evidence ...]  对给定监控证据逐份裁决（rc=0 全绿）
  2) --replay [GLOB]                          回放 RELEASE-04 归档的 L2 违规证据，
     断言「历史 verdict=pass 的证据现在必须判红」——即恒真门已改真判红。
     rc=0 = 回放成立（门有牙）；rc=1 = 门仍然放过违规证据（退化，必须修）。
  3) --self-test                              红/绿双向自测（正例必须绿、负例必须红，
     含缺失证据 / 坏证据 / 空文件 / 无输出 / 分母未声明 / 门不适用）。

用法:
  python3 eng/ci/check_frozen_gate.py --self-test
  python3 eng/ci/check_frozen_gate.py --evidence artifacts/acceptance/l2_performance/gates/real16_w16_gate.json
  python3 eng/ci/check_frozen_gate.py --replay --json-out run/ci/l2-frozen-gate/replay.json
exit 0 = 通过；1 = 判红（违规/回放不成立）；2 = 输入或依赖不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import glob as _glob
import json
import pathlib
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import l2_frozen_gate as L  # noqa: E402  (同目录：判定核心，单一实现点)

REPO = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_REPLAY_GLOB = "artifacts/acceptance/l2_performance/gates/*_gate.json"
EXIT_PASS, EXIT_RED, EXIT_UNAVAILABLE = 0, 1, 2


def _synthetic_good(workers: int = 16, interval: float = 60.0,
                    cpu_percent: float = 1600.0) -> dict:
    """合成合规证据：满负荷多核重计算区间（平均/p50/达标占比全达标，无低利用窗）。"""
    samples = [{"t": round(i * 0.2, 3), "cpu_percent": cpu_percent, "threads": workers,
                "runnable": workers} for i in range(int(interval / 0.2))]
    # conclusion-anchor: 本函数是夹具构造器（合成 100% 利用率样本），返回的是**被注入的
    # 输入数据**，其 verdict 是夹具值；真判定由 check_frozen_gate 的 replay 逻辑产生。
    return {
        "duration_seconds": interval,
        "poll_interval": 0.2,
        "cpu_samples": samples,
        "frozen_gate": {
            "verdict": "pass",
            "violations": [],
            "recorded": [],
            "metrics": {
                "effective_cpus": workers,
                "allocated": workers,
                "interval_seconds": interval,
                "avg_utilization": cpu_percent / (100.0 * workers),
                "p50_utilization": cpu_percent / (100.0 * workers),
                "sample_pass_fraction": 1.0,
                "max_low_window_seconds": 0.0,
                "utilization_evaluated": True,
            },
        },
    }


def _synthetic_violating() -> dict:
    """合成违规证据：四条冻结判据全违规（数值取 RELEASE-04 real16_w16 归档实测）。"""
    ev = _synthetic_good()
    ev["frozen_gate"]["metrics"].update({
        "avg_utilization": 0.245275,
        "p50_utilization": 0.038825,
        "sample_pass_fraction": 0.069272,
        "max_low_window_seconds": 142.049,
    })
    ev["frozen_gate"]["recorded"] = [
        "frozen_avg_utilization_low: ...（enforcement=record_and_justify）",
        "frozen_p50_utilization_low: ...（enforcement=record_and_justify）",
        "frozen_sample_utilization_low: ...（enforcement=record_and_justify）",
        "frozen_low_utilization_window_no_queue: 连续 142.0s 利用率<60% 但无就绪线程积压",
    ]
    return ev


def judge_file(path: pathlib.Path, thresholds: dict) -> dict:
    """读取并裁决一份证据文件（不可读/不可解析 -> red, fail-closed）。"""
    try:
        raw = path.read_text(encoding="utf-8")
    except OSError as exc:
        return {"verdict": L.V_RED, "violations": [f"evidence_missing: 证据不可读 {path}（{exc}）"],
                "criteria": [], "recorded": [], "metrics": {}, "source": str(path)}
    if not raw.strip():
        return {"verdict": L.V_RED, "violations": [f"evidence_missing: 证据为空文件 {path}"],
                "criteria": [], "recorded": [], "metrics": {}, "source": str(path)}
    try:
        data = json.loads(raw)
    except ValueError as exc:
        return {"verdict": L.V_RED,
                "violations": [f"evidence_missing: 证据不可解析 {path}（{exc}）"],
                "criteria": [], "recorded": [], "metrics": {}, "source": str(path)}
    return L.adjudicate(data, thresholds=thresholds, source=str(path))


def _line(tag: str, name: str, got: str, want: str, ok: bool, detail: str = "") -> dict:
    return {"case": tag, "name": name, "got": got, "want": want, "ok": ok,
            "detail": detail}


def self_test() -> int:
    repo = REPO
    try:
        th = L.load_thresholds(repo)
    except L.ThresholdError as exc:
        print(f"FROZEN_GATE_SELFTEST_FAIL: 阈值合同不可用（fail-closed）：{exc}",
              file=sys.stderr)
        return EXIT_UNAVAILABLE
    cases: list[dict] = []

    def expect(tag: str, name: str, result: dict, want_verdict: str,
               want_code: str | None = None) -> None:
        got = result.get("verdict")
        joined = " ".join(result.get("violations") or [])
        ok = got == want_verdict and (want_code is None or want_code in joined)
        cases.append(_line(tag, name, got, want_verdict, ok,
                           "" if ok else f"violations={result.get('violations')}"))

    # G1/G2 正例：合规证据必须绿（利用率可评估 + 分母已声明）
    expect("G1", "合规满负荷证据 -> pass", L.adjudicate(
        _synthetic_good(), thresholds=th, source="synthetic-good"), L.V_PASS)
    expect("G2", "分母未声明但 require_evaluable=False -> 记录不裁决（契约口径）",
           L.adjudicate(_synthetic_good(), thresholds=th, require_evaluable=False,
                        source="synthetic-undeclared"),
           L.V_PASS)
    # N1 违规证据（D-10 复现）：四条判据全违规 -> red
    n1 = L.adjudicate(_synthetic_violating(), thresholds=th, source="synthetic-bad")
    expect("N1", "四条冻结判据全违规 -> red", n1, L.V_RED, "avg_utilization_ge_min")
    cases.append(_line("N1b", "四条判据逐条登记违规",
                       str(len(n1.get("criteria", []))), "4",
                       len(n1.get("criteria", [])) == 4
                       and sum(1 for c in n1["criteria"] if not c["ok"]) == 4))
    # N2 缺证据 / N3 坏证据 / N4 空文件 / N5 不可解析（走 CLI 文件路径）
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = pathlib.Path(tmp)
        empty = tmpdir / "empty.json"
        empty.write_text("", encoding="utf-8")
        broken = tmpdir / "broken.json"
        broken.write_text("{not json", encoding="utf-8")
        expect("N2", "缺失证据文件 -> red(fail-closed)",
               judge_file(tmpdir / "absent.json", th), L.V_RED, "evidence_missing")
        expect("N3", "坏证据（frozen_gate 非法） -> red",
               L.adjudicate({"cpu_samples": [], "frozen_gate": "pass"},
                            thresholds=th, source="bad-gate"),
               L.V_RED, "evidence_missing")
        expect("N4", "空文件（无输出） -> red",
               judge_file(empty, th), L.V_RED, "evidence_missing")
        expect("N5", "不可解析 JSON -> red",
               judge_file(broken, th), L.V_RED, "evidence_missing")
    # N6 门不适用（L2 验收证据必须适用）
    short = _synthetic_good(interval=5.0)
    short["frozen_gate"]["metrics"]["interval_seconds"] = 5.0
    expect("N6", "L2 证据落在门适用域外 -> red",
           L.adjudicate(short, thresholds=th, source="short"), L.V_RED,
           "gate_not_applicable")
    cases.append(_line("N6b", "同一证据 require_applicable=False -> not_applicable（分类非豁免）",
                       L.adjudicate(short, thresholds=th, require_applicable=False,
                                    source="short")["verdict"], L.V_NOT_APPLICABLE,
                       L.adjudicate(short, thresholds=th, require_applicable=False,
                                    source="short")["verdict"] == L.V_NOT_APPLICABLE))
    # N7 分母未声明 + L2 语义 -> red（同一证据在 require_evaluable=False 下为记录项）
    undeclared = _synthetic_good()
    undeclared["frozen_gate"]["metrics"]["allocated"] = 0
    undeclared["frozen_gate"]["metrics"]["utilization_evaluated"] = False
    expect("N7", "L2 证据分母未声明 -> red",
           L.adjudicate(undeclared, thresholds=th, source="undeclared"),
           L.V_RED, "l2_denominator_undeclared")
    expect("N7b", "同一证据 require_evaluable=False -> 记入 recorded 不裁决（契约口径）",
           L.adjudicate(undeclared, thresholds=th, require_evaluable=False,
                        source="undeclared"),
           L.V_PASS)
    # N8 无 CPU 采样且无 frozen_gate -> red
    expect("N8", "既无采样也无判定 -> red",
           L.adjudicate({"duration_seconds": 60.0}, thresholds=th, source="no-evidence"),
           L.V_RED, "evidence_missing")
    # N9 阈值合同缺失 -> ThresholdError（不回落字面量）
    with tempfile.TemporaryDirectory() as tmp:
        try:
            L.load_thresholds(tmp)
            cases.append(_line("N9", "阈值合同缺失 -> ThresholdError", "no-raise",
                               "ThresholdError", False))
        except L.ThresholdError:
            cases.append(_line("N9", "阈值合同缺失 -> ThresholdError", "ThresholdError",
                               "ThresholdError", True))

    passed = sum(1 for c in cases if c["ok"])
    for c in cases:
        print(f"  [{c['case']}] {c['name']}: got={c['got']} want={c['want']} "
              f"{'OK' if c['ok'] else 'MISMATCH ' + c['detail']}")
    print(f"FROZEN_GATE_SELFTEST: cases={len(cases)} passed={passed} "
          f"failed={len(cases) - passed}")
    if passed != len(cases):
        print("FROZEN_GATE_SELFTEST_FAIL", file=sys.stderr)
        return EXIT_RED
    print("FROZEN_GATE_SELFTEST_PASS: 正例绿 / 负例红（缺失证据、坏证据、空文件、"
          "门不适用、分母未声明、合同缺失）双向成立")
    return EXIT_PASS


def replay(pattern: str, json_out: str | None) -> int:
    try:
        th = L.load_thresholds(REPO)
    except L.ThresholdError as exc:
        print(f"FROZEN_GATE_REPLAY_FAIL: 阈值合同不可用（fail-closed）：{exc}",
              file=sys.stderr)
        return EXIT_UNAVAILABLE
    paths = sorted(pathlib.Path(p) for p in _glob.glob(str(REPO / pattern)))
    if not paths:
        print(f"FROZEN_GATE_REPLAY_FAIL: 回放语料为空（{pattern}）-> fail-closed",
              file=sys.stderr)
        return EXIT_UNAVAILABLE
    rows, failures = [], []
    for path in paths:
        rel = path.relative_to(REPO).as_posix()
        verdict = judge_file(path, th)
        try:
            hist = (json.loads(path.read_text(encoding="utf-8")).get("frozen_gate")
                    or {}).get("verdict")
        except (OSError, ValueError):
            hist = None
        row = {
            "evidence": rel,
            "historical_verdict": hist,
            "l2_verdict": verdict["verdict"],
            "violations": verdict.get("violations", []),
            "criteria": verdict.get("criteria", []),
            "metrics": verdict.get("metrics", {}),
        }
        rows.append(row)
        print(f"  {rel}: 历史 verdict={hist!r} -> L2 门 verdict={verdict['verdict']!r} "
              f"违规 {len(verdict.get('violations', []))} 条")
        if hist == "pass" and verdict["verdict"] != L.V_RED:
            failures.append(f"{rel}: 历史 pass 且违规证据未被判红（恒真门残留）")
        if hist == "pass" and not verdict.get("violations"):
            failures.append(f"{rel}: 历史 pass 但裁决未给出任何违规条目")
    passed_hist = [r for r in rows if r["historical_verdict"] == "pass"]
    red_now = [r for r in passed_hist if r["l2_verdict"] == L.V_RED]
    summary = {
        "pattern": pattern,
        "evidence_count": len(rows),
        "historical_pass_count": len(passed_hist),
        "historical_pass_now_red": len(red_now),
        "rows": rows,
        "failures": failures,
    }
    if json_out:
        out = pathlib.Path(json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(summary, ensure_ascii=False, indent=1),
                       encoding="utf-8")
    if failures:
        print("FROZEN_GATE_REPLAY_FAIL:")
        for f in failures:
            print("  " + f)
        return EXIT_RED
    print(f"FROZEN_GATE_REPLAY_PASS: 回放 {len(rows)} 份归档 L2 证据；"
          f"历史 verdict=pass 的 {len(passed_hist)} 份现全部判红"
          f"（{len(red_now)}/{len(passed_hist)}）——恒真门已改真判红")
    return EXIT_PASS


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="L2 冻结判据裁决器（fail-closed，含红绿自测）")
    ap.add_argument("--evidence", action="append", default=[], metavar="PATH",
                    help="待裁决的监控证据 JSON（可多次）")
    ap.add_argument("--replay", nargs="?", const=DEFAULT_REPLAY_GLOB, default=None,
                    metavar="GLOB", help=f"回放归档 L2 证据（默认 {DEFAULT_REPLAY_GLOB}）")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="红/绿双向自测")
    ap.add_argument("--json-out", default=None, metavar="PATH",
                    help="裁决/回放结果 JSON 输出路径")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()
    if args.replay is not None:
        return replay(args.replay, args.json_out)
    if not args.evidence:
        ap.error("需要 --evidence / --replay / --self-test 之一")
    try:
        th = L.load_thresholds(REPO)
    except L.ThresholdError as exc:
        print(f"FROZEN_GATE_FAIL: 阈值合同不可用（fail-closed）：{exc}", file=sys.stderr)
        return EXIT_UNAVAILABLE
    failures = []
    for rel in args.evidence:
        result = judge_file(pathlib.Path(rel), th)
        print(f"  {rel}: verdict={result['verdict']!r} "
              f"violations={result.get('violations')}")
        if result["verdict"] != L.V_PASS:
            failures.append(f"{rel}: {result.get('violations')}")
    if failures:
        print("FROZEN_GATE_FAIL:")
        for f in failures:
            print("  " + f)
        return EXIT_RED
    print(f"FROZEN_GATE_PASS: {len(args.evidence)} 份证据四条冻结判据全达标")
    return EXIT_PASS


if __name__ == "__main__":
    raise SystemExit(main())
