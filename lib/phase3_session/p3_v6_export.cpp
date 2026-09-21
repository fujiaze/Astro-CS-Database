// ── RETIRED-CODE-RETAINED (ENGINEERING_SPEC §2 保留则注释) ─────────────
// WHAT:       Phase3 V6 三模式产品导出接线层（OutputGrid / ExportInputs / build_output_grid /
//             export_product / verify_product_on_disk；承载 FZ-P3-MODES / FZ-P3-QW-RECOMPUTE /
//             FZ-P3-BUNIT-QUADRATIC / FZ-P3-KERNEL-REGISTRY 四条 v6 合同）。
// WHY-KEPT:   删除会同时打断三处他域锚，本轮不能删：
//             ① eng/ci/checks.json CHK-CONTRACT-TEST 以 ctest_targets 登记 v6_p3_export_positive /
//                v6_p3_export_negative / v6_p3_export_oracle（并在 V6-CTEST-INTEGRATION step 的
//                --expect 列出），eng/tools/quality/check_ctest_registration.py 的 C4 对
//                「ctest_targets 匹配不到现存目标」fail-closed ⇒ 删测试即判红；eng/ci/checks.json 属
//                DOC-403 文件域，本任务无权同步；
//             ② eng/ci/spec_named_impls.json SNI-S4-P3X-06/P3X-12 与 eng/ci/ledgers/spec_named_impl_gaps.json
//                以本文件为锚（删除须同提交改表，属 CI 登记面，需与 DOC-403 同批）；
//             ③ docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:338 与
//                docs/algorithms/v6/phase3/ALG-P3-001_SPEC.md 仍点名本文件（docs/** 属 DOC-402 域）。
// STATUS:     未接入生产。不在任何生产 target 的源列表内（grep -c p3_v6_export CMakeLists.txt = 0），
//             仅被 eng/tests/integration/v6_p3 编译；生产 export 路径 = lib/phase3_session/p3_session.cpp
//             → lib/algorithms/projection/p3_wcs.cpp（TAN），不依赖本文件任何符号
//             （p3_session.cpp:3-17 的 include 面无 p3_v6_export.h）。
// EXIT:       删除（ENGINEERING_SPEC §2 第 1 种处置），需同批完成：
//             ① DOC-403 从 eng/ci/checks.json 移除 v6_p3_export_* 三个 ctest_targets 及
//                V6-CTEST-INTEGRATION step 的三条 --expect；
//             ② 同提交删除 eng/ci/spec_named_impls.json 的 SNI-S4-P3X-06/SNI-S4-P3X-12 两条与
//                eng/ci/ledgers/spec_named_impl_gaps.json 的 SNI-S4-P3X-06 条；
//             ③ DOC-402 退役 docs/algorithms/v6/phase3/ALG-P3-001_SPEC.md 并把
//                PRODUCTION_EXECUTION_INVENTORY.csv:338 的 production=yes 更正为 retired；
//             ④ 删除 eng/tests/integration/v6_p3/** 与 CMakeLists.txt:980 的 add_subdirectory。
// AUTHORITY:  ENGINEERING_SPEC.md §2（历史实现处置：保留则注释）；ASTROCS_DESIGN.md §6.3
//             （注册表中未实现的投影被选择时显式报「不支持」，当前仅 TAN 可用）；
//             工程控制/RELEASE-04/GAP_AUDIT.md G2-1/G3-2；eng/ci/spec_named_impls.json SNI-S4-P3X-06。
// ──────────────────────────────────────────────────────────────────────
// lib/phase3_session/p3_v6_export.cpp — Phase3 V6 三模式产品导出接线实现。
//
// 接线关系（不修改任何底层模块）:
//   phase3proj::v6::plan/solid_angle_grid  -> OutputGrid（逐像素 Ω', registry v2）
//   p3rsmp::make_bilinear_4quad_neighborhood + build_row/column_normalized -> R/S 算子
//   p3rsmp::propagate_surface_brightness / propagate_point_source_flux / propagate_visualization
//   aio::FitsStreamWriter + aio::atomic_publish_file -> 流式 FITS + 原子发布
//   aio::verify_fits_file / validate_provenance_json / validate_bunit_law -> 重开独立验证
//
// 产品 HDU 布局的 schema 依据（W6 生产 schema，SCHEMA-INTEGRATE-001）:
//   eng/contracts/schemas/v6/astrocs.v6.provenance.v1.schema.json#allOf 规定：
//     units.bunit == "ADU"  =>  units.pixel_semantics == "surface_brightness"
//                              且 units.pixel_area_power == -2；
//     units.bunit 匹配 /px\^2$ => units.pixel_area_power == -2。
//   叠加 astrocs.v6.signal.v1.schema.json（integrated_flux => pixel_area_power == 0）后，
//   纯积分通量主面在 v6 生产 schema 下不可表达。故 point_source_flux 模式以 schema 合法的
//   面亮度主面承载重采样 signal，matched-filter 通量/effective PSF 以扩展 HDU 承载
//   （BUNIT=ADU / ADU^2 / 1，逐层量纲可判）。该交叉张力登记为 finding（详见任务返回）。
#include "p3_v6_export.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <nlohmann/json.hpp>

