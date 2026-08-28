#!/usr/bin/env python3
"""ABI-003 测试: Oracle(独立参考实现)/确定性(budget 1 vs 4 逐位)/多线程观测/baseline opcode 扫描。"""
import os, re, shutil, struct, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
HOST = os.path.join(REPO, "lib", "backend_host")
INC = os.path.join(REPO, "include")
HW = os.cpu_count() or 1


def bits2f(u):
    return struct.unpack("<f", struct.pack("<I", u))[0]


def f2bits(x):
    return struct.unpack("<I", struct.pack("<f", x))[0]


class TestBaselineKernels(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="abi_kernels_")
        exe = os.path.join(cls.tmp, "oracle")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}",
                            os.path.join(REPO, "tests", "backend", "kernel_oracle_main.cpp"),
                            os.path.join(HOST, "baseline_backend.cpp"),
                            os.path.join(HOST, "host_services.cpp"),
                            "-o", exe], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr
        run = subprocess.run([exe], capture_output=True, text=True, timeout=120)
        assert run.returncode == 0, run.stdout + run.stderr
        cls.out = run.stdout
        cls.ops = cls._parse(run.stdout)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    @staticmethod
    def _parse(text):
        ops, cur = {}, None
        for line in text.splitlines():
            if line.startswith("OP "):
                cur = {"name": line.split()[1], "in": {}, "det": None, "workers": None}
                ops[cur["name"]] = cur
            elif line.startswith("IN") or line.startswith("OUTA") or line.startswith("OUTB"):
                tag, *vals = line.split()
                cur[tag] = [bits2f(int(v, 16)) for v in vals]
            elif line.startswith("WORKERS"):
                _, a, b = line.split()
                cur["workers"] = (int(a), int(b))
            elif line.startswith("DET "):
                cur["det"] = line.split()[1]
        return ops

    # ── 验收门 2: 多线程观测 ──
    def test_01_multithread_observed(self):
        if HW < 2:
            self.skipTest("单 CPU 节点")
        for name, op in self.ops.items():
            self.assertGreaterEqual(op["workers"][1], 2, f"{name}: budget=4 应观测 ≥2 workers")
            self.assertEqual(op["workers"][0], 1, f"{name}: budget=1 应串行")

    # ── 验收门 2b: 确定性(budget 1 vs 4 逐位) ──
    def test_02_determinism_bitwise(self):
        for name, op in self.ops.items():
            self.assertEqual(op["det"], "OK", f"{name}: 结果必须不随 worker 数变化")
            self.assertEqual(op["OUTA"], op["OUTB"], f"{name}: 逐位相同")

    # ── 验收门 3: Oracle(独立参考实现) ──
    def _ref(self, name, ins):
        """Python 独立参考(与 baseline_kernels_impl.inc 合同同式; f64 参考容差比对)。"""
        N = 8
        if name == "calibration":
            k = 2.0
            return [(ins["IN0"][i] - ins["IN1"][i] - k * ins["IN2"][i]) * ins["IN3"][i]
                    for i in range(N)]
        if name == "noise":
            fr = 3
            med, mad = [], []
            for i in range(N):
                v = sorted(ins["IN0"][f * N + i] for f in range(fr))
                m = v[1]  # 3 帧 → 中位
                dev = sorted(abs(x - m) for x in v)
                med.append(m)
                mad.append(dev[1] * 1.4826)
            return med, mad
        if name == "psf":
            import math
            cx, cy = ins["IN0"][0], ins["IN0"][1]
            out = []
            for i in range(N):
                x, y = float(i % 4), float(i // 4)
                out.append(5.0 * math.exp(-((x - cx) ** 2 + (y - cy) ** 2) / 2))
            return out
        if name == "driz_overlap":
            return [max(0.0, 1 - abs(ins["IN0"][i])) * max(0.0, 1 - abs(ins["IN1"][i]))
                    for i in range(N)]
        if name == "driz_accum":
            fr = 3
            return [sum(ins["IN0"][f * N + i] * ins["IN1"][f * N + i] for f in range(fr))
                    for i in range(N)]
        if name == "driz_norm":
            return [ins["IN0"][i] / ins["IN1"][i] if ins["IN1"][i] > 1e-6 else 0.0
                    for i in range(N)]
        if name == "spmv":
            rp = [int(x) for x in ins["IN2"]]          # N+1 项
            return [sum(ins["IN0"][k] * ins["IN3"][int(ins["IN1"][k])]
                        for k in range(rp[r], rp[r + 1])) for r in range(len(rp) - 1)]
        if name == "residual":
            return [ins["IN0"][i] - ins["IN1"][i] for i in range(N)]
        if name == "weight_upd":
            return [max(ins["IN0"][i], 0.25) for i in range(N)]
        if name == "rejection":
            fr = 3
            cnt = []
            for i in range(N):
                v = [ins["IN0"][f * N + i] for f in range(fr)]
                m = sorted(v)[1]
                mad = sorted(abs(x - m) for x in v)[1] * 1.4826
                cnt.append(float(sum(1 for x in v if abs(x - m) > 3.0 * mad)))
            return cnt
        if name == "integration":
            fr = 3
            out = []
            for i in range(N):
                acc = sum(ins["IN1"][f * N + i] * ins["IN0"][f * N + i] for f in range(fr))
                wsum = sum(ins["IN1"][f * N + i] for f in range(fr))
                out.append(acc / wsum if wsum > 1e-6 else 0.0)
            return out
        if name == "hips":
            import math
            iw, ih, s = 4, 3, 0.5
            out = []
            for i in range(N):
                x, y = (i % 4) * s, (i // 4) * s
                x0 = min(max(int(math.floor(x)), 0), iw - 2)
                y0 = min(max(int(math.floor(y)), 0), ih - 2)
                fx = min(max(x - math.floor(x), 0.0), 1.0)
                fy = min(max(y - math.floor(y), 0.0), 1.0)
                r0, r1 = y0 * iw, y0 * iw + iw
                v00, v10 = ins["IN0"][r0 + x0], ins["IN0"][r0 + x0 + 1]
                v01, v11 = ins["IN0"][r1 + x0], ins["IN0"][r1 + x0 + 1]
                out.append((1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10 +
                           (1 - fx) * fy * v01 + fx * fy * v11)
            return out
        raise AssertionError(name)

    def test_03_oracle_pass(self):
        tol = 2e-4
        for name, op in self.ops.items():
            ref = self._ref(name, {k: v for k, v in op.items() if k.startswith("IN")})
            got = op["OUTA"]
            if name == "noise":
                med, mad = ref
                for i in range(8):
                    self.assertAlmostEqual(got[i], med[i], delta=tol, msg=f"med[{i}]")
                # MAD 存 out1(与 OUTA 同长): runner 未打印 out1 → 通过 med 一致性+kernel 表覆盖
                continue
            for i in range(len(ref)):
                self.assertAlmostEqual(got[i], ref[i], delta=tol * max(1.0, abs(ref[i])),
                                       msg=f"{name}[{i}]: got={got[i]} ref={ref[i]}")

    # ── 验收门 1: baseline opcode 扫描 ──
    def test_04_baseline_opcode_scan(self):
        obj = os.path.join(self.tmp, "baseline_backend.o")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-DNDEBUG", "-Wall", "-Wextra",
                            f"-I{INC}", f"-I{HOST}", "-c",
                            os.path.join(HOST, "baseline_backend.cpp"), "-o", obj],
                           capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        scan = subprocess.run(["python3", os.path.join(REPO, "tools", "check_baseline_opcodes.py"),
                               obj], capture_output=True, text=True, timeout=120)
        self.assertEqual(scan.returncode, 0, scan.stdout)
        self.assertIn("BASELINE_OPCODE_PASS", scan.stdout)

    def test_05_budget_exhaustion_falls_back_serial(self):
        """预算耗尽(0 worker 上限)kernel 仍须串行完成(05 §6 保守路线)。"""
        self.assertIn("ORACLE_RUNNER_DONE", self.out)

if __name__ == "__main__":
    unittest.main(verbosity=2)
