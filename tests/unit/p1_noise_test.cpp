// P1-005 单元测试: Noise/SNR SCI 公式 + Monte Carlo + 边界
//
// NOISE-MODEL-CANON-001 (2026-09-20, 负责人 §9.67 定案 3「选对的」):
//   本测试原锚定 `wrapper_phase1::NoiseModel` (B) —— 它是 SCI-NOISE-001 §5/§5a
//   唯一实现 (A, `cpp/src/noise_model.cpp`) 的**退化子集**，且生产调用点曾把整帧
//   喂入其「空白背景像素集」域。B 已退役，本测试**重锚到 A**，保留原有断言强度
//   (blank sky / MC Poisson / 低高信号 / 负值与离群稳健 / gain 边界 / ivar 不混)。
#include "snr_estimator.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 用 A 估计一帧（无星掩膜输入；测试只关心 blank-sky 稳健统计）。
// 返回 rc；模型在 out 中，调用方负责 snr_noise_model_v1_free。
static int estimate_frame(const std::vector<float>& px, int h, int w,
                          NoiseWeightModelV1* out) {
  SnrNoiseModelConfig cfg{};
  if (snr_noise_model_v1_default_config(&cfg) != 0) return -100;
  return snr_noise_model_v1(px.data(), h, w, nullptr, nullptr, nullptr,
                            nullptr, nullptr, 0, &cfg, out);
}

static void test_blank_sky() {
  // blank sky: 已知 sigma=10 白噪声 → 恢复 variance≈100
  std::mt19937 rng(42);  // 固定 seed
  std::normal_distribution<float> dist(100.0f, 10.0f);
  std::vector<float> px(64 * 64);
  for (auto& v : px) v = dist(rng);
  NoiseWeightModelV1 nm{};
  const int rc = estimate_frame(px, 64, 64, &nm);
  CHECK(rc == 0 || rc == 1);
  if (rc == 0) {
    CHECK(nm.degenerate == 0);
    CHECK(std::fabs(nm.variance_bg_global - 100.0) < 15.0);   // 采样误差
    CHECK(std::fabs(nm.sigma_bg_global - 10.0) < 1.0);
    // ivar 与 variance 显式不混
    CHECK(std::fabs(nm.ivar_bg_global * nm.variance_bg_global - 1.0) < 1e-9);
    CHECK(nm.n_qualified_patches > 0);
  }
  snr_noise_model_v1_free(&nm);
}

static void test_monte_carlo_poisson() {
  // Poisson+read noise Monte Carlo (固定 seed): 解析均值/方差对照
  // 模型 (SCI-NOISE-001 §5:58; gain 为 e-/ADU):
  //   N_e ~ N(signal*gain, signal*gain)     (Poisson 大均值正态近似)
  //   ADU = N_e/gain + N(0, read_noise_e/gain)
  //   E[x] = signal [ADU];  Var(x) = signal/gain + (read_noise_e/gain)^2 [ADU^2]
  const double signal = 500.0, gain = 1.5, read_noise = 3.0;
  const std::size_t n_pix = 65536;  // MAD 的 SE≈1.166/sqrt(n)=0.46% => 5% 门 >>5sigma
  const int side = 256;
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
  NoiseWeightModelV1 nm{};
  const int rc = estimate_frame(px, side, side, &nm);
  CHECK(rc == 0 || rc == 1);
  if (rc == 0) {
    // 解析: variance = signal/gain + (read_noise_e/gain)^2  [ADU^2] (SCI §5:58)
    const double analytic_var =
        signal / gain + (read_noise / gain) * (read_noise / gain);
    CHECK(std::fabs(nm.variance_bg_global - analytic_var) / analytic_var < 0.05);
  }
  snr_noise_model_v1_free(&nm);
  // 生产诊断式必须与 SCI 解析式一致 (原测试从未调用它 ⇒ 抓不住方向反转)
  const double gv = snr_noise_gain_variance(signal, gain, read_noise);
  const double analytic_var =
      signal / gain + (read_noise / gain) * (read_noise / gain);
  CHECK(std::fabs(gv - analytic_var) < 1e-9 * analytic_var);
}

static void test_low_high_signal() {
  // 低信号 (近零方差) 与高信号: 高信号方差必须更大
  std::vector<float> low(32 * 32, 100.0f);   // 全相同 → 稳健 sigma=0
  NoiseWeightModelV1 nl{};
  const int rcl = estimate_frame(low, 32, 32, &nl);
  CHECK(rcl == 0 || rcl == 1);
  snr_noise_model_v1_free(&nl);

  std::mt19937 rng(1);
  std::normal_distribution<float> dist(1000.0f, 50.0f);
  std::vector<float> high(64 * 64);
  for (auto& v : high) v = dist(rng);
  NoiseWeightModelV1 nh{};
  const int rch = estimate_frame(high, 64, 64, &nh);
  CHECK(rch == 0 || rch == 1);
  if (rch == 0) CHECK(nh.variance_bg_global > 1000.0);   // 高信号 → 大方差
  snr_noise_model_v1_free(&nh);
}

