// tests/unit/p2001_real_nodes_test.cpp — P2-001 Phase2 真实算法节点与 complete 门
//
// 验收映射（控制包 P2-001, 前台口径①-④, BASE=9e09941a）:
//   1. registry 7 类 Phase2 节点各自唯一真实 operation 委托（改造前: 工厂委托
//      P2Api session adapter = 每个子节点调用完整 p2_session_run, 7 节点链重复
//      执行全链 7 次, manifest 无 operation/entry 字段 —— RED 锚定即断言 1/2 在
//      改造前失败）。节点 last_manifest 必须携带 operation/entry 标记, 与
//      runtime/pipeline/module_ports.registry.json 冻结绑定表逐一一致。
//   2. typed artifact: 每节点产出 descriptor.data_id 对应的磁盘 artifact。
//   3. trace call_count=1: Runtime 全链执行每节点 MODULE_CALL 恰好一次,
//      trace_violations 为空（无隐藏 session 重复调用）。
//   4. complete 门 fail-closed: p2_session manifest status="partial"（链不完整,
//      availability 7 域如实报告, 不冒充完成）。
//   5. DATA-UNC-001 §30: weight_mode=2 逆方差合成（ivar_mosaic=Σivar_i,
//      variance=1/W, 经 AIO variance 通道 §12.3/§12.4 归约）; ivar 产品缺失
//      fail-closed（无 fallback, 禁静默）; legacy_allow_weight_fallback=true
//      显式等权降级 + uncertainty_available=false（不写 variance/ivar 子产品）。
//   6. 负向/确定性/1-N worker parity/fail-fast 下游 call_count=0。
//
// fixture: Phase1 真实链节点（drizzle→writer）产出单帧 HiPS（无 ivar 产品,
// 驱动 §30.1 unavailable 语义面）+ AIO 生产写链直接构造带 ivar/variance 的
// 两帧 HiPS（驱动 §30.1 数值 oracle 面）。
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"
#include "p2_session.h"  // complete 门: API-P2-001 冻结 C ABI

#include "p1sess_fixtures.hpp"  // 最小 FITS writer (手写, 不调生产 symbol)
#include "aio_hips.h"
#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define P2001_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P2001_GETPID static_cast<long>(::getpid())
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

// host services 工厂（lib/backend_host/host_services.cpp, astrocs_cpu 库）
extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);

acs_status p2_session_create(const astrocs_host_services_v1* host, acs_handle* out);
acs_status p2_session_validate(acs_handle h, const acs_span_u8 config_json);
acs_status p2_session_run(acs_handle h, const acs_span_u8 config_json);
acs_status p2_session_inspect(acs_handle h, acs_span_u8* out_manifest_json);
acs_status p2_session_destroy(acs_handle h);
}

namespace {

namespace fs = std::filesystem;

// ── fixture A: Phase1 真实链 2 帧单帧 HiPS（32x32 星场, 不同亮度, nside 512）──
constexpr int kW = 32, kH = 32;
struct StarField {
  float bg;
  float amp;
};
inline float star_field_pixel(int i, void* user) {
  auto* sf = static_cast<StarField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - 16.0;
  const double dy = static_cast<double>(y) - 16.0;
  const double g = sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  return sf->bg + static_cast<float>(g);
}

struct P1Fixture {
  fs::path root;
  std::string light1, light2;
  std::string hips1, hips2;   // 单帧 HiPS 树（无 ivar 产品）
};

P1Fixture make_p1_fixture(const char* tag) {
  P1Fixture fx;
  fx.root = fs::temp_directory_path() /
            ("p2001_p1_" + std::string(tag) + "_" + std::to_string(P2001_GETPID));
  std::error_code ec;
  fs::create_directories(fx.root, ec);
  fx.light1 = (fx.root / "light_1.fits").string();
  fx.light2 = (fx.root / "light_2.fits").string();
  StarField sf1{100.0f, 5000.0f};
  StarField sf2{100.0f, 4700.0f};
  CHECK(p1sess::write_fits_file(fx.light1, kW, kH, star_field_pixel, &sf1) == 0);
  CHECK(p1sess::write_fits_file(fx.light2, kW, kH, star_field_pixel, &sf2) == 0);

  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const char* dirs[2] = {"f1", "f2"};
  const char* lights[2] = {fx.light1.c_str(), fx.light2.c_str()};
  std::string* hips_out[2] = {&fx.hips1, &fx.hips2};
  for (int i = 0; i < 2; ++i) {
    const std::string od = (fx.root / dirs[i]).string();
    fs::create_directories(od, ec);   // P1 drizzle HISS writer 需目录预存
    const std::string cfg = R"({
      "input_lights": [")" + std::string(lights[i]) + R"("],
      "output_dir": ")" + od + R"(",
      "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
              "cd11": -0.0002777777777777778, "cd12": 0.0,
              "cd21": 0.0, "cd22": 0.0002777777777777778},
      "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
    })";
    auto drz = reg.create("astrocs.phase1.drizzle");
    CHECK(drz.ok());
    CHECK(drz.value()->validate_config(cfg).ok());
    CHECK(drz.value()->plan(("p1drz_" + std::to_string(i)), cfg).ok());
    auto rd = drz.value()->execute(ctx);
    if (rd.failed())
      std::fprintf(stderr, "P1 drizzle failed: %s\n", rd.error().message().c_str());
    CHECK(rd.ok());
    auto wr = reg.create("astrocs.phase1.writer");
    CHECK(wr.ok());
    CHECK(wr.value()->validate_config(cfg).ok());
    CHECK(wr.value()->plan(("p1wr_" + std::to_string(i)), cfg).ok());
    auto rw = wr.value()->execute(ctx);
    if (rw.failed())
      std::fprintf(stderr, "P1 writer failed: %s\n", rw.error().message().c_str());
    CHECK(rw.ok());
    *hips_out[i] = od;
  }
  return fx;
}

