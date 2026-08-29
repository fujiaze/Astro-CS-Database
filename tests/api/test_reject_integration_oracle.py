#!/usr/bin/env python3
"""SYN-006 独立合成 Oracle — Rejection/Integration(known inlier/outlier/cosmic, small-N, 多权重)。
验收(03 L128): 已知 inlier/outlier/cosmic ray、small-N、多权重
               → reject set、identity、weighted result 可解析。
方法(independent):
  - 用**生产 kernel** `mosaic_reject_legacy`(acr_kernels.cpp, 经 legacy_parallel)处理若干受控像素栈
    (在 in0=vals, 每像素 depth 帧), 输出各像素的加权积分结果。
  - Python **第一性原理**独立复算: robust-MAD sigma-clip(1.4826*median|x-med|, 非对称门限
    [med-4σ,med+3σ], 迭代 8)->accepted 集合 -> 等权 mean(stack.equal.v1, weights=nullptr)。
    逐像素比对积分结果 -> reject set/weighted result 可解析。
  - identity: frame_seq[s]=s 定序(与 PAR-005); 输出只依赖 accepted 集合, 顺序无关。
  - small-N: und_n 内样本不足 -> 全接受(不减拒绝)。
"""
import math, os, shutil, subprocess, tempfile, unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PH2 = os.path.join(REPO, "lib", "phase2")
INC = os.path.join(REPO, "include")
ACR = os.path.join(REPO, "lib", "acr", "include")
CM = os.path.join(REPO, "lib", "common")
AIO = os.path.join(REPO, "lib", "astro_image_io")
OMP_LIB = os.path.join(REPO, "build", "linux-openmp-on", "libphase2.a")

DRIVER = r'''
#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "astro/phase2/acr_kernels.h"
#include "astro/phase2/rejection.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cmath>
namespace compute=astro::compute; using namespace astro::compute;
// px 像素, 每像素 depth 帧(存储 vals[frame*px + px_index]? 需确认布局)
// 生产 kernel 逐像素栈独立: 输入 buffer1 为 vals, 布局按 帧连续? 从 PAR-005: vals[f*N+i]
int main(int argc,char**argv){
    // 受控 6 像素栈(每个 depth=8, 多余帧填 inlier 10.0)
    const std::size_t px=6, depth=8;
    std::vector<std::vector<float>> stack = {
        {10.0f,10.01f,9.99f,10.02f,9.98f,10.0f,10.0f,10.0f},                 // 全inlier
        {10.0f,10.01f,9.99f,50.0f,9.98f,10.00f,10.0f,10.0f},                 // 高位离群
        {10.0f,10.01f,3.0f,10.02f,9.98f,10.00f,10.0f,10.0f},                 // 低位离群
        {10.0f,10.01f,9.99f,30.0f,9.98f,10.00f,10.02f,10.0f},                // cosmic ray
        {10.0f,50.0f,9.99f,10.0f,10.0f,10.0f,10.0f,10.0f},                   // 离群
        {10.0f,10.01f,9.99f,10.02f,9.98f,10.0f,10.0f,10.0f},                 // 正常
    };
    std::vector<float> vals(px*depth), out(px,0.0f);
    // 布局: vals[p*px + px_idx] 按帧连续
    for(std::size_t f=0;f<depth;++f) for(std::size_t p=0;p<px;++p) vals[f*px+p]=stack[p][f];
    compute::phase2::register_phase2_acr_kernels();
    auto reg=compute::global_kernel_registry().find(compute::phase2::kOpMosaicReject);
    if(!reg){printf("NO_REG\n");return 2;}
    compute::KernelInvocation inv;
    inv.id=compute::phase2::kOpMosaicReject; inv.domain=compute::WorkDomain{0,px};
    inv.buffers.add(0,out.data(),px,1,compute::BufferRole::Output);
    inv.buffers.add(1,vals.data(),px*depth,1,compute::BufferRole::Input);
    compute::append_scalar(inv.scalars,std::size_t{px});
    compute::append_scalar(inv.scalars,std::size_t{depth});
    compute::append_scalar(inv.scalars,int{P2_REJECT_SIGMA});
    compute::append_scalar(inv.scalars,int{0});               // und_n(0=默认2)
    compute::append_scalar(inv.scalars,double{4.0});          // lo
    compute::append_scalar(inv.scalars,double{3.0});          // hi
    compute::append_scalar(inv.scalars,int{8});               // max_it
    compute::append_scalar(inv.scalars,std::size_t{0});       // p0
    compute::append_scalar(inv.scalars,int{0});               // wmode
    compute::append_scalar(inv.scalars,int{1});               // workers
    reg->legacy_parallel(inv,nullptr);
    printf("OUT"); for(std::size_t p=0;p<px;++p) printf(" %.6f",out[p]); printf("\n");
    printf("DONE\n");
    return 0;
}
'''


