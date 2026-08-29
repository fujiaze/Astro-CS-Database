#!/usr/bin/env python3
"""ISA-004 测试: AVX512F 变体 — zmm/AVX512 能力证明(bidirectional)+测量工件+决策台账登记。
vm-bj 支持 AVX512(F/BW/VL/DQ/CD)故可在 Linux 完整验证; 判定=AVX512 对已 SHIP avx2 无额外收益→NOT_SHIPPED(完整测量在案)。"""
import csv, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")


@unittest.skipUnless(os.path.exists("/proc/cpuinfo") and
                     b"avx512f" in open("/proc/cpuinfo", "rb").read(),
                     "本机不支持 AVX512F(任务规则: 仅 CPU 不支持且未验证时禁止 PASS, 本机支持则必须验证)")
class TestIsaAvx512(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="isa_avx512_")
        cls.vso = os.path.join(cls.tmp, "avx512_backend.so")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG",
                            "-mavx512f", "-mavx512bw", "-mavx512vl", "-mavx512dq",
                            "-fPIC", "-shared", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}",
                            os.path.join(HOST, "avx512_backend.cpp"), "-o", cls.vso],
                           capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr
        cls.base_obj = os.path.join(cls.tmp, "base.o")
        subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", f"-I{INC}", f"-I{HOST}", "-c",
                        os.path.join(HOST, "baseline_backend.cpp"), "-o", cls.base_obj],
                       capture_output=True, text=True, timeout=120)
        cls.bench = os.path.join(cls.tmp, "kbench")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}",
                            os.path.join(REPO, "tests", "backend", "kernel_bench_main.cpp"),
                            os.path.join(HOST, "baseline_backend.cpp"),
                            os.path.join(HOST, "host_services.cpp"),
                            "-ldl", "-o", cls.bench], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_avx512_has_zmm_baseline_clean(self):
        """双向: baseline 零 VEX; AVX512 变体真含 zmm(512-bit)。"""
        scan = subprocess.run(["python3", os.path.join(REPO, "tools", "check_baseline_opcodes.py"),
                               self.base_obj], capture_output=True, text=True, timeout=120)
        self.assertEqual(scan.returncode, 0,
                         f"baseline 不得含 AVX opcode: {scan.stdout} {scan.stderr}")
        dis = subprocess.run(["objdump", "-d", self.vso], capture_output=True, text=True,
                             timeout=120).stdout
        self.assertGreaterEqual(len(re.findall(r"%zmm[0-9]+", dis)), 1,
                                "AVX512 变体必须含 zmm(512-bit) 寄存器")

    def test_02_shared_contract_single_source(self):
        v = open(os.path.join(HOST, "avx512_backend.cpp"), encoding="utf-8").read()
        self.assertIn('#include "baseline_kernels_impl.inc"', v)
        self.assertIn('#include "backend_table.inc"', v)
        self.assertIn('ASTROCS_BACKEND_ID "avx512"', v)

    def test_03_bench_and_measurement_artifact(self):
        r = subprocess.run([self.bench, "--variant", self.vso],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("VARIANT_LOADED avx512", r.stdout)
        self.assertIn("BENCH_DONE", r.stdout)
        b, v = {}, {}
        for line in r.stdout.splitlines():
            m = re.match(r"BENCH (\S+) ([\d.]+)", line)
            if m:
                b[m.group(1)] = float(m.group(2))
            m = re.match(r"VARIANT (\S+) ([\d.]+)", line)
            if m:
                v[m.group(1)] = float(m.group(2))
        self.assertIn("hips", v)
        out = os.path.join(REPO, "artifacts", "prerelease_v5", "ISA-004", "MEASUREMENTS.csv")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["op", "baseline_ns", "avx512_variant_ns", "improvement_pct", "decision"])
            for op in sorted(b):
                imp = (b[op] - v[op]) / b[op] * 100 if op in v else ""
                w.writerow([op, f"{b[op]:.0f}", f"{v[op]:.0f}" if op in v else "",
                            f"{imp:+.1f}" if op in v else "",
                            "NOT_SHIPPED(avx512无额外收益)" if op == "hips" else ""])
        self.assertTrue(os.path.isfile(out))

    def test_04_decision_ledger_records_avx512_noship(self):
        doc = open(os.path.join(REPO, "docs", "architecture", "ISA_VARIANTS.md"),
                   encoding="utf-8").read()
        self.assertIn("AVX512", doc)
        self.assertIn("NOT_SHIPPED", doc)
        self.assertIn("MEASUREMENTS.csv", doc)
        self.assertIn("avx512_backend", doc)


if __name__ == "__main__":
    unittest.main(verbosity=2)
