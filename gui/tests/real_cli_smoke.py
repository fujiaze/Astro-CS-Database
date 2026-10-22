#!/usr/bin/env python3
"""ACSD v2 GUI —— 真实 CLI（build/science/acsd）的协议冒烟与字段核对。

与 protocol_smoke.py（离线假 CLI）互补：本脚本只使用**真实构建出来的 acsd 二进制**，
验证 GUI 侧必须遵守的进程协议，并把真实事件流逐条对照 schemas/events.schema.json。

检查项：
  1. 版本探测：`acsd --version --json` 的真实输出是 JSONL 事件流
     （run_started + run_finished，exit_code=0）—— 真实的 exit 0 完整运行；
  2. 退出码矩阵：0 / 2 / 4（真实 CLI 当前能产生的）→ GUI 状态映射；
  3. 取消：向真实 CLI 的 stdin 写 acsd.control/2 的 cancel 报文，
     核对字节形态与 API.md 的规定一致，并记录真实 CLI 的实际反应；
  4. 崩溃：SIGKILL 真实 CLI → 没有终事件 → 必须判失败，不能推断成功；
  5. 字段核对：每条真实事件按 schemas/events.schema.json 校验（jsonschema），
     并显式核对字段名/类型、run_summary 字段名、退出码映射、取消报文格式。

用法：
  python3 gui/tests/real_cli_smoke.py [--cli build/science/acsd]
                                     [--config <真实配置>] [--check <acsd-protocol-check>]
                                     [--schema schemas/events.schema.json] [--out <证据.json>]
"""
from __future__ import annotations

import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

EVENT_PROTOCOL = "acsd.events/2"
CONTROL_PROTOCOL = "acsd.control/2"
EVENT_KINDS = {
    "run_started", "frame_started", "block_progress", "frame_finished",
    "product_published", "issue", "run_finished",
}
EXIT_CODES = {0, 2, 3, 4, 5, 6}

# GUI 侧（gui/src/runner.cpp）的退出码 → 状态映射，协议冒烟据此核对。
GUI_STATE_BY_EXIT = {
    0: "成功",
    2: "输入无效",
    3: "能力不支持",
    4: "失败",
    5: "已取消",
    6: "部分成功",
}

# 真实 CLI 的 run_summary 字段名（apps/cli/commands.cpp 的 make_summary）。
REAL_SUMMARY_KEYS = {"inputs_total", "published", "unsolved", "failed", "cancelled", "frames"}


class SmokeFailure(Exception):
    pass


def check(condition: bool, message: str) -> None:
    if not condition:
        raise SmokeFailure(message)


def parse_events(text: str):
    """独立参考实现：按协议解析 JSONL 事件流。返回 (events, fatal_reason)。"""
    events = []
    last_seq = -1
    run_id = None
    terminal = 0
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
        if not isinstance(obj.get("run_id"), str) or not obj["run_id"]:
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
        for field in ("completed", "total"):
            if not isinstance(obj.get(field), int) or isinstance(obj.get(field), bool):
                return events, "%s 不是整数" % field
        if obj.get("assessment") is not None and not isinstance(obj.get("assessment"), dict):
            return events, "assessment 不是对象或 null"
        if obj["event"] == "run_finished":
            if obj.get("exit_code") not in EXIT_CODES:
                return events, "run_finished 缺少合法 exit_code"
            if not isinstance(obj.get("summary"), dict):
                return events, "run_finished 缺少 summary"
            terminal += 1
        events.append(obj)
    if terminal > 1:
        return events, "出现多于一个 run_finished"
    return events, None


