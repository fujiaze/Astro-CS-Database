// eng/tests/unit/v6_p2_sky_kappa/v6_p2_sky_kappa_test.cpp
// SCI-502 FIX-3 专项回归锁：天光面 κ 可观测 + 粗糙度正则化自适应（红/绿双向）。
//
// 依据：DOC-502 docs/science/PHASE2_UPM.md §7a、docs/plugins/algorithms_phase2/11_upm.md §4.6、
//       CONTRACT-501 PERF_GATE_CONTRACT。
// 被测面：lib/algorithms/coverage/src/sky_plane.cpp（p2_sky_plane_build）。
//
// 本测试同时固定一条**实证订正**（已写入 §7a）：门控 κ 必须取**求解矩阵**
// H_solve = H_red + λ·DᵀD 的条件数，不能取未惩罚数据矩阵 H_red 的条件数——
// 后者与 λ 无关，「κ 超限 ⇒ 走粗糙度正则化」将永远无法成功（实测：λ 从 1e-3 提到
// 1e6，H_red 的 κ 恒为 2.497e9；而 H_solve 的 κ 在 λ=1e-3 时为 8.3e3）。
#include "astro/phase2/sky_plane.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {
int g_fail = 0;
int g_total = 0;
void check(bool ok, const std::string& what) {
  ++g_total;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL %s\n", what.c_str());
  }
}

std::vector<P2SkySample> make_samples() {
  std::vector<P2SkySample> v;
  for (std::uint64_t f = 0; f < 2; ++f) {
    for (int iy = 0; iy < 12; ++iy) {
      for (int ix = 0; ix < 12; ++ix) {
        P2SkySample s{};
        s.frame_id = 100 + f;
        s.control_id = static_cast<std::uint64_t>(iy * 12 + ix);
        s.ra_deg = 10.0 + 0.03 * ix;
        s.dec_deg = 20.0 + 0.03 * iy;
        const double u = 0.03 * ix, w = 0.03 * iy;
        s.value = 300.0 + 40.0 * u + 25.0 * w + 3.0 * u * w + 7.0 * static_cast<double>(f);
        s.variance = 4.0;
        s.snr = 150.0;
        s.flags = P2_SKY_FLAG_NONE;
        v.push_back(s);
      }
    }
  }
  return v;
}

int build(const std::vector<P2SkySample>& s, double kappa_max, double roughness,
          double* kappa_out, double* kappa_data_out, std::string* err_out) {
  P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
  cfg.node_spacing_deg = 0.30;    // 良态节点布局（本测试只考察 κ 路径）
  cfg.frame_gradient_order = 0;   // 仅帧偏移
  cfg.kappa_max = kappa_max;
  cfg.roughness_penalty = roughness;
  char err[512] = {0};
  void* m = nullptr;
  const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, err, sizeof(err));
  if (err_out) *err_out = err;
  if (rc == P2_SKY_PLANE_OK && m) {
    P2SkyPlaneInfo info{};
    if (p2_sky_plane_info(m, &info) == 0) {
      if (kappa_out) *kappa_out = info.kappa;
      if (kappa_data_out) *kappa_data_out = info.kappa_data;
    }
    p2_sky_plane_close(m);
  }
  return rc;
}
}  // namespace

int main() {
  const std::vector<P2SkySample> s = make_samples();
  std::string err;

  // A. κ 可观测
  double k_def = 0.0, kd_def = 0.0;
  const int rc_a = build(s, 1e300, 1e-3, &k_def, &kd_def, &err);
  check(rc_a == P2_SKY_PLANE_OK, "A1 default build ok, rc=" + std::to_string(rc_a) + " " + err);
  check(std::isfinite(k_def) && k_def > 0.0,
        "A2 kappa (solved matrix) finite>0, got " + std::to_string(k_def));
  check(std::isfinite(kd_def) && kd_def > 0.0,
        "A3 kappa_data (unpenalized) finite>0, got " + std::to_string(kd_def));
  std::printf("INFO kappa(solved, lambda=1e-3) = %.6g ; kappa_data = %.6g\n", k_def, kd_def);

  double k_zero = 0.0, kd_zero = 0.0;
  const int rc_z = build(s, 1e300, 0.0, &k_zero, &kd_zero, &err);
  check(rc_z == P2_SKY_PLANE_OK, "A4 lambda=0 build ok, rc=" + std::to_string(rc_z) + " " + err);
  check(std::fabs(k_zero - kd_zero) <= 1e-9 * std::max(1.0, kd_zero),
        "A5 lambda=0 => kappa == kappa_data (same matrix)");
  std::printf("INFO kappa(lambda=0) = %.6g ; kappa_data(lambda=0) = %.6g\n", k_zero, kd_zero);

  // D. 正则化方向有效；κ_data 与 λ 无关
  check(k_def < k_zero * 0.5,
        "D1 regularization reduces gated kappa: " + std::to_string(k_def) + " vs " +
            std::to_string(k_zero));
  check(std::fabs(kd_def - kd_zero) <= 1e-6 * std::max(1.0, kd_zero),
        "D2 kappa_data independent of lambda: " + std::to_string(kd_def) + " vs " +
            std::to_string(kd_zero));

  // B/C. 红绿双向：gate 取几何中点，必然夹在 λ=0 与生产 λ 之间
  const double kappa_gate = std::sqrt(k_def * k_zero);
  const int rc_b = build(s, kappa_gate, 0.0, nullptr, nullptr, &err);
  check(rc_b == P2_SKY_PLANE_KAPPA_EXCEEDED,
        "B1 lambda=0 must be KAPPA_EXCEEDED under gate=" + std::to_string(kappa_gate) +
            ", got rc=" + std::to_string(rc_b) + " " + err);
  check(err.find("kappa") != std::string::npos,
        "E1 error text names kappa (not rank), got: " + err);
  check(rc_b != P2_SKY_PLANE_RANK_DEFICIENT,
        "E2 not rank-deficient (rank is fine, kappa is not)");

  double k_c = 0.0;
  const int rc_c = build(s, kappa_gate, 1e-3, &k_c, nullptr, &err);
  check(rc_c == P2_SKY_PLANE_OK,
        "C1 production lambda must succeed under the same gate, got rc=" +
            std::to_string(rc_c) + " " + err);
  check(k_c <= kappa_gate, "C2 gated kappa within gate: " + std::to_string(k_c));
  std::printf("INFO gate = %.6g ; lambda=0 rc=%d ; lambda=1e-3 rc=%d kappa=%.6g\n",
              kappa_gate, rc_b, rc_c, k_c);

  std::printf("P2-SKY-KAPPA: %d/%d checks passed, %d failed\n", g_total - g_fail, g_total,
              g_fail);
  return g_fail == 0 ? 0 : 1;
}
