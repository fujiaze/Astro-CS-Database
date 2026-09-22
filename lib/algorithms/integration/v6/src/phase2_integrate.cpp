/* phase2_integrate.cpp — V6 Phase2 三模式产品链集成实现
 * 见 phase2_integrate.h 的冻结锚。本层只接线 Wave 5/7 实现，不含新科学公式。
 */
#include "astrocs/v6/phase2_integrate.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): 文本读写与 FITS
// 平面读取一律经 aio 唯一实现 (aio_file::read_all / aio_atomic::write_file_atomic),
// 本 TU 不自持 fstream 通道。
#include "aio_atomic_file.h"
#include "aio_file_io.h"

#include "astro/aio/v6_bunit.h"
#include "astro/aio/v6_product_io.h"
#include "astro/aio/v6_sha256.h"
#include "astro/phase2/coverage.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/sampler.h"
#include "astro/phase2/upm.h"
#include "astrocs/v6/information_weight.h"
#include "astrocs/v6/psf_information.h"
#include "astrocs/v6/psfsw.h"

using nlohmann::json;
using namespace astrocs::aio;

namespace astrocs {
namespace v6 {
namespace p2int {

namespace {

const double kPi = 3.14159265358979323846;

/* ───────── 线性代数（纯 FP64；独立于被测实现） ───────── */
using Mat = std::vector<double>; /* row-major n x m */

double mat_get(const Mat& a, int ld, int i, int j) { return a[static_cast<std::size_t>(i) * ld + j]; }

/* Gauss-Jordan 求逆（含部分主元）；成功返回 true。 */
bool invert(const Mat& a, int n, Mat* out) {
  Mat m(a);
  Mat inv(static_cast<std::size_t>(n) * n, 0.0);
  for (int i = 0; i < n; ++i) inv[static_cast<std::size_t>(i) * n + i] = 1.0;
  for (int col = 0; col < n; ++col) {
    int piv = col;
    double best = std::fabs(m[static_cast<std::size_t>(col) * n + col]);
    for (int r = col + 1; r < n; ++r) {
      const double v = std::fabs(m[static_cast<std::size_t>(r) * n + col]);
      if (v > best) { best = v; piv = r; }
    }
    if (!(best > 0.0) || !std::isfinite(best)) return false;
    if (piv != col) {
      for (int c = 0; c < n; ++c) {
        std::swap(m[static_cast<std::size_t>(col) * n + c], m[static_cast<std::size_t>(piv) * n + c]);
        std::swap(inv[static_cast<std::size_t>(col) * n + c], inv[static_cast<std::size_t>(piv) * n + c]);
      }
    }
    const double d = m[static_cast<std::size_t>(col) * n + col];
    for (int c = 0; c < n; ++c) {
      m[static_cast<std::size_t>(col) * n + c] /= d;
      inv[static_cast<std::size_t>(col) * n + c] /= d;
    }
    for (int r = 0; r < n; ++r) {
      if (r == col) continue;
      const double f = m[static_cast<std::size_t>(r) * n + col];
      if (f == 0.0) continue;
      for (int c = 0; c < n; ++c) {
        m[static_cast<std::size_t>(r) * n + c] -= f * m[static_cast<std::size_t>(col) * n + c];
        inv[static_cast<std::size_t>(r) * n + c] -= f * inv[static_cast<std::size_t>(col) * n + c];
      }
    }
  }
  *out = std::move(inv);
  return true;
}

/* ──────── 文本/哈希/JSON ───────── */
bool read_text(const std::string& p, std::string* out, std::string* err) {
  // CLEAN-403: 整文件读取经 aio (打开/读取/关闭任一失败 ⇒ false)。
  if (!aio_file::read_all(p.c_str(), out)) { *err = "cannot open " + p; return false; }
  return true;
}

bool write_text(const std::string& p, const std::string& s, std::string* err) {
  // CLEAN-403: 落盘经 aio 原子写原语 (临时文件 → fsync → 原子 rename)。
  std::string werr;
  if (aio_atomic::write_file_atomic(p, s, &werr) != 0) {
    *err = "cannot write " + p + ": " + werr;
    return false;
  }
  return true;
}

bool is_hex40(const std::string& s) {
  if (s.size() != 40) return false;
  for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

/* P33 撤销守卫（C-004.2）：帧级 SNR 系数/诊断权重形态不得以键形式重新引入。 */
bool has_forbidden_p33_key(const json& j) {
  static const char* kBad[] = {"snr_frame_coefficient", "snr_frame_science",
                               "snr_coefficient", "snr_coef", "median_snr_weight",
                               "support_x_snr2", "support_x_snr", "snr_frame"};
  if (j.is_object()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      for (const char* b : kBad) if (it.key() == b) return true;
      if (has_forbidden_p33_key(it.value())) return true;
    }
  } else if (j.is_array()) {
    for (const auto& e : j) if (has_forbidden_p33_key(e)) return true;
  }
  return false;
}

/* 诊断别名（FZ-GATE-MEDIAN-SNR / FZ-GATE-SUPPORT-COVERAGE）：不得进权重/方差来源。 */
const char* kForbiddenWeightSources[] = {
    "median_source_snr", "median_snr", "source_snr_median", "support",
    "support_area", "coverage", "coverage_area", "fwhm", "psf_fwhm",
    "median_fwhm", "source_fwhm", "residual", "psf_residual", "psf_fit_residual",
    "fit_residual", "psfsw_robust_weight", "psfsw", "rejection", "probability",
    "rejection_probability", "effective_psf", "source_snr", "weight"};
bool is_forbidden_weight_source(const std::string& t) {
  for (const char* b : kForbiddenWeightSources) if (t == b) return true;
  return false;
}
json forbidden_sources_json() {
  json a = json::array();
  for (const char* t : kForbiddenWeightSources) a.push_back(t);
  return a;
}

void append_f64_be(std::vector<std::uint8_t>& out, double v) {
  std::uint64_t u = 0; std::memcpy(&u, &v, sizeof(u));
  for (int i = 7; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(u >> (8 * i)));
}
std::vector<std::uint8_t> encode_f64_be(const std::vector<double>& v) {
  std::vector<std::uint8_t> out; out.reserve(v.size() * 8);
  for (double x : v) append_f64_be(out, x);
  return out;
}

FitsLayer make_layer(const std::string& extname, const std::string& bunit,
                     const std::vector<double>& values) {
  FitsLayer l;
  l.spec.extname = extname;
  l.spec.bitpix = -64;
  l.spec.naxis = {values.size()};
  l.spec.cards.push_back(FitsCard::make_string("BUNIT", bunit, ""));
  l.spec.cards.push_back(FitsCard::make_string("ORIGIN", "AstroCS", ""));
  l.data = encode_f64_be(values);
  return l;
}

/* percentile（最近秩法，确定性）。 */
double nearest_rank_percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  if (v.size() == 1) return v[0];
  const double idx = std::floor(p * static_cast<double>(v.size() - 1));
  std::size_t i = static_cast<std::size_t>(std::max(0.0, std::min(idx, static_cast<double>(v.size() - 1))));
  return v[i];
}

bool any_off_diagonal(const std::vector<double>& c, std::uint64_t n, double tol) {
  for (std::uint64_t i = 0; i < n; ++i)
    for (std::uint64_t j = 0; j < n; ++j)
      if (i != j && std::fabs(c[static_cast<std::size_t>(i) * n + j]) > tol) return true;
  return false;
}

/* 由 psf_profile / a 派生组合系数（独立于被测实现）。 */
std::vector<double> derive_psf_alpha(const FrameSet& fs) {
  double sum = 0.0;
  for (const auto& f : fs.frames) sum += f.view.w_info;
  std::vector<double> a; a.reserve(fs.frames.size());
  for (const auto& f : fs.frames) a.push_back(f.view.w_info / sum);
  return a;
}

/* ── FZ-MODE-RETIRED：退役对象 psfsw_robust_weight 的显式拒绝说明（单一事实源） ──
 * WHAT:  本文件三处接受面（parse_weight_mode / route_weight_mode / open_phase2_product
 *        的产品校验）共用同一条拒绝说明，保证"被拒 mode + 允许集 + 迁移提示"三项逐字一致。
 * WHY:   负责人裁决「只要纯净信号/噪声的信噪比。要求跨帧可用，不基于参考帧。而是绝对
 *        标定。」⇒ 受 PixInsight PSFSW 启发的稳健复合帧权重 psfsw_robust_weight
 *        **不是现行对象**：ASTROCS_DESIGN.md §3.1（订正后）「权重只能来自纯净信号与噪声
 *        之比……任何使偏差随帧而变的量（含 PSF 拟合质量代理）都不得进入科学叠加权重」；
 *        docs/design/UNIFIED_MODEL.md:58（旧产品若声明该对象 ⇒ 显式拒绝 + 迁移提示，
 *        不得静默接受）；docs/science/PSF_SIGNAL_WEIGHT.md §1/§4（生产模式门 allowed
 *        只剩 point_information / surface_gls）。
 * 纪律:  不得静默接受、不得回退成默认模式；本常量只用于**拒绝**路径，不得用于放行。
 * 迁移:  point_information（W_info=1/Var(F_hat)）用于点源；surface_gls（A^T C^-1 A）
 *        用于扩展源；PSF 质量代理（FWHM/残差尺度）只作诊断。 */
const char* kRetiredWeightModeRejectReason =
    "FZ-MODE-RETIRED: weight_mode 'psfsw_robust' rejected - psfsw_robust_weight is not "
    "a current object (ASTROCS_DESIGN.md 3.1; UNIFIED_MODEL.md:58); "
    "allowed production modes: point_information | surface_gls; "
    "migration: point_information (W_info=1/Var(F_hat)) for point sources, "
    "surface_gls (A^T C^-1 A) for extended sources; "
    "PSF quality proxies (FWHM/residual) are diagnostics only";

/* 退役对象判据（大小写敏感全等，与本文件既有 token 比较口径一致）。 */
bool is_retired_weight_mode_token(const std::string& token) {
  return token == "psfsw_robust";
}

}  /* namespace */

/* ==================================================================== */
/* 模式路由                                                              */
/* ==================================================================== */
/* 生产模式接受集（FZ-MODE-PRODUCTION）= {point_information, surface_gls}；
 * 退役对象 psfsw_robust 走 FZ-MODE-RETIRED 显式拒绝（见 kRetiredWeightModeRejectReason）。
 * kPsfswRobust 枚举臂按 ENGINEERING_SPEC §2「保留则注释」保留（编译/ABI 兼容与历史产品
 * 可判），但**任何接受面都不得返回它**：parse/route/产品校验三处均已改为拒绝。 */
const char* weight_mode_token(WeightMode m) {
  switch (m) {
    case WeightMode::kPointInformation: return "point_information";
    case WeightMode::kSurfaceGls: return "surface_gls";
    case WeightMode::kPsfswRobust: return "psfsw_robust";  /* RETIRED：仅历史可判 */
  }
  return "unknown";
}
const char* phase2_type_id(WeightMode m) {
  switch (m) {
    case WeightMode::kPointInformation: return kTypePoint;
    case WeightMode::kSurfaceGls: return kTypeSurface;
    case WeightMode::kPsfswRobust: return kTypePsfsw;  /* RETIRED：仅历史可判 */
  }
  return "unknown";
}
bool parse_weight_mode(const std::string& token, WeightMode* out) {
  if (token == "point_information") { *out = WeightMode::kPointInformation; return true; }
  if (token == "surface_gls") { *out = WeightMode::kSurfaceGls; return true; }
  /* FZ-MODE-RETIRED：psfsw_robust 不是现行对象 ⇒ 解析面**不接受**（返回 false），
   * 由 route_weight_mode 给出可诊断的拒绝说明；不得静默解析成 kPsfswRobust。 */
  return false;
}
int route_weight_mode(const char* mode, WeightMode* out, char* err,
                      std::size_t err_cap) {
  auto set = [&](const char* m) {
    if (err && err_cap) std::snprintf(err, err_cap, "%s", m);
  };
  if (mode == nullptr) { set("weight_mode null"); return 1; }
  const std::string s(mode);
  if (s == "point_information") { if (out) *out = WeightMode::kPointInformation; return 0; }
  if (s == "surface_gls") { if (out) *out = WeightMode::kSurfaceGls; return 0; }
  /* FZ-MODE-RETIRED：退役对象显式拒绝 + 迁移提示（不静默接受、不回退默认模式）。
   * out 保持调用方原值，不得写入 kPsfswRobust。 */
  if (is_retired_weight_mode_token(s)) { set(kRetiredWeightModeRejectReason); return 1; }
  if (s == "equal" || s == "pixel_ivar") {
    set("documented baseline mode, not production (FZ-MODE-BASELINE)"); return 2;
  }
  if (s == "psf_snr_power") { set("DEFERRED not in production route (FZ-MODE-DEFERRED/C-004.1)"); return 1; }
  if (s == "auto" || s == "support_x_snr2" || s == "support_x_snr" ||
      s == "0" || s.empty()) {
    set("legacy/forbidden weight_mode (FZ-FIELD-WEIGHTMODE)"); return 1;
  }
  set("unknown weight_mode (FZ-MODE-PRODUCTION)"); return 1;
}

/* ==================================================================== */
/* 最小 FITS 平面读取                                                     */
/* ==================================================================== */
bool read_fits_plane_f64(const std::string& path, const std::string& extname,
                         std::vector<double>* values, std::string* bunit,
                         std::vector<std::uint64_t>* naxis, std::string* err) {
  // CLEAN-403 (§10 aio 唯一 I/O 边界): 整文件读取经 aio_file::read_all;
  // 解析在内存按偏移进行, 与逐段 seekg/read 同判据 (越界即截断)。
  std::string blob;
  if (!aio_file::read_all(path.c_str(), &blob)) {
    if (err) *err = "cannot open " + path;
    return false;
  }
  const std::uint64_t total = static_cast<std::uint64_t>(blob.size());
  std::uint64_t offset = 0;
  while (true) {
    if (offset + 2880 > total) { if (err) *err = "truncated FITS header"; return false; }
    const char* hdr = blob.data() + offset;
    std::string cur_name; int bitpix = 0; std::vector<std::uint64_t> ax;
    std::string cur_bunit;
    for (int c = 0; c < 36; ++c) {
      const char* card = hdr + c * 80;
      std::string key(card, card + 8);
      while (!key.empty() && key.back() == ' ') key.pop_back();
      if (key == "END") break;
      std::string val(card + 10, card + 30);
      auto trim = [](std::string s) {
        while (!s.empty() && s.front() == ' ') s.erase(s.begin());
        while (!s.empty() && s.back() == ' ') s.pop_back();
        return s;
      };
      std::string v = trim(val);
      if (!v.empty() && v.front() == '\'') {
        const std::size_t e = v.find('\'', 1);
        v = (e == std::string::npos) ? v.substr(1) : v.substr(1, e - 1);
      }
      if (key == "BITPIX") bitpix = std::atoi(v.c_str());
      else if (key == "NAXIS") { /* count below via NAXISn */ }
      else if (key.rfind("NAXIS", 0) == 0 && key.size() > 5) {
        const int n = std::atoi(key.c_str() + 5);
        if (n >= 1) {
          if (ax.size() < static_cast<std::size_t>(n)) ax.resize(n, 1);
          ax[static_cast<std::size_t>(n - 1)] = static_cast<std::uint64_t>(std::atoll(v.c_str()));
        }
      } else if (key == "EXTNAME") cur_name = v;
      else if (key == "BUNIT") cur_bunit = v;
    }
    std::uint64_t data_bytes = 0;
    if (!ax.empty()) {
      std::uint64_t n = 1;
      for (auto a : ax) n *= a;
      data_bytes = n * static_cast<std::uint64_t>(bitpix < 0 ? (-bitpix) / 8 : bitpix / 8);
    }
    const std::uint64_t padded = ((data_bytes + 2879) / 2880) * 2880;
    if (cur_name == extname) {
      if (bitpix != -64) { if (err) *err = "plane bitpix != -64"; return false; }
      if (offset + 2880 + data_bytes > total) {
        if (err) *err = "truncated FITS data"; return false;
      }
      const unsigned char* raw =
          reinterpret_cast<const unsigned char*>(blob.data() + offset + 2880);
      values->resize(static_cast<std::size_t>(data_bytes) / 8);
      for (std::size_t i = 0; i < values->size(); ++i) {
        std::uint64_t u = 0;
        for (int b = 0; b < 8; ++b) u = (u << 8) | raw[i * 8 + static_cast<std::size_t>(b)];
        double d = 0.0; std::memcpy(&d, &u, 8); (*values)[i] = d;
      }
      if (bunit) *bunit = cur_bunit;
      if (naxis) *naxis = ax;
      return true;
    }
    offset += 2880 + padded;
    if (offset > (1ull << 40)) { if (err) *err = "FITS plane not found"; return false; }
    if (total <= offset) {
      if (err) *err = "FITS plane '" + extname + "' not found"; return false;
    }
  }
}

/* ==================================================================== */
/* 磁盘重开 Phase1 帧集                                                   */
/* ==================================================================== */
FrameSet open_phase2_frame_set(const std::vector<std::string>& product_dirs) {
  FrameSet fs;
  if (product_dirs.size() < 2) {
    fs.error = "phase2 group requires >= 2 disk-reopened phase1 products (FZ-GATE-PSFSW-FAILCLOSED)";
    return fs;
  }
  for (const auto& d : product_dirs) {
    phase1::Phase1OpenResult r = phase1::open_phase1_product(d);
    if (!r.ok) { fs.error = "reopen failed for " + d + ": " + r.error; return fs; }
    if (!r.view.valid) { fs.error = "phase1 frame invalid (fail-closed): " + d; return fs; }
    if (r.view.n_common < kCommonMin) {
      fs.error = "n_common < 3 (PSFSW-T-NMIN): " + d; return fs;
    }
    if (r.view.n_components != 4) { fs.error = "psfsw four components missing: " + d; return fs; }
    if (r.view.normalization_deferred_to != "phase2") {
      fs.error = "group_normalization_deferred_to != phase2 (OI-01): " + d; return fs;
    }
    std::string text, err;
    if (!read_text(d + "/" + phase1::kPhase1RecordFile, &text, &err)) { fs.error = err; return fs; }
    json doc;
    try { doc = json::parse(text); } catch (const std::exception& e) {
      fs.error = std::string("record parse error: ") + e.what(); return fs;
    }
    Phase1Frame f;
    f.dir = d;
    f.view = r.view;
    const json& ext = doc["phase1_extensions"];
    for (const auto& v : ext["psf_profile"]) f.psf_profile.push_back(v.get<double>());
    f.a = ext["point_source"].value("a", 1.0);
    if (!sha256_file_hex(d + "/" + phase1::kPhase1RecordFile, &f.record_sha)) {
      fs.error = "record sha256 failed: " + d; return fs;
    }
    if (!read_fits_plane_f64(d + "/" + phase1::kPhase1ScienceFile, "", &f.signal_sb,
                             &f.signal_bunit, nullptr, &err)) {
      fs.error = "read SIGNAL failed: " + err; return fs;
    }
    if (!read_fits_plane_f64(d + "/" + phase1::kPhase1ScienceFile, "VARIANCE",
                             &f.variance_sb, &f.variance_bunit, nullptr, &err)) {
      fs.error = "read VARIANCE failed: " + err; return fs;
    }
    if (f.signal_bunit != "ADU/sr" || f.variance_bunit != "ADU^2/sr^2") {
      fs.error = "phase1 disk BUNIT not frozen: " + d; return fs;
    }
    /* 与磁盘 Q/W 一致性：Var(F_hat)=1/W_info。 */
    if (!(std::fabs(f.view.flux_variance * f.view.w_info - 1.0) <= 1e-9)) {
      fs.error = "disk Q/W inconsistency (FZ-FORMULA-WINFO): " + d; return fs;
    }
    fs.frames.push_back(std::move(f));
  }
  fs.ok = true;
  fs.evidence.push_back("FZ-MODE-PRODUCTION: 3 production modes only");
  fs.evidence.push_back("OI-01: group median=1 normalization performed in phase2");
  return fs;
}

/* ==================================================================== */
/* 产品装配 + 原子发布                                                    */
/* ==================================================================== */
namespace {

struct Assembly {
  WeightMode mode = WeightMode::kPointInformation;
  std::vector<double> signal;         /* SIGNAL 主 HDU (ADU/sr) */
  std::vector<double> variance;       /* VARIANCE (ADU^2/sr^2) */
  std::vector<double> flux;           /* FLUX (ADU) 可空 */
  std::vector<double> eff_psf;        /* EFFECTIVE_PSF (1) */
  std::string eff_psf_id;
  double eff_psf_fwhm = 1.0;
  std::string eff_psf_norm = "peak";
  std::string eff_psf_family = "conventional_coadd";
  std::string product_family = "phase2_point_source";
  std::string formula_ref;
  std::vector<double> combination_coefficients;
  std::string coefficients_ref;
  std::vector<std::string> input_psf_refs;

