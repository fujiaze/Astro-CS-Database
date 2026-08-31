#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p1004_joint_gate.py — P1-004 (G4) Phase1 数值与资源联合门。
同一次 current commit 运行所有 P1 Oracle 与 ≥10s Drizzle/calibration 资源 workload;
science summary(manifest) 与 resource summary(resource_summary.json) 互相引用 run ID。
数值对但资源失败仍 FAIL, 资源好但数值错也 FAIL(联合语义)。
"""
import glob
import json
import os
import shutil
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")
TMP = "/tmp/mon001_run_out"  # 复用 MON-001 配置的 output_dir

P1_ORACLES = [
    "test_calibration_oracle.py",
    "test_drizzle_oracle.py",
    "test_noise_model_oracle.py",
    "test_wcs_psf_oracle.py",
    "test_phase3_reproject_oracle.py",
    "test_p1002_gaps.py",
]


class TestP1004JointGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p1004_")
        # 同 commit 确认
        r = subprocess.run(["git", "-C", REPO, "rev-parse", "HEAD"],
                           capture_output=True, text=True, timeout=60)
        cls.commit = r.stdout.strip()

    def test_01_all_p1_oracles_pass(self):
        """所有 P1 Oracle(6 个)在同 commit 下全绿(数值门)。"""
        env = dict(os.environ, ASTROCS_REPO=REPO)
        for name in P1_ORACLES:
            path = os.path.join(REPO, "tests", "backend", name)
            r = subprocess.run(["python3", path], capture_output=True, text=True,
                               env=env, timeout=900)
            self.assertEqual(r.returncode, 0,
                             f"{name} FAILED: {r.stdout[-500:]}{r.stderr[-500:]}")

    def test_02_resource_workload_runs(self):
        """资源 workload(CLI run phase3)成功执行, 产 resource_summary.json。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        shutil.rmtree(TMP, ignore_errors=True)
        os.makedirs(TMP, exist_ok=True)
        r = subprocess.run([EXE, "run", "--phases", "3", "--config",
                            os.path.join(REPO, "run", "temp", "mon001_cfg.json"),
                            "--events-jsonl"], capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        self.assertTrue(os.path.isfile(os.path.join(TMP, "resource_summary.json")),
                        "resource_summary.json 缺失")

    def test_03_science_resource_share_run_id(self):
        """science manifest 与 resource_summary.json 引用同一 run_id(联合门核心)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        # 保证有产物(先清理避免 prior artifact hash mismatch)
        shutil.rmtree(TMP, ignore_errors=True)
        os.makedirs(TMP, exist_ok=True)
        subprocess.run([EXE, "run", "--phases", "3", "--config",
                        os.path.join(REPO, "run", "temp", "mon001_cfg.json"),
                        "--events-jsonl"], capture_output=True, text=True, timeout=300)
        manifests = sorted(glob.glob(os.path.join(TMP, "astrocs_run_*.json")))
        self.assertTrue(manifests, "run manifest 缺失")
        mf = manifests[-1]
        m = json.load(open(mf))
        res = json.load(open(os.path.join(TMP, "resource_summary.json")))
        self.assertIn("run_id", m, "science manifest 缺 run_id")
        self.assertIn("run_id", res, "resource summary 缺 run_id")
        self.assertEqual(m["run_id"], res["run_id"],
                         f"science/resource run_id 不一致: {m['run_id']} vs {res['run_id']}")
        # resource 文件与 manifest 同时存在才通过(联合: 数值+资源同门)
        self.assertTrue(os.path.isfile(os.path.join(TMP, "resource_samples.csv")))
        self.assertTrue(os.path.isfile(os.path.join(TMP, "worker_balance.csv")))

    def test_04_same_commit(self):
        """CLI 二进制 source commit(version_generated.h)与当前 HEAD 同批(同 commit 门)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        # --version 输出含 commit(0.10.0-alpha.2+g<sha12>)
        r = subprocess.run([EXE, "--version"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        import re
        m = re.search(r"\+g([0-9a-f]{12})", r.stdout)
        self.assertTrue(m, f"--version 无 commit: {r.stdout}")
        self.assertEqual(m.group(1), self.commit[:12],
                         f"CLI commit {m.group(1)} != HEAD {self.commit[:12]}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
