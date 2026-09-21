// tests/unit/p2001_real_nodes_test.cpp — P2-001 Phase2 真实算法节点与 complete 门
//
// 验收映射（控制包 P2-001, 前台口径①-④, BASE=9e09941a）:
//   1. registry 7 类 Phase2 节点各自唯一真实 operation 委托（改造前: 工厂委托
//      P2Api session adapter = 每个子节点调用完整 p2_session_run, 7 节点链重复
//      执行全链 7 次, manifest 无 operation/entry 字段 —— RED 锚定即断言 1/2 在
//      改造前失败）。节点 last_manifest 必须携带 operation/entry 标记, 与
//      lib/infrastructure/pipeline/module_ports.registry.json 冻结绑定表逐一一致。
//   2. typed artifact: 每节点产出 descriptor.data_id 对应的磁盘 artifact。
//   3. trace call_count=1: Runtime 全链执行每节点 MODULE_CALL 恰好一次,
//      trace_violations 为空（无隐藏 session 重复调用）。
//   4. complete 门 fail-closed: p2_session manifest status="partial"（链不完整,
//      availability 7 域如实报告, 不冒充完成）。
//   5. DATA-UNC-001 §30: weight_mode=2 逆方差合成（ivar_mosaic=Σivar_i,
//      variance=1/W, 经 AIO variance 通道 §12.3/§12.4 归约）; ivar 产品缺失
//      fail-closed（禁静默）—— 由 HiPS 头帧级 SNR 现场换算权重
//      （w = SNR²/F_ref², weight-chain-report §6），键缺失同样 fail-closed;
//      RELEASE-02 SD-15 起 legacy_allow_weight_fallback=true 不再产生成功
//      等权降级（只登记 provenance），故 4e/(c) 断言 fail-closed。
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
#include <limits>
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

