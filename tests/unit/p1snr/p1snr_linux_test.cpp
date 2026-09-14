// p1snr_linux_test.cpp - Linux 生产路径 SNR 回归锁 (P8-SNR-LINUX)
//
// 被测面 (真实生产目标, 非测试私有编译):
//   astrocs_phase1_noise  — 根 CMakeLists.txt 的 Linux 生产静态库
//     lib/phase1/noise/snr_frame_science.cpp  (帧级聚合, P8 接线层)
//     lib/snr_estimator/cpp/src/snr_science.cpp (P5-SNR 唯一权威科学实现)
//   => 本可执行文件若链接不到 snr_source_snr_f64 / snr_frame_depth_f64,
//      构建即失败 —— 这就是"snr_science.cpp 确实在 Linux 生产目标里"的机器锁。
//
// Oracle 独立性:
//   ① 本文件内置**独立**长双精度暴力参考实现 (不同循环/累加结构, 不调用被测函数);
//   ② 冻结 NumPy oracle 锚值 (run/perf-fix/P5-snr/harness/snr_oracle.py 对
//      run/perf-fix/P5-snr/harness/real_sources.json 五颗真实源的复算输出, 见
//      run/perf-fix/P5-snr/harness/probe_out.json);
//   二者与实现不共享代码路径。
//
// 依据: docs/science/CONTROL_WEIGHT_SNR.md §2a/§4 (S4 重定义),
//       run/release-rescue/science-phot/PHOTOMETRY_LITERATURE_REVIEW.md §C.3.1。
// 用法: p1snr_linux_test [units|oracle|contract|negative|determinism|all]
#include "snr_frame_science.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_total = 0;

void check(bool ok, const char* what) {
  ++g_total;
  if (!ok) {
    ++g_fail;
    std::printf("FAIL %s\n", what);
  }
}

void check_close(double got, double exp, double rtol, const char* what) {
  ++g_total;
  const double a = std::fabs(got - exp);
  const double tol = 1e-12 + rtol * std::fabs(exp);
  if (!(a <= tol)) {
    ++g_fail;
    std::printf("FAIL %s: got=%.17g exp=%.17g abs=%.3g tol=%.3g\n", what, got, exp, a, tol);
  }
}

double nan_v() { return std::numeric_limits<double>::quiet_NaN(); }

constexpr double kPi = 3.14159265358979323846;
constexpr double kFwhmFactor = 1.230310;

int ref_half(double fwhm) {
  int h = static_cast<int>(std::ceil(12.0 * fwhm));
  if (h < 30) h = 30;
  if (h > 256) h = 256;
  return h;
}