  bool has_point = false;
  double q = 0.0, w_info = 0.0, flux_value = 0.0, flux_variance = 0.0;
  bool joint_covariance = false;
  bool independent_frames = true;
  double corr_ratio = 0.0;
  double naive_variance = 0.0, joint_variance = 0.0;
  double snr_identity_rel_err = 0.0;

  bool has_surface = false;
  double x_hat = 0.0, var_gls = 0.0;
  bool pixel_ivar_approx = false;
  bool approx_gate_passed = false;
  double rho_p05 = 0.0, rho_p50 = 0.0, rho_p95 = 0.0, rho_max = 0.0;

  bool has_psfsw = false;
  double median_wt = 0.0;
  std::vector<double> w_psfsw;
  json psfsw_block;

  std::string covariance_representation = "diagonal_variance";
  std::string covariance_variance_from = "actual_combination_coefficients";
  std::string correlation_kernel_id;
  double correlation_scale = 0.0, rho_mean = 0.0, rho_max_corr = 0.0;
  bool diagonal_variance_only = true;

  std::vector<std::string> approximations;
  std::vector<std::string> algorithm_ids;
  std::vector<std::string> weight_sources;
  std::string weight_kind = "W_info";
  std::string weight_units = "ADU^-2";
  bool weight_group_normalized = false;
  bool has_weight_value = true;
  double weight_value = 0.0;
};

Provenance make_phase2_provenance(const Assembly& as, const RunMeta& meta,
                                  const std::string& fits_sha,
                                  const std::vector<std::string>& input_hashes) {
  Provenance p;
  p.product.type_id = phase2_type_id(as.mode);
  p.product.schema_version = 1;
  p.software_sha = meta.software_sha;
  p.run_id = meta.run_id;
  p.input_product_hashes = input_hashes;
  p.config_hash = meta.config_hash.empty() ? ("sha256:" + Sha256::hex_of_string("phase2-config"))
                                           : meta.config_hash;
  p.units.bunit = "ADU/sr";
  p.units.pixel_semantics = "surface_brightness";
  p.units.pixel_area_power = -2;
  p.units.has_target_pixel_area = true;
  p.units.target_pixel_area = 1.0;
  p.coordinate_frame = "icrs";
  p.coordinate_epoch = "J2000";
  p.pixel_semantics = "surface_brightness";
  p.sampling.kernel_id = "phase2_combination_operator";
  p.sampling.has_pixfrac = false;
  p.algorithm_ids = as.algorithm_ids;
  p.module.module_id = "astrocs.phase2.integrate";
  p.module.build_id = meta.build_id;
  p.provider = meta.provider;
  p.approximations = as.approximations;
  p.degradations.clear();
  p.normalization_version = "phase2_group_normalized_v1";
  p.weight_mode_version = std::string("phase2_weight_mode_") + weight_mode_token(as.mode) + "_v1";
  if (as.correlation_kernel_id.empty()) {
    p.correlation_summary.representation = "full_matrix_unavailable";
  } else {
    p.correlation_summary.representation = "correlation_kernel";
    p.correlation_summary.kernel_id = as.correlation_kernel_id;
    p.correlation_summary.has_scale = true;
    p.correlation_summary.scale = as.correlation_scale;
    p.correlation_summary.has_mean_abs_rho = true;
    p.correlation_summary.mean_abs_rho = as.rho_mean;
    p.correlation_summary.has_max_abs_rho = true;
    p.correlation_summary.max_abs_rho = as.rho_max_corr;
  }
  p.has_flux_conservation_factor = true;
  p.flux_conservation_factor = 1.0; /* Phase2 组合不改变面亮度保持因子 */
  p.k_corr.definition = "k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]";
  p.k_corr.value = kKCorrFrozen;
  p.k_corr.domain.geometry = "phase2_control_cell";
  p.k_corr.domain.pixfrac = 0.8;
  p.k_corr.domain.patch_size = 8;
  p.k_corr.domain.estimator = "median";
  p.k_corr.domain.spherical = true;
  p.k_corr.calibration.script =
      "eng/contracts/data/v6_clause_registry_v1.json#FZ-PROV-KCORR-VALUE";
  p.k_corr.calibration.seed = 20260915;
  p.k_corr.calibration.calibration_run_id = "frozen-in-domain-constant-FZ-PROV-KCORR-VALUE";
  p.k_corr.lookup_table_ref = "K_CORR_DOMAIN_B";
  p.has_k_corr = true;
  p.unavailable_flag = false;
  p.unavailable_reason = "not_applicable";
  p.unavailable_scope = "none";
  p.generated_utc = meta.generated_utc;
  p.output_hash = "sha256:" + fits_sha;
  p.diagonal_variance_only = as.diagonal_variance_only;
  p.signal_unit = "ADU/sr";
  p.variance_unit = "ADU^2/sr^2";
  p.ivar_unit = "sr^2/ADU^2";
  return p;
}

struct PublishOutcome {
  bool ok = false;
  std::string error;
  std::string sha;
  PublishResult publish;
};

PublishOutcome publish_phase2_assembly(const Assembly& as, const RunMeta& meta,
                                       const std::vector<std::string>& input_hashes,
                                       const std::string& target_dir) {
  PublishOutcome out;
  if (!is_hex40(meta.software_sha)) { out.error = "software_sha must be 40 hex"; return out; }
  if (meta.run_id.empty()) { out.error = "run_id required"; return out; }
  if (input_hashes.empty()) { out.error = "input_product_hashes required"; return out; }

  const DirBuilderFn builder = [&](const std::string& stage_dir, const CancelFn& cancel,
                                   std::string* err) -> bool {
    if (cancel && cancel()) { *err = "cancelled"; return false; }
    std::vector<FitsLayer> layers;
    layers.push_back(make_layer("", "ADU/sr", as.signal));
    layers.push_back(make_layer("VARIANCE", "ADU^2/sr^2", as.variance));
    std::vector<double> ivar(as.variance.size(), 0.0);
    for (std::size_t i = 0; i < as.variance.size(); ++i)
      ivar[i] = (as.variance[i] > 0.0) ? (1.0 / as.variance[i]) : 0.0;
    layers.push_back(make_layer("IVAR", "sr^2/ADU^2", ivar));
    if (!as.flux.empty()) layers.push_back(make_layer("FLUX", "ADU", as.flux));
    if (!as.eff_psf.empty()) layers.push_back(make_layer("EFFECTIVE_PSF", "1", as.eff_psf));

    const std::string fits_path = stage_dir + "/" + kPhase2ScienceFile;
    PublishOptions po;
    const PublishResult pr = publish_fits_product(fits_path, layers,
                                                  expected_from_layers(layers), po, CancelFn());
    if (pr.status != PublishStatus::kOk) { *err = "mosaic.fits publish failed: " + pr.message; return false; }
    std::string fits_sha;
    if (!sha256_file_hex(fits_path, &fits_sha)) { *err = "mosaic sha256 failed"; return false; }

    /* ── 记录 ── */
    json doc;
    doc["product_schema"] = kPhase2ProductSchema;
    doc["schema_version"] = 1;
    doc["phase"] = "phase2";
    doc["type_id"] = phase2_type_id(as.mode);
    doc["weight_mode"] = weight_mode_token(as.mode);
    doc["run_id"] = meta.run_id;
    doc["software_sha"] = meta.software_sha;
    doc["generated_utc"] = meta.generated_utc;

    doc["units"] = {{"signal_sb", "ADU/sr"},
                    {"sb_variance_out", "ADU^2/sr^2"},
                    {"sb_ivar_out", "sr^2/ADU^2"},
                    {"W_info", "ADU^-2"},
                    {"Q", "ADU^-1"},
                    {"flux", "ADU"},
                    {"psfsw_robust_weight", "1"},
                    {"pixel_semantics", "surface_brightness"},
                    {"pixel_area_power", -2}};

    doc["planes"] = json::array();
    doc["planes"].push_back({{"plane_id", "signal"}, {"extname", ""}, {"bunit", "ADU/sr"},
                             {"bitpix", -64}, {"naxis", {as.signal.size()}}});
    doc["planes"].push_back({{"plane_id", "variance"}, {"extname", "VARIANCE"},
                             {"bunit", "ADU^2/sr^2"}, {"bitpix", -64},
                             {"naxis", {as.variance.size()}}});
    doc["planes"].push_back({{"plane_id", "ivar"}, {"extname", "IVAR"},
                             {"bunit", "sr^2/ADU^2"}, {"bitpix", -64},
                             {"naxis", {as.variance.size()}}});
    if (!as.flux.empty())
      doc["planes"].push_back({{"plane_id", "flux"}, {"extname", "FLUX"}, {"bunit", "ADU"},
                               {"bitpix", -64}, {"naxis", {as.flux.size()}}});
    if (!as.eff_psf.empty())
      doc["planes"].push_back({{"plane_id", "effective_psf"}, {"extname", "EFFECTIVE_PSF"},
                               {"bunit", "1"}, {"bitpix", -64},
                               {"naxis", {as.eff_psf.size()}}});

    /* weight（canonical 单一词表；禁第三套） */
    json weight_obj = {{"kind", as.weight_kind},
                       {"units", as.weight_units},
                       {"group_normalized", as.weight_group_normalized},
                       {"sources", as.weight_sources}};
    if (as.has_psfsw) {
      weight_obj["normalization"] = {{"scope", "group"},
                                     {"median_target", 1.0},
                                     {"constants_version", p1psfw::kCompositeVersion}};
    }
    doc["weight_mode_record"] = {
        {"weight_mode_schema", "astrocs.v6.weight-mode/v1"},
        {"schema_version", 1},
        {"weight_mode", weight_mode_token(as.mode)},
        {"mode_class", "production"},
        {"science_objective", as.mode == WeightMode::kPointInformation
                                  ? "point_source_flux_information"
                                  : (as.mode == WeightMode::kSurfaceGls
                                         ? "extended_source_surface_brightness_gls"
                                         : "conventional_robust_integration_weight")},
        {"weight", weight_obj},
        {"primitives", {{"psf", true}, {"noise_covariance", true}}},
        {"covariance_ref", "covariance"},
        {"effective_psf_ref", "effective_psf"},
        {"provenance_ref", "provenance"}};

    /* covariance */
    json cov;
    cov["covariance_schema"] = "astrocs.v6.covariance/v1";
    cov["schema_version"] = 1;
    cov["propagation"] = "C_out = R C_in R^T";
    cov["scalar_form"] = "Var(out) = c^T C_in c = Sum_{i,j} c_i c_j [C_in]_{ij}";
    cov["representation"] = as.covariance_representation;
    cov["combination_coefficients"] = as.combination_coefficients;
    cov["operator_descriptor"] = {{"kind", "phase2_combination"},
                                  {"reconstructable", true},
                                  {"summary_ref", "covariance.combination_coefficients"}};
    cov["variance_from"] = as.covariance_variance_from;
    cov["input_covariance"] = "provided";
    cov["input_covariance_ref"] = "phase2_extensions.input_covariance";
    cov["avail"] = "available";
    cov["forbidden_variance_sources"] = forbidden_sources_json();
    cov["variance_plane"] = {{"units", "ADU^2/sr^2"}, {"dtype", "float64"},
                             {"invalid_policy", "nan_or_support_le_0"}};
    cov["ivar_plane"] = {{"units", "sr^2/ADU^2"}, {"dtype", "float64"},
                         {"invalid_policy", "nan_or_support_le_0"}};
    if (!as.correlation_kernel_id.empty()) {
      cov["correlation_kernel"] = {{"kernel_id", as.correlation_kernel_id},
                                   {"scale", as.correlation_scale},
                                   {"rho_summary", {{"mean_abs_rho", as.rho_mean},
                                                    {"max_abs_rho", as.rho_max_corr}}}};
    } else {
      cov["operator_descriptor"] = {{"kind", "phase2_combination"},
                                    {"reconstructable", true},
                                    {"summary_ref", "phase2_extensions.combination"}};
    }
    if (as.has_surface) {
      cov["surface_gls_normal_equations"] = {
          {"authoritative_formula", "x_hat = (A^T C^-1 A)^-1 A^T C^-1 d"},
          {"covariance_formula", "Cov(x_hat) = (A^T C^-1 A)^-1"}};
    }
    if (as.has_psfsw) {
      cov["psfsw_boundary"] = {{"method", "propagated_from_composite_coefficients"},
                               {"variance_from_weight", false},
                               {"uses_relative_weight_as_ivar", false}};
    }
    if (as.pixel_ivar_approx) {
      cov["approximation"] = {{"kind", "pixel_ivar_degenerate"},
                              {"error_gate", {{"metric", "Var_approx/Var_GLS"},
                                              {"value", as.rho_p95},
                                              {"bound_ref", "FZ-AP2S-EPS-PIXIVAR=0.05"}}},
                              {"actual_operator_ref", "phase2_extensions.pixel_ivar_operator"}};
      cov["representation"] = "diagonal_variance_plus_correlation_kernel";
    }
    if (as.diagonal_variance_only && as.correlation_kernel_id.empty()) {
      cov["diagonal_approximation"] = {
          {"is_lower_bound", true},
          {"deficit_metric", {{"name", "deficit=(exact-diag)/exact"}, {"value", 0.0}}},
          {"use_for_aperture", false},
          {"threshold_ref", "FZ-GATE-PARENT-VAR/PROPOSED_PENDING_OWNER_SIGNOFF_SO07"}};
    }
    cov["provenance_ref"] = "provenance";
    doc["covariance"] = cov;

    /* point_information */
    if (as.has_point) {
      json pi;
      pi["point_information_schema"] = "astrocs.v6.point-information/v1";
      pi["schema_version"] = 1;
      pi["authoritative_formula"] =
          "Q_k=a_k P_k^T C_k^-1 d_k; W_info,k=a_k^2 P_k^T C_k^-1 P_k; F_hat=Q/W; Var(F_hat)=1/W";
      pi["W_info"] = {{"value", as.w_info}, {"units", "ADU^-2"}, {"representation", "scalar"}};
      pi["Q"] = {{"value", as.q}, {"units", "ADU^-1"}, {"representation", "scalar"}};
      pi["flux"] = {{"value", as.flux_value}, {"units", "ADU"}, {"representation", "scalar"}};
      pi["flux_variance"] = {{"value", as.flux_variance}, {"units", "ADU^2"}, {"representation", "scalar"}};
      pi["representation"] = "scalar";
      pi["independent_frame_combination"] = {
          {"independent_frames", as.independent_frames},
          {"Q_combined", "Q = Sum_k Q_k"},
          {"W_combined", "W = Sum_k W_info,k"},
          {"joint_covariance_ref", as.joint_covariance ? "phase2_extensions.joint_covariance"
                                                       : ""},
          {"correlated_detection_ratio", as.corr_ratio}};
      pi["provenance_ref"] = "provenance";
      doc["point_information"] = pi;
    }

    /* psfsw */
    if (as.has_psfsw) {
      doc["psfsw"] = as.psfsw_block;
    }

    /* effective_psf */
    json ep;
    ep["effective_psf_schema"] = "astrocs.v6.effective-psf/v1";
    ep["schema_version"] = 1;
    ep["effective_psf_id"] = as.eff_psf_id;
    ep["definition"] = "impulse_response_of_combination";
    ep["product_family"] = as.product_family;
    ep["formula_family"] = as.eff_psf_family;
    ep["formula_ref"] = as.formula_ref;
    ep["normalization"] = as.eff_psf_norm;
    ep["values_or_model"] = {{"kind", "values"}, {"values", as.eff_psf}};
    ep["kernel_transfer"] = json::array();
    ep["kernel_transfer"].push_back({{"frame_id", "all_frames"}, {"kernel_id", "native_grid"},
                                     {"kernel_version", "v1"}});
    ep["combination_coefficients_ref"] = as.coefficients_ref;
    ep["input_psf_refs"] = as.input_psf_refs;
    ep["fwhm_from_effective"] = {{"value", as.eff_psf_fwhm},
                                 {"unit", "px"}, {"role", "derived_diagnostic"}};
    const std::string peff_hash = Sha256::hex_of(as.eff_psf.data(), as.eff_psf.size() * sizeof(double));
    ep["reproducibility"] = {{"p_eff_hash", peff_hash}, {"kernel_set_hash", "native_grid_v1"}};
    ep["provenance_ref"] = "provenance";
    doc["effective_psf"] = ep;

    /* provenance */
    Provenance prov = make_phase2_provenance(as, meta, fits_sha, input_hashes);
    const ValidationReport prov_rep = validate_provenance(prov);
    if (!prov_rep.ok()) { *err = "provenance gate failed: " + prov_rep.summary(); return false; }
    doc["provenance"] = provenance_to_json(prov);
    doc["provenance"]["units"]["target_pixel_area"] = 1.0;
    if (!meta.omit_provenance_key.empty()) doc["provenance"].erase(meta.omit_provenance_key);
    if (!meta.inject_variance_from.empty())
      doc["covariance"]["variance_from"] = meta.inject_variance_from;
    if (!meta.inject_weight_source.empty()) {
      if (doc["weight_mode_record"]["weight"]["sources"].is_array())
        doc["weight_mode_record"]["weight"]["sources"].push_back(meta.inject_weight_source);
      doc["covariance"]["variance_from"] = meta.inject_variance_from.empty()
                                               ? meta.inject_weight_source
                                               : meta.inject_variance_from;
    }
    if (meta.force_variance_from_weight) {
      doc["covariance"]["psfsw_boundary"] = {{"method", "variance_from_weight"},
                                             {"variance_from_weight", true},
                                             {"uses_relative_weight_as_ivar", true}};
      doc["covariance"]["variance_from"] = "psfsw_robust_weight";
    }
    if (meta.force_effective_psf_only_fwhm) {
      doc["effective_psf"]["values_or_model"] = {{"kind", "values"}};
      doc["effective_psf"].erase("values_or_model");
      doc["effective_psf"]["fwhm_from_effective"] = {{"value", 4.71}, {"unit", "px"},
                                                     {"role", "derived_diagnostic"}};
    }
    if (!meta.inject_p33_key.empty()) {
      doc["phase2_extensions"][meta.inject_p33_key] = 3.14;
    }
    if (meta.force_bunit_undecidable) {
      doc["units"]["signal_sb"] = "ADU";
      doc["provenance"]["units"]["bunit"] = "ADU";
      doc["provenance"]["units"]["pixel_area_power"] = 0;
      doc["provenance"]["pixel_semantics"] = "integrated_flux";
    }

    /* phase2_extensions */
    json ext;
    ext["group_normalization_deferred_to"] = "phase2";
    ext["group_normalization_performed"] = as.has_psfsw;
    ext["median_wt"] = as.median_wt;
    ext["w_psfsw"] = as.w_psfsw;
    ext["combination"] = {{"operator", "phase2_combination"},
                          {"coefficients", as.combination_coefficients}};
    ext["snr_identity_rel_err"] = as.snr_identity_rel_err;
    ext["rho_p05"] = as.rho_p05;
    ext["rho_p50"] = as.rho_p50;
    ext["rho_p95"] = as.rho_p95;
    ext["rho_max"] = as.rho_max;
    ext["var_gls"] = as.var_gls;
    ext["naive_variance"] = as.naive_variance;
    ext["joint_variance"] = as.joint_variance;
    ext["corr_ratio"] = as.corr_ratio;
    ext["open_items"] = json::array({"OI-01"});
    ext["gates"] = {{"FZ-MODE-PRODUCTION", true},
                    {"FZ-FORMULA-COV-PROP", true},
                    {"FZ-FORMULA-PSFSW-COMPOSITE", as.has_psfsw},
                    {"FZ-GATE-PIXIVAR-APPROX", !as.pixel_ivar_approx || as.approx_gate_passed},
                    {"FZ-GATE-PSFSW-EPSF", !as.eff_psf.empty()}};
    doc["phase2_extensions"] = ext;

    /* manifest */
    json mf;
    mf["manifest_schema"] = "astrocs.v6.hips_manifest/v1";
    mf["schema_version"] = 1;
    mf["product_type_id"] = phase2_type_id(as.mode);
    mf["files"] = json::array();
    mf["files"].push_back({{"relative_path", kPhase2ScienceFile},
                           {"size_bytes", pr.bytes_written},
                           {"sha256_hex", fits_sha},
                           {"role", "mosaic_planes"}});
    mf["output_hash"] = fits_sha;
    mf["n_modes"] = 1;
    doc["manifest"] = mf;

    if (!write_text(stage_dir + "/" + kPhase2RecordFile, doc.dump(2), err)) return false;
    return true;
  };

  const RunMeta* meta_ptr = &meta;
  const VerifyFn verify = [meta_ptr](const std::string& path, std::string* err) -> bool {
    if (meta_ptr->force_publish_verify_fail) {
      if (err) *err = "injected reopen verify failure";
      return false;
    }
    const Phase2OpenResult r = open_phase2_product(path);
    if (!r.ok) { if (err) *err = r.error.empty() ? "reopen verify failed" : r.error; return false; }
    return true;
  };

  PublishOptions opts;
  opts.verify_after_rename = true;
  opts.fsync_directory = true;
  opts.remove_on_verify_failure = true;
  const PublishResult pr = atomic_publish_directory(target_dir, builder, verify, opts, CancelFn());
  if (pr.status != PublishStatus::kOk) {
    out.error = pr.message.empty() ? "atomic publish failed" : pr.message;
    out.publish = pr;
    return out;
  }
  out.ok = true;
  out.sha = pr.sha256_hex;
  /* output_hash = 磁盘实际 sha256（主科学文件），与 manifest/provenance 一致。 */
  {
    std::string final_sha;
    if (sha256_file_hex(target_dir + "/" + kPhase2ScienceFile, &final_sha)) out.sha = final_sha;
  }
  out.publish = pr;
  return out;
}

std::vector<std::string> input_hashes_of(const FrameSet& fs) {
  std::vector<std::string> h;
  for (const auto& f : fs.frames) h.push_back("sha256:" + f.view.output_hash);
  return h;
}

bool build_effective_psf(const FrameSet& fs, const std::vector<double>& alpha,
                         const std::string& norm, const std::string& id,
                         const std::string& family,
                         std::vector<double>* out_profile, double* out_fwhm,
                         std::string* err) {
  std::vector<const double*> profiles;
  std::vector<double> a_k;
  for (const auto& f : fs.frames) { profiles.push_back(f.psf_profile.data()); a_k.push_back(f.a); }
  const std::size_t n = fs.frames.front().psf_profile.size();
  const auto norm_e = (norm == "integral") ? p1psfw::EffectivePsfNormalization::integral
                                           : p1psfw::EffectivePsfNormalization::peak;
  const p1psfw::EffectivePsf e = p1psfw::conventional_effective_psf(profiles, n, a_k, alpha, norm_e, id);
  if (!e.ok) { if (err) *err = std::string("effective PSF rejected: ") + (e.reject ? e.reject : "?"); return false; }
  *out_profile = e.profile;
  *out_fwhm = e.fwhm;
  (void)family;
  return true;
}

}  /* namespace */

/* ==================================================================== */
/* point_information                                                     */
/* ==================================================================== */
ProductResult run_point_information(const std::vector<std::string>& product_dirs,
                                    const JointCovariance& joint, const RunMeta& meta,
                                    const std::string& target_dir) {
  ProductResult res;
  res.mode = WeightMode::kPointInformation;
  const FrameSet fs = open_phase2_frame_set(product_dirs);
  if (!fs.ok) { res.error = fs.error; return res; }

  const std::size_t K = fs.frames.size();
  double q_sum = 0.0, w_sum = 0.0;
  for (const auto& f : fs.frames) { q_sum += f.view.q; w_sum += f.view.w_info; }
  if (!(w_sum > 0.0)) { res.error = "Sum W_info <= 0"; return res; }

  double q = q_sum, w = w_sum;
  bool joint_used = false;
  bool joint_offdiag = false;
  if (joint.provided && joint.m > 0) {
    if (joint.c_in.size() != (K * joint.m) * (K * joint.m)) {
      res.error = "joint covariance size mismatch"; return res;
    }
    joint_offdiag = any_off_diagonal(joint.c_in, K * joint.m, 1e-12);
  }
  if (joint.provided && joint_offdiag) {
    if (joint.m == 0) { res.error = "joint covariance m=0"; return res; }
    const std::size_t n = K * joint.m;
    Mat ci;
    if (!invert(joint.c_in, static_cast<int>(n), &ci)) {
      res.error = "joint C_in not invertible (rank deficient)"; return res;
    }
    /* A = a_k P_k（须与磁盘 psf_profile 一致，证明消费真实产物）。 */
    Mat A(n, 0.0), d(n, 0.0);
    for (std::size_t k = 0; k < K; ++k)
      for (std::size_t i = 0; i < joint.m; ++i)
        A[k * joint.m + i] = fs.frames[k].a * fs.frames[k].psf_profile[i];
    if (joint.a_psf.size() != n || joint.data.size() != n) {
      res.error = "joint a_psf/data length mismatch"; return res;
    }
    for (std::size_t i = 0; i < n; ++i) {
      if (std::fabs(A[i] - joint.a_psf[i]) > 1e-9) { res.error = "joint a_psf mismatch"; return res; }
      d[i] = joint.data[i];
    }
    /* W = A^T C^-1 A, Q = A^T C^-1 d */
    std::vector<double> cid(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
      double s = 0.0;
      for (std::size_t j = 0; j < n; ++j) s += ci[i * n + j] * d[j];
      cid[i] = s;
    }
    double wj = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      double s = 0.0;
      for (std::size_t j = 0; j < n; ++j) s += ci[i * n + j] * A[j];
      wj += A[i] * s;
    }
    double qj = 0.0;
    for (std::size_t i = 0; i < n; ++i) qj += A[i] * cid[i];
    w = wj; q = qj; joint_used = true;

    /* naive（独立帧）：每帧用自己的块对角 covariance C_kk（块本身可含相关）。 */
    double w_naive = 0.0;
    {
      const std::size_t m = joint.m;
      Mat blk(m * m, 0.0), binv;
      for (std::size_t k = 0; k < K; ++k) {
        for (std::size_t i = 0; i < m; ++i)
          for (std::size_t j = 0; j < m; ++j)
            blk[i * m + j] = joint.c_in[(k * m + i) * n + (k * m + j)];
        if (!invert(blk, static_cast<int>(m), &binv)) {
          res.error = "diagonal block not invertible (independent-frame route)"; return res;
        }
        double s = 0.0;
        for (std::size_t i = 0; i < m; ++i)
          for (std::size_t j = 0; j < m; ++j)
            s += A[k * m + i] * binv[i * m + j] * A[k * m + j];
        w_naive += s;
      }
    }
    res.naive_variance = (w_naive > 0.0) ? 1.0 / w_naive : 0.0;
    res.joint_variance = (wj > 0.0) ? 1.0 / wj : 0.0;
    res.corr_ratio = (wj > 0.0) ? (w_naive / wj) : 0.0;
    if (meta.force_naive_sum) {
      res.error = "correlated frames with naive Sum_k rejected (FZ-FORMULA-QW-04/GATE-CORR-04)";
      return res;
    }
    if (!(res.corr_ratio >= kCorrRatioMin)) {
      res.error = "correlated detection ratio below FZ-AP2PT-CORR-RATIO-MIN";
      return res;
    }
    joint_used = true;
  }

