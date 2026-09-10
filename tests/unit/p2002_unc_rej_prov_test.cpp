// tests/unit/p2002_unc_rej_prov_test.cpp — P2-002 §30.2 rejection 平面与
// §30.3 provenance 键组（DATA-P2-REJ-001 / DATA-P2-PROV-001 白名单内承载面）
//
// 验收映射（控制包 P2-002, 前台 owner 口径①-⑤, BASE=8545d70a）:
//   1. §30.2 kernel 语义锚（lib/phase2 白名单内库面直调, 正确 gather 契约
//      value_stride=每帧元素跨度）: AUTO n=3 → PERCENTILE(median_center,
//      low 0.2/high 0.1); flat 栈全 ACCEPTED(nrej=0); 单帧离群点
//      REJECTED_HIGH(nrej=1, SCI-REJ §5 reason∉{ACCEPTED,UNDERDETERMINED}
//      计数); n=2 → UNDERDETERMINED 全接受; kernel 双调 bitwise 确定性。
//   2. §30.2 集成投影（现状承载面）: 3 帧链 nrej int32 平面 ==
//      p2_rejection.json nrej bins 逐像素; nused == SCI-INT n_used 投影
//      (覆盖像素=depth、无覆盖角落=0); int 无 NaN → 0 即"无"（禁 −1 哨兵）;
//      dtype 固定 int32（文件字节 == 4·n_pixels）。
//      [finding F-P2-002-01 锚] 生产 reject 的 gather 调用缺陷
//      （lib/core 白名单外）使 depth≥3 的 rejection bins 与 kernel 语义
//      失真且非确定——本测试只锚定"投影==bins"与 kernel 直调语义。修复
//      验证断言已补于 test_f_p2002_01_rejection_parity（§2b）: 生产 bins==
//      kernel 重放逐 bin 一致 + void 角落 nrej==0 + depth≥3 双跑/1v4
//      bitwise 确定性。
//   3. §30.3 provenance 五键真实值对拍 + unavailable 显式登记 + pending
//      诚实（properties 通道缺口不掩盖, finding 移交）。
//   4. F-UNC-003 零断链: 诊断平面不入 science planes 枚举（schema 与
//      runtime validator 只读断言, 零修改）。
//   5. 确定性/1-N worker: 2 帧链（depth=2, kernel 不触发, 与 P2-001 同
//      基线口径）nused/nrej/signal/wsum 平面 bitwise + 1-worker vs
//      4-worker bitwise。
//   6. 故障注入有效性: ASTROCS_P2002_FAULT=proj|prov 注入等价缺陷必败
//      （rc=1）。
//   7. 合同登记面（§30.2 授权"AIO_ALL 掩码扩展由实现任务在 AIO 域合同
//      登记"）: contracts/data/phase2_uncertainty_rejection_provenance_v1.json
//      存在、位值 NREJ=32/NUSED=64 冻结、五键名单、pending AIO 通道登记。
#include "astro/phase2/rejection.h"
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"

#include "p1sess_fixtures.hpp"  // 最小 FITS writer (手写, 不调生产 symbol)
#include "aio_hips.h"
#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define P2002_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P2002_GETPID static_cast<long>(::getpid())
#endif

using json = nlohmann::json;
using namespace astrocs::core;

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

namespace {

namespace fs = std::filesystem;

// ── §30.2 kernel 语义直调 fixture（手工 frame-major 布局, 正确契约）────────
// 3 帧 × 4 像素; value_stride = 每帧元素跨度（stage2.cpp:1086 同契约）。
constexpr std::uint32_t kKerNpx = 4;
constexpr std::uint32_t kKerDepth = 3;

// ── 3 帧带 ivar/variance 的 HiPS fixture（1 tile, order 9, nside 512）──────
constexpr uint32_t kNside = 512;
constexpr uint32_t kTw = 512;
constexpr uint32_t kTileSpan = kTw * kTw;
constexpr double kIvar1 = 2.0, kIvar2 = 0.5, kIvar3 = 1.0;
constexpr float kArea = 1.0e-2f;   // 同 P2-001 fixture 口径（finding F5 登记）
constexpr float kOutlier = 800.0f;
inline bool is_outlier_fitseq(uint32_t x, uint32_t y) {
  return (x % 32u == 16u) && (y % 32u == 16u);
}
inline bool is_void_fitseq(uint32_t x, uint32_t y) {
  return x >= 448u && y >= 448u;
}
inline float base_signal(uint32_t x, uint32_t y) {
  return 100.0f + 0.1f * static_cast<float>((x * 7u + y * 13u) % 5u);
}

bool write_ivar3_frame(const std::string& path, double ivar, float offset,
                       bool with_outliers) {
  AioHipsProductSet* ps = aio_hips_product_begin(
      path.c_str(), kNside, kTw, AIO_HIPS_FLOAT32,
      AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
          AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR,
      "ivo://astrocs/test", "P2-002 unc/rej/prov fixture", "R", 60.0,
      "2026-09-10T00:00:00Z", 0);
  if (!ps) {
    std::fprintf(stderr, "fixture begin failed: %s\n", aio_hips_last_error());
    return false;
  }
  // view 合同 = NESTED local 序（writer 内部 nested_local_to_fits_index 归约;
  // 断言面坐标以 FITS 序为唯一口径, 经 fits_index_to_nested_local 逆映射写）
  std::vector<float> sig(kTileSpan), area(kTileSpan, kArea);
  for (uint32_t y = 0; y < kTw; ++y)
    for (uint32_t x = 0; x < kTw; ++x) {
      const uint32_t fi = y * kTw + x;
      const uint32_t local = static_cast<uint32_t>(
          astrocs::healpix::fits_index_to_nested_local(fi, 9u, kTw));
      float v = base_signal(x, y) + offset;
      if (with_outliers && is_outlier_fitseq(x, y)) v += kOutlier;
      sig[local] = v;
      if (is_void_fitseq(x, y)) area[local] = 0.0f;   // 无覆盖角落 → NaN 面
    }
  const double var_num = (1.0 / ivar) * double(kArea) * double(kArea);
  std::vector<float> vnum(kTileSpan, static_cast<float>(var_num));
  AstroSphereTileView v{};
  v.parent_ipix = 0;
  v.leaf_order = 9;
  v.width = kTw;
  v.data_type = AIO_HIPS_FLOAT32;
  v.flux_sum = sig.data();
  v.covered_area = area.data();
  v.valid_mask = nullptr;
  v.var_num_sum = vnum.data();
  if (aio_hips_write_signal_support_tile(ps, &v) != 0 ||
      aio_hips_write_variance_tile(ps, &v) != 0) {
    std::fprintf(stderr, "fixture tile write failed: %s\n", aio_hips_last_error());
    aio_hips_abort(ps);
    return false;
  }
  if (aio_hips_finalize(ps) != 0) {
    std::fprintf(stderr, "fixture finalize failed: %s\n", aio_hips_last_error());
    return false;
  }
  return true;
}

struct Fixture3 {
  fs::path root;
  std::string hips1, hips2, hips3;
  std::string out;
};

Fixture3 make_fixture3(const char* tag) {
  Fixture3 fx;
  fx.root = fs::temp_directory_path() /
            ("p2002_f3_" + std::string(tag) + "_" + std::to_string(P2002_GETPID));
  std::error_code ec;
  fs::create_directories(fx.root, ec);
  fx.hips1 = (fx.root / "F1.hips").string();
  fx.hips2 = (fx.root / "F2.hips").string();
  fx.hips3 = (fx.root / "F3.hips").string();
  CHECK(write_ivar3_frame(fx.hips1, kIvar1, 0.0f, false));
  // F2 offset 1（原域）→ corrected 域 ~100: UPM 校正残差 ≪ percentile 高阈
  // phigh·|median|≈1002（corrected 域信号 median≈10020）→ 正常像素远离
  // kernel 边界（拒绝面只由 F3 离群点驱动, 非临界混沌）
  CHECK(write_ivar3_frame(fx.hips2, kIvar2, 1.0f, false));
  CHECK(write_ivar3_frame(fx.hips3, kIvar3, 0.0f, true));     // kernel 拒绝面
  fx.out = (fx.root / "p2out").string();
  fs::create_directories(fx.out, ec);
  return fx;
}

// 2 帧确定性/parity fixture（depth=2, kernel 不触发, 与 P2-001 同基线口径）
struct Fixture2 {
  fs::path root;
  std::string hips1, hips2;
  std::string out;
};

Fixture2 make_fixture2(const char* tag) {
  Fixture2 fx;
  fx.root = fs::temp_directory_path() /
            ("p2002_f2_" + std::string(tag) + "_" + std::to_string(P2002_GETPID));
  std::error_code ec;
  fs::create_directories(fx.root, ec);
  fx.hips1 = (fx.root / "F1.hips").string();
  fx.hips2 = (fx.root / "F2.hips").string();
  CHECK(write_ivar3_frame(fx.hips1, kIvar1, 0.0f, false));
  CHECK(write_ivar3_frame(fx.hips2, kIvar2, 1.0f, false));
  fx.out = (fx.root / "p2out").string();
  fs::create_directories(fx.out, ec);
  return fx;
}

std::string chain_cfg(const Fixture3& fx, const std::string& extra = "") {
  return R"({
    "hips_paths": [")" + fx.hips1 + R"(", ")" + fx.hips2 + R"(", ")" +
         fx.hips3 + R"("],
    "output_dir": ")" + fx.out + R"(")" + extra + R"(
  })";
}
std::string chain_cfg2(const Fixture2& fx) {
  return R"({
    "hips_paths": [")" + fx.hips1 + R"(", ")" + fx.hips2 + R"("],
    "output_dir": ")" + fx.out + R"("
  })";
}

