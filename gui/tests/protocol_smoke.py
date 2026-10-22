#!/usr/bin/env python3
"""ACSD v2 GUI —— 不依赖 Qt 的协议级冒烟。

目的：用假 CLI 回放 JSONL，独立验证 GUI 侧必须遵守的进程协议语义
（API.md「CLI 配置、事件与 GUI」节）：

  * 每行 UTF-8 单对象，protocol="acsd.events/2"；
  * seq 从 0 严格递增，run_id 在一次 run 内唯一；
  * 每 run 恰一个 run_finished，且带 exit_code 与 summary；
  * 取消消息是 {"protocol":"acsd.control/2","command":"cancel","run_id":"..."} + 换行；
  * 意外崩溃可能缺终事件 —— GUI 必须标失败，不能推断成功。

如果给了 --check <acsd-protocol-check>，还会用 GUI 自己的协议代码跑同样的场景，
并把两边的规范化事件流逐行对比（两套独立实现必须给出同一结论）。

用法：
  python3 gui/tests/protocol_smoke.py [--fake fake_acsd.py] [--config sample-config.json]
                                      [--check <acsd-protocol-check>] [--out evidence.json]
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile

EVENT_PROTOCOL = "acsd.events/2"
CONTROL_PROTOCOL = "acsd.control/2"
EVENT_KINDS = {
    "run_started", "frame_started", "block_progress", "frame_finished",
    "product_published", "issue", "run_finished",
}
EXIT_CODES = {0, 2, 3, 4, 5, 6}


class SmokeFailure(Exception):
    pass


def check(condition: bool, message: str) -> None:
    if not condition:
        raise SmokeFailure(message)


# ---------------------------------------------------------------------------
# 独立参考实现：按协议解析 JSONL 事件流
# ---------------------------------------------------------------------------
def parse_stream(text: str):
    """返回 (events, fatal_reason)。fatal_reason 非空表示流本身违规。"""
    events = []
    last_seq = -1
    run_id = None
    terminal_count = 0
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except ValueError as error:
            return events, "事件行不是合法 JSON：%s" % error
        if not isinstance(obj, dict):
            return events, "事件行不是 JSON 对象"
        if obj.get("protocol") != EVENT_PROTOCOL:
            return events, "协议标识不是 %s" % EVENT_PROTOCOL
        if "run_id" not in obj or not isinstance(obj["run_id"], str):
            return events, "缺少 run_id"
        if run_id is not None and obj["run_id"] != run_id:
            return events, "run_id 在同一流内变化"
        run_id = obj["run_id"]
        seq = obj.get("seq")
        if not isinstance(seq, int) or isinstance(seq, bool) or seq < 0:
            return events, "seq 非法"
        if seq <= last_seq:
            return events, "seq 非严格递增"
        last_seq = seq
        if obj.get("event") not in EVENT_KINDS:
            return events, "未知事件类型 %r" % obj.get("event")
        for field in ("code", "message"):
            if not isinstance(obj.get(field), str):
                return events, "缺少 %s" % field
        if not isinstance(obj.get("completed"), int) or not isinstance(obj.get("total"), int):
            return events, "completed/total 非法"
        if obj["event"] == "run_finished":
            if "exit_code" not in obj or obj["exit_code"] not in EXIT_CODES:
                return events, "run_finished 缺少合法 exit_code"
            if not isinstance(obj.get("summary"), dict):
                return events, "run_finished 缺少 summary"
            terminal_count += 1
        events.append(obj)
    if terminal_count > 1:
        return events, "出现多于一个 run_finished"
    return events, None


def normalized(events):
    return ["%s|%s|%s|%s/%s|%s|%s" % (
        e["seq"], e["event"], e["code"], e["completed"], e["total"],
        e.get("frame_id") or "-", e.get("block") if e.get("block") is not None else "-")
        for e in events]


def run_fake(fake: str, config: str, mode: str, cancel: bool, record: str | None = None):
    """跑一次假 CLI；cancel=True 时在收到 run_started 后发出取消控制消息。"""
    env = dict(os.environ)
    env["FAKE_ACSD_MODE"] = mode
    if record:
        env["FAKE_ACSD_RECORD"] = record
    process = subprocess.Popen(
        [sys.executable, fake, "normalize", "--json", config],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=env, text=True, bufsize=1)
    sent = None
    collected = []
    if cancel:
        first = process.stdout.readline()
        collected.append(first)
        try:
            run_id = json.loads(first)["run_id"]
        except (ValueError, KeyError) as error:
            process.kill()
            raise SmokeFailure("首个事件无法解析 run_id：%s" % error)
        sent = '{"protocol":"%s","command":"cancel","run_id":"%s"}\n' % (CONTROL_PROTOCOL, run_id)
        process.stdin.write(sent)
        process.stdin.flush()
    stdout, stderr = process.communicate(timeout=60)
    collected.append(stdout)
    return {
        "exit_code": process.returncode,
        "stdout": "".join(collected),
        "stderr": stderr,
        "sent": sent,
    }


# ---------------------------------------------------------------------------
# 场景
# ---------------------------------------------------------------------------
def scenario_normal(fake: str, config: str, results: list) -> None:
    result = run_fake(fake, config, "normal", cancel=False)
    events, fatal = parse_stream(result["stdout"])
    check(fatal is None, "normal：事件流违规 %s" % fatal)
    check(len(events) > 0, "normal：没有事件")
    check(events[0]["seq"] == 0, "normal：首个事件 seq 不是 0")
    check([e["seq"] for e in events] == list(range(len(events))), "normal：seq 不连续")
    terminal = [e for e in events if e["event"] == "run_finished"]
    check(len(terminal) == 1, "normal：run_finished 数量为 %d" % len(terminal))
    check(terminal[0]["exit_code"] == 0, "normal：exit_code 不是 0")
    check("summary" in terminal[0], "normal：缺 summary")
    check(result["exit_code"] == 0, "normal：进程退出码 %d" % result["exit_code"])
    results.append(("normal", True, "run_finished=1 exit_code=0 事件数=%d" % len(events)))


def scenario_cancel(fake: str, config: str, results: list) -> None:
    with tempfile.TemporaryDirectory() as directory:
        record = os.path.join(directory, "stdin.bin")
        result = run_fake(fake, config, "cancel", cancel=True, record=record)
        events, fatal = parse_stream(result["stdout"])
        check(fatal is None, "cancel：事件流违规 %s" % fatal)
        terminal = [e for e in events if e["event"] == "run_finished"]
        check(len(terminal) == 1, "cancel：run_finished 数量为 %d" % len(terminal))
        check(terminal[0]["exit_code"] == 5, "cancel：exit_code 不是 5")
        check(result["exit_code"] == 5, "cancel：进程退出码 %d" % result["exit_code"])
        with open(record, "rb") as handle:
            received = handle.read()
        check(received == result["sent"].encode("utf-8"),
              "cancel：取消报文字节不符：收到 %r，期望 %r" % (received, result["sent"].encode("utf-8")))
        check(result["sent"].encode("utf-8").endswith(b"}\n"), "cancel：取消报文未以换行结束")
        check(json.loads(result["sent"])["protocol"] == CONTROL_PROTOCOL, "cancel：控制协议标识错误")
        check(json.loads(result["sent"])["command"] == "cancel", "cancel：命令不是 cancel")
        results.append(("cancel", True, "取消报文字节一致 exit_code=5"))


def scenario_crash(fake: str, config: str, results: list) -> None:
    result = run_fake(fake, config, "crash", cancel=False)
    events, fatal = parse_stream(result["stdout"])
    check(fatal is None, "crash：事件流违规 %s" % fatal)
    terminal = [e for e in events if e["event"] == "run_finished"]
    check(len(terminal) == 0, "crash：竟然收到 %d 个 run_finished" % len(terminal))
    check(result["exit_code"] != 0, "crash：进程退出码 %d" % result["exit_code"])
    results.append(("crash", True, "无终事件 → 必须判失败（不得推断成功）"))


def scenario_terminate(fake: str, config: str, results: list) -> None:
    """用户主动终止：假 CLI 忽略取消，GUI 必须标 aborted 而不是成功。"""
    env = dict(os.environ)
    env["FAKE_ACSD_MODE"] = "hang"
    process = subprocess.Popen(
        [sys.executable, fake, "normalize", "--json", config],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        env=env, text=True, bufsize=1)
    first = process.stdout.readline()
    run_id = json.loads(first)["run_id"]
    process.stdin.write('{"protocol":"%s","command":"cancel","run_id":"%s"}\n' % (CONTROL_PROTOCOL, run_id))
    process.stdin.flush()
    process.kill()
    stdout, _ = process.communicate(timeout=60)
    events, fatal = parse_stream(first + stdout)
    check(fatal is None, "terminate：事件流违规 %s" % fatal)
    terminal = [e for e in events if e["event"] == "run_finished"]
    check(len(terminal) == 0, "terminate：竟然收到 run_finished")
    results.append(("terminate", True, "主动终止后无终事件 → 必须标 aborted"))


def cross_check(checker: str, fake: str, config: str, results: list) -> None:
    """用 GUI 自己的协议代码跑同样场景，并逐行对比规范化事件流。"""
    with tempfile.TemporaryDirectory() as directory:
        for mode, cancel in (("normal", False), ("cancel", True), ("crash", False)):
            out_path = os.path.join(directory, "%s.json" % mode)
            command = [checker, "--cli", fake, "--config", config, "--mode", mode, "--out", out_path]
            if mode == "cancel":
                command += ["--cancel-delay-ms", "300"]
            completed = subprocess.run(command, capture_output=True, text=True, timeout=120)
            check(completed.returncode == 0,
                  "cross_check：%s 模式返回 %d：%s" % (mode, completed.returncode, completed.stderr))
            with open(out_path, "r", encoding="utf-8") as handle:
                report = json.load(handle)
            gui_stream = report.get("stream", [])
            check(report.get("seq_strictly_increasing") is True,
                  "cross_check：%s 模式 seq 非严格递增" % mode)
            check(report.get("terminal_count") == (1 if mode != "crash" else 0),
                  "cross_check：%s 模式终事件数 %s" % (mode, report.get("terminal_count")))
            if mode == "crash":
                check(report.get("state") == "失败",
                      "cross_check：崩溃模式状态是 %s，应为失败" % report.get("state"))
            if mode == "cancel":
                check(report.get("cancel_sent") is True, "cross_check：取消没有发出")
                check(report.get("state") == "已取消",
                      "cross_check：取消模式状态是 %s" % report.get("state"))
            if mode == "normal":
                check(report.get("state") == "成功",
                      "cross_check：正常模式状态是 %s" % report.get("state"))
            # 独立参考实现与 GUI 实现的规范化流必须一致
            reference = run_fake(fake, config, mode, cancel=cancel)
            reference_events, fatal = parse_stream(reference["stdout"])
            check(fatal is None, "cross_check：参考实现解析失败 %s" % fatal)
            reference_stream = normalized(reference_events)
            check(reference_stream == gui_stream,
                  "cross_check：%s 模式两套实现事件流不一致\n参考：%s\nGUI ：%s"
                  % (mode, reference_stream, gui_stream))
            results.append(("cross_check:%s" % mode, True, "两套实现事件流一致（%d 行）" % len(gui_stream)))


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    parser = argparse.ArgumentParser(description="ACSD v2 GUI 协议级冒烟（不依赖 Qt）")
    parser.add_argument("--fake", default=os.path.join(here, "fake_acsd.py"))
    parser.add_argument("--config", default=os.path.join(here, "sample-config.json"))
    parser.add_argument("--check", default=None, help="acsd-protocol-check 可执行文件路径")
    parser.add_argument("--out", default=None, help="证据 JSON 输出路径")
    arguments = parser.parse_args()

    results: list = []
    failures = 0
    scenarios = [
        ("normal", lambda: scenario_normal(arguments.fake, arguments.config, results)),
        ("cancel", lambda: scenario_cancel(arguments.fake, arguments.config, results)),
        ("crash", lambda: scenario_crash(arguments.fake, arguments.config, results)),
        ("terminate", lambda: scenario_terminate(arguments.fake, arguments.config, results)),
    ]
    for name, function in scenarios:
        try:
            function()
        except SmokeFailure as error:
            failures += 1
            results.append((name, False, str(error)))
            print("FAIL  %-10s %s" % (name, error))
        else:
            print("PASS  %-10s %s" % (name, results[-1][2]))
    if arguments.check:
        try:
            cross_check(arguments.check, arguments.fake, arguments.config, results)
        except SmokeFailure as error:
            failures += 1
            results.append(("cross_check", False, str(error)))
            print("FAIL  %-10s %s" % ("cross_check", error))
        else:
            print("PASS  %-10s %s" % ("cross_check", results[-1][2]))

    print("")
    print("总计 %d 项，失败 %d 项" % (len(results), failures))
    if arguments.out:
        os.makedirs(os.path.dirname(os.path.abspath(arguments.out)), exist_ok=True)
        with open(arguments.out, "w", encoding="utf-8") as handle:
            json.dump({
                "tool": "protocol_smoke.py",
                "python": sys.version.split()[0],
                "results": [{"scenario": name, "ok": ok, "detail": detail} for name, ok, detail in results],
                "failures": failures,
            }, handle, ensure_ascii=False, indent=2)
        print("证据已写入：%s" % arguments.out)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
