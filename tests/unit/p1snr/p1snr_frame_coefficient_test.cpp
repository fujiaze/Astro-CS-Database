// ============================================================================
// p1snr_frame_coefficient_test.cpp — P33-COEF 帧级单一 SNR 系数回归锁
//
// 被测面 = 生产目标 astrocs_phase1_noise 的 lib/phase1/noise/
//   snr_frame_coefficient.cpp (DATA-P1-SNR-COEF/1)。
//   value      : 系数 = median(SNR_F), 与 sci.median_snr 一致; 分位/备选估计量正确
//   robust     : 通量匹配中位数/暗亮四分位比对样本选择敏感 -> 证其非恒真
//   determinism: 输入顺序打乱 -> 逐位一致 (确定性聚合)
//   negative   : valid=false / 全非有限 -> fail-closed 不产系数; 且
//                「SNR 与通量无关」构造下 faint_over_bright ~ 1 (对照 ~ 0.25)
//   serialize  : JSON schema/字段/往返
// ============================================================================
#include "snr_frame_coefficient.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using nlohmann::json;
using astrocs::phase1::SnrFrameCoefficient;
using astrocs::phase1::SnrFrameCoefficientConfig;
using astrocs::phase1::SnrFrameScienceResult;

static int g_fail = 0;
static int g_check = 0;
#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    ++g_check;                                                                 \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s -- %s\n", __FILE__, __LINE__,        \
                   #cond, (msg));                                              \
      ++g_fail;                                                                \
    } else {                                                                   \
      std::printf("ok  : %s\n", (msg));                                        \
    }                                                                          \
  } while (0)

static SnrFrameScienceResult make_sci(const std::vector<double>& snr) {
  SnrFrameScienceResult s;
  s.valid = true;
  s.n_input = static_cast<int>(snr.size());
  s.n_used = static_cast<int>(snr.size());
  s.snr_f = snr;
  s.sigma_f_adu.assign(snr.size(), 1.0);
  s.local_snr.assign(snr.size(), 1.0);
  std::vector<double> t = snr;
  std::sort(t.begin(), t.end());
  const double m = t.empty() ? 0.0 : t[t.size() / 2];
  s.median_snr = m; s.snr_phot = m; s.median_source_snr = m;
  return s;
}
// 通量: 与 SNR 同序 (对数正态) —— 用确定性构造避免随机
static std::vector<double> flux_of(const std::vector<double>& snr) {
  std::vector<double> f(snr.size());
  for (size_t i = 0; i < snr.size(); ++i) f[i] = 10.0 + 5.0 * static_cast<double>(i);
  return f;
}
// SNR ∝ 通量 (背景受限): snr_i = c * flux_i
static std::vector<double> snr_lin_flux(const std::vector<double>& f, double c) {
  std::vector<double> s(f.size());
  for (size_t i = 0; i < f.size(); ++i) s[i] = c * f[i];
  return s;
}