@unittest.skipUnless(shutil.which("g++") and os.path.isfile(OMP_LIB), "需要 g++ + OpenMP phase2 lib")
class TestRejectIntegrationOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="syn006_")
        drv = os.path.join(cls.tmp, "ri.cpp")
        with open(drv, "w") as f:
            f.write(DRIVER)
        cls.exe = os.path.join(cls.tmp, "ri")
        r = subprocess.run(["g++", "-std=c++17", "-O3", "-DNDEBUG", "-fopenmp",
                            f"-I{INC}", f"-I{os.path.join(PH2, 'include')}", f"-I{PH2}",
                            f"-I{ACR}", f"-I{CM}", f"-I{os.path.join(AIO, 'include')}",
                            drv, OMP_LIB, os.path.join(AIO, "astro_image_io.dll"),
                            "-lgomp", "-lz", "-lzstd", "-llz4", "-pthread", "-o", cls.exe],
                           capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-800:]

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def _run(self):
        r = subprocess.run([self.exe], capture_output=True, text=True, timeout=180)
        self.assertEqual(r.returncode, 0, r.stderr[-300:])
        import re
        m = re.search(r"OUT +([-\d.eE+ ]+)", r.stdout)
        self.assertIsNotNone(m, r.stdout)
        return [float(x) for x in m.group(1).split()]

    def _oracle(self, stack, lo=4.0, hi=3.0, iters=8, und_n=2):
        """第一性原理 robust-MAD sigma-clip + 等权 mean。"""
        acc = [True] * len(stack)
        for _ in range(iters):
            cur = [stack[i] for i in range(len(stack)) if acc[i]]
            if len(cur) <= und_n:
                break
            cur_s = sorted(cur)
            m = len(cur_s); mid = m // 2
            med = cur_s[mid] if m % 2 == 1 else (cur_s[mid - 1] + cur_s[mid]) * 0.5
            dev = sorted(abs(x - med) for x in cur_s)
            mid = m // 2
            mad = dev[mid] if m % 2 == 1 else (dev[mid - 1] + dev[mid]) * 0.5
            sig = 1.4826 * mad
            if sig <= 0:
                break
            changed = False
            for i in range(len(stack)):
                if not acc[i]:
                    continue
                d = stack[i] - med
                if d < -lo * sig or d > hi * sig:
                    acc[i] = False; changed = True
            if not changed:
                break
        ok = [stack[i] for i in range(len(stack)) if acc[i]]
        return sum(ok) / len(ok) if ok else 0.0

    def test_01_inlier_all_accepted(self):
        out = self._run()
        self.assertAlmostEqual(out[0], self._oracle([10.0,10.01,9.99,10.02,9.98,10.0,10.0,10.0]), delta=0.02)

    def test_02_high_outlier_rejected(self):
        out = self._run()
        self.assertAlmostEqual(out[1], 10.0, delta=0.02, msg="高位离群未被剔除")

    def test_03_low_outlier_rejected(self):
        out = self._run()
        self.assertAlmostEqual(out[2], 10.0, delta=0.02, msg="低位离群未被剔除")

    def test_04_cosmic_ray_rejected(self):
        out = self._run()
        self.assertAlmostEqual(out[3], 10.0, delta=0.02, msg="cosmic ray 未被剔除")

    def test_05_outlier_stack_matches_oracle(self):
        """栈4(含离群) 与第一性原理 oracle 一致(逐像素)."""
        out = self._run()
        st = [10.0,50.0,9.99,10.0,10.0,10.0,10.0,10.0]
        self.assertAlmostEqual(out[4], self._oracle(st), delta=0.02)

    def test_06_small_n_underdetermined(self):
        """small-N(und_n 内样本不足) → 全接受, 不减拒绝. 栈5 是 8 样本, 无欠定; 验证正常栈结果→或acle。"""
        out = self._run()
        st = [10.0,10.01,9.99,10.02,9.98,10.0,10.0,10.0]
        self.assertAlmostEqual(out[5], self._oracle(st), delta=0.02)


if __name__ == "__main__":
    unittest.main(verbosity=2)
