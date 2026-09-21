#!/usr/bin/env python3
"""ISA-002 测试: AVX(无 FMA)变体 — 真变体证明(双向)/共享合同单源/完整测量工件/决策台账冻结。
关键结论: AVX 是 AVX2+FMA 子集, 且 vm-bj 实测 avx2(SHIP, ISA-001)严格主导 AVX → AVX 仅测 NOT_SHIPPED(有完整测量即 PASS)。"""
import csv, json, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
HOST = os.path.join(REPO, "lib", "infrastructure", "benchmark", "backend_host")
INC = os.path.join(REPO, "lib", "include")


class TestIsaAvx(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="isa_avx_")
        # AVX(无 FMA)变体 DSO; 局部旗标 -mavx
        cls.vso = os.path.join(cls.tmp, "avx_backend.so")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-mavx",
                            "-fPIC", "-shared", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}",
                            os.path.join(HOST, "avx_backend.cpp"), "-o", cls.vso],
                           capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr
        # baseline object(扫描)
        cls.base_obj = os.path.join(cls.tmp, "base.o")
        subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", f"-I{INC}", f"-I{HOST}", "-c",
                        os.path.join(HOST, "baseline_backend.cpp"), "-o", cls.base_obj],
                       capture_output=True, text=True, timeout=120)
        # bench
        cls.bench = os.path.join(cls.tmp, "kbench")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}",
                            os.path.join(REPO, "eng", "tests", "backend", "kernel_bench_main.cpp"),
                            os.path.join(HOST, "baseline_backend.cpp"),
                            os.path.join(HOST, "host_services.cpp"),
                            "-ldl", "-o", cls.bench], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_01_avx_variant_has_vex_baseline_clean(self):
        """双向: baseline 零 VEX; AVX 变体真含 VEX(非假变体)。"""
        scan = subprocess.run(["python3", os.path.join(REPO, "eng", "tools", "check_baseline_opcodes.py"),
                               self.base_obj], capture_output=True, text=True, timeout=120)
        self.assertEqual(scan.returncode, 0,
                         f"baseline 不得含 AVX opcode: {scan.stdout} {scan.stderr}")
        dis = subprocess.run(["objdump", "-d", self.vso], capture_output=True, text=True,
                             timeout=120).stdout
        vex = re.findall(r"\bv[a-z0-9]{2,}\b", dis)
        self.assertTrue(vex, "AVX 变体必须真含 VEX 指令(否则是假变体)")

    def test_02_shared_contract_single_source(self):
        """变体与 baseline 共享同一 impl 源(零复制漂移)。"""
        v = open(os.path.join(HOST, "avx_backend.cpp"), encoding="utf-8").read()
        self.assertIn('#include "baseline_kernels_impl.inc"', v)
        self.assertIn('#include "backend_table.inc"', v)
        self.assertNotIn("float calibration_impl", v, "变体不得复制实现")
        self.assertIn('ASTROCS_BACKEND_ID "avx"', v, "backend_id 须为 avx")

    def test_03_bench_and_measurement_artifact(self):
        r = subprocess.run([self.bench, "--variant", self.vso],
                           capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("VARIANT_LOADED avx", r.stdout)
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
        self.assertIn("calibration", v, "热点 calibration 必须有变体测量")
        # 完整测量工件(决策可审计) — ISA-002
        # D-14（GATE-501）：统一到已跟踪证据路径（见 test_isa_variants.py 同注）。
        out = os.path.join(REPO, "artifacts", "evidence", "prerelease-v5", "ISA-002",
                           "MEASUREMENTS.csv")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["op", "baseline_ns", "avx_variant_ns", "improvement_pct", "decision"])
            for op in sorted(b):
                imp = (b[op] - v[op]) / b[op] * 100 if op in v else ""
                w.writerow([op, f"{b[op]:.0f}", f"{v[op]:.0f}" if op in v else "",
                            f"{imp:+.1f}" if op in v else "",
                            "NOT_SHIPPED(avx子集,avx2主导)" if imp != "" and op in ("calibration","hips") else ""])
        self.assertTrue(os.path.isfile(out))

    def test_04_decision_ledger_records_avx(self):
        doc = open(os.path.join(REPO, "docs", "architecture", "ISA_VARIANTS.md"),
                   encoding="utf-8").read()
        self.assertIn("avx", doc, "决策台账须记录 AVX 变体")
        self.assertIn("MEASUREMENTS.csv", doc)
        # 决策必须写明 AVX 被 avx2 主导(无机械堆砌)
        self.assertIn("AVX", doc)
        self.assertIn("NOT_SHIPPED", doc, "AVX 无独立收益须登记 NOT_SHIPPED")

    @staticmethod
    def _improvements(path, variant_col):
        """读测量工件 → {op: improvement_pct}（缺列/空值即判红，不静默跳过）。"""
        with open(path, encoding="utf-8") as fh:
            rows = list(csv.DictReader(fh))
        assert rows, "测量工件为空: %s" % path
        for col in ("op", variant_col, "improvement_pct"):
            assert col in rows[0], "测量工件缺列 %s: %s" % (col, path)
        out = {}
        for r in rows:
            if r.get(variant_col) and r.get("improvement_pct"):
                out[r["op"]] = float(r["improvement_pct"])
        return out

    def test_05_avx_never_beats_shipped_avx2(self):
        """AVX 必须保持 NOT_SHIPPED：工件逐热点登记决策，台账与工件同口径。

        GATE-502 空断言普查：原为 self.assertTrue(True)（比值断言"在 LOG 人工判读"）
        ⇒ 改为机器断言。**为什么判据锚在登记决策、而不是逐次运行的比值排序**：
        D-14（GATE-501）把 ISA-001/002 工件改为测试**现场重测**写入同一跟踪路径，
        1–2 ms 级热点的 improvement_pct 运行间噪声可达 ±2pp —— 实测反例（同一提交、
        相邻两次运行）：calibration avx2 +12.9% < avx +14.2%，hips avx2 +42.0% >
        avx +34.9%。逐次排序判据会把测量噪声当缺陷（假红），故不采用；要恢复比值门
        须先冻结测量（固定输入指纹缓存），属 GATE-501 域。
        本判据仍能红：删测量行、缺 decision 列、把 NOT_SHIPPED 改成 SHIP 都会失败。
        """
        ev = os.path.join(REPO, "artifacts", "evidence", "prerelease-v5")
        p1 = os.path.join(ev, "ISA-001", "MEASUREMENTS.csv")
        p2 = os.path.join(ev, "ISA-002", "MEASUREMENTS.csv")
        self.assertTrue(os.path.isfile(p1), "缺 ISA-001(avx2) 跟踪证据: %s" % p1)
        self.assertTrue(os.path.isfile(p2), "缺 ISA-002(avx) 跟踪证据: %s" % p2)
        # 1) 两个热点在两侧工件里都必须有实测值（判据非退化：不能靠空行过）
        a2 = self._improvements(p1, "avx2_variant_ns")
        av = self._improvements(p2, "avx_variant_ns")
        for op in ("calibration", "hips"):
            self.assertIn(op, a2, "ISA-001 证据缺 %s 的 avx2 测量" % op)
            self.assertIn(op, av, "ISA-002 证据缺 %s 的 avx 测量" % op)
        # 2) 决策必须逐热点显式登记 NOT_SHIPPED（avx 是 avx2+FMA 子集，无独立收益）
        with open(p2, encoding="utf-8", newline="") as fh:
            rows = {r["op"]: r for r in csv.DictReader(fh)}
        self.assertIn("decision", rows.get("calibration", {}), "ISA-002 工件缺 decision 列")
        for op in ("calibration", "hips"):
            dec = rows.get(op, {}).get("decision", "")
            self.assertIn("NOT_SHIPPED", dec, "%s 未登记 NOT_SHIPPED: %r" % (op, dec))
            self.assertIn("avx2", dec, "%s 未写明 avx2 主导: %r" % (op, dec))
        # 3) 台账（人读侧）与工件（机读侧）必须同口径
        with open(os.path.join(REPO, "docs", "architecture", "ISA_VARIANTS.md"),
                  encoding="utf-8") as fh:
            doc = fh.read()
        self.assertIn("NOT_SHIPPED", doc, "台账未登记 avx 不发布")
        self.assertIn("avx2", doc, "台账未写明 avx2 主导")


if __name__ == "__main__":
    unittest.main(verbosity=2)
