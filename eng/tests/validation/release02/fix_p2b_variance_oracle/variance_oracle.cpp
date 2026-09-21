/* variance_oracle.cpp — RELEASE-02 FIX-P2b 独立 Oracle（能红能绿）
 *
 * 目的: 证明
 *   (1) 残差制造者 Var(c)=PΣPᵀ 与错误形式 σ²+Var(ĝ) 在**生产尺度**下可判别；
 *   (2) 权重序：w=1/σ_raw²（现行）与 w=1/Var(corrected)（正确, 含 ÷g²）序不同；
 *   (3) 全部退化输入 fail-closed（缺 gain/非法 gain/非正方差/负 σ²）。
 *
 * 独立性: 期望值在本 TU 内以**闭式**独立复算（不调用被测实现），只调用公开 API 对拍。
 * 生产尺度: σ = 4.22759e10 ADU（L4 p2_samples 观测 uncertainty 中位），
 *           σ² = 1.78725e21（control_variance 中位）；N = 49（L4 帧数）。
 * 构建: 见 build_oracle.sh（g++，链接 variance_propagation.cpp + weight_chain.cpp）。
 */
#include "astrocs/v6/variance_propagation.h"
#include "astrocs/v6/weight_chain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using astrocs::v6::p2var::corrected_pixel_variance;
using astrocs::v6::p2var::HatRow;
using astrocs::v6::p2var::mean_model_naive_over_correct;
using astrocs::v6::p2var::naive_variance;
using astrocs::v6::p2var::normalized_weight_hat_row;
using astrocs::v6::p2var::residual_maker_variance;
using astrocs::v6::p2var::weight_from_variance;
using astrocs::v6::p2weight::compute_inverse_variance_weights;
using astrocs::v6::p2weight::FrameSnrKind;
using astrocs::v6::p2weight::FrameWeightInput;
using astrocs::v6::p2weight::WeightChainPolicy;
using astrocs::v6::p2weight::WeightChainResult;
using astrocs::v6::p2weight::WeightClosure;

namespace {
int g_fail = 0, g_pass = 0;
void check(bool c, const std::string& w) {
  if (c) { ++g_pass; std::printf("  [PASS] %s\n", w.c_str()); }
  else { ++g_fail; std::printf("  [FAIL] %s\n", w.c_str()); }
}
bool close(double a, double b, double rtol = 1e-12) {
  if (!std::isfinite(a) || !std::isfinite(b)) return false;
  return std::fabs(a - b) <= rtol * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}
/* Kendall τ（0 基序，无并列）。 */
double kendall_tau(const std::vector<double>& a, const std::vector<double>& b) {
  const int n = (int)a.size();
  int conc = 0, disc = 0;
  for (int i = 0; i < n; ++i)
    for (int j = i + 1; j < n; ++j) {
      const double da = a[i] - a[j], db = b[i] - b[j];
      if (da * db > 0) ++conc; else if (da * db < 0) ++disc;
    }
  return (double)(conc - disc) / (double)(conc + disc);
}
const double kSigma = 4.22759e10;   /* L4 生产尺度 uncertainty 中位 [ADU] */
}  /* namespace */

