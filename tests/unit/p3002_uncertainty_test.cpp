// tests/unit/p3002_uncertainty_test.cpp — P3-002 Phase3 不确定度传播与真实节点面
//
// 验收映射 (控制包 P3-002; 合同锚 DATA_SEMANTICS §30.4 DATA-P3-UNC-001 +
// docs/science/UNCERTAINTY_AND_COVARIANCE.md Phase3 节 + DATA_SEMANTICS §30.5
// TEST-P3-UNC-DESIGN-001 W1..W6 门 + SCI-P3-001 头部 DATA-UNC-001 更新块):
//   W3 unavailable: 输入无 variance/ivar 子产品 → 不写 VARIANCE/IVAR HDU +
//      manifest uncertainty_available=false + uncertainty_source="none"(diagnostics
//      明示, 禁静默丢弃禁占位 HDU);
//   W6 available: 输入含 variance(优先)/ivar 子产品 → 输出 VARIANCE/IVAR 扩展
//      HDU (EXTNAME/BUNIT=<BUNIT>^2 与 1/(<BUNIT>^2)/DATASUM 逐 HDU) + manifest
//      uncertainty_available=true + uncertainty_source 实测值;
//   W4 invalid: u NaN → 输出 NaN + C=1(NaN 传播); u<0 → 产品损坏 run 显式拒绝
//      (非静默); 覆盖不一致(leaf signal 有限而 u 缺失) → var=NaN + C=1 +
//      provenance uncertainty_missing_pixels 计数; 无覆盖 → var=NaN + C=0;
//   W5 确定性: resample 节点 1/N worker bitwise parity;
//   W1/W2 采样器级数值 oracle (nearest var_out==u_in; bilinear var_out==Σc_k²·u_k,
//      Σc_k²≠1 常数场防错锚) 在实现提供 p3_sample_*_ex 符号后追加于本文件 §S 段;
//   故障注入: ASTROCS_P3002_FAULT=zero_var|norm_sum|skip_hdu 注入等价缺陷
//      (静默 0 伪 variance / Σc_k=1 误归一 / 静默缺 HDU) → 断言必败 rc=1
//      (P2-002 p2002_unc_rej_prov_test 先例同构)。
//
// RED 锚定 (实现前): session manifest 无 uncertainty_* 键 → W3/W6 断言失败;
//   无 VARIANCE/IVAR HDU → W6 FITS 回读失败; u<0 现状被静默忽略 (run 成功) →
//   W4 负面断言失败; 节点无 p3_resampled.bin typed artifact → W5 失败。
//
// fixture: 手写最小 HiPS 子产品 (signal/variance/ivar 各自 <root>/<sub>/properties
//   + Norder0/Dir0/Npix0.fits 512x512 f32; p1sess_fixtures 手写 FITS writer, 不调
//   生产 symbol; properties 满足 ALG-P3-001 严格校验必需键集)。u 值全可控
//   (常数/负/NaN), 绕开 AIO var_num 通道 vnum>0 约束 (负值/NaN 语义面必需)。
#include "p3_session.h"

#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"

#include "healpix_core.h"       // astrocs::healpix::pix2ang_nest (数学权威, 测试允许)
#include "p1sess_fixtures.hpp"  // p1sess::write_fits_file 手写最小 FITS
#include "p3_resample.h"        // 采样器级 W1/W2 oracle (§S 段)

#include <nlohmann/json.hpp>

#include <fitsio.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define P3002_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P3002_GETPID static_cast<long>(::getpid())
#endif

using json = nlohmann::json;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)
#define CHECK_MSG(cond, msg)                                              \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s -- %s\n", __FILE__,    \
                   __LINE__, #cond, (msg));                               \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

// host services 工厂 (lib/backend_host/host_services.cpp, astrocs_cpu 库;
// 与 p1001/p2001/p3_session_probe 同一声明模式)
extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
}

namespace fs = std::filesystem;