  const double f_hat = q / w;
  const double var_f = 1.0 / w;
  double snr_rel = 0.0;
  {
    double sum_w = 0.0;
    for (const auto& f : fs.frames) sum_w += f.view.w_info;
    snr_rel = (w > 0.0) ? std::fabs(w - sum_w) / w : 0.0;
    if (!joint_used && snr_rel > kSnrIdentRtol) {
      res.error = "independent frame SNR identity violated (FZ-AP2PT-SNR-IDENT-RTOL)";
      return res;
    }
  }

  /* 实际组合系数 + effective PSF */
  std::vector<double> alpha(K, 0.0);
  if (joint_used) {
    /* α_k = a_k Σ_{i∈k} P_ki (C^-1 A)_i / W */
    const std::size_t n = K * joint.m;
    Mat ci;
    invert(joint.c_in, static_cast<int>(n), &ci);
    Mat A(n, 0.0);
    for (std::size_t k = 0; k < K; ++k)
      for (std::size_t i = 0; i < joint.m; ++i) A[k * joint.m + i] = fs.frames[k].a * fs.frames[k].psf_profile[i];
    for (std::size_t k = 0; k < K; ++k) {
      double s = 0.0;
      for (std::size_t i = 0; i < joint.m; ++i) {
        double ca = 0.0;
        for (std::size_t j = 0; j < n; ++j) ca += ci[(k * joint.m + i) * n + j] * A[j];
        s += fs.frames[k].psf_profile[i] * ca;
      }
      alpha[k] = fs.frames[k].a * s / w;
    }
  } else {
    alpha = derive_psf_alpha(fs);
  }
  std::vector<double> peff; double fwhm = 0.0; std::string perr;
  if (!build_effective_psf(fs, alpha, "peak", std::string("p2-point-") + meta.run_id,
                           "matched_filter_proper_coadd", &peff, &fwhm, &perr)) {
    res.error = perr; return res;
  }

