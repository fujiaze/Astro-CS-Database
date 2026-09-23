#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PERF-501 节点级瀑布 + 逐节点并行宽度（补 resource_probe/resource_monitor 的空白）。

职责
----
现有探针只给**进程级**曲线：
  · resource_probe.py      —— 可用资源/并发容量探测（affinity ∩ cgroup，无时间轴）；
  · resource_recorder.h    —— 进程内 /proc 采样（进程级，无节点归属）；
  · resource_monitor.py    —— 进程树外部采样（进程级，无节点归属）；
  · run_monitored.py       —— 同一采样面 + G-RES-01 冻结门判定（无节点归属）。
四者都回答不了「各节点各花了多少墙钟、每个节点真正有几条线程在算」。
本工具把调度器已有的 env-gated 观测行（**零新增埋点**）解析成瀑布表：

  [nodetrace] BEGIN <node> <steady_clock_s>     (module_adapters.cpp P10-UTIL2-006)
  [nodetrace] END   <node> <elapsed_s>          (同上; END 行给的是**时长**)
  [lease]     <node> host_workers=%u acquired=%d cap=%u budget_available=%u
                                                (module_adapters.cpp P7-UTIL-001)
  [p1cap]     frame_workers node=%s lease=%u memory_cap=%u frame_workers=%u ...
  [p1cap]     parallel_for  node=%s lease=%u memory_cap=%u frame_workers=%u n_units=%llu inner_omp=%u
                                                (PERF-501 并行轴分配快照)

并把时序曲线按时间轴对齐后归因到节点窗口：节点窗口内的 CPU 均值（等效核 ×100）、
并行宽度（活跃计算线程数）、系统级 io_wait%、read/write 字节。

时间轴对齐
----------
C++ 侧 steady_clock 在 libstdc++/Linux 上 = clock_gettime(CLOCK_MONOTONIC)，与 Python
time.monotonic() 同源 ⇒ wall(steady) = steady + (time.time() - time.monotonic())。
曲线起点 t0 的墙钟由「CSV mtime − 末样本 t」推定（曲线在 run 末尾才落盘）。
对齐误差上限 = 采样周期，在报告里显式回显；两条曲线都不可得时并行宽度列标 n/a，
**不冒充有数据**。

用法
----
  node_waterfall.py --log <run.stderr> --timeseries <resource_timeseries.csv> [--out-dir DIR]
  node_waterfall.py --monitor <run_monitored.json>          # 只有节点墙钟，宽度 n/a
  node_waterfall.py --self-test

边界
----
· 只读；不改任何被测进程行为；
· 缺 [nodetrace] 行 ⇒ 明确报 "no_nodetrace"（不静默返回空表）。
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
import time
from pathlib import Path

__all__ = ["parse_trace_lines", "load_timeseries", "attribute_windows", "render",
           "self_test"]

_RE_BEGIN = re.compile(r"^\[nodetrace\] BEGIN (\S+) ([0-9.]+)\s*$")
_RE_END = re.compile(r"^\[nodetrace\] END (\S+) ([0-9.]+)\s*$")
_RE_LEASE = re.compile(
    r"^\[lease\] (\S+) host_workers=(\d+) acquired=(\d+) cap=(\d+) budget_available=(\d+)")
_RE_P1CAP = re.compile(
    r"^\[p1cap\] (\S+) node=(\S+) lease=(\d+) memory_cap=(\d+) frame_workers=(\d+) "
    r"n_units=(\d+) inner_omp=(\d+)")


def parse_trace_lines(lines):
    """解析观测行 → (nodes, leases, p1caps)。

    nodes: [{node, begin_s, wall_s}]，按 begin_s 升序。BEGIN 给绝对 steady 时刻，
    END 给时长 ⇒ wall_s = END 值（END 缺失则该节点 wall_s = None，如实标注）。
    """
    nodes, leases, p1caps = [], {}, {}
    for raw in lines:
        line = raw.rstrip("\n")
        m = _RE_BEGIN.match(line)
        if m:
            nodes.append({"node": m.group(1), "begin_s": float(m.group(2)),
                          "wall_s": None})
            continue
        m = _RE_END.match(line)
        if m:
            node, dur = m.group(1), float(m.group(2))
            for nd in reversed(nodes):
                if nd["node"] == node and nd["wall_s"] is None:
                    nd["wall_s"] = dur
                    break
            continue
        m = _RE_LEASE.match(line)
        if m:
            leases[m.group(1)] = {
                "host_workers": int(m.group(2)), "acquired": int(m.group(3)),
                "cap": int(m.group(4)), "budget_available": int(m.group(5)),
            }
            continue
        m = _RE_P1CAP.match(line)
        if m:
            rec = {"lease": int(m.group(3)), "memory_cap": int(m.group(4)),
                   "frame_workers": int(m.group(5)), "n_units": int(m.group(6)),
                   "inner_omp": int(m.group(7))}
            p1caps.setdefault(m.group(2), {})[m.group(1)] = rec
            continue
    nodes.sort(key=lambda d: d["begin_s"])
    return nodes, leases, p1caps