static void test_negative_values() {
  // 负值 (校准后可能) + 离群: MAD 稳健, 不受个别离群值影响
  std::mt19937 rng(3);
  std::normal_distribution<float> dist(0.0f, 5.0f);
  std::vector<float> px(64 * 64);
  for (auto& v : px) v = dist(rng);
  px[10] = -100.0f;   // 离群负值
  px[11] = 5000.0f;   // 离群正值 (cosmic 稳健裁剪必须挡住)
  NoiseWeightModelV1 nm{};
  const int rc = estimate_frame(px, 64, 64, &nm);
  CHECK(rc == 0 || rc == 1);
  if (rc == 0) {
    // patch 内 5sigma cosmic 裁剪 + MAD 双重稳健 ⇒ 仍应接近 25
    CHECK(std::fabs(nm.variance_bg_global - 25.0) < 8.0);
  }
  snr_noise_model_v1_free(&nm);
}

static void test_gain_edges() {
  // 零 gain: 解析式不可用 (非有限或非正)
  const double v0 = snr_noise_gain_variance(100.0, 0.0, 3.0);
  CHECK(!(std::isfinite(v0) && v0 > 0.0));
  // 零 read noise: 退化为纯 Poisson 项 (SCI 未规定拒绝; A 的有效域允许 rn=0)
  const double v1 = snr_noise_gain_variance(100.0, 2.0, 0.0);
  CHECK(std::isfinite(v1) && v1 > 0.0);
  CHECK(std::fabs(v1 - 50.0) < 1e-9);          // 100/2 + 0
  // 有效: variance = signal/gain + read_noise^2/gain^2
  const double v2 = snr_noise_gain_variance(100.0, 2.0, 4.0);
  CHECK(std::fabs(v2 - (50.0 + 4.0)) < 1e-9);  // 50 + 16/4
}

static void test_variance_ivar_not_mixed() {
  // variance 与 ivar 显式不混: 大 variance → 小 ivar
  std::mt19937 rng(9);
  std::normal_distribution<float> d1(50.0f, 2.0f);
  std::vector<float> lo_var(64 * 64);
  for (auto& v : lo_var) v = d1(rng);
  std::normal_distribution<float> d2(50.0f, 30.0f);
  std::vector<float> hi_var(64 * 64);
  for (auto& v : hi_var) v = d2(rng);
  NoiseWeightModelV1 nl{}, nh{};
  const int rl = estimate_frame(lo_var, 64, 64, &nl);
  const int rh = estimate_frame(hi_var, 64, 64, &nh);
  CHECK(rl == 0 && rh == 0);
  if (rl == 0 && rh == 0) {
    CHECK(nl.variance_bg_global < nh.variance_bg_global);
    CHECK(nl.ivar_bg_global > nh.ivar_bg_global);   // 反比, 不混
    CHECK(std::fabs(nl.ivar_bg_global * nl.variance_bg_global - 1.0) < 1e-9);
    CHECK(std::fabs(nh.ivar_bg_global * nh.variance_bg_global - 1.0) < 1e-9);
  }
  snr_noise_model_v1_free(&nl);
  snr_noise_model_v1_free(&nh);
}

static void test_per_pixel_fill() {
  // NOISE-MODEL-CANON-001: 逐像素 variance/ivar 产品面 (只有 A 能给)。
  // 空间场启用且 >=4 控制点 → var(x,y)=a+b*x+c*y; 否则全局常量。
  std::mt19937 rng(11);
  std::normal_distribution<float> dist(100.0f, 8.0f);
  std::vector<float> px(64 * 64);
  for (auto& v : px) v = dist(rng);
  NoiseWeightModelV1 nm{};
  const int rc = estimate_frame(px, 64, 64, &nm);
  CHECK(rc == 0 || rc == 1);
  if (rc == 0) {
    std::vector<float> var(64 * 64, 0.0f), ivar(64 * 64, 0.0f);
    const int frc = snr_noise_model_v1_fill(&nm, 64, 64, var.data(), ivar.data());
    CHECK(frc == 0);
    if (frc == 0) {
      for (std::size_t i = 0; i < var.size(); ++i) {
        CHECK(std::isfinite(var[i]) && var[i] > 0.0f);
        CHECK(std::fabs(static_cast<double>(ivar[i]) * var[i] - 1.0) < 1e-6);
      }
    }
  }
  snr_noise_model_v1_free(&nm);
}

int main() {
  test_blank_sky();
  test_monte_carlo_poisson();
  test_low_high_signal();
  test_negative_values();
  test_gain_edges();
  test_variance_ivar_not_mixed();
  test_per_pixel_fill();
  if (failures == 0) {
    std::printf("P1-005 TESTS PASS (SCI 公式/blank sky/MC Poisson/低高信号/负值离群/零 gain/ivar 不混/逐像素 fill)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-005 TESTS FAIL (%d)\n", failures);
  return 1;
}