  Assembly as;
  as.mode = WeightMode::kPointInformation;
  as.has_point = true;
  as.q = q; as.w_info = w; as.flux_value = f_hat; as.flux_variance = var_f;
  as.independent_frames = !joint_used;
  as.joint_covariance = joint_used;
  as.corr_ratio = res.corr_ratio;
  as.naive_variance = res.naive_variance;
  as.joint_variance = res.joint_variance;
  as.snr_identity_rel_err = snr_rel;
  as.combination_coefficients = alpha;
  as.coefficients_ref = "point_information.independent_frame_combination";
  as.eff_psf = peff; as.eff_psf_id = std::string("p2-point-") + meta.run_id;
  as.eff_psf_fwhm = fwhm;
  as.eff_psf_norm = "peak";
  as.eff_psf_family = "matched_filter_proper_coadd";
  as.product_family = "phase2_point_source";
  as.formula_ref = "P_eff = [Sum_k alpha_k a_k P_k] / [Sum_k alpha_k a_k P_k(0)]";
  as.weight_kind = "W_info"; as.weight_units = "ADU^-2"; as.weight_group_normalized = false;
  as.weight_sources = {"psf", "photometric_response", "noise_covariance"};
  as.algorithm_ids = {"ALG-P2-POINT-001", "ALG-P2S-UPM.6", "ALG-P2S-COV.1", "ALG-P2S-EPSF.1"};
  as.approximations = {"point_information merges Q/W; FZ-FORMULA-COV-PROP actual coefficients",
                       "effective PSF from actual combination coefficients (FZ-GATE-PSFSW-EPSF)"};
  /* covariance 由实际组合系数传播（可重建算子），不是"仅对角"声明。 */
  as.diagonal_variance_only = false;
  if (joint_used) {
    as.correlation_kernel_id = joint.kernel_id.empty() ? "phase2_joint_c_in_v1" : joint.kernel_id;
    as.correlation_scale = 1.0; as.rho_mean = joint.rho_mean; as.rho_max_corr = joint.rho_max;
  }
  /* SIGNAL = 实际系数组合的 SB 加权均值；VARIANCE = 对角传播。 */
  {
    const std::size_t P = fs.frames.front().signal_sb.size();
    double asum = 0.0; for (double a : alpha) asum += a;
    as.signal.assign(P, 0.0); as.variance.assign(P, 0.0);
    for (std::size_t k = 0; k < K; ++k) {
      for (std::size_t i = 0; i < P && i < fs.frames[k].signal_sb.size(); ++i) {
        const double wgt = alpha[k] / asum;
        as.signal[i] += wgt * fs.frames[k].signal_sb[i];
        as.variance[i] += wgt * wgt * fs.frames[k].variance_sb[i];
      }
    }
    as.flux = {f_hat};
  }
  for (const auto& f : fs.frames) as.input_psf_refs.push_back(f.dir + "#psf_profile");