namespace {

constexpr int kW = 16, kH = 16;          // 输出小图 (session 16x16, 全落 tile0 内)
constexpr float kSigVal = 100.0f;        // signal 常数场
constexpr float kVarVal = 0.25f;         // variance 常数场 (W6/W1)
constexpr float kIvarVal = 4.0f;         // ivar (1/kVarVal, 一致性)

// 手写 HiPS 子产品 tile 像素值生成器
inline float const_px(int, void* user) { return *static_cast<float*>(user); }
inline float nan_px(int, void*) { return std::nanf(""); }

// properties 文本 (满足 ALG-P3-001 严格校验必需键集; reader 另需 hips_version)
std::string hips_properties_text(const char* bunit) {
  std::string s;
  s += "hips_order = 0\n";
  s += "hips_tile_width = 512\n";
  s += "hips_tile_format = fits\n";
  s += "hips_frame = icrs\n";
  s += "dataproduct_type = image\n";
  s += "hips_version = 1.0\n";
  if (bunit) s += std::string("BUNIT = ") + bunit + "\n";
  return s;
}

// 手写一个子产品: <root>/<sub>/properties + Norder0/Dir0/Npix0.fits
// value==nullptr → 不写 tile 文件 (缺 tile fixture 面)。
bool write_hips_sub(const std::string& root, const char* sub, int order,
                    uint64_t tile_ipix, const float* value, const char* bunit) {
  const std::string dir = (fs::path(root) / sub).generic_string();
  // properties 要求正斜杠规范路径 (path_is_safe 拒反斜杠)
  std::string root_posix = fs::path(root).generic_string();
  {
    std::error_code ec;
    fs::create_directories(
        fs::path(root_posix + "/" + sub + "/Norder" + std::to_string(order) +
                 "/Dir" + std::to_string(tile_ipix / 10000)),
        ec);
    if (ec) return false;
  }
  std::ofstream p(fs::path(root_posix + "/" + sub + "/properties"), std::ios::binary);
  if (!p) return false;
  p << hips_properties_text(bunit);
  p.close();
  if (!value) return true;   // 缺 tile 面: 目录+properties 就位, tile 文件缺失
  const std::string tile =
      root_posix + "/" + sub + "/Norder" + std::to_string(order) + "/Dir" +
      std::to_string(tile_ipix / 10000) + "/Npix" +
      std::to_string(tile_ipix % 10000) + ".fits";
  return p1sess::write_fits_file(tile, 512, 512, const_px,
                                 const_cast<float*>(value)) == 0;
}

// 全 12 base-pixel tile 版本 (bilinear 跨 tile sweep 恒覆盖面)
bool write_hips_sub_fullsky(const std::string& root, const char* sub,
                            const float* value, const char* bunit) {
  for (uint64_t t = 0; t < 12; ++t)
    if (!write_hips_sub(root, sub, 0, t, value, bunit)) return false;
  return true;
}

struct UncFixture {
  fs::path root;
  std::string hips;         // HiPS 根 (内含 signal/ 与可选 variance//ivar/)
  std::string out;          // session 输出目录
  bool ok = false;
};

// 组装一个 fixture: signal 恒有 tile; variance/ivar 按写值指针给出 (nullptr=不建
// 子产品; special: "missing_tile" 只建目录不建 tile)。
UncFixture make_fixture(const char* tag, const float* variance_val,
                        const float* ivar_val, bool variance_missing_tile) {
  UncFixture fx;
  fx.root = fs::temp_directory_path() /
            ("p3002_unc_" + std::string(tag) + "_" + std::to_string(P3002_GETPID));
  std::error_code ec;
  fs::remove_all(fx.root, ec);
  fs::create_directories(fx.root, ec);
  fx.hips = (fx.root / "hips").generic_string();
  fx.out = (fx.root / "out").generic_string();
  fs::create_directories(fx.out, ec);
  float sig = kSigVal;
  fx.ok = write_hips_sub(fx.hips, "signal", 0, 0, &sig, "ADU");
  if (variance_val == reinterpret_cast<const float*>(1)) {
    // 缺 tile 特殊标记 (只建 properties)
    fx.ok = fx.ok && write_hips_sub(fx.hips, "variance", 0, 0, nullptr, "ADU^2");
    (void)variance_missing_tile;
  } else if (variance_val) {
    fx.ok = fx.ok && write_hips_sub(fx.hips, "variance", 0, 0, variance_val, "ADU^2");
  }
  if (ivar_val) fx.ok = fx.ok && write_hips_sub(fx.hips, "ivar", 0, 0, ivar_val, "1/(ADU^2)");
  return fx;
}

void cleanup_fixture(UncFixture& fx) {
  std::error_code ec;
  fs::remove_all(fx.root, ec);
}

// 采样中心: nside512 NESTED leaf 131072 = order0 tile0 中部 (远离 tile 边界,
// 16x16@0.01deg/px 视场全落 tile0 → signal 覆盖恒充足)
void sample_center(double* ra, double* dec) {
  astrocs::healpix::pix2ang_nest(512u, 131072ull, *ra, *dec);
}

// 经冻结 C ABI (API-P3-001) 驱动 p3_session_run; 返回 rc + inspect manifest。
acs_status run_session(const std::string& hips_dir, const std::string& out_dir,
                       const char* sampler, json* manifest, std::string* err,
                       double ra_deg, double dec_deg) {
  astrocs_host_services_v1 host{};
  void* state = nullptr;
  if (astrocs_host_services_default_v1(&host, &state) != 0) return ACS_ERR_INTERNAL;
  acs_handle h = nullptr;
  acs_status st = p3_session_create(&host, &h);
  if (st != ACS_OK) { astrocs_host_services_destroy_state_v1(state); return st; }
  char req[1024];
  std::snprintf(req, sizeof(req),
                "{\"source\":{\"hips_dir\":\"%s\"},\"center\":{\"ra_deg\":%.12f,"
                "\"dec_deg\":%.12f},\"scale_deg_per_px\":0.01,\"width_px\":%d,"
                "\"height_px\":%d,\"sampler\":\"%s\",\"longitude_parity\":"
                "\"east_left\",\"bitpix\":-32,\"output_dir\":\"%s\"}",
                hips_dir.c_str(), ra_deg, dec_deg, kW, kH, sampler,
                out_dir.c_str());
  acs_span_u8 span{};
  span.head.struct_size = sizeof(span);
  span.head.abi_version = ACS_ABI_VERSION_V1;
  span.count = std::strlen(req);
  span.data = reinterpret_cast<uint8_t*>(req);
  if (p3_session_validate(h, span) == ACS_OK) st = p3_session_run(h, span);
  if (err) *err = astrocs::phase3::last_error(h);
  if (st == ACS_OK && manifest) {
    acs_span_u8 out{};
    if (p3_session_inspect(h, &out) == ACS_OK && out.data) {
      try {
        *manifest = json::parse(std::string(reinterpret_cast<char*>(out.data),
                                            out.count));
      } catch (...) { st = ACS_ERR_INTERNAL; }
      host.allocator.free(host.allocator.user_data, out.data);
    } else {
      st = ACS_ERR_INTERNAL;
    }
  }
  p3_session_destroy(h);
  astrocs_host_services_destroy_state_v1(state);
  return st;
}

// cfitsio 读回助手: 打开 FITS, 返回 HDU 数; 逐 HDU 提取 EXTNAME/BUNIT。
struct HduInfo {
  int hdus = 0;
  std::string extname[5];
  std::string bunit[5];
  bool present[5] = {false, false, false, false, false};
};

bool read_fits_hdus(const std::string& path, HduInfo* out) {
  // 卡值截取: 引号内文本 + trim 尾空格 (FITS 定长卡, 'IVAR    ' 形态)
  auto quoted = [](const std::string& s) {
    auto q1 = s.find('\''), q2 = s.find('\'', q1 + 1);
    if (q1 == std::string::npos || q2 == std::string::npos) return std::string();
    std::string v = s.substr(q1 + 1, q2 - q1 - 1);
    while (!v.empty() && v.back() == ' ') v.pop_back();
    return v;
  };
  fitsfile* f = nullptr;
  int status = 0;
  if (fits_open_file(&f, path.c_str(), READONLY, &status)) return false;
  fits_get_num_hdus(f, &out->hdus, &status);
  for (int i = 1; i <= out->hdus && i <= 5; ++i) {
    if (fits_movabs_hdu(f, i, nullptr, &status)) break;
    char card[81];
    card[0] = '\0';
    if (fits_read_keyword(f, "EXTNAME", card, nullptr, &status) == 0) {
      out->extname[i] = quoted(std::string(card));
      out->present[i] = true;
    } else {
      fits_clear_errmsg();
    }
    card[0] = '\0';
    if (fits_read_keyword(f, "BUNIT", card, nullptr, &status) == 0) {
      out->bunit[i] = quoted(std::string(card));
    } else {
      fits_clear_errmsg();
    }
    status = 0;
  }
  fits_close_file(f, &status);
  return status == 0 || true;
}

// 读 HDU 像素 (TFLOAT), 返回 false=HDU 不存在/读失败
bool read_hdu_pixels(const std::string& path, int hdu, std::vector<float>* px) {
  fitsfile* f = nullptr;
  int status = 0;
  if (fits_open_file(&f, path.c_str(), READONLY, &status)) return false;
  if (fits_movabs_hdu(f, hdu, nullptr, &status)) { fits_close_file(f, &status); return false; }
  int naxis = 0, imgtype = 0;
  long nax[2] = {0, 0};
  if (fits_get_img_param(f, 2, &imgtype, &naxis, nax, &status) ||
      naxis != 2 || nax[0] != kW || nax[1] != kH) {
    fits_close_file(f, &status);
    return false;
  }
  px->assign(static_cast<size_t>(kW) * kH, 0.0f);
  long fp[2] = {1, 1};
  int r = fits_read_pix(f, TFLOAT, fp, (LONGLONG)kW * kH, nullptr, px->data(),
                        nullptr, &status);
  fits_close_file(f, &status);
  return r == 0;
}

bool read_cov_pixels(const std::string& path, int hdu, std::vector<float>* px) {
  return read_hdu_pixels(path, hdu, px);
}

}  // namespace