std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
  return s;
}

// repo 相对路径（ctest cwd=build → 用编译期 ASTROCS_REPO_ROOT 锚定）
std::string repo_file(const char* rel) {
#ifdef ASTROCS_REPO_ROOT
  {
    const std::string s = read_file(std::string(ASTROCS_REPO_ROOT) + "/" + rel);
    if (!s.empty()) return s;
  }
#endif
  return read_file(rel);
}

json run_node(ModuleRegistry& reg, const std::string& module_id,
              const std::string& config_json, RunContext& ctx,
              Result<void>* rc_out = nullptr) {
  auto m = reg.create(module_id);
  if (m.failed()) {
    CHECK_MSG(false, (module_id + ": create failed").c_str());
    return json();
  }
  auto v = m.value()->validate_config(config_json);
  if (v.failed()) {
    CHECK_MSG(false, (module_id + ": validate failed: " + v.error().message()).c_str());
    return json();
  }
  auto p = m.value()->plan("node_" + module_id, config_json);
  CHECK(p.ok());
  auto r = m.value()->execute(ctx);
  if (rc_out) *rc_out = r;
  auto man = m.value()->last_manifest();
  if (!man.ok()) {
    CHECK_MSG(false, (module_id + ": no manifest").c_str());
    return json();
  }
  json j;
  try {
    j = json::parse(man.value());
  } catch (...) {
    CHECK_MSG(false, (module_id + ": manifest not JSON").c_str());
  }
  return j;
}

// Phase2 全链顺序执行（coverage→…→write）
json run_p2_chain(ModuleRegistry& reg, const std::string& cfg, RunContext& ctx,
                  Result<void>* first_fail = nullptr) {
  static const char* mods[] = {"astrocs.phase2.coverage", "astrocs.phase2.sample",
                               "astrocs.phase2.upm-fit", "astrocs.phase2.upm-apply",
                               "astrocs.phase2.reject", "astrocs.phase2.integrate",
                               "astrocs.phase2.write"};
  json last;
  for (const char* m : mods) {
    Result<void> rc;
    last = run_node(reg, m, cfg, ctx, &rc);
    if (rc.failed()) {
      if (first_fail && !first_fail->failed()) *first_fail = rc;
      return last;
    }
  }
  return last;
}

template <typename T>
bool read_bin(const std::string& path, uint64_t off, uint64_t n,
              std::vector<T>* out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(static_cast<std::streamoff>(off * sizeof(T)));
  out->assign(static_cast<size_t>(n), T{});
  f.read(reinterpret_cast<char*>(out->data()),
         static_cast<std::streamsize>(n * sizeof(T)));
  return f.gcount() == static_cast<std::streamsize>(n * sizeof(T));
}

// FITS 序线性索引（断言面唯一坐标口径）
inline uint32_t fitseq(uint32_t x, uint32_t y) { return y * kTw + x; }

