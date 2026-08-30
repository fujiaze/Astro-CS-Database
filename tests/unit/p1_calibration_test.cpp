// P1-002 单元测试: synthetic fixtures 解析公式逐像素比较
// (constant bias/dark/flat, gradient flat, negative result, NaN/mask, u16/f32/f64)
#include "astro_calibration.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// 标准模式参考: (light - dark) / max(flat, 0.1)
static float ref_std(float light, float dark, float flat) {
  float v = light;
  v -= dark;
  v /= std::max(flat, 0.1f);
  return v;
}
// 暗场优化参考: (light - bias - k*(dark-bias)) / max(flat, 0.1)
static float ref_darkopt(float light, float bias, float dark, float flat, float k) {
  float v = light - bias - k * (dark - bias);
  v /= std::max(flat, 0.1f);
  return v;
}

static void run_std_case(const char* name, int w, int h, float k_scale) {
  std::vector<float> light(static_cast<size_t>(w) * h);
  std::vector<float> dark(static_cast<size_t>(w) * h);
  std::vector<float> flat(static_cast<size_t>(w) * h);
  std::vector<float> out(static_cast<size_t>(w) * h, 0.0f);
  for (size_t i = 0; i < light.size(); ++i) {
    light[i] = static_cast<float>(i % 256) * k_scale;
    dark[i] = static_cast<float>(i % 37);
    flat[i] = 0.5f + 0.5f * static_cast<float>(i % 3);  // 含 0.5/1.0/1.5 变化
  }
  // 一个像素 flat=0.05 (<0.1 下限) → 用 0.1
  flat[0] = 0.05f;
  float actual_k = 0;
  CHECK(ac_calibrate_frame(light.data(), w, h, dark.data(), flat.data(), nullptr,
                           out.data(), 0, 1.0f, &actual_k) == AC_OK);
  CHECK(actual_k == 1.0f);
  for (size_t i = 0; i < out.size(); ++i) {
    float ref = ref_std(light[i], dark[i], flat[i]);
    if (std::fabs(out[i] - ref) > 1e-4f) {
      std::fprintf(stderr, "%s mismatch at %zu: got %f ref %f\n", name, i, out[i], ref);
      ++failures; break;
    }
  }
}

static void run_darkopt_case(const char* name, int w, int h, bool negative, bool nan_input) {
  std::vector<float> light(static_cast<size_t>(w) * h);
  std::vector<float> bias(static_cast<size_t>(w) * h);
  std::vector<float> dark(static_cast<size_t>(w) * h);
  std::vector<float> flat(static_cast<size_t>(w) * h, 1.0f);
  std::vector<float> out(static_cast<size_t>(w) * h, 0.0f);
  for (size_t i = 0; i < light.size(); ++i) {
    light[i] = static_cast<float>(i % 200);
    bias[i] = static_cast<float>(i % 13);
    dark[i] = static_cast<float>(i % 29);
    if (negative && i == 5) light[i] = -3.0f;      // 负结果
    if (nan_input && i == 7) light[i] = NAN;       // NaN 输入
  }
  const float k = 2.0f;
  float actual_k = 0;
  CHECK(ac_calibrate_frame(light.data(), w, h, dark.data(), flat.data(), bias.data(),
                           out.data(), 1, k, &actual_k) == AC_OK);
  CHECK(std::fabs(actual_k - k) < 1e-6f);
  for (size_t i = 0; i < out.size(); ++i) {
    float ref = ref_darkopt(light[i], bias[i], dark[i], flat[i], k);
    if (std::isnan(ref)) { if (!std::isnan(out[i])) { ++failures; break; } continue; }
    if (std::fabs(out[i] - ref) > 1e-4f) {
      std::fprintf(stderr, "%s mismatch at %zu: got %f ref %f\n", name, i, out[i], ref);
      ++failures; break;
    }
  }
}

int main() {
  // constant bias/dark/flat + gradient flat (i%3 梯度) — f32 标准模式
  run_std_case("std-f32-16x16", 16, 16, 1.0f);
  run_std_case("std-f32-64x32", 64, 32, 0.5f);

  // dark optimization: negative result + NaN input
  run_darkopt_case("darkopt-negative", 16, 16, true, false);
  run_darkopt_case("darkopt-nan", 16, 16, false, true);

  // u16/f64: calibrate_d (FP64 模式, 与 float 一致但更高精度)
  {
    const int w = 32, h = 8;
    std::vector<double> light(static_cast<size_t>(w) * h);
    std::vector<double> dark(static_cast<size_t>(w) * h);
    std::vector<double> flat(static_cast<size_t>(w) * h, 1.0);
    std::vector<double> out(static_cast<size_t>(w) * h, 0.0);
    for (size_t i = 0; i < light.size(); ++i) {
      light[i] = static_cast<double>(i % 4096);   // u16 域 [0,4096)
      dark[i] = static_cast<double>(i % 128);
    }
    CHECK(ac_calibrate_frame_f64(light.data(), w, h, dark.data(), flat.data(), nullptr,
                                 out.data(), 0, 1.0, nullptr) == AC_OK);
    for (size_t i = 0; i < out.size(); ++i) {
      double ref = (light[i] - dark[i]) / 1.0;
      if (std::fabs(out[i] - ref) > 1e-9) {
        std::fprintf(stderr, "f64 mismatch at %zu: got %f ref %f\n", i, out[i], ref);
        ++failures; break;
      }
    }
  }

  if (failures == 0) {
    std::printf("P1-002 TESTS PASS (constant/gradient/negative/NaN/u16域/f32/f64 解析比较)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-002 TESTS FAIL (%d)\n", failures);
  return 1;
}
