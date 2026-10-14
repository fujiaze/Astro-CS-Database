// 单元检验: 球面参考精度 GLS 完成（数学对拍 = 独立 NumPy oracle）
//
// 依据: docs/science/noise_snr/NOISE_SNR.md §3/§11,
//       docs/engineering/contracts/NORMALIZE_DATA.md §5/§6,
//       eng/tests/synthetic/README.md 的分层意图（单元层: 解析/独立实现对拍）。
//
// 判据（正例）: 已知 h/C/Z 的解析世界, V_F 与 [(A^T C^{-1} A)^{-1}]_{FF} 逐位一致
//              （相对误差 <= 1e-9）; SNR_ref = F_ref/sqrt(V_F)。
// 判据（反例）: 设计秩亏（观测数 <= 参数数）⇒ unavailable 且带具名原因,
//              不产伪 V_F/SNR（不用伪逆制造信息量）。
// 判据（负例注入）: 把 C 的最小特征方向置零（h 落入零空间）⇒ unavailable
//              （template_outside_covariance_range）, 不得静默丢弃后仍报 available。
#include <cmath>
#include <cstdio>
#include <vector>

#include "acsd/sparse_reference.h"

using namespace acsd::normalize;

static int g_failures = 0;
static void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++g_failures;
}

// 构造 3 叶 × 4 像元解析世界（见文件头注释推导）。
static PlanarPatchWindow make_window() {
  PlanarPatchWindow w;
  w.control_id = "ctrl0";
  w.cx_px = 100.0;
  w.cy_px = 200.0;
  w.pix_index = {0, 1, 2, 3};
  w.profile = {0.5, 0.3, 0.15, 0.05};
  w.variance = {1.0, 1.0, 1.0, 1.0};
  w.src_x = {99.0, 100.0, 101.0, 102.0};
  w.src_y = {199.0, 200.0, 201.0, 202.0};
  w.leaf_ipix = {11, 12, 13};
  w.leaf_order = 10;
  w.omega_leaf = 4.0 * 3.14159265358979323846 / (12.0 * 1024.0 * 1024.0);
  // R (3×4): leaf0←{0,1}, leaf1←{2,3}, leaf2←{0,2}
  w.r_coeff = {0.5, 0.5, 0.0, 0.0,
               0.0, 0.0, 0.5, 0.5,
               0.5, 0.0, 0.5, 0.0};
  w.leaf_dp = {1.0, 1.0, 1.0};
  w.leaf_np = {1.0, 1.0, 1.0};
  w.leaf_support = {w.omega_leaf, w.omega_leaf, w.omega_leaf};
  return w;
}