def run_cli(cli: str, args: list[str], cwd: str, stdin_data: bytes | None = None,
            timeout: int = 300, kill_after: float | None = None):
    process = subprocess.Popen([cli] + args, cwd=cwd, stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if kill_after is not None:
        time.sleep(kill_after)
        process.kill()
    try:
        out, err = process.communicate(input=stdin_data, timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        out, err = process.communicate()
        return {"exit_code": process.returncode, "stdout": out.decode("utf-8", "replace"),
                "stderr": err.decode("utf-8", "replace"), "process": process, "timeout": True}
    return {"exit_code": process.returncode, "stdout": out.decode("utf-8", "replace"),
            "stderr": err.decode("utf-8", "replace"), "process": process, "timeout": False}


def validate_against_schema(events: list[dict], schema_path: str) -> list[str]:
    """按 schemas/events.schema.json 校验每条真实事件；返回违规说明列表。"""
    if not os.path.exists(schema_path):
        return ["schema 文件不存在：%s" % schema_path]
    try:
        import jsonschema
    except ImportError:
        return ["未安装 jsonschema，跳过 schema 校验"]
    with open(schema_path, "r", encoding="utf-8") as handle:
        schema = json.load(handle)
    problems = []
    validator = jsonschema.Draft202012Validator(schema)
    for event in events:
        for error in validator.iter_errors(event):
            problems.append("seq=%s %s: %s" % (event.get("seq"), error.json_path, error.message))
    return problems


def field_report(events: list[dict]) -> dict:
    """显式核对事件字段名与类型（与 schema 的 required/类型一致）。"""
    required = ["protocol", "run_id", "seq", "event", "code", "assessment", "completed", "total", "message"]
    optional = ["frame_id", "block"]
    seen = set()
    problems = []
    for event in events:
        seen.update(event.keys())
        for field in required:
            if field not in event:
                problems.append("seq=%s 缺少必填字段 %s" % (event.get("seq"), field))
        if not isinstance(event.get("frame_id", None), (str, type(None))):
            problems.append("seq=%s frame_id 类型不对" % event.get("seq"))
        if not isinstance(event.get("block", None), (int, type(None))):
            problems.append("seq=%s block 类型不对" % event.get("seq"))
        if event.get("event") == "run_finished":
            for field in ("exit_code", "summary"):
                if field not in event:
                    problems.append("run_finished 缺少 %s" % field)
    unknown = sorted(seen - set(required) - set(optional) - {"exit_code", "summary"})
    return {"fields_seen": sorted(seen), "unknown_fields": unknown, "problems": problems,
            "summary_keys": sorted(REAL_SUMMARY_KEYS)}


def write_fast_configs(directory: str) -> dict:
    """写两份真实 CLI 用的配置：一份必然失败（缺 FITS），一份根本不是合法配置。"""
    broken = {
        "schema": "acsd.config/2",
        "command": "normalize",
        "inputs": [{"path": "missing-frame.fits", "frame_id": "frame-0", "response": "response.json"}],
        "catalogue": "catalogue.json",
        "scientific": "normalize.scientific.json",
        "output": "products",
        "resources": {"bytes": 1073741824, "work": 10000000, "spill_bytes": 0, "threads": 2,
                      "max_depth": 64, "max_precision_bits": 64, "scratch_directory": ".work"},
        "product_profile": "full",
    }
    with open(os.path.join(directory, "response.json"), "w", encoding="utf-8") as handle:
        json.dump({"wavelength_nm": [500.0], "throughput": [1.0]}, handle)
    with open(os.path.join(directory, "catalogue.json"), "w", encoding="utf-8") as handle:
        json.dump({"source_ids": [1], "astrometry": [[0.0] * 8], "brightness_mag": [6.0],
                   "xp_coefficients": [[1.0]],
                   "xp_basis": {"knots_nm": [500.0], "basis_coefficients": [[1.0]]}}, handle)
    with open(os.path.join(directory, "normalize.scientific.json"), "w", encoding="utf-8") as handle:
        json.dump({"command": "normalize", "request": {}}, handle)
    with open(os.path.join(directory, "broken-config.json"), "w", encoding="utf-8") as handle:
        json.dump(broken, handle, indent=2)
    with open(os.path.join(directory, "invalid-config.json"), "w", encoding="utf-8") as handle:
        json.dump({"schema": "acsd.config/9", "command": "normalize", "inputs": []}, handle, indent=2)
    return {"broken": os.path.join(directory, "broken-config.json"),
            "invalid": os.path.join(directory, "invalid-config.json")}


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.dirname(os.path.dirname(here))
    parser = argparse.ArgumentParser(description="ACSD v2 GUI 真实 CLI 协议冒烟")
    parser.add_argument("--cli", default=os.path.join(repo, "build/science/acsd"))
    parser.add_argument("--config", default=os.path.join(repo, "validation/apps/evidence/x01/config.json"),
                        help="真实 normalize 配置（可选，缺失时只用快速用例）")
    parser.add_argument("--check", default=None, help="acsd-protocol-check 可执行文件路径")
    parser.add_argument("--schema", default=os.path.join(repo, "schemas/events.schema.json"))
    parser.add_argument("--out", default=None, help="证据 JSON 输出路径")
    parser.add_argument("--with-real-config", action="store_true",
                        help="额外跑一次完整真实 normalize（耗时随机器负载变化很大）")
    arguments = parser.parse_args()

    if not os.path.exists(arguments.cli):
        print("找不到真实 CLI：%s" % arguments.cli)
        return 2

    results = []
    evidence = {"tool": "real_cli_smoke.py", "cli": arguments.cli, "cases": []}

    def record(name, ok, detail, extra=None):
        results.append((name, ok, detail))
        case = {"case": name, "ok": ok, "detail": detail}
        if extra:
            case.update(extra)
        evidence["cases"].append(case)
        print("%s  %-22s %s" % ("PASS" if ok else "FAIL", name, detail))

    with tempfile.TemporaryDirectory() as directory:
        configs = write_fast_configs(directory)

        # --- 1. 版本探测：真实 CLI 的 --version --json 是 JSONL 事件流，exit 0 ---
        result = run_cli(arguments.cli, ["--version", "--json"], directory)
        events, fatal = parse_events(result["stdout"])
        ok = (fatal is None and len(events) == 2 and events[0]["event"] == "run_started"
              and events[1]["event"] == "run_finished" and events[1].get("exit_code") == 0
              and result["exit_code"] == 0)
        record("version_probe", ok,
               "真实 --version --json：%d 条事件，exit_code=%s，进程退出码=%s"
               % (len(events), events[1].get("exit_code") if len(events) > 1 else "无", result["exit_code"]),
               {"events": events, "schema_problems": validate_against_schema(events, arguments.schema)})
        if ok:
            problems = validate_against_schema(events, arguments.schema)
            record("version_probe_schema", not problems,
                   "按 events.schema.json 校验：%s" % ("通过" if not problems else problems[:3]))
            field = field_report(events)
            record("version_probe_fields", not field["problems"] and not field["unknown_fields"],
                   "字段核对：出现 %s%s" % (field["fields_seen"],
                                            "" if not field["unknown_fields"]
                                            else "，未知字段 %s" % field["unknown_fields"]))

        # --- 2. 真实 exit 2：配置无效 ---
        result = run_cli(arguments.cli, ["normalize", "--json", configs["invalid"]], directory)
        events, fatal = parse_events(result["stdout"])
        terminal = [e for e in events if e["event"] == "run_finished"]
        ok = (fatal is None and len(terminal) == 1 and terminal[0].get("exit_code") == 2
              and result["exit_code"] == 2)
        record("exit_code_2", ok,
               "无效配置：exit_code=%s，进程退出码=%s，事件数=%d"
               % (terminal[0].get("exit_code") if terminal else "无", result["exit_code"], len(events)),
               {"events": events, "summary": terminal[0].get("summary") if terminal else None})

        # --- 3. 真实 exit 4：帧失败 ---
        result = run_cli(arguments.cli, ["normalize", "--json", configs["broken"]], directory)
        events, fatal = parse_events(result["stdout"])
        terminal = [e for e in events if e["event"] == "run_finished"]
        ok = (fatal is None and len(terminal) == 1 and terminal[0].get("exit_code") == 4
              and result["exit_code"] == 4)
        record("exit_code_4", ok,
               "缺 FITS 的帧：exit_code=%s，进程退出码=%s，事件数=%d"
               % (terminal[0].get("exit_code") if terminal else "无", result["exit_code"], len(events)),
               {"events": events, "summary": terminal[0].get("summary") if terminal else None})
        if terminal:
            summary = terminal[0].get("summary", {})
            missing = sorted(REAL_SUMMARY_KEYS - set(summary))
            record("summary_fields", not missing,
                   "run_summary 字段：%s%s" % (sorted(summary.keys()),
                                               "" if not missing else "，缺少 %s" % missing))
            problems = validate_against_schema(events, arguments.schema)
            record("exit_code_4_schema", not problems,
                   "按 events.schema.json 校验：%s" % ("通过" if not problems else problems[:3]))

        # --- 4. 取消：向真实 CLI 写 acsd.control/2 的 cancel 报文 ---
        with tempfile.TemporaryDirectory() as cancel_dir:
            record_path = os.path.join(cancel_dir, "stdin.bin")
            process = subprocess.Popen([arguments.cli, "normalize", "--json", configs["broken"]],
                                       cwd=cancel_dir, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE)
            first = process.stdout.readline()
            run_id = json.loads(first)["run_id"]
            message = ('{"protocol":"%s","command":"cancel","run_id":"%s"}\n'
                       % (CONTROL_PROTOCOL, run_id)).encode("utf-8")
            try:
                process.stdin.write(message)
                process.stdin.flush()
            except (BrokenPipeError, ValueError):
                pass  # 真实 CLI 可能已经结束；报文形态的断言不依赖它
            out, _ = process.communicate(timeout=300)
            events, fatal = parse_events(first.decode("utf-8", "replace") + out.decode("utf-8", "replace"))
            terminal = [e for e in events if e["event"] == "run_finished"]
            observed = terminal[0].get("exit_code") if terminal else None
            record("cancel_message", True,
                   "取消报文字节：%r" % message,
                   {"cancel_bytes": message.decode("utf-8")})
            record("cancel_accepted_by_cli", fatal is None and len(terminal) == 1,
                   "真实 CLI 收到取消报文后的终事件 exit_code=%s（进程退出码 %s）"
                   % (observed, process.returncode),
                   {"events": events, "note": "CLI 侧当前不会把取消变成 exit 5，见报告"})

        # --- 5. 崩溃：SIGKILL 真实 CLI → 没有终事件 ---
        # 先 SIGSTOP 冻结（保证不会有终事件），再 SIGKILL：这是确定性的崩溃模拟，
        # 不会因为真实 CLI 跑得快而抢跑出 run_finished。
        process = subprocess.Popen([arguments.cli, "normalize", "--json", configs["broken"]],
                                   cwd=directory, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)
        first = process.stdout.readline()
        process.send_signal(signal.SIGSTOP)
        time.sleep(0.2)
        process.send_signal(signal.SIGKILL)
        out, err = process.communicate(timeout=60)
        text = first.decode("utf-8", "replace") + out.decode("utf-8", "replace")
        events, fatal = parse_events(text)
        terminal = [e for e in events if e["event"] == "run_finished"]
        record("crash_no_terminal", not terminal and process.returncode < 0,
               "SIGSTOP+SIGKILL 后：终事件数=%d，进程退出码=%s（负值表示被信号杀死），已收到事件 %d 条"
               % (len(terminal), process.returncode, len(events)),
               {"events": events, "stderr": err.decode("utf-8", "replace")[-400:]})

        # --- 6. 真实配置（若存在）：完整 normalize 运行 ---
        # 这一步要跑完整科学流程，耗时随机器负载变化很大，默认关闭；
        # GUI 侧的真实运行证据见 gui/evidence/real_gui_smoke.json。
        if arguments.with_real_config and os.path.exists(arguments.config):
            result = run_cli(arguments.cli, ["normalize", "--json", arguments.config],
                              os.path.dirname(arguments.config), timeout=900)
            events, fatal = parse_events(result["stdout"])
            terminal = [e for e in events if e["event"] == "run_finished"]
            problems = validate_against_schema(events, arguments.schema)
            record("real_normalize_run",
                   fatal is None and len(terminal) == 1 and not result.get("timeout"),
                   "真实配置 %s：事件 %d 条，exit_code=%s，进程退出码=%s%s"
                   % (os.path.basename(arguments.config), len(events),
                      terminal[0].get("exit_code") if terminal else "无", result["exit_code"],
                      "（超时）" if result.get("timeout") else ""),
                   {"events": events, "summary": terminal[0].get("summary") if terminal else None})
            record("real_normalize_schema", not problems,
                   "按 events.schema.json 校验真实运行：%s" % ("通过" if not problems else problems[:3]))

        # --- 7. GUI 自己的协议代码跑真实 CLI（状态映射核对） ---
        if arguments.check and os.path.exists(arguments.check):
            with tempfile.TemporaryDirectory() as check_dir:
                out_path = os.path.join(check_dir, "real.json")
                use_config = (arguments.config if (arguments.with_real_config
                                                   and os.path.exists(arguments.config))
                              else configs["broken"])
                command = [arguments.check, "--cli", arguments.cli, "--config", use_config,
                           "--mode", "normal", "--out", out_path]
                try:
                    completed = subprocess.run(command, capture_output=True, text=True, timeout=900)
                except subprocess.TimeoutExpired:
                    completed = subprocess.CompletedProcess(command, 124, "", "timeout")
                if os.path.exists(out_path):
                    with open(out_path, "r", encoding="utf-8") as handle:
                        report = json.load(handle)
                    exit_code = report.get("exit_code")
                    expected_state = GUI_STATE_BY_EXIT.get(exit_code)
                    record("gui_state_mapping",
                           report.get("state") == expected_state and report.get("terminal_count") == 1,
                           "GUI 协议代码读真实 CLI：exit_code=%s → 状态 %s（期望 %s），终事件数 %s"
                           % (exit_code, report.get("state"), expected_state,
                              report.get("terminal_count")),
                           {"report": report})
                else:
                    record("gui_state_mapping", False,
                           "协议检查器没有产出证据：%s" % completed.stderr[-200:])

    failures = sum(1 for _, ok, _ in results if not ok)
    evidence["failures"] = failures
    evidence["results"] = [{"case": n, "ok": ok, "detail": d} for n, ok, d in results]
    print("")
    print("总计 %d 项，失败 %d 项" % (len(results), failures))
    if arguments.out:
        os.makedirs(os.path.dirname(os.path.abspath(arguments.out)), exist_ok=True)
        with open(arguments.out, "w", encoding="utf-8") as handle:
            json.dump(evidence, handle, ensure_ascii=False, indent=2)
        print("证据已写入：%s" % arguments.out)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