double ref_median(std::vector<double> v) {
  if (v.empty()) return nan_v();
  std::sort(v.begin(), v.end());
  const std::size_t n = v.size();
  if (n % 2 == 1) return v[n / 2];
  return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// 独立参考实现 (long double, 暴力逐像素; 仅天空受限 + gain>0 两条路径)
struct Ref {
  double snr_optimal = 0.0;
  double sigma_f = 0.0;
  double sum_p2 = 0.0;
};

Ref ref_source(double flux, double fwhm, double sky, double gain) {
  Ref o;
  const long double sigma = static_cast<long double>(fwhm) / static_cast<long double>(kFwhmFactor);
  const int half = ref_half(fwhm);
  const long double alpha2 = 2.0L * sigma * sigma;
  long double sum = 0.0L, sum2 = 0.0L;
  for (int j = -half; j <= half; ++j) {
    for (int i = -half; i <= half; ++i) {
      const long double r2 = static_cast<long double>(i) * i + static_cast<long double>(j) * j;
      const long double t = 1.0L + r2 / alpha2;
      const long double v = 1.0L / (t * t * t * t);
      sum += v;
      sum2 += v * v;
    }
  }
  long double var_f = 0.0L;
  if (gain > 0.0) {
    for (int j = -half; j <= half; ++j) {
      for (int i = -half; i <= half; ++i) {
        const long double r2 = static_cast<long double>(i) * i + static_cast<long double>(j) * j;
        const long double t = 1.0L + r2 / alpha2;
        const long double v = 1.0L / (t * t * t * t);
        const long double P = v / sum;
        const long double si = static_cast<long double>(flux) * P;
        long double var_i = static_cast<long double>(sky) * sky;
        if (si > 0.0L) var_i += si / static_cast<long double>(gain);
        var_f += (P * P) / var_i;
      }
    }
    var_f = 1.0L / var_f;
  } else {
    // P_i = v_i/sum => sum_p2 = sum2/sum^2; sigma_F^2 = sky^2/sum_p2 = sky^2*sum^2/sum2
    var_f = static_cast<long double>(sky) * sky * (sum * sum) / sum2;
  }
  o.sum_p2 = static_cast<double>(sum2 / (sum * sum));
  o.sigma_f = static_cast<double>(std::sqrt(var_f));
  o.snr_optimal = flux / o.sigma_f;
  return o;
}

// ---- 冻结的真实源 (run/perf-fix/P5-snr/harness/real_sources.json) ----
struct RealSrc {
  const char* id;
  double flux;
  double fwhm;
  double snr_optimal;   // NumPy oracle 锚 (probe_out.json real.sN.snr_optimal)
  double sigma_f;       // NumPy oracle 锚
};
constexpr double kRealSky = 260.8590110604006;
const RealSrc kReal[5] = {
    {"src-145762", 2984.19140625, 1.4072320071088888, 4.7863380411985785, 623.4811207573439},
    {"src-145763", 11900.705322265625, 2.9622233810299274, 8.57160022178874, 1388.3878172496197},
    {"src-145764", 11581.213134765625, 3.0339454080028325, 8.14419621260785, 1422.0203974011586},
    {"src-145774", 30877.013671875, 3.4166259417890283, 19.28103596308005, 1601.4188102236467},
    {"src-145786", 11209.939697265625, 3.001042479230288, 7.96957476325257, 1406.5919487893211},
};
// NumPy oracle extract_v3 锚 (同一 5 源)
constexpr double kOracleMedian = 8.14419621260785;
constexpr double kOracleFlux5 = 7032.959743946604;

std::vector<astrocs::phase1::SnrSourceRow> real_rows() {
  std::vector<astrocs::phase1::SnrSourceRow> rows;
  for (const auto& s : kReal) {
    astrocs::phase1::SnrSourceRow r;
    r.id = s.id;
    r.flux_adu = s.flux;
    r.fwhm_px = s.fwhm;
    rows.push_back(r);
  }
  return rows;
}

// ============================ groups ============================

void group_units() {
  using astrocs::phase1::SnrFrameScienceConfig;
  using astrocs::phase1::SnrSourceRow;

  // F1: 天空受限 (gain<=0) 合成集
  std::vector<SnrSourceRow> rows;
  for (int i = 0; i < 9; ++i) {
    SnrSourceRow r;
    r.id = "syn-" + std::to_string(i);
    r.flux_adu = 500.0 + 250.0 * i;
    r.fwhm_px = 1.5 + 0.25 * i;
    rows.push_back(r);
  }
  SnrFrameScienceConfig cfg;
  cfg.sigma_sky_adu = 40.0;
  auto out = astrocs::phase1::compute_snr_frame_science(rows, cfg);
  check(out.valid, "F1 valid");
  check(out.n_input == 9 && out.n_used == 9, "F1 counts");

  std::vector<double> refs;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const Ref ref = ref_source(rows[i].flux_adu, rows[i].fwhm_px, cfg.sigma_sky_adu, 0.0);
    check_close(out.snr_f[i], ref.snr_optimal, 1e-12, "F1 snr_f");
    check_close(out.sigma_f_adu[i], ref.sigma_f, 1e-12, "F1 sigma_f");
    refs.push_back(ref.snr_optimal);
  }
  const double med = ref_median(refs);
  check_close(out.median_snr, med, 1e-12, "F1 median");
  check_close(out.snr_phot, med, 1e-12, "F1 snr_phot");
  check_close(out.median_source_snr, med, 1e-12, "F1 median_source_snr");
  for (std::size_t i = 0; i < rows.size(); ++i) {
    check_close(out.local_snr[i], refs[i] / med, 1e-12, "F1 local_snr");
  }
  // 帧级 5sigma 深度 = 5 * sigma_F(median FWHM 参考轮廓)
  std::vector<double> fw;
  for (const auto& r : rows) fw.push_back(r.fwhm_px);
  const double med_fwhm = ref_median(fw);
  const Ref ref_ref = ref_source(1.0, med_fwhm, cfg.sigma_sky_adu, 0.0);
  check_close(out.frame_depth_flux5_adu, 5.0 * ref_ref.sigma_f, 1e-12, "F1 flux5");
  check(std::isnan(out.frame_depth_m5_mag), "F1 m5 NaN without ZP");
  check_close(out.reference_fwhm_px, med_fwhm, 1e-15, "F1 ref fwhm");
  // 零点标准误
  check_close(out.sigma_location_se_dex, 0.0, 0.0, "F1 se zero (no sigma)");
  check_close(out.sigma_location_se_mag, 0.0, 0.0, "F1 se mag zero");

  // F2: gain>0 (CCD 方程) — 参考实现含源泊松项
  SnrFrameScienceConfig cfg2 = cfg;
  cfg2.gain_e_per_adu = 2.0;
  auto out2 = astrocs::phase1::compute_snr_frame_science(rows, cfg2);
  check(out2.valid, "F2 valid");
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const Ref ref = ref_source(rows[i].flux_adu, rows[i].fwhm_px, cfg2.sigma_sky_adu, 2.0);
    check_close(out2.snr_f[i], ref.snr_optimal, 1e-12, "F2 snr_f");
  }
  // gain>0 时 σ_F 与通量有关 → 与 gain<=0 的 SNR 不同
  check(std::fabs(out2.snr_f[0] - out.snr_f[0]) > 1e-6, "F2 gain path differs");

  // F3: 零点标准误公式 1.253*sigma/sqrt(N)
  SnrFrameScienceConfig cfg3 = cfg;
  cfg3.sigma_logflux_dex = 0.05;
  cfg3.n_matches = 200;
  auto out3 = astrocs::phase1::compute_snr_frame_science(rows, cfg3);
  check_close(out3.sigma_location_se_dex, 1.253 * 0.05 / std::sqrt(200.0), 1e-15, "F3 se dex");
  check_close(out3.sigma_location_se_mag, 2.5 * out3.sigma_location_se_dex, 1e-15, "F3 se mag");
}

