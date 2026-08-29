#!/usr/bin/env python3
"""MON-001 测试: 进程/系统资源监控模块 — /proc 采集(CPU/RSS/thread/ctxsw/IO)+单调时间+采样开销测量。
仅 Linux(vm-bj)。验收: 采集指标与 OS 工具误差在冻结范围; 监控自身开销达标(07 §1 采样开销测量)。"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")

MON_DRIVER = r"""
#include "monitor.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>
int main(int argc, char** argv){
    bool burn = (argc > 1 && std::string(argv[1]) == "burn");
    double secs = (argc > 2) ? std::atof(argv[2]) : 1.0;
    std::atomic<bool> stop{false};
    std::vector<std::thread> ths;
    if (burn) {
        for (int t = 0; t < 2; ++t)
            ths.emplace_back([&stop](){
                volatile double x = 1.0;
                while (!stop.load()) { for (int i=0;i<2000;++i) x = std::sin(x)+x*0.001; }
            });
    }
    astrocs::ProcessMonitor mon(0.05);
    mon.run_for(secs);
    stop.store(true);
    for (auto& th : ths) th.join();
    auto s = mon.summary();
    std::printf("n_samples=%llu wall=%.3f\n", (unsigned long long)s.n_samples, s.wall_seconds);
    std::printf("avg_eq=%.3f peak_eq=%.3f avg_cpu_pct=%.1f\n",
                s.avg_equivalent_cores, s.peak_equivalent_cores, s.avg_cpu_percent);
    std::printf("peak_rss=%llu max_threads=%u ctx=%llu rd=%llu wr=%llu slope=%lld\n",
                (unsigned long long)s.peak_rss_bytes, s.max_threads,
                (unsigned long long)s.total_ctx_switches,
                (unsigned long long)s.total_read_bytes,
                (unsigned long long)s.total_write_bytes,
                (long long)s.rss_slope_bytes_per_s);
    std::printf("overhead_ms=%.4f\n", s.sample_overhead_ms);
    return 0;
}
"""


@unittest.skipUnless(os.path.isdir("/proc") and shutil.which("g++"), "需要 Linux + g++")
class TestMonitor(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="mon001_")
        src = os.path.join(cls.tmp, "mon_driver.cpp")
        with open(src, "w") as f:
            f.write(MON_DRIVER)
        cls.exe = os.path.join(cls.tmp, "mon_driver")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-pthread",
                            f"-I{CLI}", f"-I{os.path.join(REPO, 'third_party')}",
                            src, "-o", cls.exe], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _parse(self, out):
        d = {}
        for token in out.split():
            m = re.match(r"^(\w[\w_]*)=(\S+)$", token)
            if m and re.match(r"^-?[\d.]+$", m.group(2)):
                d[m.group(1)] = float(m.group(2))
        return d

    def test_01_idle_monotonic_and_metrics_present(self):
        """空载监控: 采样数>0, 墙钟≈时长, RSS>0(采集有效), 开销低。"""
        r = subprocess.run([self.exe, "idle", "1.0"], capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        d = self._parse(r.stdout)
        self.assertGreater(d["n_samples"], 0, "必须采样")
        self.assertAlmostEqual(d["wall"], 1.0, delta=0.2)
        self.assertGreater(d["peak_rss"], 0, "RSS 必采且 >0")
        self.assertLess(d["overhead_ms"], 2.0, "每样本采样开销须 <2ms(07 §1)")
        self.assertGreaterEqual(d["n_samples"], 10, "0.05s 采样 1s 至少 10 样本")

    def test_02_cpu_equivalence_detects_burn(self):
        """2 线程烧 CPU: peak_eq_cores 应达到 ~2(真实等价核检测, 非恒 0)。"""
        r = subprocess.run([self.exe, "burn", "1.0"], capture_output=True, text=True, timeout=120)
        self.assertEqual(r.returncode, 0, r.stderr)
        d = self._parse(r.stdout)
        self.assertGreater(d["peak_eq"], 1.0, f"烧2线程须测到峰值等价核>1, 得 {d['peak_eq']}")
        # 不等价于硬编码; 只验证"检测到多核利用"这一事实

    def test_03_rss_threads_ctxsw_reported(self):
        """RSS/thread/ctxsw 必须真实上报(非恒 0 的占位)。"""
        r = subprocess.run([self.exe, "idle", "0.5"], capture_output=True, text=True, timeout=30)
        d = self._parse(r.stdout)
        self.assertGreater(d["peak_rss"], 0)
        self.assertGreater(d["max_threads"], 0)
        self.assertGreater(d["ctx"], 0, "进程至少有些上下文切换")

    def test_04_overhead_frozen_budget(self):
        """采样开销(07 §1)须 < 5ms/样本(对 0.05s 间隔<10% 预算)。"""
        r = subprocess.run([self.exe, "idle", "1.0"], capture_output=True, text=True, timeout=30)
        d = self._parse(r.stdout)
        self.assertLess(d["overhead_ms"], 5.0,
                        f"采样开销须 <5ms(被测 {d['overhead_ms']:.2f}ms)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