#include "astro/aio/v6_bunit.h"

namespace astrocs {
namespace phase3 {
namespace v6 {

using nlohmann::json;

// ---------------------------------------------------------------------------
// 模式
// ---------------------------------------------------------------------------
const char* export_mode_token(ExportMode m) {
  switch (m) {
    case ExportMode::kSurfaceBrightness: return "surface_brightness";
    case ExportMode::kPointSourceFlux: return "point_source_flux";
    case ExportMode::kVisualization: return "visualization";
  }
  return "unknown";
}

bool parse_export_mode(const std::string& token, ExportMode* out) {
  if (out == nullptr) return false;
  if (token == "surface_brightness") { *out = ExportMode::kSurfaceBrightness; return true; }
  if (token == "point_source_flux") { *out = ExportMode::kPointSourceFlux; return true; }
  if (token == "visualization") { *out = ExportMode::kVisualization; return true; }
  return false;  // legacy auto / psf_snr_power / 未知 -> 拒（FZ-P3-MODES / C-004.1）
}

p3rsmp::P3Mode to_rsmp_mode(ExportMode m) {
  switch (m) {
    case ExportMode::kSurfaceBrightness: return p3rsmp::P3Mode::SurfaceBrightness;
    case ExportMode::kPointSourceFlux: return p3rsmp::P3Mode::PointSourceFlux;
    case ExportMode::kVisualization: return p3rsmp::P3Mode::Visualization;
  }
  return p3rsmp::P3Mode::Visualization;
}

// ---------------------------------------------------------------------------
namespace {

// 递归创建目录（产物目录的父级链）；已存在不报错。
void mkdirs(const std::string& path) {
  std::string cur;
  for (std::size_t i = 0; i < path.size(); ++i) {
    cur.push_back(path[i]);
    if (path[i] == '/' || i + 1 == path.size()) {
      if (cur == "/" || cur.empty()) continue;
#if defined(_WIN32)
      ::_mkdir(cur.c_str());
#else
      ::mkdir(cur.c_str(), 0755);
#endif
    }
  }
}

// 流式 FITS 行带高度（行）；多块写出用于验证 CHECKSUM 跨块累加路径。
constexpr int kRowsPerBand = 2;

std::string p3proj_status_reason(phase3proj::v6::ProjStatus s) {
  switch (s) {
    case phase3proj::v6::ProjStatus::kOk: return "ok";
    case phase3proj::v6::ProjStatus::kParam: return "projection_param_invalid";
    case phase3proj::v6::ProjStatus::kUnsupported: return "projection_not_registered";
    case phase3proj::v6::ProjStatus::kHemisphere: return "projection_domain_violation";
  }
  return "unknown";
}

void append_f64_be(std::vector<std::uint8_t>* out, double v) {
  std::uint64_t u = 0;
  std::memcpy(&u, &v, sizeof(u));
  for (int i = 7; i >= 0; --i) out->push_back(static_cast<std::uint8_t>(u >> (8 * i)));
}

std::vector<std::uint8_t> f64_be(const std::vector<double>& v) {
  std::vector<std::uint8_t> out;
  out.reserve(v.size() * 8);
  for (double x : v) append_f64_be(&out, x);
  return out;
}

// FITS 实数卡：以大写 E 指数文本直写（AIO make_real 的 %.12g 会产出小写 'e'，
// astropy verify 判为非标准；本层只做本任务产物的文本规范化，不改 AIO）。
std::string upper_e(std::string s) {
  for (char& c : s) {
    if (c == 'e') c = 'E';
  }
  return s;
}

aio::FitsCard real_card_text(const std::string& key, const std::string& text,
                             const std::string& comment = "") {
  aio::FitsCard c;
  c.keyword = key;
  c.type = aio::FitsCard::Type::kReal;
  c.value = upper_e(text);
  c.comment = comment;
  return c;
}

aio::FitsCard real_card(const std::string& key, double v,
                        const std::string& comment = "") {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.12E", v);
  return real_card_text(key, buf, comment);
}

std::string trim(const std::string& s) {
  std::size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

// phase3proj::v6::fits_keywords 文本 -> FITS 卡（字符串/实数）。
std::vector<aio::FitsCard> wcs_cards(const phase3proj::v6::Descriptor& d) {
  std::vector<aio::FitsCard> cards;
  const std::string kw = phase3proj::v6::fits_keywords(&d);
  std::istringstream iss(kw);
  std::string line;
  while (std::getline(iss, line)) {
    if (line.empty()) continue;
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    const std::string key = trim(line.substr(0, eq));
    std::string rhs = trim(line.substr(eq + 1));
    if (rhs.size() >= 2 && rhs.front() == '\'') {
      const std::size_t end = rhs.find('\'', 1);
      cards.push_back(aio::FitsCard::make_string(key, rhs.substr(1, end - 1), ""));
    } else {
      cards.push_back(real_card_text(key, rhs));
    }
  }
  return cards;
}

aio::FitsLayer make_layer(const std::string& extname, int width, int height,
                          std::vector<aio::FitsCard> cards,
                          const std::vector<double>& data) {
  aio::FitsLayer l;
  l.spec.extname = extname;
  l.spec.bitpix = -64;
  l.spec.naxis = {static_cast<std::size_t>(width), static_cast<std::size_t>(height)};
  l.spec.cards = std::move(cards);
  l.data = f64_be(data);
  return l;
}

// 逐像素 Ω' 邻域（bilinear 4 象限 / nearest）；覆盖 helper 的标量 Ω 占位为真实 Ω'。
p3rsmp::NeighborhoodSet build_neighborhood(const OutputGrid& grid,
                                           const ExportInputs& in,
                                           const ResamplePlan& plan, bool nearest) {
  p3rsmp::InputGrid2D ig;
  ig.width = in.in_width;
  ig.height = in.in_height;
  ig.value.assign(static_cast<std::size_t>(in.in_width) * in.in_height, 0.0);
  p3rsmp::TileMask tm;
  tm.tile_px = std::max(in.in_width, in.in_height);
  tm.tiles_x = 1;
  tm.tiles_y = 1;
  tm.present.assign(1, 1);
  p3rsmp::GridPlan gp;
  gp.out_width = grid.width;
  gp.out_height = grid.height;
  gp.out_origin_x = plan.out_origin_x;
  gp.out_origin_y = plan.out_origin_y;
  gp.out_step = plan.out_step;

  p3rsmp::NeighborhoodSet nb =
      nearest ? p3rsmp::make_nearest_neighborhood(ig, tm, gp, 1.0, 1.0)
              : p3rsmp::make_bilinear_4quad_neighborhood(ig, tm, gp, 1.0, 1.0);
  // FZ-P3-OMEGA-NONCONST：把 helper 的常数占位替换为 p3_proj_v6 的真实逐像素 Ω'。
  nb.geom.omega_in_sr = in.omega_in_sr;
  nb.geom.omega_out_sr = grid.omega_out_sr;
  for (auto& n : nb.outputs) {
    const double oo = grid.omega_out_sr[static_cast<std::size_t>(n.out_index)];
    n.omega_out_sr = oo;
    for (double& a : n.overlap_sr) a *= oo;
  }
  return nb;
}

aio::Provenance make_provenance(const ExportMode mode, const ExportInputs& in,
                                const OutputGrid& grid, const std::string& kernel_id,
                                const std::string& output_hash, bool uncertainty_unavailable,
                                const std::string& unavailable_reason) {
  (void)grid;
  aio::Provenance p;
  p.product.type_id = "astrocs.phase3.product.fits.v1";
  p.product.schema_version = 1;
  p.software_sha = in.software_sha;
  p.run_id = in.run_id;
  p.input_product_hashes = in.input_product_hashes;
  p.config_hash = in.config_hash;
  // 主 HDU = 面亮度 signal（schema allOf 约束下的唯一自洽主面）。
  p.units.bunit = "ADU/px^2";
  p.units.pixel_semantics = "surface_brightness";
  p.units.pixel_area_power = -2;
  p.units.has_target_pixel_area = true;
  p.units.target_pixel_area = 1.0;
  p.coordinate_frame = "icrs";
  p.coordinate_epoch = "J2000";
  p.pixel_semantics = "surface_brightness";
  p.sampling.kernel_id = kernel_id;
  p.sampling.has_pixfrac = false;
  p.algorithm_ids = in.algorithm_ids;
  p.module.module_id = in.module_id;
  p.module.build_id = in.module_build_id;
  p.provider = in.provider;
  if (mode == ExportMode::kVisualization) {
    p.approximations = {"visualization_no_uncertainty_propagation"};
  } else {
    p.approximations = {};
  }
  p.degradations.clear();
  p.normalization_version = in.normalisation_version;
  p.weight_mode_version = in.weight_mode_version;
  p.correlation_summary.representation = "correlation_kernel";
  p.correlation_summary.kernel_id = in.correlation_kernel_id;
  p.correlation_summary.has_scale = true;
  p.correlation_summary.scale = in.correlation_scale;
  p.has_flux_conservation_factor = true;
  p.flux_conservation_factor = 1.0;  // pixfrac=1（bilinear 无权 pixfrac）
  p.k_corr.definition = "k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]";
  p.k_corr.value = in.k_corr_value;
  p.k_corr.domain.geometry = in.k_corr_geometry;
  p.k_corr.domain.pixfrac = 1.0;
  p.k_corr.domain.patch_size = 8;
  p.k_corr.domain.estimator = "median";
  p.k_corr.domain.spherical = true;
  p.k_corr.calibration.script = in.k_corr_calibration_script;
  p.k_corr.calibration.seed = in.k_corr_seed;
  p.k_corr.calibration.calibration_run_id = in.k_corr_run_id;
  p.has_k_corr = true;
  p.unavailable_flag = uncertainty_unavailable;
  if (uncertainty_unavailable) {
    p.unavailable_reason = unavailable_reason.empty() ? "uncertainty_not_propagated"
                                                       : unavailable_reason;
    p.unavailable_scope = "variance";
  } else {
    p.unavailable_reason = "not_applicable";
    p.unavailable_scope = "none";
  }
  p.generated_utc = in.generated_utc;
  p.output_hash = output_hash;
  p.diagonal_variance_only = false;
  p.signal_unit = "ADU/px^2";
  p.variance_unit = "ADU^2/px^4";
  p.ivar_unit = "px^4/ADU^2";
  return p;
}

aio::ValidationReport validate_provenance_json_full(const json& j,
                                                    const aio::Provenance& prov) {
  aio::ValidationReport r =
      aio::validate_provenance_json(j, aio::provenance_required_keys());
  r.merge(aio::validate_bunit_law(prov.units, prov.signal_unit, prov.variance_unit,
                                  prov.ivar_unit));
  if (prov.diagonal_variance_only &&
      prov.correlation_summary.representation == "full_matrix_unavailable") {
    r.add("G-SHARED-SYSTEMATIC", "FZ-GATE-PARENT-VAR",
          "diagonal-only variance without correlation kernel");
  }
  return r;
}

// 流式原子发布：tmp -> 逐行带 FitsStreamWriter -> DATASUM/CHECKSUM -> fsync -> rename
// -> verify_fits_file 重开验证（ALG-P3-008 §7）。
aio::PublishResult publish_layers_streaming(
    const std::string& target, const std::vector<aio::FitsLayer>& layers,
    const std::vector<aio::ExpectedHdu>& expected, const aio::PublishOptions& opts,
    const aio::CancelFn& cancel, int rows_per_band, bool force_verify_fail) {
  std::vector<aio::FitsLayer> effective = layers;
  if (effective.size() > 1 && effective.front().spec.extname.empty()) {
    effective.front().spec.primary_has_extensions = true;
  }
  const aio::FileWriterFn writer = [&effective, rows_per_band](
                                       int fd, const aio::CancelFn& c,
                                       std::string* err) -> bool {
    aio::FitsStreamWriter w(fd);
    for (const auto& layer : effective) {
      if (c && c()) { if (err) *err = "cancelled"; return false; }
      if (!w.begin_hdu(layer.spec, err)) return false;
      std::size_t row_bytes = 0;
      if (!layer.spec.naxis.empty()) {
        row_bytes = layer.spec.naxis[0] *
                    aio::fits_bytes_per_pixel(layer.spec.bitpix);
      }
      std::size_t band = row_bytes > 0
                             ? static_cast<std::size_t>(rows_per_band) * row_bytes
                             : static_cast<std::size_t>(rows_per_band) * 1024;
      if (band == 0) band = 65536;
      std::size_t off = 0;
      const std::size_t n = layer.data.size();
      while (off < n) {
        const std::size_t chunk = std::min(band, n - off);
        if (!w.write_data(layer.data.data() + off, chunk, err)) return false;
        off += chunk;
        if (c && c()) { if (err) *err = "cancelled"; return false; }
      }
      if (!w.end_hdu(err)) return false;
    }
    return w.fsync_now(err);
  };
  const aio::VerifyFn verify = [&expected, force_verify_fail](
                                   const std::string& path, std::string* err) -> bool {
    if (force_verify_fail) {
      if (err) *err = "injected post-publish verification failure";
      return false;
    }
    const aio::FitsVerifyResult vr = aio::verify_fits_file(path, expected);
    if (!vr.ok) {
      if (err) {
        *err = vr.error.empty()
                   ? (vr.violations.empty()
                          ? std::string("fits verify failed")
                          : vr.violations.front().gate + ": " +
                                vr.violations.front().message)
                   : vr.error;
      }
      return false;
    }
    return true;
  };
  return aio::atomic_publish_file(target, writer, verify, opts, cancel);
}

// provenance JSON 原子写 + 重开校验（缺键/单位门/不可判即撤销发布）。
aio::PublishResult publish_provenance(const std::string& target,
                                      const std::string& text,
                                      const aio::PublishOptions& opts,
                                      const aio::CancelFn& cancel) {
  const aio::VerifyFn verify = [](const std::string& path, std::string* err) -> bool {
    std::ifstream f(path);
    if (!f) { if (err) *err = "provenance reopen failed"; return false; }
    std::stringstream ss;
    ss << f.rdbuf();
    try {
      const json j = json::parse(ss.str());
      const aio::ValidationReport r =
          aio::validate_provenance_json(j, aio::provenance_required_keys());
      if (!r.ok()) { if (err) *err = r.summary(); return false; }
    } catch (const std::exception& e) {
      if (err) *err = std::string("provenance parse failed: ") + e.what();
      return false;
    }
    return true;
  };
  return aio::atomic_write_bytes(target, text, verify, opts, cancel);
}

}  // namespace

// ---------------------------------------------------------------------------
// 输出网格
// ---------------------------------------------------------------------------
phase3proj::v6::ProjStatus build_output_grid(
    phase3proj::v6::ProjectionId id, double centre_ra_deg, double centre_dec_deg,
    double scale_deg_per_px, int width_px, int height_px, const char* parity,
    double rotation_pa_deg, OutputGrid* out, std::string* err) {
  if (out == nullptr) return phase3proj::v6::ProjStatus::kParam;
  *out = OutputGrid{};
  phase3proj::v6::Plan pl;
  const phase3proj::v6::ProjStatus st =
      phase3proj::v6::plan(id, centre_ra_deg, centre_dec_deg, scale_deg_per_px,
                           width_px, height_px, parity, rotation_pa_deg, &pl);
  if (st != phase3proj::v6::ProjStatus::kOk) {
    if (err) *err = "plan failed: " + p3proj_status_reason(st);
    return st;
  }
  out->descriptor = pl.descriptor;
  out->width = width_px;
  out->height = height_px;
  out->fov_x_deg = pl.fov_x_deg;
  out->fov_y_deg = pl.fov_y_deg;
  out->omega_min_sr = pl.omega_min_sr;
  out->omega_max_sr = pl.omega_max_sr;
  const std::size_t n = static_cast<std::size_t>(width_px) * height_px;
  out->omega_out_sr.assign(n, 0.0);
  std::vector<phase3proj::v6::ProjStatus> ss(n);
  const phase3proj::v6::ProjStatus gst =
      phase3proj::v6::solid_angle_grid(&out->descriptor, out->omega_out_sr.data(),
                                       ss.data());
  if (gst != phase3proj::v6::ProjStatus::kOk) {
    if (err) *err = "solid_angle_grid failed: " + p3proj_status_reason(gst);
    out->omega_out_sr.clear();
    return gst;
  }
  for (std::size_t i = 0; i < n; ++i) {
    const double o = out->omega_out_sr[i];
    if (!(o > 0.0) || !std::isfinite(o)) {
      if (err) *err = "non_finite_or_nonpositive_omega_at_" + std::to_string(i);
      out->omega_out_sr.clear();
      return phase3proj::v6::ProjStatus::kParam;
    }
  }
  return phase3proj::v6::ProjStatus::kOk;
}

// ---------------------------------------------------------------------------
// 三模式导出
// ---------------------------------------------------------------------------
ExportResult export_product(ExportMode mode, const OutputGrid& grid,
                            const ExportInputs& in, const ResamplePlan& plan,
                            const std::string& product_dir,
                            const aio::PublishOptions& opts,
                            const aio::CancelFn& cancel) {
  ExportResult out;
  out.product_dir = product_dir;
  out.fits_path = product_dir + "/product.fits";
  out.provenance_path = product_dir + "/provenance.json";

  const std::size_t n_out = static_cast<std::size_t>(grid.width) * grid.height;
  if (grid.width <= 0 || grid.height <= 0 ||
      grid.omega_out_sr.size() != n_out ||
      in.in_width <= 0 || in.in_height <= 0 ||
      in.omega_in_sr.size() !=
          static_cast<std::size_t>(in.in_width) * in.in_height ||
      in.x.size() != in.omega_in_sr.size()) {
    out.status = p3rsmp::Status::InvalidArgument;
    out.code = "G-P3-INTG-DIM";
    out.reason = "grid_or_input_dimension_mismatch";
    return out;
  }

  // 采样核生产准入（FZ-P3-KERNEL-REGISTRY）。visualization 用 nearest/显式选择，
  // 其余用连续场核并按模式检查语义能力。
  const std::string kernel = (mode == ExportMode::kVisualization) ? "nearest"
                                                                  : in.kernel_id;
  const p3rsmp::KernelUse kuse = (mode == ExportMode::kVisualization)
                                     ? p3rsmp::KernelUse::ExplicitUserSelection
                                     : p3rsmp::KernelUse::ContinuousField;
  {
    const p3rsmp::GateResult adm =
        p3rsmp::KernelRegistry::frozen().admit(kernel, kuse, to_rsmp_mode(mode));
    if (!adm.ok()) {
      out.status = adm.status;
      out.code = adm.code;
      out.reason = adm.reason;
      return out;
    }
  }

  // ---- visualization：非测量显示面，无归一，禁写测量 HDU ----
  if (mode == ExportMode::kVisualization) {
    p3rsmp::VisualizationInput vin;
    vin.measurement_capable = in.measurement_capable;
    vin.writes_variance = in.uncertainty_available && in.measurement_capable;
    vin.writes_ivar = false;
    vin.writes_point_information = false;
    vin.degradation_reason_present = true;
    vin.degradation_reason = "visualization_non_measurement";
    const p3rsmp::VisualizationResult vr = p3rsmp::propagate_visualization(vin);
    if (vr.status != p3rsmp::Status::Ok) {
      out.status = vr.status;
      out.code = vr.code;
      out.reason = vr.reason;
      return out;
    }
    const p3rsmp::NeighborhoodSet nb = build_neighborhood(grid, in, plan, true);
    const p3rsmp::SparseOperator rop = p3rsmp::build_row_normalized(nb, kernel);
    out.signal = p3rsmp::apply_operator(rop, in.x);
    std::vector<aio::FitsCard> cards = wcs_cards(grid.descriptor);
    cards.push_back(aio::FitsCard::make_string("BUNIT", "ADU/px^2", ""));
    cards.push_back(aio::FitsCard::make_logical("MEASFLAG", false,
                                                "measurement_capable=false"));
    std::vector<aio::FitsLayer> layers;
    layers.push_back(make_layer("", grid.width, grid.height, cards, out.signal));
    out.hdu_names = {"SIGNAL"};
    const std::vector<aio::ExpectedHdu> expected = aio::expected_from_layers(layers);

    aio::Provenance prov = make_provenance(
        mode, in, grid, kernel, "sha256:pending", true,
        in.uncertainty_unavailable_reason.empty() ? "visualization_output_is_non_measurement"
                                                  : in.uncertainty_unavailable_reason);
    json pj = aio::provenance_to_json(prov);
    if (!in.omit_provenance_key.empty()) pj.erase(in.omit_provenance_key);
    const aio::ValidationReport pr = validate_provenance_json_full(pj, prov);
    if (!pr.ok()) {
      out.status = p3rsmp::Status::Reject;
      out.code = "G-P3-VIS-PROV";
      out.reason = pr.summary();
      return out;
    }
    mkdirs(product_dir);
    const aio::PublishResult pub = publish_layers_streaming(
        out.fits_path, layers, expected, opts, cancel, kRowsPerBand, false);
    if (pub.status != aio::PublishStatus::kOk) {
      out.status = p3rsmp::Status::Reject;
      out.code = "G-P3-IO";
      out.reason = pub.message;
      out.publish = pub;
      return out;
    }
    out.publish = pub;
    prov.output_hash = "sha256:" + pub.sha256_hex;
    json pj2 = aio::provenance_to_json(prov);
    if (!in.omit_provenance_key.empty()) pj2.erase(in.omit_provenance_key);
    const aio::PublishResult ppub =
        publish_provenance(out.provenance_path, pj2.dump(2), opts, cancel);
    if (ppub.status != aio::PublishStatus::kOk) {
      std::remove(out.fits_path.c_str());
      out.status = p3rsmp::Status::Reject;
      out.code = "G-P3-IO-PROV";
      out.reason = ppub.message;
      return out;
    }
    out.provenance_reopen_check = verify_product_on_disk(product_dir, expected, &out.reopen);
    out.status = p3rsmp::Status::Ok;
    return out;
  }

  // ---- surface_brightness / point_source_flux：R 行归一主面 + 模式扩展 ----
  const p3rsmp::NeighborhoodSet nb = build_neighborhood(grid, in, plan, false);
  const p3rsmp::SparseOperator rop = p3rsmp::build_row_normalized(nb, kernel);

  p3rsmp::GateConfig cfg;
  p3rsmp::SurfaceBrightnessInput si;
  si.x = in.x;
  si.c_in = in.c_in;
  si.measurement_capable = in.measurement_capable;
  si.flux_conversion_requested = in.flux_conversion_requested;
  si.omega_per_pixel_present = !grid.omega_out_sr.empty();
  si.uncertainty_available = in.uncertainty_available;
  si.uncertainty_unavailable_reason = in.uncertainty_unavailable_reason;
  si.covariance = in.covariance_diagonal ? p3rsmp::CovarianceRepresentation::DiagonalOnly
                                         : p3rsmp::CovarianceRepresentation::ExactFull;
  si.variance_from = "propagated_covariance";
  if (!in.inject_weight_source.empty()) si.weight_sources.push_back(in.inject_weight_source);
  si.uses_relative_weight_as_ivar = false;
  if (in.force_omega_absent) si.omega_per_pixel_present = false;

  const p3rsmp::SurfaceBrightnessResult sb =
      p3rsmp::propagate_surface_brightness(rop, si, cfg);
  if (sb.status != p3rsmp::Status::Ok) {
    out.status = sb.status;
    out.code = sb.code;
    out.reason = sb.reason;
    return out;
  }
  out.signal = sb.y;
  out.variance = sb.variance;

  std::vector<aio::FitsLayer> layers;
  std::vector<std::string> hdu_names;
  {
    std::vector<aio::FitsCard> cards = wcs_cards(grid.descriptor);
    cards.push_back(aio::FitsCard::make_string("BUNIT", "ADU/px^2", ""));
    cards.push_back(aio::FitsCard::make_logical("MEASFLAG", true, "measurement_capable"));
    layers.push_back(make_layer("", grid.width, grid.height, cards, out.signal));
    hdu_names.push_back("SIGNAL");
  }
  if (in.uncertainty_available) {
    std::vector<aio::FitsCard> vc;
    vc.push_back(aio::FitsCard::make_string(
        "BUNIT", in.force_variance_unit.empty() ? "ADU^2/px^4" : in.force_variance_unit, ""));
    layers.push_back(make_layer("VARIANCE", grid.width, grid.height, vc, out.variance));
    hdu_names.push_back("VARIANCE");
  }
  {
    std::vector<aio::FitsCard> cc;
    cc.push_back(aio::FitsCard::make_string("BUNIT", "1", ""));
    layers.push_back(make_layer("COVERAGE", grid.width, grid.height, cc, rop.coverage));
    hdu_names.push_back("COVERAGE");
  }

  // ---- point_source_flux 扩展：列归一 S -> f=S d, pi=S p, C_y=S C_d S^T, Q/W 输出帧重算 ----
  if (mode == ExportMode::kPointSourceFlux) {
    const p3rsmp::SparseOperator sop = p3rsmp::build_column_normalized(nb, kernel);
    p3rsmp::PointSourceInput pi;
    pi.s_op = sop;
    pi.x = in.x;
    pi.c_x = in.c_in;
    pi.geom = nb.geom;
    pi.x_is_surface_brightness = true;
    pi.psf_p = in.psf_p;
    pi.psf_present = in.psf_present;
    pi.point_information_present = in.point_information_present;
    pi.rebuildable_from_frames = in.rebuildable_from_frames;
    pi.photometric_scale_present = (in.photometric_scale_a > 0.0);
    pi.a = in.photometric_scale_a;
    pi.effective_psf_present = in.effective_psf_present;
    pi.effective_psf_fwhm_only = in.effective_psf_fwhm_only;
    pi.effective_psf_normalization_declared = in.effective_psf_normalization_declared;
    pi.input_qw_resampled = in.input_qw_resampled;
    pi.w_from_sum_input = in.w_from_sum_input;
    pi.upstream_w_recomputed = in.upstream_w_recomputed;
    pi.covariance = in.covariance_diagonal
                        ? p3rsmp::CovarianceRepresentation::DiagonalOnly
                        : p3rsmp::CovarianceRepresentation::ExactFull;
    pi.variance_from = "propagated_covariance";
    if (!in.inject_weight_source.empty()) pi.weight_sources.push_back(in.inject_weight_source);
    const p3rsmp::PointSourceResult ps = p3rsmp::propagate_point_source_flux(pi, cfg);
    if (ps.status != p3rsmp::Status::Ok) {
      out.status = ps.status;
      out.code = ps.code;
      out.reason = ps.reason;
      return out;
    }
    if (!ps.frame_is_output_recompute) {
      out.status = p3rsmp::Status::Reject;
      out.code = "G-P3-QW-FRAME";
      out.reason = "qw_not_recomputed_in_output_frame";
      return out;
    }
    out.flux = ps.f;
    out.effective_psf = ps.pi;
    out.Q = ps.Q;
    out.W = ps.W;
    out.F_hat = ps.F_hat;
    out.var_F_hat = ps.var_F_hat;
    out.frame_is_output_recompute = true;
    {
      std::vector<aio::FitsCard> fc;
      fc.push_back(aio::FitsCard::make_string("BUNIT", "ADU", ""));
      fc.push_back(real_card("QSTAT", ps.Q, "ADE^-1"));
      fc.push_back(real_card("WSTAT", ps.W, "ADE^-2"));
      fc.push_back(real_card("FHAT", ps.F_hat, "ADU"));
      fc.push_back(real_card("VARFHAT", ps.var_F_hat, "ADU^2"));
      fc.push_back(aio::FitsCard::make_logical("QWRECOMP", true, "output_frame"));
      layers.push_back(make_layer("FLUX", grid.width, grid.height, fc, out.flux));
      hdu_names.push_back("FLUX");
    }
    {
      std::vector<aio::FitsCard> fv;
      fv.push_back(aio::FitsCard::make_string("BUNIT", "ADU^2", ""));
      layers.push_back(make_layer("FLUX_VARIANCE", grid.width, grid.height, fv,
                                  p3rsmp::matrix_diagonal(ps.c_y)));
      hdu_names.push_back("FLUX_VARIANCE");
    }
    {
      std::vector<aio::FitsCard> ec;
      ec.push_back(aio::FitsCard::make_string("BUNIT", "1", ""));
      ec.push_back(real_card("PSFSUM", 1.0, ""));
      layers.push_back(make_layer("EFFECTIVE_PSF", grid.width, grid.height, ec,
                                  out.effective_psf));
      hdu_names.push_back("EFFECTIVE_PSF");
    }
  }

  out.hdu_names = hdu_names;
  const std::vector<aio::ExpectedHdu> expected = aio::expected_from_layers(layers);

  aio::Provenance prov = make_provenance(mode, in, grid, kernel, "sha256:pending",
                                         !in.uncertainty_available,
                                         in.uncertainty_unavailable_reason);
  if (!in.force_variance_unit.empty()) prov.variance_unit = in.force_variance_unit;
  json pj = aio::provenance_to_json(prov);
  if (!in.omit_provenance_key.empty()) pj.erase(in.omit_provenance_key);
  const aio::ValidationReport pr = validate_provenance_json_full(pj, prov);
  if (!pr.ok()) {
    out.status = p3rsmp::Status::Reject;
    out.code = "G-P3-PROV";
    out.reason = pr.summary();
    return out;
  }

  mkdirs(product_dir);
  const aio::PublishResult pub = publish_layers_streaming(
      out.fits_path, layers, expected, opts, cancel, kRowsPerBand,
      in.force_publish_verify_fail);
  if (pub.status != aio::PublishStatus::kOk) {
    out.status = p3rsmp::Status::Reject;
    out.code = "G-P3-IO";
    out.reason = pub.message;
    out.publish = pub;
    return out;
  }
  out.publish = pub;
  out.wrote_variance = in.uncertainty_available;
  out.wrote_flux = (mode == ExportMode::kPointSourceFlux);
  out.wrote_effective_psf = (mode == ExportMode::kPointSourceFlux);

  prov.output_hash = "sha256:" + pub.sha256_hex;
  json pj2 = aio::provenance_to_json(prov);
  if (!in.omit_provenance_key.empty()) pj2.erase(in.omit_provenance_key);
  const aio::PublishResult ppub =
      publish_provenance(out.provenance_path, pj2.dump(2), opts, cancel);
  if (ppub.status != aio::PublishStatus::kOk) {
    std::remove(out.fits_path.c_str());
    out.status = p3rsmp::Status::Reject;
    out.code = "G-P3-IO-PROV";
    out.reason = ppub.message;
    return out;
  }
  out.provenance_reopen_check = verify_product_on_disk(product_dir, expected, &out.reopen);
  if (!out.provenance_reopen_check.ok() || !out.reopen.ok) {
    out.status = p3rsmp::Status::Reject;
    out.code = "G-P3-REOPEN";
    out.reason = out.reopen.error + " " + out.provenance_reopen_check.summary();
    return out;
  }
  out.status = p3rsmp::Status::Ok;
  return out;
}

// ---------------------------------------------------------------------------
// 独立重开校验
// ---------------------------------------------------------------------------
aio::ValidationReport verify_product_on_disk(
    const std::string& product_dir,
    const std::vector<aio::ExpectedHdu>& expected,
    aio::FitsVerifyResult* reopen_out) {
  aio::ValidationReport report;
  const std::string fits_path = product_dir + "/product.fits";
  const std::string prov_path = product_dir + "/provenance.json";
  const aio::FitsVerifyResult vr = aio::verify_fits_file(fits_path, expected);
  if (reopen_out != nullptr) *reopen_out = vr;
  if (!vr.ok) {
    report.add("G-FITS-REOPEN", "ALG-P3-008",
               vr.error.empty()
                   ? (vr.violations.empty()
                          ? std::string("fits reopen failed")
                          : vr.violations.front().gate + ": " +
                                vr.violations.front().message)
                   : vr.error);
  }
  std::ifstream f(prov_path);
  if (!f) {
    report.add("G-PROV-REOPEN", "FZ-PROV-MINIMAL-SET", "provenance.json not reopenable");
    return report;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  json j;
  try {
    j = json::parse(ss.str());
  } catch (const std::exception& e) {
    report.add("G-PROV-REOPEN", "FZ-PROV-MINIMAL-SET",
               std::string("provenance parse failed: ") + e.what());
    return report;
  }
  report.merge(aio::validate_provenance_json(j, aio::provenance_required_keys()));
  // 由重开后的 units 重建二次律（与 C++ 内存态独立，逐层对照冻结单位表）。
  aio::ProvenanceUnits u;
  u.bunit = j.value("units", json::object()).value("bunit", std::string());
  u.pixel_semantics =
      j.value("units", json::object()).value("pixel_semantics", std::string());
  u.pixel_area_power = j.value("units", json::object()).value("pixel_area_power", 0);
  u.has_target_pixel_area =
      j.value("units", json::object()).contains("target_pixel_area");
  if (u.has_target_pixel_area) {
    u.target_pixel_area = j["units"]["target_pixel_area"].get<double>();
  }
  aio::UnitExponents se;
  if (aio::parse_unit_exponents(u.bunit, &se)) {
    const std::string signal = u.bunit;
    const std::string variance =
        aio::format_unit_exponents({2 * se.adu_power, 2 * se.px_power});
    const std::string ivar =
        aio::format_unit_exponents({-2 * se.adu_power, -2 * se.px_power});
    report.merge(aio::validate_bunit_law(u, signal, variance, ivar));
  } else {
    report.add("G-BUNIT-SEMANTICS", "FZ-BUNIT-SEMANTICS",
               "reopened units.bunit unparsable: " + u.bunit);
  }
  return report;
}

}  // namespace v6
}  // namespace phase3
}  // namespace astrocs
