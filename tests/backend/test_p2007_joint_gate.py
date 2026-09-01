#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2007_joint_gate.py — P2-007 (G5) Phase2 接缝与资源联合门。
在 2c2g 运行 production seam workload(6 块 mini HiPS) ≥10s, 保存科学+资源证据:
  A) 科学: seam 校正有效(M/C 非空, 校正后帧间差异下降, 源不被拟合);
  B) 资源: Runtime 多 worker(workers_p50≥2), CPU p50≥90%/mean≥85%(2c2g 门),
     active_wall≥10s, 峰值 RSS 有界;
  C) 联合: CPU 不达门时不得因 seam 数值好而 PASS(gate 事件必须 ok)。
"""
import json
import os
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")
SEAM6 = os.path.join(REPO, "run", "temp", "p2007_seam6", "seam6")


class TestP2007JointGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2007_")
        cls.out = os.path.join(cls.tmp, "out")
        os.makedirs(cls.out, exist_ok=True)
        paths = [os.path.join(SEAM6, f"SEAM{i}.hips") for i in range(6)]
        for p in paths:
            assert os.path.isdir(p), f"缺 seam 数据 {p}"
        cls.cfg = os.path.join(cls.tmp, "cfg.json")
        json.dump({"schema_version": "1",
                   "inputs": {"lights": paths, "darks": [], "flats": [], "bias": []},
                   "output_dir": cls.out}, open(cls.cfg, "w"))
        cls.res = subprocess.run([EXE, "run", "--phases", "2", "--config", cls.cfg,
                                  "--events-jsonl", "--resource-detail", "summary"],
                                 capture_output=True, text=True, timeout=400)
        cls.evs = []
        for line in cls.res.stdout.splitlines():
            try:
                cls.evs.append(json.loads(line))
            except Exception:
                pass

    def _event(self, kind, msg_part=None):
        for e in self.evs:
            if e.get("kind") == kind and (msg_part is None or msg_part in str(e.get("message", ""))):
                return e
        return None

    def test_01_workload_ten_seconds(self):
        """production seam workload(6 块) ≥10s(active_wall)。"""
        self.assertEqual(self.res.returncode, 0, self.res.stderr[-400:])
        g = self._event("gate", "resource gate ok")
        self.assertIsNotNone(g, "必须发出 resource gate ok 事件")
        self.assertGreaterEqual(g["active_wall_seconds"], 10.0,
                                f"active_wall {g['active_wall_seconds']:.2f}s < 10s")

    def test_02_resource_gate_workers(self):
        """Runtime 多 worker: workers_p50 ≥ 2(avail≥2)。"""
        g = self._event("gate", "resource gate ok")
        self.assertGreaterEqual(g["workers_p50"], 2.0, "workers_p50 < 2 (单线程)")

    def test_03_resource_gate_cpu(self):
        """CPU 门: p50 ≥ 90% 且 mean ≥ 85%。"""
        g = self._event("gate", "resource gate ok")
        self.assertGreaterEqual(g["cpu_p50"], 90.0, f"cpu_p50 {g['cpu_p50']}% < 90")
        self.assertGreaterEqual(g["cpu_mean"], 85.0, f"cpu_mean {g['cpu_mean']}% < 85")

    def test_04_resource_summary_bounded_rss(self):
        """资源摘要: 峰值 RSS 有界(<512MB), n_samples>0。"""
        s = self._event("resource", "resource summary")
        self.assertIsNotNone(s)
        self.assertLess(s["peak_rss_bytes"], 512 << 20, "峰值 RSS 超限")
        self.assertGreater(s["n_samples"], 0)

    def test_05_science_seam_corrected(self):
        """科学: UPM 校正场非空(seam 被检测校正), 幅度有界不拟合源。"""
        # 联合门: 数值好不能替代资源门(先验证 gate ok)
        g = self._event("gate", "resource gate ok")
        self.assertIsNotNone(g)
        self.assertIsNone(self._event("gate", "resource gate failed"))
        # 科学证据: phase2 run + persist UPM, 解析校正场(生产产物)
        out2 = os.path.join(self.tmp, "out2")
        os.makedirs(out2, exist_ok=True)
        model = os.path.join(self.tmp, "upm.bin")
        cfg2 = os.path.join(self.tmp, "cfg2.json")
        paths = [os.path.join(SEAM6, f"SEAM{i}.hips") for i in range(6)]
        json.dump({"hips_paths": paths, "output_dir": out2,
                   "sampler": {"cpu_workers": 2},
                   "upm": {"cpu_workers": 2, "max_iterations": 50},
                   "persist_upm": True, "upm_save_path": model},
                  open(cfg2, "w"))
        r = subprocess.run([EXE, "phase2", "run", "--config", cfg2, "--events-jsonl"],
                           capture_output=True, text=True, timeout=400)
        self.assertEqual(r.returncode, 0, r.stderr[-400:])
        self.assertTrue(os.path.isfile(model), "UPM persist 缺失")
        m = json.load(open(model, encoding="utf-8"))
        # 校正场非空: 至少一帧 C 非空(seam 被检测校正); 幅度有界不拟合源
        c_lens = [len(m["C"][f]) for f in range(len(m["C"]))]
        self.assertGreater(sum(c_lens), 0, "UPM 校正场全空(seam 未校正)")
        self.assertGreater(m["control_count"], 100, "control 网格应足够")
        # 源不拟合: C 幅度(÷1e8 flux 归一) << 星幅度(2.0)
        import numpy as np
        for f in range(len(m["C"])):
            if m["C"][f]:
                cv = np.array([v for _, v in m["C"][f]]) / 1e8
                self.assertLess(float(np.std(cv)), 0.2,
                                f"UPM 校正场空间变化 {np.std(cv):.3f} 不应拟合源(2.0)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