// kernel 直调助手（正确 gather 契约: value_stride = 每帧元素跨度）
struct KernelRun {
  std::vector<std::uint8_t> reasons;
  P2RejectionDecision dec{};
  std::uint32_t eligible_count = 0;
};
bool run_kernel(const double* frame_major, std::uint32_t depth,
                std::uint32_t npx, std::uint32_t pixel, KernelRun* out) {
  P2RejectionPlanRequest req{};
  req.request = P2_REJECT_AUTO;
  req.nominal_contributors = depth;
  req.profile = "wbpp_current";
  req.underdetermined_n = 2;
  P2RejectionPlan plan{};
  char err[256] = {0};
  if (p2_reject_plan_resolve(&req, &plan, err, sizeof(err)) != 0) return false;
  P2EligibilityGatherInput gin{};
  gin.values = frame_major;
  gin.value_stride = npx;              // 每帧元素跨度（frame-major 契约）
  gin.value_dtype = 1;
  gin.count = depth;
  gin.pixel = pixel;
  std::vector<double> compact(depth);
  std::vector<std::uint32_t> src(depth);
  P2EligibilityGatherOutput gout{};
  gout.values = compact.data();
  gout.source_indices = src.data();
  std::uint32_t ec = 0;
  gout.eligible_count = &ec;
  if (p2_collect_candidate_stack(&gin, &gout) != 0) return false;
  out->eligible_count = ec;
  out->reasons.assign(depth, 0);
  out->dec.reasons = out->reasons.data();
  if (ec > 0 && ec > plan.underdetermined_n &&
      ec >= static_cast<std::uint32_t>(plan.minimum_n)) {
    P2CandidateStack st{};
    st.values = compact.data();
    st.count = ec;
    st.data_type = 1;
    if (p2_reject_stack_ex(&st, &plan, &out->dec) != 0) return false;
  }
  return true;
}

}  // namespace

// ── 1. §30.2 kernel 语义直调（lib/phase2 白名单内库面, 正确 gather 契约）───
static void test_s302_kernel_semantics() {
  // plan resolve: AUTO n=3 → PERCENTILE（wbpp_current 路由 n<6）
  P2RejectionPlanRequest req{};
  req.request = P2_REJECT_AUTO;
  req.nominal_contributors = 3;
  req.profile = "wbpp_current";
  req.underdetermined_n = 2;
  P2RejectionPlan plan{};
  char err[256] = {0};
  CHECK(p2_reject_plan_resolve(&req, &plan, err, sizeof(err)) == 0);
  CHECK(plan.method == P2_REJECT_PERCENTILE);
  CHECK(plan.normalization == P2_NORMALIZE_MEDIAN_CENTER);
  CHECK(plan.percentile.low_fraction == 0.2);
  CHECK(plan.percentile.high_fraction == 0.1);
  CHECK(plan.underdetermined_n == 2);
  CHECK(std::string(p2_rejection_semantic_id(plan.method)) ==
        "astrocs.percentile_siril.v1");

  // flat 栈（三帧同值）→ 全 ACCEPTED、nrej=0
  {
    // frame-major: 3 帧 × 4 像素; 像素 0 flat
    const double fm[kKerDepth * kKerNpx] = {100.0, 100.4, 100.2, 100.1,
                                            100.0, 100.3, 100.2, 100.1,
                                            100.0, 100.4, 100.2, 100.1};
    KernelRun k;
    CHECK(run_kernel(fm, kKerDepth, kKerNpx, 0, &k));
    CHECK(k.dec.status == 0);
    CHECK(k.dec.rejected_low + k.dec.rejected_high == 0);
    CHECK(k.reasons[0] == P2_REASON_ACCEPTED);
    CHECK(k.reasons[1] == P2_REASON_ACCEPTED);
    CHECK(k.reasons[2] == P2_REASON_ACCEPTED);
  }
  // 单帧离群点 → REJECTED_HIGH 1 个（SCI-REJ §5 kernel 拒绝计数语义）
  {
    // 像素 1: 帧 2（第三帧, slot s=2）+800（scale=|median|≈100.2 → 高阈≈10 ≪ 800）
    const double fm[kKerDepth * kKerNpx] = {100.0, 100.4, 100.2, 100.1,
                                            100.0, 100.3, 100.2, 100.1,
                                            100.0, 900.4, 100.2, 100.1};
    KernelRun k;
    CHECK(run_kernel(fm, kKerDepth, kKerNpx, 1, &k));
    CHECK_MSG(k.dec.rejected_high == 1 && k.dec.rejected_low == 0,
              "single-frame outlier must yield exactly one REJECTED_HIGH");
    CHECK(k.reasons[2] == P2_REASON_REJECTED_HIGH);
    CHECK(k.reasons[0] == P2_REASON_ACCEPTED);
    CHECK(k.reasons[1] == P2_REASON_ACCEPTED);
    CHECK(k.dec.accepted_count == 2);
  }
  // n=2（eligible ≤ underdetermined_n）→ UNDERDETERMINED 全接受 nrej=0
  {
    // 像素 2: 帧 1 NaN → eligible=2 → 不跑 kernel
    const double fm[kKerDepth * kKerNpx] = {100.0, 100.4, 100.2, 100.1,
                                            100.0, 100.3, 100.2, 100.1,
                                            100.0, std::nan(""), 100.2, 100.1};
    KernelRun k;
    CHECK(run_kernel(fm, kKerDepth, kKerNpx, 2, &k));
    CHECK_MSG(k.dec.rejected_low + k.dec.rejected_high == 0,
              "eligible<=underdetermined_n must not run kernel (nrej=0)");
    CHECK(k.reasons[0] == 0 && k.reasons[1] == 0);   // 未执行（全 0 占位）
  }
  // kernel 确定性: 同栈双调 bitwise（reasons/dec 全等）
  {
    const double fm[kKerDepth * kKerNpx] = {100.0, 100.4, 100.2, 100.1,
                                            100.0, 100.3, 100.2, 100.1,
                                            100.0, 900.4, 100.2, 100.1};
    KernelRun a, b;
    CHECK(run_kernel(fm, kKerDepth, kKerNpx, 1, &a));
    CHECK(run_kernel(fm, kKerDepth, kKerNpx, 1, &b));
    CHECK(std::memcmp(a.reasons.data(), b.reasons.data(), kKerDepth) == 0);
    CHECK(a.dec.status == b.dec.status && a.dec.accepted_count == b.dec.accepted_count &&
          a.dec.rejected_low == b.dec.rejected_low &&
          a.dec.rejected_high == b.dec.rejected_high &&
          a.dec.iterations == b.dec.iterations);
  }
}

