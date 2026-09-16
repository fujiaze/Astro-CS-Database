// P1-005 单元测试: Noise/SNR SCI 公式 + Monte Carlo + 边界
#include "noise_model.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using astrocs::phase1::NoiseModel;
using astrocs::phase1::NoiseResult;
using astrocs::phase1::kMadToSigma;
using astrocs::phase1::kVarianceFloor;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

static void test_blank_sky() {
  // blank sky: 已知 σ=10 白噪声 → 恢复 variance≈100
  std::mt19937 rng(42);  // 固定 seed
  std::normal_distribution<float> dist(100.0f, 10.0f);
  std::vector<float> px(4096);
  for (auto& v : px) v = dist(rng);
  NoiseModel m;
  auto r = m.estimate(px);
  CHECK(r.ok());
  if (r.ok()) {
    NoiseResult& n = r.value();
    CHECK(n.valid);
    CHECK(std::fabs(n.variance - 100.0) < 15.0);   // 采样误差
    CHECK(std::fabs(n.ivar - 1.0 / n.variance) < 1e-12);  // ivar=1/variance 不混
    CHECK(std::fabs(n.background - 100.0) < 2.0);
    CHECK(std::fabs(n.sigma - 10.0) < 1.0);
  }
}

static void test_monte_carlo_poisson() {
  // Poisson+read noise Monte Carlo (固定 seed): 解析均值/方差对照
  // 模型 (SCI-NOISE-001 §5:58; gain 为 e-/ADU):
  //   N_e ~ N(signal·gain, signal·gain)     (Poisson 大均值正态近似)
  //   ADU = N_e/gain + N(0, read_noise_e/gain)
  //   E[x] = signal [ADU];  Var(x) = signal/gain + (read_noise_e/gain)² [ADU²]
  // M3-A-006 订正: 原 fixture 写 ADU = N_e·gain (增益方向与 SCI 相反),
  // 其"解析真值"随之变成 signal·gain + rn², 与 SCI §5:58 相差 gain²=2.25 倍。
  const double signal = 500.0, gain = 1.5, read_noise = 3.0;
  const std::size_t n_pix = 65536;  // MAD 的 SE≈1.166/√n=0.46% ⇒ 5% 门 ≫5σ
  std::mt19937 rng(7);
  std::vector<float> px;
  px.reserve(n_pix);
  const double mean_e = signal * gain;  // 电子数均值 [e-]
  std::normal_distribution<double> pdist(mean_e, std::sqrt(mean_e));
  std::normal_distribution<double> rdist(0.0, read_noise / gain);  // [ADU]
  for (std::size_t i = 0; i < n_pix; ++i) {
    const double electrons = pdist(rng);
    const double adus = electrons / gain + rdist(rng);
    px.push_back(static_cast<float>(adus));
  }
  NoiseModel m;
  auto r = m.estimate(px);
  CHECK(r.ok());
  if (r.ok()) {
    // 解析: variance = signal/gain + (read_noise_e/gain)²  [ADU²] (SCI §5:58)
    const double analytic_var =
        signal / gain + (read_noise / gain) * (read_noise / gain);
    CHECK(std::fabs(r.value().variance - analytic_var) / analytic_var < 0.05);
    // 生产诊断式必须与 SCI 解析式一致 (原测试从未调用它 ⇒ 抓不住方向反转)
    auto gv = m.gain_variance(signal, gain, read_noise);
    CHECK(gv.ok() && gv.value().valid);
    CHECK(std::fabs(gv.value().variance - analytic_var) < 1e-9 * analytic_var);
  }
}

static void test_low_high_signal() {
  // 低信号 (近零方差): clamp 到 floor; 高信号: 方差大
  NoiseModel m;
  std::vector<float> low(512, 100.0f);       // 全相同 → 方差 0 → clamp floor
  auto rl = m.estimate(low);
  CHECK(rl.ok() && rl.value().valid);
  CHECK(rl.value().variance == kVarianceFloor);
  CHECK(rl.value().ivar == 1.0 / kVarianceFloor);

  std::mt19937 rng(1);
  std::normal_distribution<float> dist(1000.0f, 50.0f);
  std::vector<float> high(4096);
  for (auto& v : high) v = dist(rng);
  auto rh = m.estimate(high);
  CHECK(rh.ok() && rh.value().valid);
  CHECK(rh.value().variance > 1000.0);       // 高信号 → 大方差
}

static void test_negative_values() {
  // 负值 (校准后可能): MAD 稳健, 不受个别负值影响
  std::mt19937 rng(3);
  std::normal_distribution<float> dist(0.0f, 5.0f);
  std::vector<float> px(2048);
  for (auto& v : px) v = dist(rng);
  px[10] = -100.0f;  // 离群负值
  NoiseModel m;
  auto r = m.estimate(px);
  CHECK(r.ok() && r.value().valid);
  CHECK(std::fabs(r.value().variance - 25.0) < 5.0);  // MAD 稳健
}

static void test_gain_edges() {
  NoiseModel m;
  // 零 gain → 无效
  auto r0 = m.gain_variance(100.0, 0.0, 3.0);
  CHECK(r0.ok() && !r0.value().valid);
  CHECK(r0.value().reason.find("gain") != std::string::npos);
  // 无效 read noise (0/负) → 无效
  auto rn = m.gain_variance(100.0, 1.0, 0.0);
  CHECK(rn.ok() && !rn.value().valid);
  CHECK(rn.value().reason.find("read_noise") != std::string::npos);
  // 有效: variance = signal/gain + read_noise²/gain²
  auto rv = m.gain_variance(100.0, 2.0, 4.0);
  CHECK(rv.ok() && rv.value().valid);
  CHECK(std::fabs(rv.value().variance - (50.0 + 4.0)) < 1e-9);  // 50 + 16/4
  CHECK(std::fabs(rv.value().ivar * rv.value().variance - 1.0) < 1e-12);
}

static void test_variance_ivar_not_mixed() {
  // variance 与 ivar 显式不混: 大 variance → 小 ivar
  NoiseModel m;
  std::mt19937 rng(9);
  std::normal_distribution<float> dist(50.0f, 2.0f);
  std::vector<float> lo_var(4096);
  for (auto& v : lo_var) v = dist(rng);
  auto rl = m.estimate(lo_var);
  std::normal_distribution<float> dist2(50.0f, 30.0f);
  std::vector<float> hi_var(4096);
  for (auto& v : hi_var) v = dist2(rng);
  auto rh = m.estimate(hi_var);
  CHECK(rl.ok() && rh.ok());
  CHECK(rl.value().variance < rh.value().variance);
  CHECK(rl.value().ivar > rh.value().ivar);   // 反比, 不混
}

int main() {
  test_blank_sky();
  test_monte_carlo_poisson();
  test_low_high_signal();
  test_negative_values();
  test_gain_edges();
  test_variance_ivar_not_mixed();
  if (failures == 0) {
    std::printf("P1-005 TESTS PASS (SCI 公式/blank sky/MC Poisson/低高信号/负值/零 gain/无效 read noise/ivar 不混)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-005 TESTS FAIL (%d)\n", failures);
  return 1;
}
