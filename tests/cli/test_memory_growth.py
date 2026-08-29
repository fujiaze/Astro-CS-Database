#!/usr/bin/env python3
"""MON-004 测试: 20 次循环/预热剔除/稳健斜率/峰值/retained bytes/OOM 预警。
验收(07 §5): 注入泄漏被抓; 稳定 cache 不误判; 报告含曲线摘要。"""
import os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.path.join(REPO, "cli")


@unittest.skipUnless(shutil.which("g++"), "需要 g++")
class TestMemoryGrowth(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="mon004_")
        src = os.path.join(cls.tmp, "mg.cpp")
        with open(src, "w") as f:
            f.write(r'''
#include "memory_growth.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace astrocs;
static void show(const char* name, const MemAnalysisResult& r){
    std::printf("%s=%s slope=%.2f peak=%llu growth=%llu pct=%.1f n=%zu msg=%s\n",
        name, mem_diag_name(r.diag), r.robust_slope_bytes_per_iter,
        (unsigned long long)r.peak_bytes, (unsigned long long)r.retained_growth_bytes,
        r.relative_growth_pct, r.n_analyzed, r.detail.c_str());
}
int main(int argc, char** argv){
    const std::string c = (argc>1)?argv[1]:"";
    MemAnalysisConfig cfg; cfg.warmup=3; cfg.leak_slope_bytes_per_iter=1024.0;
    cfg.oom_frac=0.85; cfg.mem_limit_bytes=1024*1024*1024;  // 1GB 上限
    if (c=="leak"){   // 每轮 +64KB, 无界泄漏
        std::vector<uint64_t> rss; uint64_t base=1000000;
        for(int i=0;i<20;++i){ rss.push_back(base + i*65536); }
        show("r", analyze_memory_growth(rss,cfg));
    } else if (c=="stable-cache"){  // 稳定 cache: 首 warmup 涨, 此后持平
        std::vector<uint64_t> rss; uint64_t base=1000000;
        for(int i=0;i<20;++i){ rss.push_back(i<4 ? base+i*2000000 : base+4*2000000); }
        show("r", analyze_memory_growth(rss,cfg));
    } else if (c=="oscillating"){  // 峰谷震荡, 无净增长
        std::vector<uint64_t> rss; uint64_t base=1000000;
        for(int i=0;i<20;++i){ rss.push_back(base + (i%2?500000:0)); }
        show("r", analyze_memory_growth(rss,cfg));
    } else if (c=="flat"){  // 完全平稳
        std::vector<uint64_t> rss(20, 1000000);
        show("r", analyze_memory_growth(rss,cfg));
    } else if (c=="oom"){  // 峰值逼近 1GB 上限(>85%)
        std::vector<uint64_t> rss; uint64_t base=900*1024*1024;
        for(int i=0;i<20;++i){ rss.push_back(base + i*1024*1024); }
        show("r", analyze_memory_growth(rss,cfg));
    } else if (c=="growing-mild"){  // 每轮 +512B(低于阈值 1024B 但正增长)
        std::vector<uint64_t> rss; uint64_t base=1000000;
        for(int i=0;i<20;++i){ rss.push_back(base + i*512); }
        show("r", analyze_memory_growth(rss,cfg));
    } else { std::printf("unknown-case\n"); return 2; }
    return 0;
}
''')
        cls.exe = os.path.join(cls.tmp, "mg")
        r = subprocess.run(["g++", "-std=c++17", "-O2", f"-I{CLI}",
                            f"-I{os.path.join(REPO, 'third_party')}",
                            src, "-o", cls.exe], capture_output=True, text=True, timeout=180)
        assert r.returncode == 0, r.stderr

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, case):
        o = subprocess.run([self.exe, case], capture_output=True, text=True, timeout=30).stdout
        d = {}
        for line in o.strip().splitlines():
            for tok in line.split():
                if "=" in tok:
                    k, v = tok.split("=", 1)
                    d[k] = v
        # diag 在首个 = 字段(见 show: name=diag ...)
        d["diag"] = d.get("r", "").split(" ")[0] if " " in d.get("r", "") else d.get("r", "")
        return d

    def test_01_injected_leak_detected(self):
        """注入线性泄漏(每轮+64KB) → leak。"""
        d = self._run("leak")
        self.assertEqual(d["diag"], "leak", d)
        self.assertGreater(float(d["slope"]), 0)

    def test_02_stable_cache_not_misclassified(self):
        """稳定 cache(warmup 后持平) → 不误判为 leak。"""
        d = self._run("stable-cache")
        self.assertNotEqual(d["diag"], "leak", d)
        self.assertEqual(d["diag"], "stable", d)

    def test_03_oscillating_not_leak(self):
        """峰谷震荡(每轮释放, 无净增长) → oscillating, 非 leak。"""
        d = self._run("oscillating")
        self.assertEqual(d["diag"], "oscillating", d)

    def test_04_flat_stable(self):
        """完全平稳 → stable。"""
        d = self._run("flat")
        self.assertEqual(d["diag"], "stable", d)

    def test_05_oom_prewarning(self):
        """峰值逼近 85% 上限 → oom_pre_warning。"""
        d = self._run("oom")
        self.assertEqual(d["diag"], "oom_pre_warning", d)

    def test_06_mild_growth_prewarning(self):
        """正增长但低于泄漏阈值 → growing(早期预警)。"""
        d = self._run("growing-mild")
        self.assertEqual(d["diag"], "growing", d)

    def test_07_report_has_curve_summary(self):
        """报告含曲线摘要(peak/growth/n_analyzed/slope)。"""
        d = self._run("leak")
        for k in ("peak", "growth", "n", "slope", "msg"):
            self.assertIn(k, d, f"曲线摘要缺 {k}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
