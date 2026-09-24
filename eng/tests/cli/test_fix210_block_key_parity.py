#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FIX-210 D1 机器判据：blocks[] 块内键门 == 平铺单块简写键门（§3.3「两形态等价」）。

根因（E2E-201 D1，本任务复核）：lib/infrastructure/cli/parser.cpp 的
session_blocks_errors() 曾用 session_commands.h config_fields()（= --template
骨架键表）派生块内允许键，而平铺形态用 parser.cpp session_keys()（45 键）⇒
两门不是同一集合：normalize 块内拒绝 10 个本阶段科学键、mosaic 4 个、export 5 个。
真实 testdata 上因此**无任何可运行配置**：留着键 ⇒ rc=3 unknown key；去掉键 ⇒
cal 节点 fail-closed rc=2（MASTER_DARK_CONVENTION_UNDECLARED / MASTER_UNIT_MISMATCH）；
顶层平铺键与 blocks 互斥又被拒。

判据（**不新造第二份键名清单**：键宇宙从唯一声明现场派生）：
  ① 键宇宙 U = parser.cpp session_keys() ∪ parser.cpp block_keys()
     （机器读取器 = eng/tests/config/test_cfg004_unified_contract.py，本文件只引用）；
  ② 平铺门 F(S) / 块内门 B(S) 全部由**已构建 CLI 二进制**实测（探针法）：
     对每个 K ∈ U 构造「最小合法配置 + K」，K 被接受 ⇔ 无 unknown key 诊断
     （被接受时预检停在「输入路径不存在」rc=3 —— 该分支在未知键门之后）；
  ③ 等价：∀K ∈ U − BLOCK_ONLY：K ∈ F(S) ⇔ K ∈ B(S)；且 U ⊆ B(S)
     （= 块内键集 ⊇ 本会话全部 CLI 科学键）；BLOCK_ONLY={name} ⊆ B(S) ∧ ∉ F(S)；
  ④ 负例（能红）：把任一科学键从 B(S) 观测集删掉 ⇒ 判据必须报红并点名该键；
  ⑤ 探针判别力自证：真未知键 workers 必须被两门同时拒绝（否则探针恒绿=空门）；
  ⑥ 契约面残差登记（只减不增）：schema 块内键 − U == 已登记差异
     （normalize 的 sparse_snr_spacing_px / algorithm_drizzle_pixfrac：schema 声明
     而 CLI 两门都不认 ⇒ **既有** schema/CLI 分叉，非 FIX-210 引入；新增差异判红）。

跑法：python3 -B -m unittest discover -s eng/tests/cli -t eng/tests/cli -p "test_fix210_*.py"
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(REPO, "eng", "tests", "config"))
import test_cfg004_unified_contract as CFG004  # noqa: E402  （键表唯一声明的机器读取器）

from cli_test_hygiene import run_cwd  # noqa: E402

SCHEMAS = {
    "normalize": "eng/contracts/schemas/phase_config_normalize.schema.json",
    "mosaic": "eng/contracts/schemas/phase_config_mosaic.schema.json",
    "export": "eng/contracts/schemas/phase_config_export.schema.json",
}
# 第三件 = 一组输入帧（§3.3；键名逐字取 input_contract 声明）
INPUT_PROBE = {
    "normalize": ("input_lights", ["/nonexistent/FIX210/light.fits"]),
    "mosaic": ("hips_paths", ["/nonexistent/FIX210/hips"]),
    "export": ("source", {"hips_dir": "/nonexistent/FIX210/hips"}),
}
# E2E-201 D1 实测差集（**回归锁**，不是键门定义 —— 键门定义唯一 =
# session_keys() ∪ block_keys()）：这些键曾只在平铺形态可达、块内 rc=3。
# 判据 = 它们现在必须落在块内门里；本表只作回归证据，不参与键门推导。
D1_REGRESSION_KEYS = {
    "normalize": {"dark_optimization", "dark_scale_factor", "cosmetic", "master_units",
                  "master_scale", "master_flat_normalize", "master_flat_median_range",
                  "photometry", "sparse_snr_layer", "algorithm_psf_model"},
    "mosaic": {"upm_save_path", "persist_upm", "reject_profile", "algorithm_upm_gauge"},
    "export": {"max_tiles", "frame", "output_fits_path", "sampler_used", "mode"},
}
# 契约面残差（只减不增；既有分叉，非 FIX-210 引入）：schema 块内声明但 CLI 两门都不认。
# 登记依据：eng/contracts/schemas/phase_config_normalize.schema.json x-astrocs-notes
# 「登记在案的既存差异（本任务实测，未修）」（收口二选一：补 config_fields() 或改 schema）。
SCHEMA_NOT_IN_CLI_KEYS = {
    "normalize": {"sparse_snr_spacing_px", "algorithm_drizzle_pixfrac"},
    "mosaic": set(),
    "export": set(),
}
UNKNOWN_KEY_PROBE = "workers"   # 真未知键（cpu_profile 字段，phase_config 禁止）


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (("build", "acsd"), ("build", "cli", "acsd")):
        cand = os.path.join(REPO, *rel)
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "build", "acsd")


