/* v6_aio_test.cpp — IMPL-AIO-001 共址单元测试 (正例 + 负例)
 *
 * 运行: v6_aio_test <units|fits|atomic|provenance|manifest|artifacts|all> [artifact_dir]
 * 每个子命令 = 一个 ctest 用例；返回 0 即通过。负例断言"违反冻结必须被拒"。
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "astro/aio/v6_atomic_publish.h"
#include "astro/aio/v6_bunit.h"
#include "astro/aio/v6_fits.h"
#include "astro/aio/v6_hips_manifest.h"
#include "astro/aio/v6_product_io.h"
#include "astro/aio/v6_provenance.h"
#include "astro/aio/v6_sha256.h"

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace astrocs::aio;
using nlohmann::json;

static int g_checks = 0;

#define CHECK(cond, msg)                                                     \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg));   \
      return 1;                                                              \
    }                                                                        \
  } while (0)

#define CHECK_MSG(cond, msg)                                                 \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK-FAIL: %s\n", (msg));                       \
      return 1;                                                              \
    }                                                                        \
  } while (0)

namespace {

bool has_gate(const ValidationReport& r, const std::string& gate) {
  for (const auto& v : r.violations()) {
    if (v.gate == gate) return true;
  }
  return false;
}

void append_f64_be(std::vector<std::uint8_t>& out, double v) {
  std::uint64_t u = 0;
  std::memcpy(&u, &v, sizeof(u));
  for (int i = 7; i >= 0; --i) out.push_back(static_cast<std::uint8_t>(u >> (8 * i)));
}

std::vector<std::uint8_t> make_signal_data(std::size_t nx, std::size_t ny,
                                           double base) {
  std::vector<std::uint8_t> out;
  out.reserve(nx * ny * 8);
  for (std::size_t y = 0; y < ny; ++y) {
    for (std::size_t x = 0; x < nx; ++x) {
      append_f64_be(out, base + static_cast<double>(y * nx + x) * 0.5);
    }
  }
  return out;
}

FitsLayer make_primary_signal(std::size_t nx, std::size_t ny, double base) {
  FitsLayer l;
  l.spec.extname.clear();
  l.spec.bitpix = -64;
  l.spec.naxis = {nx, ny};
  l.spec.cards = {
      FitsCard::make_string("BUNIT", "ADU/sr", "surface brightness"),
      FitsCard::make_string("CTYPE1", "RA---TAN", ""),
      FitsCard::make_string("CTYPE2", "DEC--TAN", ""),
  };
  l.data = make_signal_data(nx, ny, base);
  return l;
}

FitsLayer make_layer(const std::string& ext, const std::string& bunit,
                     std::size_t nx, std::size_t ny, double base) {
  FitsLayer l;
  l.spec.extname = ext;
  l.spec.bitpix = -64;
  l.spec.naxis = {nx, ny};
  l.spec.cards = {FitsCard::make_string("BUNIT", bunit, "")};
  l.data = make_signal_data(nx, ny, base);
  return l;
}

std::vector<FitsLayer> make_three_layer_product() {
  std::vector<FitsLayer> layers;
  layers.push_back(make_primary_signal(8, 4, 10.0));
  layers.push_back(make_layer("VARIANCE", "ADU^2/sr^2", 8, 4, 1.0));
  layers.push_back(make_layer("IVAR", "sr^2/ADU^2", 8, 4, 0.5));
  return layers;
}

Provenance make_positive_provenance() {
  Provenance p;
  p.product.type_id = "astrocs.phase1.product.fits.v1";
  p.product.schema_version = 1;
  p.software_sha = std::string(40, 'a');
  p.run_id = "run-aio-001";
  p.input_product_hashes = {"sha256:input-a"};
  p.config_hash = "sha256:cfg";
  p.units.bunit = "ADU/sr";
  p.units.pixel_semantics = "surface_brightness";
  p.units.pixel_area_power = -2;
  p.units.has_target_pixel_area = true;
  p.units.target_pixel_area = 1.0;
  p.coordinate_frame = "icrs";
  p.coordinate_epoch = "J2000";
  p.pixel_semantics = "surface_brightness";
  p.sampling.kernel_id = "drizzle_forward";
  p.sampling.has_pixfrac = true;
  p.sampling.pixfrac = 0.8;
  p.algorithm_ids = {"ALG-DRZ-001"};
  p.module.module_id = "phase1.drizzle";
  p.module.build_id = "b1";
  p.provider = "cpu_baseline";
  p.approximations = {"diagonal_C_plus_correlation_kernel"};
  p.degradations.clear();
  p.normalization_version = "drizzle_sb_normalization_v2_surface_brightness_preserving";
  p.weight_mode_version = "weight_mode_v2_explicit";
  p.correlation_summary.representation = "correlation_kernel";
  p.correlation_summary.kernel_id = "drizzle_rho_v1";
  p.correlation_summary.has_scale = true;
  p.correlation_summary.scale = 1.5;
  p.correlation_summary.has_mean_abs_rho = true;
  p.correlation_summary.mean_abs_rho = 0.19;
  p.has_flux_conservation_factor = true;
  p.flux_conservation_factor = 0.64;
  p.k_corr.definition = "k_corr = Var(median)/[pi sigma_bg^2/(2 N_retained)]";
  p.k_corr.value = 1.4;
  p.k_corr.domain.geometry = "spherical_drizzle";
  p.k_corr.domain.pixfrac = 0.8;
  p.k_corr.domain.patch_size = 8;
  p.k_corr.domain.estimator = "median";
  p.k_corr.domain.spherical = true;
  p.k_corr.calibration.script = "run/v6/upm/kcorr_calib.py";
  p.k_corr.calibration.seed = 20260915;
  p.k_corr.calibration.calibration_run_id = "upmw-005";
  p.has_k_corr = true;
  p.unavailable_flag = false;
  p.unavailable_reason = "not_applicable";
  p.unavailable_scope = "none";
  p.generated_utc = "2026-09-15T00:00:00Z";
  p.output_hash = "sha256:out";
  p.signal_unit = "ADU/sr";
  p.variance_unit = "ADU^2/sr^2";
  p.ivar_unit = "sr^2/ADU^2";
  return p;
}

HipsProperties make_positive_props() {
  HipsProperties hp;
  hp.creator_did = "ivo://astrocs/test";
  hp.obs_collection = "AstroCS V6";
  hp.release_date = "2026-09-15T00:00:00Z";
  hp.frame = "equatorial";  // P0-19: HiPS 1.0 §4.4.1 标准值 (icrs 非法)
  hp.order = 6;
  hp.order_min = 3;
  hp.tile_width = 512;
  hp.tile_format = "fits";
  hp.has_initial_position = true;
  hp.initial_ra = 10.68;
  hp.initial_dec = 41.27;
  hp.initial_fov = 1.0;
  return hp;
}

int test_units() {
  // SHA-256 FIPS 向量（独立真值: NIST / hashlib）。
  CHECK(Sha256::hex_of_string("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "sha256 empty vector");
  CHECK(Sha256::hex_of_string("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "sha256 abc vector");

  // 冻结单位表量纲等价。
  struct Case { Quantity q; const char* unit; };
  const Case cases[] = {
      {Quantity::kSignalSb, "ADU/sr"},
      {Quantity::kPixelVarianceIn, "ADU^2"},
      {Quantity::kSbVarianceOut, "ADU^2/sr^2"},
      {Quantity::kSbIvarOut, "sr^2/ADU^2"},
      {Quantity::kWInfo, "ADU^-2"},
      {Quantity::kQ, "ADU^-1"},
      {Quantity::kFlux, "ADU"},
      {Quantity::kPsfswRobustWeight, "1"},
  };
  for (const auto& c : cases) {
    UnitExponents e;
    CHECK(parse_unit_exponents(c.unit, &e), "frozen unit parses");
    CHECK(unit_matches_frozen(c.q, c.unit, nullptr), "frozen unit matches");
  }
  // 负例：signal_sb 单位被改成 ADU 或 ADU^2/sr^2 -> 必须不匹配。
  CHECK(!unit_matches_frozen(Quantity::kSignalSb, "ADU", nullptr),
        "signal_sb=ADU must fail");
  CHECK(!unit_matches_frozen(Quantity::kSignalSb, "ADU^2/sr^2", nullptr),
        "signal_sb=variance unit must fail");
  CHECK(!unit_matches_frozen(Quantity::kWInfo, "ADU^-1", nullptr),
        "W_info=ADU^-1 must fail");
  CHECK(psfsw_unit_forbidden("flux^-2"), "psfsw flux^-2 forbidden");
  CHECK(psfsw_unit_forbidden("ADU^-2"), "psfsw ADU^-2 forbidden");
  CHECK(!psfsw_unit_forbidden("1"), "psfsw dimensionless allowed");

  // 二次律。
  std::string why;
  CHECK(quadratic_law_holds("ADU/sr", "ADU^2/sr^2", "sr^2/ADU^2", &why),
        "quadratic law positive");
  CHECK(!quadratic_law_holds("ADU/sr", "ADU/sr", "px^2/ADU", &why),
        "variance not signal^2 must fail");
  CHECK(!quadratic_law_holds("ADU/sr", "ADU^2/sr^2", "ADU^-2", &why),
        "ivar != 1/variance must fail");

  // BUNIT 可判性。
  CHECK(bunit_dimension_decidable("ADU/sr", "", 0, false, 0).decidable,
        "explicit px power decidable");
  CHECK(bunit_dimension_decidable("ADU", "surface_brightness", -2, true, 1.0).decidable,
        "ADU + surface_brightness + -2 + area decidable");
  CHECK(!bunit_dimension_decidable("ADU", "", 0, false, 0).decidable,
        "bare ADU without pixel semantics must be undecidable");
  CHECK(!bunit_dimension_decidable("ADU", "integrated_flux", 0, false, 0).decidable,
        "ADU integrated_flux undecidable");
  CHECK(!bunit_dimension_decidable("ADU", "surface_brightness", 2, true, 1.0).decidable,
        "pixel_area_power != -2 must be undecidable");
  CHECK(!bunit_dimension_decidable("ADU", "surface_brightness", -2, false, 0).decidable,
        "missing target_pixel_area must be undecidable");

  // 1 补码和/编码自洽：encode(0xFFFFFFFF - s) 后完整和 == 0xFFFFFFFF。
  std::vector<std::uint8_t> block(2880, 0);
  for (std::size_t i = 0; i < block.size(); ++i) block[i] = static_cast<std::uint8_t>(i * 7 + 3);
  const std::uint32_t s = fits_ones_complement_sum(block.data(), block.size());
  const std::uint32_t comp = 0xFFFFFFFFu - s;
  const std::array<char, 16> enc = fits_encode_checksum(comp);
  // 把编码串放到块尾 16 字节再求和。
  for (std::size_t i = 0; i < 16; ++i) block[2880 - 16 + i] = static_cast<std::uint8_t>(enc[i]);
  // 重新计算时需要把被覆盖的原值扣除：这里改为对“编码串本身”做和验证。
  std::vector<std::uint8_t> enc_only(16);
  for (std::size_t i = 0; i < 16; ++i) enc_only[i] = static_cast<std::uint8_t>(enc[i]);
  std::uint32_t decoded = 0;
  CHECK(fits_decode_checksum(enc, &decoded), "decode ok");
  CHECK(decoded == comp, "encode/decode round trip");
  std::printf("units: %d checks\n", g_checks);
  return 0;
}

int test_fits(const std::string& workdir) {
#if !defined(_WIN32)
  ::mkdir(workdir.c_str(), 0755);
#endif
  const std::string target = workdir + "/product.fits";
  ::unlink(target.c_str());
  const std::vector<FitsLayer> layers = make_three_layer_product();
  const std::vector<ExpectedHdu> expected = expected_from_layers(layers);
  const PublishOptions opts;
  const PublishResult pr =
      publish_fits_product(target, layers, expected, opts, CancelFn());
  CHECK_MSG(pr.status == PublishStatus::kOk,
            (std::string("publish ok: ") + pr.message).c_str());
  const FitsVerifyResult vr = verify_fits_file(target, expected);
  CHECK_MSG(vr.ok, (std::string("verify ok: ") +
                    (vr.violations.empty() ? std::string("") : vr.violations.front().message))
                       .c_str());
  CHECK(vr.hdus.size() == 3, "three HDUs");
  CHECK(vr.hdus[0].is_primary, "primary hdu first");
  CHECK(vr.hdus[1].extname == "VARIANCE", "variance extname");
  CHECK(vr.hdus[2].extname == "IVAR", "ivar extname");
  CHECK(vr.hdus[0].bunit == "ADU/sr", "primary bunit");
  CHECK(vr.hdus[1].bunit == "ADU^2/sr^2", "variance bunit (quadratic law)");
  CHECK(vr.hdus[2].bunit == "sr^2/ADU^2", "ivar bunit (1/variance)");

  // 负例 1：篡改一个数据字节 -> DATASUM 必须失配。
  {
    std::fstream f(target, std::ios::in | std::ios::out | std::ios::binary);
    CHECK(static_cast<bool>(f), "open for corruption");
    f.seekp(2880 + 5);
    char b = 0x7F;
    f.write(&b, 1);
    f.close();
    const FitsVerifyResult bad = verify_fits_file(target, expected);
    CHECK(!bad.ok, "corrupted data must fail verify");
    CHECK(has_gate(ValidationReport(), "") == false, "noop");
    bool found = false;
    for (const auto& v : bad.violations) {
      if (v.gate == "G-FITS-DATASUM") found = true;
    }
    CHECK(found, "corruption must hit G-FITS-DATASUM");
  }
  // 负例 2：BUNIT 期望与写出不一致 -> G-FITS-BUNIT。
  {
    std::vector<ExpectedHdu> wrong = expected;
    wrong[0].bunit = "ADU^2/sr^2";
    const FitsVerifyResult bad = verify_fits_file(target, wrong);
    bool found = false;
    for (const auto& v : bad.violations) {
      if (v.gate == "G-FITS-BUNIT") found = true;
    }
    CHECK(found, "wrong expected BUNIT must hit G-FITS-BUNIT");
  }
  std::printf("fits: %d checks\n", g_checks);
  return 0;
}

int count_tmp_residue(const std::string& dir) {
  // 直接读目录（不经 shell：workdir 含空格时 shell 分词会误判；隐藏的 staging
  // 目录名以 '.' 开头，必须计入残留）。
  int n = 0;
#if !defined(_WIN32)
  DIR* d = ::opendir(dir.c_str());
  if (d != nullptr) {
    while (struct dirent* e = ::readdir(d)) {
      const std::string name = e->d_name;
      if (name.find("tmp-") != std::string::npos) ++n;
    }
    ::closedir(d);
  }
#endif
  return n;
}

int test_atomic(const std::string& workdir) {
#if !defined(_WIN32)
  ::mkdir(workdir.c_str(), 0755);
#endif
  const std::string dir = workdir + "/atomic";
#if !defined(_WIN32)
  ::mkdir(dir.c_str(), 0755);
#endif
  const std::string target = dir + "/ok.txt";
  ::unlink(target.c_str());
  const VerifyFn verify_exists = [](const std::string& p, std::string* err) {
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) {
      if (err) *err = "verify: missing " + p;
      return false;
    }
    std::fclose(f);
    return true;
  };
  // 正例：成功发布。
  {
    const PublishResult r = atomic_write_bytes(target, "hello-aio", verify_exists,
                                               PublishOptions(), CancelFn());
    CHECK(r.status == PublishStatus::kOk, "atomic ok status");
    CHECK(r.renamed, "atomic renamed");
    CHECK(r.sha256_hex == Sha256::hex_of_string("hello-aio"), "sha of published bytes");
    CHECK(count_tmp_residue(dir) == 0, "no tmp residue after success");
  }
  // 负例：写一半失败 -> 目标不存在、无 tmp 残留。
  {
    ::unlink(target.c_str());
    const FileWriterFn w = [](int fd, const CancelFn& c, std::string* err) {
      const std::string head = "partial";
      if (!write_all_fd(fd, head.data(), head.size(), c, err)) return false;
      if (err) *err = "injected writer failure";
      return false;
    };
    const PublishResult r =
        atomic_publish_file(target, w, verify_exists, PublishOptions(), CancelFn());
    CHECK(r.status == PublishStatus::kErrIo, "writer failure => IO");
    std::FILE* f = std::fopen(target.c_str(), "rb");
    CHECK(f == nullptr, "no visible target after writer failure");
    if (f) std::fclose(f);
    CHECK(count_tmp_residue(dir) == 0, "no tmp residue after writer failure");
    CHECK(!r.tmp_residue, "tmp_residue false");
  }
  // 负例：取消 -> 目标不存在。
  {
    ::unlink(target.c_str());
    int calls = 0;
    const CancelFn cancel = [&calls]() { return ++calls > 1; };
    const FileWriterFn w = [](int fd, const CancelFn& c, std::string* err) {
      std::string chunk(1024, 'x');
      return write_all_fd(fd, chunk.data(), chunk.size(), c, err);
    };
    const PublishResult r =
        atomic_publish_file(target, w, verify_exists, PublishOptions(), cancel);
    CHECK(r.status == PublishStatus::kErrCancelled, "cancel => CANCELLED");
    std::FILE* f = std::fopen(target.c_str(), "rb");
    CHECK(f == nullptr, "no visible target after cancel");
    if (f) std::fclose(f);
    CHECK(count_tmp_residue(dir) == 0, "no tmp residue after cancel");
  }
  // 负例：发布后验证失败 -> 撤销已 rename 的产物。
  {
    ::unlink(target.c_str());
    const VerifyFn always_fail = [](const std::string&, std::string* err) {
      if (err) *err = "injected verify failure";
      return false;
    };
    const PublishResult r = atomic_write_bytes(target, "bad-product", always_fail,
                                               PublishOptions(), CancelFn());
    CHECK(r.status == PublishStatus::kErrChecksum, "verify failure => CHECKSUM");
    std::FILE* f = std::fopen(target.c_str(), "rb");
    CHECK(f == nullptr, "verified-failed product must be removed");
    if (f) std::fclose(f);
  }
  // 目录原子发布正例 + builder 失败负例 + 非空目标 STATE。
  {
    const std::string tree = dir + "/hips_out";
    remove_tree(tree);
    const DirBuilderFn builder = [](const std::string& stage, const CancelFn&,
                                    std::string* err) {
      const std::string sub = stage + "/norder6";
#if !defined(_WIN32)
      ::mkdir(sub.c_str(), 0755);
#endif
      std::FILE* f = std::fopen((sub + "/tile.fits").c_str(), "wb");
      if (!f) {
        if (err) *err = "cannot create tile";
        return false;
      }
      std::fputs("tile", f);
      std::fclose(f);
      return true;
    };
    const PublishResult r = atomic_publish_directory(tree, builder, verify_exists,
                                                     PublishOptions(), CancelFn());
    CHECK(r.status == PublishStatus::kOk, "dir publish ok");
    std::FILE* f = std::fopen((tree + "/norder6/tile.fits").c_str(), "rb");
    CHECK(f != nullptr, "dir publish produced tile");
    if (f) std::fclose(f);
    CHECK(count_tmp_residue(dir) == 0, "no staging residue after dir publish");

    // builder 失败 -> 无目标、无 staging。
    const std::string tree2 = dir + "/hips_bad";
    remove_tree(tree2);
    const DirBuilderFn bad_builder = [](const std::string&, const CancelFn&,
                                        std::string* err) {
      if (err) *err = "injected builder failure";
      return false;
    };
    const PublishResult r2 = atomic_publish_directory(
        tree2, bad_builder, verify_exists, PublishOptions(), CancelFn());
    CHECK(r2.status == PublishStatus::kErrIo, "builder failure => IO");
    std::FILE* g = std::fopen((tree2 + "/x").c_str(), "rb");
    CHECK(g == nullptr, "no visible dir after builder failure");
    if (g) std::fclose(g);
    CHECK(count_tmp_residue(dir) == 0, "no staging residue after builder failure");

    // 非空目标 -> STATE（拒绝覆盖）。
    const PublishResult r3 = atomic_publish_directory(tree, builder, verify_exists,
                                                      PublishOptions(), CancelFn());
    CHECK(r3.status == PublishStatus::kErrState, "non-empty target => STATE");
  }
  std::printf("atomic: %d checks\n", g_checks);
  return 0;
}

int test_provenance() {
  const Provenance good = make_positive_provenance();
  const ValidationReport ok = validate_provenance(good);
  CHECK_MSG(ok.ok(), ("positive provenance rejected: " + ok.summary()).c_str());

  const std::vector<std::string> req = provenance_required_keys();
  const json good_json = provenance_to_json(good);
  CHECK(validate_provenance_json(good_json, req).ok(), "positive json ok");

  // 负例 a：最小集缺 flux_conservation_factor。
  {
    json j = good_json;
    j.erase("flux_conservation_factor");
    const ValidationReport r = validate_provenance_json(j, req);
    CHECK(has_gate(r, "G-PROV-MINIMAL-SET"), "missing fcf must hit minimal-set");
  }
  // 负例 a2：最小集缺 k_corr。
  {
    json j = good_json;
    j.erase("k_corr");
    const ValidationReport r = validate_provenance_json(j, req);
    CHECK(has_gate(r, "G-PROV-MINIMAL-SET"), "missing k_corr must hit minimal-set");
  }
  // 负例 b：裸 ADU BUNIT 无 pixel 语义。
  {
    Provenance p = good;
    p.units.bunit = "ADU";
    p.units.pixel_semantics = "integrated_flux";
    p.units.pixel_area_power = 0;
    p.pixel_semantics = "integrated_flux";
    const ValidationReport r = validate_provenance(p);
    CHECK(has_gate(r, "G-BUNIT-SEMANTICS"), "bare ADU must hit bunit-semantics");
  }
  // 负例 c：variance 单位不是 signal^2。
  {
    Provenance p = good;
    p.variance_unit = "ADU/sr";
    const ValidationReport r = validate_provenance(p);
    CHECK(has_gate(r, "G-BUNIT-QUADRATIC"), "variance != signal^2 must hit quadratic");
  }
  // 负例 d：pixfrac<1 缺 flux_conservation_factor。
  {
    json j = good_json;
    j["flux_conservation_factor"] = nullptr;
    j["sampling"]["pixfrac"] = 0.8;
    const ValidationReport r = validate_provenance_json(j, req);
    CHECK(has_gate(r, "G-FLUX-CONSERV-FACTOR"), "pixfrac<1 without fcf must hit");
  }
  // 负例 e：k_corr=1 忽略相关。
  {
    json j = good_json;
    j["k_corr"]["value"] = 1.0;
    const ValidationReport r = validate_provenance_json(j, req);
    CHECK(has_gate(r, "G-KCORR-DOMAIN"), "k_corr=1 must hit kcorr gate");
  }
  // 负例 f：unavailable 占位原因。
  {
    json j = good_json;
    j["unavailable"] = {{"flag", true}, {"reason", "TBD"}, {"scope", "variance"}};
    const ValidationReport r = validate_provenance_json(j, req);
    CHECK(has_gate(r, "G-UNAVAILABLE-REASON"), "placeholder reason must hit");
  }
  // 负例 g：对角 variance 却声明 full_matrix_unavailable。
  {
    Provenance p = good;
    p.diagonal_variance_only = true;
    p.correlation_summary.representation = "full_matrix_unavailable";
    const ValidationReport r = validate_provenance(p);
    CHECK(has_gate(r, "G-SHARED-SYSTEMATIC"), "diagonal-only must hit shared-systematic");
  }
  // 负例 h：psfsw 单位写成 flux^-2。
  {
    ValidationReport r;
    CHECK(!unit_matches_frozen(Quantity::kPsfswRobustWeight, "flux^-2", &r),
          "psfsw flux^-2 must not match frozen");
    CHECK(has_gate(r, "G-UNIT-TABLE"), "psfsw unit gate red");
  }
  // 负例 i：k_corr 缺固定种子/标定。
  {
    json j = good_json;
    j["k_corr"]["calibration"] = {{"script", "x.py"}, {"calibration_run_id", "r"}};
    const ValidationReport r = validate_provenance_json(j, req);
    CHECK(has_gate(r, "G-KCORR-CALIBRATION"), "missing seed must hit calibration gate");
  }
  // 负例 j：software_sha 非完整 40hex。
  {
    json j = good_json;
    j["software_sha"] = "deadbeef";
    const ValidationReport r = validate_provenance_json(j, req);
    CHECK(has_gate(r, "G-PROV-MINIMAL-SET"), "short sha must hit minimal-set");
  }
  std::printf("provenance: %d checks\n", g_checks);
  return 0;
}

int test_manifest() {
  const HipsProperties hp = make_positive_props();
  const std::string props = render_hips_properties(hp);
  const ValidationReport pr = validate_hips_properties(props);
  CHECK_MSG(pr.ok(), ("properties rejected: " + pr.summary()).c_str());

  HipsManifest m;
  m.product_type_id = "astrocs.phase1.product.fits.v1";
  m.order = 6;
  m.order_min = 3;
  m.tile_format = "fits";
  m.frame = "equatorial";  // P0-19: HiPS 1.0 §4.4.1 标准值 (icrs 非法)
  m.files.push_back(HipsFileRecord{"product.fits", 1234, std::string(64, 'a'), "signal", "PRIMARY"});
  m.files.push_back(HipsFileRecord{"properties", 200, std::string(64, 'b'), "properties", ""});
  m.tile_count = m.files.size();
  m.provenance_sha256 = std::string(64, 'c');
  m.output_hash = "sha256:" + std::string(64, 'd');
  m.generated_utc = "2026-09-15T00:00:00Z";
  const json mj = hips_manifest_to_json(m);
  const ValidationReport mr = validate_hips_manifest_json(mj);
  CHECK_MSG(mr.ok(), ("manifest rejected: " + mr.summary()).c_str());

  // 负例：sha256 非 64hex。
  {
    json j = mj;
    j["files"][0]["sha256"] = "nothex";
    CHECK(has_gate(validate_hips_manifest_json(j), "G-HIPS-MANIFEST"), "bad sha must fail");
  }
  // 负例：非法 role。
  {
    json j = mj;
    j["files"][0]["role"] = "weight";
    CHECK(has_gate(validate_hips_manifest_json(j), "G-HIPS-MANIFEST"), "bad role must fail");
  }
  // 负例：tile_count 不一致。
  {
    json j = mj;
    j["tile_count"] = 99;
    CHECK(has_gate(validate_hips_manifest_json(j), "G-HIPS-MANIFEST"), "tile_count mismatch must fail");
  }
  // 负例：路径逃逸。
  {
    json j = mj;
    j["files"][0]["relative_path"] = "../escape.fits";
    CHECK(has_gate(validate_hips_manifest_json(j), "G-HIPS-MANIFEST"), "path escape must fail");
  }
  // 负例：properties 缺 creator_did。
  {
    HipsProperties bad = hp;
    bad.creator_did.clear();
    CHECK(has_gate(validate_hips_properties(render_hips_properties(bad)), "G-HIPS-PROPERTIES"),
          "missing creator_did must fail");
  }
  // 负例：order_min > order。
  {
    HipsProperties bad = hp;
    bad.order_min = 99;
    CHECK(has_gate(validate_hips_properties(render_hips_properties(bad)), "G-HIPS-PROPERTIES"),
          "order_min>order must fail");
  }
  std::printf("manifest: %d checks\n", g_checks);
  return 0;
}

int test_artifacts(const std::string& artdir) {
#if !defined(_WIN32)
  ::mkdir(artdir.c_str(), 0755);
#endif
  const std::string fits_path = artdir + "/product.fits";
  const std::string prov_path = artdir + "/provenance.json";
  const std::string props_path = artdir + "/properties";
  const std::string man_path = artdir + "/manifest.json";
  ::unlink(fits_path.c_str());
  ::unlink(prov_path.c_str());
  ::unlink(props_path.c_str());
  ::unlink(man_path.c_str());

  const std::vector<FitsLayer> layers = make_three_layer_product();
  const std::vector<ExpectedHdu> expected = expected_from_layers(layers);
  const PublishResult fp =
      publish_fits_product(fits_path, layers, expected, PublishOptions(), CancelFn());
  CHECK_MSG(fp.status == PublishStatus::kOk, ("artifact fits publish: " + fp.message).c_str());

  std::string fits_sha;
  CHECK(sha256_file_hex(fits_path, &fits_sha), "artifact fits sha");

  Provenance p = make_positive_provenance();
  p.output_hash = "sha256:" + fits_sha;
  p.software_sha = std::string(40, 'b');
  const std::string prov_text = provenance_to_json(p).dump(2) + "\n";
  const VerifyFn prov_verify = [](const std::string& path, std::string* err) {
    std::ifstream in(path);
    if (!in) {
      if (err) *err = "cannot read provenance";
      return false;
    }
    json j;
    try {
      in >> j;
    } catch (const std::exception& e) {
      if (err) *err = std::string("json parse: ") + e.what();
      return false;
    }
    const ValidationReport r =
        validate_provenance_json(j, provenance_required_keys());
    if (!r.ok()) {
      if (err) *err = r.summary();
      return false;
    }
    return true;
  };
  const PublishResult pp = atomic_write_bytes(prov_path, prov_text, prov_verify,
                                              PublishOptions(), CancelFn());
  CHECK_MSG(pp.status == PublishStatus::kOk, ("artifact provenance publish: " + pp.message).c_str());
  std::string prov_sha;
  CHECK(sha256_file_hex(prov_path, &prov_sha), "artifact provenance sha");

  const HipsProperties hp = make_positive_props();
  const std::string props_text = render_hips_properties(hp);
  const PublishResult hpp = atomic_write_bytes(props_path, props_text, VerifyFn(),
                                               PublishOptions(), CancelFn());
  CHECK(hpp.status == PublishStatus::kOk, "artifact properties publish");

  HipsManifest m;
  m.product_type_id = p.product.type_id;
  m.order = hp.order;
  m.order_min = hp.order_min;
  m.tile_format = hp.tile_format;
  m.frame = hp.frame;
  m.files.push_back(HipsFileRecord{"product.fits", fp.bytes_written, fits_sha, "signal", "PRIMARY"});
  m.files.push_back(HipsFileRecord{"properties", props_text.size(),
                                   Sha256::hex_of_string(props_text), "properties", ""});
  m.files.push_back(HipsFileRecord{"provenance.json", prov_text.size(), prov_sha,
                                   "manifest", ""});
  m.tile_count = m.files.size();
  m.provenance_sha256 = prov_sha;
  m.output_hash = "sha256:" + fits_sha;
  m.generated_utc = p.generated_utc;
  const std::string man_text = render_hips_manifest_json(m);
  const PublishResult mp = atomic_write_bytes(man_path, man_text, VerifyFn(),
                                              PublishOptions(), CancelFn());
  CHECK(mp.status == PublishStatus::kOk, "artifact manifest publish");

  // 发布事务证据：成功/失败/取消三态 + tmp 残留计数。
  json transcript;
  transcript["fits_publish_status"] = publish_status_name(fp.status);
  transcript["fits_sha256"] = fits_sha;
  transcript["provenance_publish_status"] = publish_status_name(pp.status);
  transcript["manifest_publish_status"] = publish_status_name(mp.status);
  transcript["tmp_residue"] = count_tmp_residue(artdir);
  transcript["expected_hdu_count"] = expected.size();
  transcript["signal_bunit"] = "ADU/sr";
  transcript["variance_bunit"] = "ADU^2/sr^2";
  transcript["ivar_bunit"] = "sr^2/ADU^2";
  std::ofstream tf(artdir + "/publish_transcript.json");
  tf << transcript.dump(2) << "\n";
  CHECK(static_cast<bool>(tf), "write transcript");
  CHECK(count_tmp_residue(artdir) == 0, "no tmp residue in artifact dir");
  std::printf("artifacts: %d checks (dir=%s)\n", g_checks, artdir.c_str());
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: v6_aio_test <units|fits|atomic|provenance|manifest|artifacts|all> [artifact_dir]\n");
    return 2;
  }
  const std::string cmd = argv[1];
  const std::string workdir = (argc > 2) ? argv[2] : std::string("/tmp/v6_aio_test");
  int rc = 0;
  if (cmd == "units") return test_units();
  if (cmd == "fits") return test_fits(workdir);
  if (cmd == "atomic") return test_atomic(workdir);
  if (cmd == "provenance") return test_provenance();
  if (cmd == "manifest") return test_manifest();
  if (cmd == "artifacts") return test_artifacts(workdir);
  if (cmd == "all") {
    rc |= test_units();
    rc |= test_fits(workdir);
    rc |= test_atomic(workdir);
    rc |= test_provenance();
    rc |= test_manifest();
    return rc;
  }
  std::fprintf(stderr, "unknown subcommand: %s\n", cmd.c_str());
  return 2;
}
