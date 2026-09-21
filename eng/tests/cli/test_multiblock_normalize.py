#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CLI-MULTIBLOCK（GAP_AUDIT §9.68 负责人裁决 2026-09-20）CLI 行为门。

负责人原话（逐字）：「如果多套设备，多个通道需要不同的校准场以及运行参数的话。
可以在一个 json 里写很多块。就像一个 main 下面可以写很多个函数一样。不需要每条
都详细写出校准帧。**同一组校准帧和运行参数应该支持一组 light**」。

本模块锁 CLI 面（schema 面见 eng/tests/config/test_cfg003_multiblock.py）：
  ① 多块解析通过（预检页逐块列出，结构门不报错）
  ② 平铺单块简写仍通过（向后兼容）
  ③ blocks 与平铺键同时出现 → 报错（互斥，不静默取一）
  ④ 块缺 output_dir → 报错
  ⑤ 块内未知键 → 报错
  ⑥ 逐帧 inputs[] 形态 → 明确拒绝 + 迁移提示
  ⑦ 多块各自产出独立 manifest（负例：不得合并成一个）

跑法：python3 -m unittest discover -s eng/tests/cli -t eng/tests/cli
"""
import json
import os
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tests.cli.cli_test_hygiene import run_cwd  # noqa: E402


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


class TestMultiBlockNormalize(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（cmake -S . -B build && ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="multiblock_")
        # 预检只核「路径存在/可读」，不解析 FITS 内容 ⇒ 空文件足以走到逐块派发面。
        cls.bias = cls._touch("bias.fits")
        cls.dark = cls._touch("dark.fits")
        cls.flat_red = cls._touch("flat_red.fits")
        cls.flat_ha = cls._touch("flat_ha.fits")
        cls.light1 = cls._touch("light1.fits")
        cls.light2 = cls._touch("light2.fits")
        cls.light3 = cls._touch("light3.fits")

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    @classmethod
    def _touch(cls, name):
        p = os.path.join(cls.tmp, name)
        with open(p, "wb") as fh:
            fh.write(b"")
        return p

    def _run(self, *args, timeout=600, confirm=False):
        """confirm=True: 走运行确认页（stdin 回 'yes'）—— 预检绿/橙/红页面只在
        非 -y 的确认路径打印（confirm_run），因此「逐块列出」的判据要用它。"""
        return subprocess.run([EXE, "normalize", *args], capture_output=True, text=True,
                              timeout=timeout, cwd=run_cwd(),
                              input="yes\n" if confirm else None)

    def _write(self, name, doc):
        p = os.path.join(self.tmp, name)
        with open(p, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return p

    def _multi_block_doc(self, tag):
        return {
            "schema_version": "1",
            "blocks": [
                {"name": "red",
                 "input_lights": [self.light1, self.light2],
                 "master_bias": self.bias, "master_dark": self.dark,
                 "master_flat": self.flat_red,
                 "output_dir": os.path.join(self.tmp, tag + "_red"),
                 "filter_passband": "Baader R"},
                {"name": "ha",
                 "input_lights": [self.light3],
                 "master_bias": self.bias, "master_dark": self.dark,
                 "master_flat": self.flat_ha,
                 "output_dir": os.path.join(self.tmp, tag + "_ha"),
                 "filter_passband": "Baader 7nm H-alpha"},
            ],
        }

    # ── ① 多块解析通过 ──
    def test_01_multi_block_parses_and_dispatches_per_block(self):
        doc = self._multi_block_doc("ok")
        cfg = self._write("mb_ok.json", doc)
        r = self._run("--json", cfg, confirm=True)
        # 预检页逐块列出（块归属 + 块级 output_dir + light 条目数）
        self.assertIn("[correct] blocks[0] 'red' output_dir = " + doc["blocks"][0]["output_dir"],
                      r.stderr)
        self.assertIn("[correct] blocks[1] 'ha' output_dir = " + doc["blocks"][1]["output_dir"],
                      r.stderr)
        self.assertNotIn("is required (block-level", r.stderr)
        self.assertNotIn("unknown key", r.stderr)
        self.assertNotIn("mutually exclusive", r.stderr)
        # 逐块派发（日志给出块归属）
        self.assertIn("astrocs: normalize block 1/2 'red' → ", r.stderr)
        self.assertIn("astrocs: normalize block 2/2 'ha' → ", r.stderr)
        self.assertNotEqual(r.returncode, 70, "多块配置不得触发未分类错误")

    # ── ② 平铺单块简写仍通过（向后兼容） ──
    def test_02_flat_single_block_shorthand_still_works(self):
        out = os.path.join(self.tmp, "flat_out")
        cfg = self._write("flat.json", {
            "schema_version": "1",
            "input_lights": [self.light1],
            "master_bias": self.bias, "master_dark": self.dark, "master_flat": self.flat_red,
            "output_dir": out,
            "filter_passband": "Baader R",
        })
        r = self._run("--json", cfg, confirm=True)
        self.assertIn("[correct] output_dir = " + out, r.stderr)
        self.assertIn("[correct] input_lights 条目数 = 1", r.stderr)
        self.assertNotIn("blocks[", r.stderr, "平铺简写不得被当成多块形态")
        self.assertNotIn("unknown key", r.stderr)
        self.assertNotEqual(r.returncode, 70)
        # 简写 = 单块：manifest 不带 block 子对象（旧形态逐字节兼容）
        mans = [f for f in os.listdir(out) if f.startswith("astrocs_run_")]
        self.assertEqual(1, len(mans), "单块简写必须恰一份 run manifest")
        with open(os.path.join(out, mans[0]), encoding="utf-8") as fh:
            self.assertNotIn("block", json.load(fh))

    # ── ③ 两形态互斥 ──
    def test_03_blocks_and_flat_keys_together_is_rejected(self):
        doc = self._multi_block_doc("mixed")
        doc["input_lights"] = [self.light1]          # 平铺键与 blocks 同时出现
        doc["output_dir"] = os.path.join(self.tmp, "mixed_flat")
        cfg = self._write("mb_mixed.json", doc)
        r = self._run("--json", cfg, "-y")
        self.assertEqual(2, r.returncode, r.stderr[-400:])
        self.assertIn("mutually exclusive", r.stderr)
        self.assertIn("blocks", r.stderr)

    # ── ④ 块缺 output_dir ──
    def test_04_block_without_output_dir_is_rejected(self):
        doc = self._multi_block_doc("noout")
        del doc["blocks"][1]["output_dir"]
        cfg = self._write("mb_noout.json", doc)
        r = self._run("--json", cfg, "-y")
        self.assertEqual(2, r.returncode, r.stderr[-400:])
        self.assertIn("blocks[1].output_dir is required", r.stderr)

    # ── ⑤ 块内未知键 ──
    def test_05_block_unknown_key_is_rejected(self):
        doc = self._multi_block_doc("unk")
        doc["blocks"][0]["workers"] = 8              # cpu_profile 字段混入块内
        cfg = self._write("mb_unk.json", doc)
        r = self._run("--json", cfg, "-y")
        self.assertEqual(3, r.returncode, r.stderr[-400:])
        self.assertIn("blocks[0] has unknown key 'workers'", r.stderr)

    # ── ⑥ 逐帧 inputs[] 形态：明确拒绝 + 迁移提示 ──
    def test_06_retired_per_frame_form_is_rejected_with_migration_hint(self):
        cfg = self._write("perframe.json", {
            "phase_name": "normalize",
            "config": {"output_dir": os.path.join(self.tmp, "pf_out"), "precision": "fp64"},
            "inputs": [{"light": self.light1, "bias": self.bias, "dark": self.dark,
                        "flat": self.flat_red, "filter": "Baader R"}],
        })
        r = self._run("--json", cfg, "-y")
        self.assertEqual(3, r.returncode, r.stderr[-400:])
        self.assertIn("retired per-frame phase_config form", r.stderr)
        self.assertIn("migrate to the multi-block form", r.stderr)
        self.assertIn("blocks", r.stderr)
        # -force 越过预检也必须同样明确拒绝（同一判据、同一退出码）
        rf = self._run("--json", cfg, "-force")
        self.assertEqual(3, rf.returncode, rf.stderr[-400:])
        self.assertIn("migrate to the multi-block form", rf.stderr)

    # ── ⑦ 多块各自产出独立 manifest（负例：不得合并） ──
    def test_07_each_block_writes_its_own_manifest(self):
        doc = self._multi_block_doc("man")
        cfg = self._write("mb_man.json", doc)
        r = self._run("--json", cfg, "-y")
        outs = [b["output_dir"] for b in doc["blocks"]]
        self.assertNotEqual(outs[0], outs[1])
        seen = []
        for i, out in enumerate(outs):
            mans = sorted(f for f in os.listdir(out) if f.startswith("astrocs_run_"))
            self.assertEqual(1, len(mans),
                             "blocks[%d] 必须恰一份自己的 run manifest（不得混块）" % i)
            with open(os.path.join(out, mans[0]), encoding="utf-8") as fh:
                man = json.load(fh)
            self.assertEqual(doc["blocks"][i]["name"], man["block"]["name"])
            self.assertEqual(i, man["block"]["index"])
            self.assertEqual(2, man["block"]["count"])
            self.assertEqual([1], man["phases"])
            # 归属正确：manifest 的 config 路径 = 用户给的同一个配置文件
            self.assertTrue(man["config_path"].endswith("mb_man.json"), man["config_path"])
            seen.append(os.path.join(out, mans[0]))
        self.assertEqual(2, len(set(seen)), "两块不得写同一份 manifest")
        # 负例自证（判别力）：块间 output_dir 重复 ⇒ 必被拒（否则 manifest 必被覆盖）
        dup = self._multi_block_doc("dup")
        dup["blocks"][1]["output_dir"] = dup["blocks"][0]["output_dir"]
        rdup = self._run("--json", self._write("mb_dup.json", dup), "-y")
        self.assertEqual(2, rdup.returncode, rdup.stderr[-400:])
        self.assertIn("duplicates an earlier block's output_dir", rdup.stderr)

    # ── 附加：多块与平铺的 output_dir 隔离（不得写到对方目录） ──
    def test_08_blocks_do_not_write_into_each_other(self):
        doc = self._multi_block_doc("iso")
        cfg = self._write("mb_iso.json", doc)
        self._run("--json", cfg, "-y")
        for i, blk in enumerate(doc["blocks"]):
            files = os.listdir(blk["output_dir"])
            self.assertTrue(any(f.startswith("astrocs_run_") for f in files), files)
            # 另一块的 manifest 不得出现在本块目录
            other = doc["blocks"][1 - i]["name"]
            for f in files:
                if f.startswith("astrocs_run_"):
                    with open(os.path.join(blk["output_dir"], f), encoding="utf-8") as fh:
                        self.assertNotEqual(other, json.load(fh)["block"]["name"])


    # ── 附加：多块运行的事件流纪律（一次运行恰一个 final；逐块各起一次会话） ──
    def test_09_event_stream_one_final_per_run(self):
        doc = self._multi_block_doc("ev")
        cfg = self._write("mb_ev.json", doc)
        r = self._run("--json", cfg, "--events-jsonl", "-y")
        evs = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        self.assertTrue(evs, "运行期必须发事件流")
        finals = [e for e in evs if e["kind"] == "final"]
        self.assertEqual(1, len(finals), "多块运行必须恰一个 final（不得逐块各发一个）")
        self.assertEqual(r.returncode, finals[0]["exit_code"])
        starts = [e for e in evs if e["kind"] == "stage_start" and e["stage"] == "phase1_session"]
        self.assertEqual(2, len(starts), "每块各起一次 phase1 会话")
        for i, e in enumerate(evs):
            self.assertEqual(i, e["sequence"], "sequence 必须从 0 单调")
        # 每块的 manifest 各发一条 artifact 事件，路径落在各自 output_dir
        mans = {e["path"] for e in evs
                if e["kind"] == "artifact" and e.get("role") == "run_manifest"}
        self.assertEqual(2, len(mans), "每块必须各发一条 run_manifest artifact 事件")
        outs = [b["output_dir"] for b in doc["blocks"]]
        for p in mans:
            self.assertIn(os.path.dirname(p), outs)


if __name__ == "__main__":
    unittest.main(verbosity=2)