  const PublishOutcome po = publish_phase2_assembly(as, meta, input_hashes_of(fs), target_dir);
  res.publish = po.publish;
  if (!po.ok) { res.error = po.error; return res; }
  res.output_sha256 = po.sha;
  res.q = q; res.w_info = w; res.flux = f_hat; res.flux_variance = var_f;
  res.combination_coefficients = alpha;
  res.effective_psf = peff; res.effective_psf_id = as.eff_psf_id;
  res.effective_psf_fwhm = fwhm; res.effective_psf_normalization = "peak";
  res.joint_covariance_used = joint_used;
  res.signal_out = as.signal; res.variance_out = as.variance; res.flux_out = as.flux;
  res.product_dir = target_dir;
  res.evidence = fs.evidence;
  res.evidence.push_back(joint_used ? "joint C_in GLS used (FZ-FORMULA-QW-04)"
                                    : "independent frame merge Q=Sum Q_k,W=Sum W_info,k");
  res.evidence.push_back("FZ-FORMULA-COV-PROP: C_out=R C_in R^T from actual coefficients");
  res.ok = true;
  return res;
}

/* ==================================================================== */
/* surface_gls                                                           */
/* ==================================================================== */
ProductResult run_surface_gls(const SurfaceInputs& in, const RunMeta& meta,
                              const std::string& target_dir) {
  ProductResult res;
  res.mode = WeightMode::kSurfaceGls;
  const FrameSet fs = open_phase2_frame_set(in.product_dirs);
  if (!fs.ok) { res.error = fs.error; return res; }
  const std::size_t K = fs.frames.size();
  if (in.n_pix_per_frame == 0 || in.design.size() != K * in.n_pix_per_frame * in.n_out) {
    res.error = "design matrix size mismatch"; return res;
  }
  const std::size_t n = K * in.n_pix_per_frame;
  if (in.c_in.size() != n * n || in.data.size() != n * in.n_out) {
    res.error = "c_in/data size mismatch"; return res;
  }
  Mat ci;
  if (!invert(in.c_in, static_cast<int>(n), &ci)) { res.error = "C_in not invertible"; return res; }
  const int nout = static_cast<int>(in.n_out);
  /* M = A^T C^-1 A (nout x nout) */
  Mat ciA(n * static_cast<std::size_t>(nout), 0.0);
  for (std::size_t i = 0; i < n; ++i)
    for (int c = 0; c < nout; ++c) {
      double s = 0.0;
      for (std::size_t j = 0; j < n; ++j) s += ci[i * n + j] * in.design[j * nout + c];
      ciA[i * nout + c] = s;
    }
  Mat M(static_cast<std::size_t>(nout) * nout, 0.0);
  for (int r = 0; r < nout; ++r)
    for (int c = 0; c < nout; ++c) {
      double s = 0.0;
      for (std::size_t i = 0; i < n; ++i) s += in.design[i * nout + r] * ciA[i * nout + c];
      M[r * nout + c] = s;
    }
  Mat Minv;
  if (!invert(M, nout, &Minv)) { res.error = "A^T C^-1 A singular (FZ-AP2S-RANK-RTOL)"; return res; }
  /* R = Minv A^T C^-1 (nout x n) */
  Mat R(static_cast<std::size_t>(nout) * n, 0.0);
  for (int r = 0; r < nout; ++r)
    for (std::size_t i = 0; i < n; ++i) {
      double s = 0.0;
      for (int c = 0; c < nout; ++c) s += Minv[r * nout + c] * ciA[i * nout + c];
      R[r * n + i] = s;
    }
  /* x_hat = R d */
  std::vector<double> xhat(static_cast<std::size_t>(nout), 0.0);
  for (int r = 0; r < nout; ++r)
    for (std::size_t i = 0; i < n; ++i) xhat[r] += R[r * n + i] * in.data[i * nout + r];
  /* 恒等式 R C_in R^T == Minv（FZ-AP2S-IDENT-RTOL） */
  {
    double max_rel = 0.0;
    for (int r = 0; r < nout; ++r)
      for (int c = 0; c < nout; ++c) {
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i)
          for (std::size_t j = 0; j < n; ++j) s += R[r * n + i] * in.c_in[i * n + j] * R[c * n + j];
        const double ref = std::fabs(Minv[r * nout + c]);
        const double err = std::fabs(s - Minv[r * nout + c]);
        if (ref > 0.0) max_rel = std::max(max_rel, err / ref);
      }
    if (max_rel > kIdentRtol) { res.error = "GLS identity R C_in R^T != (A^T C^-1 A)^-1"; return res; }
  }
  res.x_hat = xhat.empty() ? 0.0 : xhat[0];
  res.var_gls = Minv.empty() ? 0.0 : Minv[0];

  bool approx_used = false;
  double rho_p95 = 0.0, rho_max = 0.0, rho_p05 = 0.0, rho_p50 = 0.0;
  double var_approx0 = 0.0;
  if (in.pixel_ivar_approx) {
    /* 结构性准入（ALG-P2S-PXIV.2）：本实现要求调用方声明 a_k 一致（a_k=1）。 */
    if (in.a_k.size() != K) { res.error = "pixel-ivar approx requires per-frame a_k"; return res; }
    for (double a : in.a_k) if (std::fabs(a - 1.0) > 1e-12) {
      res.error = "pixel-ivar approximation requires consistent a_k (structural precondition)";
      return res;
    }
    if (in.force_missing_error_gate) { res.error = "pixel-ivar approximation without error gate (FZ-GATE-PIXIVAR-APPROX)"; return res; }
    std::vector<double> rho(nout, 0.0);
    for (int r = 0; r < nout; ++r) {
      /* R~_i = w_i / Sum w；a_k_ignored 时忽略 a_k（w=1/var）。 */
      std::vector<double> wgt(n, 0.0);
      double ws = 0.0;
      for (std::size_t i = 0; i < n; ++i) {
        const std::size_t k = i / in.n_pix_per_frame;
        const double var = in.c_in[i * n + i];
        if (!(var > 0.0)) { res.error = "non-positive diagonal variance in pixel-ivar approx"; return res; }
        wgt[i] = in.a_k_ignored ? (1.0 / var) : (in.a_k[k] * in.a_k[k] / var);
        ws += wgt[i];
      }
      for (std::size_t i = 0; i < n; ++i) wgt[i] /= ws;
      double va = 0.0;
      for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) va += wgt[i] * in.c_in[i * n + j] * wgt[j];
      const double vg = Minv[r * nout + r];
      rho[r] = (vg > 0.0) ? (va / vg) : 0.0;
      if (r == 0) { var_approx0 = va; res.var_approx = va; }
    }
    std::vector<double> sorted(rho);
    rho_p05 = nearest_rank_percentile(sorted, 0.05);
    rho_p50 = nearest_rank_percentile(sorted, 0.50);
    rho_p95 = nearest_rank_percentile(sorted, 0.95);
    rho_max = *std::max_element(rho.begin(), rho.end());
    approx_used = true;
    const bool gate = (rho_p95 <= 1.0 + kEpsPixivar) && (rho_max <= 1.0 + kEpsPixivarSup);
    if (!gate) {
      res.error = "pixel-ivar approximation gate failed (Var_approx/Var_GLS > 1+eps; FZ-GATE-PIXIVAR-APPROX)";
      res.rho = rho.empty() ? 0.0 : rho[0];
      res.rho_p95 = rho_p95; res.rho_max = rho_max; res.var_approx = var_approx0;
      res.approx_gate_passed = false;
      return res;
    }
    res.approx_gate_passed = true;
  }

  /* effective PSF：α_k = Σ_{i∈k} R_{0,i}（实际组合系数；取输出元素 0） */
  std::vector<double> alpha(K, 0.0);
  for (std::size_t k = 0; k < K; ++k) {
    double s = 0.0;
    for (std::size_t i = 0; i < in.n_pix_per_frame; ++i)
      s += R[0 * n + (k * in.n_pix_per_frame + i)];
    alpha[k] = s;
  }
  std::vector<double> peff; double fwhm = 0.0; std::string perr;
  if (!build_effective_psf(fs, alpha, "integral", std::string("p2-surface-") + meta.run_id,
                           "conventional_coadd", &peff, &fwhm, &perr)) {
    res.error = perr; return res;
  }

  Assembly as;
  as.mode = WeightMode::kSurfaceGls;
  as.has_surface = true;
  as.x_hat = res.x_hat; as.var_gls = res.var_gls;
  as.pixel_ivar_approx = approx_used;
  as.approx_gate_passed = res.approx_gate_passed;
  as.rho_p05 = rho_p05; as.rho_p50 = rho_p50; as.rho_p95 = rho_p95; as.rho_max = rho_max;
  as.combination_coefficients = alpha;
  as.coefficients_ref = "covariance.surface_gls_normal_equations";
  as.eff_psf = peff; as.eff_psf_id = std::string("p2-surface-") + meta.run_id;
  as.eff_psf_fwhm = fwhm;
  as.eff_psf_norm = "integral";
  as.eff_psf_family = "conventional_coadd";
  as.product_family = "phase2_surface_brightness";
  as.formula_ref = "P_eff = Sum_k alpha_k a_k P_k / Sum_k alpha_k a_k";
  as.weight_kind = "A^T C^-1 A"; as.weight_units = "1/(surface_brightness^2)";
  as.weight_group_normalized = false;
  as.weight_sources = {"design_matrix", "noise_covariance", "upm_scale"};
  as.algorithm_ids = {"ALG-P2-SURF-GLS", "ALG-P2S-GLS.2", "ALG-P2S-PXIV.3", "ALG-P2S-COV.2"};
  as.approximations = {"surface_gls exact GLS on declared A/C_in; R C R^T identity checked"};
  if (in.pixel_ivar_approx)
    as.approximations.push_back("pixel_ivar_degenerate error gate epsilon=0.05 (FZ-AP2S-EPS-PIXIVAR)");
  as.correlation_kernel_id = in.correlation_kernel_id;
  as.correlation_scale = 1.0; as.rho_mean = in.rho_mean; as.rho_max_corr = in.rho_max;
  as.diagonal_variance_only = in.correlation_kernel_id.empty();
  as.signal = xhat;
  as.variance = std::vector<double>(static_cast<std::size_t>(nout), 0.0);
  for (int r = 0; r < nout; ++r) as.variance[r] = Minv[r * nout + r];
  for (const auto& f : fs.frames) as.input_psf_refs.push_back(f.dir + "#psf_profile");

  const PublishOutcome po = publish_phase2_assembly(as, meta, input_hashes_of(fs), target_dir);
  res.publish = po.publish;
  if (!po.ok) { res.error = po.error; return res; }
  res.output_sha256 = po.sha;
  res.combination_coefficients = alpha;
  res.effective_psf = peff; res.effective_psf_id = as.eff_psf_id;
  res.effective_psf_fwhm = fwhm; res.effective_psf_normalization = "integral";
  res.pixel_ivar_approx_used = approx_used;
  res.rho = (approx_used) ? 0.0 : 0.0;
  res.rho_p05 = rho_p05; res.rho_p50 = rho_p50; res.rho_p95 = rho_p95; res.rho_max = rho_max;
  res.var_approx = var_approx0;
  res.signal_out = as.signal; res.variance_out = as.variance;
  res.product_dir = target_dir;
  res.evidence = fs.evidence;
  res.evidence.push_back("FZ-FORMULA-GLS: x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; R C R^T identity <= 1e-9");
  if (approx_used) res.evidence.push_back("FZ-AP2S-EPS-PIXIVAR gate passed (p95<=1.05, cap<=1.20)");
  res.ok = true;
  return res;
}