EXE = cli_binary()
# parser.cpp kAllowedKeys = V1 顶层合同白名单（**结构面**声明，非会话科学键表；
# 引用唯一实现而非手抄键名清单）：output_dir 由此在平铺形态可达。
FLAT_TOP_LEVEL_CONTRACT_KEYS = {"schema_version", "inputs", "output_dir", "phase3"}
# 平铺顶层专有、块内由 session_blocks_errors 显式跳过（与 test_cfg004 的
# 「schema_version 由两形态各自单列」同口径）⇒ 不进块内门判据。
FLAT_TOP_ONLY = FLAT_TOP_LEVEL_CONTRACT_KEYS - {"output_dir"}
SESSION_KEY_TABLE = CFG004.parser_session_keys() | CFG004.cli_block_keys()
# 块级专有键 = block_keys() − 会话键表 − 平铺顶层合同键（现为 {name}；output_dir
# 两形态都是必填键，属等价面而非块级专有面）。
BLOCK_ONLY = (CFG004.cli_block_keys() - CFG004.parser_session_keys()
              - FLAT_TOP_LEVEL_CONTRACT_KEYS)


def gate_violations(session, flat_gate, block_gate, universe, block_only,
                    regression_keys):
    """键门判据（纯函数：同一判据既跑真实观测也跑负例变异体）。

    返回违规清单（空 = 绿）。判据只比较**观测到的键集**，不假设实现。
    """
    v = []
    for k in sorted(universe - block_only):
        if (k in flat_gate) != (k in block_gate):
            v.append("%s: 两形态不等价 key=%s flat=%s block=%s"
                     % (session, k, k in flat_gate, k in block_gate))
    for k in sorted(universe):
        if k not in block_gate:
            v.append("%s: 块内键集缺科学键 %s（§3.3 块内键 = session_keys() ∪ block_keys()）"
                     % (session, k))
    for k in sorted(block_only):
        if k not in block_gate:
            v.append("%s: 块级专有键 %s 必须被块内门接受" % (session, k))
        if k in flat_gate:
            v.append("%s: 块级专有键 %s 不得被平铺门接受" % (session, k))
    for k in sorted(regression_keys):
        if k not in block_gate:
            v.append("%s: D1 回归键 %s 仍不可达（块内 rc=3）" % (session, k))
    return v