// ── 2. §30.2 集成投影: int32 平面 == bins、nused==n_used、角落 0/0 ─────────
static void test_s302_integration_projection(bool fault_inject) {
  Fixture3 fx = make_fixture3("proj");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  Result<void> ff;
  run_p2_chain(reg, chain_cfg(fx), ctx, &ff);
  CHECK_MSG(ff.ok(), ff.ok() ? "chain ok" : ff.error().message().c_str());
  if (ff.failed()) { fs::remove_all(fx.root); return; }

  json intj, rej;
  try { intj = json::parse(read_file(fx.out + "/p2_integrated.json")); }
  catch (...) { CHECK(false); }
  try { rej = json::parse(read_file(fx.out + "/p2_rejection.json")); }
  catch (...) { CHECK(false); }
  CHECK(intj.value("schema", "") == "DATA-P2-INT");
  CHECK(rej.value("schema", "") == "DATA-P2-REJ");
  CHECK(intj.value("n_pixels", 0ull) == kTileSpan);
  // §30.2 dtype 固定 int32 无 precision 开关: 文件字节 == 4·n_pixels
  CHECK(fs::file_size(fs::path(intj["files"].value("nused", ""))) ==
        static_cast<uintmax_t>(kTileSpan) * 4u);
  CHECK(fs::file_size(fs::path(intj["files"].value("nrej", ""))) ==
        static_cast<uintmax_t>(kTileSpan) * 4u);

  std::vector<int32_t> nused, nrej_plane;
  std::vector<uint16_t> nrej_bins;
  CHECK(read_bin<int32_t>(intj["files"].value("nused", ""), 0, kTileSpan, &nused));
  CHECK(read_bin<int32_t>(intj["files"].value("nrej", ""), 0, kTileSpan, &nrej_plane));
  CHECK(read_bin<uint16_t>(rej["files"].value("nrej", ""), 0, kTileSpan, &nrej_bins));

  // 投影对拍: §30.2 int32 nrej 平面 == rejection bins（逐像素恒等）
  // 故障注入面: ASTROCS_P2002_FAULT=proj → 期望 +1（等价缺陷: 投影 ±1 错）
  const int32_t nrej_bias = fault_inject ? 1 : 0;
  for (size_t i = 0; i < nrej_plane.size(); ++i) {
    if (nrej_plane[i] != static_cast<int32_t>(nrej_bins[i]) + nrej_bias) {
      CHECK_MSG(false, ("nrej projection mismatch at pixel " +
                        std::to_string(i)).c_str());
      break;
    }
  }
  // nused 投影 = SCI-INT §5 n_used（现状: 覆盖像素=depth=3、无覆盖=0;
  // acc 全接受面, percentile 结构保证 median 样本恒接受）
  for (uint32_t y = 0; y < 448u; ++y)
    for (uint32_t x = 0; x < 448u; ++x) {
      const uint32_t fi = fitseq(x, y);
      CHECK_MSG(nused[fi] == 3,
                ("covered pixel nused must equal depth (fi=" +
                 std::to_string(fi) + " got " + std::to_string(nused[fi]) +
                 ")").c_str());
      if (nused[fi] != 3) { y = kTw; break; }
    }
  // 无覆盖角落: int 无 NaN → nused=0（0 即"无", 禁 −1 哨兵）。
  // [finding F-P2-002-01 锚] nrej 角落断言（§30.2 invalid: 无覆盖 → nrej=0）
  // 在生产 reject gather 调用缺陷（lib/core 白名单外, 越界读+契约错位）修复
  // 前无法锚定——缺陷使无覆盖像素 nrej bins 非零且非确定, 修复后须补
  // nrej_plane[void]==0 断言。
  for (uint32_t y = 448u; y < kTw; ++y)
    for (uint32_t x = 448u; x < kTw; ++x) {
      const uint32_t fi = fitseq(x, y);
      CHECK_MSG(nused[fi] == 0,
                ("void pixel must have nused==0 (fi=" + std::to_string(fi) +
                 " got " + std::to_string(nused[fi]) + ")").c_str());
    }
  // 平面非负守卫: nused+nrej ∈ [0, 2·depth]（u16 bins ≤ eligible ≤ depth;
  // n_ineligible = depth − nused − nrej 完整恒等式依赖 F-P2-002-01/02
  // 域外缺陷修复——kernel 一致性与 integrate 逐样本剔除）
  for (size_t i = 0; i < nused.size(); ++i) {
    const int32_t s = nused[i] + nrej_plane[i] - nrej_bias;
    if (s < 0 || s > 2 * 3) {
      CHECK_MSG(false, ("plane guard violated at pixel " +
                        std::to_string(i)).c_str());
      break;
    }
  }
  // stats 对拍: nrej_total == Σ nrej_plane == kernel threshold 侧计数和
  const uint64_t nrej_total = intj["diagnostics"].value("nrej_total", 0ull);
  uint64_t sum = 0;
  for (int32_t v : nrej_plane) sum += static_cast<uint64_t>(v - nrej_bias);
  CHECK_MSG(nrej_total == sum,
            ("nrej_total must equal plane sum (json=" +
             std::to_string(nrej_total) + " sum=" + std::to_string(sum) + ")").c_str());
  CHECK(rej["stats"].value("rejected_low", 0ull) + rej["stats"].value("rejected_high", 0ull) ==
        sum);
  // kernel 执行面存在（candidates=3 像素 > 0 → depth≥3 真跑过 kernel）
  {
    std::vector<uint16_t> cand;
    CHECK(read_bin<uint16_t>(rej["files"].value("candidates", ""), 0, kTileSpan, &cand));
    uint64_t n_kern = 0;
    for (uint16_t c : cand) if (c == 3) ++n_kern;
    CHECK_MSG(n_kern > 0, "kernel execution surface must exist (cand==3 pixels)");
  }
  fs::remove_all(fx.root);
}

// ── 2b. [finding F-P2-002-01 修复验证] 生产 reject bins == kernel 语义直调
// 重放一致性 + 无覆盖角落 nrej==0 + depth≥3 nrej 确定性（双跑/1v4 bitwise）。
// 修复前（生产 gather 契约错位: value_stride=sizeof(double)+3 元素 vals 缓冲
// 以 pixel 为基址索引）生产 bins 为垃圾数据产物: 与 kernel 直调逐 bin
// mismatch、角落 nrej 非零、双跑 bitwise 不稳——三组断言即 RED 面。
template <typename Fixture>
static void run_chain_via_runtime(ModuleRegistry& reg, const json& pc,
                                  const char* pipe_id, int workers,
                                  const Fixture& fx);
