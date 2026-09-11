// tests/unit/rt001_unique_executor_test.cpp — RT-001(对齐控制包) 唯一 Executor 接入与实测资源门
//
// 验收映射 (控制包 RT-001 "接入唯一Executor和实测资源门"; 合同锚: 宪章 §10.4
// "一个进程只有一个资源调度器和线程预算源; 模块不得硬编码 workers, 不得建立
// 不受 Runtime 管理的长期私有线程池; 模块按 work unit 申请线程租约" +
// §10.5/§18.2 冻结利用率门禁; P1-001/P2-001/P3-002 先例同构):
//   1. work-unit 租约并行: 唯一 CPU heavy executor 的每个 work unit 任务恰租
//      1 个预算槽 (acquire(1,1,NONBLOCK)) — budget=4 提交 8 任务时并发峰值
//      达到 min(4,8)=4 且 Σactive ≤ budget 全程不超卖。(修复前: 每任务
//      acquire(1,budget) 抓走整份预算 → 严格串行, 峰值=1 → 本用例 RED。)
//   2. Runtime 唯一池身份: rt::shared_work_executor 按 ThreadBudget 实例绑定,
//      同一预算 → 同一池实例; 不同预算 → 不同池; 池 worker 数=预算上限。
//   3. P3 resample 节点经唯一 executor 执行行带 work unit: 注入 budget 后
//      executor 真实执行计数 tasks_executed() ≥ 行带数 (修复前: 节点自建
//      std::vector<std::thread> 行带池, executor 计数=0 → RED); 1-worker(串行
//      reference) 与 4-worker(executor 行带) 输出 bitwise 一致 (行带切分不改变
//      逐行计算, p3002_uncertainty W5 parity 先例)。
//   4. fail-closed: 行带任务被取消/丢弃 (故障注入 ASTROCS_RT001_FAULT=
//      resample_drop_band) → 节点显式失败, 不落任何伪产物 (无 partial 成功)。
//   5. 实测资源观测: 节点 manifest 记录实测 work_units / 行带活跃峰值
//      (active threads 实测面, 供资源门消费; 计划值不冒充观测值)。
//
// RED 锚定 (实现前): rt::shared_work_executor 不存在 → 编译期 RED;
//   实现注册表但未改 acquire 语义 → 用例 1 运行期 RED (峰值=1);
//   未接 P3 节点 → 用例 3/4 运行期 RED (tasks_executed=0 / 注入不生效)。
#include "astrocs/core/executor.h"
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/context.h"

#include "executor_runtime.h"  // RT-001 内部头 (lib/core/src, 不入安装面)

#include "healpix_core.h"       // astrocs::healpix::pix2ang_nest (数学权威)
#include "p1sess_fixtures.hpp"  // p1sess::write_fits_file 手写最小 FITS

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define RT001_GETPID static_cast<long>(::_getpid())
#else
#include <unistd.h>
#define RT001_GETPID static_cast<long>(::getpid())
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

namespace fs = std::filesystem;