int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "all";
  const std::vector<double> f = flux_of(std::vector<double>(40, 0.0));
  const std::vector<double> snr = snr_lin_flux(f, 2.0);  // 20,30,...,215
  std::vector<double> sorted = snr;
  std::sort(sorted.begin(), sorted.end());
  const double med = 0.5 * (sorted[19] + sorted[20]);

  if (mode == "all" || mode == "value") {
    SnrFrameCoefficient c = astrocs::phase1::compute_snr_frame_coefficient(
        make_sci(snr), f);
    CHECK(c.valid, "value: 正常输入 -> valid");
    CHECK(c.value == med && c.median_snr_f == med,
          "value: value == median_snr_f == median(SNR_F)");
    CHECK(c.estimator == "median_snr_f", "value: estimator=median_snr_f");
    CHECK(c.n_sources == 40, "value: n_sources=40");
    CHECK(c.p16_snr_f < c.value && c.value < c.p84_snr_f,
          "value: p16 < value < p84 (离散度非退化)");
    // 备选估计量: 线性场 + 均匀通量 -> 几何均值 < 算术中位数
    CHECK(c.geomean_snr_f > 0.0 && c.geomean_snr_f < c.value,
          "value: 几何均值 < 中位数 (线性场), 且有限");
    CHECK(c.trimmed10_mean_snr_f > 0.0, "value: 截尾均值有限");
    // 通量 10..205 线性; q25/q75 落在 55/160 附近 => 暗/亮中位数比 ~ 32.5/182.5
    CHECK(std::fabs(c.faint_over_bright - 0.178) < 0.03,
          "value: faint/bright ~ 暗亮四分位通量比 (线性场, 强样本依赖)");
  }

  if (mode == "all" || mode == "robust") {
    // (1) SNR ∝ 通量: 通量匹配中位数 << 全样本中位数 (强样本依赖)
    SnrFrameCoefficient a = astrocs::phase1::compute_snr_frame_coefficient(
        make_sci(snr), f);
    // (2) SNR 与通量无关 (置换): 通量匹配中位数 ~ 全局中位数, faint/bright ~ 1
    std::vector<double> snr_ind(snr.size());
    for (size_t i = 0; i < snr.size(); ++i) snr_ind[i] = 100.0 + 3.0 * static_cast<double>(i % 7);
    std::vector<double> fin = f;
    std::reverse(fin.begin(), fin.end());   // 通量与 SNR 反序 => 无相关
    SnrFrameCoefficient b = astrocs::phase1::compute_snr_frame_coefficient(
        make_sci(snr_ind), fin);
    CHECK(a.faint_over_bright < 0.20,
          "robust: 线性场 faint/bright 显著 < 1 (样本依赖被检出)");
    CHECK(b.faint_over_bright > 0.60,
          "robust: 无关场 faint/bright ~ 1 (对照: 指标非恒真)");
    CHECK(a.n_flux_matched > 0 && a.flux_matched_median_snr_f > 0.0,
          "robust: 通量匹配窗有样本且中位数为正");
    // 截尾均值应介于 p16..p84 之间
    CHECK(a.trimmed10_mean_snr_f > a.p16_snr_f &&
              a.trimmed10_mean_snr_f < a.p84_snr_f,
          "robust: 截尾均值落在 p16..p84 内");
  }

  if (mode == "all" || mode == "determinism") {
    SnrFrameCoefficient a = astrocs::phase1::compute_snr_frame_coefficient(
        make_sci(snr), f);
    // 逆序输入 (SNR 与 flux 同步逆序) -> 逐位一致
    std::vector<double> s2 = snr, f2 = f;
    std::reverse(s2.begin(), s2.end());
    std::reverse(f2.begin(), f2.end());
    SnrFrameCoefficient b = astrocs::phase1::compute_snr_frame_coefficient(
        make_sci(s2), f2);
    CHECK(a.value == b.value && a.median_snr_f == b.median_snr_f &&
              a.p16_snr_f == b.p16_snr_f && a.p84_snr_f == b.p84_snr_f &&
              a.geomean_snr_f == b.geomean_snr_f &&
              a.trimmed10_mean_snr_f == b.trimmed10_mean_snr_f &&
              a.flux_matched_median_snr_f == b.flux_matched_median_snr_f &&
              a.faint_over_bright == b.faint_over_bright &&
              a.n_sources == b.n_sources,
          "determinism: 输入顺序打乱 -> 全部字段逐位一致");
  }

  if (mode == "all" || mode == "negative") {
    // (1) 上游 science invalid -> fail-closed, 不产系数
    SnrFrameScienceResult bad = make_sci(snr);
    bad.valid = false; bad.reason = "degenerate input";
    SnrFrameCoefficient c1 = astrocs::phase1::compute_snr_frame_coefficient(bad, f);
    CHECK(!c1.valid && !c1.reason.empty() && c1.value == 0.0,
          "negative: 上游 invalid -> valid=false 且带原因 (fail-closed)");
    // (2) 全非有限 SNR -> fail-closed
    SnrFrameScienceResult nn = make_sci(snr);
    for (double& v : nn.snr_f) v = std::nan("");
    SnrFrameCoefficient c2 = astrocs::phase1::compute_snr_frame_coefficient(nn, f);
    CHECK(!c2.valid && c2.reason.find("finite") != std::string::npos,
          "negative: 无有限正 SNR_F -> fail-closed (reason 指明)");
    // (3) 空样本 -> fail-closed (不静默回填 1.0)
    SnrFrameCoefficient c3 = astrocs::phase1::compute_snr_frame_coefficient(
        make_sci({}), {});
    CHECK(!c3.valid && c3.value == 0.0,
          "negative: 空样本 -> 不产系数 (禁 1.0 伪装)");
  }

  if (mode == "all" || mode == "serialize") {
    SnrFrameCoefficient c = astrocs::phase1::compute_snr_frame_coefficient(
        make_sci(snr), f);
    const std::string txt = astrocs::phase1::snr_frame_coefficient_to_json(c);
    json j = json::parse(txt);
    CHECK(j.value("schema", std::string()) == "DATA-P1-SNR-COEF/1",
          "serialize: schema=DATA-P1-SNR-COEF/1");
    CHECK(j.value("estimator", std::string()) == "median_snr_f",
          "serialize: estimator=median_snr_f");
    CHECK(j.contains("value") && j["value"].is_number() &&
              std::fabs(j["value"].get<double>() - c.value) <= 1e-12 * c.value,
          "serialize: value 往返一致");
    CHECK(j.contains("sample") && j["sample"].get<std::string>().find("flux>0") !=
              std::string::npos,
          "serialize: 落盘样本定义 (可审计)");
    CHECK(j.contains("dispersion") && j["dispersion"].contains("p16") &&
              j["dispersion"].contains("p84"),
          "serialize: 帧内离散度落盘");
    CHECK(j.contains("alternatives") &&
              j["alternatives"].contains("flux_matched_median_snr_f") &&
              j["alternatives"].contains("geometric_mean_snr_f"),
          "serialize: 备选估计量落盘 (选型可追溯)");
    CHECK(j.contains("sample_sensitivity") &&
              j["sample_sensitivity"].contains("faint_over_bright"),
          "serialize: 样本敏感性落盘");
    // invalid 时 value 必须为 null (不得 0/1 伪装)
    SnrFrameScienceResult bad = make_sci(snr);
    bad.valid = false; bad.reason = "x";
    json jb = json::parse(astrocs::phase1::snr_frame_coefficient_to_json(
        astrocs::phase1::compute_snr_frame_coefficient(bad, f)));
    CHECK(!jb["valid"].get<bool>() && jb["value"].is_null(),
          "serialize: invalid -> value=null (不伪装)");
  }

  std::printf("%s: checks=%d fail=%d\n", mode.c_str(), g_check, g_fail);
  return g_fail == 0 ? 0 : 1;
}
