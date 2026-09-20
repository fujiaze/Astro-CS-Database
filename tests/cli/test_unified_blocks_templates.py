#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""FIX-207 三命令同构输入合同（§9.71 裁决 2）CLI 面机器门。

锁的验收门（可机器复跑）：
  ① 三命令 <cmd> --template 输出通过各自 schema（模板 = 同一份键表的产物）
  ② 三命令 <cmd> --json <blocks> 预检页无 [error]（唯一非零来自确认环节——CLI 无
     precheck-only 退出码，precheck 页之后即确认/运行，故判据 = 「页全 correct/optimize
     + 阻断原因是 not confirmed」）
  ③ <cmd> --template 输出（补齐真实输入路径后）同样走到「只差确认」——往返一致
  ④ config/templates/*.json（仓库模板文件）补齐路径后同样被 CLI 接受
  ⑤ 旧合同形态 {phase_name, config, inputs[]} ⇒ rc=3 + 迁移提示（三命令，§9.71 裁决 2 定案 3/4）
  ⑥ 块内 weight_mode ⇒ rc=3（§9.73 裁决 A44）
  ⑦ blocks[] 与平铺单块简写同时出现 ⇒ rc=2「mutually exclusive」（不静默取一）

跑法：python3 -B -m unittest discover -s tests/cli -t tests/cli -p "test_unified_blocks_templates.py"
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tests", "config"))
import cfg_common as C  # noqa: E402
import test_cfg004_unified_contract as CFG004  # noqa: E402  （登记差异表唯一来源）

from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402

SCHEMAS = {
    "normalize": "contracts/schemas/phase_config_normalize.schema.json",
    "mosaic": "contracts/schemas/phase_config_mosaic.schema.json",
    "export": "contracts/schemas/phase_config_export.schema.json",
}
TEMPLATES = {
    "normalize": "config/templates/normalize.phase_config.json",
    "mosaic": "config/templates/mosaic.phase_config.json",
    "export": "config/templates/export.phase_config.json",
}
INPUT_KEY = {"normalize": "input_lights", "mosaic": "hips_paths", "export": "source"}
FORM_ERROR_MARKERS = ("unknown key", "mutually exclusive", "phase_config contract form",
                      "retired per-frame", "not allowed inside a block")
# 已登记偏差（CLI 侧 lib/infrastructure/cli/session_commands.h，不在 FIX-207 文件域）：
#   ① CLI --template 的 mosaic/export 输入占位为**空**（hips_paths: [] / source.hips_dir: ""），
#      与合同「输入必填非空」不符 ⇒ 模板产物自身不通过合同 schema，且未填路径时预检判 error
#      「没有输入产品」。normalize 的模板用非空占位（path/to/light1.fits）⇒ 无此偏差。
#   ② CLI --template（export）**不带 output_mode**（config_fields 里该键 json==nullptr），
#      而合同与运行期都要求显式声明：旧合同 $defs.export_config.required 含 output_mode
#      （docs/contracts/CONFIG_CONTRACT.md:81），p3 resample 节点缺键即 REJECT（FZ-P3-MODES）
#      ⇒ 合同按 fail-closed 判必填，CLI 骨架缺值 ⇒ 登记为偏差（tests/cli/test_phase3_inprocess.py
#      ::test_11_template_runnable_without_force 即因此红）。
# 本表是**精确登记**（多一条/少一条都判红）。收口（lib/** 后续项，各一行）：
#   ① 两处输入占位改非空；② kExport 的 output_mode 由 nullptr 改为 "\"surface_brightness\""
#      （值取 config_registry.json#16_fits_output/mode.declared_default）。
CLI_TEMPLATE_DEVIATIONS = {
    "normalize": set(),
    "mosaic": {("hips_paths", "minItems")},
    "export": {("source", "hips_dir", "minLength"), ("required: missing 'output_mode'",)},
}
CLI_TEMPLATE_INPUT_ERRORS = {
    "normalize": "指向的路径不存在",
    "mosaic": "hips_paths 为空",
    "export": "source.hips_dir 为空",
}


def cli_binary():
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in (("build", "astrocs"), ("build", "cli", "astrocs")):
        cand = os.path.join(REPO, *rel)
        if os.path.isfile(cand):
            return cand
    return os.path.join(REPO, "build", "astrocs")


EXE = cli_binary()


class TestUnifiedBlocksCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="unified_blocks_")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── helpers ──
    def _touch(self, name):
        p = os.path.join(self.tmp, name)
        with open(p, "wb") as fh:
            fh.write(b"")
        return p

    def _dir(self, name):
        p = os.path.join(self.tmp, name)
        os.makedirs(p, exist_ok=True)
        return p

    def _write(self, name, doc):
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh, ensure_ascii=False)
        return p

    def _run(self, cmd, *args, confirm=False, timeout=600):
        return subprocess.run([EXE, cmd, *args], capture_output=True, text=True,
                              timeout=timeout, cwd=run_cwd(),
                              input="yes\n" if confirm else None)

    def _fill(self, node, key=None, tag=""):
        """把模板占位路径替换成真实存在路径（预检只核存在性/可读性）。"""
        if isinstance(node, dict):
            return {k: self._fill(v, k, tag) for k, v in node.items()}
        if isinstance(node, list):
            return [self._fill(v, key, tag) for v in node]
        if isinstance(node, str) and node.startswith("path/to/"):
            leaf = node.rsplit("/", 1)[-1]
            if key in ("hips_dir", "gaia_data_dir") or "hips" in leaf:
                return self._dir("%s_%s" % (tag, leaf))
            if key == "output_dir":
                return os.path.join(self.tmp, "%s_out_%s" % (tag, leaf))
            return self._touch("%s_%s" % (tag, leaf))
        return node

    def _filled_template(self, cmd, tag):
        return self._fill(json.loads(json.dumps(C.load_json(TEMPLATES[cmd]))), tag=tag)

    def _assert_precheck_passed(self, cmd, cfg, label):
        """预检通过判据：页无 [error]，阻断原因只能是确认环节。"""
        r = self._run(cmd, "--json", cfg)
        self.assertNotIn("[error]", r.stderr, "%s：预检页出现 error：\n%s" % (label, r.stderr))
        for marker in FORM_ERROR_MARKERS:
            self.assertNotIn(marker, r.stderr, "%s：形态被拒（%s）：\n%s" % (label, marker, r.stderr))
        self.assertIn("not confirmed", r.stderr,
                      "%s：预检未通过（阻断原因不是确认环节，rc=%d）：\n%s" % (label, r.returncode, r.stderr))
        self.assertEqual(2, r.returncode, "%s：确认前退出码应为 2（配置/确认面）" % label)
        return r

    def _blocks_doc(self, cmd, tag):
        if cmd == "normalize":
            return {"schema_version": "1", "blocks": [{
                "name": "n1",
                "input_lights": [self._touch("%s_l1.fits" % tag), self._touch("%s_l2.fits" % tag)],
                "master_bias": self._touch("%s_bias.fits" % tag),
                "master_dark": self._touch("%s_dark.fits" % tag),
                "master_flat": self._touch("%s_flat.fits" % tag),
                "output_dir": os.path.join(self.tmp, tag + "_out"),
                "drizzle": {"nested": 1, "pixfrac": 1.0, "precision_mode": 1},
                "filter_passband": "Baader R"}]}
        if cmd == "mosaic":
            return {"schema_version": "1", "blocks": [{
                "name": "m1",
                "hips_paths": [self._dir(tag + "_hips_a"), self._dir(tag + "_hips_b")],
                "output_dir": os.path.join(self.tmp, tag + "_out")}]}
        return {"schema_version": "1", "blocks": [{
            "name": "e1",
            "source": {"hips_dir": self._dir(tag + "_hips")},
            "output_dir": os.path.join(self.tmp, tag + "_out"),
            "center": {"ra_deg": 0.0, "dec_deg": 0.0},
            "width_px": 1024, "height_px": 1024, "scale_deg_per_px": 0.001,
            "output_mode": "surface_brightness"}]}

    def _cli_template(self, cmd):
        r = self._run(cmd, "--template")
        self.assertEqual(0, r.returncode, "%s --template rc=%d：%s" % (cmd, r.returncode, r.stderr))
        return json.loads(r.stdout)

    def _fill_cli_template(self, cmd, doc, tag):
        """补齐 CLI 模板：非空路径占位 + 空输入占位（空占位是已登记偏差）。"""
        doc = self._fill(json.loads(json.dumps(doc)), tag=tag)
        hosts = doc["blocks"] if isinstance(doc.get("blocks"), list) else [doc]
        for host in hosts:
            if cmd == "mosaic" and not host.get("hips_paths"):
                host["hips_paths"] = [self._dir(tag + "_hips")]
            if cmd == "export" and isinstance(host.get("source"), dict) and not host["source"].get("hips_dir"):
                host["source"]["hips_dir"] = self._dir(tag + "_hips")
            if cmd == "normalize" and not host.get("input_lights"):
                host["input_lights"] = [self._touch(tag + "_l.fits")]
        return doc

    # ── ① 模板通过各自 schema（只允许已登记的「空输入占位」偏差） ──
    def test_01_template_output_passes_its_own_schema(self):
        for cmd, schema_rel in SCHEMAS.items():
            doc = self._cli_template(cmd)
            errs = C.validate(C.load_json(schema_rel), doc)
            got = {tuple(str(p) for p in path) + (msg,) for path, msg in errs}
            self.assertEqual(CLI_TEMPLATE_DEVIATIONS[cmd], got,
                             "%s --template 输出与 %s 的偏差集变化（只减不增）：%s"
                             % (cmd, schema_rel, sorted(got)))

    # ── ②/③ --json <blocks> 预检通过；模板往返一致 ──
    def test_02_blocks_precheck_passes_for_all_three_commands(self):
        for cmd in SCHEMAS:
            cfg = self._write("%s_blocks.json" % cmd, self._blocks_doc(cmd, cmd))
            self._assert_precheck_passed(cmd, cfg, "%s blocks[]" % cmd)

    def test_03_template_output_roundtrips_through_json(self):
        """<cmd> --template 输出：形态/键集被 CLI 接受；补齐输入后走到「只差确认」。"""
        for cmd in SCHEMAS:
            raw = self._write("%s_tpl_raw.json" % cmd, self._cli_template(cmd))
            r = self._run(cmd, "--json", raw)
            for marker in FORM_ERROR_MARKERS:
                self.assertNotIn(marker, r.stderr, "%s --template 产物被形态门拒绝（%s）" % (cmd, marker))
            err_lines = [l for l in r.stderr.splitlines() if "[error]" in l]
            self.assertTrue(err_lines, "%s --template 占位路径应报输入缺失（判据非恒真）" % cmd)
            for line in err_lines:
                self.assertIn(CLI_TEMPLATE_INPUT_ERRORS[cmd], line,
                              "%s --template 出现非输入占位的 error：%s" % (cmd, line))
            filled = self._write("%s_tpl_filled.json" % cmd,
                                 self._fill_cli_template(cmd, self._cli_template(cmd), "rt_" + cmd))
            self._assert_precheck_passed(cmd, filled, "%s --template 补齐后往返" % cmd)

    def test_04_repo_template_files_are_cli_accepted(self):
        for cmd in SCHEMAS:
            tpl = self._filled_template(cmd, "file_" + cmd)
            cfg = self._write("%s_file_tpl.json" % cmd, tpl)
            self._assert_precheck_passed(cmd, cfg, "%s 模板文件" % cmd)

    # ── ⑧ help 与模板/ schema 同一份键表 ──
    def test_08_help_and_template_share_the_single_key_table(self):
        """--help 字段说明与 schema 块内键集逐字同面（模板/help 同源，禁第二份键名清单）。"""
        for cmd, schema_rel in SCHEMAS.items():
            r = self._run(cmd, "--help")
            self.assertEqual(0, r.returncode, "%s --help rc=%d" % (cmd, r.returncode))
            keys = set()
            for line in r.stdout.splitlines():
                line = line.strip()
                if " — " not in line or line.startswith("blocks —"):
                    continue
                head = line.split(" — ")[0].strip()
                head = head.replace(" (optional)", "").strip()
                if head.startswith("blocks[]."):
                    head = head[len("blocks[]."):]
                keys.add(head.split(".")[0])
            self.assertTrue(keys, "%s --help 未列出任何字段（判据不得恒真）" % cmd)
            expect = set(CFG004.cli_config_fields()[cmd])
            self.assertEqual(expect, keys,
                             "%s --help 字段与 config_fields() 单一键表不一致：help-only=%s table-only=%s"
                             % (cmd, sorted(keys - expect), sorted(expect - keys)))
            schema = C.load_json(schema_rel)
            block_props = set(schema["$defs"]["%s_block" % cmd]["properties"])
            self.assertTrue((expect - {"schema_version"}) <= block_props,
                            "%s --help 的字段必须全部落在 schema 块内键集内：%s"
                            % (cmd, sorted((expect - {"schema_version"}) - block_props)))

    # ── ⑤ 旧形态显式拒绝 + 迁移提示 ──
    def test_05_legacy_contract_form_is_rejected_with_migration_hint(self):
        for cmd in SCHEMAS:
            legacy = C.load_json("tests/config/fixtures/positive/%s_legacy_contract.phase_config.json" % cmd) \
                if cmd != "normalize" else {
                    "phase_name": "normalize", "config": {"output_dir": "o"},
                    "inputs": [{"light": "l.fits"}]}
            cfg = self._write("%s_legacy.json" % cmd, legacy)
            r = self._run(cmd, "--json", cfg)
            self.assertEqual(3, r.returncode, "%s 旧形态必须 rc=3：%s" % (cmd, r.stderr))
            self.assertIn("phase_config", r.stderr)
            self.assertTrue("migrate" in r.stderr or "not the CLI session form" in r.stderr,
                            "%s 旧形态必须给迁移提示：%s" % (cmd, r.stderr))
            self.assertIn("blocks", r.stderr, "%s 迁移提示必须指向块形态" % cmd)

    # ── ⑥ A44 ──
    def test_06_weight_mode_in_block_is_rejected(self):
        for cmd in SCHEMAS:
            doc = self._blocks_doc(cmd, "a44_" + cmd)
            doc["blocks"][0]["weight_mode"] = 1
            cfg = self._write("%s_wm.json" % cmd, doc)
            r = self._run(cmd, "--json", cfg)
            self.assertEqual(3, r.returncode, "%s 块内 weight_mode 必须 rc=3：%s" % (cmd, r.stderr))
            self.assertIn("weight_mode", r.stderr)

    # ── ⑦ 互斥 ──
    def test_07_blocks_and_flat_keys_are_mutually_exclusive(self):
        for cmd in SCHEMAS:
            doc = self._blocks_doc(cmd, "mix_" + cmd)
            doc["output_dir"] = os.path.join(self.tmp, "flat_out")
            cfg = self._write("%s_mixed.json" % cmd, doc)
            r = self._run(cmd, "--json", cfg)
            self.assertEqual(2, r.returncode, "%s blocks+平铺必须 rc=2：%s" % (cmd, r.stderr))
            self.assertIn("mutually exclusive", r.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)