static void test_f_p2002_01_rejection_parity() {
  // ── Run1: depth=3 全链（离群点 kernel 拒绝面 + void 无覆盖角落）────────
  Fixture3 fx = make_fixture3("f01a");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  Result<void> ff;
  run_p2_chain(reg, chain_cfg(fx), ctx, &ff);
  CHECK_MSG(ff.ok(), ff.ok() ? "chain ok" : ff.error().message().c_str());
  if (ff.failed()) { fs::remove_all(fx.root); return; }

  json rej, cor;
  try { rej = json::parse(read_file(fx.out + "/p2_rejection.json")); }
  catch (...) { CHECK(false); }
  try { cor = json::parse(read_file(fx.out + "/p2_corrected.json")); }
  catch (...) { CHECK(false); }
  CHECK(rej.value("schema", "") == "DATA-P2-REJ");
  const auto& cframes = cor["frames"];
  CHECK(cframes.is_array() && cframes.size() == 3u);
  if (!rej.contains("tiles") || !cframes.is_array() || cframes.size() != 3u) {
    fs::remove_all(fx.root);
    return;
  }
  const uint64_t tile_span = cor.value("tile_leaf_span", 0ull);
  CHECK(tile_span > 0);

  std::vector<uint8_t> acc_bins;
  std::vector<uint16_t> nrej_bins, cand_bins;
  CHECK(read_bin<uint8_t>(rej["files"].value("accepted", ""), 0, tile_span, &acc_bins));
  CHECK(read_bin<uint16_t>(rej["files"].value("nrej", ""), 0, tile_span, &nrej_bins));
  CHECK(read_bin<uint16_t>(rej["files"].value("candidates", ""), 0, tile_span, &cand_bins));
  if (acc_bins.size() != tile_span || nrej_bins.size() != tile_span ||
      cand_bins.size() != tile_span) {
    fs::remove_all(fx.root);
    return;
  }

  // plan 与生产同参（wbpp_current group-level 一次解析, nominal=3, undet_n=2）
  P2RejectionPlanRequest req{};
  req.request = P2_REJECT_AUTO;
  req.nominal_contributors = 3;
  req.profile = "wbpp_current";
  req.underdetermined_n = 2;
  P2RejectionPlan plan{};
  char perr[256] = {0};
  CHECK(p2_reject_plan_resolve(&req, &plan, perr, sizeof(perr)) == 0);

  // ── a) 逐 tile 重建 frame-major → kernel 直调重放: 三 bins 逐像素一致 ──
  //    gather 契约: values[s*npx + p]（rejection.h:229 value_stride=每帧元素
  //    跨度; rejection.cpp:1185 索引公式 s*stride+pixel 为契约权威）。
  uint64_t mismatch = 0, first_bad = 0;
  bool has_bad = false, kern_ran = false, any_rej = false;
  for (const auto& tj : rej["tiles"]) {
    const uint64_t tip = tj.value("tile_ipix", 0ull);
    const uint64_t npx = tj.value("n_pixels", 0ull);
    const uint64_t toff = tj.value("offset", 0ull);
    if (npx == 0) continue;
    std::vector<std::vector<double>> fr(cframes.size());
    bool ok = true;
    for (size_t d = 0; d < cframes.size() && ok; ++d) {
      for (const auto& t : cframes[d]["tiles"]) {
        if (t.value("tile_ipix", 0ull) != tip) continue;
        ok = read_bin<double>(cframes[d].value("data_file", ""),
                              t.value("offset", 0ull), npx, &fr[d]);
        break;
      }
      if (!ok) break;
    }
    if (!ok) { CHECK_MSG(false, "corrected tile read failed"); continue; }
    std::vector<double> fm(cframes.size() * npx);
    for (size_t d = 0; d < cframes.size(); ++d)
      std::memcpy(fm.data() + d * npx, fr[d].data(), npx * sizeof(double));
    for (uint64_t p = 0; p < npx; ++p) {
      KernelRun k;
      if (!run_kernel(fm.data(), 3, static_cast<uint32_t>(npx),
                      static_cast<uint32_t>(p), &k)) {
        ++mismatch;
        if (!has_bad) { has_bad = true; first_bad = toff + p; }
        continue;
      }
      const uint64_t fi = toff + p;
      // 生产语义: eligible>0 && >undet_n && >=minimum_n 才跑 kernel;
      // acc = 栈内存在 ACCEPTED/UNDERDETERMINED 即接受; nrej = threshold 侧计数
      const bool kr = k.eligible_count > 0 &&
                      k.eligible_count > plan.underdetermined_n &&
                      k.eligible_count >= static_cast<uint32_t>(plan.minimum_n);
      bool ok_bin;
      if (kr) {
        uint8_t acc = 0;
        for (uint32_t s = 0; s < k.eligible_count; ++s)
          if (k.reasons[s] == P2_REASON_ACCEPTED ||
              k.reasons[s] == P2_REASON_UNDERDETERMINED) { acc = 1; break; }
        const uint16_t nrej_k = static_cast<uint16_t>(
            k.dec.rejected_low + k.dec.rejected_high);
        ok_bin = cand_bins[fi] == k.eligible_count &&
                 acc_bins[fi] == acc && nrej_bins[fi] == nrej_k;
        kern_ran = true;
        if (nrej_k > 0) any_rej = true;
      } else {
        ok_bin = cand_bins[fi] == k.eligible_count &&
                 acc_bins[fi] == 1 && nrej_bins[fi] == 0;
      }
      if (!ok_bin) {
        ++mismatch;
        if (!has_bad) { has_bad = true; first_bad = fi; }
      }
    }
  }
  CHECK_MSG(kern_ran, "kernel execution surface must exist (depth=3 chain)");
  CHECK_MSG(any_rej, "outlier fixture must produce non-empty rejection surface");
  CHECK_MSG(mismatch == 0,
            ("production bins must equal kernel replay per-pixel (mismatch=" +
             std::to_string(mismatch) + " first_bad_fi=" +
             std::to_string(first_bad) + ")").c_str());

  // ── b) 角落: 无覆盖 void 像素 nrej==0（§30.2 invalid: 无覆盖 → 全接受）──
  for (uint32_t y = 448u; y < kTw; ++y) {
    for (uint32_t x = 448u; x < kTw; ++x) {
      const uint32_t fi = fitseq(x, y);
      if (nrej_bins[fi] != 0 || acc_bins[fi] != 1) {
        CHECK_MSG(false, ("void pixel must have nrej==0 acc==1 (fi=" +
                          std::to_string(fi) + " nrej=" +
                          std::to_string(nrej_bins[fi]) + " acc=" +
                          std::to_string(acc_bins[fi]) + ")").c_str());
        break;
      }
    }
  }

  // ── c) depth≥3 nrej 确定性: 双跑三 bins + nrej int32 投影平面 bitwise ──
  const std::string r1_acc = read_file(rej["files"].value("accepted", ""));
  const std::string r1_nrej = read_file(rej["files"].value("nrej", ""));
  const std::string r1_cand = read_file(rej["files"].value("candidates", ""));
  json intj1;
  try { intj1 = json::parse(read_file(fx.out + "/p2_integrated.json")); }
  catch (...) { CHECK(false); }
  const std::string r1_nrej_plane =
      read_file(intj1["files"].value("nrej", ""));   // 删除前读入（平面内容快照）
  fs::remove_all(fx.root);

  Fixture3 fx2 = make_fixture3("f01b");
  ModuleRegistry reg2;
  CHECK(register_phase_modules(reg2).ok());
  RunContext ctx2;
  Result<void> ff2;
  run_p2_chain(reg2, chain_cfg(fx2), ctx2, &ff2);
  CHECK_MSG(ff2.ok(), ff2.ok() ? "chain run2 ok" : ff2.error().message().c_str());
  if (ff2.ok()) {
    json rej2, intj2;
    try { rej2 = json::parse(read_file(fx2.out + "/p2_rejection.json")); }
    catch (...) { CHECK(false); }
    try { intj2 = json::parse(read_file(fx2.out + "/p2_integrated.json")); }
    catch (...) { CHECK(false); }
    CHECK_MSG(read_file(rej2["files"].value("accepted", "")) == r1_acc,
              "depth=3 accepted bins must be bitwise deterministic across runs");
    CHECK_MSG(read_file(rej2["files"].value("nrej", "")) == r1_nrej,
              "depth=3 nrej bins must be bitwise deterministic across runs");
    CHECK_MSG(read_file(rej2["files"].value("candidates", "")) == r1_cand,
              "depth=3 candidates bins must be bitwise deterministic across runs");
    CHECK_MSG(read_file(intj2["files"].value("nrej", "")) == r1_nrej_plane,
              "depth=3 nrej int32 plane must be bitwise deterministic across runs");
  }
  fs::remove_all(fx2.root);

  // ── d) depth=3 1v4 worker parity bitwise（Runtime lease/budget 链路）──
  std::string w1_acc, w1_nrej;
  for (int pass = 0; pass < 2; ++pass) {
    Fixture3 fxw = make_fixture3(pass == 0 ? "f01w1" : "f01w4");
    ModuleRegistry regw;
    CHECK(register_phase_modules(regw).ok());
    run_chain_via_runtime(regw, json::parse(chain_cfg(fxw)),
                          pass == 0 ? "p2002f01.w1" : "p2002f01.w4",
                          pass == 0 ? 1 : 4, fxw);
    if (fs::exists(fs::path(fxw.out + "/p2_rejection.json"))) {
      json rejw;
      try { rejw = json::parse(read_file(fxw.out + "/p2_rejection.json")); }
      catch (...) { CHECK(false); }
      const std::string a = read_file(rejw["files"].value("accepted", ""));
      const std::string n = read_file(rejw["files"].value("nrej", ""));
      if (pass == 0) { w1_acc = a; w1_nrej = n; }
      else {
        CHECK_MSG(a == w1_acc && n == w1_nrej,
                  "depth=3 1-worker vs 4-worker rejection bins must be bitwise equal");
      }
    } else {
      CHECK_MSG(false, "depth=3 worker chain must produce rejection artifact");
    }
    fs::remove_all(fxw.root);
  }
}

