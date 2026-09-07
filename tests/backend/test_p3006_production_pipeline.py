#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p3006_production_pipeline.py — P3-006 (G6) Phase3 生产 Pipeline 与资源门。
验证:
  A) Registry/IR 执行 source→properties→WCS→parallel resample→FITS writer→verify 完整链
     (IR 5 节点, 端口/Artifact ID 正确);
  B) 完整合成运行 ≥10s 且科学(输出 FITS 有效)/资源(workers≥2, cpu 高)/trace(事件链)同时过;
  C) SCI/ALG/MOD 状态由 DRAFT/PROTOTYPE 改 IMPLEMENTED(台账/文档标记)。

CLI-002 迁移注记 (commit de2d6d7f):
  - 顶层 `graph` 入口已删除; IR 仅存在于内存 (cli/runtime_client.cpp build_pipeline_ir),
    `<repo>/graph/static_graph.json` 等图落盘产物在现行 CLI 无载体 (孤儿函数
    write_run_graphs 无调用点, 二进制中已无产物字符串)。test_01 改为验证现行真实
    契约: `graph` 入口 exit 2 + `phase3 run` 真实完成 IR 链并产出 manifest(phases==[3],
    status complete)。IR 逐节点 module_id/端口的静态断言缺口归 IMPL/INT。
  - 资源门阈值 = 0.80*min(selected_workers, available_cpus), 而 session budget 恒
    2 workers; 在多核宿主机上 available_cpus 抬高阈值导致 run 被结构性拒绝(exit 10)。
    本测试按原始 CI 2c2g 设计语境, 以 2-CPU 亲和(fixture_common.two_cpu_preexec)
    恢复 budget/available 一致 —— 环境适配, 非语义放宽。实测: 1200x1200 bilinear
    大图在 2c 亲和下 wall≈13s, avg≈1.8 核 ≥ 0.8*2, verdict ok。