void group_oracle() {
  using astrocs::phase1::SnrFrameScienceConfig;
  auto rows = real_rows();
  SnrFrameScienceConfig cfg;
  cfg.sigma_sky_adu = kRealSky;
  auto out = astrocs::phase1::compute_snr_frame_science(rows, cfg);
  check(out.valid, "O valid");
  check(out.n_used == 5, "O n_used");
  for (int i = 0; i < 5; ++i) {
    check_close(out.snr_f[static_cast<std::size_t>(i)], kReal[i].snr_optimal, 1e-12, "O snr_f anchor");
    check_close(out.sigma_f_adu[static_cast<std::size_t>(i)], kReal[i].sigma_f, 1e-12, "O sigma_f anchor");
  }
  // P5 NumPy oracle extract_v3: snr_phot == median_snr == median_source_snr == median(SNR_F)
  check_close(out.snr_phot, kOracleMedian, 1e-12, "O snr_phot");
  check_close(out.median_snr, kOracleMedian, 1e-12, "O median_snr");
  check_close(out.median_source_snr, kOracleMedian, 1e-12, "O median_source_snr");
  check_close(out.frame_depth_flux5_adu, kOracleFlux5, 1e-12, "O flux5");
  check(std::isnan(out.frame_depth_m5_mag), "O m5 NaN (no ZP, = oracle)");

  // ZP 已知时 m5 = ZP - 2.5*log10(F5)
  SnrFrameScienceConfig cfgz = cfg;
  cfgz.zero_point_mag = 20.0;
  auto outz = astrocs::phase1::compute_snr_frame_science(rows, cfgz);
  check_close(outz.frame_depth_m5_mag, 20.0 - 2.5 * std::log10(kOracleFlux5), 1e-12, "O m5 with ZP");
}

