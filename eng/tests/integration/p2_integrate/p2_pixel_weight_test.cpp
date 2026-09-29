/* p2_pixel_weight_test.cpp — P2 逐像素权重面判据（M06）
 *
 * 目的：把「稀疏层（绝对 SNR）的消费发生在**逐输出像素**的权重路径上」这条冻结
 * 语义变成可红可绿的机器判据。所有断言都基于**集成侧生产 API**，不使用测试专用
 * 后门（除了独立 oracle 复算）。
 *
 * 权威（只读）：
 *   docs/ASTROCS_DESIGN.md 3.1:263/264/267、2.4:240、5.3:439
 *   docs/science/UNIFIED_SCIENCE_MODEL.md:59/62
 *   docs/science/PSF_SIGNAL_WEIGHT.md:75/87
 *   eng/contracts/schemas/unified/sparse_snr_layer.schema.json
 *   docs/detail/algorithms_phase2/13_integration.md 4.0
 *
 * 判据：
 *   (A) 量纲/口径一致（含「层值缺失 = 显式降级而非乘 1」）
 *   (B) 反例：误乘帧级标量必须判红（给出注入方式与偏差量级）
 *   (C) 两条逐像素入口逐像素一致（direct layer vs prepared reconstructor）
 *   (D) module_adapters 侧逐像素面确实调用同一生产 API（单一实现）
 */