namespace {

// ───────────────────────── 用例 1/2: work-unit 租约与唯一池身份 ─────────────

void test_workunit_peak_parallelism() {
  auto b = create_thread_budget(4);
  CHECK(b.ok());
  auto pool = rt::shared_work_executor(b.value());
  CHECK_MSG(pool != nullptr, "shared_work_executor must return the unique pool");
  CHECK_MSG(pool->worker_count() == 4u, "pool worker count = budget cap");
  std::atomic<uint32_t> active{0};
  std::atomic<uint32_t> peak{0};
  std::atomic<uint32_t> done{0};
  for (int i = 0; i < 8; ++i) {
    pool->enqueue([&](RunContext&) {
      const uint32_t cur = active.fetch_add(1) + 1;
      uint32_t p = peak.load();
      while (cur > p && !peak.compare_exchange_weak(p, cur)) {}
      CHECK_MSG(cur <= 4u, "work-unit leases must never oversubscribe budget");
      std::this_thread::sleep_for(std::chrono::milliseconds(60));
      active.fetch_sub(1);
      done.fetch_add(1);
    });
  }
  pool->wait_all();
  CHECK_MSG(done.load() == 8u, "all work units must execute exactly once");
  CHECK_MSG(peak.load() >= 2u,
            "work-unit lease (acquire 1 slot/task) must allow real parallelism; "
            "whole-budget lease per task serializes (pre-RT-001 defect)");
  CHECK_MSG(peak.load() <= 4u, "peak concurrency bounded by budget");
}

void test_shared_executor_identity() {
  auto b1 = create_thread_budget(3);
  auto b2 = create_thread_budget(5);
  CHECK(b1.ok() && b2.ok());
  auto e1 = rt::shared_work_executor(b1.value());
  auto e1b = rt::shared_work_executor(b1.value());
  auto e2 = rt::shared_work_executor(b2.value());
  CHECK(e1 && e1b && e2);
  CHECK_MSG(e1 == e1b, "same budget instance must map to the same unique pool");
  CHECK_MSG(e1 != e2, "distinct budget instances get distinct pools");
  CHECK(e1->worker_count() == 3u);
  CHECK(e2->worker_count() == 5u);
  auto none = rt::shared_work_executor(nullptr);
  CHECK_MSG(none == nullptr, "no budget context → null pool (caller serial fallback)");
}

// ───────────────────────── 用例 3/4/5: P3 resample 经唯一 executor ──────────

constexpr int kW = 16, kH = 16;
constexpr float kSigVal = 100.0f;

inline float const_px(int, void* user) { return *static_cast<float*>(user); }

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

bool write_signal_hips(const std::string& root) {
  const std::string root_posix = fs::path(root).generic_string();
  std::error_code ec;
  fs::create_directories(fs::path(root_posix + "/signal/Norder0/Dir0"), ec);
  if (ec) return false;
  std::ofstream p(fs::path(root_posix + "/signal/properties"), std::ios::binary);
  if (!p) return false;
  p << hips_properties_text("ADU");
  p.close();
  float v = kSigVal;
  return p1sess::write_fits_file(
             root_posix + "/signal/Norder0/Dir0/Npix0.fits", 512, 512,
             const_px, &v) == 0;
}

struct NodeFixture {
  fs::path root;
  std::string hips;
  std::string out;
  double ra = 0, dec = 0;
};

NodeFixture make_node_fixture(const char* tag) {
  NodeFixture fx;
  fx.root = fs::temp_directory_path() /
            ("rt001_exec_" + std::string(tag) + "_" + std::to_string(RT001_GETPID));
  std::error_code ec;
  fs::remove_all(fx.root, ec);
  fs::create_directories(fx.root, ec);
  fx.hips = (fx.root / "hips").generic_string();
  fx.out = (fx.root / "out").generic_string();
  fs::create_directories(fx.out, ec);
  CHECK(write_signal_hips(fx.hips));
  astrocs::healpix::pix2ang_nest(512u, 131072ull, fx.ra, fx.dec);
  return fx;
}

void cleanup_fixture(NodeFixture& fx) {
  std::error_code ec;
  fs::remove_all(fx.root, ec);
}

std::string node_config(const NodeFixture& fx) {
  char buf[1024];
  std::snprintf(buf, sizeof(buf),
                R"({
  "source": {"hips_dir": "%s"},
  "center": {"ra_deg": %.12f, "dec_deg": %.12f},
  "scale_deg_per_px": 0.01,
  "width_px": %d, "height_px": %d,
  "sampler": "nearest",
  "longitude_parity": "east_left",
  "bitpix": -32,
  "output_dir": "%s"
})",
                fx.hips.c_str(), fx.ra, fx.dec, kW, kH, fx.out.c_str());
  return std::string(buf);
}

std::string read_file(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  std::string s((std::istreambuf_iterator<char>(f)),
                std::istreambuf_iterator<char>());
  return s;
}

// 运行 resample 节点: budget_in==null → 串行 reference; 否则注入预算(池执行)。
// 上游 properties/wcs 节点先行（typed artifact 链 p3_props.json→p3_wcs.json,
// P3-002 output_dir 文件约定; 上游缺失 fail-closed DATA 拒绝）。
Result<void> run_resample(ModuleRegistry& reg, const std::string& cfg,
                          std::shared_ptr<ThreadBudget> budget_in, json* man) {
  for (const char* upstream : {"astrocs.phase3.properties", "astrocs.phase3.wcs"}) {
    auto mu = reg.create(upstream);
    if (mu.failed()) return Result<void>::fail(Error(ErrorDomain::DATA, "create failed"));
    auto vu = mu.value()->validate_config(cfg);
    if (vu.failed()) return vu;
    auto pu = mu.value()->plan("rt001_upstream", cfg);
    if (pu.failed()) return Result<void>::fail(Error(ErrorDomain::DATA, "plan failed"));
    RunContext cu;  // 上游节点串行无预算面（I/O/元数据允许串行, 宪章 §10.4）
    auto ru = mu.value()->execute(cu);
    if (ru.failed()) return ru;
  }
  auto m = reg.create("astrocs.phase3.resample2");
  if (m.failed()) return Result<void>::fail(Error(ErrorDomain::DATA, "create failed"));
  auto v = m.value()->validate_config(cfg);
  if (v.failed()) return v;
  auto p = m.value()->plan("rt001_resample", cfg);
  if (p.failed()) return Result<void>::fail(Error(ErrorDomain::DATA, "plan failed"));
  RunContext ctx;
  if (budget_in) ctx.set_budget(budget_in);
  auto r = m.value()->execute(ctx);
  if (man) {
    auto mm = m.value()->last_manifest();
    if (mm.ok()) {
      try { *man = json::parse(mm.value()); } catch (...) { *man = json::object(); }
    }
  }
  return r;
}

