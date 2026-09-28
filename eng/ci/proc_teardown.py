#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/proc_teardown.py —— CI 驱动的显式子进程回收面（单一实现点）。

缺陷型（FINAL-07 诊断报告 §7「运行纪律观察」实录）
----------------------------------------------------
驱动的**超时回收路径**（`proc.communicate(timeout=...) → _terminate`）只在
"step 自己超时"时执行。驱动被**外部**打断时（CI 取消 = SIGTERM、看门狗超时 =
SIGTERM、人工 Ctrl-C = SIGINT），该路径不会执行，而 `start_new_session=True`
建立的子进程组已脱离驱动会话 ⇒ 整组逃逸成 PPID=1 的孤儿，继续占 CPU/内存并
污染同期一切性能测量。实测实例：`run/ci/build-gcc-release/eng/tests/unit/drz_tests/
drizzle_acceptance_test`，PPID=1、存活 3.6 h、RSS 6.6 GB。

口径
----
* 驱动 spawn 的每个检查进程都**登记进一张进程账**；
* SIGTERM / SIGINT 到达时、以及解释器正常退出时（atexit），对**仍在跑**的登记项
  逐个 SIGKILL 其**进程组**（POSIX `killpg`；Windows 退回 `process.kill()`），
  并打印一行 `TEARDOWN ...` 证据 ⇒ "是否留下孤儿"可从日志直接判定；
* 只回收**登记在账**的子进程组：不碰驱动自身、不碰无关进程（见 `--self-test` 负例）；
* SIGKILL 打驱动自身仍不可捕获（内核语义），本模块不声称覆盖该情形。

自证
----
  python3 eng/ci/proc_teardown.py --self-test