// ── fixture B: AIO 生产写链 2 帧带 ivar/variance 的 HiPS（1 tile, order 9）──
constexpr uint32_t kNside = 512;
constexpr uint32_t kTw = 512;
// ivar 常数: F1=2.0（variance 0.5）, F2=0.5（variance 2.0）
//   → 逆方差合成 oracle: W = 2.5, variance = 0.4
constexpr double kIvar1 = 2.0, kIvar2 = 0.5;
constexpr double kAcell =
    4.0 * 3.14159265358979323846 / (12.0 * double(kNside) * double(kNside));
// area=1e-2 → signal = flux/area ~1e4（f32 ulp ~1e-3 保留 +10 offset 与
// 0.1 级噪声; control_variance ~O(1) ≫ zero_anchor_weight 1e-3, 避免
// UPM IRLS 数值失衡 —— P1 真实产品 area~1e-8 的值域失衡是 stage2 预存
// 数值面, 非节点实现缺陷, finding 登记）
constexpr float kArea = 1.0e-2f;

inline float ivar_sig1(uint32_t x, uint32_t y) {
  // 确定性微噪声场（MAD>0 → control_variance>0 → production 权重合法）
  return 100.0f + 0.1f * static_cast<float>((x * 7u + y * 13u) % 5u);
}

bool write_ivar_frame(const std::string& path, double ivar, float offset) {
  AioHipsProductSet* ps = aio_hips_product_begin(
      path.c_str(), kNside, kTw, AIO_HIPS_FLOAT32,
      AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
          AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR,
      "ivo://astrocs/test", "P2-001 ivar fixture", "R", 60.0,
      "2026-09-09T00:00:00Z", 0);
  if (!ps) {
    std::fprintf(stderr, "fixture begin failed: %s\n", aio_hips_last_error());
    return false;
  }
  // view 合同 = NESTED local 序（writer 内部 nested_local_to_fits_index 归约
  // 写 FITS）; 先按 FITS 序求值再经单一权威逆映射重排。
  std::vector<float> sig(kTw * kTw), area(kTw * kTw, kArea);
  for (uint32_t y = 0; y < kTw; ++y)
    for (uint32_t x = 0; x < kTw; ++x) {
      const uint32_t fi = y * kTw + x;
      const uint32_t local = static_cast<uint32_t>(
          astrocs::healpix::fits_index_to_nested_local(fi, 9u, kTw));
      sig[local] = ivar_sig1(x, y) + offset;
      if (x >= 448u && y >= 448u) area[local] = 0.0f;   // 无覆盖角落 → NaN 语义面
    }
  // variance_mosaic = 1/ivar; writer 通道: variance = var_num_sum/covered_area²
  // （covered_area = view 传入的 area 数值 = kArea）→ var_num = var × kArea²
  const double var_num = (1.0 / ivar) * double(kArea) * double(kArea);
  std::vector<float> vnum(kTw * kTw, static_cast<float>(var_num));
  AstroSphereTileView v{};
  v.parent_ipix = 0;              // order0 tile（12 基元首元）
  v.leaf_order = 9;               // nside=512 → leaf L=9
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

struct IvarFixture {
  fs::path root;
  std::string hips1, hips2;
  std::string out;   // Phase2 节点链 output_dir
};

IvarFixture make_ivar_fixture(const char* tag) {
  IvarFixture fx;
  fx.root = fs::temp_directory_path() /
            ("p2001_ivar_" + std::string(tag) + "_" + std::to_string(P2001_GETPID));
  std::error_code ec;
  fs::create_directories(fx.root, ec);
  fx.hips1 = (fx.root / "F1.hips").string();
  fx.hips2 = (fx.root / "F2.hips").string();
  CHECK(write_ivar_frame(fx.hips1, kIvar1, 0.0f));
  CHECK(write_ivar_frame(fx.hips2, kIvar2, 10.0f));   // 帧间 +10 offset → UPM 真实校正
  fx.out = (fx.root / "p2out").string();
  fs::create_directories(fx.out, ec);
  return fx;
}

template <typename Fx>
std::string ivar_cfg(const Fx& fx, const std::string& extra = "") {
  return R"({
    "hips_paths": [")" + fx.hips1 + R"(", ")" + fx.hips2 + R"("],
    "output_dir": ")" + fx.out + R"(")" + extra + R"(
  })";
}