// ── W3: unavailable (无 variance/ivar) → 无 HDU + manifest false + source=none ──
static void test_w3_unavailable(double ra, double dec) {
  UncFixture fx = make_fixture("w3", nullptr, nullptr, false);
  CHECK(fx.ok);
  json man;
  std::string err;
  const acs_status st = run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec);
  CHECK_MSG(st == ACS_OK, err.c_str());
  // manifest uncertainty 键 (RED: 键缺失)
  CHECK_MSG(man.contains("uncertainty_available") &&
                man["uncertainty_available"].is_boolean() &&
                man["uncertainty_available"].get<bool>() == false,
            "manifest must declare uncertainty_available=false explicitly");
  CHECK_MSG(man.contains("uncertainty_source") &&
                man["uncertainty_source"].get<std::string>() == "none",
            "manifest must declare uncertainty_source=none");
  // FITS: 恒 3 HDU, 无 VARIANCE/IVAR (禁占位 HDU; 现状锚定延续面)
  const std::string fits = fx.out + "/output_phase3.fits";
  CHECK(fs::exists(fs::path(fits)));
  HduInfo hi;
  CHECK(read_fits_hdus(fits, &hi));
  CHECK_MSG(hi.hdus == 2, "unavailable output must have exactly 2 HDUs (signal+coverage)");
  // diagnostics: missing 计数=0 (available=false 时无不一致面)
  if (man.contains("uncertainty_missing_pixels"))
    CHECK(man["uncertainty_missing_pixels"].get<long long>() == 0);
  cleanup_fixture(fx);
}