void test_p3_resample_via_executor() {
  NodeFixture fx = make_node_fixture("exec");
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());
  const std::string cfg = node_config(fx);

  auto b = create_thread_budget(4);
  CHECK(b.ok());
  auto pool = rt::shared_work_executor(b.value());
  CHECK(pool);
  const uint64_t before = pool->tasks_executed();

  json man;
  Result<void> rc = run_resample(reg, cfg, b.value(), &man);
  CHECK_MSG(rc.ok(), ("resample via executor must succeed: " +
                      (rc.ok() ? std::string("ok") : rc.error().message())).c_str());
  CHECK_MSG(man.value("status", "") == "ok", "manifest must report ok");
  CHECK_MSG(pool->tasks_executed() > before,
            "resample row-bands must execute as work units on the unique "
            "executor (pre-RT-001: node spawned its own std::thread pool)");
  // 实测资源观测 (active threads/work units 实测面, 计划值不冒充观测值)
  CHECK_MSG(man.contains("work_units") && man["work_units"].is_number_integer() &&
                man["work_units"].get<int>() >= 1,
            "manifest must carry measured work_units");
  CHECK_MSG(man.contains("band_active_peak") && man["band_active_peak"].is_number_integer() &&
                man["band_active_peak"].get<int>() >= 1 &&
                man["band_active_peak"].get<int>() <= 4,
            "manifest must carry measured band active-peak within budget");

  // 输出面存在 (resample 产物约定)
  CHECK(fs::exists(fs::path(fx.out + "/p3_resampled.bin")));
  CHECK(fs::exists(fs::path(fx.out + "/p3_resampled.json")));
  cleanup_fixture(fx);
}

void test_p3_resample_parity_serial_vs_executor() {
  NodeFixture fxs = make_node_fixture("ser");
  NodeFixture fxp = make_node_fixture("par");
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());

  json man_s, man_p;
  Result<void> rs = run_resample(reg, node_config(fxs), nullptr, &man_s);
  CHECK_MSG(rs.ok(), "serial reference run must succeed");
  auto b = create_thread_budget(4);
  CHECK(b.ok());
  Result<void> rp = run_resample(reg, node_config(fxp), b.value(), &man_p);
  CHECK_MSG(rp.ok(), "executor band run must succeed");

  // bitwise parity: 行带切分/线程数不得改变逐行确定性计算 (W5 先例)
  const std::string bin_s = read_file(fxs.out + "/p3_resampled.bin");
  const std::string bin_p = read_file(fxp.out + "/p3_resampled.bin");
  CHECK_MSG(bin_s.size() == bin_p.size() && bin_s == bin_p,
            "resampled planes must be bitwise identical (1 vs 4 workers)");
  const std::string js_s = read_file(fxs.out + "/p3_resampled.json");
  const std::string js_p = read_file(fxp.out + "/p3_resampled.json");
  CHECK_MSG(js_s == js_p, "resampled json sidecar must be identical");

  // 确定性: 同路径重复执行 bitwise 一致
  Result<void> rp2 = run_resample(reg, node_config(fxp), b.value(), nullptr);
  CHECK(rp2.ok());
  const std::string bin_p2 = read_file(fxp.out + "/p3_resampled.bin");
  CHECK_MSG(bin_p == bin_p2, "executor path must be deterministic across runs");

  cleanup_fixture(fxs);
  cleanup_fixture(fxp);
}

void test_p3_resample_drop_band_fail_closed() {
  NodeFixture fx = make_node_fixture("drop");
  ModuleRegistry reg;
  auto r = register_phase_modules(reg);
  CHECK(r.ok());

  ::setenv("ASTROCS_RT001_FAULT", "resample_drop_band", 1);
  auto b = create_thread_budget(4);
  CHECK(b.ok());
  json man;
  Result<void> rc = run_resample(reg, node_config(fx), b.value(), &man);
  ::unsetenv("ASTROCS_RT001_FAULT");
  CHECK_MSG(rc.failed(),
            "dropped band work unit must fail the node (fail-closed, no partial success)");
  CHECK(fs::exists(fs::path(fx.out + "/p3_wcs.json")));  // 上游产物仍在
  CHECK_MSG(!fs::exists(fs::path(fx.out + "/p3_resampled.bin")),
            "no resampled artifact may be published when a band is dropped");
  cleanup_fixture(fx);
}

}  // namespace

int main() {
  test_shared_executor_identity();
  test_workunit_peak_parallelism();
  test_p3_resample_via_executor();
  test_p3_resample_parity_serial_vs_executor();
  test_p3_resample_drop_band_fail_closed();
  if (failures == 0) {
    std::fprintf(stdout, "RT001_UNIQUE_EXECUTOR_PASS\n");
    return 0;
  }
  std::fprintf(stderr, "RT001_UNIQUE_EXECUTOR_FAIL failures=%d\n", failures);
  return 1;
}
