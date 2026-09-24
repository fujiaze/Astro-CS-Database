#!/usr/bin/env python3
"""API-002 测试: CLI 协议合同机器门（按现行 tracked 权威重写）。

权威（现行，逐条对应）:
  * `docs/api/CLI_PROTOCOL_V1.md` —— 断言对象（§1 命令树 / §2 退出码 / §3 stdout 纪律 /
    §4 JSONL 事件流 / §5 取消与崩溃 / §6 检查器合同 / §7 output_dir）；
  * `ASTROCS_DESIGN.md` §7.1（唯一命令树）、§7.2（配置、事件与退出码）、
    §4.5（运行前预检三档）、§1.2（三命令平级独立）；
  * `lib/infrastructure/cli/exit_codes.h` —— 退出码**唯一源**（生产实现正本）；
  * `eng/contracts/schemas/jsonl_event_v1.schema.json` —— JSONL 事件流机器 schema（派生件）。

GAP_AUDIT G2-4 / D-8 处置（2026-09-22，GATE-502）:
  本文件旧版断言 `"全部删除且 rc=2"`、`" 10 "`、`"旧 Phase exe"`、`"以 04 为准"` 四条措辞，
  这些串在 tracked 权威文档（RELEASE-04 换版前后）中**都不存在** —— 旧断言对象是已不可得的
  04 控制包副本。现改为：对现行文档做**结构化解析**（代码块 / 枚举表 / 必含字段表 / 编号条款）
  并与生产唯一源（exit_codes.h / jsonl schema / ASTROCS_DESIGN §7.1）**交叉核对**；
  任何一处漂移都会判红（负例见 GATE-502 回执 §红绿双向证据）。
"""
import os
import re
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DOC = os.path.join(REPO, "docs", "api", "CLI_PROTOCOL_V1.md")
DESIGN = os.path.join(REPO, "ASTROCS_DESIGN.md")
EXIT_CODES_H = os.path.join(REPO, "lib", "infrastructure", "cli", "exit_codes.h")
JSONL_SCHEMA = os.path.join(REPO, "eng", "contracts", "schemas", "jsonl_event_v1.schema.json")

# §1 冻结命令树（ASTROCS_DESIGN §7.1 的唯一命令树在本合同的落地形态）。
SPEC_TREE = [
    "acsd --version [--json]",
    "acsd normalize (--json <config.json> | --template [-o <path>] | --help)",
    "acsd mosaic (--json <config.json> | --template [-o <path>] | --help)",
    "acsd export (--json <config.json> | --template [-o <path>] | --help)",
    "acsd help",
    "acsd doctor [--json]",
    "acsd benchmark",
]
# 旧命令面（§1 显式声明不在命令面上，调用 rc=2）。
RETIRED_ALIASES = ["phase1|2|3", "config *", "modules *", "selftest", "test synthetic",
                   "verify*", "drizzle", "benchmark cpu", "verify-profile", "hardware inspect"]


def read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def fenced_block(text, heading_prefix):
    """取 heading_prefix 开头的小节之后第一个 ```text 代码块（逐行，去空行）。"""
    lines = text.splitlines()
    start = next((i for i, l in enumerate(lines) if l.startswith(heading_prefix)), None)
    if start is None:
        raise AssertionError("文档缺小节 %r" % heading_prefix)
    i = start
    while i < len(lines) and not lines[i].startswith("```"):
        i += 1
    if i >= len(lines):
        raise AssertionError("%s 小节后无代码块" % heading_prefix)
    body = []
    for l in lines[i + 1:]:
        if l.startswith("```"):
            return body
        if l.strip():
            body.append(l.rstrip())
    raise AssertionError("%s 代码块未闭合" % heading_prefix)


