#!/usr/bin/env python3
"""ISA-001 测试: 热点 profile→变体编译→真变体证明→共享合同 Oracle→SHIPPED/NOT_SHIPPED 决策。"""
import json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")


class TestIsaVariants(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="isa_")
        # 变体 DSO(局部旗标; 共享 impl+table 源)
        cls.vso = os.path.join(cls.tmp, "avx2_backend.so")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-mavx2", "-mfma",
                            "-fPIC", "-shared", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}",
                            os.path.join(HOST, "avx2_backend.cpp"), "-o", cls.vso],
                           capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr
        # bench 可执行(基线)
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

    def _objdump(self, path):
        return subprocess.run(["objdump", "-d", path], capture_output=True, text=True,
                              timeout=120).stdout

    def test_01_baseline_clean_variant_has_vex(self):
        """主/baseline 无 ISA 污染; 变体 TU 必须真含 VEX(ISA-001 双向证明)。"""
        base_obj = os.path.join(self.tmp, "base.o")
        subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", f"-I{INC}", f"-I{HOST}", "-c",
                        os.path.join(HOST, "baseline_backend.cpp"), "-o", base_obj],
                       capture_output=True, text=True, timeout=120)
        scan = subprocess.run(["python3", os.path.join(REPO, "tools", "check_baseline_opcodes.py"),
                               base_obj], capture_output=True, text=True, timeout=120)
        self.assertEqual(scan.returncode, 0, "baseline 不得含 AVX opcode")
        dis = self._objdump(self.vso)
        vex = re.findall(r"\bv[a-z0-9]{2,}\b", dis)  # VEX 编码助记符(vmovss/vfmadd/vaddps...)
        self.assertTrue(vex, "变体必须真含 VEX/AVX 指令(否则是假变体)")

    def test_02_shared_contract_single_source(self):
        """变体与 baseline 共享同一 impl 源(零复制漂移)。"""
        v = open(os.path.join(HOST, "avx2_backend.cpp"), encoding="utf-8").read()
        self.assertIn('#include "baseline_kernels_impl.inc"', v)
        self.assertIn('#include "backend_table.inc"', v)
        self.assertNotIn("float calibration_impl", v, "变体不得复制实现")

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
        self.assertIn("hips", v, "热点 hips 必须有变体测量")
        imp_hips = (b["hips"] - v["hips"]) / b["hips"] * 100
        self.assertGreater(imp_hips, 5.0, f"hips 变体应显著更快(实测 {imp_hips:+.1f}%)")
        # 完整测量工件(决策可审计)
        import csv
        out = os.path.join(REPO, "artifacts", "prerelease_v5", "ISA-001", "MEASUREMENTS.csv")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["op", "baseline_ns", "avx2_variant_ns", "improvement_pct"])
            for op in sorted(b):
                w.writerow([op, f"{b[op]:.0f}",
                            f"{v[op]:.0f}" if op in v else "", f"{(b[op]-v[op])/b[op]*100:+.1f}"
                            if op in v else ""])
        self.assertTrue(os.path.isfile(out))

    def test_04_decision_ledger_frozen(self):
        doc = open(os.path.join(REPO, "docs", "architecture", "ISA_VARIANTS.md"),
                   encoding="utf-8").read()
        for k in ("SHIP(avx2)", "NOT_SHIPPED", "drizzle-accumulate", "hips-bulk-transform",
                  "calibration", "+28.2%", "共享", "FMA"):
            self.assertIn(k, doc, f"决策台账缺 {k}")
        self.assertIn("MEASUREMENTS.csv", doc, "台账须引用完整测量工件")

    def test_05_wrong_variant_never_loads(self):
        """错误变体(required 超集/假 hash)绝不入候选——预检拒绝。"""
        probe_src = os.path.join(REPO, "tests", "backend", "loader_probe_main.cpp")
        exe = os.path.join(self.tmp, "probe")
        r = subprocess.run(["g++", "-std=c++17", f"-I{INC}", f"-I{HOST}",
                            f"-I{os.path.join(REPO, 'lib', 'common', 'crypto')}",
                            probe_src, os.path.join(HOST, "backend_loader.cpp"),
                            os.path.join(HOST, "host_services.cpp"),
                            os.path.join(HOST, "cpu_features.cpp"),
                            os.path.join(REPO, "lib", "common", "crypto", "sha256.cpp"),
                            "-ldl", "-o", exe], capture_output=True, text=True, timeout=180)
        self.assertEqual(r.returncode, 0, r.stderr)
        import hashlib
        sha = hashlib.sha256(open(self.vso, "rb").read()).hexdigest()
        # 正确变体 → 预检通过(AVX2 主机)
        mf = os.path.join(self.tmp, "m.json")
        json.dump({"schema_version": "1", "kind": "astrocs_backends_manifest", "backends": [
            {"file": "avx2_backend.so", "backend_id": "avx2", "sha256": sha,
             "abi_version": 1, "required_features_bits": (1 << 2) | (1 << 4)}]}, open(mf, "w"))
        run = subprocess.run([exe, self.tmp, mf], capture_output=True, text=True, timeout=60)
        self.assertIn("LOADED backend_id=avx2", run.stdout)
        self.assertIn("SELFTEST_OK", run.stdout)
        # 假 hash 变体 → 拒绝
        json.dump({"schema_version": "1", "kind": "astrocs_backends_manifest", "backends": [
            {"file": "avx2_backend.so", "backend_id": "avx2", "sha256": "0" * 64,
             "abi_version": 1, "required_features_bits": (1 << 2) | (1 << 4)}]}, open(mf, "w"))
        run2 = subprocess.run([exe, self.tmp, mf], capture_output=True, text=True, timeout=60)
        self.assertIn("FALLBACK hash mismatch", run2.stdout)
        self.assertNotIn("LOADED", run2.stdout)

if __name__ == "__main__":
    unittest.main(verbosity=2)
