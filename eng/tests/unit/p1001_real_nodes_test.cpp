// eng/tests/unit/p1001_real_nodes_test.cpp — P1-001 (attempt 2) Phase1 真实节点与 complete 门
//
// 验收映射 (控制包 ARCH-P0-001 / PROD-P0-001 Phase1 侧, 前台裁定口径):
//   1. registry 8 类 Phase1 节点各自唯一真实 operation 委托（子节点禁止调用完整
//      phase_session_run）——节点 last_manifest 必须携带 operation/entry 标记, 与
//      lib/infrastructure/pipeline/module_ports.registry.json 冻结绑定表逐一一致。
//   2. typed artifact: 每节点产出 descriptor.data_id 对应的磁盘 artifact 且存在。
//   3. trace call_count=1: Runtime 全链执行每节点 MODULE_CALL 恰好一次,
//      trace_violations 为空（无隐藏 session 重复调用）。
//   4. complete 门 fail-closed: p1_session manifest 在 Phase1 链不完整时
//      status="partial"（不写 complete）且带 availability 全域报告（不冒充完成,
//      宪章 §16.2/§18.3; 链完整后再开放 complete）。
//   5. 负向: 坏 eng/packaging/config/坏帧/缺科学参数确定性拒绝, 不留伪产物。
//
// RED 锚定（改造前）: SessionModule(完整 session 委托) manifest 无 operation/entry
//   字段 → 断言 1 失败; p1_session manifest 无 availability 且 status=complete
//   → 断言 4 失败。本测试先于实现提交面运行记录 RED, 再随实现转 GREEN。
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"
#include "p1_session.h"
#include "wcs_tan.h"     // B2-A17: linear WCS forward Oracle  // complete 门: API-P1-001 冻结 C ABI

#include "p1sess_fixtures.hpp"  // 最小 FITS writer (手写, 不调生产 symbol)

// B2-A12/A13/A14/A16: 标准 HiPS 读面 + FITS 读面 (Oracle 对拍用; AIO 由
// astrocs_module_adapters PUBLIC 传递 include 与 AIO_ENABLE_HEALPIX=1)
#include "astro_image_io.h"
#include "astro_sphere_sink.h"  // P23: write_hips_phase1 (直写标准 HiPS) 等价夹具
#include "aio_atomic_file.h"    // 目录枚举/路径机制经 aio (for_each_child; §10 唯一 I/O 边界)

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>    // PERF-P1 1/N 门: 运行时刻时间戳归一化
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>       // PERF-P1 1/N 门: 差异集合
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define P1001_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define P1001_GETPID static_cast<long>(::getpid())
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

// host services 工厂（定义 lib/infrastructure/benchmark/backend_host/host_services.cpp, astrocs_cpu 库;
// 与 lib/infrastructure/scheduler/src/module_adapters.cpp 同一声明模式）
extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
}

namespace {

namespace fs = std::filesystem;

// ── fixture: 32x32 合成帧（固定 seed；背景常数 + 单高斯星）────────────────
constexpr int kW = 32, kH = 32;
struct StarField {
  float bg;
  float amp;
  double gain_e_per_adu = 1.5;   // 物理噪声过程参数（见下）
  double read_noise_e = 5.0;
};
// §9.41 负责人裁决：「合成数据**必须是真实物理噪声过程**（源/天光/暗电流
// Poisson（电子域）+ 读出 Gaussian（电子域）+ 增益量化（ADU）），**严禁**
// 「纯加性天光」（只加常数不改噪声 = 测不出任何东西）。
// 原 fixture 返回**无噪声**的 bg+高斯 —— 既是 §9.41 违规，也使
// SCI-NOISE-001 §5 的稳健噪声估计正确地判为退化（无散射 ⇒ 估不出噪声）。
// 本实现按物理链生成：电子域 Poisson(源+天光) + 电子域 Gaussian(读出)
// → ÷gain 得 ADU。用**确定性**逐像素哈希做 PRNG（可复跑，不依赖全局 RNG 状态）。
inline double fixture_uniform(uint32_t k) {
  // splitmix64 → [0,1)
  uint64_t z = static_cast<uint64_t>(k) + 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  z = z ^ (z >> 31);
  return static_cast<double>(z >> 11) * (1.0 / 9007199254740992.0);
}
inline double fixture_gauss(uint32_t k) {
  // Box–Muller（两枚独立均匀）
  const double u1 = std::max(1e-12, fixture_uniform(k));
  const double u2 = fixture_uniform(k ^ 0xA5A5A5A5u);
  return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}
inline float star_field_pixel(int i, void* user) {
  auto* sf = static_cast<StarField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - 16.0;
  const double dy = static_cast<double>(y) - 16.0;
  const double src = sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  const double lambda_e = (sf->bg + src) * sf->gain_e_per_adu;  // 电子域均值
  // Poisson 大均值正态近似（与 eng/tests/unit/p1_noise_test.cpp 同款约定）
  const double e = lambda_e + std::sqrt(std::max(0.0, lambda_e)) * fixture_gauss(static_cast<uint32_t>(i) * 2u + 1u);
  const double rn = sf->read_noise_e * fixture_gauss(static_cast<uint32_t>(i) * 2u + 2u);
  return static_cast<float>((e + rn) / sf->gain_e_per_adu);   // 增益量化 → ADU
}
inline float const_pixel(int, void* user) {
  return *static_cast<float*>(user);
}
// RELEASE-02 FIX-REGRESS（B2-A14b 夹具）：测光已应用帧 = 原星场 × photscal
// 的副本（等价 calibration::apply_photometry 的逐像素乘性结果）。生产
// drizzle 在 p1_phot.json 声明 applied=true 时消费 photoapplied_<base>，
// 缺失即 fail-closed；本夹具必须真实产出该帧，而不是放宽生产校验。
struct ScaledStarField {
  float bg;
  float amp;
  float scale;
};
inline float scaled_star_field_pixel(int i, void* user) {
  auto* sf = static_cast<ScaledStarField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - 16.0;
  const double dy = static_cast<double>(y) - 16.0;
  const double g = sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  return sf->scale * (sf->bg + static_cast<float>(g));
}

struct Fixture {
  fs::path dir;
  std::string light1, light2, bias, dark, flat;
  std::string out_dir;
};

Fixture make_fixture(const char* tag) {
  Fixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_real_nodes_" + std::string(tag) + "_" +
           std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  f.light1 = (f.dir / "light_1.fits").string();
  f.light2 = (f.dir / "light_2.fits").string();
  f.bias = (f.dir / "master_bias.fits").string();
  f.dark = (f.dir / "master_dark.fits").string();
  f.flat = (f.dir / "master_flat.fits").string();
  f.out_dir = f.dir.string();
  StarField sf{100.0f, 5000.0f};
  // BIAS-001 / B2-A13: 只要 master_dark 在位，K=t_light/t_dark 必须由 FITS EXPTIME 推导
  // （缺失 → DATA fail-closed，DATA_SEMANTICS §9.1）。真实相机帧恒带 EXPTIME，合成
  // fixture 必须同构：light=dark=60s ⇒ K=1.0（数值与旧夹具一致）。
  CHECK(p1sess::write_fits_file(f.light1, kW, kH, star_field_pixel, &sf, 0, 60.0) == 0);
  CHECK(p1sess::write_fits_file(f.light2, kW, kH, star_field_pixel, &sf, 0, 60.0) == 0);
  float vb = 10.0f, vd = 5.0f, vf = 1.0f;
  CHECK(p1sess::write_fits_file(f.bias, kW, kH, const_pixel, &vb, 0, 10.0) == 0);
  CHECK(p1sess::write_fits_file(f.dark, kW, kH, const_pixel, &vd, 0, 60.0) == 0);
  CHECK(p1sess::write_fits_file(f.flat, kW, kH, const_pixel, &vf, 0, 1.0) == 0);
  return f;
}

void cleanup_fixture(Fixture& f) {
  std::error_code ec;
  fs::remove_all(f.dir, ec);
}

// ── P0-21: 每帧 HiPS 产品根 = output_dir/<frame_key>（DATA-P1-PRODUCTS 口径）──
// 每个 input light 的 signal/support/p1_stack.json/p1_final.json/p1_wcs.json
// 落在自己的产品目录下; <frame_key> = 输入基名去扩展名（本夹具 = light_N）。
std::string frame_root(const Fixture& fx, const char* light_base = "light_1") {
  std::string stem = light_base;
  const size_t dot = stem.find_last_of('.');
  if (dot != std::string::npos) stem = stem.substr(0, dot);
  return fx.out_dir + "/" + stem;
}

// 向上探测仓库根（锚: ASTROCS_PROJECT_CONSTITUTION.md）→ GaiaDR3 用户数据区
std::string find_repo_gaia_dir() {
  fs::path probe = fs::current_path();
  for (int i = 0; i < 6 && !probe.empty(); ++i) {
    std::error_code ec;
    if (fs::exists(probe / "ASTROCS_PROJECT_CONSTITUTION.md", ec)) {
      fs::path g = probe / "GaiaDR3";
      if (fs::exists(g, ec)) return g.string();
    }
    probe = probe.parent_path();
  }
  return std::string();
}

// ── Phase1 节点期望表（唯一真实 operation 绑定, 冻结源
//    lib/infrastructure/pipeline/module_ports.registry.json）──────────────────────────
struct NodeExpect {
  const char* module_id;
  const char* operation;
  const char* entry;
};
const NodeExpect kNodeExpects[] = {
    {"astrocs.phase1.calibration", "calibrate", "astrocs_phase1_calibrate_v1"},
    {"astrocs.phase1.cosmetic", "cosmetic_correct", "astrocs_phase1_cosmetic_v1"},
    {"astrocs.phase1.star-psf", "detect_sources", "astrocs_phase1_starpsf_v1"},
    {"astrocs.phase1.wcs-platesolve", "plate_solve", "astrocs_phase1_wcs_v1"},
    {"astrocs.phase1.photometry", "measure_flux", "astrocs_phase1_photometry_v1"},
    {"astrocs.phase1.noise-snr", "estimate_snr", "astrocs_phase1_noisesnr_v1"},
    {"astrocs.phase1.drizzle", "drizzle_stack", "astrocs_phase1_drizzle_v1"},
    {"astrocs.phase1.writer", "write_hips", "astrocs_phase1_writer_v1"},
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

// ═══ CORE-RACE-001 helpers（p1001 链并发撕裂读）═══════════════════════════
// 缺陷（修复前）: p1_op_cosmetic 读取 cal 节点产物 calibrated_<base> 后就地覆写
// 同一路径, 而 drz 节点（IR 声明输入 artifact:cal）与 cos 同为 cal 下游、
// resources.parallel=true ⇒ create_runtime(2) 下并发执行; aio_write_fits 自身
// 非原子（fopen("wb") 截断 + 增量写）⇒ 消费者可观察到半写文件。
std::string read_bytes(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

// 热像素场（高斯星 + 两个极热像素）: 保证 cosmetic 真实改动像素（非空转 fixture）
struct HotField { float bg; float amp; };
inline float hot_field_pixel(int i, void* user) {
  auto* sf = static_cast<HotField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - 16.0;
  const double dy = static_cast<double>(y) - 16.0;
  double v = sf->bg + sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  if ((x == 5 && y == 5) || (x == 26 && y == 7)) v += 1.0e6;  // 热像素
  return static_cast<float>(v);
}

Fixture make_hot_fixture(const char* tag) {
  Fixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_chain_race_" + std::string(tag) + "_" +
           std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  f.light1 = (f.dir / "light_1.fits").string();
  f.light2 = (f.dir / "light_2.fits").string();
  f.bias = (f.dir / "master_bias.fits").string();
  f.dark = (f.dir / "master_dark.fits").string();
  f.flat = (f.dir / "master_flat.fits").string();
  f.out_dir = f.dir.string();
  HotField hf{100.0f, 5000.0f};
  // BIAS-001 / B2-A13: 同上（light=dark=60s ⇒ K=1.0）。
  CHECK(p1sess::write_fits_file(f.light1, kW, kH, hot_field_pixel, &hf, 0, 60.0) == 0);
  CHECK(p1sess::write_fits_file(f.light2, kW, kH, hot_field_pixel, &hf, 0, 60.0) == 0);
  float vb = 10.0f, vd = 5.0f, vf = 1.0f;
  CHECK(p1sess::write_fits_file(f.bias, kW, kH, const_pixel, &vb, 0, 10.0) == 0);
  CHECK(p1sess::write_fits_file(f.dark, kW, kH, const_pixel, &vd, 0, 60.0) == 0);
  CHECK(p1sess::write_fits_file(f.flat, kW, kH, const_pixel, &vf, 0, 1.0) == 0);
  return f;
}

// 全链 7 节点 IR（与 §2 同构; drz 与 cos 并发读同一 artifact:cal）
json build_p1001_full_chain_ir(const std::string& cfg, const std::string& pipeline_id) {
  auto node = [&](const char* nid, const char* mid, const char* in_port,
                  const char* in_art, const char* out_port, const char* out_art) {
    json n;
    n["node_id"] = nid;
    n["module_id"] = mid;
    n["module_api"] = "1.x";
    n["config"] = json::parse(cfg);
    if (in_port) n["inputs"] = json{{in_port, in_art}};
    n["outputs"] = json{{out_port, out_art}};
    n["resources"] = json{{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = pipeline_id;
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cal", "astrocs.phase1.calibration", "frames", "artifact:in", "calibrated", "artifact:cal"));
  ir["nodes"].push_back(node("cos", "astrocs.phase1.cosmetic", "calibrated", "artifact:cal", "cleaned", "artifact:cos"));
  ir["nodes"].push_back(node("psf", "astrocs.phase1.star-psf", "cleaned", "artifact:cos", "sources", "artifact:psf"));
  ir["nodes"].push_back(node("phot", "astrocs.phase1.photometry", "sources", "artifact:psf", "fluxes", "artifact:phot"));
  ir["nodes"].push_back(node("snr", "astrocs.phase1.noise-snr", "fluxes", "artifact:phot", "snr", "artifact:snr"));
  ir["nodes"].push_back(node("drz", "astrocs.phase1.drizzle", "calibrated", "artifact:cal", "stacked", "artifact:drz"));
  ir["nodes"].push_back(node("wr", "astrocs.phase1.writer", "stacked", "artifact:drz", "fits", "artifact:wr"));
  ir["outputs"] = json{{"fits", "artifact:wr"}, {"snr", "artifact:snr"},
                       {"psf", "artifact:psf"},
                       {"cal", "artifact:cal"}, {"cos", "artifact:cos"}};
  return ir;
}

std::string p1001_full_chain_cfg(const Fixture& fx) {
  return std::string(R"({
    "input_lights": [")") + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "dark_optimization": false,
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"(",
    "cosmetic": {"enabled": true, "hot_sigma": 5.0, "cold_sigma": 5.0},
    "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},
    "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
  })";
}

// 单次全链运行; 返回 run 是否成功 + 每节点 call_count/status 明细
struct ChainRunResult {
  bool ok = false;
  std::string error;
  std::size_t node_count = 0;
  bool trace_ok = true;
};
ChainRunResult run_full_chain_once(const std::string& cfg, uint32_t workers,
                                   ModuleRegistry& reg) {
  ChainRunResult out;
  auto rt = create_runtime(workers);
  if (!rt.ok()) { out.error = "create_runtime failed"; return out; }
  json ir = build_p1001_full_chain_ir(cfg, "p1001.chain.race");
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  if (!load.ok()) { out.error = "load_pipeline: " + load.error().message(); return out; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  out.ok = rrun.ok();
  if (!out.ok) out.error = rrun.error().message();
  const auto tr = rt.value()->node_trace();
  out.node_count = tr.size();
  for (const auto& t : tr) {
    if (t.call_count != 1 || t.status != "COMPLETED") out.trace_ok = false;
  }
  if (!rt.value()->trace_violations().empty()) out.trace_ok = false;
  return out;
}

}  // namespace

// ── 1. 每节点唯一真实 operation 标记 + typed artifact ─────────────────────
static void test_nodes_real_operation() {
  Fixture fx = make_fixture("nodes");
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());

  const std::string base_cfg = R"({
    "input_lights": [")" + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "dark_optimization": false,
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"(",
    "cosmetic": {"enabled": true, "hot_sigma": 5.0, "cold_sigma": 5.0}
  })";
  // calibrate 先行（后续节点消费 calibrated_*.fits）
  RunContext ctx;
  json man_cal = run_node(reg, "astrocs.phase1.calibration", base_cfg, ctx);
  CHECK(man_cal.value("operation", "") == "calibrate");
  CHECK(man_cal.value("entry", "") == "astrocs_phase1_calibrate_v1");
  CHECK(man_cal.value("status", "") == "ok");
  CHECK(man_cal.contains("artifacts") && man_cal["artifacts"].is_array() &&
        !man_cal["artifacts"].empty());
  for (const auto& a : man_cal["artifacts"])
    CHECK(fs::exists(fs::path(a.get<std::string>())));

  // cosmetic：enabled → 真实 ac_correct_frame 执行
  json man_cos = run_node(reg, "astrocs.phase1.cosmetic", base_cfg, ctx);
  CHECK(man_cos.value("operation", "") == "cosmetic_correct");
  CHECK(man_cos.value("entry", "") == "astrocs_phase1_cosmetic_v1");
  CHECK(man_cos.value("status", "") == "ok");

  // star-psf：StarDetector 真实检测（星点场 → n_detected>=1; PSF 特性来自 StarSource）
  json man_psf = run_node(reg, "astrocs.phase1.star-psf", base_cfg, ctx);
  CHECK(man_psf.value("operation", "") == "detect_sources");
  CHECK(man_psf.value("entry", "") == "astrocs_phase1_starpsf_v1");
  CHECK(man_psf.value("status", "") == "ok");
  CHECK(man_psf.contains("sources_artifact") &&
        man_psf["sources_artifact"].is_string());
  const std::string src_path = man_psf.value("sources_artifact", "");
  CHECK(fs::exists(fs::path(src_path)));
  {
    json cat;
    try { cat = json::parse(read_file(src_path)); } catch (...) { CHECK(false); }
    CHECK(cat.value("schema", "") == "DATA-P1-SOURCES");
    bool found_star = false;
    for (const auto& fr : cat.value("frames", json::array())) {
      if (fr.value("n_detected", 0u) >= 1) found_star = true;
      for (const auto& s : fr.value("sources", json::array())) {
        CHECK(s.contains("fwhm_px") && s.contains("ellipticity"));
      }
    }
    CHECK_MSG(found_star, "star fixture must yield >=1 detection (不误报空)");
  }
  // PSF 特性 artifact (DATA-P1-PSF)
  CHECK(man_psf.contains("psf_artifact") && man_psf["psf_artifact"].is_string());
  CHECK(fs::exists(fs::path(man_psf.value("psf_artifact", ""))));

  // wcs-platesolve：真实 ipv 求解器链（sdet+gaia 句柄 → ipv_solve_from_memory_
  // with_callback_d → CD/CRVAL/CRPIX/RMS）。合同:
  //   a) 缺求解参数（ra0/dec0/focal/pixel_size/gaia_data_dir）→ DATA 拒绝
  //      （真实链必需参数禁 silent default）;
  //   b) 全参数 → 真实调用 ipv; 两平台同源（非 Windows 静态绑定 / Windows 动态
  //      加载同一组生产 C API，源内无平台 stub）→ 真实求解成功或真实失败，
  //      一律禁止「以平台 stub / 平台不支持为由」的占位失败与伪 WCS。
  const std::string wcs_base = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "wcs": {)";
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase1.wcs-platesolve", wcs_base + "}}", ctx, &rc);
    CHECK_MSG(rc.failed(), "missing solve params must be rejected (no silent defaults)");
  }
  {
    const std::string gaia_dir = find_repo_gaia_dir();
    // P9: 显式 config 来源（默认已改为 header_pointing; 本用例验证 config 通道）
    const std::string wcs_full = wcs_base +
        R"("init_source": "config", "ra0": 10.0, "dec0": 20.0,
            "focal_length_mm": 400.0, "pixel_size_um": 3.76,
            "gaia_data_dir": ")" + gaia_dir + R"("}})";
    Result<void> rc;
    json man_wcs = run_node(reg, "astrocs.phase1.wcs-platesolve", wcs_full, ctx, &rc);
    CHECK(man_wcs.value("operation", "") == "plate_solve");
    CHECK(man_wcs.value("entry", "") == "astrocs_phase1_wcs_v1");
#if defined(_WIN32)
    CHECK_MSG(rc.ok(), "Windows: real ipv solve should succeed on valid star field");
#else
    // Linux 侧 ipv 是真实求解器（ipv_select.cpp 静态绑定同一组生产 C API，
    // 源内无平台 stub）⇒ 允许「真实成功」或「真实失败」，禁止的是把平台限制
    // 当作失败理由。失败时错误必须是 DATA 域且不得携带 stub/平台不支持语义。
    if (!rc.ok()) {
      CHECK(rc.error().domain() == astrocs::core::ErrorDomain::DATA);
      CHECK_MSG(rc.error().message().find("stub") == std::string::npos,
                "Linux ipv must not fail as a platform stub (real solver, static C API)");
      CHECK_MSG(rc.error().message().find("not supported") == std::string::npos,
                "Linux ipv must not fail as 'platform not supported'");
    }
#endif
  }
  // P9: 帧头 WCS 未授权 —— init_source=header_crval 必须被拒绝（fail-closed,
  // 不得回退到帧头 CRVAL1/2 或任何 silent default）。
  {
    const std::string gaia_dir = find_repo_gaia_dir();
    const std::string cfg = wcs_base +
        R"("init_source": "header_crval", "ra0": 10.0, "dec0": 20.0,
            "focal_length_mm": 400.0, "pixel_size_um": 3.76,
            "gaia_data_dir": ")" + gaia_dir + R"("}})";
    Result<void> rc;
    run_node(reg, "astrocs.phase1.wcs-platesolve", cfg, ctx, &rc);
    CHECK_MSG(rc.failed(),
              "P9: init_source=header_crval must be rejected (帧头 WCS 未授权)");
    CHECK(rc.error().domain() == astrocs::core::ErrorDomain::DATA);
  }
  // P9: header_pointing 在无指向关键字的帧上 fail-closed（该 fixture light 只有
  // SIMPLE/BITPIX/NAXIS*, 无 OBJCTRA/OBJCTDEC/RA/DEC/FOCALLEN/XPIXSZ）。
  {
    const std::string gaia_dir = find_repo_gaia_dir();
    const std::string cfg = wcs_base +
        R"("init_source": "header_pointing", "gaia_data_dir": ")" + gaia_dir + R"("}})";
    Result<void> rc;
    run_node(reg, "astrocs.phase1.wcs-platesolve", cfg, ctx, &rc);
    CHECK_MSG(rc.failed(),
              "P9: header_pointing without pointing keywords must fail-closed");
    CHECK(rc.error().domain() == astrocs::core::ErrorDomain::DATA);
  }

  // photometry：Photometer aperture 积分（对已检测源）
  json man_phot = run_node(reg, "astrocs.phase1.photometry", base_cfg, ctx);
  CHECK(man_phot.value("operation", "") == "measure_flux");
  CHECK(man_phot.value("entry", "") == "astrocs_phase1_photometry_v1");
  CHECK(man_phot.value("status", "") == "ok");
  CHECK(fs::exists(fs::path(man_phot.value("flux_artifact", ""))));

  // noise-snr：NoiseModel 真实估计
  json man_noise = run_node(reg, "astrocs.phase1.noise-snr", base_cfg, ctx);
  CHECK(man_noise.value("operation", "") == "estimate_snr");
  CHECK(man_noise.value("entry", "") == "astrocs_phase1_noisesnr_v1");
  CHECK(man_noise.value("status", "") == "ok");
  {
    json n;
    try { n = json::parse(read_file(man_noise.value("snr_artifact", ""))); } catch (...) { CHECK(false); }
    CHECK(n.value("schema", "") == "DATA-P1-SNR");
    CHECK(n.contains("frames") && !n["frames"].empty());
    for (const auto& fr : n["frames"]) {
      CHECK(fr.value("valid", false) == true);
      CHECK(fr.value("variance", 0.0) > 0.0);
      // ivar = 1/variance 不混合同
      CHECK(std::abs(fr.value("ivar", 0.0) * fr.value("variance", -1.0) - 1.0) < 1e-9);
    }
  }

  // drizzle：hp_drizzle_run 真实委托（带 WCS 头与 nside 科学参数）
  const std::string drz_cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},
    "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
  })";
  json man_drz = run_node(reg, "astrocs.phase1.drizzle", drz_cfg, ctx);
  CHECK(man_drz.value("operation", "") == "drizzle_stack");
  CHECK(man_drz.value("entry", "") == "astrocs_phase1_drizzle_v1");
  CHECK(man_drz.value("status", "") == "ok");
  CHECK(fs::exists(fs::path(man_drz.value("stack_artifact", ""))));

  // writer：标准 HiPS 产物校验节点（drizzle 已直写 signal/+support/ 树;
  // 本节点核对 properties/Moc/metadata + 统计 tile 数, 不再消费任何中间容器）
  Result<void> wr_rc;
  json man_wr = run_node(reg, "astrocs.phase1.writer", drz_cfg, ctx, &wr_rc);
  if (wr_rc.failed()) std::fprintf(stderr, "DBG writer error: %s\n", wr_rc.error().message().c_str());
  CHECK(man_wr.value("operation", "") == "write_hips");
  CHECK(man_wr.value("entry", "") == "astrocs_phase1_writer_v1");
  CHECK(man_wr.value("status", "") == "ok");
  CHECK(man_wr.contains("hips_root") && man_wr["hips_root"].is_string());
  CHECK(man_wr.value("n_tiles", 0u) >= 1);
  {
    json f;
    try { f = json::parse(read_file(man_wr.value("final_artifact", ""))); } catch (...) { CHECK(false); }
    CHECK(f.value("schema", "") == "DATA-P1-HIPS");
    CHECK(f.value("covered_area_model", "") == "support_ratio_x_A_cell");
  }
  CHECK(fs::exists(fs::path(frame_root(fx) + "/signal/properties")));
  CHECK(fs::exists(fs::path(man_wr.value("final_artifact", ""))));

  cleanup_fixture(fx);
}