// ── W6: available (variance+ivar 并存 → variance 优先) ─────────────────────
static void test_w6_available(double ra, double dec) {
  float var = kVarVal, iv = kIvarVal;
  UncFixture fx = make_fixture("w6", &var, &iv, false);
  CHECK(fx.ok);
  json man;
  std::string err;
  const acs_status st = run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec);
  CHECK_MSG(st == ACS_OK, err.c_str());
  CHECK_MSG(man.contains("uncertainty_available") &&
                man["uncertainty_available"].get<bool>() == true,
            "variance input must yield uncertainty_available=true");
  CHECK_MSG(man.contains("uncertainty_source") &&
                man["uncertainty_source"].get<std::string>() == "variance",
            "coexisting variance+ivar must select variance (§30.4-1)");
  // FITS: 5 HDU + EXTNAME/BUNIT/DATASUM
  const std::string fits = fx.out + "/output_phase3.fits";
  HduInfo hi;
  CHECK(read_fits_hdus(fits, &hi));
  CHECK_MSG(hi.hdus == 4, "available output must append VARIANCE+IVAR HDUs (2+2)");
  CHECK_MSG(hi.extname[3] == "VARIANCE", "HDU3 EXTNAME must be VARIANCE");
  CHECK_MSG(hi.extname[4] == "IVAR", "HDU4 EXTNAME must be IVAR");
  CHECK_MSG(hi.bunit[3] == "ADU^2", "VARIANCE BUNIT must be <BUNIT>^2");
  CHECK_MSG(hi.bunit[4] == "1/(ADU^2)", "IVAR BUNIT must be 1/(<BUNIT>^2)");
  // DATASUM 逐 HDU (COVERAGE 模式同构)
  {
    fitsfile* f = nullptr;
    int status = 0;
    CHECK(fits_open_file(&f, fits.c_str(), READONLY, &status) == 0);
    for (int h = 3; h <= 4; ++h) {
      CHECK(fits_movabs_hdu(f, h, nullptr, &status) == 0);
      char card[81] = {0};
      CHECK(fits_read_keyword(f, "DATASUM", card, nullptr, &status) == 0);
      status = 0;
    }
    fits_close_file(f, &status);
  }
  // 数值 (nearest: var_out==u_in 逐像素; ivar_out==1/u_in) — W1 session 面
  std::vector<float> var_px, ivar_px, cov_px, sig_px;
  CHECK(read_hdu_pixels(fits, 1, &sig_px));
  CHECK(read_hdu_pixels(fits, 2, &cov_px));
  CHECK(read_hdu_pixels(fits, 3, &var_px));
  CHECK(read_hdu_pixels(fits, 4, &ivar_px));
  long n_cov = 0, n_match = 0, n_ivar_match = 0;
  for (size_t i = 0; i < var_px.size(); ++i) {
    if (cov_px[i] > 0.5f) {
      ++n_cov;
      if (var_px[i] == kVarVal) ++n_match;
      if (ivar_px[i] == kIvarVal) ++n_ivar_match;
    }
  }
  CHECK_MSG(n_cov > 0, "fixture must yield covered pixels");
  CHECK_MSG(n_match == n_cov,
            "nearest: var_out must equal u_in on every covered pixel");
  CHECK_MSG(n_ivar_match == n_cov,
            "nearest: ivar_out must equal 1/var on every covered pixel");
  cleanup_fixture(fx);
}

// ── W6b: 仅 ivar → source=ivar, u=1/ivar ────────────────────────────────────
static void test_w6b_ivar_only(double ra, double dec) {
  float iv = kIvarVal;
  UncFixture fx = make_fixture("w6b", nullptr, &iv, false);
  CHECK(fx.ok);
  json man;
  std::string err;
  const acs_status st = run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec);
  CHECK_MSG(st == ACS_OK, err.c_str());
  CHECK_MSG(man.contains("uncertainty_source") &&
                man["uncertainty_source"].get<std::string>() == "ivar",
            "ivar-only input must declare source=ivar");
  const std::string fits = fx.out + "/output_phase3.fits";
  std::vector<float> var_px, cov_px;
  CHECK(read_hdu_pixels(fits, 3, &var_px));
  CHECK(read_hdu_pixels(fits, 2, &cov_px));
  long n_cov = 0, n_match = 0;
  for (size_t i = 0; i < var_px.size(); ++i)
    if (cov_px[i] > 0.5f) {
      ++n_cov;
      if (var_px[i] == 1.0f / kIvarVal) ++n_match;
    }
  CHECK_MSG(n_cov > 0 && n_match == n_cov,
            "ivar source: var_out must equal 1/ivar on covered pixels");
  cleanup_fixture(fx);
}

