#!/usr/bin/env python3
"""预检语义矩阵测试（GATE-502 步骤 6）—— correct/warn/error × yes/-y/无确认/-force。

权威：
  * ASTROCS_DESIGN.md §4.5（运行前预检三档 + 确认与越权语义）；
  * docs/api/CLI_PROTOCOL_V1.md §1（error 阻断且 -y 不可越；correct/warn 都需 yes 确认，
    -y/-yes 跳过确认；-force 跳过整个检查步骤）；
  * lib/infrastructure/cli/subcommand.h Subcommand::run（唯一实现：预检页 → 阻断优先级
    → 确认 → 派发；-force 在任何检查之前直接派发）。

矩阵（3 档 × 4 条确认/越权路径，每格都有判据）：
               stdin "yes"        -y/--yes            stdin 空（不确认）      -force
  correct      放行               放行                rc=2 not confirmed      跳过整个预检
  warn         放行               放行                rc=2 not confirmed      跳过整个预检
  error        阻断（不进确认）    阻断（不进确认）     阻断                    跳过整个预检

判据定义（都能红）：
  * 放行 = stderr 无 "not confirmed" 且无 "blocked by"；
  * 阻断 = stderr 含 "blocked by"，且**不含** "not confirmed"（阻断发生在确认之前），
    且该档 output_dir 零产物（无 astrocs_run_*.json）；
  * -force = stderr 含 "skipping precheck and confirmation"，且不含 "blocked by"。

三档如何构造（只用预检自身的判据，不解析 FITS —— 预检只核磁盘可达性）：
  * correct：output_dir 存在 + light/母版三件套都在盘上 ⇒ 全 [correct] 行；
  * warn   ：input_lights 指向**稀疏文件**（apparent size 200 GiB > 可用空间）
             ⇒ 磁盘余量不足 [warn] 行（不阻断、不改退出码；disk_gate.h 唯一判据）；
  * error  ：母版三件套缺一（master_flat 未提供）⇒ calibration_checks 出 [error] 行。

每档用独立 output_dir（判据「零产物」必须可判，不能被其它用例的 run manifest 污染）。
"""
import json
import os
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from cli_test_hygiene import run_cwd  # noqa: E402

SPARSE_GIB = 200          # > 本机可用空间（约 100 GiB），保证触发磁盘余量 warn


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