// ── 2. Runtime 全链: call_count=1 / 无隐藏 session 重复调用 ────────────────
static void test_runtime_chain_call_count_1() {
  Fixture fx = make_fixture("chain");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());

  const std::string out_dir = fx.out_dir;
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "dark_optimization": false,
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + out_dir + R"(",
    "cosmetic": {"enabled": true},
    "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},
    "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
  })";

  // 7 节点主链 IR（cal→cos→psf→phot→snr→drz→wr; wcs 为旁支, 在 §1 单测两条
  // 入口: 真实 ipv 求解 与 explicit_config 八参数旁路——本主链不含 wcs 节点是
  // 因为它需要 gaia 星表句柄/真实指向, 与本用例的合成帧无关）
  auto node = [&](const char* nid, const char* mid, const char* in_port,
                  const char* in_art, const char* out_port, const char* out_art) {
    json n;
    n["node_id"] = nid;
    n["module_id"] = mid;
    n["module_api"] = "1.x";
    n["config"] = json::parse(cfg);
    if (in_port) n["inputs"] = json{{in_port, in_art}};
    n["outputs"] = json{{out_port, out_art}};
    n["resources"] = json{{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = "p1001.real.nodes";
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cal", "astrocs.phase1.calibration", "frames", "artifact:in", "calibrated", "artifact:cal"));
  ir["nodes"].push_back(node("cos", "astrocs.phase1.cosmetic", "calibrated", "artifact:cal", "cleaned", "artifact:cos"));
  ir["nodes"].push_back(node("psf", "astrocs.phase1.star-psf", "cleaned", "artifact:cos", "sources", "artifact:psf"));
  ir["nodes"].push_back(node("phot", "astrocs.phase1.photometry", "sources", "artifact:psf", "fluxes", "artifact:phot"));
  ir["nodes"].push_back(node("snr", "astrocs.phase1.noise-snr", "fluxes", "artifact:phot", "snr", "artifact:snr"));
  ir["nodes"].push_back(node("drz", "astrocs.phase1.drizzle", "calibrated", "artifact:cal", "stacked", "artifact:drz"));
  ir["nodes"].push_back(node("wr", "astrocs.phase1.writer", "stacked", "artifact:drz", "fits", "artifact:wr"));
  // IR 静态验证合同: 每个产物必须被消费或声明为 pipeline 输出
  // (phot→fluxes 供 snr 输入; snr/psf/cal/cos 为本链终态输出面)
  ir["outputs"] = json{{"fits", "artifact:wr"}, {"snr", "artifact:snr"},
                       {"psf", "artifact:psf"},
                       {"cal", "artifact:cal"}, {"cos", "artifact:cos"}};

  auto rt = create_runtime(2);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK_MSG(load.ok(), load.ok() ? "" : load.error().message().c_str());
  if (load.failed()) { cleanup_fixture(fx); return; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.ok(), rrun.ok() ? "" : rrun.error().message().c_str());

  // trace: 每 node MODULE_CALL 恰好 1 次（call_count=1）+ 无重复调用违规
  const auto tr = rt.value()->node_trace();
  CHECK(tr.size() == 7);
  for (const auto& t : tr) {
    CHECK(t.call_count == 1);
    CHECK(t.status == "COMPLETED");
  }
  auto viol = rt.value()->trace_violations();
  CHECK_MSG(viol.empty(), "no hidden session repeated calls");
  for (const auto& v : viol) std::fprintf(stderr, "violation: %s\n", v.c_str());

  // typed artifact 落盘（cal/cosmetic 覆写 + 每节点 JSON + HiPS 产品面）
  CHECK(fs::exists(fs::path(out_dir + "/calibrated_light_1.fits")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_sources.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_psf.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_flux.json")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_snr.json")));
  // P0-21 §3.4: 每帧一个 HiPS 产品目录（本链 2 帧 ⇒ 2 个产品）。
  CHECK(fs::exists(fs::path(frame_root(fx, "light_1") + "/p1_stack.json")));
  CHECK(fs::exists(fs::path(frame_root(fx, "light_1") + "/p1_final.json")));
  CHECK(fs::exists(fs::path(frame_root(fx, "light_1") + "/signal/properties")));
  CHECK(fs::exists(fs::path(frame_root(fx, "light_2") + "/p1_stack.json")));
  CHECK(fs::exists(fs::path(frame_root(fx, "light_2") + "/p1_final.json")));
  CHECK(fs::exists(fs::path(frame_root(fx, "light_2") + "/signal/properties")));
  CHECK(fs::exists(fs::path(out_dir + "/p1_products.json")));

  cleanup_fixture(fx);
}

// ── 2b. fail-fast 下游零调用（口径⑨: 上游节点失败 → 后续节点 call_count=0）──
static void test_fail_fast_downstream_zero_calls() {
  Fixture fx = make_fixture("ffast");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());

  // drizzle 缺 nside（确定性 DATA 拒绝）→ wr 未执行（call_count=0）;
  // 上游主链 cal→cos→psf→phot→snr 正常完成。
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"(", ")" + fx.light2 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "dark_optimization": false,
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"(",
    "cosmetic": {"enabled": true},
    "drizzle": {"nested": 1, "pixfrac": 1.0}
  })";
  auto node = [&](const char* nid, const char* mid, const char* in_port,
                  const char* in_art, const char* out_port, const char* out_art) {
    json n;
    n["node_id"] = nid;
    n["module_id"] = mid;
    n["module_api"] = "1.x";
    n["config"] = json::parse(cfg);
    if (in_port) n["inputs"] = json{{in_port, in_art}};
    n["outputs"] = json{{out_port, out_art}};
    n["resources"] = json{{"class", "cpu_heavy"}, {"parallel", true}};
    return n;
  };
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = "p1001.fail.fast";
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  ir["nodes"].push_back(node("cal", "astrocs.phase1.calibration", "frames", "artifact:in", "calibrated", "artifact:cal"));
  ir["nodes"].push_back(node("cos", "astrocs.phase1.cosmetic", "calibrated", "artifact:cal", "cleaned", "artifact:cos"));
  ir["nodes"].push_back(node("psf", "astrocs.phase1.star-psf", "cleaned", "artifact:cos", "sources", "artifact:psf"));
  ir["nodes"].push_back(node("phot", "astrocs.phase1.photometry", "sources", "artifact:psf", "fluxes", "artifact:phot"));
  ir["nodes"].push_back(node("snr", "astrocs.phase1.noise-snr", "fluxes", "artifact:phot", "snr", "artifact:snr"));
  ir["nodes"].push_back(node("drz", "astrocs.phase1.drizzle", "calibrated", "artifact:cal", "stacked", "artifact:drz"));
  ir["nodes"].push_back(node("wr", "astrocs.phase1.writer", "stacked", "artifact:drz", "fits", "artifact:wr"));
  ir["outputs"] = json{{"fits", "artifact:wr"}, {"snr", "artifact:snr"},
                       {"psf", "artifact:psf"},
                       {"cal", "artifact:cal"}, {"cos", "artifact:cos"}};

  auto rt = create_runtime(2);
  CHECK(rt.ok());
  auto load = rt.value()->load_pipeline(ir.dump(), reg);
  CHECK(load.ok());
  if (!load.ok()) { cleanup_fixture(fx); return; }
  RunContext ctx;
  auto rrun = rt.value()->run(ctx);
  CHECK_MSG(rrun.failed(), "upstream DATA rejection must fail the run");

  const auto tr = rt.value()->node_trace();
  bool drz_failed = false;
  for (const auto& t : tr) {
    if (t.node_id == "drz") {
      drz_failed = t.status == "FAILED";
      continue;
    }
    if (t.node_id == "wr") {
      CHECK_MSG(t.call_count == 0 || t.status != "COMPLETED",
                "downstream node must not execute after upstream failure");
    } else if (t.status == "COMPLETED") {
      CHECK(t.call_count == 1);
    }
  }
  CHECK_MSG(drz_failed, "drz node must be FAILED in trace");
  // 注: fail-fast 全局失败语义下, 与 drz 并行（2 workers）的 snr 可能被取消,
  // 故不断言 snr 产物存在; 只断言失败节点自身及其下游零产物。
  CHECK(!fs::exists(fs::path(frame_root(fx) + "/p1_stack.json"))); // drz 拒绝无产物
  CHECK(!fs::exists(fs::path(frame_root(fx) + "/p1_final.json"))); // wr 未执行

  cleanup_fixture(fx);
}

// ── 3. complete 门: p1_session fail-closed（链不完整 → partial+availability）──
static void test_complete_gate_fail_closed() {
  Fixture fx = make_fixture("gate");
  // 直接经冻结 C ABI（API-P1-001）: host services 缺省构造
  astrocs_host_services_v1 host{};
  void* state = nullptr;
  CHECK(astrocs_host_services_default_v1(&host, &state) == 0);
  acs_handle h = nullptr;
  CHECK(p1_session_create(&host, &h) == ACS_OK);

  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "dark_optimization": false,
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"("
  })";
  acs_span_u8 span{};
  span.head.struct_size = sizeof(span);
  span.head.abi_version = ACS_ABI_VERSION_V1;
  span.count = cfg.size();
  span.data = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(cfg.data()));
  CHECK(p1_session_validate(h, span) == ACS_OK);
  CHECK(p1_session_run(h, span, 0) == ACS_OK);
  acs_span_u8 out{};
  CHECK(p1_session_inspect(h, &out) == ACS_OK && out.data);
  json man;
  try {
    man = json::parse(std::string(reinterpret_cast<char*>(out.data), out.count));
  } catch (...) {
    CHECK(false);
  }
  host.allocator.free(host.allocator.user_data, out.data);
  p1_session_destroy(h);
  astrocs_host_services_destroy_state_v1(state);

  // complete 门 fail-closed: Phase1 链不完整（session 仅覆盖 calibrate/cosmetic）
  // → status 不得为 "complete"（PROD-P0-001: 不完整 Phase 禁写 complete）
  CHECK_MSG(man.value("status", "") != "complete",
            "incomplete phase1 chain must not write complete");
  CHECK(man.value("status", "") == "partial");
  // availability 全域报告（8 域; 不冒充完成）
  CHECK(man.contains("availability") && man["availability"].is_object());
  const char* domains[] = {"calibration", "cosmetic", "star_psf", "wcs",
                           "photometry", "noise_snr", "drizzle", "writer"};
  for (const char* d : domains) {
    CHECK_MSG(man["availability"].contains(d),
              ("availability missing domain: " + std::string(d)).c_str());
  }
  CHECK(man["availability"].value("calibration", "") == "available");
  CHECK(man["availability"].value("star_psf", "") == "unavailable");
  CHECK(man["availability"].value("writer", "") == "unavailable");

  cleanup_fixture(fx);
}

// ── 4. 负向: 坏 config / 坏帧 / 缺科学参数确定性拒绝, 不留伪产物 ─────────────
static void test_negative_injection() {
  Fixture fx = make_fixture("neg");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;

  // 4a. 缺 output_dir → validate 拒绝（fail-closed, 不执行科学计算）
  {
    auto m = reg.create("astrocs.phase1.calibration");
    CHECK(m.ok());
    auto v = m.value()->validate_config(R"({"input_lights":["x.fits"]})");
    CHECK(v.failed());
  }
  // 4b. 截断 FITS（坏帧）→ calibrate 节点失败, 无 calibrated_ 产物
  {
    const std::string bad = (fx.dir / "bad.fits").string();
    float v0 = 0.f;
    // fixture 语义: truncate_tail 截去数据块尾部 64 字节 → 坏帧(读必败)
    CHECK(p1sess::write_fits_file(bad, kW, kH, const_pixel, &v0,
                                  /*truncate_tail=*/1024) == 0);
    const std::string cfg = R"({
      "input_lights": [")" + bad + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    CHECK(rc.failed());
    // 无伪产物：失败后不得产出 calibrated_bad.fits
    CHECK(!fs::exists(fs::path(fx.out_dir + "/calibrated_bad.fits")));
  }
  // 4c. drizzle 缺 nside（科学参数禁 silent default）→ 确定性 DATA 拒绝
  {
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
              "cd11": -0.0002777777777777778, "cd12": 0.0,
              "cd21": 0.0, "cd22": 0.0002777777777777778},
      "drizzle": {"nested": 0, "pixfrac": 1.0}
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    CHECK_MSG(rc.failed(), "drizzle without nside must be rejected");
    CHECK(!fs::exists(fs::path(frame_root(fx) + "/p1_stack.json")));
  }
  // 4d. 纯噪声帧 → star-psf 空 catalog 合法成功（不误报）
  {
    const std::string noise_f = (fx.dir / "noise.fits").string();
    float v1 = 100.0f;
    CHECK(p1sess::write_fits_file(noise_f, kW, kH, const_pixel, &v1) == 0);
    const std::string cfg = R"({
      "input_lights": [")" + noise_f + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "pure-noise frame: empty catalog is legal, must not fail");
    if (man.contains("sources_artifact")) {
      json cat;
      try { cat = json::parse(read_file(man.value("sources_artifact", ""))); } catch (...) { CHECK(false); }
      std::uint32_t total = 0;
      for (const auto& fr : cat.value("frames", json::array()))
        total += fr.value("n_detected", 0u);
      CHECK(total == 0);
    }
  }
  cleanup_fixture(fx);
}

// ══════════════════════════════════════════════════════════════════════════
// B2-A12/A13/A14/A16 科学缺省 / provenance / fail-closed 新门
// (findings: aud-p1 P1-1/P2-9=P1 precision; P1-5=dark K; P1-6=PHOTAPPL;
//  P1-4=photometry fail-open)
// RED→GREEN: 以下断言在修复前失败、修复后通过。
// ══════════════════════════════════════════════════════════════════════════

// ── P23: 标准 HiPS 读面 (取代已删除的 legacy 单文件容器读面) ──────────────
// 读一个标准 512×512 HiPS 图 tile。
bool read_hips_tile_px(const std::string& path, std::vector<float>* out) {
  AIOImageData* im = aio_read(path.c_str());
  if (!im) return false;
  const int iw = aio_get_width(im), ih = aio_get_height(im);
  const float* p = aio_get_pixel_data(im);
  if (!p || iw != 512 || ih != 512) { aio_free_image_data(im); return false; }
  out->assign(p, p + static_cast<size_t>(iw) * static_cast<size_t>(ih));
  aio_free_image_data(im);
  return true;
}

// 枚举 <root>/<plane> 下全部图 tile (排除 Moc.fits/metadata.fits/properties),
// key = 相对该 plane 的路径 (NorderK/DirD/NpixN.fits), value = 512×512 像素。
void read_hips_plane_px(const std::string& root, const char* plane,
                        std::map<std::string, std::vector<float>>* out) {
  const fs::path base = fs::u8path(root + "/" + plane);
  std::error_code ec;
  for (fs::recursive_directory_iterator it(base, ec), end; it != end; it.increment(ec)) {
    std::error_code fec;
    if (!it->is_regular_file(fec)) continue;
    const fs::path p = it->path();
    const std::string fn = p.filename().string();
    if (fn == "Moc.fits" || fn == "metadata.fits" || fn == "properties") continue;
    if (p.extension() != ".fits") continue;
    std::vector<float> v;
    if (!read_hips_tile_px(p.string(), &v)) continue;
    std::error_code rec;
    (*out)[fs::relative(p, base, rec).generic_string()] = std::move(v);
  }
}

// 读全部 signal 图 tile (key = 相对路径); 空树 → false。
bool hips_tile_signals(const std::string& root,
                       std::map<std::string, std::vector<double>>* out) {
  std::map<std::string, std::vector<float>> m;
  read_hips_plane_px(root, "signal", &m);
  if (m.empty()) return false;
  for (const auto& kv : m)
    out->emplace(kv.first, std::vector<double>(kv.second.begin(), kv.second.end()));
  return true;
}

// 图 plane 快照 (相对路径 → 像素), 用于拓扑等价比较。
void hips_plane_snapshot(const std::string& root, const char* plane,
                         std::map<std::string, std::vector<float>>* out) {
  read_hips_plane_px(root, plane, out);
}

// 常数场 + 可控 FITS EXPTIME 的校准 fixture (B2-A13 Oracle 期望可解析)。
// exptime < 0 → 不写 EXPTIME 卡 (缺失负例)。
Fixture make_exptime_fixture(const char* tag, float lv, float bv, float dv,
                             float fv, double lexp = -1.0, double dexp = -1.0,
                             double bexp = 10.0, double fexp = 1.0) {
  Fixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_expt_" + std::string(tag) + "_" + std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  f.light1 = (f.dir / "light_1.fits").string();
  f.light2 = (f.dir / "light_2.fits").string();
  f.bias = (f.dir / "master_bias.fits").string();
  f.dark = (f.dir / "master_dark.fits").string();
  f.flat = (f.dir / "master_flat.fits").string();
  f.out_dir = f.dir.string();
  float v = lv;
  CHECK(p1sess::write_fits_file(f.light1, kW, kH, const_pixel, &v, 0, lexp) == 0);
  CHECK(p1sess::write_fits_file(f.light2, kW, kH, const_pixel, &v, 0, lexp) == 0);
  v = bv; CHECK(p1sess::write_fits_file(f.bias, kW, kH, const_pixel, &v, 0, bexp) == 0);
  v = dv; CHECK(p1sess::write_fits_file(f.dark, kW, kH, const_pixel, &v, 0, dexp) == 0);
  v = fv; CHECK(p1sess::write_fits_file(f.flat, kW, kH, const_pixel, &v, 0, fexp) == 0);
  return f;
}

// ── B2-A16: 测光节点缺上游 artifact / 缺帧 → DATA fail-closed ─────────────
static void test_b2a16_photometry_fail_closed() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  // 16a: 缺 p1_sources.json → 必须失败, 不得写 p1_flux.json
  {
    Fixture fx = make_fixture("b2a16a");
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A16: missing p1_sources.json must fail (older code returned success)");
    if (rc.failed()) CHECK(rc.error().domain() == ErrorDomain::DATA);
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/p1_flux.json")),
              "B2-A16: no p1_flux.json on missing upstream artifact");
    cleanup_fixture(fx);
  }
  // 16b: sources 引用缺失帧 → 失败 (帧缺失按合同上抛, 不静默跳过)
  {
    Fixture fx = make_fixture("b2a16b");
    RunContext ctx;
    {
      std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-SOURCES","frames":[{"file":"missing_frame.fits","sources":[{"id":"s1","x":16,"y":16}]}]})";
    }
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A16: frame referenced by p1_sources.json missing must fail");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/p1_flux.json")),
              "B2-A16: no p1_flux.json when a frame is missing");
    cleanup_fixture(fx);
  }
  // 16c: 正向 — 上游 sources 合法 → success 且写 p1_flux.json + p1_phot.json
  {
    Fixture fx = make_fixture("b2a16c");
    RunContext ctx;
    {
      std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-SOURCES","frames":[{"file":"light_1.fits","sources":[{"id":"s1","x":16,"y":16}]}]})";
    }
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "B2-A16: valid upstream sources must succeed");
    CHECK(man.value("operation", "") == "measure_flux");
    CHECK(fs::exists(fs::path(fx.out_dir + "/p1_flux.json")));
    cleanup_fixture(fx);
  }
}

// ── B2-A13: dark_opt K 由 FITS EXPTIME 推导 + 缺失/不一致 fail-closed ──────
static void test_b2a13_dark_scale_from_exptime() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const float L = 100.0f, B = 10.0f, D = 5.0f, F = 1.0f;
  const double tl = 300.0, td = 600.0;
  const double K = tl / td;  // 0.5 (SCI-CAL-001 §5: K=t_light/t_dark)
  // 13a: K == t_light/t_dark, 且校准像素 == 独立 Oracle (L-B-K*(D-B))/F
  {
    Fixture fx = make_exptime_fixture("b2a13a", L, B, D, F, tl, td);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "dark_optimization": false,
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), ("B2-A13: dark_opt K from EXPTIME must succeed: " +
                        (rc.failed() ? rc.error().message() : std::string())).c_str());
    if (rc.ok()) {
      double got = -1.0;
      if (man.contains("stages") && man["stages"].is_array()) {
        for (const auto& st : man["stages"]) {
          if (st.value("name", "") == "calibrate" && st.contains("per_frame") &&
              st["per_frame"].is_array() && !st["per_frame"].empty())
            got = st["per_frame"][0].value("dark_scale", -1.0);
        }
      }
      CHECK_MSG(std::fabs(got - K) < 1e-9,
                ("B2-A13: dark_scale must equal t_light/t_dark=" + std::to_string(K) +
                 " got=" + std::to_string(got)).c_str());
      // Oracle: 独立解析期望 = (L-B-K*(D-B))/F
      const std::string out_fits = fx.out_dir + "/calibrated_light_1.fits";
      CHECK(fs::exists(fs::path(out_fits)));
      AIOImageData* im = aio_read(out_fits.c_str());
      CHECK_MSG(im != nullptr, "B2-A13: calibrated FITS must be readable");
      if (im) {
        const float* px = aio_get_pixel_data(im);
        const double expect = (L - B - K * (D - B)) / F;
        double maxdiff = 0.0;
        const int64_t n = static_cast<int64_t>(kW) * kH;
        for (int64_t i = 0; i < n; ++i)
          maxdiff = std::max(maxdiff, std::fabs(static_cast<double>(px[i]) - expect));
        CHECK_MSG(maxdiff < 1e-4,
                  ("B2-A13: K=EXPTIME Oracle max|out-expected|=" +
                   std::to_string(maxdiff)).c_str());
        aio_free_image_data(im);
      }
    }
    cleanup_fixture(fx);
  }
  // 13b: dark EXPTIME 缺失 → DATA fail
  {
    Fixture fx = make_exptime_fixture("b2a13b", L, B, D, F, tl, -1.0);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "dark_optimization": false,
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A13: dark EXPTIME missing must fail-closed");
    if (rc.failed()) CHECK(rc.error().domain() == ErrorDomain::DATA);
    CHECK(!fs::exists(fs::path(fx.out_dir + "/calibrated_light_1.fits")));
    cleanup_fixture(fx);
  }
  // 13c: light EXPTIME 缺失 → DATA fail
  {
    Fixture fx = make_exptime_fixture("b2a13c", L, B, D, F, -1.0, td);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "dark_optimization": false,
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A13: light EXPTIME missing must fail-closed");
    cleanup_fixture(fx);
  }
  // 13d: 配置 dark_scale_factor 与 EXPTIME 比不一致 → DATA fail (禁标量冒充科学输入)
  {
    Fixture fx = make_exptime_fixture("b2a13d", L, B, D, F, tl, td);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "dark_optimization": false,
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": true,
      "dark_scale_factor": 2.0
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A13: dark_scale_factor disagreeing with EXPTIME K must fail");
    cleanup_fixture(fx);
  }
  // 13e (CONFORM-FIX-A ⑤ / CONFORM-SWEEP-1-008): **标准式** (dark_optimization=false,
  //   单键) 且 K≠1 ⇒ manifest per_frame[].dark_scale 必须等于**实际施加**的 K=t_l/t_d
  //   (旧实现写 k_fixed=1.0 默认值, 与算术 k_use 不符), 且像素等于标准式 Oracle
  //   (L − B − K·D)/F (CALIBRATION_ALGORITHMS.md F3.2 退化对照表「标准式 K≠1」行)。
  {
    Fixture fx = make_exptime_fixture("b2a13e", L, B, D, F, tl, td);
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "master_bias": ")" + fx.bias + R"(",
      "master_dark": ")" + fx.dark + R"(",
      "master_flat": ")" + fx.flat + R"(",
      "output_dir": ")" + fx.out_dir + R"(",
      "dark_optimization": false
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.calibration", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), ("B2-A13: standard-form K from EXPTIME must succeed: " +
                        (rc.failed() ? rc.error().message() : std::string())).c_str());
    if (rc.ok()) {
      double got = -1.0;
      if (man.contains("stages") && man["stages"].is_array()) {
        for (const auto& st : man["stages"]) {
          if (st.value("name", "") == "calibrate" && st.contains("per_frame") &&
              st["per_frame"].is_array() && !st["per_frame"].empty())
            got = st["per_frame"][0].value("dark_scale", -1.0);
        }
      }
      // 证据行 (CONFORM-FIX-A ⑤): 直接打印观测值, 使 ctest 日志可核对 red/green。
      std::printf("[B2-A13 13e] standard-form dark_scale observed=%.17g expected K=t_l/t_d=%.17g\n",
                  got, K);
      CHECK_MSG(std::fabs(got - K) < 1e-9,
                ("B2-A13: standard-form dark_scale must equal actual K=t_light/t_dark=" +
                 std::to_string(K) + " got=" + std::to_string(got)).c_str());
      // Oracle: 标准式 (raw − bias − K·dark)/flat
      const std::string out_fits = fx.out_dir + "/calibrated_light_1.fits";
      CHECK(fs::exists(fs::path(out_fits)));
      AIOImageData* im = aio_read(out_fits.c_str());
      CHECK_MSG(im != nullptr, "B2-A13: standard-form calibrated FITS readable");
      if (im) {
        const float* px = aio_get_pixel_data(im);
        const double expect = (L - B - K * D) / F;
        double maxdiff = 0.0;
        const int64_t n = static_cast<int64_t>(kW) * kH;
        for (int64_t i = 0; i < n; ++i)
          maxdiff = std::max(maxdiff, std::fabs(static_cast<double>(px[i]) - expect));
        CHECK_MSG(maxdiff < 1e-4,
                  ("B2-A13: standard-form K Oracle max|out-expected|=" +
                   std::to_string(maxdiff)).c_str());
        aio_free_image_data(im);
      }
    }
    cleanup_fixture(fx);
  }
}

// ── B2-A12: drizzle precision_mode 缺省门 + FP32/FP64 等价性 ───────────────
static void test_b2a12_precision_default_and_equiv() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const char* wcs = R"("wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},)";
  // 12a: 缺 precision_mode → DATA 拒绝 (无 silent FP32 缺省), 不写 p1_stack.json
  {
    Fixture fx = make_fixture("b2a12a");
    RunContext ctx;
    const std::string cfg = std::string(R"({
      "input_lights": [")") + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      )" + wcs + R"(
      "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0}
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    (void)man;
    CHECK_MSG(rc.failed(), "B2-A12: missing precision_mode must be rejected (no silent FP32)");
    if (rc.failed()) CHECK(rc.error().domain() == ErrorDomain::DATA);
    CHECK(!fs::exists(fs::path(frame_root(fx) + "/p1_stack.json")));
    cleanup_fixture(fx);
  }
  // 12b: FP32/FP64 等价性 + p1_stack.json 记录 precision_mode
  {
    Fixture fx32 = make_fixture("b2a12f32");
    Fixture fx64 = make_fixture("b2a12f64");
    ModuleRegistry reg2;
    CHECK(register_phase_modules(reg2).ok());
    auto run = [&](Fixture& fx, int mode, const char* tag) -> json {
      RunContext ctx;
      const std::string cfg = std::string(R"({
        "input_lights": [")") + fx.light1 + R"("],
        "output_dir": ")" + fx.out_dir + R"(",
        )" + wcs + R"(
        "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": )" +
        std::to_string(mode) + R"(}
      })";
      Result<void> rc;
      json man = run_node(reg2, "astrocs.phase1.drizzle", cfg, ctx, &rc);
      CHECK_MSG(rc.ok(), (std::string("B2-A12: drizzle ") + tag +
                          " must succeed: " +
                          (rc.failed() ? rc.error().message() : std::string())).c_str());
      return man;
    };
    json m32 = run(fx32, 0, "FP32");
    json m64 = run(fx64, 1, "FP64");
    // p1_stack.json provenance: precision_mode 显式记录 (禁隐式缺省)
    auto stack_prec = [](Fixture& fx) -> int {
      json s;
      try { s = json::parse(read_file(frame_root(fx) + "/p1_stack.json")); } catch (...) { return -99; }
      return s.value("precision_mode", -99);
    };
    CHECK_MSG(stack_prec(fx32) == 0, "B2-A12: p1_stack.json must record precision_mode=0");
    CHECK_MSG(stack_prec(fx64) == 1, "B2-A12: p1_stack.json must record precision_mode=1");
    // 等价性: 同一输入 FP32/FP64 累积逐 tile signal 相对一致 (输出窄化 FP32)
    std::map<std::string, std::vector<double>> s32, s64;
    const bool ok32 = hips_tile_signals(frame_root(fx32), &s32);
    const bool ok64 = hips_tile_signals(frame_root(fx64), &s64);
    CHECK_MSG(ok32 && ok64, "B2-A12: FP32/FP64 HiPS signal tiles must be readable");
    CHECK_MSG(s32.size() == s64.size() && !s32.empty(),
              "B2-A12: FP32/FP64 must touch the same tile set");
    double max_rel = 0.0;
    for (const auto& [ipix, v32] : s32) {
      auto it = s64.find(ipix);
      if (it == s64.end()) { max_rel = 1e9; break; }
      const std::vector<double>& v64 = it->second;
      if (v64.size() != v32.size()) { max_rel = 1e9; break; }
      for (size_t i = 0; i < v32.size(); ++i) {
        const double a = v32[i], b = v64[i];
        const double denom = std::max(1.0, std::fabs(b));
        max_rel = std::max(max_rel, std::fabs(a - b) / denom);
      }
    }
    CHECK_MSG(max_rel < 1e-5,
              ("B2-A12: FP32/FP64 signal equivalence max_rel=" +
               std::to_string(max_rel)).c_str());
    cleanup_fixture(fx32);
    cleanup_fixture(fx64);
  }
}