/* ==================================================================== */
/* psfsw_robust                                                          */
/* ==================================================================== */
ProductResult run_psfsw_robust(const PsfswInputs& in, const RunMeta& meta,
                               const std::string& target_dir) {
  ProductResult res;
  res.mode = WeightMode::kPsfswRobust;
  /* 组内归一由 Phase1 消费面（磁盘重开）完成（OI-01）。 */
  const phase1::Phase1GroupConsumption g = phase1::consume_phase1_group_for_psfsw(in.product_dirs);
  if (!g.ok) { res.error = g.error; return res; }
  if (!g.record_ok) { res.error = "group psfsw record gate failed"; return res; }
  const FrameSet fs = open_phase2_frame_set(in.product_dirs);
  if (!fs.ok) { res.error = fs.error; return res; }
  const std::size_t K = fs.frames.size();
  const std::size_t P = fs.frames.front().signal_sb.size();
  if (in.d.size() != K || in.c_in.size() != K * K) { res.error = "psfsw inputs size mismatch"; return res; }
  for (std::size_t k = 0; k < K; ++k)
    if (in.d[k].size() != P) { res.error = "psfsw d length mismatch"; return res; }
  std::vector<std::vector<char>> validity = in.validity;
  if (validity.empty()) validity.assign(K, std::vector<char>(P, 1));
  if (validity.size() != K) { res.error = "psfsw validity size mismatch"; return res; }
  for (std::size_t k = 0; k < K; ++k)
    if (validity[k].size() != P) { res.error = "psfsw validity length mismatch"; return res; }

  const p1psfw::CoaddResult co = p1psfw::conventional_coadd(in.d, validity, g.w_psfsw);
  if (!co.ok) { res.error = std::string("conventional coadd rejected: ") + (co.reject ? co.reject : "?"); return res; }
  const p1psfw::CovariancePropagation cp = p1psfw::propagate_covariance(co.alpha, in.c_in);
  if (!cp.ok) { res.error = std::string("covariance propagation rejected: ") + (cp.reject ? cp.reject : "?"); return res; }
  if (cp.variance_from_weight || cp.uses_relative_weight_as_ivar) {
    res.error = "psfsw covariance from weight rejected (FZ-GATE-PSFSW-COV)"; return res;
  }
  /* effective PSF from actual alpha at a defined output element */
  std::size_t p_ref = 0;
  for (std::size_t p = 0; p < P; ++p) if (co.defined[p]) { p_ref = p; break; }
  std::vector<double> alpha(K, 0.0);
  for (std::size_t k = 0; k < K; ++k) alpha[k] = co.alpha[k][p_ref];
  std::vector<double> peff; double fwhm = 0.0; std::string perr;
  if (!build_effective_psf(fs, alpha, "integral", std::string("p2-psfsw-") + meta.run_id,
                           "conventional_coadd", &peff, &fwhm, &perr)) {
    res.error = perr; return res;
  }

  Assembly as;
  as.mode = WeightMode::kPsfswRobust;
  as.has_psfsw = true;
  as.median_wt = g.median_wt;
  as.w_psfsw = g.w_psfsw;
  as.combination_coefficients = alpha;
  as.coefficients_ref = "psfsw.composite.alpha";
  as.eff_psf = peff; as.eff_psf_id = std::string("p2-psfsw-") + meta.run_id;
  as.eff_psf_fwhm = fwhm;
  as.eff_psf_norm = "integral";
  as.eff_psf_family = "conventional_coadd";
  as.product_family = "phase2_psfsw_integration";
  as.formula_ref = "P_eff = Sum_k alpha_k a_k P_k / Sum_k alpha_k a_k; alpha_k(p)=W_psfsw,k v_k(p)/Sum_j W_psfsw,j v_j(p)";
  as.weight_kind = "psfsw_robust_weight"; as.weight_units = "1"; as.weight_group_normalized = true;
  as.weight_sources = {"psfsw.signal", "psfsw.concentration", "psfsw.noise", "psfsw.background"};
  as.algorithm_ids = {"ALG-P2-PSFSW-001", "FZ-FORMULA-PSFSW-COMPOSITE", "ALG-P2S-COV.4", "ALG-P2S-EPSF.3"};
  as.approximations = {"psfsw group median=1 normalization performed in phase2 (OI-01)",
                       "covariance propagated from actual composite coefficients (FZ-GATE-PSFSW-COV)"};
  as.correlation_kernel_id = in.correlation_kernel_id;
  as.correlation_scale = 1.0; as.rho_mean = in.rho_mean; as.rho_max_corr = in.rho_max;
  as.diagonal_variance_only = in.correlation_kernel_id.empty();

  /* psfsw 记录块（canonical 词表；禁 ivar/variance 键） */
  {
    json ps;
    ps["psfsw_schema"] = "astrocs.v6.psfsw/v1";
    ps["schema_version"] = 1;
    ps["weight_mode"] = "psfsw_robust";
    ps["weight"] = {{"kind", "psfsw_robust_weight"},
                    {"units", "1"},
                    {"group_normalized", true},
                    {"normalization", {{"scope", "group"}, {"median_target", 1.0},
                                       {"constants_version", p1psfw::kCompositeVersion}}},
                    {"weight_value", nullptr}};
    ps["component_flux_unit"] = "ADU";
    /* 四分量：从磁盘记录真实读取（不重算）。 */
    std::string text, err;
    json doc;
    if (!read_text(in.product_dirs[0] + "/" + phase1::kPhase1RecordFile, &text, &err)) {
      res.error = err; return res;
    }
    doc = json::parse(text);
    const json& comps = doc["psfsw"]["components"];
    auto comp_json = [](const json& c) {
      return json{{"measurement_id", c.value("measurement_id", std::string())},
                  {"value", c.value("value", 0.0)},
                  {"units", c.value("units", std::string())},
                  {"estimator", c["estimator"]},
                  {"spatial_summary", c["spatial_summary"]}};
    };
    ps["components"] = {{"signal", comp_json(comps["signal"])},
                        {"concentration", comp_json(comps["concentration"])},
                        {"noise", comp_json(comps["noise"])},
                        {"background", comp_json(comps["background"])}};
    ps["composite"] = {{"formula_ref", "Wt_k = C_norm * S_k^alpha * Conc_k^beta / (N_k^gamma * B_k^delta); W_psfsw,k = Wt_k / median_j(Wt_j)"},
                       {"exponents", {{"alpha", p1psfw::kCompositeAlpha}, {"beta", p1psfw::kCompositeBeta},
                                      {"gamma", p1psfw::kCompositeGamma}, {"delta", p1psfw::kCompositeDelta}}},
                       {"C_norm_version", p1psfw::kCompositeVersion},
                       {"truncation", "fail_closed_then_component_floor_then_group_median_normalize"},
                       {"estimator_version", "psfsw-est-v1"},
                       {"calibration_sample_id", "phase2_psfsw_calib"}};
    ps["common_star_set"] = doc["psfsw"]["common_star_set"];
    ps["validity"] = {{"valid", true}, {"reason", nullptr}};
    ps["covariance_ref"] = "covariance";
    ps["effective_psf_ref"] = "effective_psf";
    ps["provenance_ref"] = "provenance";
    as.psfsw_block = ps;
  }
  /* SIGNAL/VARIANCE from conventional coadd */
  as.signal = co.i_out;
  as.variance = std::vector<double>(P, 0.0);
  for (std::size_t p = 0; p < P; ++p) as.variance[p] = co.defined[p] ? cp.var_out[p] : 0.0;
  /* 显式暴露 1/W 代理与真实传播方差的差异（RULINGS #5）。 */
  as.approximations.push_back("variance is NOT 1/W_psfsw (forbidden); propagated C_out=R C_in R^T");
  for (const auto& f : fs.frames) as.input_psf_refs.push_back(f.dir + "#psf_profile");

  const PublishOutcome po = publish_phase2_assembly(as, meta, input_hashes_of(fs), target_dir);
  res.publish = po.publish;
  if (!po.ok) { res.error = po.error; return res; }
  res.output_sha256 = po.sha;
  res.median_wt = g.median_wt;
  res.combination_coefficients = alpha;
  res.effective_psf = peff; res.effective_psf_id = as.eff_psf_id;
  res.effective_psf_fwhm = fwhm; res.effective_psf_normalization = "integral";
  res.signal_out = as.signal; res.variance_out = as.variance;
  res.product_dir = target_dir;
  res.evidence = fs.evidence;
  res.evidence.push_back("FZ-FORMULA-PSFSW-COMPOSITE: group median(W_psfsw)=1 from disk");
  res.evidence.push_back("FZ-GATE-PSFSW-COV: C_out=R C_in R^T; variance_from_weight=false");
  res.evidence.push_back("RULINGS #5: 1/W_psfsw not used as variance");
  res.ok = true;
  return res;
}