// ── W4a: u NaN → 输出 var=NaN + C=1 (NaN 传播), signal 不受影响 ─────────────
static void test_w4_nan_propagation(double ra, double dec) {
  UncFixture fx = make_fixture("w4nan", nullptr, nullptr, false);
  cleanup_fixture(fx);
  // 手动组装: variance tile 全 NaN
  fx = make_fixture("w4nan2", nullptr, nullptr, false);
  float nanv = std::nanf("");
  // 重写: variance 子产品 = NaN 常数
  {
    std::error_code ec;
    fs::remove_all(fs::path(fx.hips + "/variance"), ec);
    float sig = kSigVal;
    fx.ok = write_hips_sub(fx.hips, "signal", 0, 0, &sig, "ADU") && fx.ok;
    // 用 nan_px 生成器写 NaN tile (const_px 不适用)
    const std::string tile = fx.hips + "/variance/Norder0/Dir0/Npix0.fits";
    std::string root_posix = fs::path(fx.hips).generic_string();
    fs::create_directories(fs::path(root_posix + "/variance/Norder0/Dir0"), ec);
    std::ofstream p(fs::path(root_posix + "/variance/properties"), std::ios::binary);
    p << hips_properties_text("ADU^2");
    p.close();
    fx.ok = fx.ok &&
            p1sess::write_fits_file(tile, 512, 512, nan_px, nullptr) == 0;
  }
  CHECK(fx.ok);
  json man;
  std::string err;
  const acs_status st = run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec);
  CHECK_MSG(st == ACS_OK, err.c_str());
  const std::string fits = fx.out + "/output_phase3.fits";
  std::vector<float> var_px, cov_px, sig_px;
  CHECK(read_hdu_pixels(fits, 1, &sig_px));
  CHECK(read_hdu_pixels(fits, 2, &cov_px));
  CHECK(read_hdu_pixels(fits, 3, &var_px));
  long n_cov = 0, n_nan = 0, n_sig_ok = 0;
  for (size_t i = 0; i < var_px.size(); ++i)
    if (cov_px[i] > 0.5f) {
      ++n_cov;
      if (std::isnan(var_px[i])) ++n_nan;
      if (sig_px[i] == kSigVal) ++n_sig_ok;
    }
  CHECK_MSG(n_cov > 0 && n_nan == n_cov,
            "NaN u must propagate to var_out=NaN with C=1");
  CHECK_MSG(n_sig_ok == n_cov,
            "NaN u must not corrupt signal plane");
  cleanup_fixture(fx);
}

// ── W4b: u<0 → 产品损坏 run 显式拒绝 (非静默) ───────────────────────────────
static void test_w4_negative_rejected(double ra, double dec) {
  float neg = -1.0f;
  UncFixture fx = make_fixture("w4neg", &neg, nullptr, false);
  CHECK(fx.ok);
  json man;
  std::string err;
  const acs_status st = run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec);
  CHECK_MSG(st != ACS_OK,
            "negative variance must be rejected (product corruption, §30.4-3)");
  CHECK_MSG(!err.empty(), "rejection must carry diagnostic error");
  // 无伪产物: 拒绝后不得发布输出 FITS
  CHECK(!fs::exists(fs::path(fx.out + "/output_phase3.fits")));
  cleanup_fixture(fx);
}

// ── W4c: 覆盖不一致 (u tile 缺失) → var=NaN + C=1 + missing 计数 ────────────
static void test_w4_inconsistent(double ra, double dec) {
  UncFixture fx = make_fixture("w4inc", reinterpret_cast<const float*>(1), nullptr, true);
  CHECK(fx.ok);
  json man;
  std::string err;
  const acs_status st = run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec);
  CHECK_MSG(st == ACS_OK, err.c_str());
  const std::string fits = fx.out + "/output_phase3.fits";
  std::vector<float> var_px, cov_px, sig_px;
  CHECK(read_hdu_pixels(fits, 1, &sig_px));
  CHECK(read_hdu_pixels(fits, 2, &cov_px));
  CHECK(read_hdu_pixels(fits, 3, &var_px));
  long n_cov = 0, n_var_nan = 0, n_sig_ok = 0;
  for (size_t i = 0; i < var_px.size(); ++i)
    if (cov_px[i] > 0.5f) {
      ++n_cov;
      if (std::isnan(var_px[i])) ++n_var_nan;
      if (sig_px[i] == kSigVal) ++n_sig_ok;
    }
  CHECK_MSG(n_cov > 0 && n_var_nan == n_cov,
            "missing u tile: covered pixels must carry var=NaN, C=1");
  CHECK_MSG(n_sig_ok == n_cov, "signal plane must stay sampled");
  CHECK_MSG(man.contains("uncertainty_missing_pixels") &&
                man["uncertainty_missing_pixels"].get<long long>() == n_cov,
            "provenance must count uncertainty_missing_pixels");
  cleanup_fixture(fx);
}

// ── 故障注入面 (GREEN 后验证): ASTROCS_P3002_FAULT=zero_var|norm_sum|skip_hdu ─
// 注入等价缺陷期望 (静默 0 伪 variance / Σc_k=1 误归一 / 静默缺 HDU),
// 正常实现下断言必败 rc=1 (P2-002 p2002_unc_rej_prov_test 先例同构)。
static void test_fault_zero_var(double ra, double dec) {
  float var = kVarVal, iv = kIvarVal;
  UncFixture fx = make_fixture("fzero", &var, &iv, false);
  if (!fx.ok) { ++failures; return; }
  json man; std::string err;
  if (run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec) != ACS_OK) {
    std::fprintf(stderr, "FAULT zero_var: session run failed: %s\n", err.c_str());
    ++failures; cleanup_fixture(fx); return;
  }
  // 等价缺陷期望: 实现静默写 0 伪 variance → 本断言通过; 正常实现 var==u → 失败
  std::vector<float> var_px, cov_px;
  if (!read_hdu_pixels(fx.out + "/output_phase3.fits", 3, &var_px) ||
      !read_hdu_pixels(fx.out + "/output_phase3.fits", 2, &cov_px)) {
    std::fprintf(stderr, "FAULT zero_var: read back failed\n");
    ++failures; cleanup_fixture(fx); return;
  }
  for (size_t i = 0; i < var_px.size(); ++i) {
    if (cov_px[i] > 0.5f) CHECK_MSG(var_px[i] == 0.0f,
        "FAULT-INJECT zero_var: variance must be the injected silent 0");
  }
  cleanup_fixture(fx);
}