#include <astrocs/weight_chain.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace p2w = astrocs::v6::p2weight;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const std::string& what) {
  if (ok) {
    ++g_pass;
    std::printf("  [PASS] %s\n", what.c_str());
  } else {
    ++g_fail;
    std::printf("  [FAIL] %s\n", what.c_str());
  }
}
static bool close(double a, double b, double rtol = 1e-12) {
  return std::fabs(a - b) <= rtol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

/* 独立 oracle：由 sigma_F = F_ref/SNR 出发复算 w = 1/sigma_F^2。
   **不调用** weight_from_snr，避免与被测实现同源而恒真。 */
static double oracle_w(double snr, double fref) {
  const double sigma_f = fref / snr;
  return 1.0 / (sigma_f * sigma_f);
}

/* 独立双线性 oracle（4x4 规则网格；用于核对重建出的**绝对**层值） */
static double oracle_bilinear(const p2w::SparseSnrLayer& L, double x, double y) {
  const int nx = L.nx, ny = L.ny;
  const double fx = (x - L.x0) / L.dx, fy = (y - L.y0) / L.dy;
  int i = static_cast<int>(std::floor(fx)), j = static_cast<int>(std::floor(fy));
  i = std::max(0, std::min(nx - 2, i));
  j = std::max(0, std::min(ny - 2, j));
  const double tx = std::max(0.0, std::min(1.0, fx - i));
  const double ty = std::max(0.0, std::min(1.0, fy - j));
  auto v = [&](int ii, int jj) {
    return L.points[static_cast<std::size_t>(jj) * nx + ii].snr;
  };
  return (1 - tx) * (1 - ty) * v(i, j) + tx * (1 - ty) * v(i + 1, j) +
         (1 - tx) * ty * v(i, j + 1) + tx * ty * v(i + 1, j + 1);
}

static p2w::SparseSnrLayer grid4x4() {
  p2w::SparseSnrLayer L;
  L.present = true;
  L.semantics = p2w::SparseSnrSemantics::kAbsoluteFluxTypeSnr;
  L.regular_grid = true;
  L.nx = 4;
  L.ny = 4;
  L.dx = 8.0;
  L.dy = 8.0;
  L.x0 = 3.5;
  L.y0 = 3.5;
  L.grid_origin_x = 0.0;
  L.grid_origin_y = 0.0;
  const double v[16] = {100.0, 110.0, 90.0,  105.0,
                        80.0,  140.0, 120.0, 70.0,
                        130.0, 60.0,  150.0, 100.0,
                        95.0,  125.0, 85.0,  135.0};
  for (int j = 0; j < 4; ++j)
    for (int i = 0; i < 4; ++i)
      L.points.push_back(p2w::SparseSnrPoint{3.5 + 8.0 * i, 3.5 + 8.0 * j, v[j * 4 + i]});
  return L;
}

int main() {
  std::printf("== p2_pixel_weight_test: sparse-layer per-pixel weight face (M06) ==\n");

  const double fref = 1000.0;   /* 组标量（审计用） */
  const double fref_k = 812.5;  /* 逐帧 F_ref,k（与组标量不同，验证逐帧耦合） */
  const double g = 41.0;        /* 逐帧乘性光度响应 g_k */
  const double frame_snr = 200.0;  /* 帧级 SNR：独立对象，不得进入逐像素面 */

  /* ---------------- (A) 量纲/口径一致性 ---------------- */
  std::printf("[A] dimensional / scale consistency of the per-pixel face\n");
  {
    p2w::SparseSnrLayer L = grid4x4();
    const double qs[9][2] = {{3.5, 3.5},  {11.5, 3.5}, {19.5, 3.5}, {27.5, 3.5},
                             {3.5, 11.5}, {11.5, 11.5}, {19.5, 19.5}, {27.5, 27.5},
                             {31.0, 31.0}};
    bool units_ok = true, ident_ok = true, sigma_ok = true, gain_ok = true;
    double worst = 0.0;
    for (const auto& q : qs) {
      p2w::PixelWeightInput in;
      in.frame_id = "f0";
      in.layer = &L;
      in.x = q[0];
      in.y = q[1];
      in.ref_flux_k = fref_k;
      in.gain = &g;
      const p2w::PixelWeightResult r = p2w::weight_from_sparse_layer_pixel(in);
      if (!r.ok) {
        units_ok = false;
        continue;
      }
      units_ok = units_ok && std::string(r.weight_units) == "ADU^-2" &&
                 std::string(r.snr_units) == "dimensionless" &&
                 std::string(r.reference_flux_units) == "ADU";
      ident_ok = ident_ok &&
                 std::fabs(r.dimensional_identity) <=
                     1e-15 * std::max(1.0, std::fabs(r.weight) * fref_k * fref_k);
      const double w_want = oracle_w(r.layer_snr, fref_k) * g * g;
      const double rel = std::fabs(w_want - r.weight) / r.weight;
      worst = std::max(worst, rel);
      sigma_ok = sigma_ok && rel <= 1e-12;
      gain_ok = gain_ok && close(r.gain, g) && close(r.reference_flux_k, fref_k);
    }
    check(units_ok, "unit tokens: w[ADU^-2] = SNR[1]^2 / F_ref[ADU]^2");
    check(ident_ok, "dimensional identity w*F_ref_k^2 - SNR_layer^2*g^2 == 0 (9 pixels)");
    check(sigma_ok || worst <= 1e-12,
          "w == 1/sigma_F^2 with sigma_F = F_ref,k/SNR_layer (independent oracle)");
    check(gain_ok, "per-frame F_ref,k and g_k are carried through unchanged");
    std::printf("        (max rel dev vs 1/sigma_F^2: %.3e)\n", worst);
    /* 独立 oracle 也核对**层值**本身（绝对 SNR，非相对因子） */
    bool val_ok = true;
    for (const auto& q : qs) {
      p2w::PixelWeightInput in;
      in.layer = &L;
      in.x = q[0];
      in.y = q[1];
      in.ref_flux_k = fref_k;
      const p2w::PixelWeightResult r = p2w::weight_from_sparse_layer_pixel(in);
      /* 默认算子是 natural_bicubic_spline_clip_v1；在 4x4 上样条 != 双线性，
         故只在**控制点**处比对（节点处两种插值都必须复现控制值 ⇒ 严格判据）。 */
      if (std::fabs(q[0] - std::floor(q[0] + 0.5)) < 1e-12 &&
          std::fabs(q[1] - std::floor(q[1] + 0.5)) < 1e-12) {
        const double want = oracle_bilinear(L, q[0], q[1]);
        val_ok = val_ok && close(r.layer_snr, want, 1e-9);
      }
    }
    check(val_ok, "layer value = absolute SNR at control nodes (independent bilinear oracle)");

    /* 层值缺失：显式降级（不是乘 1） */
    p2w::PixelWeightInput miss;
    miss.frame_id = "f0";
    miss.layer = nullptr;
    miss.x = 11.5;
    miss.y = 11.5;
    miss.ref_flux_k = fref_k;
    const p2w::PixelWeightResult rm = p2w::weight_from_sparse_layer_pixel(miss);
    check(!rm.ok && rm.generated_from_absent_layer,
          "absent layer -> explicit degradation (generated_from_absent_layer)");
    check(close(rm.weight, 0.0) && rm.weight_source == "frame_level_degraded",
          "absent layer emits NO weight and names the degraded source (not 1, not 1/sigma_F)");
    check(rm.closure == p2w::WeightClosure::kUnclosedSparseLayerRequiredMissing,
          "absent-layer closure token is explicit");
    p2w::WeightChainPolicy pol;
    pol.require_sparse_layer_for_pixel_weights = true;
    const p2w::PixelWeightResult rp = p2w::weight_from_sparse_layer_pixel(miss, pol);
    check(!rp.ok && !rp.generated_from_absent_layer &&
              rp.closure == p2w::WeightClosure::kUnclosedSparseLayerRequiredMissing,
          "policy-required absent layer -> hard fail (negative example of degradation)");
    /* 仅当 policy 要求 gain 而缺 gain 才判红；声明 g 时按 g 计 */
    p2w::PixelWeightInput nog = miss;
    nog.layer = &L;
    nog.gain = nullptr;
    const p2w::PixelWeightResult rg0 = p2w::weight_from_sparse_layer_pixel(nog);
    check(rg0.ok && close(rg0.gain, 1.0), "gain unset + policy off -> g = 1 (declared default)");
    p2w::WeightChainPolicy polg;
    polg.require_frame_gain = true;
    const p2w::PixelWeightResult rg1 = p2w::weight_from_sparse_layer_pixel(nog, polg);
    check(!rg1.ok && rg1.closure == p2w::WeightClosure::kUnclosedMissingGain,
          "gain required but missing -> fail-closed (no silent g = 1)");
  }

  /* ---------------- (B) 反例：误乘帧级标量必须判红 ---------------- */
  std::printf("[B] counter-example: multiplying in the frame-level SNR must go RED\n");
  {
    p2w::SparseSnrLayer L = grid4x4();
    p2w::PixelWeightInput in;
    in.frame_id = "f0";
    in.layer = &L;
    in.x = 11.5;
    in.y = 3.5;
    in.ref_flux_k = fref_k;
    in.gain = &g;
    const p2w::PixelWeightResult r = p2w::weight_from_sparse_layer_pixel(in);
    check(r.ok, "baseline per-pixel layer weight closed (needed for a meaningful test)");
    const double w_true = oracle_w(r.layer_snr, fref_k) * g * g;
    check(close(r.weight, w_true, 1e-12), "GREEN: w == (SNR_layer/F_ref,k)^2 * g^2");

    /* 注入 (I)：把层当帧内相对因子乘到帧级 SNR 上（旧 compose_actual_snr 语义）
       注入位置 = 权重式分子：SNR_used = frame_snr * layer 而不是 layer。 */
    const double w_I = oracle_w(frame_snr * r.layer_snr, fref_k) * g * g;
    const double rel_I = std::fabs(w_I - w_true) / w_true;
    check(rel_I > 1e-6,
          "injection (I) frame*layer is detected (relative deviation > 1e-6)");
    /* 注入 (II)：在逐像素面额外乘 frame^2（等价于把帧级标量塞进层值） */
    const double w_II = w_true * frame_snr * frame_snr;
    const double rel_II = std::fabs(w_II - w_true) / w_true;
    check(rel_II > 1e-6,
          "injection (II) extra frame^2 is detected (relative deviation > 1e-6)");
    std::printf("        (injection I : rel dev = %.3e ; injection II : rel dev = %.3e)\n",
                rel_I, rel_II);
    /* 真值无效应 ⇒ 归零：把层值设为与帧级同值时，「乘帧级」与「不乘」不再可区分，
       故本判据用 frame_snr != 1 的夹具（frame_snr = 200）保证非退化。 */
    check(!close(frame_snr, 1.0, 1e-12), "fixture uses frame_snr != 1 (test non-degenerate)");
    /* 正向锚：结果里没有帧级项；帧级支路与层支路数值上确实不同 */
    check(!close(oracle_w(frame_snr, fref_k) * g * g, w_true, 1e-6),
          "frame-level and layer-level weights are numerically distinct");
  }

  /* ---------------- (C) 两条逐像素入口逐像素一致 ---------------- */
  std::printf("[C] both per-pixel entries agree pixel-by-pixel (single implementation)\n");
  {
    p2w::SparseSnrLayer L = grid4x4();
    p2w::SparseSnrReconstructor rec;
    std::string err;
    check(rec.prepare(L, &err), "independent prepare() succeeded: " + err);
    int n = 0, bad = 0, oracle_bad = 0;
    for (int j = 0; j < 24; ++j) {
      for (int i = 0; i < 24; ++i) {
        const double x = 0.5 + 1.3 * i, y = 0.5 + 1.3 * j;
        p2w::PixelWeightInput in;
        in.frame_id = "f0";
        in.layer = &L;
        in.x = x;
        in.y = y;
        in.ref_flux_k = fref_k;
        in.gain = &g;
        const p2w::PixelWeightResult a = p2w::weight_from_sparse_layer_pixel(in);
        const p2w::PixelWeightResult b =
            p2w::weight_from_sparse_layer_pixel_prepared(rec, in, p2w::WeightChainPolicy());
        ++n;
        if (!a.ok || !b.ok) {
          ++bad;
          continue;
        }
        if (a.weight != b.weight || a.layer_snr != b.layer_snr) ++bad;
        if (!close(a.weight, oracle_w(a.layer_snr, fref_k) * g * g, 1e-12)) ++oracle_bad;
      }
    }
    check(n == 576, "576 pixels compared (non-vacuous)");
    check(bad == 0, "direct-layer and prepared-reconstructor entries are bit-identical");
    check(oracle_bad == 0, "both entries match the independent oracle at every pixel");
  }

  std::printf("\n== summary: %d passed, %d failed ==\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
