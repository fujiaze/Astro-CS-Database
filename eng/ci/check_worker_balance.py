#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/check_worker_balance.py — worker_balance 利用率指标的判别力门（GATE-501）。

背景（RELEASE-04 偏差 D-10 第二半 / GAP_AUDIT G2-1）
  artifacts/acceptance/l2_performance/worker_balance/*.csv 的 utilization_pct
  在 11 个 run、680 行上**恒为 50.00**，零判别力。根因（域外，已登记派单）：
    * lib/infrastructure/cli/commands.cpp 调 recorder.set_workers(budget, budget)
      -> active_workers 与 runnable_workers 同源同值；
    * lib/infrastructure/cli/resource_recorder.h 的
      util = active*100/(active+runnable) -> 两字段同值即恒 50.00。
  正确算法（本文件实现，供 CI 侧复算与判别力裁决）：
      每个采样窗  利用率 = 忙碌 worker 数 / 已分配 worker 数
      忙碌 worker 数 = cpu_pct / 100（cpu_pct 口径 = 单核百分比）
      即 utilization_pct = cpu_pct / allocated_workers
  权威采样面是 resource_timeseries.csv（cpu_pct/active_workers/runnable_workers）
  与 gate JSON 的 cpu_samples；worker_balance.csv 只是派生产物。

判据（fail-closed）
  * 证据文件缺失 / 空 / 不可解析 / 缺表头      -> red
  * 派生列 utilization_pct 与实际算法不符      -> red（wrong_algorithm）
  * active_workers 与 runnable_workers 逐行同源同值 -> red（degenerate_input）
  * 利用率序列恒定（无判别力）                  -> red（constant_metric）
  * 复算序列出现 >=2 个不同取值                 -> 绿（有判别力）

用法:
  python3 eng/ci/check_worker_balance.py --self-test
  python3 eng/ci/check_worker_balance.py --timeseries artifacts/.../real16_w16_resource_timeseries.csv --workers 16
  python3 eng/ci/check_worker_balance.py --balance-csv artifacts/.../real16_w16_worker_balance.csv --workers 16
  python3 eng/ci/check_worker_balance.py --replay-archived --json-out run/ci/worker-balance/replay.json
exit 0 = 通过；1 = 判红；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import csv
import glob as _glob
import io
import json
import pathlib
import statistics
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
L2_DIR = "artifacts/acceptance/l2_performance"
EXIT_PASS, EXIT_RED, EXIT_UNAVAILABLE = 0, 1, 2
BALANCE_COLUMNS = ("elapsed_seconds", "active_workers", "runnable_workers",
                   "utilization_pct")


def _red(code: str, detail: str, **extra) -> dict:
    out = {"verdict": "red", "violations": [f"{code}: {detail}"], "metrics": {}}
    out.update(extra)
    return out


def recompute_utilization(cpu_pct: float, workers: int) -> float:
    """正确算法：utilization_pct = 忙碌 worker 数 / 已分配 worker 数 * 100。

    cpu_pct 口径 = 单核百分比（resource_summary.json cpu_pct_units =
    percent_of_one_core），故忙碌 worker 数 = cpu_pct / 100。
    """
    if workers <= 0:
        raise ValueError("workers 必须为正（已分配容量未声明 -> 不可算）")
    return cpu_pct / 100.0 / workers * 100.0


def check_series(rows: list, *, source: str, workers: int,
                 reported: list | None = None) -> dict:
    """对一组采样行做判别力 + 算法一致性裁决（rows = [(busy_or_cpu, runnable)]）。"""
    if workers <= 0:
        return _red("allocated_undeclared",
                    f"{source}: 已分配 worker 数未声明/非法（{workers}）", source=source)
    if not rows:
        return _red("no_output", f"{source}: 无任何采样行（无输出 -> fail-closed）",
                    source=source)
    utils = [recompute_utilization(cpu, workers) for cpu, _ in rows]
    distinct = sorted({round(u, 6) for u in utils})
    metrics = {
        "source": source,
        "workers": workers,
        "samples": len(utils),
        "distinct_values": len(distinct),
        "utilization_min": round(min(utils), 4),
        "utilization_max": round(max(utils), 4),
        "utilization_mean": round(statistics.fmean(utils), 4),
        "utilization_stdev": round(statistics.pstdev(utils), 4) if len(utils) > 1 else 0.0,
        "first_values": [round(u, 4) for u in utils[:5]],
    }
    violations = []
    if len(distinct) < 2:
        violations.append(
            f"constant_metric: {source} 复算利用率恒为 {distinct[0]:.4f}%"
            f"（{len(utils)} 样本零判别力）")
    if reported is not None:
        mismatches = [(i, r, round(u, 2)) for i, (r, u) in enumerate(zip(reported, utils))
                      if r is None or abs(float(r) - u) > 0.01]
        metrics["reported_mismatch_rows"] = len(mismatches)
        if mismatches:
            violations.append(
                f"wrong_algorithm: {source} 派生列 utilization_pct 与正确算法不符"
                f"（首例 row={mismatches[0][0]} reported={mismatches[0][1]} "
                f"correct={mismatches[0][2]}；共 {len(mismatches)} 行）")
    return {"verdict": "red" if violations else "pass", "violations": violations,
            "metrics": metrics, "source": source}


def check_balance_csv(path: pathlib.Path, workers: int) -> dict:
    """worker_balance.csv：算法一致性 + 输入同源退化检测 + 判别力。"""
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        return _red("evidence_missing", f"worker_balance.csv 不可读 {path}（{exc}）")
    if not text.strip():
        return _red("no_output", f"worker_balance.csv 为空文件 {path}")
    reader = csv.DictReader(io.StringIO(text))
    header = tuple(reader.fieldnames or ())
    if not set(BALANCE_COLUMNS).issubset(set(header)):
        return _red("bad_evidence",
                    f"worker_balance.csv 表头非法：{header}（期望含 {BALANCE_COLUMNS}）")
    rows, reported, actives, runnables = [], [], [], []
    for row in reader:
        try:
            active = float(row["active_workers"])
            runnable = float(row["runnable_workers"])
            reported.append(float(row["utilization_pct"]))
        except (TypeError, ValueError):
            return _red("bad_evidence", f"worker_balance.csv 数据行非法：{row}")
        actives.append(active)
        runnables.append(runnable)
        rows.append((active, runnable))
    result = check_series(rows, source=str(path), workers=workers, reported=reported)
    same_source = all(a == r for a, r in zip(actives, runnables))
    result["metrics"]["active_equals_runnable_all_rows"] = same_source
    if same_source:
        result["violations"].append(
            "degenerate_input: active_workers 与 runnable_workers 逐行同值 "
            f"（{len(actives)} 行）——两列同源，利用率不可由该文件判定"
            "（域外根因：commands.cpp set_workers(budget, budget) + "
            "resource_recorder.h util=active/(active+runnable)）")
        result["verdict"] = "red"
    return result


def check_timeseries_csv(path: pathlib.Path, workers: int | None) -> dict:
    """resource_timeseries.csv：按正确算法复算利用率（权威采样面）。"""
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        return _red("evidence_missing", f"resource_timeseries.csv 不可读 {path}（{exc}）")
    if not text.strip():
        return _red("no_output", f"resource_timeseries.csv 为空文件 {path}")
    reader = csv.DictReader(io.StringIO(text))
    header = tuple(reader.fieldnames or ())
    if "cpu_pct" not in header:
        return _red("bad_evidence", f"resource_timeseries.csv 缺 cpu_pct 列：{header}")
    rows, observed_workers = [], 0.0
    for row in reader:
        if (row.get("stage") or "active") not in ("active", ""):
            continue
        try:
            cpu = float(row["cpu_pct"])
        except (TypeError, ValueError):
            continue
        rows.append((cpu, 0.0))
        try:
            observed_workers = max(observed_workers, float(row.get("active_workers") or 0))
        except (TypeError, ValueError):
            pass
    effective_workers = workers if workers else int(observed_workers)
    result = check_series(rows, source=str(path), workers=effective_workers)
    result["metrics"]["workers_source"] = "cli" if workers else "active_workers_max"
    return result


def check_gate_json(path: pathlib.Path, workers: int | None) -> dict:
    """gate JSON：由 cpu_samples 复算利用率（与 L2 冻结判据同一采样面）。"""
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        return _red("evidence_missing", f"gate 证据不可读 {path}（{exc}）")
    except ValueError as exc:
        return _red("bad_evidence", f"gate 证据不可解析 {path}（{exc}）")
    samples = data.get("cpu_samples") or []
    if not isinstance(samples, list) or not samples:
        return _red("no_output", f"gate 证据无 cpu_samples（无输出）{path}")
    allocated = workers
    if not allocated:
        gate_metrics = ((data.get("frozen_gate") or {}).get("metrics") or {})
        allocated = int(gate_metrics.get("allocated") or 0)
    rows = []
    for s in samples:
        cpu = s.get("cpu_percent")
        if isinstance(cpu, bool) or not isinstance(cpu, (int, float)):
            continue
        rows.append((float(cpu), 0.0))
    result = check_series(rows, source=str(path), workers=int(allocated or 0))
    result["metrics"]["workers_source"] = "cli" if workers else "frozen_gate.metrics.allocated"
    return result


def self_test() -> int:
    cases: list[dict] = []

    def expect(tag: str, name: str, got: str, want: str, detail: str = "") -> None:
        cases.append({"case": tag, "name": name, "got": got, "want": want,
                      "ok": got == want, "detail": detail})

    # 合成负载 A / B：两组不同负载必须给出不同且非常数的利用率输出
    load_a = [(100.0, 0.0), (200.0, 0.0), (300.0, 0.0), (400.0, 0.0)]   # 4 worker
    load_b = [(800.0, 0.0), (400.0, 0.0), (0.0, 0.0), (200.0, 0.0)]     # 8 worker
    ra = check_series(load_a, source="synthetic-load-A", workers=4)
    rb = check_series(load_b, source="synthetic-load-B", workers=8)
    expect("G1", "合成负载 A（4 worker，100/200/300/400% 单核）-> 非常数",
           ra["verdict"], "pass", str(ra["violations"]))
    expect("G2", "合成负载 B（8 worker，800/400/0/200% 单核）-> 非常数",
           rb["verdict"], "pass", str(rb["violations"]))
    expect("G3", "两组负载输出不同（A != B）",
           "different" if ra["metrics"]["first_values"] != rb["metrics"]["first_values"]
           else "same", "different",
           f"A={ra['metrics']['first_values']} B={rb['metrics']['first_values']}")
    cases.append({"case": "G3b", "name": "A/B 利用率序列回显",
                  "got": f"A={ra['metrics']['first_values']} B={rb['metrics']['first_values']}",
                  "want": "非恒定且互不相同", "ok": True, "detail": ""})

    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = pathlib.Path(tmp)
        # N1 归档退化件复刻：active==runnable==16、util 恒 50.00
        degenerate = tmpdir / "degenerate_worker_balance.csv"
        degenerate.write_text(
            "elapsed_seconds,active_workers,runnable_workers,utilization_pct\n"
            + "".join(f"{i * 0.5:.3f},16,16,50.00\n" for i in range(6)), encoding="utf-8")
        r = check_balance_csv(degenerate, 16)
        expect("N1", "退化 worker_balance.csv（同源 16/16 恒 50.00）-> red",
               r["verdict"], "red", str(r["violations"]))
        cases.append({"case": "N1b", "name": "退化根因被点名（degenerate_input + constant_metric）",
                      "got": ",".join(sorted({v.split(":")[0] for v in r["violations"]})),
                      "want": "constant_metric,degenerate_input",
                      "ok": {"constant_metric", "degenerate_input"}
                      <= {v.split(":")[0] for v in r["violations"]}, "detail": ""})
        # N2 算法不符：reported 50.00 但正确算法给 100.00
        wrong = tmpdir / "wrong_worker_balance.csv"
        wrong.write_text(
            "elapsed_seconds,active_workers,runnable_workers,utilization_pct\n"
            "0.000,16,1,50.00\n0.500,8,1,50.00\n", encoding="utf-8")
        r = check_balance_csv(wrong, 16)
        expect("N2", "派生列与正确算法不符 -> red(wrong_algorithm)", r["verdict"], "red",
               str(r["violations"]))
        # N3 缺证据 / N4 空文件 / N5 坏表头
        expect("N3", "证据文件缺失 -> red(fail-closed)",
               check_balance_csv(tmpdir / "absent.csv", 16)["verdict"], "red")
        empty = tmpdir / "empty.csv"
        empty.write_text("", encoding="utf-8")
        expect("N4", "空文件（无输出）-> red",
               check_balance_csv(empty, 16)["verdict"], "red")
        bad = tmpdir / "bad.csv"
        bad.write_text("a,b\n1,2\n", encoding="utf-8")
        expect("N5", "坏表头 -> red(bad_evidence)",
               check_balance_csv(bad, 16)["verdict"], "red")
        # N6 分母未声明 -> red
        expect("N6", "已分配 worker 未声明 -> red(allocated_undeclared)",
               check_series([(100.0, 0.0)], source="x", workers=0)["verdict"], "red")
        # N7 常量时间序列 -> red（判别力判据本身能红）
        const = tmpdir / "const_timeseries.csv"
        const.write_text("elapsed_seconds,stage,cpu_pct,active_workers\n"
                         + "".join(f"{i},active,400.0,4\n" for i in range(5)),
                         encoding="utf-8")
        expect("N7", "时间序列利用率恒定 -> red(constant_metric)",
               check_timeseries_csv(const, 4)["verdict"], "red")

    passed = sum(1 for c in cases if c["ok"])
    for c in cases:
        print(f"  [{c['case']}] {c['name']}: got={c['got']} want={c['want']} "
              f"{'OK' if c['ok'] else 'MISMATCH ' + c['detail']}")
    print(f"WORKER_BALANCE_SELFTEST: cases={len(cases)} passed={passed} "
          f"failed={len(cases) - passed}")
    if passed != len(cases):
        print("WORKER_BALANCE_SELFTEST_FAIL", file=sys.stderr)
        return EXIT_RED
    print("WORKER_BALANCE_SELFTEST_PASS: 正确算法两组负载输出不同且非常数；"
          "退化/算法不符/缺失/空/坏表头/未声明分母/恒定序列全部判红")
    return EXIT_PASS


def replay_archived(json_out: str | None) -> int:
    """回放归档 L2 证据：退化 worker_balance 必须判红；权威采样面复算必须非常数。"""
    bal = sorted(_glob.glob(str(REPO / L2_DIR / "worker_balance" / "*_worker_balance.csv")))
    ts = sorted(_glob.glob(str(REPO / L2_DIR / "timeseries" / "*_resource_timeseries.csv")))
    if not bal or not ts:
        print(f"WORKER_BALANCE_REPLAY_FAIL: 归档语料缺失（{L2_DIR}）-> fail-closed",
              file=sys.stderr)
        return EXIT_UNAVAILABLE
    rows, failures = [], []
    for p in bal:
        path = pathlib.Path(p)
        workers = 16 if "_w16" in path.name else (4 if "_w4" in path.name else 1)
        r = check_balance_csv(path, workers)
        rows.append({"evidence": path.relative_to(REPO).as_posix(), "kind": "worker_balance",
                     "verdict": r["verdict"], "metrics": r["metrics"],
                     "violations": r["violations"]})
        if r["verdict"] != "red":
            failures.append(f"{path.name}: 退化派生件未被判红（判别力门失效）")
    for p in ts:
        path = pathlib.Path(p)
        workers = 16 if "_w16" in path.name else (4 if "_w4" in path.name else 1)
        r = check_timeseries_csv(path, workers)
        rows.append({"evidence": path.relative_to(REPO).as_posix(), "kind": "timeseries",
                     "verdict": r["verdict"], "metrics": r["metrics"],
                     "violations": r["violations"]})
        if r["verdict"] != "pass":
            failures.append(f"{path.name}: 权威采样面复算未通过：{r['violations']}")
        print(f"  {path.name}: workers={workers} "
              f"distinct={r['metrics'].get('distinct_values')} "
              f"min={r['metrics'].get('utilization_min')} "
              f"max={r['metrics'].get('utilization_max')}")
    summary = {"worker_balance_degenerate": len(bal), "timeseries_recomputed": len(ts),
               "rows": rows, "failures": failures}
    if json_out:
        out = pathlib.Path(json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(summary, ensure_ascii=False, indent=1), encoding="utf-8")
    if failures:
        print("WORKER_BALANCE_REPLAY_FAIL:")
        for f in failures:
            print("  " + f)
        return EXIT_RED
    print(f"WORKER_BALANCE_REPLAY_PASS: {len(bal)} 份退化派生件全部判红；"
          f"{len(ts)} 份权威时间序列按正确算法复算全部非常数")
    return EXIT_PASS


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="worker_balance 利用率指标判别力门")
    ap.add_argument("--balance-csv", action="append", default=[], metavar="PATH")
    ap.add_argument("--timeseries", action="append", default=[], metavar="PATH")
    ap.add_argument("--gate", action="append", default=[], metavar="PATH")
    ap.add_argument("--workers", type=int, default=None,
                    help="已分配 worker 数（缺省由证据自解析）")
    ap.add_argument("--replay-archived", action="store_true", dest="replay",
                    help="回放归档 L2 证据（退化件必须判红）")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    ap.add_argument("--json-out", default=None, metavar="PATH")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()
    if args.replay:
        return replay_archived(args.json_out)
    if not (args.balance_csv or args.timeseries or args.gate):
        ap.error("需要 --balance-csv / --timeseries / --gate / --replay-archived / --self-test")
    failures, rows = [], []
    for rel in args.balance_csv:
        r = check_balance_csv(pathlib.Path(rel), args.workers or 0)
        rows.append(r)
        print(f"  {rel}: verdict={r['verdict']!r} {r.get('violations')}")
        if r["verdict"] != "pass":
            failures.append(f"{rel}: {r.get('violations')}")
    for rel in args.timeseries:
        r = check_timeseries_csv(pathlib.Path(rel), args.workers)
        rows.append(r)
        print(f"  {rel}: verdict={r['verdict']!r} metrics={r.get('metrics')}")
        if r["verdict"] != "pass":
            failures.append(f"{rel}: {r.get('violations')}")
    for rel in args.gate:
        r = check_gate_json(pathlib.Path(rel), args.workers)
        rows.append(r)
        print(f"  {rel}: verdict={r['verdict']!r} metrics={r.get('metrics')}")
        if r["verdict"] != "pass":
            failures.append(f"{rel}: {r.get('violations')}")
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"rows": rows, "failures": failures},
                                  ensure_ascii=False, indent=1), encoding="utf-8")
    if failures:
        print("WORKER_BALANCE_FAIL:")
        for f in failures:
            print("  " + f)
        return EXIT_RED
    print(f"WORKER_BALANCE_PASS: {len(rows)} 份证据利用率序列有判别力且算法一致")
    return EXIT_PASS


if __name__ == "__main__":
    raise SystemExit(main())