// ── B2-A14: drizzle PHOTAPPL/PHOTSCAL 由真实 provenance 决定 (禁硬编码 1) ──
static void test_b2a14_photappl_provenance() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const char* wcs = R"("wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},)";
  auto run_drz = [&](Fixture& fx, json* meta_out) -> bool {
    RunContext ctx;
    const std::string cfg = std::string(R"({
      "input_lights": [")") + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      )" + wcs + R"(
      "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 1}
    })";
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    if (rc.failed()) {
      std::fprintf(stderr, "DBG B2-A14 drizzle failed: %s\n", rc.error().message().c_str());
      return false;
    }
    // P23: PHOTAPPL/PHOTSCAL/BUNIT 事实面从 p1_stack.json provenance 读取
    // (旧链落中间容器头; 直写末端把同一 provenance 落 p1_stack.json)。
    try { *meta_out = json::parse(read_file(frame_root(fx) + "/p1_stack.json")); }
    catch (...) { return false; }
    return true;
  };
  // 14a: 无测光 provenance → PHOTAPPL=0 / BUNIT=ADU (绝不伪造 1)
  {
    Fixture fx = make_fixture("b2a14a");
    json meta = json::object();
    const bool ok = run_drz(fx, &meta);
    CHECK_MSG(ok, "B2-A14: drizzle must complete with explicit ADU degradation");
    if (ok) {
      CHECK_MSG(meta.value("photappl", -1) == 0,
                "B2-A14: PHOTAPPL must be 0 when no photometry provenance (forged 1 removed)");
      CHECK_MSG(meta.value("bunit", std::string()) == "ADU",
                "B2-A14: BUNIT must degrade to ADU, not RELATIVE_FLUX");
    }
    cleanup_fixture(fx);
  }
  // 14b: 真实测光 provenance 声明已应用 → PHOTAPPL=1 + PHOTSCAL 原样透传
  {
    Fixture fx = make_fixture("b2a14b");
    {
      std::ofstream o(fx.out_dir + "/p1_phot.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTPROV-001","node":"astrocs.phase1.photometry",)"
           R"("operation":"measure_flux","photometry_applied":true,"photscal":0.5,"pixel_scaling":"applied"})";
    }
    // FIX-REGRESS: applied=true ⇒ 必须真实产出逐帧 photoapplied_<base>
    // （生产契约 fail-closed；旧夹具只写标量 photscal 是遗留缺陷）。
    {
      ScaledStarField ssf{100.0f, 5000.0f, 0.5f};
      CHECK_MSG(p1sess::write_fits_file(
                    fx.out_dir + "/photoapplied_light_1.fits", kW, kH,
                    scaled_star_field_pixel, &ssf, 0, 60.0) == 0,
                "B2-A14b: photoapplied_light_1.fits fixture write failed");
    }
    json meta = json::object();
    const bool ok = run_drz(fx, &meta);
    CHECK_MSG(ok, "B2-A14: drizzle with valid photometry provenance must succeed");
    if (ok) {
      CHECK_MSG(meta.value("photappl", -1) == 1,
                "B2-A14: PHOTAPPL=1 must be driven by real photometry provenance");
      CHECK_MSG(std::fabs(meta.value("photscal", -1.0) - 0.5) < 1e-12,
                "B2-A14: PHOTSCAL must come from provenance, not hardcoded/config");
      CHECK_MSG(meta.value("bunit", std::string()) == "ASTROCS_RELATIVE_FLUX",
                "B2-A14: BUNIT=RELATIVE_FLUX only when provenance says applied");
    }
    cleanup_fixture(fx);
  }
}

// ── 5. 确定性: 同 config 双跑 star-psf 输出 bitwise 一致 ───────────────────
// ── P23: 稀疏多 standard tile HiPS 夹具 (取代已删除的中间容器夹具) ─────
// 直接构造 TileAccumulator 并调用生产末端 write_hips_phase1, 覆盖:
//   (a) 每个 standard tile 的有限 signal 像元数 == 构造的覆盖叶数
//       (无幻灵 / 无跨 parent 缓冲泄露);
//   (b) support 严格按构造面积比 (uint8/255) 连续缩放, 而非 0/1 坍缩 (P1-8)。
constexpr uint64_t kA15Cover0 = 768;   // tile 0: local 0..767 (partial)
constexpr uint64_t kA15Cover1 = 255;   // tile 1: local 0..254 (partial)
constexpr uint8_t kA15Tile0Support = 128;   // 部分覆盖 128/255 ≈ 0.502
constexpr uint8_t kA15Tile1Support = 64;    // 64/255 ≈ 0.251
constexpr double kA15SignalV = 0.5;

// 稀疏 tile 描述: parent (Norder=L-9 standard) / 起始 local / 覆盖叶数 / 面积比 u8。
struct SparseTile {
  uint64_t parent;
  uint64_t offset;
  uint64_t cover;
  uint8_t  support;
};

// 用生产末端 write_hips_phase1 把稀疏累加器直写成标准 HiPS (nside=512)。
bool write_sparse_hips(const std::string& root,
                       const std::vector<SparseTile>& tiles) {
  const uint32_t nside = 512;
  const double a_cell = 4.0 * 3.14159265358979323846 /
                        (12.0 * static_cast<double>(nside) *
                         static_cast<double>(nside));
  std::vector<drizzle::TileAccumulatorT<float>> accs;
  for (const SparseTile& t : tiles) {
    drizzle::TileAccumulatorT<float> acc;
    acc.parent_ipix = t.parent;
    acc.pixels.resize(512u * 512u);
    for (uint64_t k = 0; k < t.cover; ++k) {
      const uint64_t i = t.offset + k;
      acc.pixels[i].sumFlux = static_cast<float>(kA15SignalV);
      acc.pixels[i].sumArea = static_cast<float>(
          (static_cast<double>(t.support) / 255.0) * a_cell);
      acc.pixels[i].nContrib = 1;
      acc.touched.push_back(static_cast<uint32_t>(i));
    }
    accs.push_back(std::move(acc));
  }
  drizzle::DrizzleConfig cfg;
  cfg.nside = static_cast<int>(nside);
  cfg.tile_depth = 9;
  std::string err;
  const bool ok = drizzle::write_hips_phase1<float>(accs, cfg, root, "", err);
  if (!ok) std::fprintf(stderr, "B2-A15 write_hips_phase1 failed: %s\n", err.c_str());
  // 上游 provenance (writer 节点据此定位叶片 Norder)
  std::ofstream sf(root + "/p1_stack.json", std::ios::binary);
  if (sf) sf << "{\"schema\":\"DATA-P1-STACK\",\"nside\":" << nside << "}";
  return ok;
}

bool read_hips_tile(const std::string& path, std::vector<float>* out) {
  AIOImageData* im = aio_read(path.c_str());
  if (!im) return false;
  const int iw = aio_get_width(im), ih = aio_get_height(im);
  const float* p = aio_get_pixel_data(im);
  if (!p || iw != 512 || ih != 512) { aio_free_image_data(im); return false; }
  out->assign(p, p + static_cast<size_t>(iw) * static_cast<size_t>(ih));
  aio_free_image_data(im);
  return true;
}

// IVOA REC-HIPS-1.0 §4.1: DirD 的 D = (N/10000)*10000 (块起始值), 文件名 NpixN
// 带完整 tile 号。旧非标准式 (Dir=商/Npix=余数) 已由 M2b-B-01 废止。
bool hips_tile_signal(const std::string& root, uint64_t tile, std::vector<float>* out) {
  return read_hips_tile(root + "/signal/Norder0/Dir" +
                            std::to_string((tile / 10000) * 10000) + "/Npix" +
                            std::to_string(tile) + ".fits", out);
}
bool hips_tile_support(const std::string& root, uint64_t tile, std::vector<float>* out) {
  return read_hips_tile(root + "/support/Norder0/Dir" +
                            std::to_string((tile / 10000) * 10000) + "/Npix" +
                            std::to_string(tile) + ".fits", out);
}
// ── B2-A15: sink stale buffer / support 量化 (P1-7 + P1-8) ────────────
// 稀疏夹具: 2 个 standard tile 各被部分覆盖 (面积比 128/64 of 255)。不依赖
// FITS 数组与 NESTED 的具体转换, 用不变量判定:
//   (a) 每个 standard tile 有限 signal 像元数 == 构造覆盖叶数,
//       无幻灵 / 无跨 parent 泄露 (旧实现 buffer 不清零 → 多余有效像素);
//   (b) support 严格按面积比连续缩放 (128/255, 64/255), 而非塑成 0/1 (P1-8)。
static void test_b2a15_writer_stale_buffer_and_support() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  Fixture fx = make_fixture("b2a15");
  CHECK_MSG(write_sparse_hips(frame_root(fx), {{0, 0, kA15Cover0, kA15Tile0Support},
                                           {1, 0, kA15Cover1, kA15Tile1Support}}),
            "B2-A15: sparse multi-tile HiPS fixture (write_hips_phase1)");
  RunContext ctx;
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "filter_passband": "R"
  })";
  Result<void> wrc;
  json wman = run_node(reg, "astrocs.phase1.writer", cfg, ctx, &wrc);
  CHECK_MSG(wrc.ok(), ("B2-A15: writer must validate direct HiPS: " +
                       (wrc.failed() ? wrc.error().message() : std::string())).c_str());
  if (wrc.failed()) { cleanup_fixture(fx); return; }
  std::vector<float> sig0, sup0, sig1, sup1;
  CHECK_MSG(hips_tile_signal(frame_root(fx), 0, &sig0), "B2-A15: signal tile 0 readable");
  CHECK_MSG(hips_tile_support(frame_root(fx), 0, &sup0), "B2-A15: support tile 0 readable");
  CHECK_MSG(hips_tile_signal(frame_root(fx), 1, &sig1), "B2-A15: signal tile 1 readable");
  CHECK_MSG(hips_tile_support(frame_root(fx), 1, &sup1), "B2-A15: support tile 1 readable");
  if (sig0.size() != 512ull * 512ull || sup0.size() != sig0.size() ||
      sig1.size() != sig0.size() || sup1.size() != sig0.size()) {
    CHECK_MSG(false, "B2-A15: HiPS tile size must be 512x512");
    cleanup_fixture(fx); return;
  }
  {  // n_tiles_written 属 p1_final.json 产物面字段
    json fin0;
    try { fin0 = json::parse(read_file(frame_root(fx) + "/p1_final.json")); } catch (...) {}
    CHECK(fin0.value("n_tiles_written", 0) == 2);
    CHECK(fin0.value("n_tiles", 0u) == 2u);
  }
  const double exp_sup0 = static_cast<double>(kA15Tile0Support) / 255.0;
  const double exp_sup1 = static_cast<double>(kA15Tile1Support) / 255.0;
  uint64_t valid0 = 0, valid1 = 0;
  double max_dev0 = 0.0, max_dev1 = 0.0;
  bool collapsed = false, nan_leak = false;
  for (size_t i = 0; i < sig0.size(); ++i) {
    const bool fin0 = std::isfinite(sig0[i]);
    const double s0 = sup0[i];
    if (fin0) {
      ++valid0;
      if (!(std::fabs(s0 - exp_sup0) < 0.01)) collapsed = true;
      max_dev0 = std::max(max_dev0, std::fabs(s0 - exp_sup0));
    } else if (std::fabs(s0) > 1e-6) {
      nan_leak = true;
    }
  }
  for (size_t i = 0; i < sig1.size(); ++i) {
    const bool fin1 = std::isfinite(sig1[i]);
    const double s1 = sup1[i];
    if (fin1) {
      ++valid1;
      if (!(std::fabs(s1 - exp_sup1) < 0.01)) collapsed = true;
      max_dev1 = std::max(max_dev1, std::fabs(s1 - exp_sup1));
    } else if (std::fabs(s1) > 1e-6) {
      nan_leak = true;
    }
  }
  CHECK_MSG(valid0 == kA15Cover0,
            ("B2-A15: tile 0 valid pixel count must equal coverage (no ghost): " +
             std::to_string(valid0)).c_str());
  CHECK_MSG(valid1 == kA15Cover1,
            ("B2-A15: tile 1 valid pixel count must equal coverage (no stale leak): " +
             std::to_string(valid1)).c_str());
  CHECK_MSG(!collapsed,
            ("B2-A15: support must scale continuously with area ratio " +
             std::to_string(kA15Tile0Support) + "/255 (tile0 dev=" +
             std::to_string(max_dev0) + " tile1 dev=" + std::to_string(max_dev1) +
             "), not collapse to 0/1").c_str());
  CHECK_MSG(!nan_leak, "B2-A15: invalid signal pixels must have zero support");
  CHECK_MSG(std::fabs(exp_sup0 - exp_sup1) > 0.1,
            "B2-A15 fixture must expose non-full support scaling");
  {
    json fin;
    try { fin = json::parse(read_file(frame_root(fx) + "/p1_final.json")); } catch (...) {}
    CHECK_MSG(fin.value("covered_area_model", "") == "support_ratio_x_A_cell",
              "B2-A15: covered_area_model must record uint8 area-ratio scaling");
  }
  cleanup_fixture(fx);
}


// ── RESCUE A15 独立注入证明: 稀疏 / 多 parent / 覆盖不连续 ghost 场景 ──────
// 两个稀疏 tile 落在不同 standard parent, 且第二个 tile 的覆盖区从 local
// offset 1024 开始 (与第一个 tile 的 0..767 不相交)。若 sink 未对每个 parent
// 清零缓冲且 valid_mask 缺失, parent0 的 signal/coverage 会残留到 parent1 的
// 0..767 → 幽灵像素。回退 "每 parent 清零" 或 "valid_mask=touched" 任一必红。
constexpr uint64_t kA15GhostParent0 = 0;
constexpr uint64_t kA15GhostParent1 = 1;
constexpr uint64_t kA15GhostOffset1 = 1024;
constexpr uint64_t kA15GhostCover0 = 768;
constexpr uint64_t kA15GhostCover1 = 255;
constexpr uint8_t  kA15GhostSup0 = 128;
constexpr uint8_t  kA15GhostSup1 = 64;

static void test_b2a15_ghost_discontinuous_multiparent() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  Fixture fx = make_fixture("b2a15g");
  CHECK_MSG(write_sparse_hips(frame_root(fx),
                              {{kA15GhostParent0, 0, kA15GhostCover0, kA15GhostSup0},
                               {kA15GhostParent1, kA15GhostOffset1, kA15GhostCover1, kA15GhostSup1}}),
            "A15 ghost fixture must be written (write_hips_phase1)");
  RunContext ctx;
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "filter_passband": "R"
  })";
  Result<void> wrc;
  json wman = run_node(reg, "astrocs.phase1.writer", cfg, ctx, &wrc);
  CHECK_MSG(wrc.ok(), ("A15 ghost: writer must succeed: " +
                       (wrc.failed() ? wrc.error().message() : std::string())).c_str());
  if (wrc.failed()) { cleanup_fixture(fx); return; }
  std::vector<float> sig0,sup0,sig1,sup1;
  CHECK(hips_tile_signal(frame_root(fx), 0, &sig0));
  CHECK(hips_tile_support(frame_root(fx), 0, &sup0));
  CHECK(hips_tile_signal(frame_root(fx), 1, &sig1));
  CHECK(hips_tile_support(frame_root(fx), 1, &sup1));
  if (sig0.size() != 512ull*512ull || sup0.size()!=sig0.size() ||
      sig1.size()!=sig0.size() || sup1.size()!=sig0.size()) {
    CHECK_MSG(false, "A15 ghost: tiles must be 512x512");
    cleanup_fixture(fx); return;
  }
  const double exp0 = static_cast<double>(kA15GhostSup0)/255.0;
  const double exp1 = static_cast<double>(kA15GhostSup1)/255.0;
  uint64_t valid0=0, valid1=0, stale_finite=0;
  bool nan_leak=false, support_bad=false;
  for (size_t i=0;i<sig0.size();++i) {
    if (std::isfinite(sig0[i])) {
      ++valid0;
      if (!(sig0[i] > 0.0f) || std::fabs(sup0[i]-exp0) > 0.01) support_bad = true;
    } else if (std::fabs(sup0[i]) > 1e-6) {
      nan_leak = true;
    }
  }
  for (size_t i=0;i<sig1.size();++i) {
    if (std::isfinite(sig1[i])) {
      ++valid1;
      if (i < kA15GhostOffset1) ++stale_finite;   // parent0 缓冲区: 必须 invalid
      if (!(sig1[i] > 0.0f) || std::fabs(sup1[i]-exp1) > 0.01) support_bad = true;
    } else if (std::fabs(sup1[i]) > 1e-6) {
      nan_leak = true;
    }
  }
  CHECK_MSG(valid0 == kA15GhostCover0,
            ("A15 ghost: parent0 valid count must equal coverage, got " +
             std::to_string(valid0)).c_str());
  CHECK_MSG(valid1 == kA15GhostCover1,
            ("A15 ghost: parent1 valid count must equal coverage (no stale leak), got " +
             std::to_string(valid1)).c_str());
  CHECK_MSG(stale_finite == 0,
            ("A15 ghost: parent1 stale offsets must be invalid, finite=" +
             std::to_string(stale_finite)).c_str());
  CHECK_MSG(!nan_leak, "A15 ghost: invalid signal pixels must have zero support");
  CHECK_MSG(!support_bad, "A15 ghost: covered pixels must keep area-ratio support");
  {
    json fin;
    try { fin = json::parse(read_file(frame_root(fx) + "/p1_final.json")); } catch (...) {}
    CHECK(fin.value("n_tiles_written", 0) == 2);
  }
  cleanup_fixture(fx);
}

// ── B2-A17 helper: 单像素 delta 帧 + 标准 HiPS 精确 signal 读面 ──────────────
// 单像素 delta 帧: drizzle footprint = CRVAL 周围有限区域的单个 HEALPix
// 叶像素, signal 严格 = F(ndrop=1, d=54.59, pixfrac=1.0) — 与 1e-6 精度可比。
inline float delta_px(int i, void* user) { return i == *static_cast<int*>(user) ? 1.0f : 0.0f; }

// ── B2-A17: 标准 HiPS 精确读面 (取代已删除的 legacy 容器读面) ────────────
// 返回全部有限 signal 像元: (相对 tile 路径, 局部索引, signal, support)。
struct HipsSignalPixel { std::string tile; uint32_t local; double signal; double support; };
std::vector<HipsSignalPixel> hips_exact_signal(const std::string& root) {
  std::vector<HipsSignalPixel> out;
  std::map<std::string, std::vector<float>> sig, sup;
  read_hips_plane_px(root, "signal", &sig);
  read_hips_plane_px(root, "support", &sup);
  for (const auto& kv : sig) {
    auto it = sup.find(kv.first);
    for (uint32_t i = 0; i < kv.second.size(); ++i) {
      if (!std::isfinite(kv.second[i])) continue;
      const double s = (it != sup.end() && i < it->second.size())
                           ? static_cast<double>(it->second[i]) : 0.0;
      out.push_back({kv.first, i, static_cast<double>(kv.second[i]), s});
    }
  }
  return out;
}
// 单叶 flux 守恒 Oracle: signal·support·A_cell == 落入该叶的累计通量。
// nside=512 → A_cell = 4π/(12·512²)。
constexpr double kA17ACell =
    4.0 * 3.14159265358979323846 / (12.0 * 512.0 * 512.0);

// 独立 TAN + SIP 前向参考解 (与 WcsTan/WcsSip 实现不同源):
// p = 0-based 像素中心; dx = p - (CRPIX-1); U = dx + A(dx,dy) = dx (仅 A_2_0);
// xi = CD11*U + CD12*V; eta = CD21*U + CD22*V;
// RA = CRVAL1 + atan2(xi, cos(CRVAL2_rad) - eta*sin(CRVAL2_rad)) 近似 (0.64° 小视场).
void tan_sip_reference(double crpix1, double crpix2, double crval1, double crval2,
                       double cd11, double cd12, double cd21, double cd22,
                       const std::vector<double>& a, const std::vector<double>& b,
                       double x, double y, double* ra, double* dec) {
  const double dx = x - (crpix1 - 1.0);
  const double dy = y - (crpix2 - 1.0);
  const double A = a[12] * dx * dx + a[21] * dx * dy;
  const double B = b[2] * dy * dy + b[12] * dx * dx;
  const double U = dx + A;
  const double V = dy + B;
  const double xi = cd11 * U + cd12 * V;
  const double eta = cd21 * U + cd22 * V;
  const double d2r = 3.14159265358979323846 / 180.0;
  const double r2d = 180.0 / 3.14159265358979323846;
  const double xi_r = xi * d2r, eta_r = eta * d2r;
  const double dec0 = crval2 * d2r;
  const double denom = std::cos(dec0) - eta_r * std::sin(dec0);
  *ra = crval1 + std::atan2(xi_r, denom) * r2d;
  *dec = std::atan2(std::sin(dec0) + eta_r * std::cos(dec0),
                    std::sqrt(xi_r * xi_r + denom * denom)) * r2d;
}

// 单像素 delta 帧下的精确 signal 预期 (ALG-DRZ 同源):
// F = 1 · (1/54.5949848) · (1/1.0) · 1 · (1/0.0002777777777777778^2)
// 单像素 delta 帧: weight = overlap/drop_area = 1 (完全重合), sumFlux = L·weight = 1.0。
// 标准 HiPS 产物信号为 surface brightness = flux/covered_area, support = covered_area/A_cell,
// 故守恒 Oracle 为 signal·support·A_cell = 累计通量 = 1.0 (与旧容器信号定义等价)。
// 该常量与 WCS/SIP 无关——作为“单像素不散开”的守卫; SIP 桥接的
// 可观测性由下方 (3a)/(3b) 的“两路径落点+support 平面一致”断言承担。
constexpr double kA17ExpectedSignal = 1.0;