def section(text, heading_prefix, next_prefix="## "):
    """取某小节正文（不含标题行，含到下一个同级标题前）。"""
    lines = text.splitlines()
    start = next((i for i, l in enumerate(lines) if l.startswith(heading_prefix)), None)
    if start is None:
        raise AssertionError("文档缺小节 %r" % heading_prefix)
    out = []
    for l in lines[start + 1:]:
        if l.startswith(next_prefix) and not l.startswith(heading_prefix):
            break
        out.append(l)
    return "\n".join(out)


class TestCliProtocol(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.s = read(DOC)
        cls.design = read(DESIGN)
        cls.preamble = cls.s.split("## 1 ", 1)[0]
        cls.sec1 = section(cls.s, "## 1 ", "## ")
        cls.sec2 = section(cls.s, "## 2 ", "## ")

    # ── 1. §1 命令树 == §7.1 唯一命令树（逐行冻结 + 旧别名声明不在命令面） ──
    def test_01_command_tree_is_frozen_spec_tree(self):
        got = fenced_block(self.s, "## 1 ")
        self.assertEqual(SPEC_TREE, got, "§1 命令树必须与冻结七行逐行一致（顺序与拼写）")
        for legacy in RETIRED_ALIASES:
            self.assertIn(legacy, self.preamble, "文档头缺旧命令面删除声明: %s" % legacy)
        self.assertIn("不在命令面上", self.preamble)
        self.assertIn("返回 rc=2", self.preamble)
        # 发布 manifest 恰一 acsd（§6-5 的联动声明必须在 §1 正文可见）
        self.assertIn("恰一 acsd", self.s)

    # ── 2. §1 与最高设计 §7.1 交叉核对（两份 tracked 权威不得漂移） ──
    def test_02_command_tree_matches_design_71(self):
        design_lines = fenced_block(self.design, "### 7.1")
        design_cmds = set()
        for l in design_lines:
            tok = l.split()[0]
            design_cmds.add(tok)
        self.assertEqual(
            {"normalize", "mosaic", "export", "help", "--version", "doctor", "benchmark"},
            design_cmds, "§7.1 命令集漂移")
        # 从**文档正文**取命令集（不是从本文件的冻结常量），这样文档漂移与设计漂移
        # 都会被这条抓住；常量侧由 test_01 逐行锁。
        got = fenced_block(self.s, "## 1 ")
        doc_cmds = {l.split()[1] for l in got}
        self.assertEqual(design_cmds, doc_cmds, "CLI_PROTOCOL §1 与 ASTROCS_DESIGN §7.1 命令集不一致")
        # 三命令的三种形态在 §7.1 逐条存在
        for cmd in ("normalize", "mosaic", "export"):
            for form in ("--json <config.json>", "--template [-o <path>]", "--help"):
                self.assertIn("%s %s" % (cmd, form), "\n".join(design_lines),
                              "§7.1 缺 %s %s" % (cmd, form))

    # ── 3. §2 退出码 == exit_codes.h 唯一源（11 条冻结） ──
    def test_03_exit_codes_match_single_source(self):
        header = read(EXIT_CODES_H)
        pairs = re.findall(r"^\s*([A-Z_]+)\s*=\s*(\d+)\s*,", header, re.M)
        self.assertEqual(11, len(pairs), "exit_codes.h 必须是 11 条冻结退出码")
        codes = {int(v) for _, v in pairs}
        self.assertEqual({0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 70}, codes, "退出码集合漂移")
        self.assertIn("## 2 退出码(全 11 条冻结", self.s, "§2 标题必须声明 11 条冻结")
        self.assertIn("exit_codes.h", self.sec2, "§2 必须声明唯一源")
        self.assertIn("唯一源", self.sec2)
        for c in sorted(codes):
            self.assertRegex(self.sec2, r"(?<![0-9])%d(?![0-9])" % c,
                             "§2 缺退出码 %d（与 exit_codes.h 漂移）" % c)

    # ── 4. §4 JSONL 必含字段 == 机器 schema required ──
    def test_04_jsonl_required_fields_match_schema(self):
        import json
        schema = json.loads(read(JSONL_SCHEMA))
        sec4 = section(self.s, "## 4 ", "## ")
        m = re.search(r"每行必含:\s*`([^`]+)`", sec4)
        self.assertIsNotNone(m, "§4 缺「每行必含」字段清单")
        doc_fields = [f.strip() for f in m.group(1).split(",") if f.strip()]
        self.assertEqual(sorted(schema["required"]), sorted(doc_fields),
                         "§4 必含字段与 jsonl_event_v1.schema.json required 不一致")
        self.assertEqual(10, len(doc_fields), "JSONL 必含字段应为 10 个")
        self.assertIn("从 0 单调递增", sec4, "§4 缺 sequence 单调递增语义")

    # ── 5. §6 检查器合同 = 恰 6 条，且引用的检查器真实存在（fail-closed） ──
    def test_05_checker_contract_six_items(self):
        sec6 = section(self.s, "## 6 ", "## ")
        items = re.findall(r"^(\d+)\.\s+(.*)$", sec6, re.M)
        self.assertEqual(6, len(items), "§6 检查器合同必须恰 6 条，实得 %d" % len(items))
        self.assertEqual([str(i) for i in range(1, 7)], [n for n, _ in items],
                         "§6 编号必须为 1..6 且连续")
        keys = ["--help", "schema", "唯一源", "追溯", "manifest", "双平台"]
        for (n, body), k in zip(items, keys):
            self.assertIn(k, body, "§6-%s 缺关键判据 %r" % (n, k))
        self.assertIn("exit_codes.h", sec6, "§6-3 必须点名退出码唯一源")
        # 锚存活（ENGINEERING_SPEC §10）：合同点名的检查器路径必须在树中
        refs = re.findall(r"`(eng/tools/[A-Za-z0-9_./-]+\.py)`", self.s)
        self.assertTrue(refs, "§6 必须点名机器检查器脚本路径")
        for rel in refs:
            self.assertTrue(os.path.isfile(os.path.join(REPO, rel)),
                            "合同点名的检查器不存在（悬空引用）: %s" % rel)

    # ── 6. §5 取消与崩溃语义 ──
    def test_06_cancel_crash_semantics(self):
        sec5 = section(self.s, "## 5 ", "## ")
        self.assertIn("acs_cancel", sec5, "取消必须走协作取消令牌")
        self.assertIn("exit 9", sec5, "取消必须 exit 9")
        self.assertIn("incomplete manifest", sec5, "取消必须写 incomplete manifest")
        self.assertIn("不得留下看似完整", sec5, "取消不得留下看似完整的产品")
        self.assertIn("70", sec5, "未捕获异常必须 70")
        self.assertIn("脱敏 crash report", sec5, "70 必须出脱敏 crash report")
        self.assertIn("不泄露凭据", sec5)

    # ── 7. 上游权威指针存活（替换旧 "以 04 为准" 自指断言） ──
    def test_07_upstream_authority_sections_exist(self):
        self.assertIn("ASTROCS_DESIGN.md §7.1", self.s, "文档头必须声明上游 §7.1（命令树）")
        self.assertIn("§7.2", self.s, "文档头必须声明上游 §7.2（配置、事件与退出码）")
        for h in ("### 7.1 命令树（唯一）", "### 7.2 配置、事件与退出码", "### 4.5 运行前预检（三个命令通用）"):
            self.assertIn(h, self.design, "最高设计缺小节 %r（上游指针失效）" % h)
        # §1 命令语义必须声明三命令平级独立 / 无断点续算（§1.2）
        self.assertIn("无断点续算", self.sec1, "§1 缺 §1.2 的「无断点续算」语义")
        self.assertIn("禁止", self.sec1, "§1 必须声明禁止隐式串接")


if __name__ == "__main__":
    unittest.main(verbosity=2)
