#!/usr/bin/env python3
"""假 acsd —— 只实现 GUI 需要的协议面（JSONL 事件 + 取消控制 + 版本探测）。

这不是科学实现，也不产生任何科学数据；它只回放协议，用于验证 GUI 侧
的解析、排序、取消与终事件判定。行为由环境变量 FAKE_ACSD_MODE 选择：

  normal    完整事件流 + run_finished(exit_code=0)
  cancel    发出 run_started 后等待 stdin 的取消控制消息，
            收到后发 issue + run_finished(exit_code=5)
  crash     发出部分事件后直接退出，不给终事件（模拟意外崩溃）
  hang      发出 run_started 后一直等待，忽略取消（用于强制终止/aborted）
  terminate 与 hang 同义（GUI 冒烟里的场景名）

环境变量：
  FAKE_ACSD_RECORD   把收到的 stdin 原始字节写到该文件（供测试断言取消报文）
"""
from __future__ import annotations

import json
import os
import sys
import time
import uuid

EVENT_PROTOCOL = "acsd.events/2"
CONTROL_PROTOCOL = "acsd.control/2"


def emit(event: str, seq: int, run_id: str, code: str = "ok", message: str = "",
         completed: int = 0, total: int = 0, frame_id=None, block=None,
         assessment=None, exit_code=None, summary=None) -> None:
    payload = {
        "protocol": EVENT_PROTOCOL,
        "run_id": run_id,
        "seq": seq,
        "event": event,
        "frame_id": frame_id,
        "block": block,
        "code": code,
        "assessment": assessment,
        "completed": completed,
        "total": total,
        "message": message,
    }
    if exit_code is not None:
        payload["exit_code"] = exit_code
    if summary is not None:
        payload["summary"] = summary
    sys.stdout.write(json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n")
    sys.stdout.flush()


def read_inputs(config_path: str | None):
    if not config_path:
        return []
    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            config = json.load(handle)
    except (OSError, ValueError):
        return []
    return config.get("inputs", []) or []


def read_config(config_path: str | None):
    if not config_path:
        return {}
    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return {}


def version_probe() -> int:
    sys.stdout.write(json.dumps({
        "protocol": EVENT_PROTOCOL,
        "major": 2,
        "minor": 0,
        "version": "2.0.0-fake",
        "command": "acsd",
    }, ensure_ascii=False) + "\n")
    sys.stdout.flush()
    return 0


def wait_for_cancel(run_id: str, seq: int) -> bool:
    """读 stdin 直到收到 acsd.control/2 的 cancel 消息。"""
    for raw in sys.stdin:
        record = os.environ.get("FAKE_ACSD_RECORD")
        if record:
            with open(record, "ab") as handle:
                handle.write(raw.encode("utf-8"))
        line = raw.strip()
        if not line:
            continue
        try:
            message = json.loads(line)
        except ValueError:
            continue
        if (message.get("protocol") == CONTROL_PROTOCOL
                and message.get("command") == "cancel"
                and message.get("run_id") == run_id):
            return True
    return False


def main(argv: list[str]) -> int:
    if "--version" in argv:
        return version_probe()

    mode = os.environ.get("FAKE_ACSD_MODE", "normal")
    command = None
    config_path = None
    index = 0
    while index < len(argv):
        argument = argv[index]
        if argument == "--json" and index + 1 < len(argv):
            config_path = argv[index + 1]
            index += 2
            continue
        if not argument.startswith("-") and command is None:
            command = argument
        index += 1

    inputs = read_inputs(config_path)
    config = read_config(config_path)
    run_id = str(uuid.uuid4())
    seq = 0

    emit("run_started", seq, run_id, message=f"fake acsd 开始：command={command} mode={mode}")
    seq += 1

    if mode == "crash":
        emit("frame_started", seq, run_id, frame_id="frame-crash", message="崩溃演示：即将异常退出")
        seq += 1
        sys.stderr.write("fake acsd: 模拟崩溃，不发终事件\n")
        sys.stderr.flush()
        return 1

    if mode in ("hang", "terminate"):
        # 忽略取消，一直等到被强制终止。
        deadline = time.time() + 120
        while time.time() < deadline:
            time.sleep(0.05)
        return 0

    if mode == "cancel":
        # 等待取消控制消息。
        if wait_for_cancel(run_id, seq):
            emit("issue", seq, run_id, code="cancelled", message="收到取消控制消息，开始安全收尾")
            seq += 1
            emit("run_finished", seq, run_id, code="cancelled", message="运行已取消",
                 completed=0, total=len(inputs), exit_code=5,
                 summary={"inputs": len(inputs), "published": 0, "unsolved": 0,
                          "failed": 0, "cancelled": len(inputs)})
            return 5
        emit("run_finished", seq, run_id, code="internal", message="未收到取消控制消息",
             exit_code=4, summary={"inputs": len(inputs), "published": 0, "unsolved": 0,
                                   "failed": 0, "cancelled": 0})
        return 4

    # normal
    published = 0
    for position, item in enumerate(inputs):
        frame_id = item.get("frame_id") or f"frame-{position}"
        emit("frame_started", seq, run_id, frame_id=frame_id, message=f"开始处理帧 {frame_id}")
        seq += 1
        blocks = 3
        for block in range(blocks):
            emit("block_progress", seq, run_id, frame_id=frame_id, block=block,
                 completed=block + 1, total=blocks, message=f"{frame_id} 块 {block + 1}/{blocks}")
            seq += 1
        emit("frame_finished", seq, run_id, frame_id=frame_id, code="ok",
             completed=blocks, total=blocks, message=f"帧 {frame_id} 完成",
             assessment={"complete": True, "validity": "certified", "science": "bounded",
                         "noise": "conditional", "execution": "complete", "capability": "supported",
                         "issues": []})
        seq += 1
        published += 1

    emit("product_published", seq, run_id, message=f"发布产品到 {config.get('output', '')}")
    seq += 1
    emit("run_finished", seq, run_id, code="ok", message="全部完成",
         completed=published, total=len(inputs), exit_code=0,
         summary={"inputs": len(inputs), "published": published, "unsolved": 0,
                  "failed": 0, "cancelled": 0})
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except BrokenPipeError:
        sys.exit(4)
    except KeyboardInterrupt:
        sys.exit(5)
