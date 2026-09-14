// lib/phase2/tests/kcorr_lookup_test.cpp
//
// ALG-P2-SMP-001 §11.3 F2 回归锁（M4-C-01 / RQS-B3）：
//   kcorr_lookup 的 pixfrac 网格 {0.5,0.8,1.0} 是非均匀的；必须按真实网格
//   两段分段线性插值（0.5–0.8、0.8–1.0 各自归一），不得按 [0.5,1.0] 均匀
//   网格取列。F2 冻结容差：角点值 exact；插值点 rtol 1e-12。
//
// 通过 sampler.cpp 末尾的内部测试接缝驱动与生产完全相同的实现（同一 TU、
// 同一冻结表值），不复制逻辑。
#include <gtest/gtest.h>

#include <cmath>

extern "C" double astrocs_phase2_kcorr_lookup_for_test(double pixfrac,
                                                       double scale_arcsec);

namespace {

// 冻结表值（docs/algorithms/PHASE2_SAMPLER.md §11.3 F2 / sampler.cpp:91-94）
constexpr double kTab300[3] = {1.2112, 1.3925, 1.4980};
constexpr double kTab600[3] = {2.3958, 2.8971, 3.2035};
constexpr double kPf[3] = {0.5, 0.8, 1.0};
constexpr double kSc[2] = {300.0, 600.0};

double lookup(double pf, double sc) {
  return astrocs_phase2_kcorr_lookup_for_test(pf, sc);
}

}  // namespace

// F2：九个角点（pf × sc）必须精确等于表值（bitwise）。
TEST(kcorr, corner_exact) {
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(lookup(kPf[i], 300.0), kTab300[i])
        << "pf=" << kPf[i] << " sc=300";
    EXPECT_EQ(lookup(kPf[i], 600.0), kTab600[i])
        << "pf=" << kPf[i] << " sc=600";
  }
}

// F2：生产默认 pixfrac=0.8 必须命中表列（旧均匀网格实现返回表外值 +1.5%/+2.1%）。
TEST(kcorr, production_default_pixfrac_0p8) {
  EXPECT_EQ(lookup(0.8, 300.0), 1.3925);
  EXPECT_EQ(lookup(0.8, 600.0), 2.8971);
}

// F2：两段线性插值中点（独立硬编码参考，rtol 1e-12）。
TEST(kcorr, piecewise_midpoints_rtol_1e_12) {
  const double tol = 1e-12;
  EXPECT_NEAR(lookup(0.65, 300.0), (1.2112 + 1.3925) / 2.0, tol);
  EXPECT_NEAR(lookup(0.65, 600.0), (2.3958 + 2.8971) / 2.0, tol);
  EXPECT_NEAR(lookup(0.90, 300.0), (1.3925 + 1.4980) / 2.0, tol);
  EXPECT_NEAR(lookup(0.90, 600.0), (2.8971 + 3.2035) / 2.0, tol);
  // 行间（scale=450）线性中点。
  EXPECT_NEAR(lookup(0.65, 450.0),
              0.5 * ((1.2112 + 1.3925) / 2.0 + (2.3958 + 2.8971) / 2.0), tol);
}

// F2：域外 clamp（[0.5,1.0] × [300,600]）。
TEST(kcorr, domain_clamp) {
  EXPECT_EQ(lookup(0.2, 300.0), lookup(0.5, 300.0));
  EXPECT_EQ(lookup(2.0, 600.0), lookup(1.0, 600.0));
  EXPECT_EQ(lookup(0.8, 100.0), lookup(0.8, 300.0));
  EXPECT_EQ(lookup(0.8, 900.0), lookup(0.8, 600.0));
}