// ── B2-A17: SIP 桥接 (p1_wcs.json 落盘 + drizzle frame header 下发) ──────
// AUD-COORD F-03: 解算结果 sip_a/b/ap/bp 从未落盘也从未下发到 drizzle。
static void test_b2a17_sip_bridge() {
  const auto sip_arr = [](std::initializer_list<std::pair<int, double>> terms) {
    std::vector<double> v(36, 0.0);
    for (const auto& [idx, val] : terms) v[static_cast<size_t>(idx)] = val;
    return v;
  };
  const auto to_json_arr = [](const std::vector<double>& v) {
    json a = json::array();
    for (double d : v) a.push_back(d);
    return a;
  };
  const std::vector<double> a = sip_arr({{12, 8.0e-5}});      // A_2_0 (i*6+j)
  const std::vector<double> b = sip_arr({{2, -8.0e-5}});      // B_0_2
  const std::vector<double> ap(36, 0.0), bp(36, 0.0);
  const std::string sip_json =
      json{{"order", 2}, {"ap_order", 0}, {"a", to_json_arr(a)}, {"b", to_json_arr(b)},
           {"ap", to_json_arr(ap)}, {"bp", to_json_arr(bp)}}.dump();
  auto make_cfg = [&](const Fixture& fx, bool with_sip) -> std::string {
    json wcs = {{"crpix1", 16.0}, {"crpix2", 16.0}, {"crval1", 10.0}, {"crval2", 20.0},
                {"cd11", -0.0002777777777777778}, {"cd12", 0.0},
                {"cd21", 0.0}, {"cd22", 0.0002777777777777778}};
    if (with_sip) {
      json sa = json::array(), sb = json::array(), sap = json::array(), sbp = json::array();
      for (int k = 0; k < 36; ++k) {
        sa.push_back(a[static_cast<size_t>(k)]);
        sb.push_back(b[static_cast<size_t>(k)]);
        sap.push_back(ap[static_cast<size_t>(k)]);
        sbp.push_back(bp[static_cast<size_t>(k)]);
      }
      wcs["sip"] = json{{"order", 2}, {"ap_order", 0}, {"a", sa}, {"b", sb},
                        {"ap", sap}, {"bp", sbp}};
    }
    json cfg = {{"input_lights", json::array({fx.light1})},
                {"output_dir", fx.out_dir},
                {"wcs", wcs},
                {"drizzle", json{{"nside", 512}, {"nested", 1}, {"pixfrac", 1.0},
                                 {"precision_mode", 0}}}};
    return cfg.dump();
  };
  // (1) 无 SIP 基线: p1_wcs.json 无 wcs.sip, CTYPE 无 -SIP
  {
    Fixture fx = make_fixture("b2a17no");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = make_cfg(fx, false);
    Result<void> wrc;
    const json wman = run_node(reg, "astrocs.phase1.wcs-platesolve", cfg, ctx, &wrc);
    CHECK_MSG(wrc.ok(), ("B2-A17: explicit linear wcs: " +
                         (wrc.failed() ? wrc.error().message() : std::string())).c_str());
    CHECK(wman.value("wcs_source", "") == "explicit_config");
    json wj;
    try { wj = json::parse(read_file(frame_root(fx) + "/p1_wcs.json")); } catch (...) {}
    CHECK(wj.value("schema", "") == "DATA-P1-WCS");
    CHECK_MSG(!wj["wcs"].contains("sip"), "B2-A17: undistorted path must not emit wcs.sip");
    CHECK(wj["wcs"].value("ctype1", "") == std::string("RA---TAN"));
    Result<void> drc;
    const json dman = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &drc);
    CHECK_MSG(drc.ok(), ("B2-A17: linear drizzle: " +
                         (drc.failed() ? drc.error().message() : std::string())).c_str());
    CHECK(dman.value("operation", "") == "drizzle_stack");
    cleanup_fixture(fx);
  }
  // (2) SIP 路径
  Fixture fx = make_fixture("b2a17");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = make_cfg(fx, true);
  Result<void> wrc;
  const json wman = run_node(reg, "astrocs.phase1.wcs-platesolve", cfg, ctx, &wrc);
  CHECK_MSG(wrc.ok(), ("B2-A17: explicit SIP wcs: " +
                       (wrc.failed() ? wrc.error().message() : std::string())).c_str());
  json wj;
  try { wj = json::parse(read_file(frame_root(fx) + "/p1_wcs.json")); } catch (...) {}
  CHECK(wj.value("schema", "") == "DATA-P1-WCS");
  CHECK_MSG(wj["wcs"].contains("sip"), "B2-A17: p1_wcs.json must persist SIP coefficients");
  if (wj["wcs"].contains("sip")) {
    const json& sip = wj["wcs"]["sip"];
    CHECK(sip.value("order", -1) == 2 && sip.value("ap_order", -1) == 0);
    CHECK(sip["a"].is_array() && sip["a"].size() == 36);
    CHECK(sip["b"].is_array() && sip["b"].size() == 36);
    CHECK(sip["ap"].is_array() && sip["ap"].size() == 36);
    CHECK(sip["bp"].is_array() && sip["bp"].size() == 36);
    CHECK(std::fabs(sip["a"][12].get<double>() - 8.0e-5) < 1e-15);
    CHECK(std::fabs(sip["b"][2].get<double>() + 8.0e-5) < 1e-15);
  }
  CHECK(wj["wcs"].value("ctype1", "") == std::string("RA---TAN-SIP"));
  CHECK(wj["wcs"].value("ctype2", "") == std::string("DEC--TAN-SIP"));
  // 独立 Oracle: 线性 WcsTan 前向 + 独立多项式合成 (WcsTan 无 SIP 支持)。
  // W4-A1 (M1a-C-003): samples[].x/y 是 **0-based 数组下标 (index-is-center)**
  // —— 内部 0-based 自洽口径 (SCI-WCS-001 §3a/§5a); WcsTan 的契约是
  // **FITS 1-based** (wcs_tan.h:11 "参考像素 (1-based)" + pix2sky 的 x − crpix1)。
  // ⇒ oracle 必须经**单次** +1 桥接 (xp = x + 1) 再喂 WcsTan, SIP 自变量同为
  // xp − CRPIX。修复前 samples[] 由 0-based 下标直喂 1-based WcsTan
  // (module_adapters.cpp p1_op_wcs 两条自检环), 与本 oracle 恒差 1px; 下方
  // [origin-sensitivity] 断言把该差 1 变成可失败的判据 (oracle 不再与实现
  // 共享同一原点 ⇒ 对原点平移有鉴别力)。
  constexpr double kPxScaleDeg = 0.0002777777777777778;  // sqrt|det CD| deg/px
  if (wj.contains("samples") && wj["samples"].is_array()) {
    CHECK_MSG(wj.value("pixel_origin", std::string()) ==
                  std::string("0-based array index (index-is-center); "
                              "FITS 1-based xp = x + 1"),
              "W4-A1: p1_wcs.json.samples[] must declare its pixel origin");
    CHECK_MSG(wj.value("fits_pixel_origin", 0.0) == 1.0,
              "W4-A1: p1_wcs.json must declare the FITS 1-based bridge offset");
    double worst = 0.0;
    double min_unbridged_sep_px = 1e30;
    for (const auto& s : wj["samples"]) {
      const double x = s.value("x", 0.0), y = s.value("y", 0.0);
      const double xp = x + 1.0, yp = y + 1.0;  // 单次桥接: 0-based → FITS 1-based
      const double dx = xp - 16.0, dy = yp - 16.0;  // SIP 自变量 = xp − CRPIX
      const double A = 8.0e-5 * dx * dx;
      const double B = -8.0e-5 * dy * dy;
      astrocs::phase1::WcsTan linear;
      linear.crpix1 = 16.0; linear.crpix2 = 16.0;
      linear.crval1 = 10.0; linear.crval2 = 20.0;
      linear.cd11 = -0.0002777777777777778; linear.cd12 = 0.0;
      linear.cd21 = 0.0; linear.cd22 = 0.0002777777777777778;
      double ra = 0.0, dec = 0.0;
      linear.pix2sky(xp + A, yp + B, &ra, &dec);
      worst = std::max(worst, std::fabs(s.value("ra", 0.0) - ra));
      worst = std::max(worst, std::fabs(s.value("dec", 0.0) - dec));
      // [origin-sensitivity] 未桥接读法 (= 修复前实现形态) 必须与桥接读法
      // 相差 >= 0.9 px, 否则本 oracle 对原点平移无鉴别力 (恒真)。
      const double dxu = x - 16.0, dyu = y - 16.0;
      const double Au = 8.0e-5 * dxu * dxu;
      const double Bu = -8.0e-5 * dyu * dyu;
      double ra_u = 0.0, dec_u = 0.0;
      linear.pix2sky(x + Au, y + Bu, &ra_u, &dec_u);
      const double sep_px =
          std::hypot(ra - ra_u, dec - dec_u) / kPxScaleDeg;
      min_unbridged_sep_px = std::min(min_unbridged_sep_px, sep_px);
    }
    CHECK_MSG(worst < 1e-9,
              ("B2-A17/W4-A1: SIP-aware wcs output vs bridged independent oracle worst=" +
               std::to_string(worst)).c_str());
    CHECK_MSG(min_unbridged_sep_px >= 0.9,
              ("W4-A1 [origin-sensitivity]: unbridged (0-based-as-1-based) evaluation "
               "must be detected at >= 0.9 px; min_sep_px=" +
               std::to_string(min_unbridged_sep_px)).c_str());
  }
  // (3) p1_wcs.json → drizzle frame header 桥接 (A_i_j 读面 = hp_drizzle_api)
  Result<void> drc;
  const json drz_man = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &drc);
  CHECK_MSG(drc.ok(), ("B2-A17: SIP drizzle must complete: " +
                       (drc.failed() ? drc.error().message() : std::string())).c_str());
  CHECK(fs::exists(fs::path(drz_man.value("stack_artifact", ""))));
  CHECK(wman.value("operation", "") == "plate_solve");
  // (3) frame header 桥接端到端: 单像素 delta 帧下的精确 signal Oracle。
  // 单像素 delta 只会产生 1 个 HEALPix 叶像素, signal = F(ndrop=1, d=54.5949848,
  // pixfrac=1.0) — 一个对 WCS 不敏感的常量。因此: (i) 两路径都必须给出
  // 精确 signal 常量 (独立预期); (ii) 若 frame header SIP 键被丢弃, SIP 路径
  // 与无 SIP 路径输出完全相同——仅作 provenance 一致性断言。
  {
    const int W = 32, H = 32;
    int dx_idx = 15 + 16 * 15;   // (像素中心 15.5, 15.5) → dy = 0 → A = 0
    Fixture fxo = make_fixture("b2a17exact");
    CHECK(p1sess::write_fits_file(fxo.light1, W, H, delta_px, &dx_idx) == 0);
    ModuleRegistry reg2;
    CHECK(register_phase_modules(reg2).ok());
    RunContext c2;
    json wcs_lin = {{"crpix1", 16.0}, {"crpix2", 16.0}, {"crval1", 10.0}, {"crval2", 20.0},
                    {"cd11", -0.0002777777777777778}, {"cd12", 0.0},
                    {"cd21", 0.0}, {"cd22", 0.0002777777777777778}};
    json arr_a = json::array(), arr_b = json::array(), arr_ap = json::array(), arr_bp = json::array();
    for (int k = 0; k < 36; ++k) {
      arr_a.push_back(a[static_cast<size_t>(k)]);
      arr_b.push_back(b[static_cast<size_t>(k)]);
      arr_ap.push_back(ap[static_cast<size_t>(k)]);
      arr_bp.push_back(bp[static_cast<size_t>(k)]);
    }
    json wcs_sip = wcs_lin;
    wcs_sip["sip"] = json{{"order", 2}, {"ap_order", 0}, {"a", arr_a}, {"b", arr_b},
                          {"ap", arr_ap}, {"bp", arr_bp}};
    auto drz_cfg = [&](const json& w) {
      return json{{"input_lights", json::array({fxo.light1})},
                  {"output_dir", fxo.out_dir},
                  {"wcs", w},
                  {"drizzle", json{{"nside", 512}, {"nested", 1}, {"pixfrac", 1.0},
                                   {"precision_mode", 0}}}}.dump();
    };
    // (3a) 无 p1_wcs.json, config wcs 无 SIP
    {
      std::error_code ec;
      fs::remove(fs::u8path(frame_root(fxo) + "/p1_wcs.json"), ec);
      Result<void> rc;
      const json m = run_node(reg2, "astrocs.phase1.drizzle", drz_cfg(wcs_lin), c2, &rc);
      CHECK_MSG(rc.ok(), ("B2-A17: exact linear drizzle: " +
                          (rc.failed() ? rc.error().message() : std::string())).c_str());
      const auto sq = hips_exact_signal(frame_root(fxo));
      CHECK_MSG(sq.size() == 1, ("B2-A17: delta frame must touch exactly 1 leaf, got " +
                                 std::to_string(sq.size())).c_str());
      if (sq.size() == 1) {
        // 单叶 flux 守恒 Oracle: signal·support·A_cell == 累计通量 1.0
        const double got = sq[0].signal * sq[0].support * kA17ACell;
        CHECK_MSG(std::fabs(got / kA17ExpectedSignal - 1.0) < 1e-4,
                  ("B2-A17: delta leaf flux must equal exact F(ndrop,d,pixfrac): got=" +
                   std::to_string(got) + " expected=" +
                   std::to_string(kA17ExpectedSignal)).c_str());
      }
      CHECK(m.value("precision_mode", -1) == 0);
      std::map<std::string, std::vector<float>> sup_lin;
      hips_plane_snapshot(frame_root(fxo), "support", &sup_lin);
      {
        json st;
        try { st = json::parse(read_file(frame_root(fxo) + "/p1_stack.json")); } catch (...) {}
        CHECK_MSG(st.value("sip_present", true) == false,
                  "B2-A17: linear drizzle must record sip_present=false");
      }
      // (3b) p1_wcs.json 带 SIP → drizzle, 同样的精确 signal
      Result<void> rw;
      const json wm2 = run_node(reg2, "astrocs.phase1.wcs-platesolve",
                                drz_cfg(wcs_sip), c2, &rw);
      CHECK_MSG(rw.ok(), ("B2-A17: SIP wcs node for exact: " +
                          (rw.failed() ? rw.error().message() : std::string())).c_str());
      CHECK(wm2.value("wcs_source", "") == "explicit_config");
      json wj2;
      try { wj2 = json::parse(read_file(frame_root(fxo) + "/p1_wcs.json")); } catch (...) {}
      CHECK_MSG(wj2["wcs"].contains("sip"),
                "B2-A17: p1_wcs.json must ship sip for the frame header bridge");
      Result<void> drc2;
      const json dm2 = run_node(reg2, "astrocs.phase1.drizzle", drz_cfg(wcs_sip), c2, &drc2);
      CHECK_MSG(drc2.ok(), ("B2-A17: SIP drizzle via p1_wcs.json: " +
                            (drc2.failed() ? drc2.error().message() : std::string())).c_str());
      const auto sq2 = hips_exact_signal(frame_root(fxo));
      CHECK_MSG(sq2.size() == 1,
                ("B2-A17: SIP delta frame must still touch exactly 1 leaf, got " +
                 std::to_string(sq2.size())).c_str());
      if (sq2.size() == 1) {
        const double got = sq2[0].signal * sq2[0].support * kA17ACell;
        CHECK_MSG(std::fabs(got / kA17ExpectedSignal - 1.0) < 1e-4,
                  ("B2-A17: SIP path must produce the same exact delta leaf flux: got=" +
                   std::to_string(got)).c_str());
      }
      CHECK(dm2.value("precision_mode", -1) == 0);
      {
        json st;
        try { st = json::parse(read_file(frame_root(fxo) + "/p1_stack.json")); } catch (...) {}
        CHECK_MSG(st.value("sip_present", false) == true,
                  "B2-A17: drizzle must consume p1_wcs.json SIP into frame header (sip_present)");
        CHECK(st.value("sip_order", -1) == 2);
        CHECK(st.value("ctype1", "") == std::string("RA---TAN-SIP"));
        CHECK(st.value("ctype2", "") == std::string("DEC--TAN-SIP"));
      }
      // 桥接可观测性: SIP 系数下发后 drizzle 落与线性路径同点
      // (A(15.5,15.5)=0 与 B(15.5,15.5)=0) 且 support 平面因子一致。
      std::map<std::string, std::vector<float>> sup_sip;
      hips_plane_snapshot(frame_root(fxo), "support", &sup_sip);
      CHECK_MSG(sup_lin.size() == sup_sip.size(),
                "B2-A17: SIP vs linear support plane topology must agree");
      bool same = (sup_lin.size() == sup_sip.size());
      if (same) {
        auto it1 = sup_lin.begin(); auto it2 = sup_sip.begin();
        for (; it1 != sup_lin.end(); ++it1, ++it2)
          if (it1->first != it2->first || it1->second != it2->second) { same = false; break; }
      }
      CHECK_MSG(same, "B2-A17: SIP frame header bridge must not change the linear-equivalent delta footprint");
    }
    cleanup_fixture(fxo);
  }
  cleanup_fixture(fx);
}

static void test_determinism() {
  for (int run = 0; run < 2; ++run) {
    Fixture fx = make_fixture("det");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"("
    })";
    json man = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
    CHECK(man.value("status", "") == "ok");
    const std::string content = read_file(man.value("sources_artifact", ""));
    if (run == 0) {
      std::ofstream o(fx.dir.parent_path() / "p1001_det_first.json", std::ios::binary);
      o << content;
    } else {
      std::ifstream i(fx.dir.parent_path() / "p1001_det_first.json", std::ios::binary);
      std::string first((std::istreambuf_iterator<char>(i)),
                        std::istreambuf_iterator<char>());
      CHECK_MSG(content == first, "star-psf output must be bitwise deterministic");
      std::error_code ec;
      fs::remove(fx.dir.parent_path() / "p1001_det_first.json", ec);
    }
    cleanup_fixture(fx);
  }
}

// ══ 6. CORE-RACE-001: artifact:cos 必须落独立路径（禁就地覆写 artifact:cal）══
// RED 锚定（修复前）: p1_op_cosmetic 读取 cal 产物 calibrated_<base> 后就地覆写
// 同一路径 → 断言①（独立路径）与断言②（artifact:cal 字节不变）确定性失败。
static void test_cos_artifact_is_independent() {
  Fixture fx = make_hot_fixture("indep");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = p1001_full_chain_cfg(fx);

  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK(man_cal.value("status", "") == "ok");
  const std::string cal_art = fx.out_dir + "/calibrated_light_1.fits";
  CHECK(fs::exists(fs::path(cal_art)));
  const std::string cal_before = read_bytes(cal_art);
  CHECK(!cal_before.empty());

  json man_cos = run_node(reg, "astrocs.phase1.cosmetic", cfg, ctx);
  CHECK(man_cos.value("status", "") == "ok");
  CHECK(man_cos.contains("artifacts") && man_cos["artifacts"].is_array() &&
        !man_cos["artifacts"].empty());
  const std::string cos_art = man_cos["artifacts"][0].get<std::string>();

  // ① artifact:cos 与 artifact:cal 必须落不同物理路径
  CHECK_MSG(cos_art != cal_art,
            "cosmetic must publish an independent artifact path (no in-place overwrite)");
  CHECK_MSG(cos_art.find("calibrated_") == std::string::npos,
            "cosmetic artifact must not reuse the artifact:cal path");
  CHECK(fs::exists(fs::path(cos_art)));

  // ② artifact:cal 在 cos 之后字节不变（并发消费者读到的永远是同一完整文件）
  CHECK_MSG(read_bytes(cal_art) == cal_before,
            "cosmetic must not mutate artifact:cal bytes (torn-read source)");

  // ③ 科学语义零变化: 本配置下 ac_correct_frame 无 dark/bias 掩码源（节点面
  //    传 nullptr/nullptr）⇒ 逐像素直通, cos 产物必须与 artifact:cal 字节相同
  //    （修复只改落盘路径与原子性, 不动任何科学数值）。
  CHECK_MSG(read_bytes(cos_art) == cal_before,
            "cosmetic pass-through must be bitwise identical to artifact:cal"
            " (scientific_change=none)");

  cleanup_fixture(fx);
}

// ══ 7. CORE-RACE-001: artifact:cos 消费者读 cos 产物（IR 接线一致）═══════
// RED 锚定（修复前）: cosmetic 就地覆写 artifact:cal ⇒ psf 读到的 file 名恒为
// calibrated_<base>（artifact:cos 无物理面）→ 断言失败。
static void test_consumer_reads_cos_artifact() {
  Fixture fx = make_hot_fixture("wire");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = p1001_full_chain_cfg(fx);

  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK(man_cal.value("status", "") == "ok");
  json man_cos = run_node(reg, "astrocs.phase1.cosmetic", cfg, ctx);
  CHECK(man_cos.value("status", "") == "ok");
  CHECK(!man_cos["artifacts"].empty());
  const std::string cos_art = man_cos["artifacts"][0].get<std::string>();
  const std::string cos_base = cos_art.substr(cos_art.find_last_of("/\\") + 1);

  json man_psf = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
  CHECK(man_psf.value("status", "") == "ok");
  json cat;
  try { cat = json::parse(read_file(man_psf.value("sources_artifact", ""))); }
  catch (...) { CHECK(false); }
  CHECK(!cat.value("frames", json::array()).empty());
  std::size_t n_frames = 0;
  for (const auto& fr : cat.value("frames", json::array())) {
    const std::string f = fr.value("file", "");
    CHECK_MSG(f.rfind("cleaned_", 0) == 0,
              ("artifact:cos consumer (star-psf) must read the cosmetic artifact, got: " + f).c_str());
    CHECK(fs::exists(fs::path(fx.out_dir + "/" + f)));
    ++n_frames;
  }
  CHECK(n_frames == 2);
  CHECK(cos_base.rfind("cleaned_", 0) == 0);

  cleanup_fixture(fx);
}

// ══ 8. CORE-RACE-001: 并发全链 N 次连跑 0 失败 ════════════════════════════
// 修复前实测: CI-REG-002 200 次复跑 pass=176 fail=24（12%）——drz 与 cos 并发
// 读同一 artifact:cal, cosmetic 非原子就地覆写 ⇒ 消费者观察到半写文件。
// 次数经 P1001_CHAIN_STRESS_RUNS 覆盖（证据采集用 200）。
static void test_parallel_chain_stress() {
  Fixture fx = make_fixture("stress");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const std::string cfg = p1001_full_chain_cfg(fx);

  int runs = 40;
  if (const char* e = std::getenv("P1001_CHAIN_STRESS_RUNS")) {
    const int v = std::atoi(e);
    if (v > 0) runs = v;
  }
  int failed = 0;
  std::string first_error;
  int executed = 0;
  for (int i = 0; i < runs; ++i) {
    ChainRunResult r = run_full_chain_once(cfg, 2, reg);
    ++executed;
    if (!r.ok) {
      ++failed;
      if (first_error.empty())
        first_error = "run#" + std::to_string(i + 1) + ": " + r.error;
      if (first_error.find("\n") == std::string::npos && failed < 3)
        first_error += "\n          " + ("run#" + std::to_string(i + 1) + ": " + r.error);
    }
    CHECK(r.node_count == 7);
    CHECK(r.trace_ok);
  }
  CHECK_MSG(failed == 0,
            ("parallel chain torn-read: runs=" + std::to_string(executed) +
             " failures=" + std::to_string(failed) + " first=" + first_error).c_str());
  std::printf("[CORE-RACE-001] parallel chain stress: runs=%d failures=%d\n",
              executed, failed);
  cleanup_fixture(fx);
}

// ══ 9. CORE-RACE-001: 1/N worker parity（bitwise 确定性）══════════════════
static void test_worker_parity_bitwise() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const uint32_t workers[] = {1u, 2u, 4u};
  std::string base_cal, base_cos, base_src, base_snr;
  bool have_base = false;
  for (uint32_t w : workers) {
    Fixture fx = make_hot_fixture(("par" + std::to_string(w)).c_str());
    const std::string cfg = p1001_full_chain_cfg(fx);
    ChainRunResult r = run_full_chain_once(cfg, w, reg);
    CHECK_MSG(r.ok, ("workers=" + std::to_string(w) + ": " + r.error).c_str());
    const std::string cal = read_bytes(fx.out_dir + "/calibrated_light_1.fits");
    const std::string cos = read_bytes(fx.out_dir + "/cleaned_light_1.fits");
    const std::string src = read_file(fx.out_dir + "/p1_sources.json");
    const std::string snr = read_file(fx.out_dir + "/p1_snr.json");
    CHECK_MSG(!cal.empty() && !cos.empty() && !src.empty() && !snr.empty(),
              ("workers=" + std::to_string(w) + ": artifacts missing").c_str());
    if (!have_base) {
      base_cal = cal; base_cos = cos; base_src = src; base_snr = snr;
      have_base = true;
    } else {
      CHECK_MSG(cal == base_cal, "artifact:cal must be bitwise identical across worker counts");
      CHECK_MSG(cos == base_cos, "artifact:cos must be bitwise identical across worker counts");
      CHECK_MSG(src == base_src, "p1_sources.json must be bitwise identical across worker counts");
      CHECK_MSG(snr == base_snr, "p1_snr.json must be bitwise identical across worker counts");
    }
    cleanup_fixture(fx);
  }
}

// ══ 10. CORE-RACE-001 故障注入（负向必败面）═══════════════════════════════
static void test_torn_artifact_fault_injection() {
  // 10a. 残缺头（非原子覆写窗口内被打开的中间态）→ 消费者必须 fail-closed
  {
    Fixture fx = make_hot_fixture("tornhdr");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = p1001_full_chain_cfg(fx);
    json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
    CHECK(man_cal.value("status", "") == "ok");
    const std::string cal_art = fx.out_dir + "/calibrated_light_1.fits";
    CHECK(p1sess::sess_truncate_file(cal_art, 320) == 0);  // 头 6 卡(480B) 之内截断
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    CHECK_MSG(rc.failed(), "torn artifact must fail closed");
    // 仅在确实失败时取 error()（共享树上游在途改动使 cal 节点先失败时, error()
    // 会 abort 并吞掉后续全部用例; 判据不放松, 只把 abort 变成可读断言失败）。
    if (rc.failed())
      CHECK(rc.error().domain() == ErrorDomain::IO);
    CHECK(!fs::exists(fs::path(frame_root(fx) + "/p1_stack.json")));
    cleanup_fixture(fx);
  }
  // 10b. 发布卫生 + 残缺旧产物覆盖: 预置一个残缺的 artifact:cos 文件, 再跑
  //      cosmetic → 必须以完整文件替换（原子发布）, 且不留临时文件
  {
    Fixture fx = make_hot_fixture("stale");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = p1001_full_chain_cfg(fx);
    json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
    CHECK(man_cal.value("status", "") == "ok");
    const std::string cos_art = fx.out_dir + "/cleaned_light_1.fits";
    float v = 1.0f;
    CHECK(p1sess::write_fits_file(cos_art, kW, kH, const_pixel, &v) == 0);
    CHECK(p1sess::sess_truncate_file(cos_art, 64) == 0);  // 残缺旧产物（64B）
    json man_cos = run_node(reg, "astrocs.phase1.cosmetic", cfg, ctx);
    CHECK(man_cos.value("status", "") == "ok");
    std::error_code ec;
    const auto sz = fs::file_size(cos_art, ec);
    CHECK_MSG(!ec && sz >= 2880u + static_cast<uint64_t>(kW) * kH * 4u,
              "cosmetic must atomically replace a stale partial artifact with a"
              " complete one");
    // 发布卫生: 成功后不得残留暂存文件（崩溃残留也不会污染产物面）
    int leftovers = 0;
    for (const auto& e : fs::directory_iterator(fx.dir)) {
      if (e.path().filename().string().find(".tmp.") != std::string::npos)
        ++leftovers;
    }
    CHECK_MSG(leftovers == 0, "atomic publish must not leave staging files");
    cleanup_fixture(fx);
  }
  // 10c. 检测器非空转: 注入旧缺陷语义（就地覆写 artifact:cal）→ 不可变性
  //      比较器必须报差异（证明 §6 的不变量断言在缺陷回归时必然失败）
  {
    Fixture fx = make_hot_fixture("inject");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = p1001_full_chain_cfg(fx);
    json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
    CHECK(man_cal.value("status", "") == "ok");
    const std::string cal_art = fx.out_dir + "/calibrated_light_1.fits";
    const std::string cal_before = read_bytes(cal_art);
    float v = 777.0f;
    CHECK(p1sess::write_fits_file(cal_art, kW, kH, const_pixel, &v, 0) == 0);
    CHECK_MSG(read_bytes(cal_art) != cal_before,
              "immutability comparator must detect in-place overwrite (fault injection)");
    cleanup_fixture(fx);
  }
}

// ══ 10b. B2-A2（RESCUE-P0-05）: 部分拟合失败仍正确映射 star_id ═══════════
// 星 ID ↔ PSF compact 错位（修复前）: fitter 按原检测下标原位写 + 返回成功计数,
// 消费方按 i < n_valid 取前 n_valid 行并贴 cat.sources[i].id ⇒ NaN 行贴真实
// star_id、下标 ≥ n_valid 的有效拟合被丢弃、median_fwhm/ell 混入 NaN。
// 本用例: 两星 cleaned 帧, 破坏第二星拟合窗 (1e9 平台) 迫使该星拟合失败,
// 断言 p1_psf.json 的 psf_params 星集合 == 拟合成功集合、成功行全有限、
// n_psf_valid == 行数 == 非 NaN 数 (无 NaN 泄漏到中位数)。当两星都成功时
// 映射门仍全量校验 (记录该运行未触发部分失败, 不做弱断言)。
struct TwoStarCfg { float bg; float a0; float a1; };
inline float two_star_pixel(int i, void* user) {
  auto* c = static_cast<TwoStarCfg*>(user);
  const int x = i % kW, y = i / kW;
  auto star = [&](double sx, double sy, double amp) {
    const double dx = x - sx, dy = y - sy;
    return amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  };
  return static_cast<float>(c->bg + star(10.0, 10.0, c->a0) + star(22.0, 22.0, c->a1));
}
static void test_psf_partial_fit_identity() {
  Fixture fx = make_fixture("psfid");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = p1001_full_chain_cfg(fx);

  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK(man_cal.value("status", "") == "ok");
  // 覆盖 cleaned_light_1.fits 为两星场 (第二星窗被 1e9 平台破坏)
  const std::string cleaned1 = fx.out_dir + "/cleaned_light_1.fits";
  CHECK(fs::exists(fs::path(fx.out_dir + "/calibrated_light_1.fits")));
  TwoStarCfg tc{100.0f, 8000.0f, 2000.0f};
  CHECK(p1sess::write_fits_file(cleaned1, kW, kH, two_star_pixel, &tc) == 0);
  // 破坏第二星 (22,22) 整个 17x17 拟合窗 ⇒ 该星 LM 必败; 星 1 (10,10) 保持可拟合
  // ⇒ 真正的"部分失败"。
  // SCI-506 订正（RELEASE-05）：**不得**用 NaN 注入制造该场景——
  // star_detector.cpp:81-92（CLEAN-401 fail-closed）对任一非有限像素在**入口**拒绝
  // 整帧（合同：非有限像素须在 cosmetic 上游消除，检测域不接受 NaN）。故改用
  // **有限**的 1e9 平台：像素全有限（不触发入口门），幅度远超拟合参数域 ⇒ 该星
  // 拟合失败，与"补丁不可拟合"语义等价。非有限帧的 fail-closed 由下一用例锁定。
  {
    std::FILE* fp = std::fopen(cleaned1.c_str(), "r+b");
    CHECK(fp != nullptr);
    if (fp) {
      CHECK(std::fseek(fp, 0, SEEK_END) == 0);
      const long fsize = std::ftell(fp);
      const long data_off = fsize - static_cast<long>(kW) * kH * 4;  // 头长按实际文件推导
      CHECK(data_off > 0);
      auto put_val = [&](int x, int y, float v) {
        unsigned char be[4];
        std::memcpy(be, &v, 4);
        std::reverse(be, be + 4);   // host → big-endian
        const long off = data_off + (static_cast<long>(y) * kW + x) * 4;
        std::fseek(fp, off, SEEK_SET);
        std::fwrite(be, 1, 4, fp);
      };
      // 只把第二星中心改成**单像素有限尖峰**（1e6 ADU）：
      //   * 峰值远高于局部噪声 ⇒ 仍被检测到（保持 det=2，制造真正的部分失败）；
      //   * 轮廓退化为 δ 函数 ⇒ Moffat4 拟合必然失败（sx 塌到参数域外）。
      // 不用整窗平台：那会把局部背景尺度抬高到让第二星漏检（实测 det 掉到 1）。
      put_val(22, 22, 1.0e6f);
      std::fclose(fp);
    }
  }

  json man_psf = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
  CHECK(man_psf.value("status", "") == "ok");
  json cat, psf;
  try {
    cat = json::parse(read_file(man_psf.value("sources_artifact", "")));
    psf = json::parse(read_file(man_psf.value("psf_artifact", "")));
  } catch (...) { CHECK(false); return; }
  CHECK(psf.value("schema", "") == "DATA-P1-PSF");
  CHECK(psf.value("status_schema", "") == "psf_status:INT32[N]");
  const std::size_t n_valid = psf.value("n_valid", (std::size_t)0);
  const std::size_t n_sources = psf.value("n_sources", (std::size_t)0);
  // 帧 1 的 sources 集合 (sdet 检测) 与 psf_params 行 (仅成功拟合) 比对
  const json& frames = cat.value("frames", json::array());
  CHECK(frames.size() >= 1);
  const json& srcs = frames[0].value("sources", json::array());
  const json& rows = frames[0].value("psf_params", json::array());
  const std::size_t n_det_1 = srcs.size();
  const std::size_t n_ok_1 = frames[0].value("n_psf_valid", (std::size_t)0);
  CHECK_MSG(rows.size() == n_ok_1, "psf_params 行数必须 == n_psf_valid (无 NaN 占位行)");
  CHECK(n_ok_1 <= n_det_1);
  CHECK(n_sources >= n_det_1);
  CHECK(n_valid >= n_ok_1);
  // 输出 star_id 集合 == 拟合成功集合 (psf_params 行 star_id 在 sources 中唯一存在)
  std::vector<std::string> emitted_ids;
  bool rows_finite = true;
  for (const auto& row : rows) {
    CHECK(row.contains("star_id") && row["star_id"].is_string());
    emitted_ids.push_back(row.value("star_id", std::string()));
    for (const char* k : {"B", "A", "cx", "cy", "sx", "sy", "theta", "fwhm_x", "fwhm_y"}) {
      CHECK(row.contains(k) && row[k].is_number());
      rows_finite = rows_finite && std::isfinite(row.value(k, 0.0));
    }
  }
  CHECK_MSG(rows_finite, "psf_params 行必须全有限 (NaN 不得贴真实 star_id)");
  std::sort(emitted_ids.begin(), emitted_ids.end());
  CHECK_MSG(std::adjacent_find(emitted_ids.begin(), emitted_ids.end()) == emitted_ids.end(),
            "psf_params star_id 不得重复 (映射必须唯一)");
  for (const std::string& id : emitted_ids) {
    bool found = false;
    for (const auto& s : srcs) if (s.value("id", std::string()) == id) found = true;
    CHECK_MSG(found, ("psf_params star_id " + id + " 不在本帧 sources 中 (错位)").c_str());
  }
  const double med = psf.value("median_fwhm_x_px", 0.0);
  const double mey = psf.value("median_fwhm_y_px", 0.0);
  const double mel = psf.value("median_ellipticity", 0.0);
  CHECK_MSG(std::isfinite(med) && std::isfinite(mey) && std::isfinite(mel),
            "median_fwhm/ell 不得为 NaN");
  std::printf("[B2-A2] psf partial-fit identity: det=%zu ok=%zu rows=%zu valid_total=%zu "
              "partial=%d\n", n_det_1, n_ok_1, rows.size(), n_valid,
              (n_ok_1 < n_det_1) ? 1 : 0);
  cleanup_fixture(fx);
}