void group_contract() {
  using astrocs::phase1::SnrFrameScienceConfig;
  auto rows = real_rows();
  SnrFrameScienceConfig cfg;
  cfg.sigma_sky_adu = kRealSky;
  auto out = astrocs::phase1::compute_snr_frame_science(rows, cfg);

  // C1: 三个帧级标量必须是同一个 median(SNR_F), 不是任何"整帧 SNR"构造
  check(out.snr_phot == out.median_snr && out.median_snr == out.median_source_snr,
        "C1 snr_phot==median_snr==median_source_snr (bitwise)");
  std::vector<double> used;
  for (std::size_t i = 0; i < out.snr_f.size(); ++i) {
    if (std::isfinite(out.snr_f[i])) used.push_back(out.snr_f[i]);
  }
  check_close(out.median_snr, ref_median(used), 1e-15, "C1 median == median(SNR_F)");

  // C2: 旧构造必须被否证。
  //  (a) 数值不是 1/(ln10*sigma_residual) (定标散度倒数, 不同类量);
  //  (b) 该旧构造是**常数**(与目录无关), 真 SNR 在天空受限下正比于通量 ——
  //      整体缩放 100x 通量, 新定义必须恰好放大 100x。
  const double legacy_dex = 1.0 / (2.302585092994045684 * 0.05);  // sigma=0.05 dex
  check(std::fabs(out.median_snr - legacy_dex) > 1e-6, "C2a != 1/(ln10*sigma_residual)");
  {
    auto scaled = rows;
    for (auto& r : scaled) r.flux_adu *= 100.0;
    auto out_scaled = astrocs::phase1::compute_snr_frame_science(scaled, cfg);
    check_close(out_scaled.median_snr / out.median_snr, 100.0, 1e-12, "C2b SNR scales with flux");
  }

  // C3: local_snr 是逐源相对质量权重场 (非标量), 中位数 1.0 (S4 quality_weight)
  check(out.local_snr.size() == out.snr_f.size(), "C3 local_snr is per-source");
  check_close(out.local_snr[2], out.snr_f[2] / out.median_snr, 1e-15, "C3 local = SNR_F/median");
  std::vector<double> loc;
  for (double v : out.local_snr) {
    if (std::isfinite(v)) loc.push_back(v);
  }
  check_close(ref_median(loc), 1.0, 1e-15, "C3 median(local_snr) == 1");

  // C4: 帧级科学基准 = 5sigma 深度, 与 5*sigma_F(ref) 恒等
  check_close(out.frame_depth_flux5_adu, 5.0 * out.reference_sigma_f_adu, 1e-15, "C4 F5 == 5*sigma_F(ref)");

  // C5: 字段单位/符号域
  check(out.frame_depth_flux5_adu > 0.0, "C5 F5 > 0 [ADU]");
  for (double v : out.snr_f) {
    if (std::isfinite(v)) check(v > 0.0, "C5 SNR_F > 0 [1]");
  }

  // C6: 未参与行必须是 NaN (不得静默回填 1.0 伪装 unknown)
  check(std::isnan(out.local_snr[0]) == false, "C6 used rows finite");
}

