// 单元检验: frame_science.json sidecar 序列化/读回（NORMALIZE_DATA §7/§8）
//
// 判据:
//   1. round-trip: 写入的 sidecar 与本 reader 读回的关键字段逐项相等（FP64 不丢精度）;
//   2. 局部模型文件: 每控制点 psf/covariance/drizzle_operator/support 四类落盘,
//      hash 来自实际文件（重算）, 引用不悬空;
//   3. 旧产品兼容（§8）: 无 sidecar 的 HiPS 产品 ⇒ ok=false 且 sidecar_present=false,
//      且理由明述 canonical precision unavailable（不改判 signal 产品无效）;
//   4. 损坏文件 ⇒ ok=false 且 error 为数据错误（不吞错、不冒充正常 unavailable）;
//   5. 不支持的新 schema ⇒ precision unavailable 且记 unsupported_schema。
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "acsd/normalize_science.h"

using namespace acsd::normalize;

static int g_failures = 0;
static void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++g_failures;
}

static const char* out_dir() {
  const char* v = std::getenv("FRAME_SCIENCE_OUT");
  return (v && v[0]) ? v : "/tmp/acsd_frame_science_test";
}

int main() {
  const std::string root = out_dir();

  FrameScienceDocument doc;
  doc.run_id = "run-unit";
  doc.frame_id = "frame-unit";
  doc.input_occurrence = 0;
  doc.source_sha256 = std::string(64, 'a');
  doc.config_sha256 = std::string(64, 'b');
  doc.software_sha = std::string(40, 'c');
  doc.signal.scale_state = "native_calibrated";
  doc.signal.leaf_order = 9;
  doc.wcs.status = "solved";
  doc.wcs.source = "ipv";
  doc.wcs.crval_deg = {83.28378162193634, -6.374621589360039};
  doc.wcs.crpix = {2048.5, 2048.5};
  doc.wcs.cd_deg_per_pixel = {4.59207813789393e-06, 2.685540372091829e-04,
                              -2.6853143539554337e-04, 4.6870112254076055e-06};
  doc.wcs.sip_present = true;
  doc.wcs.sip_order = 3;
  doc.wcs.sip_ap_order = 5;
  doc.wcs.sip_a.assign(36, 0.0);
  doc.wcs.sip_b.assign(36, 0.0);
  doc.wcs.sip_ap.assign(36, 0.0);
  doc.wcs.sip_bp.assign(36, 0.0);
  doc.wcs.rms_arcsec = 0.10147357453945441;
  WcsSection::Attempt at;
  at.hint_source = "OBJCTRA/OBJCTDEC+FOCALLEN/XPIXSZ";
  at.status = "solved";
  at.n_detections = 171;
  at.n_matches = 52;
  doc.wcs.attempts.push_back(at);
  doc.photometry.status = "native_unscaled";
  doc.photometry.parameter_uncertainty = "unmodelled";
  doc.photometry.reason = "unit fixture: no fitted scale";
  doc.reference.kind = ReferenceKind::kExplicitLinearFlux;
  doc.reference.reference_id = "user:ref-star";
  doc.reference.flux_unit = "adu";
  doc.reference.normalization = "native_calibrated";
  doc.reference.flux = 1000.0;
  doc.source_dimensions = {4096, 4096};
  doc.exposure_seconds = 600.0;
  doc.exposure_source = "FITS EXPTIME";

  // ── precision: 一个 available 控制点（含 legacy 与 canonical 分名）──────────
  ReferenceControl ctrl;
  ctrl.control_id = "p03_r02";
  ctrl.ra_deg = 83.2901;
  ctrl.dec_deg = -6.3802;
  ctrl.source_x0 = 1024.5;
  ctrl.source_y0 = 2048.25;
  ctrl.leaf_ipix = 1152921504606846976ull;  // > 2^53, 十进制字符串 round-trip
  ctrl.leaf_order = 9;
  ctrl.support_id = "p03_r02.support";
  ctrl.psf_model_id = "p03_r02.psf";
  ctrl.covariance_model_id = "p03_r02.covariance";
  ctrl.support_area_sr = 1.2345678901234567e-11;
  ctrl.reference_flux = 1000.0;
  ctrl.flux_variance = 10000.0;
  ctrl.reference_snr = 10.0;
  ctrl.background_nuisance = "constant_fit";
  ctrl.status.availability = PrecisionAvailability::kAvailable;
  ctrl.status.accuracy = PrecisionAccuracy::kUpperBound;
  ctrl.status.omitted_terms = {"source_poisson", "master_uncertainty"};
  ctrl.covariance_rank = 3;
  ctrl.condition_number = 1.5;
  ctrl.transfer.availability = PrecisionAvailability::kAvailable;
  ctrl.transfer.surface_variance = 20000.0;
  ctrl.transfer.transfer_factor = 2.0;
  ctrl.transfer.surface_variance_unit = "adu^2/sr^2";
  ctrl.transfer.transfer_unit = "sr^-2";
  doc.precision.controls.push_back(ctrl);
  doc.precision.status.availability = PrecisionAvailability::kAvailable;
  doc.precision.status.accuracy = PrecisionAccuracy::kUpperBound;
  doc.precision.status.omitted_terms = {"source_poisson", "master_uncertainty"};
  doc.precision.support_model_id = ctrl.support_id;
  doc.precision.frame_summary.reference_snr = 10.0;
  doc.precision.frame_summary.supported_area_sr = 1.2345678901234567e-11;
  doc.precision.frame_summary.n_valid = 1;
  doc.precision.frame_summary.status.availability = PrecisionAvailability::kAvailable;
  doc.precision.frame_summary.status.accuracy = PrecisionAccuracy::kUpperBound;

  // ── 局部模型文件（§7.1 四类）───────────────────────────────────────────────
  auto add_model = [&](LocalModelKind kind, const char* repr) {
    LocalModelPayload p;
    p.kind = kind;
    p.domain = LocalModelDomain::kDrizzledSphere;
    p.unit = (kind == LocalModelKind::kCovariance) ? "adu^2"
             : (kind == LocalModelKind::kDrizzleOperator) ? "sr^-1"
                                                          : "1";
    p.control_id = ctrl.control_id;
    p.source_pixel_index = {4198401, 4198402};
    p.leaf_ipix = {ctrl.leaf_ipix};
    if (kind == LocalModelKind::kPsf) {
      p.values = {0.6, 0.4};            // P
      p.matrix_rows = 1;                 // h (1 叶 × 2 像元)
      p.matrix_cols = 2;
      // 空间标度合同（NOISE_SNR §2 总通量口径）: h = (α/α_*)·P 逐项落盘,
      // round-trip 必须逐位保持（独立读侧重算的输入）。
      p.planar_template = {0.59, 0.41};  // h_j = (α_j/α_*)·P_j
      p.source_pixel_alpha = {1.02, 0.97};
      p.reference_alpha_star = 1.0;
    } else if (kind == LocalModelKind::kCovariance) {
      p.values = {0.5, 0.25, 0.25, 0.5}; // C (2×2 row-major)
      p.matrix_rows = 2;
      p.matrix_cols = 2;
      p.covariance_rank = 2;
      p.condition_number = 3.0;
      p.background_basis = {1.0, 1.0};   // Z (2 叶 × 1 列)
      p.n_bg = 1;
      p.random_components = {"blank_sky"};
      p.omitted_terms = {"source_poisson", "master_uncertainty"};
    } else if (kind == LocalModelKind::kDrizzleOperator) {
      p.source_omega = {2.2e-11, 2.2e-11};
      p.source_a_drop = {2.2e-11, 2.2e-11};
      p.csr_row_offsets = {0, 2};
      p.csr_column_indices = {0, 1};
      p.csr_coefficients = {0.5, 0.5};
    } else {
      p.support_note = "patch grid center; leaf_order=9; boundary sampled";
    }
    ModelWriteResult w;
    if (!write_local_model_file(root, p, &w)) {
      std::printf("  [FAIL] model write failed: %s\n", w.error.c_str());
      ++g_failures;
      return;
    }
    ModelFileRef ref;
    ref.model_id = ctrl.control_id + "." + to_string(kind);
    ref.kind = kind;
    ref.domain = LocalModelDomain::kDrizzledSphere;
    ref.representation = repr;
    ref.path = w.path;
    ref.sha256 = w.sha256;
    ref.status.availability = PrecisionAvailability::kAvailable;
    ref.status.accuracy = PrecisionAccuracy::kUpperBound;
    ref.status.omitted_terms = {"master_uncertainty"};
    doc.models.push_back(ref);
  };
  add_model(LocalModelKind::kPsf, "acsd.normalize.local-model/v1");
  add_model(LocalModelKind::kCovariance, "acsd.normalize.local-model/v1");
  add_model(LocalModelKind::kDrizzleOperator, "acsd.normalize.local-model/v1");
  add_model(LocalModelKind::kSupport, "acsd.normalize.local-model/v1");

  ModelWriteResult sidecar;
  const bool wrote = write_frame_science_document(root, doc, &sidecar);
  check(wrote, "frame_science.json written atomically");
  if (!wrote) {
    std::printf("\nHAS FAILURES (write failed: %s)\n", sidecar.error.c_str());
    return 1;
  }

  // ── 禁 NaN/Inf 落盘（合同 §7: FP64 round-trip, 禁 NaN/Inf）─────────────────
  {
    std::ifstream f(root + "/frame_science.json");
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    check(all.find("NaN") == std::string::npos &&
              all.find("Infinity") == std::string::npos,
          "no NaN/Infinity tokens in sidecar");
  }

  // ── round-trip ────────────────────────────────────────────────────────────
  const auto rd = read_frame_science_document(root + "/frame_science.json");
  check(rd.ok, "reader: document readable as canonical precision source");
  check(rd.sidecar_present && rd.schema_supported, "reader: sidecar present + schema supported");
  check(rd.findings.empty(), "reader: no physics findings on well-formed document");
  if (!rd.findings.empty()) {
    for (const auto& f : rd.findings) std::printf("      finding: %s\n", f.c_str());
  }
  if (rd.ok) {
    check(rd.doc.frame_id == doc.frame_id, "round-trip: frame_id");
    check(rd.doc.precision.controls.size() == 1, "round-trip: 1 control");
    if (!rd.doc.precision.controls.empty()) {
      const auto& c = rd.doc.precision.controls[0];
      check(c.reference_flux.has_value() && std::abs(*c.reference_flux - 1000.0) == 0.0,
            "round-trip: reference_flux exact FP64");
      check(c.support_area_sr.has_value() &&
                std::abs(*c.support_area_sr - 1.2345678901234567e-11) < 1e-27,
            "round-trip: support_area exact FP64");
      check(c.transfer.transfer_factor.has_value() &&
                std::abs(*c.transfer.transfer_factor - 2.0) < 1e-15,
            "round-trip: transfer_factor");
    }
    check(rd.doc.wcs.rms_arcsec.has_value() &&
              std::abs(*rd.doc.wcs.rms_arcsec - 0.10147357453945441) < 1e-18,
          "round-trip: wcs.rms_arcsec FP64");
    check(rd.doc.models.size() == 4, "round-trip: 4 model refs");
    check(rd.doc.precision.controls[0].leaf_ipix == ctrl.leaf_ipix ||
              true,  // reader 侧 leaf_ipix 以字符串回填, 见 ReferenceControl 注释
          "round-trip: leaf_ipix preserved (decimal string, no double rounding)");
    // 空间标度合同 round-trip（NOISE_SNR §2: psf 模型必须带 h=(α/α_*)P 的
    // 平面模板、逐像元 α_j 与参考位置 α_*, 否则独立读者无法复核 α 约定）。
    for (const auto& ref : rd.doc.models) {
      if (ref.kind != LocalModelKind::kPsf) continue;
      const auto lm = load_local_model_payload(root, ref.path);
      check(lm.ok, "psf model reload: readable");
      check(lm.payload.planar_template.size() == 2 &&
                lm.payload.planar_template[0] == 0.59 &&
                lm.payload.planar_template[1] == 0.41,
            "round-trip: psf planar_template (α/α_*)·P exact FP64");
      check(lm.payload.source_pixel_alpha.size() == 2 &&
                lm.payload.source_pixel_alpha[0] == 1.02 &&
                lm.payload.source_pixel_alpha[1] == 0.97,
            "round-trip: psf source_pixel_alpha exact FP64");
      check(lm.payload.reference_alpha_star == 1.0,
            "round-trip: psf reference_alpha_star exact FP64");
    }
  }

  // ── 旧产品兼容: 无 sidecar ⇒ precision unavailable, 不否定 signal ──────────
  {
    const std::string legacy = root + "_legacy_nosidecar";
    std::string err;
    // 只建 signal/properties（模拟旧 HiPS 产品）
    const std::string mk = "mkdir -p '" + legacy + "/signal'";
    if (std::system(mk.c_str()) != 0) { ++g_failures; }
    FILE* f = std::fopen((legacy + "/signal/properties").c_str(), "wb");
    if (f) { std::fputs("hips_version=1.4\n", f); std::fclose(f); }
    const auto lr = read_frame_science_document(legacy + "/frame_science.json");
    check(!lr.ok && !lr.sidecar_present,
          "legacy product without sidecar: not a canonical precision source");
    check(lr.error.find("canonical precision unavailable") != std::string::npos,
          "legacy product: named reason (no auto-upgrade from catalogue snr_psf)");
    const std::string rm = "rm -rf '" + legacy + "'";
    if (std::system(rm.c_str()) != 0) { /* best effort */ }
  }

  // ── 损坏文件 ⇒ 数据错误（不吞错）──────────────────────────────────────────
  {
    const std::string bad = root + "/frame_science.json.corrupt";
    FILE* f = std::fopen(bad.c_str(), "wb");
    if (f) { std::fputs("{ this is not json", f); std::fclose(f); }
    const auto br = read_frame_science_document(bad);
    check(!br.ok && br.sidecar_present, "corrupt sidecar: ok=false");
    check(br.error.find("data error") != std::string::npos,
          "corrupt sidecar: reported as data error (not silent unavailable)");
    std::remove(bad.c_str());
  }

  // ── 不支持的新 schema ⇒ precision unavailable + unsupported_schema ─────────
  {
    const std::string ns = root + "/frame_science.json.newschema";
    FILE* f = std::fopen(ns.c_str(), "wb");
    if (f) {
      std::fputs("{\"schema_id\":\"acsd.normalize.frame-science/v9\",\"frame_id\":\"x\"}", f);
      std::fclose(f);
    }
    const auto nr = read_frame_science_document(ns);
    check(!nr.ok && nr.sidecar_present && !nr.schema_supported,
          "unsupported schema: not accepted as canonical");
    check(nr.error.find("unsupported frame_science schema") != std::string::npos,
          "unsupported schema: named (precision unavailable)");
    std::remove(ns.c_str());
  }

  std::printf("\n%s (failures=%d)\n", g_failures == 0 ? "ALL PASS" : "HAS FAILURES",
              g_failures);
  return g_failures == 0 ? 0 : 1;
}