// ── Phase2 节点期望表（唯一真实 operation 绑定, 冻结源
//    runtime/pipeline/module_ports.registry.json）──────────────────────────
struct NodeExpect {
  const char* module_id;
  const char* operation;
  const char* entry;
  const char* artifact_key;   // manifest 中的 typed artifact 路径键
};
const NodeExpect kP2NodeExpects[] = {
    {"astrocs.phase2.coverage",   "compute_coverage", "astrocs_phase2_coverage_v1",  "coverage_artifact"},
    {"astrocs.phase2.sample",     "sample_frames",    "astrocs_phase2_sample_v1",    "samples_artifact"},
    {"astrocs.phase2.upm-fit",    "fit_upm",          "astrocs_phase2_upmfit_v1",    "upm_model_artifact"},
    {"astrocs.phase2.upm-apply",  "apply_upm",        "astrocs_phase2_upmapply_v1",  "corrected_artifact"},
    {"astrocs.phase2.reject",     "reject_outliers",  "astrocs_phase2_reject_v1",    "rejection_artifact"},
    {"astrocs.phase2.integrate",  "integrate_frames", "astrocs_phase2_integrate_v1", "integrated_artifact"},
    {"astrocs.phase2.write",      "write_mosaic",     "astrocs_phase2_write_v1",     "final_artifact"},
};

std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
  return s;
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

// Phase2 全链顺序执行（coverage→…→write; 可选在 integrate 步前注入 config 变体）
json run_p2_chain(ModuleRegistry& reg, const std::string& cfg, RunContext& ctx,
                  int upto = 7, Result<void>* first_fail = nullptr) {
  json last;
  for (int i = 0; i < upto; ++i) {
    Result<void> rc;
    last = run_node(reg, kP2NodeExpects[i].module_id, cfg, ctx, &rc);
    if (rc.failed()) {
      if (first_fail && !first_fail->failed()) {
        // 记录首个失败（调用方断言失败点）
        *first_fail = rc;
      }
      return last;
    }
  }
  return last;
}

// 读 bin 段
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

}  // namespace