class TestFix210BlockKeyParity(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build acsd）"
        cls.tmp = tempfile.mkdtemp(prefix="fix210_parity_")
        cls._cache = {}

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── 探针：真实二进制观测（键门实测，非源码解析） ──
    def _probe(self, session, form, key):
        """返回 True（键被该形态的门接受）/ False（unknown key 判拒）/ 抛错（不可判定）。"""
        ck = (session, form, key)
        if ck in self._cache:
            return self._cache[ck]
        ik, iv = INPUT_PROBE[session]
        out = os.path.join(self.tmp, "out_%s_%s" % (session, form))
        if form == "flat":
            doc = {"schema_version": "1", ik: iv, "output_dir": out}
        else:
            doc = {"schema_version": "1",
                   "blocks": [{ik: iv, "output_dir": out}]}
        if key not in (ik, "output_dir"):
            (doc if form == "flat" else doc["blocks"][0])[key] = "PROBE"
        cfg = os.path.join(self.tmp, "probe_%s_%s_%s.json" % (session, form, key))
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(doc, fh, ensure_ascii=False)
        r = subprocess.run([EXE, session, "--json", cfg], capture_output=True,
                           text=True, timeout=300, cwd=run_cwd(), input="")
        unknown = "unknown key '%s'" % key
        missing = "blocked by missing/unreadable input path(s)"
        if unknown in r.stderr:
            verdict = False
        elif missing in r.stderr:
            verdict = True
        else:
            raise AssertionError(
                "探针不可判定（既非 unknown key 也非输入路径阻断）：session=%s form=%s key=%s "
                "rc=%d stderr_tail=%r" % (session, form, key, r.returncode,
                                          r.stderr.strip().splitlines()[-3:]))
        self._cache[ck] = verdict
        return verdict

    def _gates(self, session):
        flat = {k for k in SESSION_KEY_TABLE if self._probe(session, "flat", k)}
        block = {k for k in SESSION_KEY_TABLE if self._probe(session, "block", k)}
        return flat, block

    # ── ①③ 主判据：平铺门 == 块内门 ∧ 块内键集 ⊇ 全部会话科学键 ──
    def test_01_flat_and_block_gates_are_equivalent_per_session(self):
        for session in SCHEMAS:
            flat, block = self._gates(session)
            v = gate_violations(session, flat, block, SESSION_KEY_TABLE, BLOCK_ONLY,
                                D1_REGRESSION_KEYS[session])
            self.assertEqual([], v, "；".join(v))
            # 非空门自证：块内门必须真的认下 ≥40 个会话键（防解析/探针退化假绿）
            self.assertGreaterEqual(len(block), 40, "块内门异常窄（探针或键表退化）")

    # ── ④ 负例：删掉某科学键 ⇒ 判据必须报红并点名 ──
    def test_02_criterion_goes_red_when_a_science_key_is_dropped(self):
        for session in SCHEMAS:
            flat, block = self._gates(session)
            for dropped in sorted(D1_REGRESSION_KEYS[session]):
                mutant = set(block) - {dropped}
                v = gate_violations(session, flat, mutant, SESSION_KEY_TABLE,
                                    BLOCK_ONLY, D1_REGRESSION_KEYS[session])
                self.assertTrue(v, "%s：删掉块内键 %s 后判据仍判绿（判据无判别力）"
                                % (session, dropped))
                self.assertTrue(any(dropped in m for m in v),
                                "%s：判红未点名被删键 %s：%s" % (session, dropped, v))

    # ── ⑤ 探针判别力自证：真未知键必须被两门同时拒绝 ──
    def test_03_probe_detects_a_genuinely_unknown_key(self):
        for session in SCHEMAS:
            for form in ("flat", "block"):
                self.assertFalse(self._probe(session, form, UNKNOWN_KEY_PROBE),
                                 "%s/%s：真未知键 %s 未被拒 ⇒ 探针恒绿（空门）"
                                 % (session, form, UNKNOWN_KEY_PROBE))

    # ── ⑥ 契约面残差登记（只减不增） ──
    def test_04_schema_block_keys_outside_the_cli_key_table_are_registered(self):
        for session, rel in SCHEMAS.items():
            with open(os.path.join(REPO, rel), encoding="utf-8") as fh:
                schema = json.load(fh)
            props = set(schema["$defs"]["%s_block" % session]["properties"])
            residue = props - SESSION_KEY_TABLE
            self.assertEqual(SCHEMA_NOT_IN_CLI_KEYS[session], residue,
                             "%s：schema 块内键 − CLI 键表 的差异必须与登记表逐字一致"
                             "（新增即判红；收口后本表清空）：多=%s 少=%s"
                             % (session, sorted(residue - SCHEMA_NOT_IN_CLI_KEYS[session]),
                                sorted(SCHEMA_NOT_IN_CLI_KEYS[session] - residue)))

    # ── 平铺顶层专有键不得混进块内（两形态互斥面的一部分，防键门无限放宽） ──
    def test_05_flat_top_only_keys_stay_out_of_blocks(self):
        for session in SCHEMAS:
            for k in sorted(FLAT_TOP_ONLY):
                self.assertFalse(self._probe(session, "block", k),
                                 "%s：平铺顶层专有键 %s 不得被块内门接受" % (session, k))


if __name__ == "__main__":
    unittest.main(verbosity=2)