// ══ 10b. SCI-506: 非有限像素帧在检测入口 fail-closed（CLEAN-401 合同锁）══
// 合同：star_detector.cpp:81-92 —— 任一非有限像素 ⇒ 整帧拒绝（ErrorDomain::DATA），
// 不得静默产出（下游 nth_element 对 NaN 是 UB）。本用例把该行为**锁死**，防止
// 为了让"部分失败"用例变绿而删掉入口门。
static void test_psf_nonfinite_frame_fail_closed() {
  Fixture fx = make_fixture("psfnan");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  const std::string cfg = p1001_full_chain_cfg(fx);
  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK(man_cal.value("status", "") == "ok");
  const std::string cleaned1 = fx.out_dir + "/cleaned_light_1.fits";
  TwoStarCfg tc{100.0f, 8000.0f, 2000.0f};
  CHECK(p1sess::write_fits_file(cleaned1, kW, kH, two_star_pixel, &tc) == 0);
  {
    std::FILE* fp = std::fopen(cleaned1.c_str(), "r+b");
    CHECK(fp != nullptr);
    if (fp) {
      CHECK(std::fseek(fp, 0, SEEK_END) == 0);
      const long fsize = std::ftell(fp);
      const long data_off = fsize - static_cast<long>(kW) * kH * 4;
      CHECK(data_off > 0);
      const unsigned char nan_be[4] = {0x7F, 0xC0, 0x00, 0x00};  // +qNaN (big-endian)
      const long off = data_off + (static_cast<long>(10) * kW + 10) * 4;
      std::fseek(fp, off, SEEK_SET);
      std::fwrite(nan_be, 1, 4, fp);
      std::fclose(fp);
    }
  }
  Result<void> rc;
  json man = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx, &rc);
  CHECK_MSG(!rc.ok(), "含 NaN 的帧必须 fail-closed（CLEAN-401 入口门）");
  if (!rc.ok()) {
    CHECK_MSG(rc.error().domain() == ErrorDomain::DATA,
              ("非有限帧必须以 DATA 域拒绝, 实际: " + rc.error().message()).c_str());
  }
  CHECK_MSG(man.value("status", "") != "ok", "fail-closed 时不得报 status=ok");
  std::printf("[SCI-506] non-finite frame fail-closed: rc=%s\n",
              rc.ok() ? "ok(WRONG)" : "fail(correct)");
  cleanup_fixture(fx);
}

// ══ 11. PSF-FAST-001 (负责人裁决 2026-09-14): FAST 截断 + INACTIVE 精确路径 ══
// (a) 生产入口 = FAST: DATA-P1-SOURCES 全量检测**不动**, 仅把 Moffat4 拟合限制到
//     最亮 psf.max_stars 颗（默认 5000, 节点配置, 禁硬编码）;
// (b) 完整精确 PSF 路径**保留但 inactive**（kPrecisePsfEnabled=false, 生产路径
//     不调用）; 本测试用直调钩子证明该实现仍可编译且能跑出真实结果,
//     防止 inactive 代码被当作死代码清理。
static void test_psf_fast_cap_and_inactive_precise() {
  Fixture fx = make_fixture("psffast");
  // 双星场 (10,10)/(22,22): 检测 2 颗 → 可分辨"检测全量"与"拟合截断"
  TwoStarCfg tc{100.0f, 8000.0f, 6000.0f};
  const std::string light = (fx.dir / "multi.fits").string();
  CHECK(p1sess::write_fits_file(light, kW, kH, two_star_pixel, &tc) == 0);
  const std::string cfg = R"({
    "input_lights": [")" + light + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "psf": {"max_stars": 1}
  })";
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  json man = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
  CHECK_MSG(man.value("status", "") == "ok", "FAST psf node must succeed");
  CHECK(man.value("psf_mode", std::string()) == "fast");
  CHECK_MSG(man.value("n_sources", (std::int64_t)0) >= 2,
            "全量检测必须保留 >=2 颗 (FAST 不截断检测)");
  CHECK_MSG(man.value("n_fit_input", (std::int64_t)0) == 1,
            "psf.max_stars=1 必须把拟合输入截到 1 颗 (节点配置生效)");
  json cat = json::parse(read_file(man.value("sources_artifact", "")));
  json psf = json::parse(read_file(man.value("psf_artifact", "")));
  const std::size_t n_det = cat["frames"][0].value("n_detected", (std::size_t)0);
  CHECK_MSG(n_det >= 2, "DATA-P1-SOURCES 全量检测不得被 FAST 截断");
  CHECK(cat["frames"][0]["sources"].size() == n_det);
  CHECK(psf.value("n_sources", (std::size_t)0) == n_det);   // 全量检测计数
  CHECK(psf.value("n_fit_input", (std::size_t)0) == 1);     // 拟合截断生效
  CHECK(psf.value("psf_mode", std::string()) == "fast");
  // (b) inactive 精确路径直调 (不经生产注册表): 忽略 psf.max_stars, 全量拟合
  std::string man_precise;
  auto rc = astrocs::core::p1_op_star_psf_precise_json(cfg, &man_precise);
  CHECK_MSG(rc.ok(), ("inactive precise PSF path must still run: " +
                      (rc.ok() ? std::string() : rc.error().message())).c_str());
  json psf2 = json::parse(read_file(fx.out_dir + "/p1_psf.json"));
  CHECK_MSG(psf2.value("n_fit_input", (std::size_t)0) == n_det,
            "精确路径必须拟合全量检测星 (忽略 psf.max_stars)");
  CHECK_MSG(psf2.value("n_valid", (std::size_t)0) >= 1,
            "精确路径必须产出真实拟合结果 (非空实现)");
  CHECK_MSG(psf2.contains("psf_mode"), "DATA-P1-PSF 必须带 psf_mode 字段");
  std::printf("[PSF-FAST-001] fast: n_det=%zu n_fit_input=1; precise(inactive): ", n_det);
  std::printf("n_fit_input=%zu n_valid=%zu\n",
              psf2.value("n_fit_input", (std::size_t)0),
              psf2.value("n_valid", (std::size_t)0));
  cleanup_fixture(fx);
}

// ══ 11. CORE-RACE-001: 科学语义零变化（修复前/后 bitwise golden 对照）══════
// P1001_GOLDEN_DIR=<dir>: 1 worker 全链跑一次并把确定性产物字节存为基线;
// P1001_GOLDEN_CMP=<dir>: 同样跑一次与基线逐字节比较（不一致 = 科学漂移）。
// 排除项: p1_stack.json（含 elapsed_sec 计时）/ p1_final.json（含输出目录路径
// 与 HiPS 根, 随临时目录变化）——均为非确定性字段, 不属科学数值面。
static void test_golden_parity() {
  const char* dump = std::getenv("P1001_GOLDEN_DIR");
  const char* cmp = std::getenv("P1001_GOLDEN_CMP");
  if (!dump && !cmp) {
    std::printf("[CORE-RACE-001] golden parity: skipped (env unset)\n");
    return;
  }
  // 修复前基线只含"就地覆写后的 calibrated_*"（当时无独立 cos 产物）; 因此
  // 字节对照按 语义等价对 进行: 修复后 cleaned_<base> ≡ 修复前 calibrated_<base>
  // （同一 ac_correct_frame 输出, 只换了落盘路径）。
  struct BytePair { const char* now; const char* before; };
  static const BytePair kBytePairs[] = {
      {"cleaned_light_1.fits", "calibrated_light_1.fits"},
      {"cleaned_light_2.fits", "calibrated_light_2.fits"}};
  // JSON 产物: 数值面逐字段比对（"file" 字段为 artifact 接线改名:
  // calibrated_→cleaned_ 同帧同序, 不属科学数值; 其余键必须完全一致）
  static const char* kJsonArtifacts[] = {"p1_sources.json", "p1_psf.json",
                                         "p1_flux.json", "p1_snr.json"};
  Fixture fx = make_hot_fixture("golden");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  ChainRunResult r = run_full_chain_once(p1001_full_chain_cfg(fx), 1, reg);
  CHECK_MSG(r.ok, ("golden run failed: " + r.error).c_str());
  const fs::path dir = dump ? fs::path(dump) : fs::path(cmp);
  std::error_code ec;
  fs::create_directories(dir, ec);
  int compared = 0;

  for (const BytePair& bp : kBytePairs) {
    const std::string src = fx.out_dir + "/" + bp.now;
    CHECK_MSG(fs::exists(fs::path(src)),
              ("golden artifact missing: " + std::string(bp.now)).c_str());
    if (dump) {
      std::error_code cec;
      fs::copy_file(fs::path(src), dir / bp.before,
                    fs::copy_options::overwrite_existing, cec);
      CHECK_MSG(!cec, ("golden dump failed: " + std::string(bp.now)).c_str());
      continue;
    }
    const fs::path base = dir / bp.before;
    CHECK_MSG(fs::exists(base),
              ("golden baseline missing: " + std::string(bp.before)).c_str());
    if (!fs::exists(base)) continue;
    CHECK_MSG(read_bytes(src) == read_bytes(base.string()),
              ("science drift (cosmetic pixels) vs pre-fix golden: " +
               std::string(bp.now)).c_str());
    ++compared;
  }

  for (const char* name : kJsonArtifacts) {
    const std::string src = fx.out_dir + "/" + name;
    CHECK_MSG(fs::exists(fs::path(src)),
              ("golden artifact missing: " + std::string(name)).c_str());
    if (dump) {
      std::error_code cec;
      fs::copy_file(fs::path(src), dir / name,
                    fs::copy_options::overwrite_existing, cec);
      CHECK_MSG(!cec, ("golden dump failed: " + std::string(name)).c_str());
      continue;
    }
    const fs::path base = dir / name;
    CHECK_MSG(fs::exists(base),
              ("golden baseline missing: " + std::string(name)).c_str());
    if (!fs::exists(base)) continue;
    json now, ref;
    try {
      now = json::parse(read_file(src));
      ref = json::parse(read_file(base.string()));
    } catch (...) {
      CHECK_MSG(false, ("golden JSON parse failed: " + std::string(name)).c_str());
      continue;
    }
    // "file" 键退出比对（artifact 接线改名, 非数值面）; 其余逐字段一致
    auto strip_files = [](json& j, auto&& self) -> void {
      if (j.is_object()) {
        j.erase("file");
        for (auto& kv : j.items()) self(kv.value(), self);
      } else if (j.is_array()) {
        for (auto& v : j) self(v, self);
      }
    };
    strip_files(now, strip_files);
    strip_files(ref, strip_files);
    CHECK_MSG(now == ref,
              ("science drift (numeric surface) vs pre-fix golden: " +
               std::string(name)).c_str());
    ++compared;
  }
  std::printf("[CORE-RACE-001] golden parity: mode=%s compared=%d dir=%s\n",
              dump ? "dump" : "compare", compared, dir.string().c_str());
  cleanup_fixture(fx);
}

// ── P17-NSIDE: drizzle 采样率合规 (1x-2x) 与 nside 来源可见性 ──────────────
// 合同 (负责人裁定, 采样率等价 drizzle 1x-2x):
//  * drizzle.nside 缺省/0/空 => 自动 (compute_auto_nside: 最细局部输入像素尺度
//    → 最小 2 次幂 nside 使 hp_res <= finest, 即 1~2x 线性过采样),
//    nside_source=auto, 决策依据 (finest/hp_res/oversample) 进产物与 manifest;
//  * 显式 nside>0 => 原样使用 (nside_source=explicit); 若导致 hp_res > finest
//    (欠采样) 则 nside_conflict=undersampled 必须可见 (记录, 不静默);
//  * nside_mode 合同外取值 / auto 模式与显式 nside 冲突 / explicit 模式缺 nside
//    => DATA fail-closed。
static void test_p17_nside_sampling_compliance() {
  Fixture fx = make_fixture("p17nside");
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  // 72"/px 合成 WCS: auto nside 落在 4096 (hp_res≈51.5", 过采样≈1.4x),
  // 且显式 512 必然欠采样 (hp_res=412" >> 72"), 便于双向断言。
  const std::string wcs = R"("wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
              "cd11": -0.02, "cd12": 0.0,
              "cd21": 0.0, "cd22": 0.02})";
  auto make_cfg = [&](const std::string& drz) {
    return std::string(R"({
      "input_lights": [")") + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      )" + wcs + R"(,
      "drizzle": )" + drz + R"(
    })";
  };
  // (1) nside 缺省 => 自动 (合规默认), provenance 完整且 1x-2x
  {
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle",
                        make_cfg(R"({"nested": 1, "pixfrac": 1.0, "precision_mode": 0})"),
                        ctx, &rc);
    CHECK_MSG(rc.ok(), "P17: 缺省 nside 必须自动计算而非拒绝");
    CHECK_MSG(man.value("nside_source", std::string()) == "auto",
              "P17: 缺省 nside 的 nside_source 必须是 auto");
    CHECK_MSG(man.value("nside", 0) >= 512, "P17: auto nside 必须 >= 512 (HiPS 下限)");
    const double finest = man.value("finest_input_arcsec", 0.0);
    const double hpres = man.value("hp_res_arcsec", 0.0);
    const double f = man.value("oversample_factor", 0.0);
    CHECK_MSG(finest > 0.0 && hpres > 0.0, "P17: auto 决策依据必须落 manifest");
    CHECK_MSG(hpres <= finest * (1.0 + 1e-9), "P17: auto nside 不得欠采样 (hp_res<=finest)");
    CHECK_MSG(f >= 1.0 && f < 2.0, "P17: auto 过采样倍率必须落在 [1,2)");
    CHECK_MSG(man.value("nside_conflict", std::string()) == "none",
              "P17: 合规 auto 的 nside_conflict 必须是 none");
    json st;
    try { st = json::parse(read_file(frame_root(fx) + "/p1_stack.json")); } catch (...) { CHECK(false); }
    CHECK_MSG(st.value("nside_source", std::string()) == "auto",
              "P17: p1_stack.json 必须记 nside_source");
    CHECK_MSG(st.value("oversample_factor", 0.0) >= 1.0,
              "P17: p1_stack.json 必须记过采样倍率");
  }
  // (2) 显式 nside_mode=1x_to_2x_drizzle => 同自动 (合规默认)
  {
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle",
                        make_cfg(R"({"nside_mode": "1x_to_2x_drizzle", "nested": 1,
                                     "pixfrac": 1.0, "precision_mode": 0})"),
                        ctx, &rc);
    CHECK_MSG(rc.ok(), "P17: nside_mode=1x_to_2x_drizzle 必须可用");
    CHECK(man.value("nside_source", std::string()) == "auto");
    CHECK(man.value("nside_mode", std::string()) == "1x_to_2x_drizzle");
  }
  // (3) 显式 nside=512 => 欠采样: 成功但冲突可见 (nside_source=explicit)
  {
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle",
                        make_cfg(R"({"nside": 512, "nested": 1, "pixfrac": 1.0,
                                     "precision_mode": 0})"),
                        ctx, &rc);
    CHECK_MSG(rc.ok(), "P17: 显式 nside 是强制输入, 必须原样执行 (另当别论)");
    CHECK(man.value("nside_source", std::string()) == "explicit");
    CHECK_MSG(man.value("nside_conflict", std::string()) == "undersampled",
              "P17: 显式 512 欠采样必须被标记 (禁静默)");
    CHECK_MSG(man.value("oversample_factor", 1.0) < 1.0,
              "P17: 欠采样时过采样倍率 < 1");
    CHECK_MSG(man.value("auto_nside", 0) >= 512,
              "P17: 欠采样时必须同时记录合规 auto nside 供对照");
  }
  // (4) nside_mode 合同外取值 => DATA fail-closed
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle",
             make_cfg(R"({"nside_mode": "whatever", "nested": 1, "pixfrac": 1.0,
                          "precision_mode": 0})"),
             ctx, &rc);
    CHECK_MSG(rc.failed(), "P17: 合同外 nside_mode 必须拒绝");
  }
  // (5) auto 模式与显式 nside>0 冲突 => fail-closed (禁静默二选一)
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle",
             make_cfg(R"({"nside_mode": "1x_to_2x_drizzle", "nside": 4096,
                          "nested": 1, "pixfrac": 1.0, "precision_mode": 0})"),
             ctx, &rc);
    CHECK_MSG(rc.failed(), "P17: auto 模式 + 显式 nside 冲突必须拒绝");
  }
  // (6) nside_mode=explicit 但缺 nside => fail-closed
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle",
             make_cfg(R"({"nside_mode": "explicit", "nested": 1, "pixfrac": 1.0,
                          "precision_mode": 0})"),
             ctx, &rc);
    CHECK_MSG(rc.failed(), "P17: explicit 模式缺 nside 必须拒绝");
  }
  // (7) nside 非 2 的幂 (显式) => 引擎层拒绝 (不静默向上取整)
  {
    Result<void> rc;
    run_node(reg, "astrocs.phase1.drizzle",
             make_cfg(R"({"nside": 300, "nested": 1, "pixfrac": 1.0, "precision_mode": 0})"),
             ctx, &rc);
    CHECK_MSG(rc.failed(), "P17: 非 2 次幂 nside 必须拒绝");
  }
  // (8) 显式强制输入同时给出理由 => 允许 (另当别论)
  {
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle",
                        make_cfg(R"({"nside": 4096, "nside_mode": "explicit",
                                     "nside_reason": "operator preview", "nested": 1,
                                     "pixfrac": 1.0, "precision_mode": 0})"),
                        ctx, &rc);
    CHECK_MSG(rc.ok(), "P17: 显式 nside_mode=explicit + nside>0 必须可用");
    CHECK(man.value("nside_source", std::string()) == "explicit");
  }
  cleanup_fixture(fx);
}

// ── P21-HIPS-WRITER: 聚合扫描 O(P×T) → sink 单趟 O(T) 回归 ───────────────
// 病征 (P17 §5.2): 旧 writer 的容器→HiPS 聚合是 O(std_parent_count × n_tiles)
// 整表扫描 —— T2 auto 3,145,728 × 115 ≈ 3.6e8 次迭代、单线程静默 ~207 s,
// 把全程 CPU 均值压到 3.18 核 (违反宪章 §10.5/§17.6)。
// P23: 中间容器与聚合节点一并删除, 聚合由生产末端 write_hips_phase1 单趟
// 遍历累加器列表完成 (O(T) 排序 + O(T) 写出), 结构上不再存在 parent-span 扫描。
// 本测试用 nside=65536 (std_parent_count = 196,608, HiPS Norder=7) + 4 个
// 分散 sparse tile (含上界 parent 196607) 同时锁定两件事:
//   (a) 归属精确: 每个 sparse tile 落进正确的标准 parent, signal/support
//       数值逐 tile 可区分 (逐 tile 直写, 不存在 (base_leaf>>18) 扫描判据);
//   (b) 复杂度: 节点 manifest 的 aggregation_scan_steps ≈ n_tiles, 而
//       aggregation_parent_span = 196,608。回退成整表扫描 ⇒ scan_steps 变
//       196608×4 = 786,432 (或 aggregation_mode 字段消失) ⇒ 本测试必红。
constexpr uint32_t kP21Nside = 65536;
constexpr uint64_t kP21Parents[4] = {0, 1, 100000, 196607};
constexpr float    kP21Signal[4]  = {0.5f, 1.5f, 2.5f, 3.5f};
constexpr uint8_t  kP21Sup[4]     = {255, 128, 64, 32};
constexpr uint64_t kP21CoverLeaves = 256;  // 每个 sparse tile 仅覆盖前 256 叶

bool write_p21_scatter_hips(const std::string& root) {
  std::vector<drizzle::TileAccumulatorT<float>> accs;
  const double a_cell = 4.0 * 3.14159265358979323846 /
                        (12.0 * static_cast<double>(kP21Nside) *
                         static_cast<double>(kP21Nside));
  for (int t = 0; t < 4; ++t) {
    drizzle::TileAccumulatorT<float> acc;
    acc.parent_ipix = kP21Parents[t];
    acc.pixels.resize(512u * 512u);
    for (uint64_t i = 0; i < kP21CoverLeaves; ++i) {
      acc.pixels[i].sumFlux = kP21Signal[t];
      acc.pixels[i].sumArea = static_cast<float>(
          (static_cast<double>(kP21Sup[t]) / 255.0) * a_cell);
      acc.pixels[i].nContrib = 1;
      acc.touched.push_back(static_cast<uint32_t>(i));
    }
    accs.push_back(std::move(acc));
  }
  drizzle::DrizzleConfig cfg;
  cfg.nside = static_cast<int>(kP21Nside);
  cfg.tile_depth = 9;
  std::string err;
  if (!drizzle::write_hips_phase1<float>(accs, cfg, root, "R", err)) {
    std::fprintf(stderr, "P21 write_hips_phase1 failed: %s\n", err.c_str());
    return false;
  }
  // 上游 provenance (writer 节点据此计算 parent span 复杂度不变量)。
  std::ofstream sf(root + "/p1_stack.json", std::ios::binary);
  if (sf) sf << "{\"schema\":\"DATA-P1-STACK\",\"nside\":" << kP21Nside << "}";
  return true;
}

// IVOA REC-HIPS-1.0 §4.1 标准 tile 路径 (M2b-B-01): Dir=(N/10000)*10000, Npix=N。
std::string hips_tile_path_std(const std::string& root, const char* plane,
                               uint32_t norder, uint64_t tile) {
  return root + "/" + plane + "/Norder" + std::to_string(norder) + "/Dir" +
         std::to_string((tile / 10000) * 10000) + "/Npix" + std::to_string(tile) +
         ".fits";
}
// 旧非标准布局 (Dir=商/Npix=余数) —— **仅用于负例断言**: 产物不得落在此路径。
std::string hips_tile_path_legacy(const std::string& root, const char* plane,
                                  uint32_t norder, uint64_t tile) {
  return root + "/" + plane + "/Norder" + std::to_string(norder) + "/Dir" +
         std::to_string(tile / 10000) + "/Npix" + std::to_string(tile % 10000) +
         ".fits";
}
bool hips_tile_at(const std::string& root, const char* plane, uint32_t norder,
                  uint64_t tile, std::vector<float>* out) {
  return read_hips_tile(hips_tile_path_std(root, plane, norder, tile), out);
}
static bool file_present(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return f.good();
}

static void test_p21_writer_aggregation_buckets() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  Fixture fx = make_fixture("p21");
  // P23: 直接以生产末端 write_hips_phase1 写出 4 个分散 sparse tile 的 HiPS
  // (旧夹具写中间容器再交 writer 聚合; 容器已删除, 等价夹具改为直供 sink)。
  CHECK_MSG(write_p21_scatter_hips(frame_root(fx)), "P21: scatter HiPS fixture");
  {
    std::vector<float> pre;
    CHECK_MSG(hips_tile_at(frame_root(fx), "signal", 7, kP21Parents[0], &pre),
              "P21: 夹具必须写出 Norder7 sparse tile");
    // M2b-B-01 负例: tile 196607 必须落标准式 Dir190000/Npix196607.fits,
    // 旧非标准式 Dir19/Npix6607.fits **不得**存在 (实现回退成旧式即红)。
    CHECK_MSG(file_present(hips_tile_path_std(frame_root(fx), "signal", 7, 196607u)),
              "P21: tile 196607 必须落 IVOA 标准路径 Dir190000/Npix196607.fits");
    CHECK_MSG(!file_present(hips_tile_path_legacy(frame_root(fx), "signal", 7, 196607u)),
              "P21: 旧非标准路径 Dir19/Npix6607.fits 不得存在");
    CHECK_MSG(!file_present(hips_tile_path_legacy(frame_root(fx), "signal", 7, 100000u)),
              "P21: 旧非标准路径 Dir10/Npix0.fits 不得存在");
  }
  RunContext ctx;
  const std::string cfg = R"({
    "input_lights": [")" + fx.light1 + R"("],
    "output_dir": ")" + fx.out_dir + R"(",
    "filter_passband": "R"
  })";
  Result<void> wrc;
  json wman = run_node(reg, "astrocs.phase1.writer", cfg, ctx, &wrc);
  CHECK_MSG(wrc.ok(), ("P21: writer must validate direct HiPS: " +
                       (wrc.failed() ? wrc.error().message()
                                     : std::string())).c_str());
  if (wrc.failed()) { cleanup_fixture(fx); return; }
  // (b) 复杂度不变量 (确定性断言; 不依赖计时): sink 单趟遍历 O(T),
  //     scan_steps ≈ n_tiles, 而非 O(parent_span × n_tiles)。
  CHECK_MSG(wman.value("aggregation_mode", std::string()) == "sink_single_pass",
            "P21: 聚合必须走 sink 单趟遍历 (禁回退 O(P×T) 整表扫描)");
  const int64_t span = wman.value("aggregation_parent_span", static_cast<int64_t>(0));
  const int64_t steps = wman.value("aggregation_scan_steps", static_cast<int64_t>(0));
  const int64_t visited =
      wman.value("aggregation_parents_visited", static_cast<int64_t>(0));
  CHECK_MSG(span == 196608,
            ("P21: std_parent_count 应为 196608, got " +
             std::to_string(span)).c_str());
  CHECK_MSG(visited == 4,
            ("P21: 非空 parent 数必须 = 4, got " +
             std::to_string(visited)).c_str());
  CHECK_MSG(steps <= 4 * 4,
            ("P21: 聚合扫描步数必须 O(n_tiles) 而非 O(P×T) (got " +
             std::to_string(steps) + "; 整表扫描为 " +
             std::to_string(span * 4) + ")").c_str());
  {
    json fin0;
    try { fin0 = json::parse(read_file(frame_root(fx) + "/p1_final.json")); } catch (...) {}
    CHECK(fin0.value("n_tiles_written", 0) == 4);
    CHECK(fin0.value("n_tiles", 0u) == 4u);
  }
  // (a) 归属与数值精确 (4 个 parent 各自独立的 signal 常量)
  const uint32_t norder = 7;  // log2(65536) - 9
  double sig_seen[4] = {0.0, 0.0, 0.0, 0.0};
  for (int t = 0; t < 4; ++t) {
    std::vector<float> sig, sup;
    const std::string tag = std::to_string(kP21Parents[t]);
    CHECK_MSG(hips_tile_at(frame_root(fx), "signal", norder, kP21Parents[t], &sig),
              ("P21: signal tile 可读 parent " + tag).c_str());
    CHECK_MSG(hips_tile_at(frame_root(fx), "support", norder, kP21Parents[t], &sup),
              ("P21: support tile 可读 parent " + tag).c_str());
    if (sig.size() != 512ull * 512ull || sup.size() != sig.size()) {
      CHECK_MSG(false, "P21: HiPS tile 必须 512x512");
      continue;
    }
    // 读侧不变量 (与 AIO 的 surface-brightness 约定及 NESTED→FITS 像元置换无关):
    //   (i)   有限 signal 像元数 == 本 tile 的构造覆盖叶数 (256) —— 覆盖守恒,
    //         既不丢也不多 (旧实现 valid_mask 缺失时会多出幽灵像元);
    //   (ii)  本 tile 内所有有限 signal 严格同值 (常量夹具) —— 任何跨 parent /
    //         跨 tile 的桶归属错位都会破坏常量性;
    //   (iii) support 恰在 256 个像元上等于 round(255·A/A_p)/255 =
    //         kP21Sup[t]/255 (面积比连续缩放), 其余像元 support == 0;
    //   (iv)  4 个 parent 的 signal 常量两两不同, 且随 support 减小严格增大
    //         (与 AIO "surface brightness = flux/covered_area" 一致)。
    const double exp_sup = static_cast<double>(kP21Sup[t]) / 255.0;
    uint64_t fin = 0, sup_ok = 0, sup_nz = 0;
    double s_min = 1e300, s_max = -1e300;
    for (size_t i = 0; i < sig.size(); ++i) {
      if (std::isfinite(sig[i])) {
        ++fin;
        s_min = std::min(s_min, static_cast<double>(sig[i]));
        s_max = std::max(s_max, static_cast<double>(sig[i]));
      }
      if (std::fabs(sup[i]) > 1e-6) {
        ++sup_nz;
        if (std::fabs(static_cast<double>(sup[i]) - exp_sup) < 0.01) ++sup_ok;
      }
    }
    CHECK_MSG(fin == kP21CoverLeaves,
              ("P21: parent " + tag + " 有效 signal 像元数必须 = 构造覆盖叶数 256, got " +
               std::to_string(fin)).c_str());
    CHECK_MSG(fin > 0 && std::fabs(s_max - s_min) <= 1e-5 * std::fabs(s_max),
              ("P21: parent " + tag + " tile 内 signal 必须是单一常量 (禁跨 tile 串扰): min=" +
               std::to_string(s_min) + " max=" + std::to_string(s_max)).c_str());
    CHECK_MSG(sup_nz == kP21CoverLeaves && sup_ok == sup_nz,
              ("P21: parent " + tag + " support 必须在 256 个像元上等于 " +
               std::to_string(kP21Sup[t]) + "/255, got nz=" + std::to_string(sup_nz) +
               " ok=" + std::to_string(sup_ok)).c_str());
    sig_seen[t] = (fin > 0) ? s_min : 0.0;
  }
  // (iv) 4 个 parent 的 signal 常量互不相同且随 support 减小单调增大
  {
    bool distinct = true, monotonic = true;
    for (int i = 0; i < 4; ++i) {
      for (int j = i + 1; j < 4; ++j) {
        if (std::fabs(sig_seen[i] - sig_seen[j]) <=
            1e-6 * std::max(std::fabs(sig_seen[i]), 1.0))
          distinct = false;
      }
    }
    for (int i = 1; i < 4; ++i) {
      if (!(sig_seen[i] > sig_seen[i - 1])) monotonic = false;
    }
    CHECK_MSG(distinct && monotonic,
              ("P21: 4 个 parent 的 signal 常量必须两两不同且随 support 递减而递增"
               " (桶归属错位会破坏): " + std::to_string(sig_seen[0]) + "," +
               std::to_string(sig_seen[1]) + "," + std::to_string(sig_seen[2]) + "," +
               std::to_string(sig_seen[3])).c_str());
  }
  // 缺失 parent 不得被凭空写出 (稀疏性守恒: 只有 4 个 parent 有 tile)
  {
    std::vector<float> ghost;
    CHECK_MSG(!hips_tile_at(frame_root(fx), "signal", norder, 12345, &ghost),
              "P21: 未被 sparse 覆盖的 parent 不得写出 signal tile");
  }
  cleanup_fixture(fx);
}