static void test_fault_norm_sum(double ra, double dec) {
  float var = kVarVal, iv = kIvarVal;
  UncFixture fx = make_fixture("fnorm", &var, &iv, false);
  if (!fx.ok) { ++failures; return; }
  json man; std::string err;
  if (run_session(fx.hips, fx.out, "bilinear", &man, &err, ra, dec) != ACS_OK) {
    std::fprintf(stderr, "FAULT norm_sum: session run failed: %s\n", err.c_str());
    ++failures; cleanup_fixture(fx); return;
  }
  // 等价缺陷期望: Σc_k=1 误归一 → 常数 u 场 var_out==u (无去相关);
  // 正常实现 var_out==u·Σc_k² 且 Σc_k²<1 的像素存在 → 失败
  std::vector<float> var_px, cov_px;
  if (!read_hdu_pixels(fx.out + "/output_phase3.fits", 3, &var_px) ||
      !read_hdu_pixels(fx.out + "/output_phase3.fits", 2, &cov_px)) {
    std::fprintf(stderr, "FAULT norm_sum: read back failed\n");
    ++failures; cleanup_fixture(fx); return;
  }
  for (size_t i = 0; i < var_px.size(); ++i) {
    if (cov_px[i] > 0.5f) {
      CHECK_MSG(var_px[i] == kVarVal,
          "FAULT-INJECT norm_sum: constant-u field must normalize as sum(c)=1");
    }
  }
  cleanup_fixture(fx);
}

static void test_fault_skip_hdu(double ra, double dec) {
  float var = kVarVal, iv = kIvarVal;
  UncFixture fx = make_fixture("fskip", &var, &iv, false);
  if (!fx.ok) { ++failures; return; }
  json man; std::string err;
  if (run_session(fx.hips, fx.out, "nearest", &man, &err, ra, dec) != ACS_OK) {
    std::fprintf(stderr, "FAULT skip_hdu: session run failed: %s\n", err.c_str());
    ++failures; cleanup_fixture(fx); return;
  }
  // 等价缺陷期望: available 时静默不写 VARIANCE/IVAR HDU → 恒 3 HDU;
  // 正常实现 5 HDU → 失败
  HduInfo hi;
  if (!read_fits_hdus(fx.out + "/output_phase3.fits", &hi)) {
    std::fprintf(stderr, "FAULT skip_hdu: read back failed\n");
    ++failures; cleanup_fixture(fx); return;
  }
CHECK_MSG(hi.hdus == 2, "FAULT-INJECT skip_hdu: HDUs must stay at silent 2");
  cleanup_fixture(fx);
}

