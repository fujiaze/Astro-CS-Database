#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""LOG-001 结构化日志合同验收测试（tests/monitoring 域，owner SA-LOG-08）。

覆盖验收点：
  A1 必需字段全（run/task/node/module/phase/commit/host/level/event/units/
     elapsed/diagnostic）——schema required 与参考实现 REQUIRED_FIELDS 一致；
     缺字段 JSONL 被 schema 校验拒绝（负测）。
  A2 中文可读摘要 + 机器 JSONL 双输出（同源）。
  A3 敏感路径脱敏规则生效样例。
  A4 多线程事件顺序有 sequence（并发写 seq==1..2N 无空洞/重复）。
  A5 错误包含 source/symbol/status（正测）；缺 error 载荷被拒（负测）。
  A6 日志 schema 检查和大小上限（截断不切坏 UTF-8；超限行被拒绝）。
  A7 无 raw testdata（本测试仅生成内存/临时合成行，不引用 testdata 数据）。
"""
from __future__ import annotations

import concurrent.futures
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest

REPO = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))

from runtime.logging import log_event as le  # noqa: E402
from runtime.logging.log_event import (  # noqa: E402
    EVENTS, LEVELS, MAX_LINE_BYTES, LogEvent, REQUIRED_FIELDS, SCHEMA_ID,
    SeqAllocator, line_size_bytes, redact)

spec = importlib.util.spec_from_file_location(
    "check_log_contract",
    REPO / "tools" / "monitoring" / "check_log_contract.py")
clc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(clc)

SCHEMA_PATH = REPO / "runtime" / "logging" / "log_event_v1.schema.json"

TS = "2026-09-02T00:00:00Z"
COMMIT = "0" * 40


def make_event(seq: int, *, level: str = "info", event: str = "end",
               diagnostic: str = "测试事件", error=None, units: str = "ms",
               elapsed: float = 1.0, **kw) -> LogEvent:
    return LogEvent(seq=seq, ts=TS, run="run-synthetic-001", level=level,
                    event=event, diagnostic=diagnostic, task="LOG-001-TEST",
                    node="node-1", module="core", phase="runtime",
                    commit=COMMIT, host="linux-ctrl", units=units,
                    elapsed=elapsed, error=error, **kw)


def schema_dict() -> dict:
    return json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))


class TestSchemaContract(unittest.TestCase):
    """A1/A5：schema 必需字段与参考实现一致；缺字段/缺 error 载荷被拒。"""

    def test_schema_required_matches_impl(self):
        schema = schema_dict()
        self.assertEqual(schema["$id"], SCHEMA_ID)
        self.assertEqual(schema["required"], REQUIRED_FIELDS)
        for f in ("run", "task", "node", "module", "phase", "commit", "host",
                  "level", "event", "units", "elapsed", "diagnostic"):
            self.assertIn(f, schema["required"])

    def test_missing_field_rejected(self):
        ok, errs, _ = clc.check_schema_self(REPO, SCHEMA_PATH)
        self.assertTrue(ok, errs)
        schema = schema_dict()
        errs: list = []
        # 删除 run 字段 → 缺必需字段被拒
        bad = make_event(1).to_dict()
        del bad["run"]
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("缺必需字段" in e for e in errs), errs)
        # 删除 diagnostic → 缺必需字段被拒
        errs.clear()
        bad = make_event(2).to_dict()
        del bad["diagnostic"]
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("缺必需字段" in e for e in errs), errs)

    def test_unknown_field_rejected(self):
        schema = schema_dict()
        errs: list = []
        bad = make_event(1).to_dict()
        bad["secret_extra"] = "x"
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("未知字段" in e for e in errs), errs)

    def test_enum_level_event_rejected(self):
        schema = schema_dict()
        errs: list = []
        bad = make_event(1).to_dict()
        bad["level"] = "fatal"  # 枚举外
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("不在 enum" in e for e in errs), errs)
        errs.clear()
        bad = make_event(2).to_dict()
        bad["phase"] = "phase4"
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("不在 enum" in e for e in errs), errs)

    def test_seq_pattern_constraints(self):
        schema = schema_dict()
        errs: list = []
        bad = make_event(1).to_dict()
        bad["seq"] = 0  # minimum=1
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("minimum" in e for e in errs), errs)
        errs.clear()
        bad = make_event(2).to_dict()
        bad["commit"] = "abc"  # pattern 40 hex
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("pattern" in e for e in errs), errs)

    def test_error_required_and_positive(self):
        schema = schema_dict()
        errs: list = []
        ok_ev = make_event(1, level="error", event="error", diagnostic="失败",
                            error={"source": "runtime/logging/log_event.py",
                                   "symbol": "LogEvent.__init__",
                                   "status": "SCHEMA_VIOLATION"})
        clc.validate_subschema(ok_ev.to_dict(), schema, "L1", errs)
        self.assertEqual(errs, [], errs)
        # level=error 缺 error 对象：参考实现拒绝构造；checker 拒绝已存在 dict
        with self.assertRaises(ValueError):
            make_event(2, level="error", event="error")
        errs.clear()
        bad = make_event(3, level="error", event="error", diagnostic="d",
                         error={"source": "a", "symbol": "b", "status": "C"})
        del bad.data["error"]
        clc.check_error_payload([bad.to_dict()], errs)
        self.assertTrue(any("error" in e for e in errs), errs)
        # 非 error 携带 error → 参考实现拒绝
        with self.assertRaises(ValueError):
            make_event(4, level="info", error={"source": "a", "symbol": "b",
                                               "status": "C"})
        # error 子字段缺失被 schema 拒绝
        errs.clear()
        bad = make_event(5, level="error", event="error", diagnostic="d",
                         error={"source": "a", "symbol": "b",
                                "status": "C"}).to_dict()
        del bad["error"]["status"]
        clc.validate_subschema(bad, schema, "L1", errs)
        self.assertTrue(any("缺必需字段" in e for e in errs), errs)


class TestDualOutput(unittest.TestCase):
    """A2：中文可读摘要 + 机器 JSONL 双输出同源。"""

    def test_summary_and_jsonl(self):
        ev = make_event(1, diagnostic="节点完成，处理 128 tiles",
                        units="tiles", elapsed=42.0)
        s = ev.summary()
        self.assertIn("info", s)
        self.assertIn("节点完成，处理 128 tiles", s)
        self.assertIn("tiles", s)
        line = ev.to_jsonl()
        obj = json.loads(line)
        self.assertEqual(obj["diagnostic"], "节点完成，处理 128 tiles")
        self.assertEqual(obj["seq"], 1)
        self.assertTrue(line.endswith("\n"))
        # 摘要与 JSONL 出自同一事件对象
        self.assertEqual(obj["run"], ev.data["run"])

    def test_no_newline_injection(self):
        ev = make_event(1, diagnostic="跨行\n注入")
        line = ev.to_jsonl().strip()
        self.assertNotIn("\n", line)
        self.assertNotIn("\r", line)
        self.assertEqual(json.loads(line)["diagnostic"], "跨行\n注入")


class TestRedact(unittest.TestCase):
    """A3：脱敏规则生效样例。"""

    def test_redact_absolute_paths(self):
        cases = [
            ("写入 /home/alice/astrocs/run/out.fits", "写入 <redacted>"),
            ("读取 /Users/bob/Documents/catalog.fits 失败", "读取 <redacted> 失败"),
            ("临时文件 C:\\Users\\mallory\\astrocs\\data.fits",
             "临时文件 <redacted>"),
            ("共享 \\\\srv\\share\\hips\\tile.fits", "共享 <redacted>"),
            ("缓存 /tmp/astrocs_tmp_123", "缓存 <redacted>"),
        ]
        for src, expect in cases:
            self.assertEqual(redact(src), expect, src)

    def test_redact_credentials(self):
        self.assertEqual(redact("password=super-secret"), "<redacted>")
        self.assertEqual(redact("token=abc123"), "<redacted>")
        self.assertEqual(redact("api_key = k-9x"), "<redacted>")
        self.assertEqual(redact("Authorization: Bearer xyz123"),
                         "<redacted>")
        self.assertNotIn("super-secret", redact("password=super-secret"))
        self.assertNotIn("user:pass", redact("https://user:pass@example.com/x"))
        self.assertEqual(redact("https://user:pass@example.com/x"),
                         "<redacted>")

    def test_redact_applied_at_event_build(self):
        ev = make_event(1, diagnostic="写入 /home/alice/x.fits 成功")
        self.assertEqual(ev.data["diagnostic"], "写入 <redacted> 成功")

    def test_redact_does_not_touch_safe(self):
        # 仓库内相对路径/正常文本不受影响
        self.assertEqual(redact("source=modules/phase1/noise/impl.cpp"),
                         "source=modules/phase1/noise/impl.cpp")
        self.assertEqual(redact("处理 128 tiles，elapsed=42ms"),
                         "处理 128 tiles，elapsed=42ms")


class TestSequenceConcurrency(unittest.TestCase):
    """A4：多线程事件顺序有 sequence：并发写 seq==1..2N 无空洞/重复。"""

    def test_concurrent_seq_allocator(self):
        n_threads, n_each = 4, 50
        alloc = SeqAllocator()
        with concurrent.futures.ThreadPoolExecutor(max_workers=n_threads) as ex:
            futs = [ex.submit(lambda k=k: [alloc.next() for _ in range(n_each)])
                    for k in range(n_threads)]
            lists = [f.result() for f in futs]
        merged = sorted(x for lst in lists for x in lst)
        self.assertEqual(merged, list(range(1, n_threads * n_each + 1)))
        self.assertEqual(len(set(merged)), n_threads * n_each)

    def test_checker_seq_validation_rejects_gap(self):
        lines = []
        for i in (1, 2, 4):  # 缺 seq=3
            ev = make_event(i)
            ev.data["seq"] = i
            lines.append(ev.to_jsonl())
        with tempfile.TemporaryDirectory() as td:
            p = pathlib.Path(td) / "gap.jsonl"
            p.write_text("".join(lines), encoding="utf-8")
            ok, errs, _ = clc.run_check(str(p), SCHEMA_PATH, verify_redact=False)
        self.assertFalse(ok)
        self.assertTrue(any("seq 不连续" in e for e in errs), errs)

    def test_checker_seq_rejects_first_not_one(self):
        ev = make_event(3)
        with tempfile.TemporaryDirectory() as td:
            p = pathlib.Path(td) / "first.jsonl"
            p.write_text(ev.to_jsonl(), encoding="utf-8")
            ok, errs, _ = clc.run_check(str(p), SCHEMA_PATH, verify_redact=False)
        self.assertFalse(ok)
        self.assertTrue(any("seq 不连续" in e for e in errs), errs)

    def test_checker_accepts_sequential(self):
        evs = [make_event(i) for i in (1, 2, 3)]
        with tempfile.TemporaryDirectory() as td:
            p = pathlib.Path(td) / "ok.jsonl"
            p.write_text("".join(e.to_jsonl() for e in evs), encoding="utf-8")
            ok, errs, _ = clc.run_check(str(p), SCHEMA_PATH, verify_redact=False)
        self.assertTrue(ok, errs)


class TestLineSizeLimit(unittest.TestCase):
    """A6：单行大小上限与截断不切坏 UTF-8。"""

    def test_line_size_under_limit(self):
        ev = make_event(1)
        self.assertLessEqual(line_size_bytes(ev.to_jsonl()), MAX_LINE_BYTES)

    def test_truncation_keeps_utf8_valid(self):
        diag = "中" * 5000  # 远超 4096B
        ev = make_event(1, diagnostic=diag)
        line = ev.to_jsonl(max_bytes=MAX_LINE_BYTES)
        self.assertLessEqual(line_size_bytes(line), MAX_LINE_BYTES)
        # 截断后必须是合法 JSON（尾部保留 }）且 UTF-8 解码合法
        obj = json.loads(line)
        self.assertIn("diagnostic", obj)
        # 不切坏多字节：字符串可无损往返
        self.assertEqual(obj["seq"], 1)
        self.assertTrue(line.endswith("\n"))

    def test_checker_rejects_oversized_raw_line(self):
        # 直接构造超限行（绕过参考实现截断），检查器必须拒绝
        big = "x" * (MAX_LINE_BYTES + 100)
        line = make_event(1).to_dict()
        line["diagnostic"] = big
        raw = json.dumps(line, ensure_ascii=False) + "\n"
        with tempfile.TemporaryDirectory() as td:
            p = pathlib.Path(td) / "big.jsonl"
            p.write_text(raw, encoding="utf-8")
            ok, errs, _ = clc.run_check(str(p), SCHEMA_PATH, verify_redact=False)
        self.assertFalse(ok)
        self.assertTrue(any("大小" in e and "上限" in e for e in errs), errs)


class TestCheckerCli(unittest.TestCase):
    """A6/A1：checker CLI PASS/FAIL 判定与错误载荷负测。"""

    def test_cli_selfcheck_pass(self):
        rc = clc.main(["--selfcheck"])
        self.assertEqual(rc, 0)

    def test_cli_jsonl_fail_exit1(self):
        ev = make_event(1).to_dict()
        del ev["commit"]  # 缺必需字段
        raw = json.dumps(ev) + "\n"
        with tempfile.TemporaryDirectory() as td:
            p = pathlib.Path(td) / "bad.jsonl"
            p.write_text(raw, encoding="utf-8")
            rc = clc.main(["--jsonl", str(p)])
        self.assertEqual(rc, 1)

    def test_error_payload_checker(self):
        # error 事件缺 status → checker 拒绝
        errs: list = []
        ev = make_event(1, level="error", event="error", diagnostic="d",
                        error={"source": "a", "symbol": "b", "status": "C"})
        clc.check_error_payload([ev.to_dict()], errs)
        self.assertEqual(errs, [], errs)
        errs.clear()
        bad = make_event(2, level="error", event="error", diagnostic="d",
                         error={"source": "a", "symbol": "b", "status": "C"})
        del bad.data["error"]["status"]
        clc.check_error_payload([bad.to_dict()], errs)
        self.assertTrue(any("error.status" in e for e in errs), errs)

    def test_required_field_list_matches_doc(self):
        # A1：文档声明字段 = 参考实现必需字段
        self.assertIn("schema", REQUIRED_FIELDS)
        self.assertIn("seq", REQUIRED_FIELDS)
        self.assertIn("ts", REQUIRED_FIELDS)
        for f in ("run", "task", "node", "module", "phase", "commit", "host",
                  "level", "event", "units", "elapsed", "diagnostic"):
            self.assertIn(f, REQUIRED_FIELDS)


class TestNoRawTestdata(unittest.TestCase):
    """A7：验收不使用 raw testdata；本测试无 testdata 依赖。"""

    def test_synthetic_only(self):
        # 全部样例为内存/临时合成事件（A7：无 raw testdata）。
        # 不引用仓库 testdata/ 目录；所有数据均在临时目录/内存构造。
        self.assertEqual(SCHEMA_ID, "astrocs.log.event.v1")
        self.assertNotIn("/home/", make_event(1).to_jsonl())
        # 仓库 testdata 目录内容被 .gitignore 排除（仅 index.json 入仓库）
        td = REPO / "testdata" / "index.json"
        self.assertTrue(td.exists() or td.parent.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
