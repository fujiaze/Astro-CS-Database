#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""resource_gate_probe.py — RESOURCE-GATE-REAL 的被监控「重计算」探针。

用途：给 `ci/resource_monitor.py --gate-required` 提供一个可复现的真实计算区间，
使资源利用率门在 CI 里有真实观测量（而不是只跑静态检查）。

- `--mode busy`  : N 进程并行紧浮点循环 ⇒ 真实多核重计算区间（门应判 pass）；
- `--mode serial`: 单进程空转 ⇒ 与「重计算面」定义相反（门应判 fail，构成负例面）。

本探针不含科学语义，只用于资源门自证。
"""
from __future__ import annotations

import argparse
import multiprocessing as mp
import os
import time


def _burn(stop_ts: float) -> float:
    x = 1.000001
    while time.time() < stop_ts:
        for _ in range(20000):
            x = x * 1.0000001 + 1e-9
    return x


def main() -> int:
    ap = argparse.ArgumentParser(description="资源门真实利用率探针")
    ap.add_argument("--mode", choices=("busy", "serial"), default="busy")
    ap.add_argument("--workers", type=int, default=0, help="0 = 取 os.cpu_count()")
    ap.add_argument("--seconds", type=float, default=20.0)
    args = ap.parse_args()

    workers = args.workers or (os.cpu_count() or 1)
    t0 = time.time()
    stop = t0 + args.seconds
    if args.mode == "serial":
        # 串行/空转：不构成重计算区间（负例注入形态）
        time.sleep(args.seconds)
    else:
        ctx = mp.get_context("fork") if hasattr(mp, "get_context") else mp
        with ctx.Pool(workers) as pool:
            pool.starmap(_burn, [(stop,)] * workers)
    print(f"PROBE_DONE mode={args.mode} workers={workers} seconds={time.time() - t0:.2f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