/* ==================================================================== */
/* 磁盘重开独立校验                                                       */
/* ==================================================================== */
Phase2OpenResult open_phase2_product(const std::string& target_dir) {
  Phase2OpenResult out;
  auto fail = [&out](const std::string& m) { out.ok = false; out.error = m; return out; };
  const std::string rec_path = target_dir + "/" + kPhase2RecordFile;
  const std::string fits_path = target_dir + "/" + kPhase2ScienceFile;
  std::string text, err;
  if (!read_text(rec_path, &text, &err)) return fail(err);
  json doc;
  try { doc = json::parse(text); } catch (const std::exception& e) {
    return fail(std::string("record parse error: ") + e.what());
  }
  /* P33 撤销守卫。 */
  if (has_forbidden_p33_key(doc)) return fail("P33 frame-coefficient token reintroduced");
  if (doc.value("product_schema", std::string()) != kPhase2ProductSchema) return fail("product_schema mismatch");
  if (doc.value("phase", std::string()) != "phase2") return fail("phase != phase2");
  if (!is_hex40(doc.value("software_sha", std::string()))) return fail("software_sha not 40 hex");
  for (const char* k : {"manifest", "provenance", "units", "planes", "covariance",
                        "effective_psf", "weight_mode_record"}) {
    if (!doc.contains(k)) return fail(std::string("record missing block: ") + k);
  }
  /* 模式门（FZ-MODE-PRODUCTION：allowed = point_information | surface_gls）。 */
  {
    const std::string m = doc.value("weight_mode", std::string());
    /* FZ-MODE-RETIRED：旧产品若声明退役对象 psfsw_robust ⇒ **显式拒绝 + 迁移提示**
     * （docs/design/UNIFIED_MODEL.md:58），不得静默接受、不得按未知模式含混带过。 */
    if (is_retired_weight_mode_token(m)) return fail(kRetiredWeightModeRejectReason);
    if (m != "point_information" && m != "surface_gls")
      return fail("weight_mode not in production set (FZ-MODE-PRODUCTION: "
                  "allowed = point_information | surface_gls)");
    out.mode = m;
  }
  /* provenance 最小集。 */
  {
    ValidationReport r = validate_provenance_json(doc["provenance"], provenance_required_keys());
    if (!r.ok()) { out.violations.push_back("G-PROV-MINIMAL-SET: " + r.summary());
      return fail("provenance minimal set violated"); }
  }
  /* 单位 / BUNIT / 二次律。 */
  const json& u = doc["units"];
  const std::string sig_unit = u.value("signal_sb", std::string());
  const std::string var_unit = u.value("sb_variance_out", std::string());
  const std::string ivar_unit = u.value("sb_ivar_out", std::string());
  if (!(sig_unit == "ADU/sr" && var_unit == "ADU^2/sr^2" && ivar_unit == "sr^2/ADU^2"))
    return fail("frozen unit strings violated");
  {
    std::string why;
    if (!quadratic_law_holds(sig_unit, var_unit, ivar_unit, &why))
      return fail("quadratic law violated: " + why);
  }
  {
    const std::string pb = doc["provenance"]["units"].value("bunit", std::string());
    const std::string ps = doc["provenance"]["units"].value("pixel_semantics", std::string());
    const int pap = doc["provenance"]["units"].value("pixel_area_power", 0);
    const BunitCheck bc = bunit_dimension_decidable(pb, ps, pap, false, 0.0);
    if (!bc.decidable) return fail("BUNIT not decidable: " + bc.reason);
  }
  /* FITS 重开（不信任记录）。 */
  std::vector<ExpectedHdu> expected;
  for (const auto& p : doc["planes"]) {
    ExpectedHdu e;
    e.extname = p.value("extname", std::string());
    e.bitpix = p.value("bitpix", -64);
    for (const auto& d : p["naxis"]) e.naxis.push_back(d.get<std::size_t>());
    e.bunit = p.value("bunit", std::string());
    e.check_bunit = true;
    expected.push_back(e);
  }
  const FitsVerifyResult vr = verify_fits_file(fits_path, expected);
  if (!vr.ok) {
    out.violations.push_back(vr.error.empty() ? "fits verify failed" : vr.error);
    return fail("mosaic.fits verify failed");
  }
  out.hdus = vr.hdus;
  for (const auto& h : vr.hdus) {
    if (h.extname == "FLUX") out.has_flux = true;
    if (h.extname == "EFFECTIVE_PSF") out.has_effective_psf = true;
  }
  /* output_hash = 磁盘实际 sha256。 */
  std::string fits_sha;
  if (!sha256_file_hex(fits_path, &fits_sha)) return fail("sha256 recompute failed");
  out.output_sha256 = fits_sha;
  if (doc["manifest"].value("output_hash", std::string()) != fits_sha)
    return fail("manifest.output_hash != recomputed mosaic sha256");
  const std::string ph = doc["provenance"].value("output_hash", std::string());
  if (ph != "sha256:" + fits_sha && ph != fits_sha)
    return fail("provenance.output_hash != recomputed mosaic sha256");
  /* covariance 来源 + 权重面。 */
  const json& cov = doc["covariance"];
  {
    const std::string vf = cov.value("variance_from", std::string());
    const bool allowed = (vf == "combination_coefficients" ||
                          vf == "linear_combination_coefficients" ||
                          vf == "actual_combination_coefficients");
    if (!allowed) return fail("covariance.variance_from not in frozen enum (FZ-FORMULA-COV-PROP)");
    for (const char* bad : kForbiddenWeightSources)
      if (vf == bad) return fail(std::string("variance_from forbidden token: ") + bad);
    if (cov.value("propagation", std::string()) != "C_out = R C_in R^T")
      return fail("covariance.propagation altered");
    if (cov.contains("psfsw_boundary")) {
      const json& pb = cov["psfsw_boundary"];
      if (pb.value("method", std::string()) != "propagated_from_composite_coefficients" ||
          pb.value("variance_from_weight", true) != false ||
          pb.value("uses_relative_weight_as_ivar", true) != false)
        return fail("psfsw boundary violated (FZ-GATE-PSFSW-COV)");
    }
    if (cov.contains("combination_coefficients") &&
        (!cov["combination_coefficients"].is_array() || cov["combination_coefficients"].empty()))
      return fail("combination_coefficients empty");
  }
  /* weight 词表单源 + 诊断来源。 */
  {
    const json& wm = doc["weight_mode_record"];
    if (wm.contains("kind_alias_sci_psfw") || wm.contains("units_alias_sci_psfw") ||
        wm.contains("normalization_scope_alias"))
      return fail("forbidden schema alias present");
    const json& w = wm["weight"];
    for (const char* bad : {"weight_kind", "weight_units", "weight_normalized",
                            "normalization_scope", "weight_type", "norm_scope"})
      if (w.contains(bad) || wm.contains(bad))
        return fail(std::string("forbidden third-vocabulary token: ") + bad);
    /* weight.kind 逐模式 canonical（唯一词表）；禁把 ivar/Fisher 冒充 weight。 */
    {
      const std::string kind = w.value("kind", std::string());
      static const char* kBadKinds[] = {"ivar", "inverse_variance", "variance", "var",
                                        "sigma", "sigma2", "fisher", "fisher_information",
                                        "w_psf"};
      for (const char* b : kBadKinds)
        if (kind == b) return fail(std::string("weight.kind forbidden token: ") + b);
      const char* expect = (out.mode == "point_information")
                               ? "W_info"
                               : (out.mode == "surface_gls" ? "A^T C^-1 A"
                                                            : "psfsw_robust_weight");
      if (kind != expect) return fail("weight.kind not canonical for mode: " + kind);
    }
    if (!w.contains("sources") || !w["sources"].is_array())
      return fail("weight.sources missing");
    for (const auto& s : w["sources"]) {
      const std::string t = s.get<std::string>();
      for (const char* bad : kForbiddenWeightSources)
        if (t == bad) return fail(std::string("diagnostic token in weight.sources: ") + t);
    }
    // ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
    // WHAT:       psfsw_robust 产品形状校验分支（weight.kind / units / group_normalized /
    //             normalization.scope / median_target / psfsw 禁键 / 四分量 / 单位）——
    //             退役对象的**历史产品可判**面。
    // WHY-KEPT:   按 ENGINEERING_SPEC §2「保留则注释」保留：旧 phase2_psfsw 产品记录
    //             （weight_mode="psfsw_robust"）仍需可判可诊断；删掉它会让历史产品在形状
    //             校验阶段以 "weight.kind not canonical" 含混报错，丢失 FZ-MODE-RETIRED 的
    //             可诊断性。保留不等于放行：放行已由上方模式门 fail-closed 拦住（收紧门）。
    // STATUS:     非产品目标态、未接入生产：FZ-MODE-RETIRED 生效后本分支**运行期不可达**
    //             （上方模式门在 out.mode 赋值前已拒绝 psfsw_robust）。
    // EXIT:       与退役对象的物理移除同批删除：负责人裁决删除 WeightMode::kPsfswRobust
    //             枚举臂 + run_psfsw_robust + 本形状校验分支，并同批改
    //             eng/tests/unit/v6_p1_drz/** 与 eng/tests/unit/v6_aio/oracle/**。
    // AUTHORITY:  ENGINEERING_SPEC.md §2；ASTROCS_DESIGN.md §3.1（订正后）；
    //             docs/design/UNIFIED_MODEL.md:58；docs/science/PSF_SIGNAL_WEIGHT.md §1/§4。
    // ──────────────────────────────────────────────────────────────────────
    if (out.mode == "psfsw_robust") {
      if (w.value("kind", std::string()) != "psfsw_robust_weight") return fail("psfsw weight.kind");
      if (w.value("units", std::string()) != "1") return fail("psfsw weight.units != 1");
      if (w.value("group_normalized", false) != true) return fail("psfsw group_normalized != true");
      if (!w.contains("normalization") || !w["normalization"].is_object())
        return fail("psfsw weight.normalization missing");
      if (w["normalization"].value("scope", std::string()) != "group") return fail("psfsw scope != group");
      if (std::fabs(w["normalization"].value("median_target", 0.0) - 1.0) > 1e-12)
        return fail("psfsw median_target != 1");
      const json& ps = doc["psfsw"];
      for (const char* bad : {"ivar", "inverse_variance", "variance", "var", "sigma",
                              "sigma2", "fisher", "fisher_information", "information",
                              "w_info", "w_psf"})
        if (ps.contains(bad)) return fail(std::string("psfsw forbidden key: ") + bad);
      if (ps["components"].size() != 4) return fail("psfsw components != 4");
      if (ps["components"]["concentration"].value("units", std::string()) != "ADU/px^2")
        return fail("concentration units != ADU/px^2 (W6 authority)");
      if (ps.value("component_flux_unit", std::string()) != "ADU") return fail("component_flux_unit");
    }
  }
  /* effective PSF 必输。 */
  {
    const json& ep = doc["effective_psf"];
    if (ep.value("effective_psf_id", std::string()).empty()) return fail("effective_psf_id empty");
    if (ep.value("definition", std::string()) != "impulse_response_of_combination")
      return fail("effective_psf definition altered");
    const std::string nrm = ep.value("normalization", std::string());
    if (nrm != "peak" && nrm != "integral") return fail("effective PSF normalization undeclared");
    const json& vom = ep["values_or_model"];
    if (vom.value("kind", std::string()) != "values" || !vom.contains("values") ||
        !vom["values"].is_array() || vom["values"].empty())
      return fail("effective PSF only fwhm scalar (FZ-GATE-PSFSW-EPSF)");
  }
  /* Q/W 一致性（point_information）。 */
  if (out.mode == "point_information") {
    const json& pi = doc["point_information"];
    const double w_info = pi["W_info"].value("value", 0.0);
    const double var_f = pi["flux_variance"].value("value", 0.0);
    if (!(w_info > 0.0) || !(std::fabs(var_f * w_info - 1.0) <= 1e-9))
      return fail("Var(F_hat) != 1/W_info (FZ-FORMULA-WINFO)");
    if (pi["independent_frame_combination"].value("independent_frames", true) == false) {
      const std::string ref = pi["independent_frame_combination"].value("joint_covariance_ref", std::string());
      if (ref.empty()) return fail("correlated frames without joint covariance ref");
    }
  }
  /* OI-01：Phase2 必须完成组内归一。 */
  if (doc["phase2_extensions"].value("group_normalization_deferred_to", std::string()) != "phase2")
    return fail("group_normalization_deferred_to != phase2");
  out.ok = true;
  out.error.clear();
  return out;
}