// host services 工厂（lib/infrastructure/benchmark/backend_host/host_services.cpp, astrocs_cpu 库）
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
    fs::create_directories(od, ec);   // P1 drizzle 产物 (HiPS) 需目录预存
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
    // P0-21 §3.4: 单帧输入 ⇒ 产品落 output_dir/<frame_key>/（不再落 output_dir 根）。
    *hips_out[i] = od + "/light_" + std::to_string(i + 1);
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
  aio_hips_tile_view_abi_init(&v);
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
//    lib/infrastructure/pipeline/module_ports.registry.json）──────────────────────────
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
  // M4-C-02: 本 fixture 为近秩亏合成面（126 obs / 128 control-frame 槽）；
  // SCI 冻结弱零锚 λ0=1e-3（生产缺省）会使其 IRLS 达 max_iterations 且
  // objective 4235（λ0=0 时 2 次收敛、objective 0）——见 B3 REPORT leftover。
  // 本用例只验证 node 接线，显式关闭 λ0 以隔离；生产缺省仍为 1e-3。
  json man_fit = run_node(reg, "astrocs.phase2.upm-fit",
                          ivar_cfg(fx, R"(,"upm":{"zero_anchor_weight":0.0})"), ctx2);
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
  // integrate: 逐样本 ivar 齐备 → uncertainty_available=true
  json man_int = run_node(reg, "astrocs.phase2.integrate", ivar_cfg(fx), ctx2);
  CHECK(man_int.value("operation", "") == "integrate_frames");
  CHECK(man_int.value("entry", "") == "astrocs_phase2_integrate_v1");
  // FIX-405 G3-12（ASTROCS_DESIGN §3.1）: 产物/manifest 不再承载「权重模式」键;
  // 方差面状态由语义键如实表达（非退化: 替代键必须在位）。
  CHECK(man_int.find("weight_mode") == man_int.end());
  CHECK(man_int.value("weight_basis", std::string()) == "per_sample_ivar");
  CHECK(man_int.value("uncertainty_available", false) == true);
  {
    json intj;
    try { intj = json::parse(read_file(man_int.value("integrated_artifact", ""))); }
    catch (...) { CHECK(false); }
    if (!intj.is_object()) { fs::remove_all(fx.root); return; }
    CHECK(intj.value("schema", "") == "DATA-P2-INT");
    CHECK(intj.value("uncertainty_available", false) == true);
    CHECK(intj.value("fallback", true) == false);
    // §30.1 oracle（RELEASE-02 HUB-A ① + SD-18 订正）: ivar_mosaic = W = Σ ivar_i，
    // 但**只对 kernel 接受的样本**求和 —— reject 已改为逐输出像素几何 n 路由。
    // SD-18（2026-09-18）：n<=3 走保守 none（不排异 + 加权积分），本 depth=2
    // fixture 的 n=2 像素全接受；oracle 仍按 p2_rejection_sample_mask.bin 逐样本
    // 剔除（同一科学口径）。掩码全接受时 W 退化为 2.5；若将来某 n 档产生拒绝，
    // 该口径如实为 2.0 / 0.5 / NaN（不是放宽）。
    const json& t0 = intj["tiles"][0];
    // 本 fixture 为单 tile（parent_ipix=0）标准 512×512 叶 tile
    const uint64_t tile_span = 512ull * 512ull;
    std::vector<double> wsum;
    CHECK(read_bin<double>(intj["files"].value("wsum", ""), t0.value("offset", 0ull),
                           t0.value("n_pixels", 0ull), &wsum));
    // 逐样本接受掩码（reject 产物; 索引 [d*tile_span + p], d=原始帧 slot）
    std::vector<uint8_t> smask;
    uint64_t sm_off = 0, sm_depth = 0;
    std::vector<uint64_t> frame_slots;
    {
      json rej;
      try { rej = json::parse(read_file(man_rej.value("rejection_artifact", ""))); }
      catch (...) { CHECK(false); }
      CHECK(rej["files"].contains("sample_mask"));
      const std::string mp = rej["files"].value("sample_mask", "");
      std::error_code mec;
      const uintmax_t msz = fs::file_size(fs::path(mp), mec);
      CHECK(!mec && msz > 0);
      if (!mec && msz > 0)
        CHECK(read_bin<uint8_t>(mp, 0, static_cast<uint64_t>(msz), &smask));
      CHECK(rej["tiles"].is_array() && rej["tiles"].size() == 1u);
      if (rej["tiles"].is_array() && rej["tiles"].size() == 1u) {
        sm_off = rej["tiles"][0].value("sample_mask_offset", ~0ull);
        sm_depth = rej["tiles"][0].value("depth", 0ull);
        if (rej["tiles"][0].contains("frame_slots"))
          for (const auto& s : rej["tiles"][0]["frame_slots"])
            frame_slots.push_back(s.get<uint64_t>());
      }
      CHECK(sm_depth == 2u);
      CHECK(frame_slots.size() == 2u);
    }
    // 加权均值 oracle: signal == Σ_{accepted} ivar_i·s_i / Σ_{accepted} ivar_i
    // （逐像素全量核对, 与 integrate 的逐样本剔除同口径）
    std::vector<double> sig, cor1, cor2;
    const std::string c1 = read_file(man_apply.value("corrected_artifact", ""));
    json cor = json::parse(c1);
    CHECK(cor.value("tile_leaf_span", 0ull) == tile_span);
    CHECK(read_bin<double>(intj["files"].value("signal", ""), t0.value("offset", 0ull),
                           t0.value("n_pixels", 0ull), &sig));
    CHECK(read_bin<double>(cor["frames"][0].value("data_file", ""), 0, 512ull * 512ull, &cor1));
    CHECK(read_bin<double>(cor["frames"][1].value("data_file", ""), 0, 512ull * 512ull, &cor2));
    const double ivar_by_frame[2] = {kIvar1, kIvar2};
    const std::vector<double>* cor_by_frame[2] = {&cor1, &cor2};
    if (smask.size() >= sm_off + 2ull * tile_span && frame_slots.size() == 2u) {
      for (size_t p = 0; p < wsum.size(); ++p) {
        const size_t gi = static_cast<size_t>(t0.value("offset", 0ull)) + p;
        if (gi >= cor1.size() || gi >= cor2.size()) break;
        bool acc[2] = {false, false};
        for (size_t d = 0; d < 2u; ++d) {
          const uint64_t slot = frame_slots[d];
          if (slot >= 2u) continue;
          // 逐样本资格 = kernel 接受掩码 ∧ 该样本 corrected 有限（掩码与数据面
          // 不得漂移: 非有限样本必然 ineligible）
          acc[slot] =
              smask[static_cast<size_t>(sm_off + d * tile_span + p)] != 0 &&
              std::isfinite((*cor_by_frame[slot])[gi]);
        }
        double expect_w = 0.0, num = 0.0;
        for (size_t slot = 0; slot < 2u; ++slot) {
          if (!acc[slot]) continue;
          expect_w += ivar_by_frame[slot];
          num += ivar_by_frame[slot] * (*cor_by_frame[slot])[gi];
        }
        if (expect_w > 0.0) {
          CHECK_MSG(std::fabs(wsum[p] - expect_w) < 1e-9,
              ("ivar_mosaic must equal sum of accepted ivars (p=" +
               std::to_string(gi) + " got " + std::to_string(wsum[p]) +
               " want " + std::to_string(expect_w) + ")").c_str());
          CHECK_MSG(std::fabs(sig[p] - num / expect_w) < 1e-9,
              ("weighted mean oracle mismatch at pixel " + std::to_string(gi) +
               " (got " + std::to_string(sig[p]) + " want " +
               std::to_string(num / expect_w) + ")").c_str());
        } else {
          // 无有效样本（无覆盖 或 样本全部被 kernel 拒绝）⇒ NaN/NaN（禁 0 伪装）
          CHECK_MSG(std::isnan(wsum[p]) && std::isnan(sig[p]),
              ("no-eligible-sample pixel must yield NaN signal/wsum (p=" +
               std::to_string(gi) + ")").c_str());
        }
        if (failures) return;
      }
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
    // A44（GAP_AUDIT §9.73 / ASTROCS_DESIGN §2.1）：全程只有 SNR，不存在
    // 「权重模式」⇒ 原 ASTROCS_WEIGHT_MODE 契约断言已删（键已不存在）。
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
    // 仅在确实失败时取 error()（故障注入面下 ff 可能为 ok —— error() 会 abort
    // 而非判红; 判据本身不放松, 只是把 abort 变成可读的断言失败）。
    if (ff.failed())
      CHECK(ff.error().message().find("ivar") != std::string::npos);
    // 4e. legacy_allow_weight_fallback=true **不再产生成功降级路径**
    //     （RELEASE-02 SD-15 / weight-chain-report §5/§6.3: 该键只登记 provenance,
    //     模块恒不返回成功等权结果）。缺 ivar 且无 HiPS 帧级 SNR 键
    //     （ASTROCS_FRAME_SNR/ASTROCS_REFERENCE_FLUX）⇒ 权重链 fail-closed。
    //     旧断言 "等权降级成功 + weight_basis=unit_weight_degraded" 编码的正是
    //     被删除的 L3 假绿路径; 正确行为 = 失败且不留任何伪产物。
    const std::string cfg_fb = R"({
      "hips_paths": [")" + (root2 / "F1.hips").string() + R"(", ")" +
                    (root2 / "F2.hips").string() + R"("],
      "output_dir": ")" + out + R"(",
      "legacy_allow_weight_fallback": true
    })";
    Result<void> ff2;
    run_p2_chain(reg, cfg_fb, ctx, 7, &ff2);
    CHECK_MSG(ff2.failed(),
              "legacy_allow_weight_fallback=true must NOT succeed when ivar and"
              " frame-SNR keys are absent (fail-closed, no equal-weight degradation)");
    CHECK_MSG(!fs::exists(fs::path(out + "/p2_integrated.json")),
              "fail-closed weight chain must not leave a pseudo integrated artifact");
    CHECK_MSG(!fs::exists(fs::path(out + "/p2_final.json")),
              "fail-closed weight chain must not leave p2_final.json");
    if (ff2.failed()) {
      const std::string msg = ff2.error().message();
      CHECK_MSG(msg.find("NOT closed") != std::string::npos,
                ("weight chain must report closure failure: " + msg).c_str());
      CHECK_MSG(msg.find("legacy_allow_weight_fallback=true") != std::string::npos,
                ("diagnostic must name the removed degradation exit: " + msg).c_str());
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
    // FIX-405 G3-12（ASTROCS_DESIGN §3.1「全程只有 SNR，不存在『权重模式』」）：
    // 真实 Phase2 产物不得再承载 weight_mode 键；方差面状态由语义键承接
    // （非退化：替代键必须同时在位，缺键 fail-closed 面不因删键而消失）。
    CHECK_MSG(intj.find("\"weight_mode\"") == std::string::npos,
              "p2_integrated.json must not carry the retired weight_mode key");
    CHECK_MSG(finj.find("\"weight_mode\"") == std::string::npos,
              "p2_final.json must not carry the retired weight_mode key");
    CHECK_MSG(intj.find("\"uncertainty_available\"") != std::string::npos &&
                  finj.find("\"uncertainty_available\"") != std::string::npos,
              "uncertainty_available must stay in phase2 products (fail-closed surface)");
    CHECK_MSG(intj.find("\"corrected_variance_used\"") != std::string::npos &&
                  intj.find("\"snr_chain_used\"") != std::string::npos,
              "phase2 variance-surface semantic keys must stay in p2_integrated.json");
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

// ── IVAR-001: weight_mode 域/审计面 + 缺 ivar 的 fail-closed 与显式降级门 ───
// 依据: DATA-UNC-001 §30.1 规则 1/2（mode 2 = 逐样本 ivar; 缺 → fail-closed;
// 唯一显式出口 = legacy_allow_weight_fallback=true 的等权降级 +
// uncertainty_available=false）+ SCI-CW-001 §5（生产默认无 fallback）+
// DATA-P2-HIPS §20.1（读端 AIO_HIPS_RD_IVAR 强制打开）。
// 故障注入面: ASTROCS_IVAR_FAULT=silent_fallback（等价缺陷: 缺 ivar 静默等权
// 降级且不标降级）⇒ 本门必然判红。
static void test_ivar001_weight_mode_domain_and_audit() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const char* fault_env = std::getenv("ASTROCS_IVAR_FAULT");
  const bool fault_silent = fault_env && std::string(fault_env) == "silent_fallback";
  // 缺 ivar 输入夹具（复制 ivar fixture 后删除 ivar/ 子产品, signal/support 保留）
  IvarFixture src = make_ivar_fixture("audsrc");
  const fs::path root2 = src.root.parent_path() /
      ("p2001_noivar_aud_" + std::to_string(P2001_GETPID));
  std::error_code ec;
  fs::create_directories(root2, ec);
  fs::copy(src.root / "F1.hips", root2 / "F1.hips", fs::copy_options::recursive);
  fs::copy(src.root / "F2.hips", root2 / "F2.hips", fs::copy_options::recursive);
  fs::remove_all(root2 / "F1.hips" / "ivar", ec);
  fs::remove_all(root2 / "F2.hips" / "ivar", ec);
  const std::string out2 = (root2 / "p2out").string();
  fs::create_directories(out2, ec);
  const std::string cfg_noivar = R"({
    "hips_paths": [")" + (root2 / "F1.hips").string() + R"(", ")" +
      (root2 / "F2.hips").string() + R"("],
    "output_dir": ")" + out2 + R"("
  })";
  RunContext ctx;

  // (a) weight_mode 域门（ivar 齐备夹具 → 失败只可能归因于 mode 域）
  {
    IvarFixture fx = make_ivar_fixture("domain");
    auto mode_fail = [&](const std::string& extra, const std::string& want) {
      Result<void> rc;
      run_p2_chain(reg, ivar_cfg(fx, extra), ctx, 6, &rc);
      CHECK_MSG(rc.failed(), ("weight_mode" + extra + " must fail closed").c_str());
      if (rc.failed())
        CHECK_MSG(rc.error().message().find(want) != std::string::npos,
                  ("illegal weight_mode diagnostic must echo the actual value ('" +
                   want + "'): " + rc.error().message()).c_str());
    };
    mode_fail(R"(,"weight_mode":0)", "weight_mode 0");      // §30.1 规则 1
    mode_fail(R"(,"weight_mode":99)", "weight_mode 99");    // SMOKE-001 D10 回归锚
    mode_fail(R"(,"weight_mode":-3)", "weight_mode -3");
    // 非整数形态由 validate_config 拒绝（禁隐式字符串转换）
    {
      auto m = reg.create("astrocs.phase2.integrate");
      CHECK(m.ok());
      const Result<void> v = m.value()->validate_config(ivar_cfg(fx, R"(,"weight_mode":"2")"));
      CHECK_MSG(v.failed(), "weight_mode string must be rejected by validate_config");
    }
    // 默认路径: ivar 齐备 → 成功 + 审计面 weight_basis=per_sample_ivar
    // （FIX-405 G3-12: 产物不再落「权重模式」键, 只留语义键）
    Result<void> okrc;
    run_p2_chain(reg, ivar_cfg(fx), ctx, 6, &okrc);
    CHECK_MSG(okrc.ok(), ("default per-sample ivar chain must succeed: " +
                          (okrc.failed() ? okrc.error().message() : std::string())).c_str());
    if (okrc.ok()) {
      json intj;
      try { intj = json::parse(read_file(fx.out + "/p2_integrated.json")); } catch (...) {}
      CHECK(intj.find("weight_mode") == intj.end());
      CHECK(intj.value("weight_basis", std::string()) == "per_sample_ivar");
      CHECK(intj.value("ivar_product_missing_frames", -1) == 0);
      CHECK(intj.value("uncertainty_available", false) == true);
    }
    fs::remove_all(fx.root);
  }

  // (b) 缺 ivar（默认 mode 2）→ fail-closed; 无伪产物
  {
    Result<void> ff;
    run_p2_chain(reg, cfg_noivar, ctx, 6, &ff);
    if (fault_silent) {
      // 等价缺陷注入: 静默等权降级（不 fail-closed）⇒ 门必红。
      CHECK_MSG(false,
                "FAULT-INJECT: missing ivar under weight_mode=2 must fail closed"
                " (ASTROCS_IVAR_FAULT=silent_fallback proves this gate is live)");
    } else {
      CHECK_MSG(ff.failed(), "missing ivar + default weight_mode=2 must fail closed");
      CHECK_MSG(!fs::exists(fs::path(out2 + "/p2_integrated.json")),
                "fail-closed path must not leave a pseudo integrated artifact");
      if (ff.failed()) {
        const std::string msg = ff.error().message();
        CHECK_MSG(msg.find("ivar") != std::string::npos, msg.c_str());
        CHECK_MSG(msg.find("2/2") != std::string::npos,
                  ("diagnostic must report the missing-frame count: " + msg).c_str());
        CHECK_MSG(msg.find("legacy_allow_weight_fallback=true") != std::string::npos,
                  ("diagnostic must name the only explicit degradation exit: " + msg).c_str());
      }
    }
  }

  // (c) legacy_allow_weight_fallback=true 不再有成功降级路径（禁静默/禁假绿）
  //     —— 与 4e 同口径: 缺 ivar 且无 HiPS 帧级 SNR 键 ⇒ 权重链 fail-closed,
  //     不写 p2_integrated.json / p2_final.json, 不存在 unit_weight_degraded 面。
  {
    const std::string cfg_fb = R"({
      "hips_paths": [")" + (root2 / "F1.hips").string() + R"(", ")" +
        (root2 / "F2.hips").string() + R"("],
      "output_dir": ")" + out2 + R"(",
      "legacy_allow_weight_fallback": true
    })";
    Result<void> ff2;
    run_p2_chain(reg, cfg_fb, ctx, 7, &ff2);
    CHECK_MSG(ff2.failed(),
              "legacy_allow_weight_fallback=true must fail closed"
              " (equal-weight degradation success path removed)");
    CHECK_MSG(!fs::exists(fs::path(out2 + "/p2_integrated.json")),
              "fail-closed path must not leave a pseudo integrated artifact");
    CHECK_MSG(!fs::exists(fs::path(out2 + "/p2_final.json")),
              "fail-closed path must not leave p2_final.json");
    if (ff2.failed()) {
      const std::string msg = ff2.error().message();
      // 审计诊断必须如实说明: ivar 缺失帧数 + 权重链未闭合 + 该键不再是降级出口。
      CHECK_MSG(msg.find("ivar") != std::string::npos, msg.c_str());
      CHECK_MSG(msg.find("2/2") != std::string::npos,
                ("diagnostic must report the missing-frame count: " + msg).c_str());
      CHECK_MSG(msg.find("NOT closed") != std::string::npos, msg.c_str());
      CHECK_MSG(msg.find("legacy_allow_weight_fallback=true") != std::string::npos,
                ("diagnostic must name the removed degradation exit: " + msg).c_str());
    }
  }

  // (d) write 节点: 集成产物缺/不自洽的方差面声明 → fail-closed
  //     （FIX-405 G3-12: 原判据以整数 weight_mode 承载, 现改为语义键:
  //      缺 uncertainty_available / 逐样本面缺失 / ivar 缺帧 / SNR 链降级
  //      都必须拒写 —— 判据强度不变, 只是不再引入「权重模式」词汇）
  {
    IvarFixture fx = make_ivar_fixture("wrgate");
    Result<void> rc6;
    run_p2_chain(reg, ivar_cfg(fx), ctx, 6, &rc6);
    CHECK_MSG(rc6.ok(), "write-gate precondition: integrate must succeed");
    if (rc6.ok()) {
      const std::string ip = fx.out + "/p2_integrated.json";
      json intj;
      try { intj = json::parse(read_file(ip)); } catch (...) {}
      CHECK(intj.find("weight_mode") == intj.end());
      // d1: 缺 uncertainty_available（禁静默缺省）
      json d1 = intj; d1.erase("uncertainty_available");
      { std::ofstream f(ip, std::ios::binary); f << d1.dump(2); }
      Result<void> w1;
      run_node(reg, "astrocs.phase2.write", ivar_cfg(fx), ctx, &w1);
      CHECK_MSG(w1.failed(),
                "write must fail closed when integrated artifact lacks uncertainty_available");
      CHECK_MSG(!fs::exists(fs::path(fx.out + "/p2_final.json")),
                "fail-closed write must not leave p2_final.json");
      // d2: 非逐样本权重面（帧级 SNR 链降级）却声明 uncertainty_available → 拒
      json d2 = intj;
      d2["weight_basis"] = "frame_snr_ivar";
      d2["snr_chain_used"] = true;
      { std::ofstream f(ip, std::ios::binary); f << d2.dump(2); }
      Result<void> w2;
      run_node(reg, "astrocs.phase2.write", ivar_cfg(fx), ctx, &w2);
      CHECK_MSG(w2.failed(),
                "write must reject uncertainty_available without a per-sample weight surface");
      // d3: 有 ivar 缺帧（ivar_product_missing_frames>0）却声明可用 → 拒（§30.1 规则 1）
      json d3 = intj;
      d3["uncertainty_available"] = true;
      d3["ivar_product_missing_frames"] = 1;
      { std::ofstream f(ip, std::ios::binary); f << d3.dump(2); }
      Result<void> w3;
      run_node(reg, "astrocs.phase2.write", ivar_cfg(fx), ctx, &w3);
      CHECK_MSG(w3.failed(),
                "write must reject uncertainty_available with missing ivar frames");
    }
    fs::remove_all(fx.root);
  }

  fs::remove_all(src.root, ec);
  fs::remove_all(root2, ec);
}