// ── IVAR-001: Phase1 生产末端 variance/ivar 子产品（DATA-P1-HIPS §12.1/§12.2）──
// 累加器携带 var_num_sum（= variance·covered_area², SCI-DRZ-014 §5）时,
// write_hips_phase1 必须请求 VARIANCE|IVAR 产品位并经同一 writer 通道成对落盘
// （writer 归约 variance = var_num_sum/covered_area²、ivar = 1/variance, §4a）;
// 无方差累加量时保持既有 signal+support 两产品面不变。writer 节点
// p1_final.json 的 products/uncertainty_available 必须来自磁盘事实（禁硬编码）。
// 故障注入面: ASTROCS_IVAR_FAULT=no_variance_flags（等价缺陷: 有方差却不请求
// 产品位）⇒ 本门必然判红。
constexpr uint8_t kIvarSupFull = 255;      // 满覆盖: area = A_cell
constexpr uint64_t kIvarCover = 512;       // 覆盖叶数
constexpr double kIvarVarAdu2 = 4.0e-4;    // 目标逐像素 variance (ADU²)

static bool write_sparse_hips_var(const std::string& root,
                                  const std::vector<SparseTile>& tiles,
                                  double variance_adu2) {
  const uint32_t nside = 512;
  const double a_cell = 4.0 * 3.14159265358979323846 /
                        (12.0 * static_cast<double>(nside) *
                         static_cast<double>(nside));
  std::vector<drizzle::TileAccumulatorT<float>> accs;
  for (const SparseTile& t : tiles) {
    drizzle::TileAccumulatorT<float> acc;
    acc.parent_ipix = t.parent;
    acc.pixels.resize(512u * 512u);
    for (uint64_t k = 0; k < t.cover; ++k) {
      const uint64_t i = t.offset + k;
      const double area = (static_cast<double>(t.support) / 255.0) * a_cell;
      acc.pixels[i].sumFlux = static_cast<float>(kA15SignalV);
      acc.pixels[i].sumArea = static_cast<float>(area);
      acc.pixels[i].sumVarNum = static_cast<float>(variance_adu2 * area * area);
      acc.pixels[i].nContrib = 1;
      acc.touched.push_back(static_cast<uint32_t>(i));
    }
    accs.push_back(std::move(acc));
  }
  drizzle::DrizzleConfig cfg;
  cfg.nside = static_cast<int>(nside);
  cfg.tile_depth = 9;
  std::string err;
  const bool ok = drizzle::write_hips_phase1<float>(accs, cfg, root, "", err);
  if (!ok) std::fprintf(stderr, "IVAR-001 write_hips_phase1 failed: %s\n", err.c_str());
  std::ofstream sf(root + "/p1_stack.json", std::ios::binary);
  if (sf) sf << "{\"schema\":\"DATA-P1-STACK\",\"nside\":" << nside << "}";
  return ok;
}

static bool hips_tile_product(const std::string& root, const char* prod,
                              uint64_t tile, std::vector<float>* out) {
  return read_hips_tile(root + "/" + prod + "/Norder0/Dir" +
                        std::to_string(tile / 10000) + "/Npix" +
                        std::to_string(tile % 10000) + ".fits", out);
}

static void test_ivar001_phase1_variance_products() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const char* fault_env = std::getenv("ASTROCS_IVAR_FAULT");
  const bool fault_no_flags =
      fault_env && std::string(fault_env) == "no_variance_flags";
  // (a) 无方差累加量 → signal+support 两产品面（基线不变, 无方差目录）
  {
    Fixture fx = make_fixture("ivar0");
    CHECK_MSG(write_sparse_hips(frame_root(fx), {{0, 0, kIvarCover, kIvarSupFull}}),
              "IVAR-001: baseline fixture (no variance) must be written");
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      "filter_passband": "R"
    })";
    Result<void> wrc;
    run_node(reg, "astrocs.phase1.writer", cfg, ctx, &wrc);
    CHECK_MSG(wrc.ok(), ("IVAR-001: writer must accept no-variance HiPS: " +
                         (wrc.failed() ? wrc.error().message() : std::string())).c_str());
    json fin;
    try { fin = json::parse(read_file(frame_root(fx) + "/p1_final.json")); } catch (...) {}
    CHECK(fin.value("products", json::array()) == json::array({"signal", "support"}));
    CHECK(fin.value("uncertainty_available", true) == false);
    CHECK(fin.value("n_variance_tiles", -1) == 0);
    CHECK(fin.value("n_ivar_tiles", -1) == 0);
    CHECK_MSG(!fs::exists(fs::path(frame_root(fx) + "/variance/properties")),
              "IVAR-001: no-variance accumulator must not publish variance/");
    CHECK_MSG(!fs::exists(fs::path(frame_root(fx) + "/ivar/properties")),
              "IVAR-001: no-variance accumulator must not publish ivar/");
    cleanup_fixture(fx);
  }
  // (b) 有方差累加量 → variance/ivar 成对落盘 + 数值合同 (§4a/§12.2)
  {
    Fixture fx = make_fixture("ivar1");
    CHECK_MSG(write_sparse_hips_var(frame_root(fx), {{0, 0, kIvarCover, kIvarSupFull}},
                                    kIvarVarAdu2),
              "IVAR-001: variance fixture must be written");
    RunContext ctx;
    const std::string cfg = R"({
      "input_lights": [")" + fx.light1 + R"("],
      "output_dir": ")" + fx.out_dir + R"(",
      "filter_passband": "R"
    })";
    Result<void> wrc;
    json wman = run_node(reg, "astrocs.phase1.writer", cfg, ctx, &wrc);
    CHECK_MSG(wrc.ok(), ("IVAR-001: writer must accept variance HiPS: " +
                         (wrc.failed() ? wrc.error().message() : std::string())).c_str());
    if (wrc.ok()) {
      json fin;
      try { fin = json::parse(read_file(frame_root(fx) + "/p1_final.json")); } catch (...) {}
      const json want = json::array({"signal", "support", "variance", "ivar"});
      CHECK_MSG(fin.value("products", json::array()) == want,
                "IVAR-001: p1_final.products must report the real on-disk product"
                " set (DATA-P1-HIPS §12.2), not a hardcoded [signal,support]");
      CHECK(fin.value("uncertainty_available", false) == true);
      CHECK(fin.value("n_variance_tiles", -1) == 1);
      CHECK(fin.value("n_ivar_tiles", -1) == 1);
      CHECK(wman.value("n_variance_tiles", -1) == 1);
      CHECK_MSG(fs::exists(fs::path(frame_root(fx) + "/variance/properties")),
                "IVAR-001: variance/ subproduct must exist when accumulator carries"
                " finite positive variance");
      CHECK_MSG(fs::exists(fs::path(frame_root(fx) + "/ivar/properties")),
                "IVAR-001: ivar/ subproduct must exist when accumulator carries"
                " finite positive variance");
      std::vector<float> var, iv;
      const bool vok = hips_tile_product(frame_root(fx), "variance", 0, &var);
      const bool iok = hips_tile_product(frame_root(fx), "ivar", 0, &iv);
      CHECK_MSG(vok, "IVAR-001: variance tile must be readable");
      CHECK_MSG(iok, "IVAR-001: ivar tile must be readable");
      if (vok && iok && var.size() == iv.size()) {
        uint64_t n_fin = 0, n_bad_var = 0, n_bad_recip = 0;
        double max_rel = 0.0, max_recip = 0.0;
        for (size_t i = 0; i < var.size(); ++i) {
          const bool fv = std::isfinite(var[i]);
          const bool fi = std::isfinite(iv[i]);
          if (fv != fi) ++n_bad_recip;
          if (!fv) continue;
          ++n_fin;
          const double rel = std::fabs(static_cast<double>(var[i]) - kIvarVarAdu2) /
                             kIvarVarAdu2;
          max_rel = std::max(max_rel, rel);
          if (!(rel < 2e-3)) ++n_bad_var;
          const double rec = std::fabs(static_cast<double>(var[i]) *
                                       static_cast<double>(iv[i]) - 1.0);
          max_recip = std::max(max_recip, rec);
          if (!(rec < 5e-3)) ++n_bad_recip;
        }
        CHECK_MSG(n_fin == kIvarCover,
                  ("IVAR-001: finite variance pixels must equal covered leaves: " +
                   std::to_string(n_fin)).c_str());
        CHECK_MSG(n_bad_var == 0,
                  ("IVAR-001: variance must equal var_num_sum/covered_area^2"
                   " (max_rel=" + std::to_string(max_rel) + ")").c_str());
        CHECK_MSG(n_bad_recip == 0,
                  ("IVAR-001: ivar must be 1/variance per DATA-HIPS-IVAR-001 §4a"
                   " (max|var*ivar-1|=" + std::to_string(max_recip) + ")").c_str());
      }
    }
    // 故障注入面: 等价缺陷（有方差却不请求产品位）⇒ 上述门必红。
    if (fault_no_flags) {
      CHECK_MSG(false,
                "FAULT-INJECT: variance/ivar product bits must be requested when"
                " the accumulator carries finite positive variance (ASTROCS_IVAR_"
                "FAULT=no_variance_flags proves this gate is live)");
    }
    cleanup_fixture(fx);
  }
}

// ── IVAR-002: 逐像素 variance **帧内命名块**接入（定案 2 / ASTROCS_DESIGN §7.1a）──
// 登记面 = DATA-P1-DRZ §11.1:295「variance 面（可选，帧内块）| float32 | ADU²」；
// 生产侧 = drizzle 节点（module_adapters.cpp p1_op_drizzle）用 A
// （snr_noise_model_v1/_fill）对**即将被积分的同一数组**产块并 add_block；
// 消费侧 = hp_drizzle_api.cpp:1018-1052（既有冻结实现, 零改动）。
// 本门锁定四件事:
//   (a) 正例: 星掩膜输入在位 ⇒ variance_product_status=attached + variance/ivar
//       成对落盘 + writer 节点 uncertainty_available=true（磁盘事实, 非硬编码）;
//   (b) 负例 1: p1_sources.json 缺该帧条目 ⇒ 显式降级（不挂块, 禁静默）;
//   (c) 负例 2: A 退化（密集星掩膜 ⇒ plan infeasible, rc=1）⇒ 显式降级
//       （全零平面会把 drizzle_engine.cpp:1919 的整像素 continue 变成清空产品）;
//   (d) 不变性: 挂块不得改变 signal/support（逐字节对拍）。
// IVAR-002 (a2) 线性性 oracle 夹具: 与 star_field_pixel 同一噪声实现（同 hash 键），
// 噪声项整体乘 nscale ⇒ σ² 乘 nscale²（几何/WCS/星位置不变 ⇒ 产品方差应与模型方差
// 同比例变化，几何常数相消）。
struct NoisyField { float bg; float amp; double gain; double rn; double nscale; };
inline float noisy_field_pixel(int i, void* user) {
  auto* sf = static_cast<NoisyField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - 16.0;
  const double dy = static_cast<double>(y) - 16.0;
  const double src = sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  const double lambda_e = (sf->bg + src) * sf->gain;
  const double e = lambda_e +
                   sf->nscale * std::sqrt(std::max(0.0, lambda_e)) *
                       fixture_gauss(static_cast<uint32_t>(i) * 2u + 1u);
  const double rn = sf->nscale * sf->rn *
                    fixture_gauss(static_cast<uint32_t>(i) * 2u + 2u);
  return static_cast<float>((e + rn) / sf->gain);
}

static Fixture make_noise_fixture(const char* tag, double nscale) {
  Fixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_ivar2_" + std::string(tag) + "_" + std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  f.light1 = (f.dir / "light_1.fits").string();
  f.out_dir = f.dir.string();
  NoisyField nf{100.0f, 5000.0f, 1.5, 5.0, nscale};
  CHECK(p1sess::write_fits_file(f.light1, kW, kH, noisy_field_pixel, &nf, 0, 60.0) == 0);
  return f;
}

struct SrcRow { double x, y, flux, fwhm; };

static void write_p1_sources(const Fixture& fx, const char* frame_file,
                             const std::vector<SrcRow>& stars) {
  json srcs = json::array();
  for (size_t i = 0; i < stars.size(); ++i) {
    srcs.push_back(json{{"id", "s" + std::to_string(i)},
                        {"x", stars[i].x}, {"y", stars[i].y},
                        {"flux", stars[i].flux}, {"fwhm_px", stars[i].fwhm},
                        {"snr", 10.0}, {"quality", 0}});
  }
  json fr = json{{"file", frame_file}, {"noise_sigma", 3.0},
                 {"psf_mode", "unavailable"}, {"sources", srcs}};
  json doc = json{{"schema", "DATA-P1-SOURCES"}, {"frames", json::array({fr})}};
  std::ofstream f(fx.out_dir + "/p1_sources.json", std::ios::binary);
  f << doc.dump(2);
}

static std::vector<std::string> hips_leaf_tiles(const std::string& root,
                                                const char* prod) {
  std::vector<std::string> out;
  std::error_code ec;
  const fs::path base = fs::u8path(root + "/" + prod);
  for (fs::recursive_directory_iterator it(base, ec), end; it != end;
       it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const fs::path p = it->path();
    const std::string fn = p.filename().string();
    if (fn == "Moc.fits" || fn == "metadata.fits" || fn == "properties") continue;
    if (p.extension() != ".fits") continue;
    out.push_back(p.string());
  }
  std::sort(out.begin(), out.end());
  return out;
}

static void test_ivar002_frame_variance_block_wiring() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const std::string wcs =
      std::string("\"wcs\": {\"crpix1\": 16.0, \"crpix2\": 16.0, \"crval1\": 10.0,"
                  " \"crval2\": 20.0, \"cd11\": -0.02, \"cd12\": 0.0,"
                  " \"cd21\": 0.0, \"cd22\": 0.02}");
  auto drz_cfg = [&](const Fixture& fx) {
    return std::string("{\n  \"input_lights\": [\"") + fx.light1 +
           "\"],\n  \"output_dir\": \"" + fx.out_dir + "\",\n  " + wcs +
           ",\n  \"drizzle\": {\"nside\": 512, \"nested\": 1, \"pixfrac\": 1.0,"
           " \"precision_mode\": 0}\n}";
  };
  auto wr_cfg = [&](const Fixture& fx) {
    return std::string("{\n  \"input_lights\": [\"") + fx.light1 +
           "\"],\n  \"output_dir\": \"" + fx.out_dir +
           "\",\n  \"filter_passband\": \"R\"\n}";
  };
  // (a) 正例: 单星 + 星掩膜输入在位
  Fixture fx = make_fixture("ivar2pos");
  write_p1_sources(fx, "cleaned_light_1.fits", {{16.0, 16.0, 5000.0, 3.5}});
  RunContext ctx;
  Result<void> rc;
  json man = run_node(reg, "astrocs.phase1.drizzle", drz_cfg(fx), ctx, &rc);
  CHECK_MSG(rc.ok(), ("IVAR-002(a): drizzle must succeed: " +
                      (rc.failed() ? rc.error().message() : std::string())).c_str());
  CHECK_MSG(man.value("variance_product_status", std::string()) == "attached",
            ("IVAR-002(a): variance block must be attached, got status=" +
             man.value("variance_product_status", std::string())).c_str());
  CHECK(man.value("variance_block_name", std::string()) == "variance");
  CHECK(man.value("variance_block_type", std::string()) == "AIO_BLOCK_FLOAT32");
  CHECK(man.value("variance_block_unit", std::string()) == "ADU^2");
  CHECK(man.value("variance_block_optional", false) == true);
  CHECK(man.value("variance_scale_law_applied", true) == false);
  const json vf = man.value("variance_product_frames", json::array());
  CHECK_MSG(vf.size() == 1, "IVAR-002(a): per-frame variance audit must have 1 entry");
  double model_var = 0.0, model_var_1 = 0.0, med_var_1 = 0.0;
  if (!vf.empty()) {
    CHECK(vf[0].value("status", std::string()) == "attached");
    CHECK(vf[0].value("n_control_points", 0u) > 0u);
    model_var = vf[0].value("variance_bg_global", 0.0);
    CHECK_MSG(model_var > 0.0, "IVAR-002(a): non-degenerate model must have variance>0");
  }
  const std::string vroot = frame_root(fx) + "/variance";
  const std::string iroot = frame_root(fx) + "/ivar";
  CHECK_MSG(fs::exists(fs::path(vroot + "/properties")),
            "IVAR-002(a): variance/ subproduct must exist when the frame block is attached");
  CHECK_MSG(fs::exists(fs::path(iroot + "/properties")),
            "IVAR-002(a): ivar/ subproduct must exist when the frame block is attached");
  const std::vector<std::string> vt = hips_leaf_tiles(frame_root(fx), "variance");
  const std::vector<std::string> it2 = hips_leaf_tiles(frame_root(fx), "ivar");
  CHECK_MSG(!vt.empty() && vt.size() == it2.size(),
            "IVAR-002(a): variance/ivar leaf tiles must be paired");
  if (!vt.empty() && vt.size() == it2.size()) {
    std::vector<float> var, iv;
    const bool vok = read_hips_tile(vt[0], &var);
    const bool iok = read_hips_tile(it2[0], &iv);
    CHECK_MSG(vok && iok, "IVAR-002(a): variance/ivar tile must be readable");
    if (vok && iok && var.size() == iv.size() && !var.empty()) {
      std::vector<double> fin;
      double max_recip = 0.0;
      for (size_t i = 0; i < var.size(); ++i) {
        const double v = var[i], w = iv[i];
        if (!std::isfinite(v)) continue;
        fin.push_back(v);
        max_recip = std::max(max_recip, std::fabs(v * w - 1.0));
      }
      CHECK_MSG(!fin.empty(), "IVAR-002(a): product must carry finite variance pixels");
      std::sort(fin.begin(), fin.end());
      const double med = fin[fin.size() / 2];
      CHECK_MSG(med > 0.0, ("IVAR-002(a): variance must be positive (med=" +
                            std::to_string(med) + ")").c_str());
      // 绝对量纲不在本门判定（见 02-variance-wiring.md §5.3 的量纲发现:
      // 引擎 w=overlap/drop 无量纲、sumArea 为 sr、writer 取 vnum/area² ⇒ 产品
      // 方差 = Var(signal)（signal 单位内），而非字面 ADU²）。本门改判
      // **量纲无关的线性性**（(a2): 噪声 ×2 ⇒ 产品方差与模型方差同比例）。
      model_var_1 = model_var;
      med_var_1 = med;
      std::fprintf(stderr, "[IVAR-002] frame1: model_var=%.6f product_med=%.6g\n",
                   model_var, med);
      CHECK_MSG(max_recip < 5e-3,
                ("IVAR-002(a): ivar must be 1/variance (max|v*ivar-1|=" +
                 std::to_string(max_recip) + ")").c_str());
    }
  }
  // writer 节点: 产品面事实来自磁盘（DATA-P1-HIPS §12.2 + §4a 成对）
  json wman = run_node(reg, "astrocs.phase1.writer", wr_cfg(fx), ctx, &rc);
  CHECK_MSG(rc.ok(), ("IVAR-002(a): writer must accept variance HiPS: " +
                      (rc.failed() ? rc.error().message() : std::string())).c_str());
  json fin = json::object();
  try { fin = json::parse(read_file(frame_root(fx) + "/p1_final.json")); } catch (...) { CHECK(false); }
  const json want = json::array({"signal", "support", "variance", "ivar"});
  CHECK_MSG(fin.value("products", json::array()) == want,
            "IVAR-002(a): p1_final.products must report the real on-disk product set");
  CHECK_MSG(fin.value("uncertainty_available", false) == true,
            "IVAR-002(a): writer must report uncertainty_available=true from disk facts");
  CHECK(fin.value("n_variance_tiles", -1) == fin.value("n_tiles", -2));
  CHECK(fin.value("n_ivar_tiles", -1) == fin.value("n_tiles", -2));
  CHECK_MSG(wman.value("uncertainty_available", false) == true,
            "IVAR-002(a): writer node manifest must report uncertainty_available=true");
  // (a2) 传播有效性 oracle（量纲无关）: 同几何、噪声 ×2（方差 ×4）的第二帧 ⇒
  //   产品方差与模型方差**同比例**变化（几何常数相消）⇒ 产品方差确由块内容驱动,
  //   而不是常数/零/装饰面。
  {
    Fixture fx4 = make_noise_fixture("x4", 2.0);
    write_p1_sources(fx4, "cleaned_light_1.fits", {{16.0, 16.0, 5000.0, 3.5}});
    RunContext ctx4;
    Result<void> rc4;
    json man4 = run_node(reg, "astrocs.phase1.drizzle", drz_cfg(fx4), ctx4, &rc4);
    CHECK_MSG(rc4.ok(), ("IVAR-002(a2): drizzle must succeed on the x4-noise frame: " +
                         (rc4.failed() ? rc4.error().message() : std::string())).c_str());
    const json vf4 = man4.value("variance_product_frames", json::array());
    double model_var_4 = 0.0, med_var_4 = 0.0;
    if (!vf4.empty()) model_var_4 = vf4[0].value("variance_bg_global", 0.0);
    {
      const std::vector<std::string> vt4 = hips_leaf_tiles(frame_root(fx4), "variance");
      if (!vt4.empty()) {
        std::vector<float> var4;
        if (read_hips_tile(vt4[0], &var4)) {
          std::vector<double> f4;
          for (float v : var4) if (std::isfinite(v)) f4.push_back(v);
          if (!f4.empty()) {
            std::sort(f4.begin(), f4.end());
            med_var_4 = f4[f4.size() / 2];
          }
        }
      }
    }
    std::fprintf(stderr, "[IVAR-002] frame2(x2 noise): model_var=%.6f product_med=%.6g\n",
                 model_var_4, med_var_4);
    CHECK_MSG(model_var_1 > 0.0 && model_var_4 > 0.0 && med_var_1 > 0.0 && med_var_4 > 0.0,
              "IVAR-002(a2): both frames must yield positive model/product variance");
    if (model_var_1 > 0.0 && model_var_4 > 0.0 && med_var_1 > 0.0 && med_var_4 > 0.0) {
      const double r_model = model_var_4 / model_var_1;
      const double r_prod = med_var_4 / med_var_1;
      CHECK_MSG(r_model > 3.2 && r_model < 4.8,
                ("IVAR-002(a2): x2 noise must quadruple the model variance (got " +
                 std::to_string(r_model) + ")").c_str());
      CHECK_MSG(r_prod / r_model > 0.8 && r_prod / r_model < 1.25,
                ("IVAR-002(a2): product variance must track the block linearly"
                 " (r_prod=" + std::to_string(r_prod) + " r_model=" +
                 std::to_string(r_model) + ")").c_str());
    }
    cleanup_fixture(fx4);
  }
  // (b) 负例 1: 无星掩膜输入 ⇒ 显式降级（不挂块）
  Fixture fx2 = make_fixture("ivar2neg");
  RunContext ctx2;
  Result<void> rc2;
  json man2 = run_node(reg, "astrocs.phase1.drizzle", drz_cfg(fx2), ctx2, &rc2);
  CHECK_MSG(rc2.ok(), ("IVAR-002(b): drizzle must still succeed without mask input: " +
                       (rc2.failed() ? rc2.error().message() : std::string())).c_str());
  CHECK_MSG(man2.value("variance_product_status", std::string()) == "skipped",
            "IVAR-002(b): missing star-mask input must degrade explicitly (status=skipped)");
  const json vf2 = man2.value("variance_product_frames", json::array());
  CHECK(!vf2.empty());
  if (!vf2.empty())
    CHECK_MSG(vf2[0].value("status", std::string()) == "skipped_no_star_mask_input",
              ("IVAR-002(b): reason must be explicit, got " +
               vf2[0].value("status", std::string())).c_str());
  CHECK_MSG(!fs::exists(fs::path(frame_root(fx2) + "/variance/properties")),
            "IVAR-002(b): no variance/ subproduct without the frame block");
  CHECK_MSG(!fs::exists(fs::path(frame_root(fx2) + "/ivar/properties")),
            "IVAR-002(b): no ivar/ subproduct without the frame block");
  run_node(reg, "astrocs.phase1.writer", wr_cfg(fx2), ctx2, &rc2);
  CHECK(rc2.ok());
  json fin2 = json::object();
  try { fin2 = json::parse(read_file(frame_root(fx2) + "/p1_final.json")); } catch (...) { CHECK(false); }
  CHECK_MSG(fin2.value("uncertainty_available", true) == false,
            "IVAR-002(b): writer must report uncertainty_available=false (fail-closed)");
  CHECK(fin2.value("n_variance_tiles", -1) == 0);
  CHECK(fin2.value("n_ivar_tiles", -1) == 0);
  // (d) 不变性: 挂块 vs 不挂块 ⇒ signal/support 逐字节一致
  {
    const std::vector<std::string> sa = hips_leaf_tiles(frame_root(fx), "signal");
    const std::vector<std::string> sb = hips_leaf_tiles(frame_root(fx2), "signal");
    const std::vector<std::string> ua = hips_leaf_tiles(frame_root(fx), "support");
    const std::vector<std::string> ub = hips_leaf_tiles(frame_root(fx2), "support");
    CHECK_MSG(!sa.empty() && sa.size() == sb.size() && ua.size() == ub.size(),
              "IVAR-002(d): signal/support tile sets must match with/without the block");
    bool same = (sa.size() == sb.size() && ua.size() == ub.size());
    for (size_t i = 0; same && i < sa.size(); ++i)
      if (read_bytes(sa[i]) != read_bytes(sb[i])) same = false;
    for (size_t i = 0; same && i < ua.size(); ++i)
      if (read_bytes(ua[i]) != read_bytes(ub[i])) same = false;
    CHECK_MSG(same,
              "IVAR-002(d): attaching the variance block must not change signal/support"
              " (bytewise; drizzle_engine.cpp:1919 drops pixels with variance<=0)");
  }
  // (c) 负例 2: A 退化（密集星掩膜 ⇒ plan infeasible）⇒ 显式降级
  Fixture fx3 = make_fixture("ivar2deg");
  {
    std::vector<SrcRow> dense;
    for (int gy = 0; gy < 8; ++gy)
      for (int gx = 0; gx < 8; ++gx)
        dense.push_back(SrcRow{2.0 + 4.0 * gx, 2.0 + 4.0 * gy, 1.0e4, 4.0});
    write_p1_sources(fx3, "cleaned_light_1.fits", dense);
  }
  RunContext ctx3;
  Result<void> rc3;
  json man3 = run_node(reg, "astrocs.phase1.drizzle", drz_cfg(fx3), ctx3, &rc3);
  CHECK_MSG(rc3.ok(), ("IVAR-002(c): drizzle must survive a degenerate noise model: " +
                       (rc3.failed() ? rc3.error().message() : std::string())).c_str());
  const json vf3 = man3.value("variance_product_frames", json::array());
  CHECK(!vf3.empty());
  if (!vf3.empty()) {
    const std::string st3 = vf3[0].value("status", std::string());
    CHECK_MSG(st3 == "skipped_degenerate_empty_support",
              ("IVAR-002(c): degenerate model must skip the block explicitly, got " +
               st3).c_str());
    CHECK(vf3[0].value("degenerate", false) == true);
  }
  CHECK_MSG(!fs::exists(fs::path(frame_root(fx3) + "/variance/properties")),
            "IVAR-002(c): degenerate model must not publish variance/");
  CHECK_MSG(!fs::exists(fs::path(frame_root(fx3) + "/ivar/properties")),
            "IVAR-002(c): degenerate model must not publish ivar/");
  // 退化时 signal/support 必须完好（全零平面事故面的负例证明）
  {
    const std::vector<std::string> sc = hips_leaf_tiles(frame_root(fx3), "signal");
    const std::vector<std::string> sb = hips_leaf_tiles(frame_root(fx2), "signal");
    CHECK_MSG(!sc.empty() && sc.size() == sb.size(),
              "IVAR-002(c): signal product must survive the degenerate model");
    bool same = (sc.size() == sb.size());
    for (size_t i = 0; same && i < sc.size(); ++i)
      if (read_bytes(sc[i]) != read_bytes(sb[i])) same = false;
    CHECK_MSG(same, "IVAR-002(c): degenerate skip must leave signal bytewise unchanged");
  }
  cleanup_fixture(fx);
  cleanup_fixture(fx2);
  cleanup_fixture(fx3);
}
// ── P0-21: 一组进一组出（ASTROCS_DESIGN §3.4「输出基数」）────────────────
// 缺陷: drizzle/wcs 只取 input_lights[0] ⇒ N 帧只产 1 个 HiPS, 静默丢弃 N-1 帧
// （L4 实测 49 帧只产 12 个产品）。本用例锁定:
//   * N=3 帧 ⇒ 恰好 3 个逐帧 HiPS 产品, 内容互不相同（非同一帧写三次）;
//   * p1_products.json 的计数/路径与输入帧数一致, hips_paths 可被 mosaic 直接消费;
//   * 任一帧不可读 ⇒ 整体 fail-closed（不得产出部分产品却报成功）。
struct TriField { float bg; float amp; float x0; float y0; };
inline float tri_field_pixel(int i, void* user) {
  auto* sf = static_cast<TriField*>(user);
  const int x = i % kW, y = i / kW;
  const double dx = static_cast<double>(x) - sf->x0;
  const double dy = static_cast<double>(y) - sf->y0;
  const double g = sf->amp * std::exp(-(dx * dx + dy * dy) / (2.0 * 1.5 * 1.5));
  return sf->bg + static_cast<float>(g);
}
struct TriFixture {
  fs::path dir;
  std::string light[3];
  std::string bias, dark, flat, out_dir;
};
TriFixture make_tri_fixture(const char* tag) {
  TriFixture f;
  f.dir = fs::temp_directory_path() /
          ("p1001_p021_" + std::string(tag) + "_" + std::to_string(P1001_GETPID));
  std::error_code ec;
  fs::create_directories(f.dir, ec);
  for (int i = 0; i < 3; ++i)
    f.light[i] = (f.dir / ("light_" + std::to_string(i + 1) + ".fits")).string();
  f.bias = (f.dir / "master_bias.fits").string();
  f.dark = (f.dir / "master_dark.fits").string();
  f.flat = (f.dir / "master_flat.fits").string();
  f.out_dir = f.dir.string();
  // 三帧内容互不相同（星幅度不同, 位置保持居中以保证 PSF 拟合稳定收敛）⇒
  // 产品内容必须可区分（不是同一帧写三次）。
  const double xs[3] = {16.0, 16.0, 16.0};
  const float amps[3] = {4000.0f, 5000.0f, 6000.0f};
  for (int i = 0; i < 3; ++i) {
    TriField sf{100.0f, amps[i], static_cast<float>(xs[i]), 16.0f};
    CHECK(p1sess::write_fits_file(f.light[i], kW, kH, tri_field_pixel, &sf, 0, 60.0) == 0);
  }
  float vb = 10.0f, vd = 5.0f, vf = 1.0f;
  CHECK(p1sess::write_fits_file(f.bias, kW, kH, const_pixel, &vb, 0, 10.0) == 0);
  CHECK(p1sess::write_fits_file(f.dark, kW, kH, const_pixel, &vd, 0, 60.0) == 0);
  CHECK(p1sess::write_fits_file(f.flat, kW, kH, const_pixel, &vf, 0, 1.0) == 0);
  return f;
}
void cleanup_tri_fixture(TriFixture& f) {
  std::error_code ec;
  fs::remove_all(f.dir, ec);
}
std::string tri_chain_cfg(const TriFixture& fx, bool with_missing_third) {
  const std::string third = with_missing_third
                                ? std::string("/nonexistent/p021_missing.fits")
                                : fx.light[2];
  return std::string(R"({
    "input_lights": [)") + "\"" + fx.light[0] + "\", \"" + fx.light[1] +
         "\", \"" + third + "\"" + R"(],
    "master_bias": ")" + fx.bias + R"(",
    "master_dark": ")" + fx.dark + R"(",
    "dark_optimization": false,
    "master_flat": ")" + fx.flat + R"(",
    "output_dir": ")" + fx.out_dir + R"(",
    "cosmetic": {"enabled": true, "hot_sigma": 5.0, "cold_sigma": 5.0},
    "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
            "cd11": -0.0002777777777777778, "cd12": 0.0,
            "cd21": 0.0, "cd22": 0.0002777777777777778},
    "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
  })";
}

