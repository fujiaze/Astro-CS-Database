/* weight_chain_selfcheck.cpp — 权重链独立合成 Oracle（C++ 侧）
 *
 * 独立性边界:
 *   - 期望值在本 TU 内**独立复算**（走 sigma_F 路径 1/sigma_F^2 与独立写的
 *     双线性插值），不调用 weight_chain 内部实现，只调用其公开 API 对拍；
 *   - Python 侧另有 oracle/weight_chain_oracle.py 以不同语言独立复算同一公式。
 *
 * 正例: 注入已知 SNR ⇒ 权重可复算（相对容差 1e-12）。
 * 负例: SNR 缺失/非有限/非正、F_ref 非法、稀疏层损坏/越界、SNR 语义冒充、
 *       legacy_allow_weight_fallback ⇒ 全部 fail-closed 且不得静默退化为等权。
 *
 * 构建（前台统一）: 见 oracle/CMakeLists.txt。
 */
#include "astrocs/v6/weight_chain.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using astrocs::v6::p2weight::FrameSnrKind;
using astrocs::v6::p2weight::FrameWeightInput;
using astrocs::v6::p2weight::SparseSnrLayer;
using astrocs::v6::p2weight::SparseSnrPoint;
using astrocs::v6::p2weight::WeightChainPolicy;
using astrocs::v6::p2weight::WeightClosure;
using astrocs::v6::p2weight::WeightChainResult;

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool cond, const std::string& what) {
  if (cond) { ++g_pass; std::printf("  [PASS] %s\n", what.c_str()); }
  else { ++g_fail; std::printf("  [FAIL] %s\n", what.c_str()); }
}

bool close(double a, double b, double rtol = 1e-12) {
  if (!std::isfinite(a) || !std::isfinite(b)) return false;
  const double d = std::fabs(a - b);
  return d <= rtol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

/* ---- 独立复算路径（不调用被测实现） ---- */
double oracle_weight_from_snr(double snr, double fref) {
  const double sigma_f = fref / snr;      /* sigma_F = F_ref / SNR */
  return 1.0 / (sigma_f * sigma_f);       /* w = 1/sigma_F^2 */
}
double oracle_bilinear(const SparseSnrLayer& L, double x, double y) {
  const double gx = (x - L.x0) / L.dx;
  const double gy = (y - L.y0) / L.dy;
  int i0 = (int)std::floor(gx);
  int j0 = (int)std::floor(gy);
  if (i0 > L.nx - 2) i0 = L.nx - 2;
  if (j0 > L.ny - 2) j0 = L.ny - 2;
  if (i0 < 0) i0 = 0;
  if (j0 < 0) j0 = 0;
  const double fx = gx - i0, fy = gy - j0;
  const std::size_t nxl = (std::size_t)L.nx;
  const double v00 = L.points[(std::size_t)j0 * nxl + (std::size_t)i0].snr;
  const double v10 = L.points[(std::size_t)j0 * nxl + (std::size_t)i0 + 1].snr;
  const double v01 = L.points[(std::size_t)(j0 + 1) * nxl + (std::size_t)i0].snr;
  const double v11 = L.points[(std::size_t)(j0 + 1) * nxl + (std::size_t)i0 + 1].snr;
  return (1 - fx) * (1 - fy) * v00 + fx * (1 - fy) * v10 + (1 - fx) * fy * v01 + fx * fy * v11;
}

FrameWeightInput mk(const std::string& id, double snr) {
  FrameWeightInput f;
  f.frame_id = id;
  f.kind = FrameSnrKind::kFluxTypeUnweightedSnr;
  f.has_frame_snr = true;
  f.frame_snr = snr;
  return f;
}

/* 2x2 规则网格: 行主序 j*nx+i, x0=y0=0, dx=dy=1 */
SparseSnrLayer grid2x2() {
  SparseSnrLayer L;
  L.present = true;
  L.regular_grid = true;
  L.nx = 2; L.ny = 2;
  L.x0 = 0.0; L.y0 = 0.0; L.dx = 1.0; L.dy = 1.0;
  L.points = {{0, 0, 1.0}, {1, 0, 1.2}, {0, 1, 0.8}, {1, 1, 1.0}};
  return L;
}

}  /* namespace */

