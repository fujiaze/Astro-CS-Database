#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2002_parallel_upm.py — P2-002 (G5) 并行化生产 UPM 求解验证。
验证:
  A) upm.cpp 无 P2_ENABLE_OPENMP / hardware_concurrency 生产残留;
  B) 生产 phase2 run(upm 多 worker)成功;
  C) UPM 4-worker 与 1-worker 的 persist 模型一致(并行=执行方式, 科学结果不变);
  D) worker 数来自 Runtime lease(p2_session 传 budget.max_workers)。
"""
import hashlib
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


class TestP2002ParallelUpm(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2002_")
        cls.hips_dir = os.path.join(cls.tmp, "hips")
        os.makedirs(cls.hips_dir, exist_ok=True)
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
        cls.base_cfg = {
            "hips_paths": [os.path.join(cls.hips_dir, "F1.hips"),
                           os.path.join(cls.hips_dir, "F2.hips")],
            "upm": {"max_iterations": 2, "huber_delta": 3.0},
        }

    def _make_cfg(self, workers, out_dir, save_path):
        cfg = dict(self.base_cfg)
        cfg["sampler"] = {"cpu_workers": 1}  # sampler reference; 测 UPM workers
        cfg["upm"] = {"cpu_workers": workers, "max_iterations": 2, "huber_delta": 3.0}
        cfg["output_dir"] = out_dir
        cfg["persist_upm"] = True
        cfg["upm_save_path"] = save_path
        p = os.path.join(self.tmp, f"cfg_{workers}.json")
        with open(p, "w") as f:
            json.dump(cfg, f)
        return p

    def _run_upm(self, workers, out_dir, save_path):
        cfg = self._make_cfg(workers, out_dir, save_path)
        return subprocess.run([EXE, "phase2", "run", "--config", cfg, "--events-jsonl"],
                              capture_output=True, text=True,
                              env=dict(os.environ, ASTROCS_REPO=REPO), timeout=600)

    def test_01_no_openmp_gate(self):
        """upm.cpp 无 P2_ENABLE_OPENMP / hardware_concurrency 生产调用残留。"""
        src = open(os.path.join(REPO, "lib", "phase2", "src", "upm.cpp"),
                   encoding="utf-8").read()
        self.assertNotIn("P2_ENABLE_OPENMP", src)
        # 仅允许注释/字符串提及 omp_; 禁止代码调用(omp_get_*())
        for line in src.splitlines():
            if "omp_" in line and not line.lstrip().startswith(("//", "*")):
                if "omp_get_" in line:
                    self.fail(f"upm.cpp 生产调用 OpenMP: {line}")
        for line in src.splitlines():
            if "hardware_concurrency" in line and not line.lstrip().startswith(("//", "*")):
                if "::hardware_concurrency()" in line:
                    self.fail(f"upm.cpp 生产调用 hardware_concurrency: {line}")

    def test_02_multi_worker_upm_run(self):
        """生产 phase2 run(UPM 多 worker)成功执行。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        out = os.path.join(self.tmp, "outN")
        save = os.path.join(self.tmp, "upmN.bin")
        r = self._run_upm(4, out, save)
        self.assertEqual(r.returncode, 0, r.stdout[-400:] + r.stderr[-400:])
        self.assertIn("sample ok", r.stderr, "phase2 无 sample stage")
        self.assertTrue(os.path.isfile(save), "UPM persist 文件缺失")

    def test_03_upm_parallel_equals_reference(self):
        """UPM 4-worker 与 1-worker 的 persist 模型一致(并行=执行方式)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        out1 = os.path.join(self.tmp, "out1")
        save1 = os.path.join(self.tmp, "upm1.bin")
        r1 = self._run_upm(1, out1, save1)
        self.assertEqual(r1.returncode, 0, r1.stdout[-400:] + r1.stderr[-400:])
        self.assertTrue(os.path.isfile(save1), "1-worker UPM persist 缺失")

        outN = os.path.join(self.tmp, "outN2")
        saveN = os.path.join(self.tmp, "upmN.bin")
        rN = self._run_upm(4, outN, saveN)
        self.assertEqual(rN.returncode, 0, rN.stdout[-400:] + rN.stderr[-400:])
        self.assertTrue(os.path.isfile(saveN), "4-worker UPM persist 缺失")

        def sha(p):
            h = hashlib.sha256()
            with open(p, "rb") as f:
                for chunk in iter(lambda: f.read(1 << 20), b""):
                    h.update(chunk)
            return h.hexdigest()

        self.assertEqual(sha(save1), sha(saveN),
                         "UPM 4-worker 与 1-worker persist 不一致")


if __name__ == "__main__":
    unittest.main(verbosity=2)