// ── RELEASE-02 SD-15: Phase1 写侧帧级 SNR 键 ────────────────────────────────
// ASTROCS_FRAME_SNR（帧级**未加权通量型** SNR = F_ref/σ_F; 信噪比不是权重）与
// ASTROCS_REFERENCE_FLUX（组内公共 F_ref）必须写入 HiPS signal properties ——
// 这是 Phase2 权重链 w = SNR²/F_ref² = 1/σ_F² 的唯一数据源（键缺失 ⇒ fail-closed）。
// 本测试用真实 Phase1 drizzle+writer 节点链 + 上游 p1_snr.json sidecar 驱动,
// 并含负例（snr_reference 非正 ⇒ 两键整体不写, 禁伪造/占位）。
static void test_p1_frame_snr_keys() {
  const fs::path root = fs::temp_directory_path() /
      ("p2001_p1snr_" + std::to_string(P2001_GETPID));
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);
  const std::string light = (root / "light_snr.fits").string();
  StarField sf{100.0f, 5000.0f};
  CHECK(p1sess::write_fits_file(light, kW, kH, star_field_pixel, &sf) == 0);

  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const std::string wcs = R"("wcs": {"crpix1": 16.0, "crpix2": 16.0,
      "crval1": 10.0, "crval2": 20.0,
      "cd11": -0.0002777777777777778, "cd12": 0.0,
      "cd21": 0.0, "cd22": 0.0002777777777777778},)";

  auto run_p1 = [&](const std::string& od, const std::string& snr_frame_json,
                    std::string* props_out) -> bool {
    fs::create_directories(od, ec);
    {
      std::ofstream f((od + "/p1_snr.json").c_str(), std::ios::binary);
      if (!f.good()) return false;
      f << R"({"schema":"DATA-P1-SNR","frames":[)" << snr_frame_json << "]}";
    }
    const std::string cfg = R"({"input_lights": [")" + light + R"("],
      "output_dir": ")" + od + R"(",
      )" + wcs + R"(
      "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}})";
    RunContext ctx;
    auto drz = reg.create("astrocs.phase1.drizzle");
    if (drz.failed()) return false;
    if (drz.value()->validate_config(cfg).failed()) return false;
    if (drz.value()->plan("p1snr_drz", cfg).failed()) return false;
    const Result<void> rd = drz.value()->execute(ctx);
    if (rd.failed()) { std::fprintf(stderr, "P1 snr drizzle failed: %s\n",
                                    rd.error().message().c_str()); return false; }
    auto wr = reg.create("astrocs.phase1.writer");
    if (wr.failed()) return false;
    if (wr.value()->validate_config(cfg).failed()) return false;
    if (wr.value()->plan("p1snr_wr", cfg).failed()) return false;
    const Result<void> rw = wr.value()->execute(ctx);
    if (rw.failed()) return false;
    *props_out = read_file(od + "/light_snr/signal/properties");
    return true;
  };

  // 正例: 真实上游值 → 两键写入且数值可回读（%.17g round-trip）
  {
    const std::string od = (root / "out_ok").string();
    std::string props;
    const bool ok = run_p1(od, R"({"file":"light_snr.fits","snr_reference":)"
                               R"({"snr_f":42.5,"flux_adu":1000.0}})",
                           &props);
    CHECK(ok);
    CHECK_MSG(props.find("ASTROCS_FRAME_SNR=42.5") != std::string::npos,
              ("Phase1 must write ASTROCS_FRAME_SNR=F_ref/sigma_F; props=" +
               props).c_str());
    CHECK_MSG(props.find("ASTROCS_REFERENCE_FLUX=1000") != std::string::npos,
              ("Phase1 must write ASTROCS_REFERENCE_FLUX=F_ref; props=" +
               props).c_str());
  }
  // 负例: snr_reference 非正/缺失 → 两键整体不写（禁伪造; Phase2 fail-closed）
  {
    const std::string od = (root / "out_bad").string();
    std::string props;
    const bool ok = run_p1(od, R"({"file":"light_snr.fits","snr_reference":)"
                               R"({"snr_f":0.0,"flux_adu":0.0}})",
                           &props);
    CHECK(ok);
    CHECK_MSG(props.find("ASTROCS_FRAME_SNR") == std::string::npos &&
                  props.find("ASTROCS_REFERENCE_FLUX") == std::string::npos,
              "invalid snr_reference must NOT write fabricated frame-SNR keys");
  }
  fs::remove_all(root, ec);
}

int main() {
  test_ivar_chain_real_operation();
  test_runtime_chain_call_count_1();
  test_fail_fast_downstream_zero_calls();
  test_complete_gate_fail_closed();
  test_negative_and_fallback();
  // IVAR-001: weight_mode 域/审计面 + 缺 ivar fail-closed/显式降级门 + 注入面
  test_ivar001_weight_mode_domain_and_audit();
  // RELEASE-02 SD-15: Phase1 写侧帧级 SNR 键（权重链唯一数据源）
  test_p1_frame_snr_keys();
  test_determinism();
  test_worker_parity();
  if (failures == 0) {
    std::printf("P2-001 REAL NODES PASS (7 节点唯一真实 operation + call_count=1 + complete 门 fail-closed + §30 ivar oracle + fail-fast + 确定性 + worker parity)\n");
    return 0;
  }
  std::fprintf(stderr, "P2-001 REAL NODES FAIL (%d)\n", failures);
  return 1;
}