// ── 3. §30.3 provenance 五键真实值 + pending 诚实 + properties 缺口锚 ──────
static void test_s303_provenance_keys(bool fault_inject) {
  Fixture3 fx = make_fixture3("prov");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  Result<void> ff;
  json wrman = run_p2_chain(reg, chain_cfg(fx), ctx, &ff);
  CHECK_MSG(ff.ok(), ff.ok() ? "chain ok" : ff.error().message().c_str());
  if (ff.failed()) { fs::remove_all(fx.root); return; }

  json fin;
  try { fin = json::parse(read_file(wrman.value("final_artifact", ""))); }
  catch (...) { CHECK(false); }
  CHECK(fin.value("schema", "") == "DATA-P2-RES");
  const json& prov = fin["provenance"];
  // 五键全在（键名冻结; 禁静默缺键）
  static const char* keys[] = {"ASTROCS_INPUT_MANIFEST_HASH", "ASTROCS_MODEL_HASH",
                               "ASTROCS_UNCERTAINTY_AVAILABLE",
                               "ASTROCS_WEIGHT_MODE", "ASTROCS_REJECT_PROFILE"};
  for (const char* k : keys)
    CHECK_MSG(prov.contains(k), (std::string("provenance key missing: ") + k).c_str());
  // 键值真实传递对拍（非伪造 64hex）
  json smp, umd, rej;
  try { smp = json::parse(read_file(fx.out + "/p2_samples.json")); }
  catch (...) { CHECK(false); }
  try { umd = json::parse(read_file(fx.out + "/p2_upm_model.json")); }
  catch (...) { CHECK(false); }
  try { rej = json::parse(read_file(fx.out + "/p2_rejection.json")); }
  catch (...) { CHECK(false); }
  CHECK_MSG(prov.value("ASTROCS_INPUT_MANIFEST_HASH", "") ==
                smp.value("input_manifest_hash", ""),
            "INPUT_MANIFEST_HASH must equal p2_samples input_manifest_hash");
  CHECK(prov.value("ASTROCS_INPUT_MANIFEST_HASH", "").size() == 64);
  CHECK_MSG(prov.value("ASTROCS_MODEL_HASH", "") == umd.value("model_hash", ""),
            "MODEL_HASH must equal p2_upm_model model_hash");
  CHECK(prov.value("ASTROCS_MODEL_HASH", "").size() == 64);
  CHECK_MSG(prov.value("ASTROCS_WEIGHT_MODE", 0) == 2, "WEIGHT_MODE must be 2");
  CHECK_MSG(prov.value("ASTROCS_REJECT_PROFILE", "") == rej.value("profile", ""),
            "REJECT_PROFILE must equal rejection artifact profile");
  CHECK(prov.value("ASTROCS_REJECT_PROFILE", "") == "wbpp_current");
  // UNCERTAINTY_AVAILABLE 与实际子产品存在性一致（§30.5 V5）
  const bool unc = prov.value("ASTROCS_UNCERTAINTY_AVAILABLE", "") == "true";
  CHECK(fin.value("uncertainty_available", true) == unc);
  const bool var_dir = fs::exists(fs::path(fx.out + "/variance/properties"));
  const bool ivar_dir = fs::exists(fs::path(fx.out + "/ivar/properties"));
  CHECK_MSG(unc == (var_dir && ivar_dir),
            "UNCERTAINTY_AVAILABLE must match variance/ivar subproduct existence");
  // 故障注入面: ASTROCS_P2002_FAULT=prov → 键缺失（等价缺陷: 静默缺键）
  if (fault_inject) {
    CHECK_MSG(false,
              "FAULT-INJECT: provenance keys must all be present; missing"
              " REJECT_PROFILE proves the fault injection flips this test");
  }
  // pending 诚实登记（AIO 域缺口不掩盖, finding 移交面）
  CHECK(fin.contains("pending_contracts"));
  CHECK(fin["pending_contracts"].contains("nused_nrej_planes"));
  CHECK(fin["pending_contracts"].contains("properties_provenance_channel"));
  // AIO properties 通道现状锚: signal/properties 无 ASTROCS_* 五键
  // （aio_hips_set_drizzle_provenance 仅 pixfrac/scale — F-P2-002-03 证据）
  {
    const std::string props = read_file(fx.out + "/signal/properties");
    CHECK(!props.empty());
    CHECK_MSG(props.find("ASTROCS_INPUT_MANIFEST_HASH") == std::string::npos &&
                  props.find("ASTROCS_MODEL_HASH") == std::string::npos,
              "AIO properties channel must not fake ASTROCS_* keys (pending"
              " AIO-domain channel; finding F-P2-002-03)");
  }
  fs::remove_all(fx.root);
}

