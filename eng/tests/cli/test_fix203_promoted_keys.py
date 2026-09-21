#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FIX-203: 提升键不得静默变 no-op —— CLI 键表 / 生产消费 / 死键台账三方一致。

权威依据：
  * ASTROCS_DESIGN.md §3.3（三命令通用输入合同：**键名一律以命令行实际认的键为准**；
    「唯一声明 = lib/infrastructure/cli/session_commands.h 的 config_fields()」）；
  * 工程控制/RELEASE-03/tasks/FIX-203.md 步骤 3（提升后**生产零消费**的键必须在
    eng/ci/ledgers/dead_config_keys.json 登记「合同声明但生产零读取」，不得静默变 no-op）；
  * GAP_AUDIT G05（snr_path 不在 CLI 白名单）/ N02（algorithm_upm_gauge 死键）/
    N03（export 几何死键）。

判据（能红能绿）：
  L1 提升键必须在 CLI 键表里（session_keys() 白名单 ∪ config_fields() 键）；
  L2 precision **不得**出现在 CLI 键表里（禁止新造同义键；阶段一 = drizzle.precision_mode，
     阶段二/三 = 位深键 bitpix —— ASTROCS_DESIGN §3.3:256）；
  L3 提升键若在 lib/** 的**生产消费面**（CLI 键表文件之外）零命中 ⇒ 必须有
     dead_config_key:<key> 台账条目且五字段齐备（id/kind/reason/owner/exit_condition）。
"""
import os
import re
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, REPO)
sys.path.insert(0, os.path.join(REPO, "eng", "ci"))

import gate_common as gc  # noqa: E402

LEDGER_REL = "eng/ci/ledgers/dead_config_keys.json"
# CLI 键表 = 「只声明键名」的文件；其中的 token 命中**不算**生产消费。
CLI_TABLE_FILES = ("lib/infrastructure/cli/parser.cpp",
                   "lib/infrastructure/cli/session_commands.h")

# FIX-203 步骤 1：进 CLI 键面的提升键（合同声明名逐字，不新造同义键）。
PROMOTED = ("snr_path", "rotation_deg", "crpix_px")
# FIX-203 登记面：生产零读取 ⇒ 必须台账登记（含 N02 的 algorithm_upm_gauge 与
# 合同旧键 precision —— 后者被 CLI 拒绝，登记的是「合同声明但 CLI/生产零读取」）。
MUST_BE_LEDGERED = PROMOTED + ("algorithm_upm_gauge", "precision")
# 禁止项：不得新造同义键。
FORBIDDEN_CLI_KEYS = ("precision",)

_FIELD_RE = re.compile(r'\{\s*"([^"]+)"')


def _read(rel):
    with open(os.path.join(REPO, rel), encoding="utf-8", errors="replace") as fh:
        return fh.read()


def _brace_block(text, marker):
    """取 marker 之后第一对配平花括号内的文本（去注释后调用）。"""
    i = text.index(marker)
    j = text.index("{", i)
    depth = 0
    for k in range(j, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return text[j:k + 1]
    raise AssertionError("unbalanced braces after %r" % marker)


def cli_key_table():
    """CLI 键表（唯一声明）= session_keys() 白名单 ∪ 三会话 config_fields() 键。

    去注释后解析（注释里的键名不是声明）；与 eng/ci/check_config_consumed.py 同用
    gate_common.strip_comments，不另写一份剥离实现。
    """
    parser_src = gc.strip_comments(_read(CLI_TABLE_FILES[0]))
    sess_src = gc.strip_comments(_read(CLI_TABLE_FILES[1]))
    keys = set(re.findall(r'"([^"]+)"', _brace_block(parser_src, "session_keys()")))
    fields = set()
    for sess in ("kNormalize", "kMosaic", "kExport"):
        fields |= set(_FIELD_RE.findall(_brace_block(sess_src, "std::vector<ConfigField> " + sess)))
    return keys, fields


def production_blob_outside_cli_table():
    """lib/** 生产源码（去掉 CLI 键表文件）拼接 —— 语义 = 真正的消费面。"""
    chunks = []
    for path, _rel in gc.iter_source_files(os.path.join(REPO, "lib")):
        rel = os.path.relpath(str(path), REPO).replace(os.sep, "/")
        if rel in CLI_TABLE_FILES:
            continue
        chunks.append(path.read_text(encoding="utf-8", errors="replace"))
    assert chunks, "lib/** 生产源码为空（锚点失效）"
    return "\n".join(chunks)


def ledger_findings(entries_by_id, keys, consumed):
    """纯判据（便于负例自检）：零消费键缺登记 / 条目字段不全 ⇒ findings。"""
    findings = []
    for key in keys:
        if consumed(key):
            continue
        entry = entries_by_id.get("dead_config_key:%s" % key)
        if entry is None:
            findings.append("missing_ledger_entry:dead_config_key:%s" % key)
            continue
        for field in gc.LEDGER_REQUIRED_FIELDS:
            if not isinstance(entry.get(field), str) or not entry[field].strip():
                findings.append("ledger_entry_field_empty:dead_config_key:%s.%s" % (key, field))
    return findings


class TestFix203PromotedKeys(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.session_keys, cls.config_fields = cli_key_table()
        cls.cli_keys = cls.session_keys | cls.config_fields
        cls.blob = production_blob_outside_cli_table()
        cls.ledger = gc.load_ledger(os.path.join(REPO, LEDGER_REL), LEDGER_REL)

    def consumed_outside_cli_table(self, key):
        return ('"%s"' % key) in self.blob

    # ── L1 提升键进 CLI 键表 ──
    def test_01_promoted_keys_are_in_cli_key_table(self):
        for key in PROMOTED:
            self.assertIn(key, self.session_keys,
                          "session_keys() 白名单缺提升键 %s" % key)
            self.assertIn(key, self.config_fields,
                          "config_fields()（模板/help/块内键同源）缺提升键 %s" % key)

    # ── L2 禁止新造同义键 ──
    def test_02_precision_synonym_not_in_cli_key_table(self):
        for key in FORBIDDEN_CLI_KEYS:
            self.assertNotIn(key, self.session_keys,
                             "不得新造同义键：%s 出现在 session_keys()" % key)
            self.assertNotIn(key, self.config_fields,
                             "不得新造同义键：%s 出现在 config_fields()" % key)
        # 既有精度载体必须在键面上（复用而非新造）：阶段一 drizzle.precision_mode、
        # 阶段三 bitpix（ASTROCS_DESIGN §3.3:256）。
        self.assertIn("drizzle", self.config_fields)
        self.assertIn("bitpix", self.session_keys)

    # ── L3 零消费 ⇒ 必须登记（不得静默 no-op） ──
    def test_03_zero_consumption_keys_are_ledgered(self):
        findings = ledger_findings(self.ledger, MUST_BE_LEDGERED,
                                   self.consumed_outside_cli_table)
        self.assertEqual([], findings,
                         "提升键生产零消费却未登记/登记不全（静默 no-op 风险）: %s" % findings)
        status = {k: ("consumed" if self.consumed_outside_cli_table(k) else "ledgered")
                  for k in MUST_BE_LEDGERED}
        self.assertIn("ledgered", set(status.values()),
                      "全部提升键都已出现生产消费点 ⇒ 台账面可整体复核: %s" % status)

    # ── 判据能红能绿（负例自检） ──
    def test_04_ledger_gate_negative_control(self):
        consumed_none = lambda _k: False  # noqa: E731
        # 绿：真台账对零消费键无 finding
        self.assertEqual([], ledger_findings(self.ledger, MUST_BE_LEDGERED, consumed_none))
        # 红 1：空台账 ⇒ 每个键都报缺条目
        self.assertEqual(["missing_ledger_entry:dead_config_key:%s" % k
                          for k in MUST_BE_LEDGERED],
                         ledger_findings({}, MUST_BE_LEDGERED, consumed_none))
        # 红 2：条目字段为空 ⇒ 判红（台账不是后门）
        broken = dict(self.ledger)
        broken["dead_config_key:snr_path"] = dict(broken["dead_config_key:snr_path"],
                                                  exit_condition="   ")
        self.assertIn("ledger_entry_field_empty:dead_config_key:snr_path.exit_condition",
                      ledger_findings(broken, ("snr_path",), consumed_none))
        # 红 3：已消费的键不强制登记（判据不是恒真）
        self.assertEqual([], ledger_findings({}, ("bitpix",), lambda k: True))


if __name__ == "__main__":
    unittest.main(verbosity=2)