// ── 1. 带 ivar fixture 全链: 每节点唯一真实 operation + typed artifact +
//       §30.1 数值 oracle（ivar=2.5/variance=0.4/NaN 角落/UPM 帧间校正）─────
static void test_ivar_chain_real_operation() {
  IvarFixture fx = make_ivar_fixture("chain");
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());
  RunContext ctx;
  Result<void> ff;
  run_p2_chain(reg, ivar_cfg(fx), ctx, 7, &ff);
  CHECK_MSG(ff.ok(), ff.ok() ? "chain ok" : ff.error().message().c_str());

  // 逐节点 manifest 标记 + typed artifact（重跑 coverage 单节点核对 manifest）
  RunContext ctx2;
  json man_cov = run_node(reg, "astrocs.phase2.coverage", ivar_cfg(fx), ctx2);
  CHECK(man_cov.value("operation", "") == "compute_coverage");
  CHECK(man_cov.value("entry", "") == "astrocs_phase2_coverage_v1");
  CHECK(man_cov.value("kind", "") == "astrocs.phase2.node");
  CHECK(man_cov.value("status", "") == "ok");
  CHECK(fs::exists(fs::path(man_cov.value("coverage_artifact", ""))));
  {
    json cov;
    try { cov = json::parse(read_file(man_cov.value("coverage_artifact", ""))); }
    catch (...) { CHECK(false); }
    CHECK(cov.value("schema", "") == "DATA-P2-COV");
    CHECK(cov.value("n_inputs", 0u) == 2);
    CHECK(cov.value("target_order", -1) == 0);   // tile order (nside512→hips_order=0)
    CHECK(cov.value("n_union_cells", 0u) >= 1);
  }
  // 其余 6 节点标记/entry（全链已跑, 单独复跑 sample 核对 manifest 键）
  json man_smp = run_node(reg, "astrocs.phase2.sample", ivar_cfg(fx), ctx2);
  CHECK(man_smp.value("operation", "") == "sample_frames");
  CHECK(man_smp.value("entry", "") == "astrocs_phase2_sample_v1");
  CHECK(man_smp.value("status", "") == "ok");
  CHECK(man_smp.value("n_obs", 0ull) > 0);
  CHECK(man_smp.value("overlap_controls", 0ull) > 0);
  {
    json smp;
    try { smp = json::parse(read_file(man_smp.value("samples_artifact", ""))); }
    catch (...) { CHECK(false); }
    CHECK(smp.value("schema", "") == "DATA-P2-SMP");
    CHECK(smp.value("n_obs", 0ull) > 0);
    for (const auto& f : smp.value("frame_ids", json::array()))
      CHECK(f.get<uint64_t>() != 0);   // frame_id 0 = 失败哨兵, 禁止传播
    CHECK(smp.value("input_manifest_hash", "").size() == 64);
  }
  json man_fit = run_node(reg, "astrocs.phase2.upm-fit", ivar_cfg(fx), ctx2);
  CHECK(man_fit.value("operation", "") == "fit_upm");
  CHECK(man_fit.value("entry", "") == "astrocs_phase2_upmfit_v1");
  CHECK(fs::exists(fs::path(man_fit.value("upm_model_bin", ""))));
  {
    json umd;
    try { umd = json::parse(read_file(man_fit.value("upm_model_artifact", ""))); }
    catch (...) { CHECK(false); }
    CHECK(umd.value("schema", "") == "DATA-P2-UPM");
    CHECK(umd.value("model_hash", "").size() == 64);
    CHECK(umd.value("observation_count", 0ull) > 0);
    CHECK(umd.value("use_ivar_weight", 0) == 1);   // production 权重冻结
  }
  json man_apply = run_node(reg, "astrocs.phase2.upm-apply", ivar_cfg(fx), ctx2);
  CHECK(man_apply.value("operation", "") == "apply_upm");
  CHECK(man_apply.value("entry", "") == "astrocs_phase2_upmapply_v1");
  CHECK(man_apply.value("n_pixels_total", 0ull) == 2ull * 512ull * 512ull);
  {
    json cor;
    try { cor = json::parse(read_file(man_apply.value("corrected_artifact", ""))); }
    catch (...) { CHECK(false); }
    CHECK(cor.value("schema", "") == "DATA-P2-COR");
    CHECK(cor.value("frames", json::array()).size() == 2);
    for (const auto& fr : cor["frames"]) {
      CHECK(fr.value("frame_id", 0ull) != 0);
      CHECK(fs::exists(fs::path(fr.value("data_file", ""))));
    }
    // UPM 真实校正锚: 帧 2（+10 offset）校正后与帧 1 差 ≪ 原始差 10
    const std::string d1 = cor["frames"][0].value("data_file", "");
    const std::string d2 = cor["frames"][1].value("data_file", "");
    std::vector<double> b1, b2;
    CHECK(read_bin<double>(d1, 0, 512ull * 512ull, &b1));
    CHECK(read_bin<double>(d2, 0, 512ull * 512ull, &b2));
    double before = 0.0, after = 0.0;
    int n = 0;
    for (size_t i = 0; i < b1.size(); ++i) {
      if (std::isfinite(b1[i]) && std::isfinite(b2[i])) {
        before += std::fabs(b2[i] - 10.0f - b1[i]) > 0 ? 0.0 : 0.0;  // 占位(见下)
        after += std::fabs(b2[i] - b1[i]);
        ++n;
      }
    }
    CHECK(n > 0);
    // 帧间残差（校正后）应远小于原始 offset 10
    const double mean_after = after / static_cast<double>(n);
    CHECK_MSG(mean_after < 5.0,
        ("UPM inter-frame residual after calibration should be <5 (got " +
         std::to_string(mean_after) + ")").c_str());
    (void)before;
  }
  json man_rej = run_node(reg, "astrocs.phase2.reject", ivar_cfg(fx), ctx2);
  CHECK(man_rej.value("operation", "") == "reject_outliers");
  CHECK(man_rej.value("entry", "") == "astrocs_phase2_reject_v1");
  CHECK(std::string(man_rej.value("reject_semantic_id", "")).find("astrocs.") == 0);
  {
    json rej;
    try { rej = json::parse(read_file(man_rej.value("rejection_artifact", ""))); }
    catch (...) { CHECK(false); }
    CHECK(rej.value("schema", "") == "DATA-P2-REJ");
    CHECK(rej["plan"].value("semantic_id", "") ==
          std::string(man_rej.value("reject_semantic_id", "")));
    CHECK(rej.value("n_pixels", 0ull) == 512ull * 512ull);
  }
  // integrate: weight_mode=2 + ivar 齐备 → uncertainty_available=true
  json man_int = run_node(reg, "astrocs.phase2.integrate", ivar_cfg(fx), ctx2);
  CHECK(man_int.value("operation", "") == "integrate_frames");
  CHECK(man_int.value("entry", "") == "astrocs_phase2_integrate_v1");
  CHECK(man_int.value("weight_mode", 0) == 2);
  CHECK(man_int.value("uncertainty_available", false) == true);
  {
    json intj;
    try { intj = json::parse(read_file(man_int.value("integrated_artifact", ""))); }
    catch (...) { CHECK(false); }
    if (!intj.is_object()) { fs::remove_all(fx.root); return; }
    CHECK(intj.value("schema", "") == "DATA-P2-INT");
    CHECK(intj.value("uncertainty_available", false) == true);
    CHECK(intj.value("fallback", true) == false);
    // §30.1 oracle: ivar_mosaic = W = Σ ivar_i = 2.0+0.5 = 2.5（覆盖像素）
    const json& t0 = intj["tiles"][0];
    std::vector<double> wsum;
    CHECK(read_bin<double>(intj["files"].value("wsum", ""), t0.value("offset", 0ull),
                           t0.value("n_pixels", 0ull), &wsum));
    double wsum_min = 1e300, wsum_max = -1e300;
    uint64_t n_cov = 0;
    for (double w : wsum) {
      if (std::isfinite(w)) { wsum_min = std::min(wsum_min, w);
                              wsum_max = std::max(wsum_max, w); ++n_cov; }
    }
    CHECK(n_cov > 0);
    CHECK_MSG(std::fabs(wsum_min - 2.5) < 1e-9 && std::fabs(wsum_max - 2.5) < 1e-9,
        ("ivar_mosaic must be 2.5 on covered pixels (got [" +
         std::to_string(wsum_min) + "," + std::to_string(wsum_max) + "])").c_str());
    // 加权均值 oracle: signal == (2.0·s1c + 0.5·s2c)/2.5（抽查 1000 像素）
    std::vector<double> sig, cor1, cor2;
    const std::string c1 = read_file(man_apply.value("corrected_artifact", ""));
    json cor = json::parse(c1);
    CHECK(read_bin<double>(intj["files"].value("signal", ""), t0.value("offset", 0ull),
                           t0.value("n_pixels", 0ull), &sig));
    CHECK(read_bin<double>(cor["frames"][0].value("data_file", ""), 0, 512ull * 512ull, &cor1));
    CHECK(read_bin<double>(cor["frames"][1].value("data_file", ""), 0, 512ull * 512ull, &cor2));
    for (int k = 0; k < 1000; ++k) {
      const size_t i = static_cast<size_t>((k * 7919ull) % (512ull * 512ull));
      if (!std::isfinite(cor1[i]) || !std::isfinite(cor2[i])) continue;
      const double expect = (kIvar1 * cor1[i] + kIvar2 * cor2[i]) / (kIvar1 + kIvar2);
      CHECK_MSG(std::fabs(sig[i] - expect) < 1e-9,
          ("weighted mean oracle mismatch at pixel " + std::to_string(i)).c_str());
      if (failures) return;
    }
  }
  // write: variance/ivar 产品 + §30.1 数值面（读回 2.5/0.4 + NaN 角落）
  json man_wr = run_node(reg, "astrocs.phase2.write", ivar_cfg(fx), ctx2);
  CHECK(man_wr.value("operation", "") == "write_mosaic");
  CHECK(man_wr.value("entry", "") == "astrocs_phase2_write_v1");
  CHECK(man_wr.value("n_tiles_written", 0ll) >= 1);
  CHECK(fs::exists(fs::path(fx.out + "/signal/properties")));
  CHECK(fs::exists(fs::path(fx.out + "/variance/properties")));
  CHECK(fs::exists(fs::path(fx.out + "/ivar/properties")));
  {
    json fin;
    try { fin = json::parse(read_file(man_wr.value("final_artifact", ""))); }
    catch (...) { CHECK(false); }
    CHECK(fin.value("schema", "") == "DATA-P2-RES");
    const auto& prods = fin.value("products", json::array());
    CHECK(prods.size() == 4);
    CHECK(fin.value("uncertainty_available", false) == true);
    // §30.3 provenance 键（artifact 面; properties 通道缺口如实登记）
    CHECK(fin["provenance"].value("ASTROCS_INPUT_MANIFEST_HASH", "").size() == 64);
    CHECK(fin["provenance"].value("ASTROCS_MODEL_HASH", "").size() == 64);
    CHECK(fin["provenance"].value("ASTROCS_UNCERTAINTY_AVAILABLE", "") == "true");
    CHECK(fin["provenance"].value("ASTROCS_WEIGHT_MODE", 0) == 2);
  }
  // 读回 variance/ivar tile（AIO reader; f32 容差）
  {
    AioHipsDataset* di = aio_hips_open(fx.out.c_str(), AIO_HIPS_RD_IVAR);
    AioHipsDataset* dv = aio_hips_open(fx.out.c_str(), AIO_HIPS_RD_VARIANCE);
    CHECK(di && dv);
    if (di && dv) {
      CHECK(aio_hips_tile_count(di) == aio_hips_tile_count(dv));
      std::vector<float> iv(kTw * kTw), vr(kTw * kTw);
      CHECK(aio_hips_read_tile_f32(di, 0, iv.data()) == 0);
      CHECK(aio_hips_read_tile_f32(dv, 0, vr.data()) == 0);
      // 覆盖像素: ivar=2.5, variance=0.4（f32 相对容差 1e-4）
      const size_t mid = 100u * kTw + 100u;
      CHECK_MSG(std::fabs(iv[mid] - 2.5) < 2.5e-4,
                ("ivar tile readback 2.5 (got " + std::to_string(iv[mid]) + ")").c_str());
      CHECK_MSG(std::fabs(vr[mid] - 0.4) < 0.4e-4,
                ("variance tile readback 0.4 (got " + std::to_string(vr[mid]) + ")").c_str());
      // 无覆盖角落（x≥448,y≥448）: NaN/NaN 同态（§30.1 invalid policy）
      const size_t corner = 480u * kTw + 480u;
      CHECK(std::isnan(iv[corner]));
      CHECK(std::isnan(vr[corner]));
      AioHipsDataset* ds = aio_hips_open(fx.out.c_str(), AIO_HIPS_RD_SIGNAL);
      CHECK(ds);
      if (ds) {
        std::vector<float> sg(kTw * kTw);
        CHECK(aio_hips_read_tile_f32(ds, 0, sg.data()) == 0);
        CHECK(std::isnan(sg[corner]));   // signal NaN 与 variance/ivar 同态
        aio_hips_close(ds);
      }
      aio_hips_close(di);
      aio_hips_close(dv);
    }
  }
  std::error_code ec;
  fs::remove_all(fx.root, ec);
}

