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
    # V8-CI-010 F-R2-01 修复：与直接执行 ``python3 tools/monitoring/run_monitored.py``
    # 对齐 sys.path/模块解析语义。脚本模式下解释器会把被执行脚本所在目录置于
    # sys.path[0]；而 runpy.run_path 不做该注入——本 shim 自身被调用时
    # sys.path[0] 是本文件所在 ci/ 目录，run_monitored 的同目录导入
    # （fallback ``import resource_probe``）与依赖 repo 根的包导入双双
    # ModuleNotFoundError，凡经 shim 包装的检查必败（hosted WIN-BUILD-RELEASE
    # / WIN-TEST-UNIT 实踩，linux 控制节点同命令可复现）。最小修：仅注入被
    # 包装脚本目录（即"目标脚本目录入 sys.path"），不改监控采集逻辑与输出
    # schema；执行后恢复 sys.path，不污染调用方（单测 in-process 复用）。
    monitor_dir = str(MONITORED.parent)
    path_injected = monitor_dir not in sys.path
    if path_injected:
        sys.path.insert(0, monitor_dir)
    try:
        runpy.run_path(str(MONITORED), run_name="__main__")
    except SystemExit as exc:
        code = exc.code
        if code is None:
            return 0
        return code if isinstance(code, int) else 2
    finally:
        if path_injected:
            try:
                sys.path.remove(monitor_dir)
            except ValueError:  # 已被包装脚本改动：不强求恢复
                pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