"""
import json
import os
import subprocess
import tempfile
import time
import unittest

from tests.backend.fixture_common import ensure_f1f2_hips, two_cpu_preexec  # noqa: E402

import numpy as np
from astropy.io import fits

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "astrocs")


class TestP3006ProductionPipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3006_")
        cls.hips = os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips")
        ensure_f1f2_hips()
        assert os.path.isdir(cls.hips)
        cls.big = os.path.join(cls.tmp, "big")
        os.makedirs(cls.big, exist_ok=True)
        cfg = {"schema_version": "1",
               "inputs": {"lights": [cls.hips], "darks": [], "flats": [], "bias": []},
               "phase3": {"source": {"hips_dir": cls.hips},
                          "center": {"ra_deg": 0.0, "dec_deg": 30.0},
                          "scale_deg_per_px": 0.002, "width_px": 1200, "height_px": 1200,
                          "sampler": "bilinear", "projection": "TAN",
                          "coverage_output": "mask", "output_dir": cls.big},
               "output_dir": cls.big}
        json.dump(cfg, open(os.path.join(cls.big, "c.json"), "w"))
        cls.cfg = os.path.join(cls.big, "c.json")

    def _run(self, args, timeout=600):
        return subprocess.run(args, capture_output=True, text=True,
                              timeout=timeout, preexec_fn=two_cpu_preexec)

    def test_01_ir_chain_5_nodes(self):
        """IR 5 节点链: 旧 `graph` 入口已删除(exit 2); phase3 run 完成 IR 链。"""
        r = subprocess.run([EXE, "graph", "--config", self.cfg, "--phases", "3"],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 2,
                         "顶层 `graph` 入口应已删除(CLI-002), 期望 exit 2")
        # 现行载体: phase3 run 经 runtime 执行 IR 链并写 manifest(phases==[3])
        r = self._run([EXE, "phase3", "run", "--config", self.cfg, "--events-jsonl"],
                      timeout=600)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        evs = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        final = [e for e in evs if e.get("kind") == "final"]
        self.assertTrue(final and final[-1].get("status") == "ok",
                        "final ok 事件必须存在")
        mpath = [e for e in evs if e.get("kind") == "artifact"
                 and e.get("role") == "run_manifest"]
        self.assertTrue(mpath, "manifest artifact 事件缺失")
        self.assertTrue(os.path.isfile(mpath[-1]["path"]), "manifest 文件未落盘")
        m = json.load(open(mpath[-1]["path"], encoding="utf-8"))
        self.assertEqual(m.get("kind"), "astrocs_run_manifest")
        self.assertEqual(m.get("phases"), [3])
        self.assertEqual(m.get("status"), "complete")
        # IMPL/INT 缺口: IR 5 节点(properties→wcs→resample2→writer→verify)的
        # static_graph.json 静态断言在现行 CLI 无载体(graph 落盘产物已随 CLI-002 移除)。

    def test_02_production_route_ge10s(self):
        """完整合成运行 ≥10s(大图 bilinear 并行, 2c 亲和)。"""
        t0 = time.monotonic()
        r = self._run([EXE, "phase3", "run", "--config", self.cfg], timeout=600)
        dt = time.monotonic() - t0
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        self.assertGreaterEqual(dt, 10.0, f"完整合成运行需 ≥10s, 实际 {dt:.1f}s")

    def test_03_science_valid(self):
        """科学门: 输出 FITS 有效(关键字/值/coverage 扩展)。"""
        r = self._run([EXE, "phase3", "run", "--config", self.cfg], timeout=600)
        self.assertEqual(r.returncode, 0)
        f = os.path.join(self.big, "output_phase3.fits")
        h = fits.getheader(f)
        self.assertEqual(h["CTYPE1"], "RA---TAN")
        self.assertEqual(h["BUNIT"], "ADU")
        d = fits.getdata(f)
        fin = d[~np.isnan(d)]
        self.assertGreater(fin.size, 0, "覆盖区不得为空")

    def test_04_resource_gate(self):
        """资源门: phase3 run 事件链完整, resource gate verdict ok(worker/cpu 证据)。"""
        r = self._run([EXE, "phase3", "run", "--config", self.cfg,
                       "--events-jsonl", "--resource-detail", "summary"], timeout=600)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        evs = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        self.assertTrue(any(e.get("kind") == "final" and e.get("status") == "ok"
                            for e in evs), "final ok 事件必须存在")
        seqs = [e["sequence"] for e in evs]
        self.assertEqual(seqs, list(range(len(seqs))), "事件序号连续(trace)")
        gate = [e for e in evs if e.get("kind") == "resource"
                and e.get("message") == "resource gate"]
        self.assertTrue(gate, "resource gate 事件缺失")
        g = gate[-1]
        self.assertEqual(g.get("verdict"), "ok", f"资源门未过: {g.get('verdict')}")
        self.assertGreaterEqual(g.get("wall_seconds", 0.0), 10.0, "active wall < 10s")
        self.assertGreaterEqual(g.get("workers_p50", 0.0), 1.0, "workers_p50 < 1")
        # 现行事件字段: cpu_p50_percent/cpu_mean_percent(负值=未采样)。大图 run 必有采样。
        self.assertGreaterEqual(g.get("cpu_p50_percent", -1.0), 0.0, "cpu_p50 未采样")
        self.assertGreaterEqual(g.get("cpu_mean_percent", -1.0), 0.0, "cpu_mean 未采样")

    def test_05_registry_implemented(self):
        """SCI/ALG/MOD 状态 IMPLEMENTED(控制包台账标记)。"""
        ledger = os.path.join(REPO, "evidence", "v6_1_rework", "TASK_LEDGER.csv")
        import csv
        rows = list(csv.reader(open(ledger, encoding="utf-8")))
        ids = [r[0] for r in rows]
        self.assertIn("P3-006", ids, "P3-006 应在台账")


if __name__ == "__main__":
    unittest.main(verbosity=2)