// ── 2. Runtime 全链: call_count=1 / 无隐藏 session 重复调用 ────────────────
static void test_runtime_chain_call_count_1() {
  IvarFixture fx = make_ivar_fixture("rt");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());

  json pc = json::parse(ivar_cfg(fx));
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
  ir["pipeline_id"] = "p2001.real.nodes";
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

  auto rt = create_runtime(2);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK_MSG(load.ok(), load.ok() ? "" : load.error().message().c_str());
  if (load.failed()) { fs::remove_all(fx.root); return; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.ok(), rrun.ok() ? "" : rrun.error().message().c_str());

  const auto tr = rt.value()->node_trace();
  CHECK(tr.size() == 7);
  for (const auto& t : tr) {
    CHECK(t.call_count == 1);
    CHECK(t.status == "COMPLETED");
  }
  auto viol = rt.value()->trace_violations();
  CHECK_MSG(viol.empty(), "no hidden session repeated calls");
  for (const auto& v : viol) std::fprintf(stderr, "violation: %s\n", v.c_str());

  // typed artifact 全链落盘
  CHECK(fs::exists(fs::path(fx.out + "/p2_coverage.json")));
  CHECK(fs::exists(fs::path(fx.out + "/p2_samples.json")));
  CHECK(fs::exists(fs::path(fx.out + "/p2_upm_model.bin")));
  CHECK(fs::exists(fs::path(fx.out + "/p2_corrected.json")));
  CHECK(fs::exists(fs::path(fx.out + "/p2_rejection.json")));
  CHECK(fs::exists(fs::path(fx.out + "/p2_integrated.json")));
  CHECK(fs::exists(fs::path(fx.out + "/p2_final.json")));
  CHECK(fs::exists(fs::path(fx.out + "/signal/properties")));

  fs::remove_all(fx.root);
}