def load_timeseries(path):
    """读时序 CSV → (samples, meta)。

    samples: [{t, cpu_pct, width, io_wait_pct, read_bytes, write_bytes}]（时间升序）。
    支持两种既有格式（按表头判别，不猜）：
      · 进程内 recorder 的 resource_timeseries.csv:
        elapsed_seconds,cpu_pct,...,active_compute_threads,...,read_bytes,write_bytes,...,io_wait_pct
        （read/write 列已是**逐区间增量**）
      · 外挂 resource_monitor.py 的 samples.csv:
        t,iso_utc,dt,cpu_pct_core,...,busy_threads,...,read_bytes_per_s,...,io_wait_all_ms
        （rate 列 × dt 还原为增量）
    meta 含 t0_epoch：曲线 t=0 的墙钟，由「CSV mtime − 末样本 t」推定（曲线在 run
    末尾落盘；误差 ~ms 级，报告里与采样周期一起回显）。
    """
    rows = list(csv.DictReader(open(path, newline="", encoding="utf-8")))
    if not rows:
        return [], {"aligned": False, "reason": "empty_timeseries"}
    head = set(rows[0].keys())
    out = []
    if "elapsed_seconds" in head:  # 进程内 recorder
        fmt = "in_process_recorder"
        for r in rows:
            out.append({
                "t": float(r["elapsed_seconds"]),
                "cpu_pct": float(r.get("cpu_pct") or 0.0),
                "width": float(r.get("active_compute_threads") or 0.0),
                "io_wait_pct": float(r.get("io_wait_pct") or 0.0),
                "read_bytes": float(r.get("read_bytes") or 0.0),
                "write_bytes": float(r.get("write_bytes") or 0.0),
            })
    elif "cpu_pct_core" in head:  # 外挂 resource_monitor
        fmt = "external_resource_monitor"
        for r in rows:
            dt = float(r.get("dt") or 0.0)
            out.append({
                "t": float(r["t"]),
                "cpu_pct": float(r.get("cpu_pct_core") or 0.0),
                "width": float(r.get("busy_threads") or 0.0),
                "io_wait_pct": float(r.get("io_wait_all_ms") or 0.0) / 10.0,
                "read_bytes": float(r.get("read_bytes_per_s") or 0.0) * dt,
                "write_bytes": float(r.get("write_bytes_per_s") or 0.0) * dt,
            })
    else:
        return [], {"aligned": False,
                    "reason": "unknown_timeseries_schema: " + ",".join(sorted(head))}
    out.sort(key=lambda s: s["t"])
    t_last = out[-1]["t"]
    t0_epoch = os.path.getmtime(path) - t_last
    return out, {"aligned": True, "format": fmt, "t0_epoch": t0_epoch,
                 "t_last": t_last, "n_samples": len(out)}


