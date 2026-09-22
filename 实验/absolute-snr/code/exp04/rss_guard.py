#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""外部 RSS 监控：包装一条命令，超过阈值即杀（本机曾发生 23 GB 吃穿全机事故）。"""
from __future__ import annotations

import os
import subprocess
import sys
import time

LIMIT_MB = float(os.environ.get("EXP04_RSS_LIMIT_MB", "3000"))
INTERVAL = float(os.environ.get("EXP04_RSS_INTERVAL", "2.0"))


def rss_mb(pid):
    try:
        with open("/proc/%d/status" % pid, encoding="utf-8") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return float(line.split()[1]) / 1024.0
    except OSError:
        return 0.0
    return 0.0


def main():
    if len(sys.argv) < 2:
        print("usage: rss_guard.py <cmd> [args...]", file=sys.stderr)
        return 2
    p = subprocess.Popen(sys.argv[1:])
    peak = 0.0
    while p.poll() is None:
        r = rss_mb(p.pid)
        peak = max(peak, r)
        if r > LIMIT_MB:
            print("[rss_guard] RSS %.0f MB > 上限 %.0f MB，杀进程" % (r, LIMIT_MB), file=sys.stderr)
            p.kill(); p.wait()
            print("[rss_guard] peak RSS = %.0f MB" % peak)
            return 3
        time.sleep(INTERVAL)
    print("[rss_guard] rc=%d peak RSS = %.0f MB" % (p.returncode, peak))
    return p.returncode


if __name__ == "__main__":
    sys.exit(main())