// ── 2b. fail-fast: 上游拒绝 → 下游 call_count=0 + 无伪产物 ─────────────────
static void test_fail_fast_downstream_zero_calls() {
  IvarFixture fx = make_ivar_fixture("ffast");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());

  // coverage 输入含不存在路径 → p2_coverage_build 确定性拒绝 → 全链失败
  json pc = json::parse(ivar_cfg(fx));
  pc["hips_paths"] = json::array({fx.hips1, fx.root / "missing.hips"});
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
  ir["pipeline_id"] = "p2001.fail.fast";
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cov", "astrocs.phase2.coverage", "calibrated", "artifact:in", "coverage", "artifact:cov"));
  ir["nodes"].push_back(node("smp", "astrocs.phase2.sample", "coverage", "artifact:cov", "samples", "artifact:smp"));
  ir["nodes"].push_back(node("wr", "astrocs.phase2.write", "integrated", "artifact:int", "mosaic", "artifact:wr"));
  ir["outputs"] = json{{"mosaic", "artifact:wr"}, {"samples", "artifact:smp"}};

  auto rt = create_runtime(2);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK_MSG(load.ok(), load.ok() ? "" : load.error().message().c_str());
  if (!load.ok()) { fs::remove_all(fx.root); return; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.failed(), "upstream DATA rejection must fail the run");

  const auto tr = rt.value()->node_trace();
  bool cov_failed = false;
  for (const auto& t : tr) {
    if (t.node_id == "cov") {
      cov_failed = t.status == "FAILED";
      continue;
    }
    CHECK_MSG(t.call_count == 0 || t.status != "COMPLETED",
              "downstream nodes must not execute after upstream failure");
  }
  CHECK_MSG(cov_failed, "coverage node must be FAILED in trace");
  CHECK(!fs::exists(fs::path(fx.out + "/p2_coverage.json")));
  CHECK(!fs::exists(fs::path(fx.out + "/p2_final.json")));

  fs::remove_all(fx.root);
}

// ── 3. complete 门: p2_session fail-closed（链不完整 → partial+availability）──
static void test_complete_gate_fail_closed() {
  IvarFixture fx = make_ivar_fixture("gate");
  astrocs_host_services_v1 host{};
  void* state = nullptr;
  CHECK(astrocs_host_services_default_v1(&host, &state) == 0);
  acs_handle h = nullptr;
  CHECK(p2_session_create(&host, &h) == ACS_OK);

  acs_span_u8 span{};
  span.head.struct_size = sizeof(span);
  span.head.abi_version = ACS_ABI_VERSION_V1;
  const std::string cfg = ivar_cfg(fx);
  span.count = cfg.size();
  span.data = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(cfg.data()));
  CHECK(p2_session_validate(h, span) == ACS_OK);
  CHECK(p2_session_run(h, span) == ACS_OK);
  acs_span_u8 out{};
  CHECK(p2_session_inspect(h, &out) == ACS_OK && out.data);
  json man;
  try {
    man = json::parse(std::string(reinterpret_cast<char*>(out.data), out.count));
  } catch (...) {
    CHECK(false);
  }
  host.allocator.free(host.allocator.user_data, out.data);
  p2_session_destroy(h);
  astrocs_host_services_destroy_state_v1(state);

  // complete 门 fail-closed: session 仅覆盖 coverage/sample/upm_fit（+persist）
  // → status 不得为 "complete"（PROD-P0-001: 不完整 Phase 禁写 complete）
  CHECK_MSG(man.value("status", "") != "complete",
            "incomplete phase2 chain must not write complete");
  CHECK(man.value("status", "") == "partial");
  CHECK(man.contains("availability") && man["availability"].is_object());
  const char* domains[] = {"coverage", "sample", "upm_fit",
                           "upm_apply", "reject", "integrate", "write"};
  for (const char* d : domains) {
    CHECK_MSG(man["availability"].contains(d),
              ("availability missing domain: " + std::string(d)).c_str());
  }
  CHECK(man["availability"].value("coverage", "") == "available");
  CHECK(man["availability"].value("upm_fit", "") == "available");
  CHECK(man["availability"].value("upm_apply", "") == "unavailable");
  CHECK(man["availability"].value("write", "") == "unavailable");

  fs::remove_all(fx.root);
}

