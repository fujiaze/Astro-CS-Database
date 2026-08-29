#!/usr/bin/env python3
"""ISA-003 测试: AVX2+FMA 变体(capability 复验+登记) — FMA 指令/ymm 证明(双向)+测量工件+决策台账冻结。
ISA-001 已 SHIP avx2_backend.so; 本任务独立复测确认 AVX2+FMA SHIP 成立, 不重复实现。"""
import csv, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")


class TestIsaAvx2Fma(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="isa_avx2fma_")
        cls.vso = os.path.join(cls.tmp, "avx2_backend.so")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-mavx2", "-mfma",
                            "-fPIC", "-shared", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}",
                            os.path.join(HOST, "avx2_backend.cpp"), "-o", cls.vso],
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

    def test_01_avx2_fma_has_fma_and_ymm_baseline_clean(self):
        """双向: baseline 零 VEX; AVX2+FMA 变体真含 FMA 指令(vfnmadd)+ ymm(256-bit)。"""
        scan = subprocess.run(["python3", os.path.join(REPO, "tools", "check_baseline_opcodes.py"),
                               self.base_obj], capture_output=True, text=True, timeout=120)
        self.assertEqual(scan.returncode, 0,
                         f"baseline 不得含 AVX opcode: {scan.stdout} {scan.stderr}")
        dis = subprocess.run(["objdump", "-d", self.vso], capture_output=True, text=True,
                             timeout=120).stdout
        fma = re.findall(r"v(?:fmadd|fmsub|fnmadd|fnmsub|fmsuba?dddj?)\S*", dis)
        self.assertTrue(fma, f"AVX2+FMA 变体必须真含 FMA 指令(vfnmadd...), 得 {fma[:5]}")
        self.assertGreaterEqual(len(re.findall(r"%ymm[0-9]+", dis)), 1,
                                "AVX2+FMA 变体必须含 ymm(256-bit) 寄存器")

    def test_02_shared_contract_single_source(self):
        v = open(os.path.join(HOST, "avx2_backend.cpp"), encoding="utf-8").read()
        self.assertIn('#include "baseline_kernels_impl.inc"', v)
        self.assertIn('#include "backend_table.inc"', v)
        self.assertIn('ASTROCS_BACKEND_ID "avx2"', v)

    def test_03_bench_and_measurement_artifact(self):
        r = subprocess.run([self.bench, "--variant", self.vso],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("VARIANT_LOADED avx2", r.stdout)
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
        self.assertIn("calibration", v)
        out = os.path.join(REPO, "artifacts", "prerelease_v5", "ISA-003", "MEASUREMENTS.csv")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["op", "baseline_ns", "avx2_variant_ns", "improvement_pct", "decision"])
            for op in sorted(b):
                imp = (b[op] - v[op]) / b[op] * 100 if op in v else ""
                w.writerow([op, f"{b[op]:.0f}", f"{v[op]:.0f}" if op in v else "",
                            f"{imp:+.1f}" if op in v else "",
                            "SHIP(avx2)" if imp != "" and op in ("calibration", "hips") else ""])
        self.assertTrue(os.path.isfile(out))

    def test_04_decision_ledger_records_avx2_fma(self):
        doc = open(os.path.join(REPO, "docs", "architecture", "ISA_VARIANTS.md"),
                   encoding="utf-8").read()
        self.assertIn("AVX2+FMA", doc)
        self.assertIn("SHIP(avx2)", doc)
        self.assertIn("MEASUREMENTS.csv", doc)


if __name__ == "__main__":
    unittest.main(verbosity=2)
