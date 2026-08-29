#!/usr/bin/env python3
"""SYN-008 独立合成 Oracle — 三块重叠合成场 seam 指标门槛(预冻结)。
验收(03 L130 + 00 L166): 三块重叠合成场, 已知背景面+星点+coverage → seam 指标门槛预冻结并过;
                       无"视觉上可以"替代。
方法(independent):
  - 构造 3 重叠合成场(帧0/1/2)观测: 已知背景 true_sky + 每帧 additive field(frame_field,
    smooth) + 星点(幅度 40)+ coverage; 经 `p2_upm_build` 联合求解每帧 C 场。
  - seam 指标(与 SCI-005 + REAUDIT seam 邻接差一致): 同一控制 cell 在重叠帧下 UPM 校准输出
    `out = obs - C_frame` 的交叉帧差 |out_f0 - out_f1|; 对平滑重叠场, 校准应使跨帧输出均≈true_sky → seam 小。
  - 门限预冻结: cross-frame seam p95 <= 3σ, max <= 5σ(σ=kNoiseRms=0.05); 星点 cell 不计入 seam
    (其本应高), 但星邻域场平滑(不破坏星 flux)。
  - 亦验证 UPM 参数恢复(control_count>=阈值, component 合理)与全 coverage。
"""
import math, os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PH2 = os.path.join(REPO, "lib", "phase2")
INC = os.path.join(REPO, "include")
C = os.path.join(REPO, "lib", "common")
CASTRO = os.path.join(C, "healpix")
AIO = os.path.join(REPO, "lib", "astro_image_io")
OMP_LIB = os.path.join(REPO, "build", "linux-openmp-on", "libphase2.a")
SIGMA = 0.05  # 同步 synthetic_gate kNoiseRms


@unittest.skipUnless(shutil.which("g++") and os.path.isfile(OMP_LIB), "需要 g++ + OpenMP phase2 lib")
class TestSeamMetricGate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="syn008_")
        cls.exe = os.path.join(cls.tmp, "syn8")
        r = subprocess.run(["g++", "-std=c++17", "-O3", "-DNDEBUG", "-fopenmp",
                            f"-I{INC}", f"-I{os.path.join(PH2, 'include')}", f"-I{PH2}",
                            f"-I{C}", f"-I{CASTRO}", f"-I{os.path.join(AIO, 'include')}",
                            f"-I{os.path.join(AIO, 'src')}", f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                            os.path.join(REPO, "tests", "backend", "syn008_seam_main.cpp"),
                            OMP_LIB, os.path.join(AIO, "astro_image_io.dll"),
                            "-lgomp", "-lz", "-lzstd", "-llz4", "-pthread", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-1200:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self):
        r = subprocess.run([self.exe], capture_output=True, text=True, timeout=300)
        self.assertEqual(r.returncode, 0, r.stderr[-300:] + r.stdout[-200:])
        m = re.search(r"SEAM n=(\d+) p50 ([\d.]+) p95 ([\d.]+) max ([\d.]+) controls (\d+) comps (\d+)", r.stdout)
        self.assertIsNotNone(m, r.stdout)
        return {"n": int(m.group(1)), "p50": float(m.group(2)), "p95": float(m.group(3)),
                "max": float(m.group(4)), "controls": int(m.group(5)), "comps": int(m.group(6))}

    def test_01_cross_frame_seam_under_gate(self):
        """cross-frame seam p95 <= 3σ, max <= 5σ(预冻结门槛)。"""
        d = self._run()
        self.assertLessEqual(d["p95"], 3.0 * SIGMA,
                             f"seam p95={d['p95']} > 3σ={3*SIGMA} (接缝未消除)")
        self.assertLessEqual(d["max"], 5.0 * SIGMA,
                             f"seam max={d['max']} > 5σ={5*SIGMA}")

    def test_02_seam_statistics_sane(self):
        """seam 分布在有限范围, p50<p95<max(单调), 无异常。"""
        d = self._run()
        self.assertGreater(d["n"], 0, "应有重叠 seam 样本")
        self.assertLessEqual(d["p50"], d["p95"])
        self.assertLessEqual(d["p95"], d["max"])
        self.assertLess(d["max"], 0.5, f"seam max={d['max']} 应远小于星幅度 40")

    def test_03_upm_coverage_parameter_recovery(self):
        """UPM 参数恢复: control_count 达规模(>=256), component>=1, 模型可用。"""
        d = self._run()
        self.assertGreaterEqual(d["controls"], 256, f"control_count={d['controls']}")
        self.assertGreaterEqual(d["comps"], 1, f"component_count={d['comps']}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