int main() {
  std::printf("== weight_chain selfcheck (independent C++ oracle) ==\n");

  /* ---------- 正例 1: 标量换算 w = SNR^2/F_ref^2 = 1/sigma_F^2 ---------- */
  {
    std::printf("[positive] scalar w = SNR^2/F_ref^2\n");
    const double fref = 1000.0;
    const double snrs[3] = {200.0, 100.0, 50.0};
    for (double s : snrs) {
      double w = 0.0; std::string e;
      const bool ok = astrocs::v6::p2weight::weight_from_snr(s, fref, &w, &e);
      check(ok, "weight_from_snr accepted finite positive SNR");
      check(close(w, oracle_weight_from_snr(s, fref)),
            "w matches independent 1/sigma_F^2 oracle");
      check(close(w, (s * s) / (fref * fref)), "w matches SNR^2/F_ref^2 form");
    }
  }

  /* ---------- 正例 2: 无稀疏层多帧权重 ---------- */
  {
    std::printf("[positive] multi-frame without sparse layer\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0), mk("f1", 100.0), mk("f2", 50.0)};
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(r.ok && r.weight_chain_closed && r.production_allowed,
          "chain closed and production allowed");
    check(r.closure == WeightClosure::kClosed, "closure token = closed");
    check(r.weight_source == "frame_snr", "weight_source = frame_snr");
    check(r.weights.size() == 3, "one weight per input frame");
    bool all = true;
    for (std::size_t i = 0; i < frames.size(); ++i)
      all = all && close(r.weights[i], oracle_weight_from_snr(frames[i].frame_snr, fref));
    check(all, "all weights match independent oracle");
    /* SNR_combined^2 = Sum SNR_k^2 (independent frames) */
    double sum_snr2 = 0.0;
    for (const auto& f : frames) sum_snr2 += f.frame_snr * f.frame_snr;
    double sum_w = 0.0;
    for (double w : r.weights) sum_w += w;
    check(close(sum_w * fref * fref, sum_snr2, 1e-12),
          "SNR_combined^2 = Sum SNR_k^2 identity holds");
  }

  /* ---------- 正例 3: 稀疏层 帧级×帧内（双线性） ---------- */
  {
    std::printf("[positive] sparse layer composition (frame x intra)\n");
    const double fref = 1000.0;
    SparseSnrLayer L = grid2x2();
    std::vector<FrameWeightInput> frames;
    FrameWeightInput a = mk("f0", 200.0); a.sparse = &L; a.x = 0.5; a.y = 0.5;
    FrameWeightInput b = mk("f1", 100.0); b.sparse = &L; b.x = 0.25; b.y = 0.5;
    frames = {a, b};
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(r.ok && r.weight_chain_closed, "sparse chain closed");
    check(r.weight_source == "frame_snr_x_sparse_snr", "weight_source = frame_snr_x_sparse_snr");
    const double intra0 = oracle_bilinear(L, 0.5, 0.5);
    const double intra1 = oracle_bilinear(L, 0.25, 0.5);
    check(close(r.intra_snr[0], intra0), "intra[0] matches independent bilinear oracle");
    check(close(r.intra_snr[1], intra1), "intra[1] matches independent bilinear oracle");
    check(close(r.actual_snr[0], 200.0 * intra0), "actual SNR = frame x intra (f0)");
    check(close(r.weights[0], oracle_weight_from_snr(200.0 * intra0, fref)),
          "weight[0] from composed SNR matches oracle");
    check(r.sparse_operator_ids[0] == "bilinear_regular_grid_v1",
          "reconstruction operator id recorded");
    check(r.sparse_node_residual[0] <= 1e-9, "node reproduction residual ~ 0");
  }

  /* ---------- 负例 1: 帧级 SNR 缺失 ---------- */
  {
    std::printf("[negative] missing frame SNR -> fail-closed\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].has_frame_snr = false;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok && !r.weight_chain_closed && !r.production_allowed,
          "missing SNR is fail-closed");
    check(r.closure == WeightClosure::kUnclosedMissingFrameSnr, "closure = missing_frame_snr");
    check(r.weights.empty(), "no weights emitted on failure");
    check(r.error.find("missing") != std::string::npos, "explicit reason reported");
  }

  /* ---------- 负例 2: SNR 非有限/非正 ---------- */
  {
    std::printf("[negative] non-finite / non-positive SNR -> fail-closed\n");
    const double fref = 1000.0;
    const double bad[3] = {std::nan(""), INFINITY, -3.0};
    for (double b : bad) {
      std::vector<FrameWeightInput> frames = {mk("f0", b)};
      const WeightChainResult r =
          astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
      check(!r.ok && r.closure == WeightClosure::kUnclosedInvalidFrameSnr,
            "invalid SNR is fail-closed (invalid_frame_snr)");
    }
  }

  /* ---------- 负例 3: F_ref 非法 ---------- */
  {
    std::printf("[negative] invalid F_ref -> fail-closed\n");
    const double bad[3] = {0.0, -1.0, std::nan("")};
    for (double b : bad) {
      std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
      const WeightChainResult r =
          astrocs::v6::p2weight::compute_inverse_variance_weights(frames, b);
      check(!r.ok && r.closure == WeightClosure::kUnclosedInvalidReferenceFlux,
            "invalid F_ref is fail-closed (invalid_reference_flux)");
    }
  }

  /* ---------- 负例 4: 稀疏层存在但损坏/空 ---------- */
  {
    std::printf("[negative] corrupt/empty sparse layer -> fail-closed, no frame fallback\n");
    const double fref = 1000.0;
    SparseSnrLayer empty;
    empty.present = true; /* present but empty */
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].sparse = &empty; frames[0].x = 0.5; frames[0].y = 0.5;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok, "present-but-empty layer is fail-closed");
    check(r.closure == WeightClosure::kUnclosedSparseLayerUnreconstructible,
          "closure = sparse_layer_unreconstructible");
    check(r.weights.empty(), "no silent fallback to frame-level weights");
  }

  /* ---------- 负例 5: 稀疏层越界 ---------- */
  {
    std::printf("[negative] sparse layer out-of-domain -> fail-closed\n");
    const double fref = 1000.0;
    SparseSnrLayer L = grid2x2();
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].sparse = &L; frames[0].x = 5.0; frames[0].y = 0.5;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok && r.closure == WeightClosure::kUnclosedSparseLayerUnreconstructible,
          "out-of-domain is fail-closed");
    check(r.error.find("fallback") != std::string::npos,
          "explicitly states frame-level fallback forbidden");
  }

  /* ---------- 负例 6: SNR 语义冒充 ---------- */
  {
    std::printf("[negative] wrong SNR semantics (quality weight) -> fail-closed\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].kind = FrameSnrKind::kRelativeQualityWeight;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    check(!r.ok && r.closure == WeightClosure::kUnclosedWrongSnrSemantics,
          "relative quality weight rejected");
  }

  /* ---------- 负例 7: legacy_allow_weight_fallback 显式请求仍 fail-closed ---------- */
  {
    std::printf("[negative] legacy_allow_weight_fallback -> explicit unclosed, no false green\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0)};
    frames[0].has_frame_snr = false;
    WeightChainPolicy p;
    p.legacy_allow_weight_fallback = true;
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref, p);
    check(!r.ok && !r.weight_chain_closed && !r.production_allowed,
          "legacy fallback never reports success");
    check(r.closure == WeightClosure::kUnclosedLegacyFallbackRejected,
          "closure = legacy_fallback_rejected");
    check(r.error.find("NOT closed") != std::string::npos ||
              r.error.find("未闭合") != std::string::npos,
          "explicit 'weight chain not closed' message");
    check(r.weights.empty() && r.legacy_equal_weight_used,
          "no scientific weights; diagnostic equal weights flagged only");
  }

  /* ---------- 负例 8: 显式等权基线不得冒充闭合 ---------- */
  {
    std::printf("[negative] explicit equal-weight baseline is not a closure\n");
    const WeightChainResult r = astrocs::v6::p2weight::make_equal_weight_baseline(3);
    check(!r.ok && !r.weight_chain_closed && !r.production_allowed,
          "baseline never reports a closed chain");
    check(r.closure == WeightClosure::kBaselineEqualWeight, "closure = baseline_equal_weight");
    check(r.weights.size() == 3, "baseline still exposes diagnostic weights");
  }

  /* ---------- 非空真检查: 等权 != 逆方差权重 ---------- */
  {
    std::printf("[non-vacuity] equal weight differs from inverse-variance weight\n");
    const double fref = 1000.0;
    std::vector<FrameWeightInput> frames = {mk("f0", 200.0), mk("f1", 50.0)};
    const WeightChainResult r =
        astrocs::v6::p2weight::compute_inverse_variance_weights(frames, fref);
    bool differs = false;
    for (double w : r.weights) differs = differs || !close(w, 1.0, 1e-9);
    check(differs, "inverse-variance weights are not all 1.0 (test is non-vacuous)");
  }

  std::printf("\n== summary: %d passed, %d failed ==\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