class TestPreflightMatrix(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert os.path.isfile(EXE), "先构建 CLI（ninja -C build astrocs）"
        cls.tmp = tempfile.mkdtemp(prefix="astrocs_preflight_")
        cls.n = 0

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    # ── fixture：每档独立 output_dir，互不污染 ──
    def _fixture(self, level):
        TestPreflightMatrix.n += 1
        base = os.path.join(self.tmp, "%s_%d" % (level, self.n))
        os.makedirs(base, exist_ok=True)
        small = {}
        for k in ("light", "bias", "dark", "flat"):
            p = os.path.join(base, k + ".fits")
            with open(p, "wb") as fh:
                fh.write(b"\0" * 1024)          # 预检只核可达性，不解析 FITS
            small[k] = p
        out = os.path.join(base, "out")
        os.makedirs(out, exist_ok=True)
        if level == "warn":
            big = os.path.join(base, "big_sparse.fits")
            with open(big, "wb") as fh:
                fh.truncate(SPARSE_GIB * 1024 ** 3)   # 稀疏：apparent 200 GiB / 实占 0
            lights = [big]
        else:
            lights = [small["light"]]
        doc = {"schema_version": "1", "input_lights": lights, "output_dir": out,
               "master_bias": small["bias"], "master_dark": small["dark"],
               "master_flat": small["flat"]}
        if level == "error":
            del doc["master_flat"]               # 缺标定帧 ⇒ [error]（-force 可越）
        cfg = os.path.join(base, "cfg.json")
        with open(cfg, "w", encoding="utf-8") as fh:
            json.dump(doc, fh)
        return cfg, out

    def _run(self, cfg, *args, stdin=None, timeout=180):
        return subprocess.run([EXE, "normalize", "--json", cfg, *args],
                              capture_output=True, text=True, encoding="utf-8",
                              errors="replace", input=stdin, timeout=timeout, cwd=run_cwd())

    @staticmethod
    def _products(out_dir):
        if not os.path.isdir(out_dir):
            return []
        return sorted(f for f in os.listdir(out_dir) if f.startswith("astrocs_run_"))

    # ── 档位前置：三档必须真的是 correct / warn / error（否则矩阵空转） ──
    def test_00_levels_are_what_they_claim(self):
        c_cfg, _ = self._fixture("correct")
        c = self._run(c_cfg, "-y")
        self.assertIn("[correct]", c.stderr, "correct 档必须出现 [correct] 行")
        self.assertNotIn("[error]", c.stderr, "correct 档不得出现 [error]")
        self.assertNotIn("[warn]", c.stderr, "correct 档不得出现 [warn]")
        w_cfg, _ = self._fixture("warn")
        w = self._run(w_cfg, "-y")
        self.assertIn("[warn]", w.stderr, "warn 档必须出现 [warn] 行（磁盘余量不足）")
        self.assertNotIn("[error]", w.stderr, "warn 档不得出现 [error]（warn 不阻塞）")
        e_cfg, _ = self._fixture("error")
        e = self._run(e_cfg, "-y")
        self.assertIn("[error]", e.stderr, "error 档必须出现 [error] 行")
        # 判据非退化：三档的阻断/放行结论确实不同
        self.assertNotIn("blocked by", c.stderr)
        self.assertNotIn("blocked by", w.stderr)
        self.assertIn("blocked by", e.stderr)

    # ── correct 档：yes 或 -y 放行；无确认 rc=2；-force 跳过预检 ──
    def test_01_correct_requires_confirmation(self):
        cfg, out = self._fixture("correct")
        r = self._run(cfg, stdin="")
        self.assertEqual(2, r.returncode, "无确认必须 rc=2")
        self.assertIn("not confirmed", r.stderr)
        self.assertNotIn("blocked by", r.stderr, "correct 档不得被阻断")
        self.assertEqual([], self._products(out), "未确认不得写任何产物")

    def test_02_correct_yes_on_stdin_passes(self):
        cfg, out = self._fixture("correct")
        r = self._run(cfg, stdin="yes\n")
        self.assertNotIn("not confirmed", r.stderr, "stdin yes 必须放行确认")
        self.assertNotIn("blocked by", r.stderr, "correct 档不得被阻断")
        self.assertIn("[correct]", r.stderr, "确认路径也必须显示检查页面（§4.5）")

    def test_03_correct_dash_y_skips_confirmation(self):
        for flag in ("-y", "--yes"):
            cfg, _ = self._fixture("correct")
            r = self._run(cfg, flag)
            self.assertNotIn("not confirmed", r.stderr, "%s 必须跳过确认" % flag)
            self.assertNotIn("blocked by", r.stderr)
            self.assertIn("[correct]", r.stderr, "-y 只跳确认、不跳页面（§4.5）")

    def test_04_correct_force_skips_whole_precheck(self):
        cfg, _ = self._fixture("correct")
        r = self._run(cfg, "-force")
        self.assertIn("skipping precheck and confirmation", r.stderr)
        self.assertNotIn("[correct]", r.stderr, "-force 不显示预检页（跳过整个检查步骤）")
        self.assertNotIn("not confirmed", r.stderr)

    # ── warn 档：同样需要确认；-y 放行；-force 跳过 ──
    def test_05_warn_requires_confirmation(self):
        cfg, out = self._fixture("warn")
        r = self._run(cfg, stdin="")
        self.assertEqual(2, r.returncode, "warn 档无确认必须 rc=2")
        self.assertIn("not confirmed", r.stderr)
        self.assertNotIn("blocked by", r.stderr, "warn 不阻塞（§4.5）")
        self.assertEqual([], self._products(out), "未确认不得写任何产物")

    def test_06_warn_yes_on_stdin_passes(self):
        cfg, _ = self._fixture("warn")
        r = self._run(cfg, stdin="yes\n")
        self.assertNotIn("not confirmed", r.stderr, "warn 档 stdin yes 必须放行")
        self.assertNotIn("blocked by", r.stderr, "warn 不阻塞")
        self.assertIn("[warn]", r.stderr, "warn 行必须出现在检查页面")

    def test_07_warn_dash_y_passes(self):
        cfg, _ = self._fixture("warn")
        r = self._run(cfg, "-y")
        self.assertNotIn("not confirmed", r.stderr)
        self.assertNotIn("blocked by", r.stderr)
        self.assertIn("[warn]", r.stderr)

    def test_08_warn_force_skips_whole_precheck(self):
        cfg, _ = self._fixture("warn")
        r = self._run(cfg, "-force")
        self.assertIn("skipping precheck and confirmation", r.stderr)
        self.assertNotIn("[warn]", r.stderr, "-force 不显示预检页")
        self.assertNotIn("not confirmed", r.stderr)

    # ── error 档：阻断且 -y 不可越；阻断发生在确认之前；-force 是唯一越权路径 ──
    def test_09_error_blocks_all_confirmation_paths(self):
        for label, args, stdin in (("-y", ("-y",), None), ("--yes", ("--yes",), None),
                                   ("yes-stdin", (), "yes\n"), ("no-stdin", (), "")):
            cfg, out = self._fixture("error")
            r = self._run(cfg, *args, stdin=stdin)
            self.assertNotEqual(0, r.returncode, "error 档不得放行（%s）" % label)
            self.assertIn("blocked by", r.stderr, "error 档必须阻断（%s）" % label)
            self.assertNotIn("not confirmed", r.stderr,
                             "阻断必须发生在确认之前（%s）：-y/yes 都不得进入确认面" % label)
            self.assertIn("[error]", r.stderr, "阻断必须完整报出原因（%s）" % label)
            self.assertEqual([], self._products(out),
                             "阻断运行不得写任何产物（%s）" % label)

    def test_10_error_force_is_the_only_bypass(self):
        cfg, _ = self._fixture("error")
        r = self._run(cfg, "-force")
        self.assertIn("skipping precheck and confirmation", r.stderr,
                      "-force 必须显式声明跳过预检与确认")
        self.assertNotIn("blocked by", r.stderr, "-force 下预检不再阻断")
        self.assertNotIn("not confirmed", r.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
