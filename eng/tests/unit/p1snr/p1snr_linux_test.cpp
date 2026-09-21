// p1snr_linux_test.cpp - Linux 生产路径 SNR 回归锁 (P8-SNR-LINUX)
//
// 被测面 (真实生产目标, 非测试私有编译):
//   astrocs_phase1_noise  — 根 CMakeLists.txt 的 Linux 生产静态库
//     lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp  (帧级聚合, P8 接线层)
//     lib/algorithms/noise_snr/cpp/src/snr_science.cpp (P5-SNR 唯一权威科学实现)
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
// 用法: p1snr_linux_test [units|oracle|contract|negative|determinism|mother_function|all]
#include "snr_frame_science.h"

// P5-SNR 权威科学实现的公开契约头 (snr_moffat4_profile_f64 / snr_source_snr_f64)。
// 该头不在 astrocs_phase1_noise 的 PUBLIC include 面 (生产源以相对包含使用),
// 故此处同款相对包含 (生产源零副本)。
#include "../../../../lib//algorithms/noise_snr/cpp/include/snr_estimator.h"

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
// 检测块母函数因子 (椭圆高斯, TWO_SQRT_2_LOG2; SCI-P1-STAR-001 §2/§5, ALG-STARDET-001 §2):
// 输入行 fwhm_px (DATA-P1-SOURCES.sources[].fwhm_px) 属该块 ⇒ sigma = fwhm/kGaussFwhmFactor。
constexpr double kGaussFwhmFactor = 2.3548200450309493;
// PSF 块母函数因子 (各向同性 Moffat4 beta=4; SCI-PSF-001 §5): 只用于本块轮廓模型的
// FWHM<->sigma 正向换算; **禁止**用它反解检测块列 (跨块混用, DISP-STAR-007)。
constexpr double kMoffat4FwhmFactor = 1.230310;

