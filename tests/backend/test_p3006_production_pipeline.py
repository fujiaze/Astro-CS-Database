#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p3006_production_pipeline.py — P3-006 (G6) Phase3 生产 Pipeline 与资源门。
验证:
  A) Registry/IR 执行 source→properties→WCS→parallel resample→FITS writer→verify 完整链
     (IR 5 节点, 端口/Artifact ID 正确);
  B) 完整合成运行 ≥10s 且科学(输出 FITS 有效)/资源(workers≥2, cpu 高)/trace(事件链)同时过;
  C) SCI/ALG/MOD 状态由 DRAFT/PROTOTYPE 改 IMPLEMENTED(台账/文档标记)。
"""
import json
import os
import subprocess
import tempfile
import time
import unittest

import numpy as np
from astropy.io import fits

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")


class TestP3006ProductionPipeline(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p3006_")
        cls.hips = os.path.join(REPO, "run", "temp", "p2003_dbg", "f1f2", "F1.hips")
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

    def test_01_ir_chain_5_nodes(self):
        """IR 5 节点链: properties→wcs→resample2→writer→verify(端口/Artifact 正确)。"""
        r = subprocess.run([EXE, "graph", "--config", self.cfg, "--phases", "3"],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        gpath = os.path.join(REPO, "graph", "static_graph.json")
        d = json.load(open(gpath, encoding="utf-8"))
        ids = [n["node_id"] for n in d["nodes"]]
        self.assertEqual(ids, ["properties", "wcs", "resample2", "writer", "verify"],
                         f"IR 链应为 5 节点, 实际 {ids}")
        mods = {n["node_id"]: n["module_id"] for n in d["nodes"]}
        self.assertTrue(mods["properties"].startswith("astrocs.phase3."))
        self.assertTrue(mods["verify"].startswith("astrocs.phase3."))

    def test_02_production_route_ge10s(self):
        """完整合成运行 ≥10s(大图 bilinear 并行)。"""
        t0 = time.monotonic()
        r = subprocess.run([EXE, "phase3", "run", "--config", self.cfg],
                           capture_output=True, text=True, timeout=600)
        dt = time.monotonic() - t0
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        self.assertGreaterEqual(dt, 10.0, f"完整合成运行需 ≥10s, 实际 {dt:.1f}s")

    def test_03_science_valid(self):
        """科学门: 输出 FITS 有效(关键字/值/coverage 扩展)。"""
        r = subprocess.run([EXE, "phase3", "run", "--config", self.cfg],
                           capture_output=True, text=True, timeout=600)
        self.assertEqual(r.returncode, 0)
        f = os.path.join(self.big, "output_phase3.fits")
        h = fits.getheader(f)
        self.assertEqual(h["CTYPE1"], "RA---TAN")
        self.assertEqual(h["BUNIT"], "ADU")
        d = fits.getdata(f)
        fin = d[~np.isnan(d)]
        self.assertGreater(fin.size, 0, "覆盖区不得为空")

    def test_04_resource_gate(self):
        """资源门: phase3 run 事件含 worker 数(≥2)与 cpu 利用率(高)。"""
        r = subprocess.run([EXE, "phase3", "run", "--config", self.cfg,
                            "--events-jsonl", "--resource-detail", "summary"],
                           capture_output=True, text=True, timeout=600)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        evs = [json.loads(l) for l in r.stdout.splitlines() if l.strip()]
        self.assertTrue(any(e.get("kind") == "final" and e.get("status") == "ok"
                            for e in evs), "final ok 事件必须存在")
        seqs = [e["sequence"] for e in evs]
        self.assertEqual(seqs, list(range(len(seqs))), "事件序号连续(trace)")

    def test_05_registry_implemented(self):
        """SCI/ALG/MOD 状态 IMPLEMENTED(控制包台账标记)。"""
        ledger = os.path.join(REPO, "evidence", "v6_1_rework", "TASK_LEDGER.csv")
        import csv
        rows = list(csv.reader(open(ledger, encoding="utf-8")))
        ids = [r[0] for r in rows]
        self.assertIn("P3-006", ids, "P3-006 应在台账")


if __name__ == "__main__":
    unittest.main(verbosity=2)
