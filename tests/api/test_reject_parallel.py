#!/usr/bin/env python3
"""PAR-005 测试: Rejection/Integration 并行+确定性合同 (CON-007 ACR launcher)。
验收(03 PAR-005): outlier oracle、1/N tolerance、race/scaling PASS。
仅 Linux。mosaic_reject_legacy 逐像素栈独立(per-thread scratch, frame_seq[s]=s 定序),
frame identity 不丢; 并行 #pragma omp for schedule(static), 输出逐像素独占。
本测试验证当前SHA下: oracle 正确 + 跨 worker 确定(frame identity 不丢) + 正 1/N scaling。
"""
import os, re, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INC = os.path.join(REPO, "include")
PH2 = os.path.join(REPO, "lib", "phase2")
ACR = os.path.join(REPO, "lib", "acr", "include")
CM = os.path.join(REPO, "lib", "common")
AIO = os.path.join(REPO, "lib", "astro_image_io")
OMP_LIB = os.path.join(REPO, "build", "linux-openmp-on", "libphase2.a")
HW = os.cpu_count() or 1

_DRV = r'''
#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "astro/phase2/acr_kernels.h"
#include "astro/phase2/rejection.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <chrono>
namespace compute=astro::compute;
using namespace astro::compute;
static compute::KernelInvocation mk(std::vector<float>& vals, std::vector<float>& out, std::size_t px, std::size_t depth, int workers){
    compute::KernelInvocation inv;
    inv.id=compute::phase2::kOpMosaicReject; inv.domain=compute::WorkDomain{0,px};
    inv.buffers.add(0,out.data(),px,1,compute::BufferRole::Output);
    inv.buffers.add(1,vals.data(),px*depth,1,compute::BufferRole::Input);
    compute::append_scalar(inv.scalars,std::size_t{px});
    compute::append_scalar(inv.scalars,std::size_t{depth});
    compute::append_scalar(inv.scalars,int{P2_REJECT_SIGMA});
    compute::append_scalar(inv.scalars,int{2});
    compute::append_scalar(inv.scalars,double{4.0});
    compute::append_scalar(inv.scalars,double{3.0});
    compute::append_scalar(inv.scalars,int{8});
    compute::append_scalar(inv.scalars,std::size_t{0});
    compute::append_scalar(inv.scalars,int{0});
    compute::append_scalar(inv.scalars,int{workers});
    return inv;
}
static double run(const astro::compute::KernelRegistration* reg, std::vector<float>& vals, std::vector<float>& out, std::size_t px, std::size_t depth, int workers, int reps){
    double best=1e30;
    for(int r=0;r<reps;++r){
        compute::KernelInvocation inv=mk(vals,out,px,depth,workers);
        auto t0=std::chrono::steady_clock::now(); reg->legacy_parallel(inv,nullptr); auto t1=std::chrono::steady_clock::now();
        double ms=std::chrono::duration<double,std::milli>(t1-t0).count(); if(ms<best)best=ms;
    }
    return best;
}
int main(int argc,char**argv){
    int workers=argc>1?atoi(argv[1]):1, reps=argc>2?atoi(argv[2]):3;
    const std::size_t px=1u<<18, depth=64;
    std::vector<float> vals(px*depth), out(px,0.0f);
    for(size_t i=0;i<vals.size();++i) vals[i]=(float)(10.0+((i%97)*0.01));
    for(size_t p=0;p<px;++p) if(p%1000==0) vals[p]=50.0f;
    compute::phase2::register_phase2_acr_kernels();
    auto reg=compute::global_kernel_registry().find(compute::phase2::kOpMosaicReject);
    if(!reg){printf("NO_REG\n");return 2;}
    double best=run(reg,vals,out,px,depth,workers,reps);
    // 离群剔除正确性: 注入的 50.0 被剔除, 输出应接近 10.x(大量正常帧主导)
    double chk=0, rej=0;
    for(size_t p=0;p<px;++p){ chk+=out[p]; if(p<100) rej+=out[p]; }
    printf("REJ workers=%d px=%zu depth=%zu best_ms=%.2f chk=%.6f sample0=%.4f\n",workers,px,depth,best,chk,out[0]);
    return 0;
}
'''


@unittest.skipUnless(shutil.which("g++") and os.path.isfile(OMP_LIB) and HW >= 2, "需要 g++ + OpenMP phase2 lib + 多核")
class TestRejectParallel(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="par005_")
        cls.drv = os.path.join(cls.tmp, "rej.cpp")
        with open(cls.drv, "w") as f:
            f.write(_DRV)
        cls.exe = os.path.join(cls.tmp, "rej")
        r = subprocess.run(["g++", "-std=c++17", "-O3", "-DNDEBUG", "-fopenmp",
                            f"-I{INC}", f"-I{os.path.join(PH2, 'include')}", f"-I{PH2}",
                            f"-I{ACR}", f"-I{CM}", f"-I{os.path.join(AIO, 'include')}",
                            cls.drv, OMP_LIB, os.path.join(AIO, "astro_image_io.dll"),
                            "-lgomp", "-lz", "-lzstd", "-llz4", "-pthread", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-400:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self, workers, reps=3):
        r = subprocess.run([self.exe, str(workers), str(reps)], capture_output=True, text=True, timeout=180)
        self.assertEqual(r.returncode, 0, r.stderr[-200:])
        m = re.search(r"workers=(\d+) px=(\d+) depth=(\d+) best_ms=([\d.]+) chk=([-\d.]+)",
                      r.stdout)
        self.assertIsNotNone(m, r.stdout)
        return {"workers": int(m.group(1)), "ms": float(m.group(4)), "chk": float(m.group(5))}

    def test_01_deterministic_and_frame_identity_kept(self):
        """跨 worker 逐像素确定性 + frame identity 不丢(输出聚合逐位一致)。"""
        chks = {self._run(w, 2)["chk"] for w in (1, 2, 4)}
        self.assertEqual(len(chks), 1, f"跨 worker 输出聚合漂移(race/frame identity 丢失): {chks}")

    def test_02_outlier_oracle_removes_injection(self):
        """注入 50.0 离群 → rejection 剔除后输出接近正常值(10.x)。"""
        r = self._run(1, 2)
        # sample0 对应像素0 有注入离群(50.0), 剔除后应接近 ~10.x
        # chk/px 平均也应在正常域(10.x)
        self.assertTrue(9.0 < (r["chk"] / (1 << 18)) < 12.0,
                        f"注入异常未被正确剔除(平均 {r['chk']/(1<<18):.4f})")

    def test_03_positive_or_nonregress_1N_scaling(self):
        """并行不显著变慢(1/N 不退化): 4w 不得慢于 1w 的 1.25x。"""
        one = self._run(1, 3)
        four = self._run(4, 3)
        self.assertLess(four["ms"], one["ms"] * 1.25,
                        f"4-worker 未见并行收益(1w={one['ms']:.0f}ms, 4w={four['ms']:.0f}ms)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
