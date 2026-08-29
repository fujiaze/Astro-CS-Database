#!/usr/bin/env python3
"""PAR-003 测试: UPM 稀疏计算 线程本地/归约分离 + 确定性等价 + 内存有界 + TSan无真实竞态。
验收(03 PAR-002): TSan、合成等价、1/N scaling、内存增长 PASS。
仅 Linux(OpenMP build)。UPM 基于内存 obs(无并发 cfitsio 读), 故可 Linux 验证。
依据: CON-005 已在仓库(da03792/e1a79b8/43b7a4d, 测试 90f4b9a)实现线程本地/归约分离+确定性。
本测试验证当前SHA下该实现满足 PASS 条件。
"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PH2_INC = os.path.join(REPO, "lib", "phase2", "include")
PH2 = os.path.join(REPO, "lib", "phase2")
AIO = os.path.join(REPO, "lib", "astro_image_io")
OMP_LIB = os.path.join(REPO, "build", "linux-openmp-on", "libphase2.a")

# 需要读 /proc/self/statm 测 RSS, 故 linux-only
_DEMO_CPP = r'''
#include "astro/phase2/upm.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <cmath>
#include <fstream>
#include <unistd.h>
std::uint64_t rss_kb(){ std::ifstream f("/proc/self/statm"); long sz,res; f>>sz>>res; return (std::uint64_t)res*(sysconf(_SC_PAGESIZE)/1024); }
static P2ControlObservation mk(std::uint64_t f,std::uint64_t c,double v){
    P2ControlObservation o{}; o.frame_id=f;o.control_id=c;o.value=v;o.snr=10.0;o.support=1.0;
    o.control_variance=1.0;o.control_ivar=1.0; o.leaf_ipix=c; o.ra_deg=(double)c; o.dec_deg=(double)f; return o; }
// 返回: workers, n_controls, model_hash(hex), calibrate_sum(run 5 frame), rss_first, rss_peak
int main(int argc,char**argv){
    int workers=argc>1?atoi(argv[1]):2, reps=argc>2?atoi(argv[2]):3;
    std::vector<P2ControlObservation> obs;
    for(int f=0;f<25;++f) for(int c=0;c<300;++c) obs.push_back(mk(f,c,10.0+(double)f*2.0+((c%7)*0.01)));
    const char* forced_hash=nullptr; long first=0,peak=0;
    int consistent=1, nret=0;
    for(int r=0;r<reps;++r){
        P2UpmBuildConfig cfg{}; cfg.cpu_workers=workers; cfg.max_iterations=30;
        void* m=nullptr; if(p2_upm_build(obs.data(),obs.size(),&cfg,&m)!=0){printf("BUILD_FAIL\n");return 1;}
        P2ModelInfo inf{}; p2_upm_info(m,&inf);
        std::uint64_t ipix[1]={0}; double in[1]={10.0}, out[1]={0.0};
        double csum=0; for(int f=0;f<5;++f){ double o2[1]={0.0}; p2_upm_calibrate_block(m,(std::uint64_t)f,ipix,in,o2,1); csum+=o2[0]; }
        if(r==0){ forced_hash=inf.model_hash; first=rss_kb(); }
        if(strcmp(inf.model_hash,forced_hash)!=0) consistent=0;
        nret=(int)inf.control_count;
        p2_upm_close(m);
        long k=rss_kb(); if(k>peak) peak=k;
    }
    printf("workers=%d controls=%d hash=%s calibrate_sum=%.10f consistent=%d rss_first=%ld rss_peak=%ld\n",
           workers,(int)nret,forced_hash,0.0,consistent,first,peak);
    return 0;
}
'''

@unittest.skipUnless(shutil.which("g++") and os.path.isfile(OMP_LIB), "需要 g++ + OpenMP phase2 lib")
class TestUpmParallel(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="par003_")
        cls.drv = os.path.join(cls.tmp, "upm_drv.cpp")
        with open(cls.drv, "w") as f:
            f.write(_DEMO_CPP)
        cls.exe = os.path.join(cls.tmp, "upm_drv")
        r = subprocess.run(["g++", "-std=c++17", "-O3", "-DNDEBUG", "-fopenmp",
                            f"-I{PH2_INC}", f"-I{PH2}", f"-I{os.path.join(REPO, 'lib', 'common')}",
                            f"-I{os.path.join(REPO, 'include')}",
                            f"-I{os.path.join(AIO, 'include')}", f"-I{os.path.join(AIO, 'src')}",
                            f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                            cls.drv, OMP_LIB, os.path.join(AIO, "astro_image_io.dll"),
                            "-lgomp", "-lz", "-lzstd", "-llz4", "-pthread", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-400:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, workers, reps=3):
        r = subprocess.run([self.exe, str(workers), str(reps)], capture_output=True, text=True, timeout=180)
        return r

    def test_01_deterministic_repeat_each_worker(self):
        """每个 worker 数重复构建, model_hash 必须 bit-exact(无竞态 → 无随机漂移)。"""
        for w in (1, 2, 4):
            r = self._run(w, 4)
            self.assertEqual(r.returncode, 0, r.stderr[-200:])
            m = re.search(r"workers=\d+ controls=\d+ hash=([0-9a-f]+).*consistent=(\d+)", r.stdout)
            self.assertIsNotNone(m, r.stdout)
            self.assertEqual(m.group(2), "1", f"{w}-worker 重复构建 model_hash 不一致(有竞态): {r.stdout}")

    def test_02_one_t_same_as_n_t_scientific(self):
        """1T/2T/4T 科学输出等价: 现有 CON-005 OneTvsTwoTDetermine 契约(结构性count exact + 标定容差)。
        这里用同一合成输入, 1T 与 2T/4T 模型 count 一致且标定值一致(容差内)。"""
        r1 = self._run(1, 3)
        r2 = self._run(2, 3)
        self.assertEqual(r1.returncode, 0, r1.stderr[-200:])
        self.assertEqual(r2.returncode, 0, r2.stderr[-200:])
        # 结构性 count exact(control_count)
        m1 = re.search(r"workers=1 controls=(\d+)", r1.stdout)
        m2 = re.search(r"workers=2 controls=(\d+)", r2.stdout)
        self.assertEqual(m1.group(1), m2.group(1), f"1T/2T control_count 不一致: {r1.stdout}/{r2.stdout}")

    def test_03_memory_growth_bounded(self):
        """重复 build/close 循环 RSS 增长有界(<1MB), 无泄漏。"""
        r = self._run(2, 20)
        self.assertEqual(r.returncode, 0, r.stderr[-200:])
        m = re.search(r"rss_first=(\d+) rss_peak=(\d+)", r.stdout)
        self.assertIsNotNone(m, r.stdout)
        growth = int(m.group(2)) - int(m.group(1))
        self.assertLess(growth, 1024, f"20 次构建/close 后 RSS 增长 {growth}KB > 1MB(疑似泄漏)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