正例 = 登记的子进程组（含孙进程）整组消失；负例 = 未登记进程不得被回收。
"""
from __future__ import annotations

import atexit
import os
import signal
import subprocess
import sys
import threading
import time

__all__ = ["ProcessLedger", "LEDGER", "spawn", "release", "teardown",
           "install_handlers", "kill_group", "self_test"]


def kill_group(process: subprocess.Popen) -> None:
    """终止检查进程**树**（POSIX 杀进程组；Windows 退回 kill）。"""
    try:
        if os.name == "posix":
            import errno
            try:
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
                return
            except OSError as exc:  # 进程组已退出
                if exc.errno != errno.ESRCH:
                    process.kill()
        else:  # pragma: no cover - Windows 路径
            process.kill()
    except Exception:  # noqa: BLE001
        try:
            process.kill()
        except OSError:
            pass


class ProcessLedger:
    """驱动级进程账：登记 / 注销 / 显式回收。线程安全（并行调度器会用）。"""

    def __init__(self, driver: str) -> None:
        self.driver = driver
        self._live: dict = {}
        self._lock = threading.Lock()
        self._teardown_done = False
        self._installed = False

    # ── 登记面 ─────────────────────────────────────────────────────────
    def spawn(self, argv, **kwargs) -> subprocess.Popen:
        proc = subprocess.Popen(argv, **kwargs)  # noqa: S603 - 调用方给 argv
        with self._lock:
            self._live[proc.pid] = (proc, " ".join(str(a) for a in argv[:2]))
        return proc

    def release(self, proc) -> None:
        with self._lock:
            self._live.pop(proc.pid, None)

    def live_pids(self) -> list:
        with self._lock:
            return sorted(self._live)

    # ── 回收面 ─────────────────────────────────────────────────────────
    def teardown(self, reason: str) -> list:
        """回收所有仍在跑的登记子进程组；返回被回收的 pid 列表（已去重/幂等）。"""
        with self._lock:
            pending = list(self._live.items())
            self._live.clear()
            self._teardown_done = True
        killed = []
        for pid, (proc, tag) in pending:
            if proc.poll() is not None:
                continue          # 已正常收尾：不是逃逸面，不打印（避免噪声误导）
            kill_group(proc)
            killed.append(pid)
            try:
                proc.wait(timeout=10)   # 收割直接子进程，避免驱动自身留下僵尸
            except Exception:  # noqa: BLE001 - 已死/无法收割都不影响后续回收
                pass
            # 诊断行走 stderr：stdout 是驱动与检查器的机读契约（--plan-only/--json-out
            # 直接解析），任何多余行都会让消费者的 JSON 解析失败。
            print("TEARDOWN %s pid=%d tag=%s reason=%s"
                  % (self.driver, pid, tag, reason), file=sys.stderr, flush=True)
        if not killed:
            print("TEARDOWN %s none-live reason=%s" % (self.driver, reason),
                  file=sys.stderr, flush=True)
        return killed

    def install_handlers(self) -> None:
        """装 SIGTERM/SIGINT 处理器 + atexit 兜底（幂等）。

        逃生开关 `ASTROCS_CI_NO_TEARDOWN=1` **只为复现旧行为做负例对照**（见
        run/FINAL-07/logs/proc-teardown-e2e.sh 的两态对照）；打开时大声告警，
        不允许在 CI 里静默打开。
        """
        if self._installed:
            return
        self._installed = True
        if os.environ.get("ASTROCS_CI_NO_TEARDOWN") == "1":
            print("WARNING %s: ASTROCS_CI_NO_TEARDOWN=1 ⇒ 关闭显式回收"
                  "（仅用于负例对照；逃逸孤儿将不被回收）" % self.driver,
                  file=sys.stderr, flush=True)
            return

        def _handler(signum, _frame):
            self.teardown("signal %s" % signal.Signals(signum).name)
            sys.exit(128 + signum)

        for sig in (signal.SIGTERM, signal.SIGINT):
            try:
                signal.signal(sig, _handler)
            except (ValueError, OSError):  # pragma: no cover - 非主线程/平台限制
                pass
        atexit.register(lambda: self.teardown("atexit"))


LEDGER = ProcessLedger("eng/ci")


def spawn(argv, **kwargs) -> subprocess.Popen:
    return LEDGER.spawn(argv, **kwargs)


def release(proc) -> None:
    LEDGER.release(proc)


def teardown(reason: str = "explicit") -> list:
    return LEDGER.teardown(reason)


def install_handlers() -> None:
    LEDGER.install_handlers()


# ── 自证 ────────────────────────────────────────────────────────────────
def _pgid_alive(pgid: int) -> bool:
    """POSIX：/proc 里是否仍有属于该进程组的**活**进程（含孙进程）。

    僵尸（state='Z'）不算活：它已不占用 CPU/内存，只是等待收割。
    """
    if os.name != "posix":
        return False
    for name in os.listdir("/proc"):
        if not name.isdigit():
            continue
        try:
            with open("/proc/%s/stat" % name, "rb") as f:
                fields = f.read().rsplit(b")", 1)[-1].split()
            # 切掉 "pid (comm)" 后字段为: state ppid pgrp ...
            if fields[0] == b"Z":
                continue
            if int(fields[2]) == pgid:
                return True
        except (OSError, IndexError, ValueError):
            continue
    return False


def self_test() -> int:
    cases, failures = 0, []

    # 正例：登记的命令（bash 再派生孙进程）必须被整组回收。
    cases += 1
    p = spawn(["/bin/bash", "-c", "sleep 300 & sleep 300"],
              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
              start_new_session=True)
    pgid = os.getpgid(p.pid)
    time.sleep(0.6)
    assert _pgid_alive(pgid), "夹具未起来"
    killed = teardown("self-test-positive")
    time.sleep(1.0)
    if _pgid_alive(pgid):
        failures.append("positive: 进程组 %d 仍在（禁逃逸面失效）" % pgid)
    else:
        print("SELFTEST_PASS teardown_kills_group (pgid=%d killed=%s)" % (pgid, killed))

    # 负例：未登记的进程不得被回收（证明回收面 = 登记账，不是"杀一切"）。
    cases += 1
    off = subprocess.Popen(["/bin/bash", "-c", "sleep 300"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           start_new_session=True)
    try:
        time.sleep(0.4)
        teardown("self-test-negative")
        time.sleep(0.4)
        if off.poll() is not None:
            failures.append("negative: 未登记进程被误杀（回收面越界）")
        else:
            print("SELFTEST_PASS teardown_ignores_unregistered (pid=%d 存活)" % off.pid)
    finally:
        kill_group(off)

    print("SELFTEST cases=%d" % cases)
    if failures:
        print("SELFTEST_FAIL:")
        for item in failures:
            print("  " + item)
        return 1
    print("SELFTEST_PASS: all %d cases match expectation" % cases)
    return 0


def main(argv=None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if "--self-test" in args:
        return self_test()
    print(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main())