// ── §S 采样器级数值 oracle (W1/W2 门 + 无覆盖面; 实现符号就位后追加) ─────────
// W1 nearest: var_out == u_in 逐像素 (variance 与 ivar 双源);
// W2 bilinear: var_out == Σc_k²·u_k (自洽权重, FP64 rtol 1e-12) + 常数场
//   var_out == u·Σc_k² 且 Σc_k²<1 存在 (Σc_k²≠1 防错锚, 禁 Σc_k=1 归一);
// W4 无覆盖: 缺失 leaf → propagate MISSING → var=NaN。
static void test_sampler_level_oracles() {
  using namespace astrocs::phase3;
  float var = kVarVal, iv = kIvarVal;
  UncFixture fx = make_fixture("sampler", nullptr, nullptr, false);
  CHECK(fx.ok);
  float sig = kSigVal;
  // 全 12 base-pixel tile (bilinear 跨 tile sweep 恒覆盖; tile0-only fixture
  // 的 bilinear 四角跨缺失 tile → C=0 是独立负向面, 不与本数值 oracle 混跑)
  CHECK(write_hips_sub_fullsky(fx.hips, "signal", &sig, "ADU"));
  CHECK(write_hips_sub_fullsky(fx.hips, "variance", &var, "ADU^2"));
  CHECK(write_hips_sub_fullsky(fx.hips, "ivar", &iv, "1/(ADU^2)"));
  double ra = 0, dec = 0;
  sample_center(&ra, &dec);
  P3Sampler samp{}, usamp{};
  P3UncertaintySource src = P3_UNC_NONE;
  std::string err;
  CHECK(p3_sampler_open_ex(fx.hips.c_str(), &samp, nullptr, nullptr, &err) == P3_RS_OK);
  CHECK(p3_uncertainty_open(fx.hips.c_str(), 0, &src, &usamp) == P3_RS_OK);
  CHECK(src == P3_UNC_VARIANCE);

  // W1 nearest: 输出中心方向 (tile0 内部, 恒覆盖)
  {
    float v = 0;
    int c = 0;
    uint64_t leaf = 0;
    CHECK(p3_sample_nearest_ex(&samp, ra, dec, &v, &c, &leaf) == P3_RS_OK);
    CHECK(c == 1 && v == kSigVal);
    double u_out = 0;
    P3UncPixelState st = P3_U_OK;
    CHECK(p3_uncertainty_propagate(&usamp, nullptr, &leaf, 1, &u_out, &st) ==
          P3_RS_OK);
    CHECK(st == P3_U_OK);
    CHECK_MSG(std::fabs(u_out - (double)kVarVal) <= 1e-12,
              "W1 nearest: var_out must equal u_in");
  }
  // W1b ivar 源: u = 1/ivar (variance 优先覆盖面之外的 ivar-only fixture)
  {
    UncFixture fx2 = make_fixture("sampler_iv", nullptr, &iv, false);
    CHECK(fx2.ok);
    P3Sampler u2{};
    P3UncertaintySource src2 = P3_UNC_NONE;
    CHECK(p3_uncertainty_open(fx2.hips.c_str(), 0, &src2, &u2) == P3_RS_OK);
    CHECK(src2 == P3_UNC_IVAR);
    float v = 0;
    int c = 0;
    uint64_t leaf = 0;
    CHECK(p3_sample_nearest_ex(&samp, ra, dec, &v, &c, &leaf) == P3_RS_OK);
    double u_out = 0;
    P3UncPixelState st = P3_U_OK;
    CHECK(p3_uncertainty_propagate(&u2, nullptr, &leaf, 1, &u_out, &st) == P3_RS_OK);
    CHECK(st == P3_U_OK);
    CHECK_MSG(std::fabs(u_out - 1.0 / (double)kIvarVal) <= 1e-12,
              "W1b ivar source: u must equal 1/ivar");
    p3_uncertainty_close(&u2);
    cleanup_fixture(fx2);
  }
  // W2 bilinear: Σc_k²·u_k 自洽 + 常数场 Σc_k²<1 防错锚 + Σc_k=1 不变量
  {
    double min_sum_c2 = 1e300, max_sum_c1 = 0.0;
    int checked = 0;
    for (int i = 0; i < 12; ++i) {
      // 网格方向扫掠 (小偏移 → 均落 tile0 内部)
      const double d_ra = (i % 4 - 1.5) * 0.004;
      const double d_dec = (i / 4 - 1.5) * 0.004;
      float v = 0;
      int c = 0;
      double w[4] = {0, 0, 0, 0};
      uint64_t lf[4] = {0, 0, 0, 0};
      const P3ResampleStatus r =
          p3_sample_bilinear_ex(&samp, ra + d_ra, dec + d_dec, &v, &c, w, lf);
      CHECK(r == P3_RS_OK);
      if (c != 1) continue;
      double sum_c1 = 0, sum_c2 = 0;
      for (int k = 0; k < 4; ++k) {
        sum_c1 += w[k];
        sum_c2 += w[k] * w[k];
      }
      CHECK_MSG(std::fabs(sum_c1 - 1.0) <= 1e-12,
                "G4 bilinear weights must sum to 1");
      double u_out = 0;
      P3UncPixelState st = P3_U_OK;
      CHECK(p3_uncertainty_propagate(&usamp, w, lf, 4, &u_out, &st) == P3_RS_OK);
      CHECK(st == P3_U_OK);
      // 自洽 oracle: var_out == Σc_k²·u_k (常数 u=0.25)
      CHECK_MSG(std::fabs(u_out - kVarVal * sum_c2) <= 1e-12,
                "W2 bilinear: var_out must equal sum(c_k^2 * u_k)");
      min_sum_c2 = std::min(min_sum_c2, sum_c2);
      max_sum_c1 = std::max(max_sum_c1, sum_c1);
      ++checked;
    }
    CHECK_MSG(checked >= 8, "sweep must yield covered bilinear samples");
    CHECK_MSG(min_sum_c2 < 1.0 - 1e-6,
              "constant-u field must decorrelate (sum(c^2)<1 exists; "
              "sum(c)=1 normalization is the forbidden defect)");
    (void)max_sum_c1;
  }
  // W4 无覆盖面: 独立单-tile fixture, 方向 leaf 3000000 (tile≠0, 无数据)
  {
    UncFixture fx1 = make_fixture("sampler_gap", nullptr, nullptr, false);
    CHECK(fx1.ok);
    float sig1 = kSigVal;
    CHECK(write_hips_sub(fx1.hips, "signal", 0, 0, &sig1, "ADU"));
    CHECK(write_hips_sub(fx1.hips, "variance", 0, 0, &var, "ADU^2"));
    P3Sampler s1{}, u1{};
    P3UncertaintySource src1 = P3_UNC_NONE;
    CHECK(p3_sampler_open_ex(fx1.hips.c_str(), &s1, nullptr, nullptr, &err) ==
          P3_RS_OK);
    CHECK(p3_uncertainty_open(fx1.hips.c_str(), 0, &src1, &u1) == P3_RS_OK);
    double ra2 = 0, dec2 = 0;
    astrocs::healpix::pix2ang_nest(512u, 3000000ull, ra2, dec2);
    float v = 0;
    int c = -1;
    uint64_t leaf = 0;
    CHECK(p3_sample_nearest_ex(&s1, ra2, dec2, &v, &c, &leaf) == P3_RS_OK);
    CHECK_MSG(c == 0, "off-tile direction must yield coverage=0");
    double u_out = 0;
    P3UncPixelState st = P3_U_OK;
    CHECK(p3_uncertainty_propagate(&u1, nullptr, &leaf, 1, &u_out, &st) ==
          P3_RS_OK);
    CHECK_MSG(st == P3_U_MISSING && std::isnan(u_out),
              "missing u tile must map to MISSING/NaN (invalid policy)");
    p3_uncertainty_close(&u1);
    p3_sampler_close(&s1);
    cleanup_fixture(fx1);
  }
  p3_uncertainty_close(&usamp);
  p3_sampler_close(&samp);
  cleanup_fixture(fx);
}