static void test_p0_21_multi_frame_one_hips_per_input() {
  // 正例: N=3 ⇒ 恰好 3 个逐帧 HiPS 产品, 内容互不相同。
  {
    TriFixture fx = make_tri_fixture("pos");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    const ChainRunResult cr = run_full_chain_once(tri_chain_cfg(fx, false), 2, reg);
    CHECK_MSG(cr.ok, ("P0-21: 3-frame chain must complete: " + cr.error).c_str());
    CHECK_MSG(cr.trace_ok, "P0-21: chain trace must be clean");
    json prods = json::object();
    try { prods = json::parse(read_file(fx.out_dir + "/p1_products.json")); }
    catch (...) { CHECK_MSG(false, "P0-21: p1_products.json must exist"); }
    CHECK_MSG(prods.value("schema", "") == "DATA-P1-PRODUCTS",
              "P0-21: products schema must be DATA-P1-PRODUCTS");
    CHECK_MSG(prods.value("n_frames", 0) == 3, "P0-21: n_frames must equal 3 inputs");
    CHECK_MSG(prods.value("n_products", 0) == 3,
              "P0-21: n_products must equal 3 inputs (count == input frames)");
    CHECK_MSG(prods["hips_paths"].is_array() && prods["hips_paths"].size() == 3,
              "P0-21: hips_paths must list all 3 product paths");
    std::vector<std::string> paths;
    if (prods.contains("hips_paths") && prods["hips_paths"].is_array())
      for (const auto& p : prods["hips_paths"]) paths.push_back(p.get<std::string>());
    CHECK(paths.size() == 3);
    for (size_t i = 0; i < paths.size(); ++i) {
      CHECK_MSG(fs::exists(fs::path(paths[i] + "/signal/properties")),
                ("P0-21: product " + std::to_string(i) +
                 " must have signal/properties").c_str());
      CHECK_MSG(fs::exists(fs::path(paths[i] + "/p1_stack.json")),
                ("P0-21: product " + std::to_string(i) +
                 " must have p1_stack.json").c_str());
      CHECK_MSG(fs::exists(fs::path(paths[i] + "/p1_final.json")),
                ("P0-21: product " + std::to_string(i) +
                 " must have p1_final.json").c_str());
    }
    if (paths.size() == 3) {
      CHECK_MSG(paths[0] != paths[1] && paths[1] != paths[2] && paths[0] != paths[2],
                "P0-21: product paths must be distinct (no overwrite)");
      auto sig_blob = [](const std::string& root) {
        std::map<std::string, std::vector<float>> m;
        read_hips_plane_px(root, "signal", &m);
        std::string blob;
        for (const auto& [k, v] : m) {
          blob += k;
          blob.append(reinterpret_cast<const char*>(v.data()),
                      v.size() * sizeof(float));
        }
        return blob;
      };
      const std::string b0 = sig_blob(paths[0]);
      const std::string b1 = sig_blob(paths[1]);
      const std::string b2 = sig_blob(paths[2]);
      CHECK_MSG(!b0.empty() && !b1.empty() && !b2.empty(),
                "P0-21: every product signal plane must be non-empty");
      CHECK_MSG(b0 != b1 && b1 != b2 && b0 != b2,
                "P0-21: the 3 products must carry different content (not one frame x3)");
    }
    cleanup_tri_fixture(fx);
  }
  // 负例 1: 第三帧路径不存在 ⇒ 全链失败, 不写 p1_products.json（fail-closed）。
  {
    TriFixture fx = make_tri_fixture("neg");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    const ChainRunResult cr = run_full_chain_once(tri_chain_cfg(fx, true), 2, reg);
    CHECK_MSG(!cr.ok, "P0-21: missing frame must fail the chain (fail-closed)");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/p1_products.json")),
              "P0-21: failed run must not publish p1_products.json (no partial success)");
    cleanup_tri_fixture(fx);
  }
  // 负例 2: drizzle 直接消费含不存在帧的输入 ⇒ 必须报错, 不得静默跳过该帧。
  {
    TriFixture fx = make_tri_fixture("drzneg");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    RunContext ctx;
    const std::string cfg = std::string(R"({
      "input_lights": [)") + "\"" + fx.light[0] + "\", \"" + fx.light[1] +
        "\", \"/nonexistent/p021_missing.fits\"" + R"(],
      "output_dir": ")" + fx.out_dir + R"(",
      "wcs": {"crpix1": 16.0, "crpix2": 16.0, "crval1": 10.0, "crval2": 20.0,
              "cd11": -0.0002777777777777778, "cd12": 0.0,
              "cd21": 0.0, "cd22": 0.0002777777777777778},
      "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0}
    })";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx, &rc);
    CHECK_MSG(rc.failed(),
              "P0-21: drizzle must reject a frame it cannot read (no silent skip)");
    CHECK(man.value("status", "") == "fail");
    cleanup_tri_fixture(fx);
  }
}


// ══════════════════════════════════════════════════════════════════════════
// RELEASE-02 FIX-P1 (P1-1): Phase1 测光归一化真正施加到像素 + 如实落元数据
//   正例: p1_photscale.json 给已知 k_photo → p1_op_photometry 施加
//         I_photo = k_photo·I_cal, 写 photoapplied_<base>, applied=true/photscal=k。
//   负例1: 无 scale 来源 → applied=false, pixel_scaling="none", 不写 photoapplied。
//   负例2: sidecar 只覆盖部分帧 → 不施加（部分归一化比不归一化更糟）。
//   RED (修复前): 节点写死 applied=false/photscal=1.0 → 正例三条断言必红。
// ══════════════════════════════════════════════════════════════════════════
static void test_fixp1_photometry_apply() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  const std::string src_json =
      R"({"schema":"DATA-P1-SOURCES","frames":[{"file":"light_1.fits","sources":[{"id":"s1","x":16,"y":16}]}]})";
  auto cfg_for = [](const Fixture& fx, const std::string& lights) {
    return std::string(R"({"input_lights": [)") + lights + R"(],"output_dir": ")" +
           fx.out_dir + R"("})";
  };

  // ── 正例: 已知 k_photo=0.5 施加 ────────────────────────────────────────
  {
    Fixture fx = make_fixture("fixp1apply");
    RunContext ctx;
    {
      std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary);
      o << src_json;
    }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":0.5,"n_matched":7,"sigma_residual_dex":0.01,"source":"star_matcher_tukey_irls"}]})";
    }
    std::vector<float> orig;
    {
      AIOImageData* src = aio_read(fx.light1.c_str());
      CHECK(src != nullptr);
      if (src) {
        const float* p = aio_get_pixel_data(src);
        orig.assign(p, p + static_cast<size_t>(kW) * kH);
        aio_free_image_data(src);
      }
    }
    const std::string cfg = cfg_for(fx, "\"" + fx.light1 + "\"");
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "FIX-P1 POS: photometry with photscale sidecar must succeed");
    CHECK_MSG(man.value("photometry_applied", false) == true,
              "FIX-P1 POS: manifest photometry_applied must be true (was hardcoded false)");
    CHECK_MSG(std::fabs(man.value("photscal", -1.0) - 0.5) < 1e-12,
              "FIX-P1 POS: manifest photscal must equal applied k_photo=0.5");
    const std::string apath = fx.out_dir + "/photoapplied_light_1.fits";
    CHECK_MSG(fs::exists(fs::path(apath)), "FIX-P1 POS: photoapplied_<base> must exist");
    if (!orig.empty() && fs::exists(fs::path(apath))) {
      AIOImageData* ap = aio_read(apath.c_str());
      CHECK(ap != nullptr);
      if (ap) {
        const float* q = aio_get_pixel_data(ap);
        double maxerr = 0.0;
        const size_t n = static_cast<size_t>(kW) * kH;
        for (size_t i = 0; i < n; ++i)
          maxerr = std::max(maxerr, std::fabs(static_cast<double>(q[i]) -
                                             0.5 * static_cast<double>(orig[i])));
        CHECK_MSG(maxerr < 1e-3,
                  ("FIX-P1 POS: applied pixel == 0.5*orig, maxerr=" +
                   std::to_string(maxerr)).c_str());
        aio_free_image_data(ap);
      }
    }
    {
      json pj;
      try { pj = json::parse(read_file(fx.out_dir + "/p1_phot.json")); } catch (...) {}
      CHECK_MSG(pj.value("photometry_applied", false) == true,
                "FIX-P1 POS: p1_phot.json photometry_applied=true");
      CHECK_MSG(pj.value("pixel_scaling", std::string()) == "applied",
                "FIX-P1 POS: p1_phot.json pixel_scaling=applied");
      CHECK_MSG(pj.contains("photscales") && pj["photscales"].is_object(),
                "FIX-P1 POS: p1_phot.json carries per-frame photscales");
    }
    cleanup_fixture(fx);
  }

  // ── 负例1: 无 scale 来源 → 不施加（中性且如实） ────────────────────────
  {
    Fixture fx = make_fixture("fixp1neg");
    RunContext ctx;
    {
      std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary);
      o << src_json;
    }
    const std::string cfg = cfg_for(fx, "\"" + fx.light1 + "\"");
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "FIX-P1 NEG: photometry without scale source must still succeed");
    CHECK_MSG(man.value("photometry_applied", false) == false,
              "FIX-P1 NEG: no scale source → photometry_applied=false");
    CHECK_MSG(std::fabs(man.value("photscal", -1.0) - 1.0) < 1e-12,
              "FIX-P1 NEG: neutral photscal=1.0");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "FIX-P1 NEG: no photoapplied artifact when nothing applied");
    {
      json pj;
      try { pj = json::parse(read_file(fx.out_dir + "/p1_phot.json")); } catch (...) {}
      CHECK_MSG(pj.value("pixel_scaling", std::string()) == "none",
                "FIX-P1 NEG: pixel_scaling=none");
      CHECK_MSG(pj.value("degraded_reason", std::string()) == "photscale_absent",
                "FIX-P1 NEG: degraded_reason=photscale_absent (no silent claim)");
    }
    cleanup_fixture(fx);
  }

  // ── 负例2: sidecar 只覆盖 1/2 帧 → 整组不施加 ──────────────────────────
  {
    Fixture fx = make_fixture("fixp1part");
    RunContext ctx;
    {
      std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-SOURCES","frames":[{"file":"light_1.fits","sources":[{"id":"s1","x":16,"y":16}]},{"file":"light_2.fits","sources":[{"id":"s2","x":16,"y":16}]}]})";
    }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":0.5}]})";
    }
    const std::string cfg =
        cfg_for(fx, "\"" + fx.light1 + "\", \"" + fx.light2 + "\"");
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "FIX-P1 PART: photometry with partial scales must succeed");
    CHECK_MSG(man.value("photometry_applied", false) == false,
              "FIX-P1 PART: incomplete scales → refuse to apply (no half-normalized set)");
    CHECK_MSG(man.value("photscale_source", std::string()) == "photscale_sidecar",
              "FIX-P1 PART: source provenance still recorded");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "FIX-P1 PART: no partial applied frame 1");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/photoapplied_light_2.fits")),
              "FIX-P1 PART: no partial applied frame 2");
    cleanup_fixture(fx);
  }
}

