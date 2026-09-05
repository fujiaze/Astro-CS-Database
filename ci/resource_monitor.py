#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/resource_monitor.py — 统一资源监控桥接入口（V8-CI-005，owner=SA-CI-32）。

背景：ci/run.py:52 定义 MONITOR_SCRIPT = "ci/resource_monitor.py"，真实执行
heavy 检查前的前置探测（probe_prerequisite）要求该文件存在；而统一监控包装器
的实体在 tools/monitoring/run_monitored.py（V8-CI-003 产物）。本文件是二者的
桥接 shim：

  * 无参数运行 → 打印能力说明 JSON 并 exit 0（被 run.py 存在性探测覆盖；
    也可用于 CI 冒烟确认监控层就绪）；
  * 带参数运行 → 透传给 tools/monitoring/run_monitored.py
    （等价于 ``python3 tools/monitoring/run_monitored.py <args>``），
    退出码透传（124=超时、128+signum=信号、否则为子进程退出码）。

不复制监控逻辑，单一事实源始终是 tools/monitoring/run_monitored.py。
仅 stdlib；子进程调用带超时保护（AGENTS 纪律）。
"""
from __future__ import annotations

import json
import runpy
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
MONITORED = REPO / "tools" / "monitoring" / "run_monitored.py"


def _capability() -> dict:
    return {
        "script": "ci/resource_monitor.py",
        "role": "bridge shim -> tools/monitoring/run_monitored.py (V8-CI-003)",
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "wrapped_target": str(MONITORED.relative_to(REPO)),
        "target_exists": MONITORED.is_file(),
        "usage": "python3 ci/resource_monitor.py [--timeout S] [--output FILE] -- <cmd> [args...]",
    }


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv:
        print(json.dumps(_capability(), ensure_ascii=False, indent=2))
        return 0
    if not MONITORED.is_file():
        print(f"ci/resource_monitor.py: 监控包装器缺失：{MONITORED}", file=sys.stderr)
        return 2
    # runpy 运行 run_monitored.py 的 __main__，SystemExit 透传其退出码。
    sys.argv = [str(MONITORED)] + argv
    try:
        runpy.run_path(str(MONITORED), run_name="__main__")
    except SystemExit as exc:
        code = exc.code
        if code is None:
            return 0
        return code if isinstance(code, int) else 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
