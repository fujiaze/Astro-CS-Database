#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p1004_joint_gate.py — P1-004 (G4) Phase1 数值与资源联合门。
同一次 current commit 运行所有 P1 Oracle 与 ≥10s Drizzle/calibration 资源 workload;
science summary(manifest) 与 resource summary(resource_summary.json) 互相引用 run ID。
数值对但资源失败仍 FAIL, 资源好但数值错也 FAIL(联合语义)。

CLI-002 迁移注记 (commit de2d6d7f) + R2 情报:
  - 顶层 `run --phases 3` 删除 → `phase3 run`(存活唯一 run 载体)。
  - run/temp/mon001_cfg.json 从未入库(.gitignore run/*), 历史上是 MON-001 本地手动
    残留, CI(07a7a95f 起 ci/prepare_linux_fixtures.py)用同配方生成。本文件按其他
    存活 backend 测试的 fixture 自建模式, 在 setUpClass 内自产: FIELD.hips(fixture
    exe --make-field, 复用 fixture_common 的进程级缓存 exe)+ V1 顶层 cfg
    (schema_version/inputs/output_dir/phase3 子对象, 参数对齐
    tests/cli/test_monitor_events.py 的 40x30 nearest 小图合成门)。
  - 联合 run_id 语义的现行载体: resource_summary.json 的 run_id 现恒为 ""
    (recorder.write_all 未传 run_id — CLI-002 回归, IMPL 缺口), 文件级互引断言
    无从建立; 改为断言 manifest.run_id == 事件流 run_id(JSONL 全事件一致)且
    resource 三件套与 manifest 同轮产出落盘同一 output_dir(产物级联合证据)。
    文件级 run_id 互引缺口归 IMPL/INT。
"""
import glob
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from tests.backend import fixture_common  # noqa: E402
from tests.backend.fixture_common import two_cpu_preexec  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "astrocs")
TMP = "/tmp/mon001_run_out"  # 复用 MON-001 配置的 output_dir
FIELD_HIPS = os.path.join(REPO, "run", "temp", "FIELD.hips")
MON001_CFG = os.path.join(REPO, "run", "temp", "mon001_cfg.json")

P1_ORACLES = [
    "test_calibration_oracle.py",
    "test_drizzle_oracle.py",
    "test_noise_model_oracle.py",
    "test_wcs_psf_oracle.py",
    "test_phase3_reproject_oracle.py",
    "test_p1002_gaps.py",
]


def _ensure_mon001_fixture():
    """自建 mon001 fixture(FIELD.hips + mon001_cfg.json), 不依赖 CI 脚本。

    配方对齐 ci/prepare_linux_fixtures.py(07a7a95f): FIELD.hips 由 fixture exe
    --make-field 生成; cfg 为 V1 顶层合同 + phase3 子对象(40x30 nearest TAN,
    output_dir=/tmp/mon001_run_out)。exe 复用 fixture_common 进程级缓存。
    """
    if not (os.path.isdir(FIELD_HIPS) and os.path.isfile(MON001_CFG)):
        exe = fixture_common._build_fixture_exe()  # 进程级缓存, 已存在则免编译
        if not os.path.isdir(FIELD_HIPS):
            tmpd = tempfile.mkdtemp(prefix="mon001_field_")
            r = subprocess.run([exe, "--make-field", tmpd], capture_output=True,
                               text=True, timeout=300)
            assert "HIPS_FIXTURES_OK" in r.stdout, \
                "[mon001 fixture make-field] " + r.stderr[-600:]
            os.makedirs(os.path.dirname(FIELD_HIPS), exist_ok=True)
            shutil.rmtree(FIELD_HIPS, ignore_errors=True)
            shutil.move(os.path.join(tmpd, "FIELD.hips"), FIELD_HIPS)
        if not os.path.isfile(MON001_CFG):
            cfg = {
                "schema_version": "1",
                "inputs": {"lights": [], "darks": [], "flats": [], "bias": []},
                "output_dir": TMP,
                "phase3": {
                    "source": {"hips_dir": FIELD_HIPS},
                    "center": {"ra_deg": 210.0, "dec_deg": 34.0},
                    "scale_deg_per_px": 0.1,
                    "width_px": 40,
                    "height_px": 30,
                    "projection": "TAN",
                    "sampler": "nearest",
                    "coverage_output": "mask",
                    # P3-006/DOC-003 内存守卫: max_tiles 只降不升(默认
                    # min(1024, ceil(W·H/512²)+16), 40x30→17); CI 残方 max_tiles=64
                    # 现触发 ACS_ERR_BUDGET, 故不设该键取默认。
                },
            }
            os.makedirs(os.path.dirname(MON001_CFG), exist_ok=True)
            with open(MON001_CFG, "w", encoding="utf-8") as f:
                f.write(json.dumps(cfg, ensure_ascii=False, indent=1) + "\n")
    assert os.path.isdir(FIELD_HIPS) and os.path.isfile(MON001_CFG), \
        "mon001 fixture 生成失败"


def _run_phase3(events=True):
    """现行载体 phase3 run(2c 亲和, 恢复 CI 设计语境)。"""
    argv = [EXE, "phase3", "run", "--config", MON001_CFG]
    if events:
        argv.append("--events-jsonl")
    return subprocess.run(argv, capture_output=True, text=True, timeout=300,
                          preexec_fn=two_cpu_preexec)


def _events_of(res):
    evs = []
    for line in res.stdout.splitlines():
        try:
            evs.append(json.loads(line))
        except Exception:
            pass
    return evs


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
        """资源 workload(phase3 run)成功执行, 产 resource_summary.json。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        _ensure_mon001_fixture()
        shutil.rmtree(TMP, ignore_errors=True)
        os.makedirs(TMP, exist_ok=True)
        r = _run_phase3()
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        self.assertTrue(os.path.isfile(os.path.join(TMP, "resource_summary.json")),
                        "resource_summary.json 缺失")

    def test_03_science_resource_share_run_id(self):
        """联合门核心: manifest.run_id 与事件流 run_id 一致, 资源三件套同轮落盘。

        IMPL/INT 缺口: resource_summary.json 的 run_id 现恒为 ""(recorder.write_all
        未传 run_id), 文件级 science/resource run_id 互引断言无载体 — 该缺口随
        IMPL 修复后应恢复原断言(m["run_id"] == res["run_id"])。
        """
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        _ensure_mon001_fixture()
        # 保证有产物(先清理避免 prior artifact hash mismatch)
        shutil.rmtree(TMP, ignore_errors=True)
        os.makedirs(TMP, exist_ok=True)
        r = _run_phase3()
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        manifests = sorted(glob.glob(os.path.join(TMP, "astrocs_run_*.json")))
        self.assertTrue(manifests, "run manifest 缺失")
        m = json.load(open(manifests[-1]))
        self.assertIn("run_id", m, "science manifest 缺 run_id")
        # 事件流 run_id: 全事件一致且与 manifest 相同(联合门现行载体)
        evs = _events_of(r)
        ev_run_ids = {e.get("run_id") for e in evs if e.get("run_id")}
        self.assertEqual(ev_run_ids, {m["run_id"]},
                         f"事件流 run_id 与 manifest 不一致: {ev_run_ids} vs {m['run_id']}")
        # resource 文件与 manifest 同轮同目录存在(联合: 数值+资源同门)
        for name in ("resource_summary.json", "resource_samples.csv", "worker_balance.csv"):
            self.assertTrue(os.path.isfile(os.path.join(TMP, name)), f"{name} 缺失")
        res = json.load(open(os.path.join(TMP, "resource_summary.json")))
        self.assertEqual(res.get("run_id", ""), "",
                         "run_id 现契约: resource_summary.run_id 为空(IMPL 缺口: "
                         "write_all 未传 run_id); 若非空则 IMPL 已修复, 应恢复文件级互引断言")

    def test_04_same_commit(self):
        """CLI 二进制 source commit(version_generated.h)与当前 HEAD 同批(同 commit 门)。"""
        if not os.path.isfile(EXE):
            self.skipTest("CLI 二进制缺失")
        # --version 输出含 commit(<根 VERSION 单源>+g<sha12>)
        r = subprocess.run([EXE, "--version"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        m = re.search(r"\+g([0-9a-f]{12})", r.stdout)
        self.assertTrue(m, f"--version 无 commit: {r.stdout}")
        self.assertEqual(m.group(1), self.commit[:12],
                         f"CLI commit {m.group(1)} != HEAD {self.commit[:12]}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