int main() {
  std::printf("== FIX-P2b variance Oracle (production scale sigma=%.6g, N=49) ==\n", kSigma);

  /* ── T1: ĝ=ȳ 均值模型, 残差制造者 vs 朴素 ────────────────────────────── */
  for (std::size_t N : {std::size_t(8), std::size_t(49)}) {
    std::vector<double> w(N, 1.0);
    std::vector<double> s2(N, kSigma * kSigma);
    HatRow h;
    std::string err;
    bool ok = normalized_weight_hat_row(w.data(), N, 0, true, &h, &err);
    check(ok, "T1 N=" + std::to_string(N) + " build include-self hat row");
    double vc = 0.0, vn = 0.0;
    ok = residual_maker_variance(h, s2.data(), N, 0, &vc, &err);
    check(ok, "T1 N=" + std::to_string(N) + " residual_maker_variance ok");
    ok = naive_variance(h, s2.data(), N, 0, &vn, &err);
    check(ok, "T1 N=" + std::to_string(N) + " naive_variance ok");
    const double closed_correct = (kSigma * kSigma) * (1.0 - 1.0 / (double)N);
    const double closed_naive = (kSigma * kSigma) * (1.0 + 1.0 / (double)N);
    /* 故障注入（能红证明）: P2B_ORACLE_FAULT=naive ⇒ 用朴素值冒充"被测正确
       实现"，正确的闭式断言必然判红、进程 rc=1。 */
    const char* fault = std::getenv("P2B_ORACLE_FAULT");
    const bool inject_naive = fault != nullptr && std::string(fault) == "naive";
    const double vc_impl = inject_naive ? vn : vc;
    check(close(vc_impl, closed_correct, 1e-12),
          "T1 N=" + std::to_string(N) + " correct == sigma^2(1-1/N) [GREEN]");
    check(close(vn, closed_naive, 1e-12),
          "T1 N=" + std::to_string(N) + " naive   == sigma^2(1+1/N) [RED]");
    check(!close(vn, closed_correct, 1e-6),
          "T1 N=" + std::to_string(N) + " RED: 若实现返回朴素值, correct 断言必红");
    check(!close(vc, closed_naive, 1e-6),
          "T1 N=" + std::to_string(N) + " RED: 若实现返回正确值, naive 断言必红");
    check(!close(vc, vn, 1e-6),
          "T1 N=" + std::to_string(N) + " naive != correct (判别力)");
    const double ratio = vn / vc;
    check(close(ratio, mean_model_naive_over_correct(N), 1e-12),
          "T1 N=" + std::to_string(N) + " naive/correct ratio closed-form");
    if (N == 8)
      check(std::fabs(ratio - 1.2857142857142858) < 1e-12,
            "T1 N=8 naive overestimates 1.2857x (报告值 1.29x)");
  }

  /* ── T2: 生产尺度控制级残差制造者闭式 1/w_k + 1/W_-k ((c) 排除自身) ── */
  {
    /* 8 帧, control_ivar 各不同（生产量级 ~5.6e-22）。σ²=1/w ⇒ 闭式。 */
    const double civ[8] = {5.59517e-22, 3.1e-22, 7.2e-22, 4.0e-22,
                           2.75e-22, 6.1e-22, 3.6e-22, 5.0e-22};
    std::vector<double> w(civ, civ + 8), s2(8);
    for (int i = 0; i < 8; ++i) s2[i] = 1.0 / civ[i];
    double W = 0.0;
    for (double v : w) W += v;
    HatRow h; std::string err;
    bool ok = normalized_weight_hat_row(w.data(), 8, 2, false, &h, &err);
    check(ok, "T2 exclude-self hat row ok");
    double v = 0.0;
    ok = residual_maker_variance(h, s2.data(), 8, 2, &v, &err);
    const double closed = 1.0 / civ[2] + 1.0 / (W - civ[2]);
    check(ok && close(v, closed, 1e-12),
          "T2 exclude-self Var == 1/w_k + 1/W_-k (control-level residual maker)");
  }

  /* ── T3: 权重序 A>C>B>D（现行 w=1/σ_raw²）vs A>B>C>D（正确 ÷g²）──── */
  {
    const double g[4] = {1.0, 2.0, 0.8, 1.0};
    const double sraw[4] = {1.0, 2.6, 2.0, 5.0};
    std::vector<double> w_raw(4), w_correct(4);
    for (int k = 0; k < 4; ++k) {
      w_raw[k] = 1.0 / (sraw[k] * sraw[k]);
      /* 正确: Var(corrected)=σ²/g²（H=∅ ⇒ P=I）→ w=g²/σ² */
      double s2k = (sraw[k] * kSigma) * (sraw[k] * kSigma);
      HatRow empty; std::string err; double var = 0.0;
      const bool ok = corrected_pixel_variance(empty, &s2k, 1, 0, g[k], 0.0, &var, &err);
      double wc = 0.0;
      const bool ok2 = ok && weight_from_variance(var, &wc, &err);
      w_correct[k] = ok2 ? wc * (kSigma * kSigma) : std::nan("");  /* 归一化回 σ=1 尺度 */
    }
    const bool raw_order = (w_raw[0] > w_raw[2] && w_raw[2] > w_raw[1] && w_raw[1] > w_raw[3]);
    const bool cor_order = (w_correct[0] > w_correct[1] && w_correct[1] > w_correct[2] &&
                            w_correct[2] > w_correct[3]);
    check(raw_order, "T3 现行 w=1/sigma_raw^2 序 = A>C>B>D");
    check(cor_order, "T3 正确 w=1/Var(corrected) 序 = A>B>C>D [GREEN]");
    check((w_raw[1] < w_raw[2]) && (w_correct[1] > w_correct[2]),
          "T3 权重序翻转：B/C 相对序在 raw 与 correct 间反转");
    const double tau = kendall_tau(w_raw, w_correct);
    check(std::fabs(tau - 2.0 / 3.0) < 1e-12,
          "T3 Kendall tau = 0.667 (q2-snr-smooth §2)");
    const double ab_raw = w_raw[0] / w_raw[1];
    const double ab_cor = w_correct[0] / w_correct[1];
    check(close(ab_raw, 6.76, 1e-9) && close(ab_cor, 1.69, 1e-2),
          "T3 A/B ratio: raw 6.76 vs correct 1.69 (g=2 帧被压低 4x 的修正)");
    std::printf("      w_raw=[%.6g %.6g %.6g %.6g] w_correct=[%.6g %.6g %.6g %.6g]\n",
                w_raw[0], w_raw[1], w_raw[2], w_raw[3],
                w_correct[0], w_correct[1], w_correct[2], w_correct[3]);
  }

  /* ── T4: weight_chain gain 配对 w = SNR²/F_ref²·g_k² ─────────────────── */
  {
    const double Fref = 1.0;
    const double g[4] = {1.0, 2.0, 0.8, 1.0};
    const double sraw[4] = {1.0, 2.6, 2.0, 5.0};
    std::vector<FrameWeightInput> in(4);
    for (int k = 0; k < 4; ++k) {
      in[k].kind = FrameSnrKind::kFluxTypeUnweightedSnr;
      in[k].has_frame_snr = true;
      in[k].frame_snr = Fref / sraw[k];   /* SNR = F_ref/σ_F */
      in[k].gain = &g[k];
    }
    WeightChainPolicy pol; pol.require_frame_gain = true;
    const WeightChainResult r = compute_inverse_variance_weights(in, Fref, pol);
    check(r.ok && r.weight_chain_closed, "T4 gain chain closed");
    bool all = r.ok && r.weights.size() == 4;
    for (int k = 0; k < 4 && all; ++k) {
      const double want = (g[k] * g[k]) / (sraw[k] * sraw[k]);
      all = close(r.weights[k], want, 1e-12);
    }
    check(all, "T4 w == SNR^2/F_ref^2 * g_k^2 for all frames");
    check(r.weights[0] > r.weights[1] && r.weights[1] > r.weights[2] &&
          r.weights[2] > r.weights[3],
          "T4 chain weight order A>B>C>D");
  }

  /* ── T5: fail-closed 负例 ────────────────────────────────────────────── */
  {
    std::string err;
    /* 缺 gain 但声明要求 */
    std::vector<FrameWeightInput> in(2);
    for (auto& x : in) {
      x.kind = FrameSnrKind::kFluxTypeUnweightedSnr;
      x.has_frame_snr = true;
      x.frame_snr = 10.0;
      x.gain = nullptr;
    }
    WeightChainPolicy pol; pol.require_frame_gain = true;
    WeightChainResult r = compute_inverse_variance_weights(in, 1.0, pol);
    check(!r.ok && r.closure == WeightClosure::kUnclosedMissingGain,
          "T5 require_gain + missing gain -> kUnclosedMissingGain");
    /* 非法 gain */
    double badg = -1.0;
    in[0].gain = &badg; in[1].gain = &badg;
    r = compute_inverse_variance_weights(in, 1.0, pol);
    check(!r.ok && r.closure == WeightClosure::kUnclosedInvalidGain,
          "T5 invalid gain -> kUnclosedInvalidGain");
    /* 非正方差 → 权重 fail-closed */
    double w = 0.0;
    check(!weight_from_variance(0.0, &w, &err), "T5 Var=0 -> weight fail-closed");
    check(!weight_from_variance(-1.0, &w, &err), "T5 Var<0 -> weight fail-closed");
    /* 负 σ² → 残差制造者 fail-closed */
    double s2[2] = {1.0, -1.0};
    double w2[2] = {1.0, 1.0};
    HatRow h; normalized_weight_hat_row(w2, 2, 0, true, &h, &err);
    double v = 0.0;
    check(!residual_maker_variance(h, s2, 2, 0, &v, &err),
          "T5 negative sigma^2 -> residual maker fail-closed");
    /* 单帧 stack 排除自身 → fail-closed（无跨帧耦合） */
    double w1[1] = {1.0};
    check(!normalized_weight_hat_row(w1, 1, 0, false, &h, &err),
          "T5 exclude-self single-frame stack -> fail-closed");
  }

  std::printf("\n== P2b Oracle: pass=%d fail=%d ==\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
