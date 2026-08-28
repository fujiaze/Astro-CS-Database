#!/usr/bin/env python3
"""BENCH-003 测试: 内存基线/worker-block 候选派生(零硬编码)/资源指标/原始样本引用。"""
import hashlib, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")


class TestBenchCandidates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="bcand_")
        common = [os.path.join(HOST, "bench_harness.cpp"),
                  os.path.join(HOST, "host_services.cpp"),
                  os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
                  f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}"]
        cls.probe = os.path.join(cls.tmp, "cprobe")
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{INC}", f"-I{HOST}",
                            os.path.join(REPO, "tests", "backend", "candidates_probe_main.cpp"),
                            *common, "-o", cls.probe], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr
        cls.runner = os.path.join(cls.tmp, "bcand")
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{INC}", f"-I{HOST}",
                            os.path.join(REPO, "tests", "backend", "bench_candidates_main.cpp"),
                            os.path.join(HOST, "baseline_backend.cpp"), *common, "-ldl",
                            "-o", cls.runner], capture_output=True, text=True, timeout=240)
        assert r.returncode == 0, r.stderr
        cls.samples = os.path.join(cls.tmp, "samples.txt")
        cls.run_out = subprocess.run([cls.runner, cls.samples], capture_output=True, text=True,
                                 timeout=300)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_worker_candidates_derived_not_hardcoded(self):
        out = subprocess.run([self.probe], capture_output=True, text=True, timeout=60).stdout
        for avail, expect in (("1", "1"), ("2", "1 2"), ("3", "1 2 3"),
                              ("8", "1 4 8"), ("16", "1 8 16"), ("33", "1 17 33")):
            m = re.search(rf"W avail={avail}:(.*)", out)
            self.assertIsNotNone(m, avail)
            self.assertEqual(m.group(1).split(), expect.split(),
                             f"avail={avail} 候选必须为 {{1, 中位, 全部}} 派生")
        self.assertNotIn("W avail=16: 2 4 8 16", out, "禁止固定 {2,4,8,16} 表")

    def test_02_block_candidates_geometric_from_l2(self):
        out = subprocess.run([self.probe], capture_output=True, text=True, timeout=60).stdout
        blocks = dict(re.findall(r"BLOCK l2=(\d+): (.*)", out))
        self.assertEqual(len(blocks), 3)
        base2 = None
        for l2, lst in blocks.items():
            vals = [int(x) for x in lst.split()]
            self.assertEqual(len(vals), 4, "几何序列 4 档")
            for a, b in zip(vals, vals[1:]):
                self.assertAlmostEqual(b / a, 4.0, msg="公比 4")
            if base2 is None:
                base2 = vals
            else:
                self.assertNotEqual(vals, base2, "不同 L2 → 不同候选(派生非写死)")
        self.assertNotIn(str(2 << 20), "".join(blocks.values()))

    def test_03_memory_bandwidth_plausible(self):
        m = re.search(r"MEMORY read=([\d.]+) write=([\d.]+) copy=([\d.]+) "
                      r"triad32=([\d.]+) triad64=([\d.]+) rss=(\d+)", self.run_out.stdout)
        self.assertIsNotNone(m, "MEMORY 行缺失")
        vals = [float(x) for x in m.groups()[:5]]
        for v in vals:
            self.assertGreater(v, 0.5, "带宽 >0.5 GB/s")
            self.assertLess(v, 500.0, "带宽 <500 GB/s(异常防护)")
        self.assertGreater(int(m.group(6)), 0, "资源指标 rss 采样存在")

    def test_04_kernel_sweep_and_raw_samples(self):
        self.assertIn("CANDIDATES_DONE", self.run_out.stdout)
        for cls_name in ("small", "medium", "large"):
            self.assertRegex(self.run_out.stdout, rf"BEST {cls_name} align0 w\d+ [\d.]+")
        # 原始样本引用: 文件存在+非空+sha256 可算(引用完整性)
        data = open(self.samples, "rb").read()
        self.assertGreater(len(data), 100)
        self.assertIn(b"small align0 w1", data)
        digest = hashlib.sha256(data).hexdigest()
        self.assertRegex(digest, r"^[0-9a-f]{64}$")

    def test_05_generator_source_no_core_constants(self):
        """验收: 候选生成源码不含硬编码 core count 数值表。"""
        src = open(os.path.join(HOST, "bench_harness.cpp"), encoding="utf-8").read()
        body = src[src.index("std::vector<uint32_t> worker_candidates"):]
        body = body[:body.index("std::vector<uint64_t> block_candidates")]
        self.assertNotRegex(body, r"\{\s*1u?,\s*2u?,\s*4u?", "禁止 {1,2,4,...} 固定表")
        self.assertNotRegex(body, r"\b16u\b")
        self.assertIn("(available_cpus + 1) / 2u", body, "中位候选=派生")

if __name__ == "__main__":
    unittest.main(verbosity=2)
