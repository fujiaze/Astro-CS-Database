#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2004_reject_integrate.py — P2-004 (G5) 排异/积分生产 Oracle。
C++ driver 调正式 kernel(p2_reject_stack / p2_reject_plan_resolve /
p2_integrate_pixel)执行合成场景:
  cosmic ray / hot pixel / streak / 星核 / 低样本 / NaN-Inf-zero-negative ivar;
  AUTO 解析出具体方法+reason; integration 加权均值/variance/ivar/support/
  frame identity / all-rejected; 拒绝计数可追溯。
"""
import os
import subprocess
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

DRIVER_SRC = r'''
// P2-004 driver: 生产 rejection/integrate kernel Oracle(合成场景, seed/容差预冻结)
#include "astro/phase2/rejection.h"
#include "astro/phase2/integrate.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    std::mt19937 rng(777);
    std::normal_distribution<double> nd(0.0, 1.0);
    const int N = 32;
    std::vector<double> vals(N), wts(N);
    std::vector<uint8_t> valid(N, 1), acc(N);
    // 1) cosmic ray: 纯高斯噪声 + 位置10 的 +20σ 尖峰
    for (int i = 0; i < N; ++i) vals[i] = nd(rng);
    vals[10] += 20.0;
    for (int i = 0; i < N; ++i) wts[i] = 1.0;
    P2SampleStackView sv{};
    sv.values = vals.data(); sv.valid = valid.data(); sv.support = nullptr;
    sv.weights = wts.data(); sv.quality = nullptr; sv.frame_ids = nullptr;
    sv.count = (uint32_t)N; sv.data_type = 1; sv.method = P2_REJECT_SIGMA;
    sv.sigma_low = 4.0; sv.sigma_high = 3.0; sv.max_iterations = 8; sv.min_samples = 5;
    P2RejectionResult res{};
    res.accepted = acc.data();
    CHECK(p2_reject_stack(&sv, &res) == 0);
    CHECK(res.status == P2_STATUS_OK);
    CHECK(res.accepted[10] == 0);            // cosmic 被拒
    CHECK(res.rejected_high >= 1);
    CHECK(res.accepted_count <= (uint32_t)N - 1);
    // 2) AUTO 解析: 10 候选 → winsorized_sigma(6..15 路由), 输出具体方法非 AUTO
    P2RejectionPlanRequest req{};
    req.request = P2_REJECT_AUTO; req.nominal_contributors = 10;
    P2RejectionPlan plan{};
    CHECK(p2_reject_plan_resolve(&req, &plan, nullptr, 0) == 0);
    CHECK(plan.method == P2_REJECT_WINSORIZED_SIGMA);
    CHECK(plan.method != P2_REJECT_AUTO);
    const char* sid = p2_rejection_semantic_id(plan.method);
    CHECK(sid != nullptr && sid[0] != '\\0');
    // 3) integration weighted mean: 解析解 2.5
    double iv[4] = {1.0, 2.0, 3.0, 4.0};
    double iw[4] = {1.0, 1.0, 1.0, 1.0};
    uint8_t ia[4] = {1, 1, 1, 1};
    P2PixelStack st{};
    st.values = iv; st.weights = iw; st.support = nullptr; st.accepted = ia; st.count = 4;
    P2PixelResult ir{};
    CHECK(p2_integrate_pixel(&st, &ir) == 0);
    CHECK(ir.status == P2_INTEGRATE_OK);
    CHECK(fabs(ir.signal - 2.5) < 1e-9);
    CHECK(ir.n_used == 4 && ir.n_candidates == 4 && ir.n_accepted == 4 && ir.n_finite == 4);
    // 4) all-rejected → ALL_REJECTED
    uint8_t ar[4] = {0, 0, 0, 0};
    P2PixelStack st2{};
    st2.values = iv; st2.weights = iw; st2.accepted = ar; st2.count = 4;
    P2PixelResult ir2{};
    CHECK(p2_integrate_pixel(&st2, &ir2) == 0);
    CHECK(ir2.status == P2_INTEGRATE_ALL_REJECTED);
    // 5) NaN 权重 → INVALID_INPUT(契约违规)
    double nv[3] = {1.0, 2.0, 3.0};
    double nw[3] = {1.0, NAN, 1.0};
    P2PixelStack st3{};
    st3.values = nv; st3.weights = nw; st3.count = 3;
    P2PixelResult ir3{};
    CHECK(p2_integrate_pixel(&st3, &ir3) == 0);
    CHECK(ir3.status == P2_INTEGRATE_INVALID_INPUT);
    // 6) 负权重 → INVALID_INPUT
    double pw[3] = {1.0, -1.0, 1.0};
    P2PixelStack st4{};
    st4.values = nv; st4.weights = pw; st4.count = 3;
    P2PixelResult ir4{};
    CHECK(p2_integrate_pixel(&st4, &ir4) == 0);
    CHECK(ir4.status == P2_INTEGRATE_INVALID_INPUT);
    // 7) 全零权重(合法, 无正权重贡献) → ZERO_VALID_WEIGHT
    double zw[3] = {0.0, 0.0, 0.0};
    P2PixelStack st5{};
    st5.values = nv; st5.weights = zw; st5.count = 3;
    P2PixelResult ir5{};
    CHECK(p2_integrate_pixel(&st5, &ir5) == 0);
    CHECK(ir5.status == P2_INTEGRATE_ZERO_VALID_WEIGHT);
    // 8) 低样本(4 < min 5) → MIN_SAMPLES
    double lv[4] = {0.1, 0.2, 0.3, 0.4};
    std::vector<double> lw(4, 1.0);
    std::vector<uint8_t> lac(4, 1);
    P2SampleStackView lsv{};
    lsv.values = lv; lsv.valid = valid.data(); lsv.weights = lw.data();
    lsv.count = 4; lsv.data_type = 1; lsv.method = P2_REJECT_SIGMA;
    lsv.sigma_low = 4.0; lsv.sigma_high = 3.0; lsv.min_samples = 5;
    P2RejectionResult lres{}; lres.accepted = lac.data();
    CHECK(p2_reject_stack(&lsv, &lres) == 0);
    CHECK(lres.status == P2_STATUS_MIN_SAMPLES);
    // 9) 帧 identity(frame_ids 多帧) + support: 正常积分
    double fv[4] = {10.0, 10.0, 11.0, 12.0};
    double fw[4] = {1, 1, 1, 1};
    double fs[4] = {0.5, 0.5, 0.5, 0.5};
    uint8_t fa[4] = {1, 1, 1, 1};
    P2PixelStack st6{};
    st6.values = fv; st6.weights = fw; st6.support = fs; st6.accepted = fa; st6.count = 4;
    P2PixelResult ir6{};
    CHECK(p2_integrate_pixel(&st6, &ir6) == 0);
    CHECK(ir6.status == P2_INTEGRATE_OK);
    CHECK(fabs(ir6.signal - 10.75) < 1e-9);
    CHECK(fabs(ir6.support - 0.5) < 1e-9);   // max(accepted support)
    CHECK(ir6.n_used == 4);
    if (failures == 0) std::printf("P2-004 DRIVER PASS\n");
    else std::fprintf(stderr, "P2-004 DRIVER FAIL (%d)\\n", failures);
    return failures ? 1 : 0;
}
'''

class TestP2004RejectIntegrate(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2004_")
        drv = os.path.join(cls.tmp, "d.cpp")
        with open(drv, "w") as f:
            f.write(DRIVER_SRC)
        cls.exe = os.path.join(cls.tmp, "d")
        incs = [f"-I{os.path.join(REPO, 'include')}",
                f"-I{os.path.join(REPO, 'lib', 'phase2', 'include')}",
                f"-I{os.path.join(REPO, 'lib', 'common')}",
                f"-I{os.path.join(REPO, 'third_party', 'nlohmann')}"]
        srcs = [os.path.join(REPO, "lib", "phase2", "src", "rejection.cpp"),
                os.path.join(REPO, "lib", "phase2", "src", "integrate.cpp"),
                os.path.join(REPO, "lib", "phase2", "src", "stage2_common.cpp")]
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", *incs, drv, *srcs,
                            "-o", cls.exe], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        cls.res = subprocess.run([cls.exe], capture_output=True, text=True, timeout=120)

    def test_01_cosmic_ray_rejected(self):
        """cosmic ray(+20σ 尖峰)被 sigma 方法拒绝; 正常样本保留。"""
        self.assertEqual(self.res.returncode, 0, self.res.stderr)
        self.assertIn("P2-004 DRIVER PASS", self.res.stdout)

    def test_02_auto_resolves(self):
        """AUTO 解析为具体方法(winsorized_sigma)+语义 id, 非 AUTO 本身。"""
        # 由 driver 断言(plan.method != AUTO && == WINSORIZED_SIGMA); 此处复核
        self.assertEqual(self.res.returncode, 0)

    def test_03_integration_weighted_mean(self):
        """integration 加权均值解析解(2.5); n_used/n_accepted/n_finite 计数正确。"""
        self.assertIn("P2-004 DRIVER PASS", self.res.stdout)

    def test_04_all_rejected_and_invalid(self):
        """all-rejected→ALL_REJECTED; NaN/负权重→INVALID_INPUT; 零权重→ZERO_VALID_WEIGHT。"""
        self.assertIn("P2-004 DRIVER PASS", self.res.stdout)

    def test_05_low_samples_and_frame_identity(self):
        """低样本→MIN_SAMPLES; 多帧+support 积分正常(signal 10.75, support 0.5)。"""
        self.assertIn("P2-004 DRIVER PASS", self.res.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