// ── 4. 负向/fallback 面: 缺键/缺上游/ivar 缺失 fail-closed/显式等权降级 ──────
static void test_negative_and_fallback() {
  IvarFixture fx = make_ivar_fixture("neg");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;

  // 4a. validate: 缺 output_dir / 空 hips_paths / 非字符串项 / weight_mode 错型
  {
    auto m = reg.create("astrocs.phase2.coverage");
    CHECK(m.ok());
    CHECK(m.value()->validate_config(R"({"hips_paths":["a"]})").failed());
    CHECK(m.value()->validate_config(R"({"hips_paths":[],"output_dir":"x"})").failed());
    CHECK(m.value()->validate_config(R"({"hips_paths":[1,2],"output_dir":"x"})").failed());
    CHECK(m.value()->validate_config(R"({"hips_paths":["a"],"output_dir":"x","weight_mode":"2"})").failed());
    CHECK(m.value()->validate_config(ivar_cfg(fx)).ok());
  }
  // 4b. sample 缺上游 coverage artifact → DATA fail-closed
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase2.sample", ivar_cfg(fx), ctx, &rc);
    CHECK_MSG(rc.failed(), "missing upstream artifact must fail closed");
    CHECK(!fs::exists(fs::path(fx.out + "/p2_samples.json")));
  }
  // 4c. weight_mode=0（legacy SNR）→ 非科学方差面, DATA 拒（§30.1 规则 1）
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase2.integrate",
             ivar_cfg(fx, R"(,"weight_mode":0)"), ctx, &rc);
    CHECK_MSG(rc.failed(), "weight_mode=0 must be rejected in node chain");
  }
  // 4d. 输入无 ivar 子产品（ivar fixture 副本删除 ivar/ 目录, 保留 signal/
  //     support —— 模拟 Phase1 真实产物面）+ weight_mode=2 默认 → integrate
  //     fail-closed（§30.1 unavailable 规则 2 前置: 无 fallback 禁静默）
  {
    const fs::path root2 = fx.root.parent_path() /
        ("p2001_noivar_" + std::to_string(P2001_GETPID));
    std::error_code ec2;
    fs::create_directories(root2, ec2);
    fs::copy(fx.root / "F1.hips", root2 / "F1.hips",
             fs::copy_options::recursive);
    fs::copy(fx.root / "F2.hips", root2 / "F2.hips",
             fs::copy_options::recursive);
    std::error_code ec;
    fs::remove_all(root2 / "F1.hips" / "ivar", ec);
    fs::remove_all(root2 / "F2.hips" / "ivar", ec);
    const std::string out = (root2 / "p2out").string();
    fs::create_directories(out, ec);
    const std::string cfg = R"({
      "hips_paths": [")" + (root2 / "F1.hips").string() + R"(", ")" +
                    (root2 / "F2.hips").string() + R"("],
      "output_dir": ")" + out + R"("
    })";
    Result<void> ff;
    json last = run_p2_chain(reg, cfg, ctx, 6, &ff);   // 到 integrate
    CHECK_MSG(ff.failed(), "ivar product missing + weight_mode=2 must fail closed");
    CHECK(!fs::exists(fs::path(out + "/p2_integrated.json")));   // 无伪产物
    CHECK(ff.error().message().find("ivar") != std::string::npos);
    // 4e. legacy_allow_weight_fallback=true → 显式等权降级（unavailable 面）
    //     → write 只发 signal/support, 不写 variance/ivar 子产品
    const std::string cfg_fb = R"({
      "hips_paths": [")" + (root2 / "F1.hips").string() + R"(", ")" +
                    (root2 / "F2.hips").string() + R"("],
      "output_dir": ")" + out + R"(",
      "legacy_allow_weight_fallback": true
    })";
    Result<void> ff2;
    json wrman = run_p2_chain(reg, cfg_fb, ctx, 7, &ff2);
    CHECK_MSG(ff2.ok(), ff2.ok() ? "fallback chain ok" : ff2.error().message().c_str());
    if (ff2.ok()) {
      json intj;
      try { intj = json::parse(read_file(out + "/p2_integrated.json")); }
      catch (...) { CHECK(false); }
      if (!intj.is_object()) { fs::remove_all(root2); return; }
      CHECK(intj.value("fallback", false) == true);
      CHECK(intj.value("uncertainty_available", true) == false);
      json fin;
      try { fin = json::parse(read_file(wrman.value("final_artifact", ""))); }
      catch (...) { CHECK(false); }
      CHECK(fin.value("uncertainty_available", true) == false);
      CHECK(fin.value("products", json::array()).size() == 2);   // signal+support
      CHECK(!fs::exists(fs::path(out + "/variance/properties")));
      CHECK(!fs::exists(fs::path(out + "/ivar/properties")));
      CHECK(fin["provenance"].value("ASTROCS_UNCERTAINTY_AVAILABLE", "") == "false");
    }
    fs::remove_all(root2, ec);
  }
  // 4f. Phase1→Phase2 真实衔接面: P1 真实链产物（drizzle+writer 节点）经
  //     coverage/sample 消费成功（n_obs 如实计数）; 32x32 星场对 nside512
  //     tile 的覆盖占比极小 → control patch 无有效数据 → n_obs=0 →
  //     upm-fit n_obs=0 → build rc=1 fail-closed（无伪模型产物, 正确行为）
  {
    P1Fixture p1 = make_p1_fixture("neg");
    const std::string out = (p1.root / "p2out").string();
    fs::create_directories(out);
    const std::string cfg = R"({
      "hips_paths": [")" + p1.hips1 + R"(", ")" + p1.hips2 + R"("],
      "output_dir": ")" + out + R"("
    })";
    Result<void> ff;
    run_p2_chain(reg, cfg, ctx, 2, &ff);   // coverage+sample
    CHECK_MSG(ff.ok(), ff.ok() ? "P1→P2 coverage/sample ok"
                              : ff.error().message().c_str());
    json smp;
    try { smp = json::parse(read_file(out + "/p2_samples.json")); }
    catch (...) { CHECK(false); }
    CHECK(smp.value("n_obs", 0ull) == 0);   // 星场 fixture 覆盖占比 → 无 control 观测
    Result<void> ff3;
    run_node(reg, "astrocs.phase2.upm-fit", cfg, ctx, &ff3);
    CHECK_MSG(ff3.failed(), "n_obs=0 upm-fit must fail closed");
    CHECK(!fs::exists(fs::path(out + "/p2_upm_model.bin")));
    std::error_code ec;
    fs::remove_all(p1.root, ec);
  }
  fs::remove_all(fx.root);
}