int main() {
  SphericalCompletionConfig cfg;
  cfg.reference.kind = ReferenceKind::kExplicitLinearFlux;
  cfg.reference.reference_id = "user:test-ref";
  cfg.reference.flux_unit = "adu";
  cfg.reference.normalization = "native_calibrated";
  cfg.reference.flux = 10.0;
  cfg.background = BackgroundNuisance::kKnown;
  cfg.gain_e_per_adu = 0.0;  // 未知 gain ⇒ 缺源泊松项 ⇒ upper_bound
  cfg.master_uncertainty_modelled = false;

  // ── 正例: 背景已知 ────────────────────────────────────────────────────────
  // h = R P = [0.4, 0.1, 0.325]
  // C = R I R^T = [[0.5,0,0.25],[0,0.5,0.25],[0.25,0.25,0.5]]
  // I_F = h^T C^{-1} h ; 独立 oracle（同参数 NumPy 复算）= 0.3625
  {
    const auto r = complete_spherical_reference({make_window()}, cfg);
    check(r.precision.controls.size() == 1, "one control produced");
    const auto& c = r.precision.controls[0];
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "background_known: control available");
    check(c.reference_flux.has_value() && std::abs(*c.reference_flux - 10.0) < 1e-12,
          "reference_flux recorded");
    check(c.flux_variance.has_value() && c.reference_snr.has_value(),
          "V_F and SNR recorded");
    if (c.flux_variance.has_value()) {
      const double oracle_i_f = 0.3625;  // numpy 对拍值
      const double got = 1.0 / *c.flux_variance;
      check(std::abs(got - oracle_i_f) <= 1e-9 * oracle_i_f,
            "V_F matches independent GLS oracle (<=1e-9)");
      const double snr = 10.0 / std::sqrt(*c.flux_variance);
      check(std::abs(*c.reference_snr - snr) <= 1e-12,
            "SNR_ref = F_ref/sqrt(V_F)");
    }
    check(c.status.accuracy == PrecisionAccuracy::kUpperBound,
          "gain unknown + template fixed ⇒ upper_bound caliber");
    check(c.status.omitted_terms.size() == 2, "omitted terms: source_poisson+master");
    check(c.transfer.availability == PrecisionAvailability::kAvailable,
          "surface transfer available (same source/position/condition)");
    if (c.transfer.transfer_factor.has_value()) {
      // T = V_B/V_F, V_B = C[target][target] (C_00 = 0.5) ⇒ T = 0.5/V_F
      check(c.transfer.surface_variance.has_value() &&
                std::abs(*c.transfer.surface_variance - 0.5) < 1e-12 &&
                std::abs(*c.transfer.transfer_factor -
                         0.5 / (*c.flux_variance)) < 1e-9,
            "T = V_B/V_F with V_B from same covariance diagonal");
    }
    const auto s = summarize_frame_reference(r.precision.controls);
    check(s.status.availability == PrecisionAvailability::kAvailable && s.n_valid == 1,
          "frame summary available (1 valid control)");
    check(s.reference_snr.has_value() &&
             std::abs(*s.reference_snr - *c.reference_snr) < 1e-12,
             "single-control summary = exp(median(log snr))");
  }

  // ── 正例: 背景常数拟合（3 观测 2 参数 ⇒ 可估计）────────────────────────────
  {
    SphericalCompletionConfig c2 = cfg;
    c2.background = BackgroundNuisance::kConstantFit;
    const auto r = complete_spherical_reference({make_window()}, c2);
    const auto& c = r.precision.controls[0];
    check(c.status.availability == PrecisionAvailability::kAvailable,
          "constant_fit: control available");
    check(c.background_nuisance == "constant_fit", "background_nuisance recorded");
    // A=[h,z], z=R·1=[1,1,1]; numpy 对拍 I_FF = 0.1125
    if (c.flux_variance.has_value()) {
      const double got = 1.0 / *c.flux_variance;
      check(std::abs(got - 0.1125) <= 1e-8,
            "constant_fit: V_F matches GLS-with-nuisance oracle");
    }
  }

  // ── 反例: 观测数不足（设计秩亏）⇒ unavailable, 不造伪值 ────────────────────
  {
    PlanarPatchWindow w = make_window();
    w.leaf_ipix = {11};                  // 只剩 1 个观测
    w.r_coeff = {0.5, 0.5, 0.0, 0.0};     // 1×4
    w.leaf_dp = {1.0};
    w.leaf_np = {1.0};
    w.leaf_support = {w.omega_leaf};
    SphericalCompletionConfig c2 = cfg;
    c2.background = BackgroundNuisance::kConstantFit;  // 1 观测 2 参数 ⇒ 秩亏
    const auto r = complete_spherical_reference({w}, c2);
    check(r.precision.controls[0].status.availability ==
              PrecisionAvailability::kUnavailable,
          "rank-deficient design ⇒ unavailable");
    check(!r.precision.controls[0].flux_variance.has_value() &&
              !r.precision.controls[0].reference_snr.has_value(),
          "rank-deficient: no fabricated V_F/SNR");
    check(!r.precision.controls[0].status.reason.empty(),
          "rank-deficient: named reason present");
  }

  // ── 负例注入: h 落入协方差零空间 ⇒ unavailable（不静默丢弃）────────────────
  // 构造: 3 叶各自只由 1 个像元贡献 (R 行 = 单位向量), 像元 2 方差 1e-20
  //   ⇒ C = diag(1,1,1e-20), 第三特征方向低于秩容差被排除; h = P = (.5,.3,.15)
  //   在该排除方向上有分量 0.15 ⇒ 模板超出协方差值域, 必须 unavailable。
  {
    PlanarPatchWindow w = make_window();
    w.leaf_ipix = {11, 12, 13};
    w.r_coeff = {1.0, 0.0, 0.0, 0.0,
                 0.0, 1.0, 0.0, 0.0,
                 0.0, 0.0, 1.0, 0.0};
    w.leaf_dp = {1.0, 1.0, 1.0};
    w.leaf_np = {1.0, 1.0, 1.0};
    w.leaf_support = {w.omega_leaf, w.omega_leaf, w.omega_leaf};
    w.variance = {1.0, 1.0, 1e-20, 1.0};
    const auto r = complete_spherical_reference({w}, cfg);
    check(r.precision.controls[0].status.availability ==
              PrecisionAvailability::kUnavailable,
          "template outside covariance range ⇒ unavailable");
    check(r.precision.controls[0].status.reason.find(
              "template_outside_covariance_range") == 0,
          "template outside covariance range: named reason");
  }

  // ── 反例: 参考缺项 ⇒ 整体 unavailable, 不发明中位 F0 ───────────────────────
  {
    SphericalCompletionConfig c2;
    c2.reference.kind = ReferenceKind::kUnavailable;
    c2.reference.reason = "no reference configured";
    const auto r = complete_spherical_reference({make_window()}, c2);
    check(r.precision.status.availability == PrecisionAvailability::kUnavailable,
          "no reference ⇒ precision unavailable");
    check(r.precision.controls[0].status.reason == "no reference configured",
          "no reference: reason propagated to control");
  }

  // ── 反例: sed_response 未接线 ⇒ 显式 unavailable（不伪造绝对零点）──────────
  {
    SphericalCompletionConfig c2;
    c2.reference.kind = ReferenceKind::kSedResponse;
    const auto r = complete_spherical_reference({make_window()}, c2);
    check(r.precision.status.availability == PrecisionAvailability::kUnavailable,
          "sed_response without evaluable integration ⇒ unavailable");
  }

  std::printf("\n%s (failures=%d)\n", g_failures == 0 ? "ALL PASS" : "HAS FAILURES",
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