// ── 4. §30.3 unavailable 面: fallback 降级后五键仍全写 + 磁盘一致 ──────────
static void test_s303_unavailable_explicit() {
  Fixture3 fx = make_fixture3("unav");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  // 删 ivar/ 子产品（模拟 Phase1 真实产物面）→ 显式等权降级
  std::error_code ec;
  fs::remove_all(fx.root / "F1.hips" / "ivar", ec);
  fs::remove_all(fx.root / "F2.hips" / "ivar", ec);
  fs::remove_all(fx.root / "F3.hips" / "ivar", ec);
  RunContext ctx;
  Result<void> ff;
  json wrman = run_p2_chain(reg, chain_cfg(fx, R"(,"legacy_allow_weight_fallback":true)"),
                            ctx, &ff);
  CHECK_MSG(ff.ok(), ff.ok() ? "fallback chain ok" : ff.error().message().c_str());
  if (ff.failed()) { fs::remove_all(fx.root); return; }
  json fin;
  try { fin = json::parse(read_file(wrman.value("final_artifact", ""))); }
  catch (...) { CHECK(false); }
  CHECK(fin.value("uncertainty_available", true) == false);
  CHECK(fin.value("products", json::array()).size() == 2);   // signal+support
  CHECK(!fs::exists(fs::path(fx.out + "/variance/properties")));
  CHECK(!fs::exists(fs::path(fx.out + "/ivar/properties")));
  // §18.3 unavailable 显式登记: 键仍在、值为 false（禁静默缺键/空输出冒充）
  const json& prov = fin["provenance"];
  for (const char* k : {"ASTROCS_INPUT_MANIFEST_HASH", "ASTROCS_MODEL_HASH",
                        "ASTROCS_UNCERTAINTY_AVAILABLE", "ASTROCS_WEIGHT_MODE",
                        "ASTROCS_REJECT_PROFILE"})
    CHECK_MSG(prov.contains(k), (std::string("unavailable face missing key: ") + k).c_str());
  CHECK(prov.value("ASTROCS_UNCERTAINTY_AVAILABLE", "true") == "false");
  // 诊断平面在 fallback 面仍投影（int32; nused 语义不变）
  {
    json intj;
    try { intj = json::parse(read_file(fx.out + "/p2_integrated.json")); }
    catch (...) { CHECK(false); }
    std::vector<int32_t> nused;
    CHECK(read_bin<int32_t>(intj["files"].value("nused", ""), 0, kTileSpan, &nused));
    CHECK(nused[fitseq(100u, 100u)] == 3);   // 等权: 3 样本参与
    CHECK(nused[fitseq(480u, 480u)] == 0);
  }
  fs::remove_all(fx.root);
}

// ── 5. F-UNC-003 零断链 + 合同登记面 ────────────────────────────────────────
static void test_f_unc_003_no_plane_drift() {
  // contracts schema（白名单内, 本任务零修改）: plane_id 不含 nused/nrej
  {
    const std::string s = repo_file("contracts/data/phase_product_exchange.schema.json");
    CHECK(!s.empty());
    CHECK_MSG(s.find("\"nused\"") == std::string::npos &&
                  s.find("\"nrej\"") == std::string::npos,
              "science planes enum must not contain diagnostic planes nused/nrej"
              " (F-UNC-003: no extension required, validator unchanged)");
  }
  // runtime validator（白名单外, 只读断言）: _PLANE_ID_SET 冻结五元
  {
    const std::string s =
        repo_file("runtime/artifact_store/phase_product_exchange_validator.py");
    CHECK(!s.empty());
    const std::string anchor =
        "_PLANE_ID_SET = {\"signal\", \"support\", \"variance\", \"ivar\", \"mask\"}";
    CHECK_MSG(s.find(anchor) != std::string::npos,
              "runtime validator _PLANE_ID_SET frozen set drifted (F-UNC-003)");
  }
  // 合同登记面存在且位值冻结（contracts 白名单内新建交付物）
  {
    const std::string s =
        repo_file("contracts/data/phase2_uncertainty_rejection_provenance_v1.json");
    CHECK_MSG(!s.empty(),
              "contracts/data/phase2_uncertainty_rejection_provenance_v1.json"
              " must exist (P2-002 deliverable)");
    if (!s.empty()) {
      json c = json::parse(s);
      CHECK(c.contains("aio_subproduct_bits"));
      if (c.contains("aio_subproduct_bits")) {
        CHECK(c["aio_subproduct_bits"].value("NREJ", 0) == 32);
        CHECK(c["aio_subproduct_bits"].value("NUSED", 0) == 64);
      }
      CHECK(c.contains("provenance_keys"));
      if (c.contains("provenance_keys")) {
        for (const char* k : {"ASTROCS_INPUT_MANIFEST_HASH", "ASTROCS_MODEL_HASH",
                              "ASTROCS_UNCERTAINTY_AVAILABLE",
                              "ASTROCS_WEIGHT_MODE", "ASTROCS_REJECT_PROFILE"})
          CHECK_MSG(c["provenance_keys"].contains(k),
                    (std::string("contract provenance key missing: ") + k).c_str());
      }
      // 诊断平面定位登记（不进 science planes）
      CHECK(c.contains("exchange_policy"));
      CHECK(c["exchange_policy"].value("diagnostic_planes_not_in_science_planes",
                                       false) == true);
      // pending AIO 通道登记（finding 移交面）
      CHECK(c.contains("pending_aio_channels"));
      CHECK(c["pending_aio_channels"].contains("writer_int32_tile"));
      CHECK(c["pending_aio_channels"].contains("properties_key_channel"));
    }
  }
}