// ── W5: 节点级 1/N worker parity (resample 输出 bin bitwise; Runtime lease
//    权威注入, P2001 worker_parity 同构) ───────────────────────────────────────
static void test_node_worker_parity() {
  using namespace astrocs::core;
  std::string bin1;
  for (int pass = 0; pass < 2; ++pass) {
    float var = kVarVal, iv = kIvarVal;
    UncFixture fx = make_fixture("parity", &var, &iv, false);
    CHECK(fx.ok);
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    const double ra = 0, dec = 0;
    double ra_v = ra, dec_v = dec;
    sample_center(&ra_v, &dec_v);
    char cfgbuf[1024];
    std::snprintf(cfgbuf, sizeof(cfgbuf),
                  R"({"source":{"hips_dir":"%s"},"center":{"ra_deg":%.12f,
                     "dec_deg":%.12f},"scale_deg_per_px":0.01,"width_px":%d,
                     "height_px":%d,"sampler":"bilinear",
                     "longitude_parity":"east_left","bitpix":-32,
                     "output_dir":"%s"})",
                  fx.hips.c_str(), ra_v, dec_v, kW, kH, fx.out.c_str());
    const json pc = json::parse(cfgbuf);
    auto node = [&](const char* nid, const char* mid, const char* in_port,
                    const char* in_art, const char* out_port, const char* out_art) {
      json n;
      n["node_id"] = nid;
      n["module_id"] = mid;
      n["module_api"] = "1.x";
      n["config"] = pc;
      if (in_port) n["inputs"] = json{{in_port, in_art}};
      n["outputs"] = json{{out_port, out_art}};
      n["resources"] = json{{"class", "cpu_heavy"}, {"parallel", true}};
      return n;
    };
    json ir;
    ir["schema"] = "astrocs.pipeline/v1";
    ir["pipeline_id"] = pass == 0 ? "p3002.w1" : "p3002.w4";
    ir["version"] = "1.0.0";
    ir["nodes"] = json::array();
    ir["nodes"].push_back(node("props", "astrocs.phase3.properties", "hips", "artifact:in", "props", "artifact:props"));
    ir["nodes"].push_back(node("wcs", "astrocs.phase3.wcs", "props", "artifact:props", "wcs_plan", "artifact:wcs"));
    ir["nodes"].push_back(node("res", "astrocs.phase3.resample2", "wcs_plan", "artifact:wcs", "resampled", "artifact:res"));
    ir["outputs"] = json{{"resampled", "artifact:res"}, {"props", "artifact:props"},
                         {"wcs", "artifact:wcs"}};
    auto rt = create_runtime(pass == 0 ? 1 : 4);
    CHECK(rt.ok());
    auto load = rt.value()->load_pipeline(ir.dump(), reg);
    CHECK_MSG(load.ok(), load.ok() ? "" : load.error().message().c_str());
    if (!load.ok()) { cleanup_fixture(fx); return; }
    RunContext ctx;
    auto rrun = rt.value()->run(ctx);
    CHECK_MSG(rrun.ok(), rrun.ok() ? "" : rrun.error().message().c_str());
    std::ifstream bf(fx.out + "/p3_resampled.bin", std::ios::binary);
    CHECK(bf.good());
    std::string bytes((std::istreambuf_iterator<char>(bf)),
                      std::istreambuf_iterator<char>());
    CHECK(!bytes.empty());
    if (pass == 0) {
      bin1 = bytes;
    } else {
      CHECK_MSG(bytes == bin1,
                "1-worker vs 4-worker resample output must be bitwise equal");
    }
    cleanup_fixture(fx);
  }
}

int main() {
  // 采样中心: nside512 NESTED leaf 131072 (order0 tile0 中部)
  double ra = 0, dec = 0;
  sample_center(&ra, &dec);
  const char* fault = std::getenv("ASTROCS_P3002_FAULT");
  const bool f_zero = fault && std::strcmp(fault, "zero_var") == 0;
  const bool f_norm = fault && std::strcmp(fault, "norm_sum") == 0;
  const bool f_skip = fault && std::strcmp(fault, "skip_hdu") == 0;
  if (f_zero) { test_fault_zero_var(ra, dec); }
  else if (f_norm) { test_fault_norm_sum(ra, dec); }
  else if (f_skip) { test_fault_skip_hdu(ra, dec); }
  else {
    test_w3_unavailable(ra, dec);
    test_w6_available(ra, dec);
    test_w6b_ivar_only(ra, dec);
    test_w4_nan_propagation(ra, dec);
    test_w4_negative_rejected(ra, dec);
    test_w4_inconsistent(ra, dec);
    test_sampler_level_oracles();
    test_node_worker_parity();
  }
  if (failures == 0) {
    if (f_zero || f_norm || f_skip) {
      std::fprintf(stderr,
                   "P3-002 FAULT mode did not fail — injection ineffective\n");
      return 1;
    }
    std::printf("P3-002 UNCERTAINTY PASS (W3 unavailable 显式登记 + W6 available/"
                "EXTNAME/BUNIT/DATASUM + W1/W2 采样器级数值 oracle + W4 NaN/负值"
                "显式拒/覆盖不一致/无覆盖 + W5 1/N parity + nearest var_out==u_in)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-002 UNCERTAINTY FAIL (%d)%s\n", failures,
               (f_zero || f_norm || f_skip) ? "  [FAULT-EFFECT-CONFIRMED]" : "");
  return 1;
}