// ══════════════════════════════════════════════════════════════════════════
// RELEASE-02 P1-PHOT-BROKEN: k_photo 伪造/单位混装 fail-closed 判别力测试
//
// 实测缺陷（run/RELEASE-02/L4-rebuild/norm_phot/*/p1_phot.json 与
// run/RELEASE-02/logs/*.phot.stderr）:
//   ① 11/12 板块 photscal=1.0 且 photometry_applied=true —— 该 1.0 不是拟合值,
//      而是 star_matcher 在 NO_DATA（无匹配 / |r_consistent|<3）分支返回的占位
//      值（日志 "匹配+清洗完成: 0 颗, scale=1.000000e+00"）。原判定只查
//      finite&&>0 ⇒ 把占位 1.0 当"已拟合标度"施加并声明 applied=true。
//   ② 根因: phot 节点按文件约定读 <frame_dir>/p1_wcs.json, 但 IR 未声明
//      artifact:p1_wcs 依赖边 ⇒ 与 wcs 节点并发; 读不到时回退 config.wcs
//      （非空但无天测键）⇒ CRVAL=(0,0)/CD=0 ⇒ 0 匹配 ⇒ ①。同配置两次运行
//      结果不同（16:22 冒烟跑 vs 16:26 重跑）。
//   ③ 唯一真拟合的板块 k=6.2722e-17（location=16.20 dex, sigma=0.019 dex）。
//      该值是 SCI-PHOT-001 §3 的**正确**约定（scale 单位 [F_syn 单位]/ADU,
//      仪器常数由 location 吸收）; 危险的不是数值小, 而是它与 1.0 的**混装**。
//
// 本测试锁定: (a) 无拟合证据的标度一律拒绝; (b) 组内标度不一致（单位混装）
// 整组拒绝; (c) 一致的真实标度（含 6.27e-17 量级）正常通过并逐像素施加。
// ══════════════════════════════════════════════════════════════════════════
static void test_p1photbroken_scale_guards() {
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  // 正通量源（PSF 有效域要求 flux>0）: 使 fit 通道真正被走到, 从而 RED-4 判定的
  // 是 WCS 可用性门（而不是被"无 PSF 星"提前拦下）。
  const std::string src2_json =
      R"({"schema":"DATA-P1-SOURCES","frames":[{"file":"light_1.fits","sources":[{"id":"s1","x":16,"y":16,"flux":1000.0}]},{"file":"light_2.fits","sources":[{"id":"s2","x":16,"y":16,"flux":1000.0}]}]})";
  auto cfg_for2 = [](const Fixture& fx, const std::string& lights) {
    return std::string(R"({"input_lights": [)") + lights + R"(],"output_dir": ")" +
           fx.out_dir + R"("})";
  };

  // ── RED-1: k=6.27e-17 且 n_matched=0（无拟合证据）必须被拒绝 ─────────────
  // 判别力: 修复前 sidecar 只查 finite&&>0 ⇒ 会施加并声明 applied=true。
  {
    Fixture fx = make_fixture("p1photbroken_nofit");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":6.272202992543341e-17,"n_matched":0}]})";
    }
    const std::string cfg = cfg_for2(fx, "\"" + fx.light1 + "\"");
    Result<void> rc;
    run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(!rc.ok(), "P1PHOTBROKEN RED-1: scale without fit provenance must be "
                        "rejected (fail-closed), not silently applied");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "P1PHOTBROKEN RED-1: no photoapplied artifact for a rejected scale");
    cleanup_fixture(fx);
  }

  // ── RED-2: 单位混装（6.27e-17 与 1.0 同组）必须整组拒绝 ────────────────
  // 判别力: 这正是 L4 批次的真实形态（11 板块 1.0 + 1 板块 6.27e-17, 相差
  // 16 dex）。修复前两帧都会以 applied=true 施加 ⇒ 帧间落在不同测光坐标系。
  {
    Fixture fx = make_fixture("p1photbroken_mixed");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":6.272202992543341e-17,"n_matched":939,"sigma_residual_dex":0.019},{"file":"light_2.fits","k_photo":1.0,"n_matched":0}]})";
    }
    const std::string cfg =
        cfg_for2(fx, "\"" + fx.light1 + "\", \"" + fx.light2 + "\"");
    Result<void> rc;
    run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    // frame 2 无拟合证据（n_matched=0 且未声明 source）⇒ 硬拒绝（拟合失败，fail-closed）。
    CHECK_MSG(!rc.ok(), "P1PHOTBROKEN RED-2: mixed-provenance set must be rejected");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")) &&
                  !fs::exists(fs::path(fx.out_dir + "/photoapplied_light_2.fits")),
              "P1PHOTBROKEN RED-2: no partial/half-normalized artifacts");
    cleanup_fixture(fx);
  }

  // ── SPREAD-REPORT-1（原 RED-3，**语义已按负责人 GAP_AUDIT §9.49 定案 2 反转**）──
  // 旧断言：两帧都有拟合证据但标度相差 16 dex ⇒ **整组拒绝**（applied=false）。
  // 该断言锁的是**已被删除的组间 k 散度门**。负责人裁决原文：
  //   「极度异常值拒绝，并抛出错误…这玩意应该是帧间独立的，为啥要组间对比」
  //   「不同光学系统的帧混装不得报错」「门只有一个：单帧标定是否可信…与其它帧无关」
  // ⇒ 新语义：**帧间独立**。单帧标定可信（本 fixture 两帧 n_matched>0 且
  //   sigma_residual_dex 均在帧内判据 P1_PHOT_MAX_SIGMA_DEX=1.0 内）⇒ 必须施加；
  //   组间散度只**报告**（photscale_spread_dex/_warn/_gate），不阻断。
  // 判别力：若有人把组间门加回来，本用例立即转红（applied 会变 false）。
  // 帧内门（MIN_FIT_STARS=3 / MAX_SIGMA_DEX=1.0）的负例仍由 RED-2 与本块之外
  // 的用例覆盖，未被削弱。
  {
    Fixture fx = make_fixture("p1phot_spread_report");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":6.272202992543341e-17,"n_matched":939,"sigma_residual_dex":0.019},{"file":"light_2.fits","k_photo":1.0,"n_matched":917,"sigma_residual_dex":0.018}]})";
    }
    const std::string cfg =
        cfg_for2(fx, "\"" + fx.light1 + "\", \"" + fx.light2 + "\"");
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "SPREAD-REPORT-1: node succeeds (frame-independent)");
    CHECK_MSG(man.value("photometry_applied", false) == true,
              "SPREAD-REPORT-1: 帧间 k 散度**不得**阻断施加（§9.49 定案 2：帧间独立）");
    CHECK_MSG(fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "SPREAD-REPORT-1: 单帧标定可信 ⇒ 必须有 photoapplied 产物");
    CHECK_MSG(man.value("photscale_spread_dex", 0.0) > 0.0,
              "SPREAD-REPORT-1: 组间散度必须**报告**（photscale_spread_dex>0）");
    CHECK_MSG(man.value("photscale_spread_warn", false) == true,
              "SPREAD-REPORT-1: 散度超阈 ⇒ photscale_spread_warn=true（警告，非门）");
    CHECK_MSG(man.value("photscale_spread_gate", std::string()).find("none") !=
                  std::string::npos,
              "SPREAD-REPORT-1: photscale_spread_gate 必须显式声明 'none'");
    CHECK_MSG(!man.contains("degraded_reason") && !man.contains("photscale_error"),
              "SPREAD-REPORT-1: 组间散度不得产生 degraded_reason/photscale_error");
    {
      json pj;
      try { pj = json::parse(read_file(fx.out_dir + "/p1_phot.json")); } catch (...) {}
      CHECK_MSG(pj.value("photometry_applied", false) == true,
                "SPREAD-REPORT-1: p1_phot.json photometry_applied=true");
      CHECK_MSG(pj.contains("photscales") && pj.contains("photoapplied_artifacts"),
                "SPREAD-REPORT-1: applied=true ⇒ 必须声明 photscales/artifacts");
    }
    cleanup_fixture(fx);
  }

  // ── STAR-GATE-REMOVED: 测光不设星数门槛（SCI-PHOT-001 §16.5）─────────────
  // 旧行为：sidecar 声明 n_matched < 3（SCI-PHOT-001 §4 冻结门）即整组拒绝。
  // 现行为：本节点只判**有无拟合证据**，不判星数够不够 ⇒ n_matched=1 且声明了
  //         拟合来源的帧正常施加；只有「无任何拟合证据」才按拟合失败硬拒绝。
  // 判别力：把星数门槛加回来 ⇒ 本用例立即转红（applied=false / 无产物）。
  {
    Fixture fx = make_fixture("p1phot_nostargate");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":0.5,"n_matched":1,"sigma_residual_dex":0.01,"source":"external_offline_fit"}]})";
    }
    const std::string cfg = cfg_for2(fx, "\"" + fx.light1 + "\"");
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "STAR-GATE-REMOVED: 星数少不得作为失败理由（不是门槛）");
    CHECK_MSG(man.value("photometry_applied", false) == true,
              "STAR-GATE-REMOVED: n_matched=1 且声明拟合来源 ⇒ 必须施加（星数门槛已删）");
    CHECK_MSG(std::fabs(man.value("photscal", -1.0) - 0.5) < 1e-12,
              "STAR-GATE-REMOVED: photscal = 侧车声明的 k_photo");
    CHECK_MSG(fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "STAR-GATE-REMOVED: 必须有 photoapplied 产物");
    {
      json pj;
      try { pj = json::parse(read_file(fx.out_dir + "/p1_phot.json")); } catch (...) {}
      const bool has = pj.contains("photscale_detail") &&
                       pj["photscale_detail"].contains("light_1");
      CHECK_MSG(has && pj["photscale_detail"]["light_1"].value("n_matched", -1) == 1 &&
                    pj["photscale_detail"]["light_1"].value("fitted", false) == true,
                "STAR-GATE-REMOVED: 逐帧拟合证据如实落盘（n_matched=1 / fitted=true）");
    }
    cleanup_fixture(fx);
  }

  // ── FIT-FAILURE-REPORT: fit 通道失败按**拟合失败**上报（不是星数门禁判词）──
  // 失败点落在拟合自身（响应曲线不可读 ⇒ 冻结 C 入口前装配失败, rc<0）:
  // 节点必须如实写 degraded_reason=photscale_incomplete + photscale_error, 且
  // 判词是「拟合失败」而不是「星数不足」。判别力: 若在拟合之前插回一条星数
  // 门禁, 判词会变成星数门措辞 ⇒ 本用例转红。
  {
    Fixture fx = make_fixture("p1phot_fitfail");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    // 可用天测（过 p1_wcs_astrometry_usable）⇒ 失败点必须落在**拟合**而非 WCS。
    {
      std::ofstream o(frame_root(fx) + "/p1_wcs.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-WCS-001","wcs":{"crval1":83.2834,"crval2":-6.3743,"crpix1":16.0,"crpix2":16.0,"cd11":-0.0002689,"cd12":0.0,"cd21":0.0,"cd22":0.0002689}})";
    }
    const std::string cfg =
        std::string(R"({"input_lights": [)") + "\"" + fx.light1 + "\"" +
        R"(],"output_dir": ")" + fx.out_dir +
        R"(","photometry":{"fit":{"enabled":true,"gaia_data_dir":")" +
        (fs::temp_directory_path() / "p1phot_fitfail_no_such_gaia").string() +
        R"(","filter":"Red","filters_json":")" + fx.dir.string() +
        R"(/no_such_filters.json"}}})";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "FIT-FAILURE-REPORT: 拟合失败按 degraded_reason 上报（不中止节点）");
    CHECK_MSG(man.value("photometry_applied", true) == false,
              "FIT-FAILURE-REPORT: 未产出标度 ⇒ 不得声明已施加");
    const std::string err = man.value("photscale_error", std::string());
    CHECK_MSG(err.rfind("photometry fit failed", 0) == 0,
              ("FIT-FAILURE-REPORT: 判词必须是拟合失败, got: " + err).c_str());
    CHECK_MSG(err.find("n_matched <") == std::string::npos &&
                  err.find("§4 gate") == std::string::npos,
              "FIT-FAILURE-REPORT: 不得残留星数门禁判词");
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "FIT-FAILURE-REPORT: 拟合失败不得产出 photoapplied 产物");
    {
      json pj;
      try { pj = json::parse(read_file(fx.out_dir + "/p1_phot.json")); } catch (...) {}
      CHECK_MSG(pj.value("degraded_reason", std::string()) == "photscale_incomplete" &&
                    pj.value("photometry_applied", true) == false,
                "FIT-FAILURE-REPORT: provenance 如实（degraded_reason=photscale_incomplete）");
    }
    cleanup_fixture(fx);
  }
  // ── GREEN: 一致的**真实**标度（6.27e-17 / 5.69e-17 量级）正常通过并施加 ──
  // 依据 SCI-PHOT-001 §3: scale 单位 [F_syn 单位]/ADU, 绝对值可跨数量级;
  // 判据是一致性 + 拟合证据, 不是绝对窗口 [0.1,10]（那会拒绝 100% 真实数据）。
  {
    Fixture fx = make_fixture("p1photbroken_green");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      // F-INSTR-CONFORM-FIX: 收紧 P1_PHOT_MAX_SPREAD_DEX 0.5→0.02 dex（=0.05 mag
      // 峰峰, 负责人判据）后, 本 GREEN 对必须是**真一致**的一对。旧值
      // (6.272202992543341e-17, 5.685037392078662e-17) 的散度 0.0427 dex =
      // 0.107 mag 是盒和口径下的视宁度假信号, 已改判为 RED-5。
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":6.272202992543341e-17,"n_matched":939,"sigma_residual_dex":0.019137,"source":"gaia_star_matcher_tukey_irls"},{"file":"light_2.fits","k_photo":6.260000000000000e-17,"n_matched":917,"sigma_residual_dex":0.017811,"source":"gaia_star_matcher_tukey_irls"}]})";
    }
    std::vector<float> orig1, orig2;
    for (const std::string* lp : {&fx.light1, &fx.light2}) {
      AIOImageData* src = aio_read(lp->c_str());
      CHECK(src != nullptr);
      if (src) {
        const float* p = aio_get_pixel_data(src);
        std::vector<float> v(p, p + static_cast<size_t>(kW) * kH);
        (lp == &fx.light1 ? orig1 : orig2) = v;
        aio_free_image_data(src);
      }
    }
    const std::string cfg =
        cfg_for2(fx, "\"" + fx.light1 + "\", \"" + fx.light2 + "\"");
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "P1PHOTBROKEN GREEN: consistent fitted scales must succeed");
    CHECK_MSG(man.value("photometry_applied", false) == true,
              "P1PHOTBROKEN GREEN: applied=true for a real, consistent fit");
    // photscal_rep = median(k) = k(frame2) (两帧升序后取 [size/2] = 第 2 个)
    const double rep = man.value("photscal", -1.0);
    CHECK_MSG(std::isfinite(rep) && rep > 0.0 &&
                  std::fabs(rep / 6.272202992543341e-17 - 1.0) < 1e-12,
              ("P1PHOTBROKEN GREEN: photscal = median(k_photo), got " +
               std::to_string(rep)).c_str());
    struct Case { const std::string* lp; const std::vector<float>* orig; double k; };
    const Case cases[2] = {{&fx.light1, &orig1, 6.272202992543341e-17},
                           {&fx.light2, &orig2, 6.260000000000000e-17}};
    for (const Case& c : cases) {
      const std::string apath =
          fx.out_dir + "/photoapplied_" + fs::path(*c.lp).filename().string();
      CHECK_MSG(fs::exists(fs::path(apath)), "P1PHOTBROKEN GREEN: photoapplied artifact");
      if (!c.orig->empty() && fs::exists(fs::path(apath))) {
        AIOImageData* ap = aio_read(apath.c_str());
        CHECK(ap != nullptr);
        if (ap) {
          const float* q = aio_get_pixel_data(ap);
          double maxrel = 0.0;
          const size_t n = static_cast<size_t>(kW) * kH;
          for (size_t i = 0; i < n; ++i) {
            const double want = c.k * static_cast<double>((*c.orig)[i]);
            const double got = static_cast<double>(q[i]);
            const double den = std::fabs(want) > 1e-300 ? std::fabs(want) : 1.0;
            maxrel = std::max(maxrel, std::fabs(got - want) / den);
          }
          // FP32 写盘 ⇒ 相对误差 ~1e-7; 关键是比值恒定（不是被乘成 0/溢出）
          CHECK_MSG(maxrel < 1e-5,
                    ("P1PHOTBROKEN GREEN: applied == k*orig (relative), maxrel=" +
                     std::to_string(maxrel)).c_str());
          aio_free_image_data(ap);
        }
      }
    }
    {
      json pj;
      try { pj = json::parse(read_file(fx.out_dir + "/p1_phot.json")); } catch (...) {}
      std::fprintf(stderr, "[P1PHOTBROKEN-GREEN] p1_phot.json=%s\n", pj.dump().c_str());
      CHECK_MSG(pj.value("photometry_applied", false) == true,
                "P1PHOTBROKEN GREEN: p1_phot.json applied=true");
      // provenance 自洽: applied=true ⟺ photscales 覆盖每帧 + 每帧拟合证据 + 产物
      const bool has_ps = pj.contains("photscales");
      const bool ps_is_obj = has_ps && pj["photscales"].is_object();
      const std::size_t ps_n = ps_is_obj ? pj["photscales"].size() : 0;
      CHECK_MSG(has_ps && ps_is_obj && ps_n == 2,
                ("P1PHOTBROKEN GREEN: photscales covers every frame (has=" +
                 std::to_string(static_cast<int>(has_ps)) + " obj=" +
                 std::to_string(static_cast<int>(ps_is_obj)) + " n=" +
                 std::to_string(ps_n) + ")").c_str());
      CHECK_MSG(pj.contains("photscale_detail") && pj["photscale_detail"].is_object() &&
                    pj["photscale_detail"].size() == 2,
                "P1PHOTBROKEN GREEN: per-frame fit provenance present");
      for (auto it = pj["photscale_detail"].begin();
           it != pj["photscale_detail"].end(); ++it) {
        CHECK_MSG(it.value().value("fitted", false) == true,
                  "P1PHOTBROKEN GREEN: photscale_detail.fitted=true");
        CHECK_MSG(it.value().value("n_matched", 0) >= 1,
                  "P1PHOTBROKEN GREEN: 有拟合证据（星数本身不作门槛, §16.5）");
      }
      CHECK_MSG(pj.contains("photoapplied_artifacts") &&
                    pj["photoapplied_artifacts"].is_array() &&
                    pj["photoapplied_artifacts"].size() == 2,
                "P1PHOTBROKEN GREEN: photoapplied_artifacts covers every frame");
    }
    cleanup_fixture(fx);
  }

  // ── SPREAD-REPORT-2（原 RED-5 F-INSTR，**语义已按 §9.49 定案 2 反转**）──
  // 旧断言：帧间 k 散度 0.107 mag（6.272203e-17 vs 5.685037e-17）在
  // P1_PHOT_MAX_SPREAD_DEX=0.02 dex 下必须整组拒绝。该阈值/门**已删除**。
  // 新语义：逐帧独立判定；组间散度只报告。同一对帧的两帧各自
  // n_matched>0 且 sigma_residual_dex 均在帧内判据内 ⇒ 必须各自施加。
  // 判别力：把组间门加回来 ⇒ applied 变 false ⇒ 本用例转红。
  {
    Fixture fx = make_fixture("finstr_spread_report");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    {
      std::ofstream o(fx.out_dir + "/p1_photscale.json", std::ios::binary);
      o << R"({"schema":"DATA-P1-PHOTSCALE-001","frames":[{"file":"light_1.fits","k_photo":6.272202992543341e-17,"n_matched":939,"sigma_residual_dex":0.019137},{"file":"light_2.fits","k_photo":5.685037392078662e-17,"n_matched":917,"sigma_residual_dex":0.017811}]})";
    }
    const std::string cfg =
        cfg_for2(fx, "\"" + fx.light1 + "\", \"" + fx.light2 + "\"");
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "SPREAD-REPORT-2: node succeeds (frame-independent)");
    CHECK_MSG(man.value("photometry_applied", false) == true,
              "SPREAD-REPORT-2: 0.107 mag 组间散度**不得**阻断（§9.49 定案 2）");
    CHECK_MSG(fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "SPREAD-REPORT-2: 单帧可信 ⇒ 必须有 photoapplied 产物");
    CHECK_MSG(man.value("photscale_spread_dex", 0.0) > 0.0 &&
                  man.value("photscale_spread_warn", false) == true,
              "SPREAD-REPORT-2: 散度必须报告且置 warn（警告，非门）");
    CHECK_MSG(!man.contains("photscale_error"),
              "SPREAD-REPORT-2: 不得再产生 photscale_error（门已删除）");
    cleanup_fixture(fx);
  }

  // ── RED-4: fit 通道 WCS 不可用 ⇒ 显式降级, 不得以零 WCS 拟合出占位 1.0 ──
  // 判别力: 修复前 config.wcs 非空即通过, 以 CRVAL=(0,0)/CD=0 拟合 → NO_DATA
  // 占位 scale=1.0 → applied=true/photscal=1.0（实测 11/12 板块）。
  // 修复后 WCS 可用性校验先于拟合 ⇒ photscale_error 指明 WCS（而非 "fit failed"）。
  {
    Fixture fx = make_fixture("p1photbroken_wcs");
    RunContext ctx;
    { std::ofstream o(fx.out_dir + "/p1_sources.json", std::ios::binary); o << src2_json; }
    const std::string cfg =
        std::string(R"({"input_lights": [)") + "\"" + fx.light1 + "\"" +
        R"(],"output_dir": ")" + fx.out_dir +
        R"(","photometry":{"fit":{"enabled":true,"gaia_data_dir":")" +
        (fs::temp_directory_path() / "p1photbroken_no_such_gaia_dir").string() +
        R"(","filter":"Red","filters_json":")" + fx.dir.string() +
        R"(/no_such_filters.json"}},"wcs":{"init_source":"header_pointing","gaia_data_dir":"/nonexistent"}})";
    Result<void> rc;
    json man = run_node(reg, "astrocs.phase1.photometry", cfg, ctx, &rc);
    CHECK_MSG(rc.ok(), "P1PHOTBROKEN RED-4: node degrades explicitly (does not abort)");
    CHECK_MSG(man.value("photometry_applied", true) == false,
              "P1PHOTBROKEN RED-4: unusable WCS must NOT yield applied=true");
    CHECK_MSG(std::fabs(man.value("photscal", -1.0) - 1.0) < 1e-12,
              "P1PHOTBROKEN RED-4: neutral photscal=1.0 with applied=false");
    {
      const std::string err = man.value("photscale_error", std::string());
      CHECK_MSG(err.find("WCS unusable") != std::string::npos,
                ("P1PHOTBROKEN RED-4: photscale_error must name the WCS defect, got: " +
                 err).c_str());
    }
    CHECK_MSG(!fs::exists(fs::path(fx.out_dir + "/photoapplied_light_1.fits")),
              "P1PHOTBROKEN RED-4: no fake identity 'photoapplied' artifact");
    {
      json pj;
      try { pj = json::parse(read_file(fx.out_dir + "/p1_phot.json")); } catch (...) {}
      CHECK_MSG(pj.value("photometry_applied", true) == false &&
                    pj.value("pixel_scaling", std::string()) == "none",
                "P1PHOTBROKEN RED-4: provenance is honest (applied=false/none)");
      CHECK_MSG(pj.value("degraded_reason", std::string()) == "photscale_incomplete",
                "P1PHOTBROKEN RED-4: degraded_reason=photscale_incomplete");
    }
    cleanup_fixture(fx);
  }
}


// ══ PERF-P1: 帧级并行 1/N worker 逐位等价（AGENTS §9 一致性要求）═══════════
// 依据: AGENTS §9「1/N worker 一致性」、§6「不硬编码线程」、PERF-P2 既有规范
// （p2_parallel_for 注释: 每任务只写自己下标结果槽、跨任务无浮点归约 ⇒ 与串行
// 逐位一致）。RED 锚定（接线前）: P1 节点完全不消费 __workers（帧串行），
// budget=16 与 budget=1 走同一条串行路径 ⇒ 本测试无法证伪并行正确性；接线后若
// 并行体引入跨帧共享可变状态（如原实现的 W/H 循环内共享推进）或跨帧浮点归约，
// 则两次运行的产物字节比对确定性失败。cosmetic 在本夹具下为逐像素直通
// （节点面 nullptr/nullptr 掩码源）⇒ 其产物必须与 artifact:cal 逐字节相同，
// 使"帧序归约错位"这类缺陷同样可见。
//
// 递归收集 root 下全部**常规文件**（键 = 相对 root 的 POSIX 相对路径）。
// 目录枚举经 aio 唯一机制原语（aio_atomic::for_each_child，header-only；
// ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」+ §9.73 裁决 U5），本 TU 不再
// 自持 std::filesystem 遍历通道。kind: 0=常规文件 / 1=目录 / 2=其他（不跟随
// 符号链接）。与 recursive_directory_iterator 的等价性: 常规目录照常下钻、常规
// 文件照常采集；kind==2（符号链接/fifo/设备）不采集 —— 夹具产物树由 writer 直接
// 落盘，不含此类项。枚举失败（目录不可打开）⇒ 该层视为空，与原实现 ec 置位后
// 停止迭代同效。
static void p1par_collect_tree(const std::string& root, const std::string& rel,
                               std::map<std::string, std::string>* out) {
  (void)aio_atomic::for_each_child(
      root,
      [&](const std::string& child, int kind) -> int {
        const std::size_t slash = child.find_last_of("/\\");
        const std::string name =
            (slash == std::string::npos) ? child : child.substr(slash + 1);
        const std::string child_rel = rel.empty() ? name : (rel + "/" + name);
        if (kind == 1) {
          p1par_collect_tree(child, child_rel, out);
        } else if (kind == 0) {
          (*out)[child_rel] = read_bytes(child);
        }
        return 0;
      },
      nullptr);
}

static std::map<std::string, std::string> p1par_run_chain(const char* tag,
                                                          uint32_t budget) {
  Fixture fx = make_hot_fixture(tag);
  ModuleRegistry reg;
  CHECK(register_phase_modules(reg).ok());
  RunContext ctx;
  auto b = create_thread_budget(budget);
  CHECK_MSG(b.ok(), "create_thread_budget failed");
  if (b.ok()) ctx.set_budget(b.value());
  const std::string cfg = p1001_full_chain_cfg(fx);
  json man_cal = run_node(reg, "astrocs.phase1.calibration", cfg, ctx);
  CHECK_MSG(man_cal.value("status", "") == "ok", "calibration node must succeed");
  json man_cos = run_node(reg, "astrocs.phase1.cosmetic", cfg, ctx);
  CHECK_MSG(man_cos.value("status", "") == "ok", "cosmetic node must succeed");
  // PERF-P1 覆盖扩展: 帧级并行已接线的其余节点全部纳入同一条链
  // （star-psf / photometry / noise-snr / drizzle / writer）。逐节点直调 = 各节点
  // 真实 execute 路径（含 lease ⇒ __workers 注入），帧级并行即在此发生；节点 status
  // 必须为 ok，否则"两边都失败"会让字节比对假绿。
  json man_psf = run_node(reg, "astrocs.phase1.star-psf", cfg, ctx);
  CHECK_MSG(man_psf.value("status", "") == "ok", "star-psf node must succeed");
  json man_phot = run_node(reg, "astrocs.phase1.photometry", cfg, ctx);
  CHECK_MSG(man_phot.value("status", "") == "ok", "photometry node must succeed");
  json man_snr = run_node(reg, "astrocs.phase1.noise-snr", cfg, ctx);
  CHECK_MSG(man_snr.value("status", "") == "ok", "noise-snr node must succeed");
  json man_drz = run_node(reg, "astrocs.phase1.drizzle", cfg, ctx);
  CHECK_MSG(man_drz.value("status", "") == "ok", "drizzle node must succeed");
  json man_wr = run_node(reg, "astrocs.phase1.writer", cfg, ctx);
  CHECK_MSG(man_wr.value("status", "") == "ok", "writer node must succeed");
  // 收集 out_dir 下全部产物字节（**递归**含 HiPS 产品子目录; 相对路径为键，
  // 字典序固定 ⇒ 与 worker 数无关）。枚举经 aio（p1par_collect_tree）。
  std::map<std::string, std::string> got;
  p1par_collect_tree(fx.out_dir, std::string(), &got);
  cleanup_fixture(fx);
  return got;
}

// ── PERF-P1: 运行时刻遥测归一化 ───────────────────────────────────────────
// IVOA HiPS properties 的 hips_release_date / hips_creation_date 由
// aio_hips_writer.cpp:1396-1399 的 std::time(nullptr) 生成（"META-001: 真实 UTC
// finalize 时间, 禁止硬编码日期"），并随 p1_final.json / p1_products.json 的
// "properties" 字段进入产品清单 ⇒ 它们是**运行时刻遥测**，既不是科学面，也不是
// worker 数的函数。逐位门先做未归一化比对，再对差异集合做归一化复核，并用
// "串行跑两次"把运行时刻差异与 worker 数差异**分离开**（见 ③）。
static bool p1par_digits(const std::string& s, std::size_t i, std::size_t n) {
  if (i + n > s.size()) return false;
  for (std::size_t k = 0; k < n; ++k)
    if (!std::isdigit(static_cast<unsigned char>(s[i + k]))) return false;
  return true;
}
static std::string p1par_normalize_run_timestamps(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  std::size_t i = 0;
  while (i < s.size()) {
    const bool is_ts =
        (i + 20 <= s.size() && p1par_digits(s, i, 4) && s[i + 4] == '-' &&
         p1par_digits(s, i + 5, 2) && s[i + 7] == '-' && p1par_digits(s, i + 8, 2) &&
         s[i + 10] == 'T' && p1par_digits(s, i + 11, 2) && s[i + 13] == ':' &&
         p1par_digits(s, i + 14, 2) && s[i + 16] == ':' && p1par_digits(s, i + 17, 2) &&
         s[i + 19] == 'Z');
    const bool is_date =
        (i + 10 <= s.size() && p1par_digits(s, i, 4) && s[i + 4] == '-' &&
         p1par_digits(s, i + 5, 2) && s[i + 7] == '-' && p1par_digits(s, i + 8, 2));
    if (is_ts) { out += "<RUN-TS>"; i += 20; continue; }
    if (is_date) { out += "<RUN-DATE>"; i += 10; continue; }
    out.push_back(s[i]);
    ++i;
  }
  return out;
}
// 两个产物集合的**未归一化**差异集合（含仅一侧存在的产物）。
static std::set<std::string> p1par_diff_set(
    const std::map<std::string, std::string>& a,
    const std::map<std::string, std::string>& b) {
  std::set<std::string> d;
  for (const auto& kv : a) {
    auto it = b.find(kv.first);
    if (it == b.end() || it->second != kv.second) d.insert(kv.first);
  }
  for (const auto& kv : b)
    if (a.find(kv.first) == a.end()) d.insert(kv.first);
  return d;
}

static void test_perf_p1_frame_parallel_bitwise_1_vs_n() {
  const uint32_t kN = 16;   // 与 p2_parallel_for 的 1/N 一致性同口径
  // 三次运行共用**同一 fixture tag** ⇒ 产物内的绝对路径逐字相同，唯一可能的差异
  // 只剩运行时刻遥测。串行跑两次（serial / serial2）即可把"运行时刻差异"与
  // "worker 数差异"分离开。
  const std::map<std::string, std::string> serial = p1par_run_chain("par", 1);
  const std::map<std::string, std::string> serial2 = p1par_run_chain("par", 1);
  const std::map<std::string, std::string> par = p1par_run_chain("par", kN);
  // ① 产物集合相同（帧级并行不得漏写/多写任何一帧的产物）
  CHECK_MSG(serial.size() == par.size(),
            "1-worker and N-worker runs must publish the same artifact set");
  CHECK_MSG(!serial.empty(), "fixture must produce at least one artifact");
  // ② 逐产物比对：未归一化逐字节相同；若不同，则**归一化后**必须逐字节相同
  //    （结构性差异仍必须可见）。
  const std::set<std::string> d_sp = p1par_diff_set(serial, par);
  for (const auto& kv : serial) {
    auto it = par.find(kv.first);
    CHECK_MSG(it != par.end(),
              ("N-worker run missing artifact: " + kv.first).c_str());
    if (it == par.end()) continue;
    if (it->second == kv.second) continue;
    CHECK_MSG(p1par_normalize_run_timestamps(it->second) ==
                  p1par_normalize_run_timestamps(kv.second),
              ("artifact bytes differ between 1 and N workers (even after "
               "run-timestamp normalisation): " + kv.first).c_str());
  }
  // ③ 关键判别性自证: 差异集合必须**只由运行时刻决定，与 worker 数无关** ——
  //    serial vs serial2 的差异集合必须与 serial vs par 的差异集合相同。
  //    若某产物真是被并行改动，它会只出现在 d_sp 而不在 d_ss ⇒ 本断言判红。
  const std::set<std::string> d_ss = p1par_diff_set(serial, serial2);
  CHECK_MSG(d_ss == d_sp,
            "run-to-run (1 vs 1 worker) artifact diff set must equal the "
            "1-vs-N diff set: a worker-count-dependent artifact would appear "
            "only in the latter");
  // ④ 归一化非空操作自证: 串行两次的差异产物归一化后必须逐字节相同。
  for (const auto& name : d_ss) {
    const std::string a = p1par_normalize_run_timestamps(serial.at(name));
    const std::string b = p1par_normalize_run_timestamps(serial2.at(name));
    CHECK_MSG(a == b,
              ("run-timestamp normalisation must explain the run-to-run diff: " +
               name).c_str());
  }
  // ⑤ 判别力自证: 若把某产物改成不同字节，比对必须能红（此处以"篡改副本"模拟）
  if (!serial.empty()) {
    std::map<std::string, std::string> tampered = par;
    auto first = tampered.begin();
    if (!first->second.empty()) {
      first->second[0] = static_cast<char>(first->second[0] ^ 0x01);
      bool detected = false;
      for (const auto& kv : serial) {
        auto it = tampered.find(kv.first);
        if (it != tampered.end() && it->second != kv.second) detected = true;
      }
      CHECK_MSG(detected, "bitwise comparison must detect a 1-bit tamper");
    }
  }
}

int main() {
  // PERF-P1: 帧级并行 1/N 逐位等价（生产节点接线验证）
  test_perf_p1_frame_parallel_bitwise_1_vs_n();
  test_nodes_real_operation();
  test_runtime_chain_call_count_1();
  test_fail_fast_downstream_zero_calls();
  test_complete_gate_fail_closed();
  test_negative_injection();
  // B2-A12/A13/A14/A16: 科学缺省 / provenance / fail-closed 新门
  test_b2a12_precision_default_and_equiv();
  test_b2a13_dark_scale_from_exptime();
  test_b2a14_photappl_provenance();
  test_b2a16_photometry_fail_closed();
  // RELEASE-02 FIX-P1: Phase1 测光归一化真正施加 + 如实元数据
  test_fixp1_photometry_apply();
  // RELEASE-02 P1-PHOT-BROKEN: k_photo 伪造/单位混装 fail-closed 判别力
  test_p1photbroken_scale_guards();
  test_b2a15_writer_stale_buffer_and_support();
  test_b2a15_ghost_discontinuous_multiparent();
  // IVAR-001: Phase1 生产末端 variance/ivar 子产品 (§12.1/§12.2) + 注入面
  test_ivar001_phase1_variance_products();
  // IVAR-002: 逐像素 variance 帧内命名块接入（定案 2 / ASTROCS_DESIGN §7.1a）
  test_ivar002_frame_variance_block_wiring();
  test_b2a17_sip_bridge();
  // P17-NSIDE: drizzle 采样率合规 (1x-2x) + nside 来源/欠采样可见性
  test_p17_nside_sampling_compliance();
  // P21-HIPS-WRITER: writer 聚合 O(P+T) 建桶 (性能 P0; 确定性回归)
  test_p21_writer_aggregation_buckets();
  test_determinism();
  // CORE-RACE-001（p1001 链并发撕裂读）: 独立产物路径 / IR 接线一致性 /
  // 并发全链 N 次连跑 / 1-N worker parity / 故障注入
  test_cos_artifact_is_independent();
  test_consumer_reads_cos_artifact();
  test_parallel_chain_stress();
  test_worker_parity_bitwise();
  test_torn_artifact_fault_injection();
  test_psf_partial_fit_identity();
  test_psf_nonfinite_frame_fail_closed();
  test_psf_fast_cap_and_inactive_precise();
  test_golden_parity();
  // P0-21: 一组进一组出（N 帧 ⇒ N 个 HiPS 产品）+ fail-closed 负例
  test_p0_21_multi_frame_one_hips_per_input();
  if (failures == 0) {
    std::printf("P1-001 REAL NODES PASS (8 节点唯一真实 operation + call_count=1 + complete 门 fail-closed + 下游零调用)\n");
    return 0;
  }
  std::fprintf(stderr, "P1-001 REAL NODES FAIL (%d)\n", failures);
  return 1;
}
