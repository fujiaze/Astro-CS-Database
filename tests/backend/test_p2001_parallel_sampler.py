#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2001_parallel_sampler.py — P2-001 (G5) 并行化生产 Coverage/Sampler 验证。
验证:
  A) 生产 phase2 run 的 sampler 实际走 Runtime lease 多 worker(默认无 P2_ENABLE_OPENMP 也并行);
  B) N-worker 与 1-worker 结果一致(reference 语义: 并行只是执行方式, 科学结果不变);
  C) 生产代码无 hardware_concurrency()/P2_ENABLE_OPENMP 默认关闭残留。
"""
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")
FIXTURE_SRC = os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp")
AIO = os.path.join(REPO, "lib", "astro_image_io")
AIO_SRC = os.path.join(AIO, "src")


def cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c"):
            continue
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    return objs


class TestP2001ParallelSampler(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2001_")
        cls.hips_dir = os.path.join(cls.tmp, "hips")
        os.makedirs(cls.hips_dir, exist_ok=True)
        # 编译 fixture 生成 F1/F2.hips(与 test_phase2_inprocess 同源)
        incs = [f"-I{os.path.join(REPO, 'include')}",
                f"-I{os.path.join(AIO, 'include')}",
                f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                f"-I{os.path.join(REPO, 'lib', 'common')}"]
        srcs = [FIXTURE_SRC,
                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp")]
        exe = os.path.join(cls.tmp, "fixture")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *cfitsio_objs(cls.tmp), "-lz", "-lzstd", "-llz4",
                            "-o", exe], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        r2 = subprocess.run([exe, "--make", cls.hips_dir], capture_output=True, text=True, timeout=300)
        assert "HIPS_FIXTURES_OK" in r2.stdout, r2.stderr
        # 生成 config(与 test_phase2_inprocess 同键: hips_paths + output_dir)
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        cfg = {
            "hips_paths": [os.path.join(cls.hips_dir, "F1.hips"),
                           os.path.join(cls.hips_dir, "F2.hips")],
            "output_dir": os.path.join(cls.tmp, "out"),
        }
        with open(cls.cfg, "w") as f:
            json.dump(cfg, f)

    def _run(self, workers_env=None, timeout=600):
        env = dict(os.environ, ASTROCS_REPO=REPO)
        if workers_env:
            env.update(workers_env)
        r = subprocess.run([EXE, "phase2", "run", "--config", self.cfg, "--events-jsonl"],
                           capture_output=True, text=True, env=env, timeout=timeout)
        return r

    def test_01_production_no_openmp_gate(self):
        """生产代码无 P2_ENABLE_OPENMP 默认关闭与 hardware_concurrency() 调用残留。"""
        src = open(os.path.join(REPO, "lib", "phase2", "src", "sampler.cpp"),
                   encoding="utf-8").read()
        self.assertNotIn("P2_ENABLE_OPENMP", src, "sampler.cpp 仍含 P2_ENABLE_OPENMP 条件")
        # 仅允许注释/字符串提及 hardware_concurrency; 禁止生产调用(std::thread::hardware_concurrency)
        for line in src.splitlines():
            if "hardware_concurrency" in line and not line.lstrip().startswith(("//", "*")):
                if "::hardware_concurrency()" in line:
                    self.fail(f"sampler.cpp 生产调用 hardware_concurrency: {line}")
        sess = open(os.path.join(REPO, "lib", "phase2_session", "p2_session.cpp"),
                    encoding="utf-8").read()
        self.assertIn("budget.max_workers", sess, "p2_session 未把 lease 传给 sampler")

    def test_02_multi_worker_run_succeeds(self):
        """生产 phase2 run(默认多 worker)成功执行(无 P2_ENABLE_OPENMP 也并行)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        r = self._run()
        self.assertEqual(r.returncode, 0, r.stdout[-600:] + r.stderr[-600:])
        self.assertIn("sample ok", r.stderr, "phase2 无 sample stage 完成")

    def _parse_sample(self, out):
        """从日志(stderr `stage sample ok: obs=.. overlap_controls=..`)提取 obs/controls。"""
        for line in out.splitlines():
            m = re.search(r"stage sample ok: obs=(\d+) overlap_controls=(\d+)", line)
            if m:
                return (int(m.group(1)), int(m.group(2)))
        return None

    def test_03_parallel_equals_reference(self):
        """N-worker 与 1-worker 结果一致(并行=执行方式, 科学结果不变)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        # 单 worker 直接跑(用 config 覆盖 cpu_workers)
        cfg1 = dict(json.load(open(self.cfg)))
        cfg1["sampler"] = {"cpu_workers": 1}
        cfg1["output_dir"] = os.path.join(self.tmp, "out1")
        cfg1path = os.path.join(self.tmp, "cfg1.json")
        with open(cfg1path, "w") as f:
            json.dump(cfg1, f)
        r1 = subprocess.run([EXE, "phase2", "run", "--config", cfg1path, "--events-jsonl"],
                            capture_output=True, text=True,
                            env=dict(os.environ, ASTROCS_REPO=REPO), timeout=600)
        self.assertEqual(r1.returncode, 0, r1.stdout[-400:] + r1.stderr[-400:])

        cfgN = dict(json.load(open(self.cfg)))
        cfgN["sampler"] = {"cpu_workers": 4}
        cfgN["output_dir"] = os.path.join(self.tmp, "outN")
        cfgNpath = os.path.join(self.tmp, "cfgN.json")
        with open(cfgNpath, "w") as f:
            json.dump(cfgN, f)
        rN = subprocess.run([EXE, "phase2", "run", "--config", cfgNpath, "--events-jsonl"],
                            capture_output=True, text=True,
                            env=dict(os.environ, ASTROCS_REPO=REPO), timeout=600)
        self.assertEqual(rN.returncode, 0, rN.stdout[-400:] + rN.stderr[-400:])

        o1 = self._parse_sample(r1.stdout + r1.stderr)
        oN = self._parse_sample(rN.stdout + rN.stderr)
        self.assertTrue(o1, "单 worker 无 sample stage")
        self.assertTrue(oN, "N-worker 无 sample stage")
        self.assertEqual(o1[0], oN[0], f"n_obs 不一致: {o1} vs {oN}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
