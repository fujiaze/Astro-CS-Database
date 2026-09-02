#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LOG-001 结构化日志事件参考实现（合同最小可执行语义）。

本模块不是生产 logger（LOG-002 才把监控/生产 Runtime 接上统一日志）。
它冻结 LOG-001 合同的机器可执行部分，供 checker 与测试引用：
  - 事件字段常量（与 log_event_v1.schema.json 的 required 集合一致）；
  - JSONL 序列化（schema 常量、必需字段齐全、无换行注入）；
  - 多线程顺序：seq 由调用方在事件产生边界（临界区）单调分配；
  - 中文可读摘要 + 机器 JSONL 双输出；
  - 敏感路径脱敏（绝对用户路径/家目录/凭据 → <redacted>）；
  - 单行大小上限（默认 4096 字节）与截断策略。

科学公式不涉及；本文件不修改任何科学/算法源码。
"""
from __future__ import annotations

import json
import pathlib
import re
from typing import Any, Dict, List, Optional

# ── 合同标识与 schema 文件位置 ────────────────────────────────────────────────
SCHEMA_ID = "astrocs.log.event.v1"
SCHEMA_VERSION = "v1"
REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]  # worktree 根
SCHEMA_PATH = REPO_ROOT / "runtime" / "logging" / "log_event_v1.schema.json"

# 必需字段（须与 log_event_v1.schema.json 的 required 完全一致；同步测试校验）
REQUIRED_FIELDS: List[str] = [
    "schema", "seq", "ts", "run", "task", "node", "module", "phase",
    "commit", "host", "level", "event", "units", "elapsed", "diagnostic",
]

LEVELS = ("debug", "info", "warn", "error")
EVENTS = ("start", "progress", "end", "warn", "error", "metric",
          "checkpoint", "cancel", "trace")

# 单行大小上限（字节；LOG-001 验收：日志 schema 检查和大小上限）
MAX_LINE_BYTES = 4096

# ── 敏感路径脱敏 ───────────────────────────────────────────────────────────────
# 匹配内容：绝对类 Unix 路径、Windows 盘符路径、UNC 路径、URL 用户信息、
# 常见家目录前缀、形似凭据的键值（password/token/secret/api_key 等）。
_REDACT_PATTERNS = [
    re.compile(r"(?i)\b(password|passwd|pwd|token|secret|api[_-]?key|credential|private[_-]?key)"
               r"\s*[=:]\s*[^\s,;\"']+"),
    re.compile(r"(?i)\bAuthorization\s*:\s*Bearer\s+\S+"),
    re.compile(r"(?i)\bbearer\s+[A-Za-z0-9._~+/=:-]+"),
    re.compile(r"[A-Za-z]:\\[^\s\"',;]+"),          # Windows 盘符绝对路径
    re.compile(r"\\\\[^\\\s\"',;]+\\[^\s\"',;]+"),  # UNC 路径
    re.compile(r"/home/[^/\s\"',;]+(?:/[^\s\"',;]*)*"),
    re.compile(r"/Users/[^/\s\"',;]+(?:/[^\s\"',;]*)*"),
    re.compile(r"/tmp/[^\s\"',;]+"),
    re.compile(r"[a-zA-Z][a-zA-Z0-9+.-]*://[^\s\"',;]+"),  # scheme://...（URL）
]


def redact(text: str) -> str:
    """对文本做敏感信息脱敏。绝对用户路径/家目录/凭据 → <redacted>。

    source/symbol/status/event 等结构化字段由 schema pattern 另行限制；
    本函数主要作用于 diagnostic 与 message 等自由文本。
    """
    out = text
    for pat in _REDACT_PATTERNS:
        out = pat.sub("<redacted>", out)
    return out


def _clip_utf8(text: str, max_bytes: int) -> str:
    """按 UTF-8 字节截断，保证不切断多字节字符（尾部截断 + 省略号）。"""
    if len(text.encode("utf-8", "replace")) <= max_bytes:
        return text
    n = 0
    cut = ""
    for ch in text:
        nb = len(ch.encode("utf-8", "replace"))
        if n + nb > max_bytes:
            break
        n += nb
        cut += ch
    return cut + "…"


def _fit_jsonl(data: Dict[str, Any], max_bytes: int) -> str:
    """序列化并保证单行 <= max_bytes（含换行）。

    超限时按 UTF-8 边界截断最长的可截字符串字段（diagnostic 优先），
    然后截掉省略号以外的内容并补省略号；截断后仍是合法 JSON。
    """
    line = json.dumps(data, ensure_ascii=False, separators=(",", ":"),
                      sort_keys=True)
    budget = max_bytes - 1  # 去掉 '\n'
    if len(line.encode("utf-8", "replace")) <= budget:
        return line + "\n"
    # 逐字段截断最长字符串（先长后短），直到整行放得下
    keys = sorted(((k, len(v)) for k, v in data.items()
                   if isinstance(v, str)), key=lambda kv: -kv[1])
    for k, _ in keys:
        v = data[k]
        # 该字段独占总预算时允许的最大字节（其余字段 + 引号/逗号/括号开销）
        overhead = len(json.dumps(
            {kk: vv for kk, vv in data.items() if kk != k},
            ensure_ascii=False, separators=(",", ":"), sort_keys=True).encode(
                "utf-8", "replace"))
        room = max(16, budget - overhead - len(k.encode("utf-8")) - 8)
        keep = _clip_utf8(v, room)
        if keep != v:
            data[k] = keep
            line = json.dumps(data, ensure_ascii=False, separators=(",", ":"),
                              sort_keys=True)
            if len(line.encode("utf-8", "replace")) <= budget:
                return line + "\n"
    # 兜底：截掉省略号（理论不可达：单字段裁剪必然足够）
    return line[:max(1, budget - 1)].rstrip()[:-1] + "}\n"


# ── 事件构造与序列化 ───────────────────────────────────────────────────────────
class LogEvent:
    """不可变结构化日志事件。字段须满足 log_event_v1.schema.json。"""

    def __init__(self, *, seq: int, ts: str, run: str, level: str, event: str,
                 diagnostic: str, task: str = "", node: str = "", module: str = "",
                 phase: str = "", commit: str = "", host: str = "",
                 units: str = "", elapsed: float = 0.0,
                 progress: Optional[float] = None,
                 error: Optional[Dict[str, str]] = None,
                 value: Optional[float] = None,
                 _raw: Optional[Dict[str, Any]] = None) -> None:
        if _raw is not None:  # 从已脱敏 dict 反序列化（checker/回放用）
            self.data: Dict[str, Any] = dict(_raw)
            return
        if level not in LEVELS:
            raise ValueError(f"非法 level: {level!r}")
        if event not in EVENTS:
            raise ValueError(f"非法 event: {event!r}")
        if level == "error" and error is None:
            raise ValueError("error 级事件必须携带 error{source,symbol,status}")
        if level != "error" and error is not None:
            raise ValueError("非 error 级事件不得携带 error 载荷")
        ev: Dict[str, Any] = {
            "schema": SCHEMA_ID,
            "seq": seq,
            "ts": ts,
            "run": run,
            "task": task or "",
            "node": node or "",
            "module": module or "",
            "phase": phase or "",
            "commit": commit or "",
            "host": host or "",
            "level": level,
            "event": event,
            "units": units or "",
            "elapsed": float(elapsed),
            "diagnostic": redact(diagnostic),
        }
        if progress is not None:
            if not 0.0 <= progress <= 1.0:
                raise ValueError("progress 必须 ∈ [0,1]")
            ev["progress"] = progress
        if error is not None:
            ev["error"] = {
                "source": error["source"],
                "symbol": error["symbol"],
                "status": error["status"],
            }
        if value is not None:
            ev["value"] = value
        self.data = ev

    @property
    def seq(self) -> int:
        return int(self.data["seq"])

    @property
    def level(self) -> str:
        return self.data["level"]

    @property
    def event(self) -> str:
        return self.data["event"]

    @property
    def diagnostic(self) -> str:
        return self.data["diagnostic"]

    # 中文可读摘要：与 JSONL 并列的第二种输出（同一事件）
    def summary(self) -> str:
        d = self.data
        where = "/".join(x for x in (d.get("phase") or "", d.get("module") or "",
                                     d.get("node") or "") if x)
        head = f"[{d['ts']}] seq={d['seq']} {d['level']} {d['event']}"
        if where:
            head += f" ({where})"
        diag = d.get("diagnostic") or ""
        if diag:
            head += f"：{diag}"
        elif d.get("error"):
            e = d["error"]
            head += f"：{e['status']} @ {e['symbol']} ({e['source']})"
        if d.get("units") and d.get("elapsed"):
            head += f"（elapsed={d['elapsed']}{d['units']}）"
        return head

    # 机器 JSONL：单行 JSON + '\n'；必需字段齐全；长度超限按 max_bytes 截断
    def to_jsonl(self, max_bytes: int = MAX_LINE_BYTES) -> str:
        return _fit_jsonl(dict(self.data), max_bytes)

    def to_dict(self) -> Dict[str, Any]:
        return dict(self.data)


# ── seq 分配器（多线程顺序；线程安全） ─────────────────────────────────────────
class SeqAllocator:
    """进程内单调 seq 分配器。

    语义：并发 emit 时，先进入临界区获得 seq 的事件其 seq 更小；
    seq 顺序即事件接受/产生顺序，为多线程事件顺序判定键。
    首事件 seq=1 且严格递增、无空洞（由调用方在临界区内完成事件提交）。
    """

    def __init__(self, start: int = 0) -> None:
        self._seq = start

    def next(self) -> int:
        self._seq += 1
        return self._seq


def line_size_bytes(line: str) -> int:
    """单行（含换行）UTF-8 字节长度；用于大小上限检查。"""
    return len(line.encode("utf-8", "replace"))