// 网格半边长: 输入为本块 Moffat4 FWHM (= kMoffat4FwhmFactor*sigma), 与实现 autoHalf 同规则。
int ref_half(double fwhm_moffat4) {
  const double fwhm = fwhm_moffat4;
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
  // fwhm 为**检测块**椭圆高斯 FWHM (DATA-P1-SOURCES 列) ⇒ 高斯因子换算 sigma;
  // 网格用本块 Moffat4 模型的 FWHM = kMoffat4FwhmFactor*sigma。
  const double sigma_d = fwhm / kGaussFwhmFactor;
  const long double sigma = static_cast<long double>(sigma_d);
  const int half = ref_half(sigma_d * kMoffat4FwhmFactor);
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
// CONFORM-FIX-A ①: 锚值按**修复后口径** (检测块高斯 FWHM -> sigma = fwhm/2.3548200450309493)
// 由独立 NumPy oracle (run/RELEASE-02/conform-fix-a/harness/conf1_oracle.py, 不调用被测
// C++ 代码) 复算; 修复前锚值 (Moffat4 因子反解) 见 CONFORM-SWEEP-1-001。
const RealSrc kReal[5] = {
    {"src-145762", 2984.19140625, 1.4072320071088888, 10.01518830728662, 297.96657982744375},
    {"src-145763", 11900.705322265625, 2.9622233810299274, 16.95958883413694, 701.7095425280245},
    {"src-145764", 11581.213134765625, 3.0339454080028325, 16.04022427426992, 722.0106737125251},
    {"src-145774", 30877.013671875, 3.4166259417890283, 37.36271474205877, 826.4124779219297},
    {"src-145786", 11209.939697265625, 3.001042479230288, 15.728210630590322, 712.7282283124464},
};
// NumPy oracle extract_v3 锚 (同一 5 源)
constexpr double kOracleMedian = 16.04022427426992;
constexpr double kOracleFlux5 = 3563.641141562232;
// WEIGHT-SCI-001（2026-09-18 裁决）: 组内公共参考通量 F0。同一帧组的所有帧必须
// 用**同一个** F0 定义 SNR 并写入 reference_flux_adu（配对性定理）。逐帧检出通量
// 中位数回退已删除；本常量取 5 颗真实源通量的中位数作为冻结的公共参考值。
constexpr double kGroupRefFlux = 11581.213134765625;

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
  // WEIGHT-SCI-001: 组内公共 F_ref（旧逐帧中位数回退已删除 ⇒ 必须显式给出）。
  cfg.reference_flux_adu = kGroupRefFlux;
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
  // WEIGHT-SCI-001: 存头参考通量 = 组内公共 F0（不是本帧检出通量中位数）。
  check_close(out.reference_flux_adu, kGroupRefFlux, 0.0, "F1 ref flux = group-common F0");
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
  cfg.reference_flux_adu = kGroupRefFlux;   // WEIGHT-SCI-001: 组内公共 F_ref
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
  cfg.reference_flux_adu = kGroupRefFlux;   // WEIGHT-SCI-001: 组内公共 F_ref
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

  // C6: 参与行 local_snr 必须有限且 = SNR_F/median(SNR_F)；
  //     未参与行必须是 NaN (不得静默回填 1.0 伪装 unknown)。
  {
    bool used_ok = (out.local_snr.size() == out.snr_f.size());
    for (std::size_t i = 0; used_ok && i < out.local_snr.size(); ++i) {
      used_ok = std::isfinite(out.local_snr[i]) &&
                (out.local_snr[i] == out.snr_f[i] / out.median_snr);
    }
    check(used_ok, "C6 participating rows local_snr finite == SNR_F/median");
    auto mixed = rows;                       // 5 真实源 + 1 条退化行
    astrocs::phase1::SnrSourceRow bad;
    bad.id = "bad-flux";
    bad.flux_adu = 0.0;                      // flux<=0 -> 不参与逐源 SNR
    bad.fwhm_px = 2.0;
    mixed.push_back(bad);
    auto outm = astrocs::phase1::compute_snr_frame_science(mixed, cfg);
    check(outm.n_used == static_cast<int>(rows.size()), "C6 degenerate row not used");
    check(std::isfinite(outm.local_snr[0]), "C6 participating row finite");
    check(std::isnan(outm.local_snr.back()),
          "C6 non-participating row local_snr must be NaN (no 1.0 fill)");
    check(std::isnan(outm.snr_f.back()), "C6 non-participating row snr_f must be NaN");
  }
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
    // WEIGHT-SCI-001: 缺组内公共 F_ref 已改为 fail-closed（逐帧中位数回退已删除），
    // 故本用例必须显式给出公共参考通量；n_used 仍应为 2（flux=0 的行仍被掩掉）。
    cfg.reference_flux_adu = 1500.0;
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
  cfg.reference_flux_adu = kGroupRefFlux;   // WEIGHT-SCI-001: 组内公共 F_ref
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

// ── WEIGHT-SCI-001: F_ref 必须是组内公共参考通量（配对性定理）───────────────
// 旧行为 = 逐帧检出通量中位数回退（已删除）。本组断言:
//   G1/G2: 同一帧组（不同检出星群）用同一 F0 ⇒ 写出的 reference_flux_adu 逐位相同;
//   G3   : reference_snr_f 在公共 F0 处评价（= F0/σ_F(F0)），不是本帧检出通量处;
//   G4/G5: F0 缺失/非有限/≤0 ⇒ fail-closed，reason 指明组内公共 F_ref（不回退）。
void group_common_ref() {
  using astrocs::phase1::SnrFrameScienceConfig;
  using astrocs::phase1::SnrSourceRow;

  const double F0 = 3000.0;   // 组内公共参考通量 [ADU]
  std::vector<SnrSourceRow> frame_a, frame_b;
  for (int i = 0; i < 6; ++i) {
    SnrSourceRow ra;
    ra.id = "a" + std::to_string(i);
    ra.flux_adu = 500.0 + 100.0 * i;
    ra.fwhm_px = 1.8;
    frame_a.push_back(ra);
    SnrSourceRow rb;
    rb.id = "b" + std::to_string(i);
    rb.flux_adu = 5000.0 + 1000.0 * i;
    rb.fwhm_px = 2.4;
    frame_b.push_back(rb);
  }
  SnrFrameScienceConfig cfg;
  cfg.sigma_sky_adu = 40.0;
  cfg.reference_flux_adu = F0;
  const auto a = astrocs::phase1::compute_snr_frame_science(frame_a, cfg);
  const auto b = astrocs::phase1::compute_snr_frame_science(frame_b, cfg);
  check(a.valid && b.valid, "G1 both frames valid with group F0");

  // 组内公共: 两帧写出的 F_ref 逐位相同（= 定义 SNR 时所用参考通量）。
  check(a.reference_flux_adu == F0 && b.reference_flux_adu == F0,
        "G2 group-common F_ref identical across frames");
  check(std::memcmp(&a.reference_flux_adu, &b.reference_flux_adu, sizeof(double)) == 0,
        "G2b group-common F_ref bitwise identical");

  // 配对性: reference_snr_f 是在公共 F0 下评价的 SNR，不是"本帧检出通量中位数"处。
  const Ref refa = ref_source(F0, a.reference_fwhm_px, cfg.sigma_sky_adu, 0.0);
  check_close(a.reference_snr_f, refa.snr_optimal, 1e-12,
              "G3 reference_snr_f evaluated at group F0 (frame a)");
  const Ref refb = ref_source(F0, b.reference_fwhm_px, cfg.sigma_sky_adu, 0.0);
  check_close(b.reference_snr_f, refb.snr_optimal, 1e-12,
              "G3 reference_snr_f evaluated at group F0 (frame b)");

  // G4: 逐帧中位数回退已删除 —— 不显式给 F0 ⇒ fail-closed, reason 指明组内公共。
  {
    SnrFrameScienceConfig bad;
    bad.sigma_sky_adu = 40.0;   // reference_flux_adu 缺省 0.0
    const auto out = astrocs::phase1::compute_snr_frame_science(frame_a, bad);
    check(!out.valid, "G4 missing F0 fail-closed");
    check(out.reason.find("reference_flux_adu required") != std::string::npos &&
              out.reason.find("group-common F_ref") != std::string::npos,
          "G4 reason names group-common F_ref requirement");
    check(std::isnan(out.reference_flux_adu),
          "G4 no per-frame median fallback (reference_flux_adu stays NaN)");
  }
  // G5: 非正 / 非有限 F0 ⇒ fail-closed（逐帧中位数不得兜底）
  {
    SnrFrameScienceConfig z = cfg;
    z.reference_flux_adu = 0.0;
    check(!astrocs::phase1::compute_snr_frame_science(frame_a, z).valid,
          "G5 F0=0 fail-closed");
    SnrFrameScienceConfig n = cfg;
    n.reference_flux_adu = nan_v();
    check(!astrocs::phase1::compute_snr_frame_science(frame_a, n).valid,
          "G5 F0=NaN fail-closed");
    SnrFrameScienceConfig neg = cfg;
    neg.reference_flux_adu = -1.0;
    check(!astrocs::phase1::compute_snr_frame_science(frame_a, neg).valid,
          "G5 F0<0 fail-closed");
  }
}

// ── CONFORM-FIX-A ①: fwhm_px 母函数口径 (禁止跨块混用) ───────────────────────
// 规范: 检测侧母函数 = 椭圆高斯 (FWHM = 2.3548200450309493*sigma; SCI-P1-STAR-001 §2,
//   ALG-STARDET-001 §2); PSF 侧 = 椭圆 Moffat4 (FWHM = 1.230310*sigma; SCI-PSF-001 §5);
//   同 sigma 下相差 1.914005x, **两列禁止跨块比较** (DISP-STAR-007)。
// 被测面: DATA-P1-SOURCES.fwhm_px (检测块高斯列) 进入 snr_science 的换算因子。
// 判别力: 输入列按高斯因子换算后必须与同 sigma 直传路径**逐位一致** (绿);
//   旧路径按 PSF 块因子 1.230310 反解 ⇒ sigma 高估 1.914005x (红)。
void group_mother_function() {
  using astrocs::phase1::SnrFrameScienceConfig;
  using astrocs::phase1::SnrSourceRow;

  const double sigma_true = 1.25;                        // 已知 sigma [px]
  const double fwhm_gauss = kGaussFwhmFactor * sigma_true;  // 检测块列 (高斯 FWHM)

  double sp2_col = 0.0, pc_col = 0.0, sp2_sig = 0.0, pc_sig = 0.0;
  check(snr_moffat4_profile_f64(fwhm_gauss, 0.0, 0, &sp2_col, &pc_col) == 0, "M1 col rc");
  check(snr_moffat4_profile_f64(0.0, sigma_true, 0, &sp2_sig, &pc_sig) == 0, "M1 sigma rc");
  check(sp2_col == sp2_sig && pc_col == pc_sig,
        "M1 detection-block Gaussian fwhm == same sigma (bitwise)");

  // 跨块因子恒等 (规范值): 2.3548200450309493/1.230310
  check_close(kGaussFwhmFactor / kMoffat4FwhmFactor, 1.9140054498711294, 1e-15,
              "M2 cross-block sigma bias = 1.914005");
  // 非恒真锁: 按 PSF 块因子反解同一列必须给出不同结果 (旧行为可被本组抓住)
  double sp2_bad = 0.0, pc_bad = 0.0;
  check(snr_moffat4_profile_f64(kMoffat4FwhmFactor * sigma_true, 0.0, 0, &sp2_bad, &pc_bad) == 0,
        "M3 cross-block rc");
  check(sp2_bad != sp2_col && pc_bad != pc_col,
        "M3 cross-block interpretation differs (non-vacuous)");

  // 帧级路径 (wrapper): 已知 sigma 的合成行 (fwhm=高斯 FWHM) 必须与同 sigma 的解析
  // 天空受限公式一致 (sigma_F = sky/sqrt(sum P^2); SNR_F = F/sigma_F)。
  std::vector<SnrSourceRow> rows(1);
  rows[0].id = "m1";
  rows[0].flux_adu = 1.0e4;
  rows[0].fwhm_px = fwhm_gauss;
  SnrFrameScienceConfig cfg;
  cfg.sigma_sky_adu = 20.0;
  cfg.reference_flux_adu = 1.0e4;
  const auto out = astrocs::phase1::compute_snr_frame_science(rows, cfg);
  check(out.valid && out.n_used == 1, "M4 frame valid");
  const double sigma_f_expect = cfg.sigma_sky_adu / std::sqrt(sp2_sig);
  check_close(out.sigma_f_adu[0], sigma_f_expect, 1e-12,
              "M4 sigma_F from Gaussian-fwhm column == sigma path");
  check_close(out.snr_f[0], 1.0e4 / sigma_f_expect, 1e-12,
              "M4b SNR_F from Gaussian-fwhm column == sigma path");
  // 旧路径 (跨块) 等价于 sigma 放大 1.914005x: 用 1.914005x 更宽的**高斯列**复现该
  // sigma ⇒ SNR 必须更低, 且比值落在 [1.85,2.15] (纯 sigma 因子 1.914005; 离散采样下
  // sigma~1px 的 Moffat4 轮廓不满足 sum_p2 ∝ sigma^-2 的连续极限, 实测 1.981)。
  std::vector<SnrSourceRow> wide_rows = rows;
  wide_rows[0].fwhm_px = rows[0].fwhm_px * (kGaussFwhmFactor / kMoffat4FwhmFactor);
  const auto wide = astrocs::phase1::compute_snr_frame_science(wide_rows, cfg);
  check(wide.valid && wide.snr_f[0] < out.snr_f[0],
        "M5 sigma x1.914 (old cross-block) yields lower SNR");
  const double ratio = out.snr_f[0] / wide.snr_f[0];
  check(ratio > 1.85 && ratio < 2.15,
        "M5b SNR ratio ~1.914 direction (discrete-sampling bounded)");
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
    {"common_ref", group_common_ref},
    {"mother_function", group_mother_function},
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