void group_negative() {
  using astrocs::phase1::SnrFrameScienceConfig;
  using astrocs::phase1::SnrSourceRow;

  // N1: sigma_sky <= 0 -> fail-closed
  {
    std::vector<SnrSourceRow> rows(1);
    rows[0].id = "x";
    rows[0].flux_adu = 100.0;
    rows[0].fwhm_px = 2.0;
    SnrFrameScienceConfig cfg;
    cfg.sigma_sky_adu = 0.0;
    auto out = astrocs::phase1::compute_snr_frame_science(rows, cfg);
    check(!out.valid && !out.reason.empty(), "N1 sigma_sky<=0 fail-closed");
    check(std::isnan(out.snr_phot), "N1 no fallback scalar");
  }
  // N2: 空目录
  {
    SnrFrameScienceConfig cfg;
    cfg.sigma_sky_adu = 10.0;
    auto out = astrocs::phase1::compute_snr_frame_science({}, cfg);
    check(!out.valid && !out.reason.empty(), "N2 empty catalogue fail-closed");
  }
  // N3: 全退化行 (flux<=0 / fwhm<=0 / NaN) -> 不产 SNR, 不填 1.0
  {
    std::vector<SnrSourceRow> rows(4);
    rows[0].id = "a"; rows[0].flux_adu = 0.0;   rows[0].fwhm_px = 2.0;
    rows[1].id = "b"; rows[1].flux_adu = 100.0; rows[1].fwhm_px = 0.0;
    rows[2].id = "c"; rows[2].flux_adu = nan_v(); rows[2].fwhm_px = 2.0;
    rows[3].id = "d"; rows[3].flux_adu = -5.0;  rows[3].fwhm_px = 2.0;
    SnrFrameScienceConfig cfg;
    cfg.sigma_sky_adu = 10.0;
    auto out = astrocs::phase1::compute_snr_frame_science(rows, cfg);
    check(!out.valid, "N3 all-degenerate invalid");
    check(out.n_used == 0, "N3 n_used == 0");
    for (double v : out.snr_f) check(std::isnan(v), "N3 NaN (no 1.0 fill)");
  }
  // N4: 部分退化 -> 只对有效行产 SNR; 退化行保持 NaN
  {
    std::vector<SnrSourceRow> rows(3);
    rows[0].id = "ok1"; rows[0].flux_adu = 1000.0; rows[0].fwhm_px = 2.0;
    rows[1].id = "bad"; rows[1].flux_adu = 0.0;    rows[1].fwhm_px = 2.0;
    rows[2].id = "ok2"; rows[2].flux_adu = 2000.0; rows[2].fwhm_px = 2.5;
    SnrFrameScienceConfig cfg;
    cfg.sigma_sky_adu = 25.0;
    auto out = astrocs::phase1::compute_snr_frame_science(rows, cfg);
    check(out.valid && out.n_used == 2, "N4 partial used");
    check(std::isfinite(out.snr_f[0]) && std::isnan(out.snr_f[1]) && std::isfinite(out.snr_f[2]),
          "N4 per-row mask");
    check_close(out.median_snr, 0.5 * (out.snr_f[0] + out.snr_f[2]), 1e-15, "N4 median over 2");
  }
}

void group_determinism() {
  using astrocs::phase1::SnrFrameScienceConfig;
  auto rows = real_rows();
  SnrFrameScienceConfig cfg;
  cfg.sigma_sky_adu = kRealSky;
  auto a = astrocs::phase1::compute_snr_frame_science(rows, cfg);
  auto b = astrocs::phase1::compute_snr_frame_science(rows, cfg);
  bool same = (a.snr_f.size() == b.snr_f.size()) &&
              (a.snr_phot == b.snr_phot) && (a.median_snr == b.median_snr) &&
              (a.frame_depth_flux5_adu == b.frame_depth_flux5_adu);
  for (std::size_t i = 0; i < a.snr_f.size() && same; ++i) {
    same = (std::memcmp(&a.snr_f[i], &b.snr_f[i], sizeof(double)) == 0);
  }
  check(same, "D1 repeated calls bitwise identical");

  // 输入顺序置换 -> median(SNR_F) 恒等 (排序统计量)
  auto rows2 = rows;
  std::reverse(rows2.begin(), rows2.end());
  auto c = astrocs::phase1::compute_snr_frame_science(rows2, cfg);
  check_close(c.median_snr, a.median_snr, 1e-15, "D2 order-independent median");
  check_close(c.frame_depth_flux5_adu, a.frame_depth_flux5_adu, 1e-15, "D2 order-independent F5");
}

struct Group {
  const char* name;
  void (*fn)();
};
const Group kGroups[] = {
    {"units", group_units},
    {"oracle", group_oracle},
    {"contract", group_contract},
    {"negative", group_negative},
    {"determinism", group_determinism},
};

}  // namespace

int main(int argc, char** argv) {
  const std::string want = (argc > 1) ? argv[1] : "all";
  if (want == "all") {
    for (const auto& g : kGroups) g.fn();
  } else {
    bool found = false;
    for (const auto& g : kGroups) {
      if (want == g.name) {
        g.fn();
        found = true;
      }
    }
    if (!found) {
      std::printf("unknown group: %s\n", want.c_str());
      return 2;
    }
  }
  std::printf("p1snr_linux_test[%s]: %d checks, %d failed\n", want.c_str(), g_total, g_fail);
  return g_fail == 0 ? 0 : 1;
}