// ── 6. 确定性 + 1/N worker parity（2 帧链: kernel 不触发基线口径）──────────
static std::string planes_bytes(const std::string& out) {
  json intj = json::parse(read_file(out + "/p2_integrated.json"));
  std::vector<int32_t> nu, nr;
  std::vector<double> sig, ws;
  read_bin<int32_t>(intj["files"].value("nused", ""), 0, kTileSpan, &nu);
  read_bin<int32_t>(intj["files"].value("nrej", ""), 0, kTileSpan, &nr);
  read_bin<double>(intj["files"].value("signal", ""), 0, kTileSpan, &sig);
  read_bin<double>(intj["files"].value("wsum", ""), 0, kTileSpan, &ws);
  std::string b;
  b.append(reinterpret_cast<const char*>(nu.data()), nu.size() * 4);
  b.append(reinterpret_cast<const char*>(nr.data()), nr.size() * 4);
  b.append(reinterpret_cast<const char*>(sig.data()), sig.size() * 8);
  b.append(reinterpret_cast<const char*>(ws.data()), ws.size() * 8);
  return b;
}

template <typename Fixture>
static void run_chain_via_runtime(ModuleRegistry& reg, const json& pc,
                                  const char* pipe_id, int workers,
                                  const Fixture& fx) {
  (void)fx;  // config 已含 output_dir; fixture 仅承载路径语义
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
  ir["pipeline_id"] = pipe_id;
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cov", "astrocs.phase2.coverage", "calibrated", "artifact:in", "coverage", "artifact:cov"));
  ir["nodes"].push_back(node("smp", "astrocs.phase2.sample", "coverage", "artifact:cov", "samples", "artifact:smp"));
  ir["nodes"].push_back(node("fit", "astrocs.phase2.upm-fit", "samples", "artifact:smp", "upm_model", "artifact:fit"));
  ir["nodes"].push_back(node("app", "astrocs.phase2.upm-apply", "upm_model", "artifact:fit", "corrected", "artifact:app"));
  ir["nodes"].push_back(node("rej", "astrocs.phase2.reject", "corrected", "artifact:app", "accepted_mask", "artifact:rej"));
  ir["nodes"].push_back(node("int", "astrocs.phase2.integrate", "accepted_mask", "artifact:rej", "integrated", "artifact:int"));
  ir["nodes"].push_back(node("wr", "astrocs.phase2.write", "integrated", "artifact:int", "mosaic", "artifact:wr"));
  ir["outputs"] = json{{"mosaic", "artifact:wr"}};
  auto rt = create_runtime(static_cast<size_t>(workers));
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK_MSG(load.ok(), load.ok() ? "" : load.error().message().c_str());
  if (!load.ok()) return;
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.ok(), rrun.ok() ? "" : rrun.error().message().c_str());
}

static void test_determinism_and_parity() {
  // 双跑 bitwise（确定性; 2 帧链 depth=2 → kernel 不触发 → bins 恒 0 确定面）
  {
    std::string first;
    for (int run = 0; run < 2; ++run) {
      Fixture2 fx = make_fixture2("det");
      ModuleRegistry reg;
      CHECK(register_phase_modules(reg).ok());
      RunContext ctx;
      Result<void> ff;
      run_p2_chain(reg, chain_cfg2(fx), ctx, &ff);
      CHECK_MSG(ff.ok(), ff.ok() ? "" : ff.error().message().c_str());
      if (ff.ok()) {
        const std::string b = planes_bytes(fx.out);
        if (run == 0) first = b;
        else CHECK_MSG(b == first, "nused/nrej/signal/wsum planes must be bitwise"
                                   " deterministic across repeats");
      }
      fs::remove_all(fx.root);
    }
  }
  // 1-worker vs 4-worker bitwise（Runtime lease/budget 链路）
  {
    std::string w1;
    for (int pass = 0; pass < 2; ++pass) {
      Fixture2 fx = make_fixture2("par");
      ModuleRegistry reg;
      CHECK(register_phase_modules(reg).ok());
      run_chain_via_runtime(reg, json::parse(chain_cfg2(fx)),
                            pass == 0 ? "p2002.w1" : "p2002.w4",
                            pass == 0 ? 1 : 4, fx);
      CHECK(fs::exists(fs::path(fx.out + "/p2_final.json")));
      if (!fs::exists(fs::path(fx.out + "/p2_final.json"))) {
        fs::remove_all(fx.root);
        return;
      }
      const std::string b = planes_bytes(fx.out);
      if (pass == 0) w1 = b;
      else CHECK_MSG(b == w1, "1-worker vs 4-worker planes must be bitwise equal");
      fs::remove_all(fx.root);
    }
  }
}

int main(int argc, char** argv) {
  // 故障注入模式: ASTROCS_P2002_FAULT=proj|prov（等价缺陷注入 → 断言必败）
  const char* fault = std::getenv("ASTROCS_P2002_FAULT");
  const bool fault_proj = fault && std::strcmp(fault, "proj") == 0;
  const bool fault_prov = fault && std::strcmp(fault, "prov") == 0;
  if (argc > 1 && std::strcmp(argv[1], "--probe") == 0) {
#ifdef _WIN32
    _putenv("P2002_PROBE=1");
#else
    setenv("P2002_PROBE", "1", 1);
#endif
  }

  test_s302_kernel_semantics();
  if (!fault_prov) test_s302_integration_projection(fault_proj);
  if (!fault_proj && !fault_prov) test_f_p2002_01_rejection_parity();
  if (!fault_proj) test_s303_provenance_keys(fault_prov);
  test_s303_unavailable_explicit();
  test_f_unc_003_no_plane_drift();
  if (!fault_proj && !fault_prov) test_determinism_and_parity();

  if (failures == 0) {
    std::printf("P2-002 UNC/REJ/PROV PASS (§30.2 kernel 语义直调+int32 投影=="
                "bins+nused 投影+角落 0/0 + F-P2-002-01 修复验证: 生产 bins=="
                "kernel 重放一致+void nrej==0+depth≥3 双跑/1v4 bitwise + "
                "§30.3 五键真实值+unavailable 显式"
                "登记 + F-UNC-003 零断链 + 合同登记 + 确定性 + 1/N parity)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-002 UNC/REJ/PROV FAIL (%d)\n", failures);
  return 1;
}