def attribute_windows(nodes, samples, meta):
    """把时序样本按时间轴对齐后归因到各节点窗口（就地写回 nodes）。"""
    if not samples or not nodes or not meta.get("aligned"):
        return nodes, meta
    skew = time.time() - time.monotonic()   # steady(CLOCK_MONOTONIC) → wall
    t0_epoch = meta["t0_epoch"]
    meta = dict(meta)
    meta["skew_s"] = round(skew, 3)
    meta["alignment_error_bound_s"] = 0.5   # 采样周期（recorder 0.5 s / 外挂默认 1 s）
    for nd in nodes:
        t_beg = nd["begin_s"] + skew - t0_epoch
        t_end = t_beg + (nd["wall_s"] or 0.0)
        nd["t_begin_monitor_s"] = round(t_beg, 3)
        nd["t_end_monitor_s"] = round(t_end, 3)
        win = [s for s in samples if t_beg <= s["t"] <= t_end]
        if not win:
            nd["attribution"] = "no_samples_in_window"
            continue
        cpu = [s["cpu_pct"] for s in win]
        wid = [s["width"] for s in win]
        iow = [s["io_wait_pct"] for s in win]
        nd["attribution"] = "ok"
        nd["n_samples"] = len(win)
        nd["cpu_pct_mean"] = round(sum(cpu) / len(cpu), 2)
        nd["cpu_pct_max"] = round(max(cpu), 2)
        nd["width_p50"] = round(sorted(wid)[len(wid) // 2], 2)
        nd["width_max"] = round(max(wid), 2)
        nd["io_wait_pct_mean"] = round(sum(iow) / len(iow), 2)
        nd["read_bytes"] = int(sum(s["read_bytes"] for s in win))
        nd["write_bytes"] = int(sum(s["write_bytes"] for s in win))
    return nodes, meta


def render(nodes, leases, p1caps, meta, out_dir=None):
    """渲染 Markdown 瀑布表（stdout）；out_dir 给出时同时落盘 md + json。"""
    lines = []
    total = sum(nd["wall_s"] or 0.0 for nd in nodes)
    lines.append("# 节点级瀑布（PERF-501 node_waterfall.py）")
    lines.append("")
    if meta.get("aligned"):
        lines.append("- 时序格式: %s（%d 样本）；时间轴对齐: OK"
                     "（steady→wall skew=%.3f s，对齐误差上限 %.1f s = 采样周期）"
                     % (meta.get("format"), meta.get("n_samples", 0),
                        meta.get("skew_s", 0.0), meta.get("alignment_error_bound_s", 0.0)))
    else:
        lines.append("- 时间轴对齐: 未对齐（%s）⇒ 并行宽度列标 n/a"
                     % meta.get("reason"))
    lines.append("- 节点墙钟合计: %.1f s" % total)
    lines.append("")
    lines.append("| # | node | wall_s | %wall | t_begin(s) | cpu%均值 | 宽度p50 | 宽度max "
                 "| io_wait% | read_MB | write_MB | lease | mem_cap | frame_w | inner_omp |")
    lines.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for i, nd in enumerate(nodes, 1):
        cap = p1caps.get(nd["node"], {})
        fw = cap.get("frame_workers", {})
        pf = cap.get("parallel_for", {})

        def g(d, k):
            return d.get(k, "n/a") if d else "n/a"

        lines.append("| %d | %s | %.2f | %.1f%% | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
            i, nd["node"], nd["wall_s"] or 0.0,
            100.0 * (nd["wall_s"] or 0.0) / total if total else 0.0,
            nd.get("t_begin_monitor_s", "n/a"),
            nd.get("cpu_pct_mean", "n/a"), nd.get("width_p50", "n/a"),
            nd.get("width_max", "n/a"), nd.get("io_wait_pct_mean", "n/a"),
            ("%.1f" % (nd["read_bytes"] / 1e6)) if "read_bytes" in nd else "n/a",
            ("%.1f" % (nd["write_bytes"] / 1e6)) if "write_bytes" in nd else "n/a",
            g(fw, "lease"), g(fw, "memory_cap"), g(fw, "frame_workers"),
            g(pf, "inner_omp")))
    text = "\n".join(lines) + "\n"
    if out_dir:
        d = Path(out_dir)
        d.mkdir(parents=True, exist_ok=True)
        (d / "node_waterfall.md").write_text(text, encoding="utf-8")
        (d / "node_waterfall.json").write_text(json.dumps(
            {"nodes": nodes, "leases": leases, "p1caps": p1caps, "timeseries": meta},
            indent=2, ensure_ascii=False), encoding="utf-8")
    return text


def self_test():
    """--self-test: 合成观测日志 + 合成曲线，断言解析与归因口径（不依赖真实运行）。"""
    log = [
        "[nodetrace] BEGIN p1_op_calibrate 100.000000",
        "[lease] p1_op_calibrate host_workers=16 acquired=1 cap=16 budget_available=0",
        "[p1cap] frame_workers node=p1_op_calibrate lease=16 memory_cap=2 frame_workers=2 n_units=0 inner_omp=0",
        "[nodetrace] END p1_op_calibrate 3.500000",
        "[nodetrace] BEGIN p1_op_drizzle 103.500000",
        "[p1cap] parallel_for node=p1_op_drizzle lease=16 memory_cap=0 frame_workers=2 n_units=2 inner_omp=8",
        "[nodetrace] END p1_op_drizzle 250.000000",
    ]
    nodes, leases, p1caps = parse_trace_lines(log)
    ok = True
    ok &= len(nodes) == 2
    ok &= nodes[0]["node"] == "p1_op_calibrate" and abs(nodes[0]["wall_s"] - 3.5) < 1e-9
    ok &= nodes[1]["wall_s"] == 250.0
    ok &= leases["p1_op_calibrate"]["cap"] == 16
    ok &= p1caps["p1_op_drizzle"]["parallel_for"]["inner_omp"] == 8
    ok &= p1caps["p1_op_calibrate"]["frame_workers"]["memory_cap"] == 2
    # 无曲线 ⇒ 不冒充并行宽度
    nodes, meta = attribute_windows(nodes, [], {"aligned": False, "reason": "self_test"})
    ok &= nodes[0].get("cpu_pct_mean") is None
    txt = render(nodes, leases, p1caps, meta, None)
    ok &= "p1_op_drizzle" in txt and "n/a" in txt
    # 合成曲线归因: 窗口 [100,103.5] 落 3 个样本
    samples = [{"t": 0.0, "cpu_pct": 100.0, "width": 1.0, "io_wait_pct": 0.0,
                "read_bytes": 10.0, "write_bytes": 1.0},
               {"t": 1.0, "cpu_pct": 200.0, "width": 2.0, "io_wait_pct": 5.0,
                "read_bytes": 20.0, "write_bytes": 2.0},
               {"t": 3.0, "cpu_pct": 300.0, "width": 3.0, "io_wait_pct": 7.0,
                "read_bytes": 30.0, "write_bytes": 3.0},
               {"t": 4.0, "cpu_pct": 400.0, "width": 4.0, "io_wait_pct": 9.0,
                "read_bytes": 40.0, "write_bytes": 4.0}]
    skew = time.time() - time.monotonic()
    # t0_epoch 取 begin+1 ms ⇒ 窗口 [-0.001, 3.499] 恰好落 t=0/1/3 三个样本。
    meta2 = {"aligned": True, "t0_epoch": 100.0 + skew + 0.001,
             "format": "synthetic", "n_samples": 4}
    n2, _, _ = parse_trace_lines(log)
    n2, meta2 = attribute_windows(n2, samples, meta2)
    ok &= n2[0]["attribution"] == "ok" and n2[0]["n_samples"] == 3
    ok &= abs(n2[0]["cpu_pct_mean"] - 200.0) < 1e-6
    ok &= n2[0]["width_max"] == 3.0
    ok &= n2[0]["read_bytes"] == 60
    print("self_test:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="节点级瀑布 + 逐节点并行宽度")
    ap.add_argument("--log", default=None, help="含 [nodetrace]/[lease]/[p1cap] 的日志文件")
    ap.add_argument("--monitor", default=None, help="run_monitored.py 的 JSON 证据（取 stderr_tail）")
    ap.add_argument("--timeseries", default=None,
                    help="时序曲线 CSV（进程内 resource_timeseries.csv 或外挂 samples.csv）")
    ap.add_argument("--out-dir", default=None, help="产物目录（写 node_waterfall.{md,json}）")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    lines = None
    if args.log:
        lines = Path(args.log).read_text(encoding="utf-8", errors="replace").splitlines()
    if args.monitor:
        monitor = json.loads(Path(args.monitor).read_text(encoding="utf-8"))
        if lines is None:
            lines = list(monitor.get("stderr_tail") or [])
    if lines is None:
        print("node_waterfall: 需要 --log 或 --monitor（或 --self-test）", file=sys.stderr)
        return 2
    nodes, leases, p1caps = parse_trace_lines(lines)
    if not nodes:
        print("node_waterfall: no_nodetrace（日志里没有 [nodetrace] 行；"
              "请以 ASTROCS_NODE_TRACE=1 运行）", file=sys.stderr)
        return 3
    samples, meta = ([], {"aligned": False, "reason": "no_timeseries"})
    if args.timeseries:
        samples, meta = load_timeseries(args.timeseries)
    nodes, meta = attribute_windows(nodes, samples, meta)
    sys.stdout.write(render(nodes, leases, p1caps, meta, args.out_dir))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