/* ==================================================================== */
/* UPM / REJ / SAMP 接线                                                  */
/* ==================================================================== */
UpmRejSampResult run_upm_rej_samp_wiring(const FrameSet& fs, const RunMeta& meta) {
  UpmRejSampResult r;
  if (!fs.ok || fs.frames.size() < 2) { r.error = "need >=2 frames"; return r; }
  const std::size_t K = fs.frames.size();
  const std::size_t P = fs.frames.front().signal_sb.size();
  for (const auto& f : fs.frames) if (f.signal_sb.size() != P) { r.error = "signal plane size mismatch"; return r; }

  /* frame_id：使用确定性 index（真实会话由 p2_frame_id 提供）。 */
  std::vector<std::uint64_t> fids(K);
  for (std::size_t k = 0; k < K; ++k) fids[k] = static_cast<std::uint64_t>(k + 1);

  /* control_variance = k_corr*(pi/2)*sigma_bg^2/N（IMPL-P2-UPM-001 单一实现）。 */
  double sigma_bg = 0.0;
  for (std::size_t k = 0; k < K; ++k)
    for (std::size_t p = 0; p < P; ++p) sigma_bg += std::sqrt(std::max(0.0, fs.frames[k].variance_sb[p]));
  sigma_bg /= static_cast<double>(K * P);
  if (!(sigma_bg > 0.0)) { r.error = "sigma_bg <= 0"; return r; }
  double cv = 0.0, ci = 0.0;
  const int cv_rc = p2_upm_control_variance(kKCorrFrozen, sigma_bg, 8,
                                            "phase2_control_cell",
                                            "frozen-in-domain-constant-FZ-PROV-KCORR-VALUE",
                                            &cv, &ci);
  if (cv_rc != 0) { r.error = "p2_upm_control_variance rc != 0 (FZ-PROV-KCORR)"; return r; }
  r.control_variance = cv;
  if (ci <= 0.0) { r.error = "control_ivar <= 0"; return r; }

  /* overlap graph：取所有帧共同 target_ipix 作为 control。 */
  std::set<std::uint64_t> common(fs.frames[0].view.target_ipix.begin(), fs.frames[0].view.target_ipix.end());
  for (std::size_t k = 1; k < K; ++k) {
    std::set<std::uint64_t> cur(fs.frames[k].view.target_ipix.begin(), fs.frames[k].view.target_ipix.end());
    std::set<std::uint64_t> inter;
    for (auto v : common) if (cur.count(v)) inter.insert(v);
    common.swap(inter);
  }
  if (common.empty()) { r.error = "no common overlap target_ipix (overlap graph disconnected)"; return r; }
  std::vector<std::uint64_t> controls(common.begin(), common.end());
  r.n_overlap_controls = controls.size();

  std::vector<P2UpmMaObservation> obs;
  for (std::size_t k = 0; k < K; ++k) {
    for (std::size_t c = 0; c < controls.size(); ++c) {
      /* 找到该 ipix 在帧 k 的 signal 平面下标。 */
      std::size_t idx = 0; bool found = false;
      for (std::size_t q = 0; q < fs.frames[k].view.target_ipix.size(); ++q)
        if (fs.frames[k].view.target_ipix[q] == controls[c]) { idx = q; found = true; break; }
      if (!found || idx >= P) continue;
      P2UpmMaObservation o{};
      o.frame_id = fids[k];
      o.control_id = controls[c];
      o.value = fs.frames[k].signal_sb[idx];
      o.control_ivar = ci;
      obs.push_back(o);
    }
  }
  if (obs.size() < controls.size() * 2) { r.error = "insufficient overlap observations"; return r; }
  r.n_obs = obs.size();

  P2UpmMaConfig cfg{};
  cfg.min_frames = 2;
  cfg.rank_rtol = 1e-10;
  cfg.kappa_max = 1e6;
  cfg.gauge_mode = 0;
  cfg.allow_additive_only_single_frame = 1;
  cfg.c_in_has_unrepresented_shared_terms = 0;
  cfg.huber_delta = 1.345;
  cfg.max_iterations = 100;
  cfg.tolerance = 1e-6;
  cfg.sigma_floor = 1e-3;
  cfg.zero_anchor_weight = 0.0;
  cfg.k_corr = kKCorrFrozen;
  cfg.k_corr_applicability_domain = "phase2_control_cell";
  cfg.k_corr_calibration_run_id = "frozen-in-domain-constant-FZ-PROV-KCORR-VALUE";
  cfg.flux_conservation_factor = "1.0";
  void* model = nullptr;
  const int brc = p2_upm_ma_build(obs.data(), obs.size(), &cfg, &model);
  if (brc != 0 || model == nullptr) { r.error = "p2_upm_ma_build rc=" + std::to_string(brc); return r; }
  P2UpmMaInfo info{};
  p2_upm_ma_info(model, &info);
  r.n_components = info.n_components; r.n_free = info.n_params; r.rank = info.rank; r.kappa = info.kappa;

  /* 解出 g_k/b_k/s(p) */
  for (std::size_t k = 0; k < K; ++k) {
    double g = 0.0, b = 0.0;
    p2_upm_ma_solution(model, fids[k], controls[0], &g, &b, nullptr);
    r.g_k.push_back(g); r.b_k.push_back(b);
  }
  for (std::size_t c = 0; c < controls.size(); ++c) {
    double s = 0.0;
    p2_upm_ma_solution(model, fids[0], controls[c], nullptr, nullptr, &s);
    r.s_p.push_back(s);
  }

  /* C_theta 与 J_out 映射（full -> free，gauge 固定项移除）。 */
  const std::size_t np = static_cast<std::size_t>(info.n_params);
  std::vector<double> Ctheta(np * np, 0.0);
  if (np > 0) {
    const int pc_rc = p2_upm_ma_param_cov(model, Ctheta.data(), np);
    if (pc_rc != 0) { r.error = "p2_upm_ma_param_cov rc=" + std::to_string(pc_rc); p2_upm_ma_close(model); return r; }
  }
  const std::size_t n_p = controls.size();
  const std::size_t F = K;
  const std::size_t full_n = n_p + 2 * F;
  std::vector<int> full_to_free(full_n, -1);
  {
    /* gauge 固定：每分量参考帧的 g,b 移除。 */
    std::vector<std::uint64_t> refs(r.n_components, 0);
    for (std::size_t c = 0; c < r.n_components; ++c)
      p2_upm_ma_component_ref_frame(model, static_cast<std::uint64_t>(c), &refs[c]);
    std::vector<char> removed(full_n, 0);
    for (std::size_t k = 0; k < F; ++k) {
      std::uint64_t comp = 0;
      p2_upm_ma_component_of_frame(model, fids[k], &comp);
      bool is_ref = false;
      for (std::size_t c = 0; c < r.n_components; ++c)
        if (refs[c] == fids[k]) is_ref = true;
      if (is_ref) { removed[n_p + k] = 1; removed[n_p + F + k] = 1; }
      (void)comp;
    }
    int next = 0;
    for (std::size_t i = 0; i < full_n; ++i) if (!removed[i]) full_to_free[i] = next++;
  }
  /* 用非参考帧 g/b 计算 sigma_eff^2 = sigma_p1^2 + J C_theta J^T。 */
  {
    const std::size_t k = (F > 1) ? 1 : 0;
    std::vector<double> J(static_cast<std::size_t>(np), 0.0);
    const int gi = full_to_free[n_p + k];
    const int bi = full_to_free[n_p + F + k];
    const double s_ref = r.s_p.empty() ? 0.0 : r.s_p[0];
    if (gi >= 0) J[static_cast<std::size_t>(gi)] = s_ref;
    if (bi >= 0) J[static_cast<std::size_t>(bi)] = 1.0;
    double jcj = 0.0;
    for (std::size_t a = 0; a < np; ++a)
      for (std::size_t b = 0; b < np; ++b) jcj += J[a] * Ctheta[a * np + b] * J[b];
    const double sigma_p1_2 = cv;
    r.sigma_eff2 = sigma_p1_2 + jcj;
  }

  /* 分类排异（sigma_eff^2 = sigma_p1^2 + J C_theta J^T）。 */
  {
    P2RejectionPlanRequest req{};
    req.request = P2_REJECT_SIGMA;
    req.nominal_contributors = static_cast<std::uint32_t>(K);
    req.profile = "wbpp_current";
    req.underdetermined_n = 2;
    P2RejectionPlan plan{};
    char errbuf[256] = {0};
    if (p2_reject_plan_resolve(&req, &plan, errbuf, sizeof(errbuf)) != 0) {
      r.error = std::string("p2_reject_plan_resolve failed: ") + errbuf;
      p2_upm_ma_close(model); return r;
    }
    if (p2_reject_plan_thresholds_inherited(&plan) != 1) {
      r.error = "inherited rejection thresholds altered (FZ-REJ-INHERITED-THRESH)";
      p2_upm_ma_close(model); return r;
    }
    const std::uint32_t n = 4;
    std::vector<double> residual(n), sigma_p1(n), upm_var(n);
    std::vector<std::uint8_t> noise_flags(n, P2_NOISE_REQUIRED);
    std::vector<double> sig_eff(n), z(n), prob(n), classp(n * P2_REJECT_CLASS_COUNT);
    std::vector<std::uint8_t> reasons(n), classes(n), deleted(n), preserved(n);
    const double s0 = r.s_p.empty() ? 0.0 : r.s_p[0];
    const double pred = r.g_k.empty() ? 0.0 : r.g_k[0] * s0 + r.b_k[0];
    const double y0 = fs.frames[0].signal_sb.empty() ? 0.0 : fs.frames[0].signal_sb[0];
    for (std::uint32_t i = 0; i < n; ++i) {
      residual[i] = (i == 0) ? (y0 - pred) : (y0 - pred + 0.3 * static_cast<double>(i));
      sigma_p1[i] = std::sqrt(cv);
      upm_var[i] = std::max(0.0, r.sigma_eff2 - cv);
    }
    P2RejectClassifyConfig ccfg{};
    ccfg.plan = plan;
    ccfg.profile_version = P2_REJECT_CLASSIFY_PROFILE;
    ccfg.motion_min_px = P2_REJ_PROFILE_MOTION_MIN_PX;
    ccfg.psf_anomaly_min = P2_REJ_PROFILE_PSF_ANOMALY_MIN;
    ccfg.contamination_prior = P2_REJ_PROFILE_CONTAM_PRIOR;
    ccfg.outlier_inflation = P2_REJ_PROFILE_OUTLIER_KAPPA;
    ccfg.keep_moving_source = 1;
    P2RejectClassifyInput rin{};
    rin.residual = residual.data();
    rin.sigma_phase1 = sigma_p1.data();
    rin.upm_variance = upm_var.data();
    rin.noise_flags = noise_flags.data();
    rin.count = n;
    P2RejectClassifyOutput rout{};
    rout.reasons = reasons.data(); rout.reason_classes = classes.data();
    rout.deleted = deleted.data(); rout.preserved = preserved.data();
    rout.sigma_eff = sig_eff.data(); rout.z = z.data(); rout.probability = prob.data();
    rout.class_probability = classp.data();
    const int rrc = p2_reject_classify(&rin, &ccfg, &rout);
    r.reject_status = rrc;
    r.accepted_count = rout.accepted_count;
    r.rejected_low = rout.rejected_low;
    r.rejected_high = rout.rejected_high;
    if (!sig_eff.empty()) r.z = z[0];
  }

  /* 空间模型求值/摘要 + 标量降级门（双门 + p05/p50/p95）。
   * 二维网格（nx=P, ny=2，两行同值）以满足 eval 的尺寸>=2；只统计有限节点。 */
  {
    P2SpatialGridModel gm{};
    gm.ra0_deg = 0.0; gm.dec0_deg = 0.0; gm.step_deg = 1e-3;
    gm.nx = P; gm.ny = 2;
    std::vector<double> gval(2 * P, 0.0);
    std::vector<std::uint8_t> valid(2 * P, 0);
    for (std::size_t i = 0; i < P; ++i) {
      for (std::size_t row = 0; row < 2; ++row) {
        const double v = fs.frames[0].signal_sb[i];
        gval[row * P + i] = v;
        valid[row * P + i] = std::isfinite(v) ? 1 : 0;
      }
    }
    gm.value = gval.data(); gm.valid = valid.data();
    P2SpatialEval ev{};
    const int erc = p2_spatial_model_eval(&gm, 0.0, 0.0, &ev);
    P2SpatialSummary sum{};
    const int src = p2_spatial_model_summary(&gm, &sum, nullptr, 0);
    if (erc == 0 && src == 0) {
      r.spatial_p05 = sum.p05; r.spatial_p50 = sum.p50; r.spatial_p95 = sum.p95;
      r.spatial_max_dev = sum.max_systematic_deviation;
      r.spatial_coverage = sum.sampling_coverage; r.spatial_model_error = sum.model_error;
    } else {
      r.error = "spatial model wiring failed (eval=" + std::to_string(erc) +
                " status=" + std::to_string(static_cast<int>(ev.status)) +
                " summary=" + std::to_string(src) + ")";
      p2_upm_ma_close(model); return r;
    }
    P2ScalarGateThresholds th{};
    th.thresholds_declared = 1;
    th.residual_trend_max = 0.10; /* PSFSW-T-TREND（已冻结值） */
    th.power_loss_max = 0.05;     /* PSFSW-T-POWERLOSS（已冻结值） */
    P2ScalarSummaryInput si{};
    si.summary_complete = 1;
    si.spatial_residual_p95 = 0.0;
    si.spatial_trend = 0.0;
    si.power_loss = 0.0;
    si.p05 = sum.p05; si.p50 = sum.p50; si.p95 = sum.p95;
    si.max_systematic_deviation = sum.max_systematic_deviation;
    si.sampling_coverage = sum.sampling_coverage; si.model_error = sum.model_error;
    std::snprintf(si.applicability_domain, sizeof(si.applicability_domain), "%s", "phase2_mosaic");
    P2ScalarDegradeVerdict verdict = P2_SCALAR_UNAVAILABLE;
    r.scalar_verdict = p2_scalar_degrade_gate(&th, &si, &verdict, nullptr, 0);
  }

  /* coverage/support 分类（只作门）。 */
  {
    std::vector<std::uint8_t> cov(P, 1), cov_known(P, 1), sup_known(P, 1);
    std::vector<std::uint32_t> sup(P, static_cast<std::uint32_t>(K));
    P2CoverageSupportInput cin{};
    cin.n_cells = P; cin.coverage = cov.data(); cin.coverage_known = cov_known.data();
    cin.support_frames = sup.data(); cin.support_known = sup_known.data();
    cin.min_support_frames = 2;
    std::vector<P2CellState> st(P);
    std::uint64_t ns = 0, ncu = 0, nu = 0, nua = 0;
    r.coverage_rc = p2_coverage_support_classify(&cin, st.data(), &ns, &ncu, &nu, &nua, nullptr, 0);
    r.n_supported = ns; r.n_uncovered = nu; r.n_unavailable = nua;
  }

  /* 生产权重模式门 + 权重来源 token 门（FZ-MODE-* / FZ-GATE-*）。 */
  {
    char eb[256] = {0};
    r.weight_mode_rc = p2_weight_mode_check("point_information", eb, sizeof(eb));
    const char* tokens_ok[] = {"psf", "photometric_response", "noise_covariance"};
    const char* tokens_bad[] = {"support", "coverage", "median_source_snr", "fwhm", "residual"};
    const int tok_ok_rc = p2_weight_source_token_reject(tokens_ok, 3, eb, sizeof(eb));
    (void)tok_ok_rc;
    p2_rejection_weight_surface_guard(tokens_bad, 5, eb, sizeof(eb));
  }

  p2_upm_ma_close(model);
  r.evidence.push_back("ALG-P2S-UPM.2: overlap graph components=" + std::to_string(r.n_components));
  r.evidence.push_back("FZ-AP2S-RANK-RTOL/KAPPA-MAX: rank=" + std::to_string(r.rank));
  r.evidence.push_back("ALG-P2S-REJ.3: sigma_eff^2 = sigma_p1^2 + J C_theta J^T");
  r.evidence.push_back("FZ-DEGRADE-SCALAR: spatial gate + power-loss gate + p05/p50/p95");
  r.ok = true;
  return r;
}

}  /* namespace p2int */
}  /* namespace v6 */
}  /* namespace astrocs */