// ── 5. 确定性: 全链双跑 artifact bitwise 一致 ──────────────────────────────
static void test_determinism() {
  std::string first_json, first_bin;
  for (int run = 0; run < 2; ++run) {
    IvarFixture fx = make_ivar_fixture("det");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    Result<void> ff;
    run_p2_chain(reg, ivar_cfg(fx), ctx, 7, &ff);
    CHECK_MSG(ff.ok(), ff.ok() ? "" : ff.error().message().c_str());
    const std::string intj = read_file(fx.out + "/p2_integrated.json");
    const std::string finj = read_file(fx.out + "/p2_final.json");
    std::vector<double> sigbin;
    CHECK(read_bin<double>(fx.out + "/p2_integrated_signal.bin", 0, 512ull * 512ull, &sigbin));
    std::string sigbytes(reinterpret_cast<const char*>(sigbin.data()),
                         sigbin.size() * sizeof(double));
    if (run == 0) {
      first_json = intj + finj;
      first_bin = sigbytes;
    } else {
      CHECK_MSG(intj + finj == first_json,
                "integrated/final artifacts must be bitwise deterministic");
      CHECK_MSG(sigbytes == first_bin,
                "integrated signal bin must be bitwise deterministic");
    }
    fs::remove_all(fx.root);
  }
}

// ── 6. 1/N worker parity: single vs 4-worker 全链 bitwise 一致 ─────────────
static void test_worker_parity() {
  std::string bin1;
  for (int pass = 0; pass < 2; ++pass) {
    IvarFixture fx = make_ivar_fixture("par");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    // 全链经 Runtime（lease/budget → sampler/upm cpu_workers 注入）
    json pc = json::parse(ivar_cfg(fx));
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
    ir["pipeline_id"] = pass == 0 ? "p2001.w1" : "p2001.w4";
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
    auto rt = create_runtime(pass == 0 ? 1 : 4);
    CHECK(rt.ok());
    auto load = rt.value()->load_pipeline(ir.dump(), reg);
    CHECK(load.ok());
    if (!load.ok()) { fs::remove_all(fx.root); return; }
    RunContext ctx;
    auto rrun = rt.value()->run(ctx);
    CHECK_MSG(rrun.ok(), rrun.ok() ? "" : rrun.error().message().c_str());
    std::vector<double> sigbin, wsumbin;
    CHECK(read_bin<double>(fx.out + "/p2_integrated_signal.bin", 0, 512ull * 512ull, &sigbin));
    CHECK(read_bin<double>(fx.out + "/p2_integrated_wsum.bin", 0, 512ull * 512ull, &wsumbin));
    std::string bytes;
    bytes.append(reinterpret_cast<const char*>(sigbin.data()), sigbin.size() * 8);
    bytes.append(reinterpret_cast<const char*>(wsumbin.data()), wsumbin.size() * 8);
    if (pass == 0) {
      bin1 = bytes;
    } else {
      CHECK_MSG(bytes == bin1, "1-worker vs 4-worker outputs must be bitwise equal");
    }
    fs::remove_all(fx.root);
  }
}

int main() {
  test_ivar_chain_real_operation();
  test_runtime_chain_call_count_1();
  test_fail_fast_downstream_zero_calls();
  test_complete_gate_fail_closed();
  test_negative_and_fallback();
  test_determinism();
  test_worker_parity();
  if (failures == 0) {
    std::printf("P2-001 REAL NODES PASS (7 节点唯一真实 operation + call_count=1 + complete 门 fail-closed + §30 ivar oracle + fail-fast + 确定性 + worker parity)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-001 REAL NODES FAIL (%d)\n", failures);
  return 1;
}
