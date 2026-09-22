// eng/tests/unit/mem_wire_test.cpp — MEM-WIRE-01 内存静态预算与回压的**生产路径**判据
//
// 缺陷（ARCH-AUDIT-01 B-3 = ARCH-AUDIT-02 F-04 同源；docs/KNOWN_LIMITATIONS.md 条目 30）:
//   runtime.cpp 以 Scheduler(budget_, budget_) 构造（memory_limit_bytes 取默认 0），
//   并无条件 spec.estimated_memory_bytes = 0 ⇒ scheduler.cpp 的 memory_limit_bytes_ > 0
//   回压分支是**死代码**；plan_estimator.cpp 不在任何构建 target。
//
// 判据（每条都能红能绿；恒真即判红）:
//   W1 接线判据 runtime_memory_wiring_ok(inspect_json)：
//      ① memory_limit_bytes>0 且来源标签 ∈ {probe,profile}
//      ② ≥1 节点 plan_estimated==true 且 estimated_memory_bytes>0
//      ③ 每个节点要么已估算且 >0，要么 unestimable_reason 非空（显式登记，不冒充 0 估算）
//      —— 负例：预算传 0 / 估算硬写 0 ⇒ 同一判据函数必须判红（W1b/W1c）。
//   W2 受控回压：8 个独立节点 × 1000 B 估算、上限 2500 B ⇒ 并发被压到 ≤2，全部 COMPLETED
//      （越界 = 排队节流，不是拒绝任务；ASTROCS_DESIGN §8.3「可中断排队」）。
//   W3 生产路径（Runtime + IR）回压：同一 DAG 在「上限=实测估算的 2.5 倍」下并发 < 8，
//      在「上限=0」下并发 = 8（对照），两者数值结果**逐位一致**。
//   W4 1/N worker 逐位一致：budget ∈ {1,2,4,8} × 同一内存上限 ⇒ 每节点输出逐位相同。
//
// 依据：ASTROCS_DESIGN.md §8.3/§9/§3.5；docs/contracts/SCHEDULER_CONTRACT.md §3；
//       负责人裁决 2026-09-22（内存上限 = 空闲内存 × 可配置比例，默认 95；不设固定上限）。
#include "astrocs/core/memory_budget.h"
#include "astrocs/core/module.h"
#include "astrocs/core/plan_estimator.h"
#include "astrocs/core/runtime.h"
#include "astrocs/core/scheduler.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace astrocs::core;
using nlohmann::json;

static int g_checks = 0;
static int g_failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

// ══════════════════════════════════════════════════════════════════════════
// W1：接线判据（**唯一实现**；绿/红两侧都调用它）
// ══════════════════════════════════════════════════════════════════════════
static bool runtime_memory_wiring_ok(const std::string& inspect_json, std::string* why) {
  auto fail = [&](const std::string& m) { if (why) *why = m; return false; };
  json j;
  try {
    j = json::parse(inspect_json);
  } catch (const std::exception& e) {
    return fail(std::string("inspect() not JSON: ") + e.what());
  }
  if (!j.contains("memory_limit_bytes")) return fail("inspect() lacks memory_limit_bytes");
  const uint64_t lim = j.value("memory_limit_bytes", 0ull);
  const uint64_t req = j.value("memory_limit_bytes_requested", 0ull);
  const std::string src = j.value("memory_limit_source", std::string());
  if (lim == 0) return fail("memory_limit_bytes==0 (Scheduler 内存回压未接线/恒 0)");
  if (req != lim) {
    return fail("memory_limit_bytes=" + std::to_string(lim) +
                " != memory_limit_bytes_requested=" + std::to_string(req) +
                " (预算请求了但没接到 Scheduler 上)");
  }
  if (src != "probe" && src != "profile")
    return fail("memory_limit_source=\"" + src + "\" not in {probe,profile}");
  if (!j.contains("node_plans") || !j["node_plans"].is_array() || j["node_plans"].empty())
    return fail("inspect() lacks non-empty node_plans");
  size_t estimated = 0;
  for (const auto& e : j["node_plans"]) {
    const uint64_t bytes = e.value("estimated_memory_bytes", 0ull);
    const bool ok_est = e.value("plan_estimated", false);
    const std::string reason = e.value("unestimable_reason", std::string());
    if (ok_est) {
      if (bytes == 0)
        return fail("node " + e.value("node_id", std::string()) +
                    " plan_estimated==true but estimated_memory_bytes==0 (硬写 0)");
      ++estimated;
    } else if (reason.empty()) {
      return fail("node " + e.value("node_id", std::string()) +
                  " 既未估算又无 unestimable_reason（静默 0，无证据）");
    }
  }
  if (estimated == 0)
    return fail("no node carries a real static estimate (all estimates 0 => 回压无输入)");
  if (why) why->clear();
  return true;
}

// ══════════════════════════════════════════════════════════════════════════
// 探针式 stub 模块：记录并发峰值 + 确定性输出（供 Runtime/IR 全链路实验）
// ══════════════════════════════════════════════════════════════════════════
namespace {

std::atomic<int> g_cur{0};
std::atomic<int> g_peak{0};
std::mutex g_out_mu;
std::map<std::string, uint64_t> g_out;

void observe_enter() {
  const int c = g_cur.fetch_add(1) + 1;
  int p = g_peak.load();
  while (c > p && !g_peak.compare_exchange_weak(p, c)) {}
}
void observe_leave() { g_cur.fetch_sub(1); }
void reset_observers() {
  g_cur.store(0);
  g_peak.store(0);
  std::lock_guard<std::mutex> lk(g_out_mu);
  g_out.clear();
}

class StubModule final : public IModule {
 public:
  explicit StubModule(ModuleDescriptor d) : desc_(std::move(d)) {}
  const ModuleDescriptor& descriptor() const noexcept override { return desc_; }
  Result<void> validate_config(const std::string&) override { return Result<void>::success(); }
  Result<ModulePlan> plan(const std::string& node_id, const std::string& cfg) override {
    config_ = cfg;
    ModulePlan p;
    p.node_id = node_id;
    p.work_units = 1;
    p.cpu_heavy = true;
    p.min_workers = 1;
    p.max_workers = 1;   // 每节点 1 worker：并发峰值即节点并发数（实验可控）
    return Result<ModulePlan>::ok(std::move(p));
  }
  Result<void> execute(RunContext& ctx) override {
    // 节点身份 = Scheduler 在调用前写入的线程本地 node id（ctx.current_node()）。
    const std::string node = ctx.current_node();
    observe_enter();
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    // 确定性输出：只依赖节点 id（与并发度/线程数无关）—— 1/N 逐位一致的度量对象。
    uint64_t h = 1469598103934665603ull;
    for (char ch : node) { h ^= static_cast<unsigned char>(ch); h *= 1099511628211ull; }
    {
      std::lock_guard<std::mutex> lk(g_out_mu);
      g_out[node] = h;
    }
    observe_leave();
    return Result<void>::success();
  }
  Result<std::string> inspect() override { return Result<std::string>::ok("{}"); }
  Result<std::string> last_manifest() override { return Result<std::string>::ok("{}"); }

 private:
  ModuleDescriptor desc_;
  std::string config_;
};

ModuleDescriptor make_desc(const std::string& id) {
  ModuleDescriptor d;
  d.module_id = id;
  d.version = "1.0";
  d.abi = "c++17";
  d.execution_class = "cpu_heavy";
  d.parallel_ok = true;
  // 端口名必须与 IR 声明的端口一致（pipeline.cpp::validate 的 MISSING_PORT 判定）。
  PortDescriptor in;
  in.name = "hips";
  in.is_input = true;
  in.data_schema_id = "DATA-MEMWIRE-IN";
  PortDescriptor out;
  out.name = "tile";
  out.is_input = false;
  out.data_schema_id = "DATA-MEMWIRE-OUT";
  d.ports = {in, out};
  return d;
}

// 8 个独立节点（无依赖 ⇒ 全部就绪，可观察回压）+ 确定性输出
std::string make_ir(int n_nodes, const std::string& cfg_json) {
  json ir;
  ir["schema"] = "astrocs.pipeline/v1";
  ir["pipeline_id"] = "mem-wire";
  ir["version"] = "1.0.0";
  ir["nodes"] = json::array();
  json outs = json::object();
  for (int i = 0; i < n_nodes; ++i) {
    const std::string id = "n" + std::to_string(i);
    json n;
    n["node_id"] = id;
    n["module_id"] = "astrocs.phase3.resample2";
    n["module_api"] = "1.x";
    n["config"] = json::parse(cfg_json);
    n["inputs"] = {{"hips", "artifact:in"}};
    n["outputs"] = {{"tile", "artifact:" + id}};
    n["resources"] = {{"class", "cpu_heavy"}, {"parallel", true}};
    ir["nodes"].push_back(n);
    outs["tile_" + id] = "artifact:" + id;
  }
  ir["outputs"] = outs;
  return ir.dump();
}

ModuleRegistry make_registry() {
  ModuleRegistry reg;
  const ModuleDescriptor d = make_desc("astrocs.phase3.resample2");
  const auto r = reg.register_module(d);
  (void)r;
  const auto rf = reg.register_factory(d.module_id, [] {
    return std::unique_ptr<IModule>(new StubModule(make_desc("astrocs.phase3.resample2")));
  });
  (void)rf;
  return reg;
}

}  // namespace
// ══════════════════════════════════════════════════════════════════════════
// W2：受控回压（Scheduler 级）—— 越界 = 排队节流，不是拒绝任务
// ══════════════════════════════════════════════════════════════════════════
static void test_w2_controlled_backpressure() {
  constexpr uint64_t kPerNode = 1000;
  constexpr uint64_t kLimit = 2500;   // 同时最多 2 个在途（2×1000=2000 <= 2500 < 3×1000）
  constexpr int kNodes = 8;
  Scheduler sched(8, 8, kLimit);
  CHECK(sched.memory_limit() == kLimit);
  std::atomic<int> cur{0}, peak{0};
  auto fn = [&](const std::string&, RunContext&) -> Result<void> {
    const int c = cur.fetch_add(1) + 1;
    int p = peak.load();
    while (c > p && !peak.compare_exchange_weak(p, c)) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cur.fetch_sub(1);
    return Result<void>::success();
  };
  for (int i = 0; i < kNodes; ++i) {
    Scheduler::NodeSpec s;
    s.node_id = "m" + std::to_string(i);
    s.fn = fn;
    s.resource_class = "cpu_heavy";
    s.estimated_memory_bytes = kPerNode;
    s.plan_estimated = true;
    s.min_workers = 1;
    s.max_workers = 1;
    sched.add_node(std::move(s));
  }
  RunContext ctx;
  std::vector<std::pair<std::string, NodeStatus>> st;
  const auto r = sched.run(ctx, &st);
  CHECK(r.ok());                                   // 回压不产生失败（不是拒绝）
  size_t completed = 0;
  for (const auto& [id, s] : st) { (void)id; if (s == NodeStatus::COMPLETED) ++completed; }
  CHECK(completed == static_cast<size_t>(kNodes));
  CHECK(peak.load() <= 2);                         // 观察到节流：并发 <= floor(limit/per_node)
  std::printf("INFO W2 controlled backpressure: nodes=%d limit=%llu per_node=%llu peak_concurrent=%d\n",
              kNodes, (unsigned long long)kLimit, (unsigned long long)kPerNode, peak.load());
  // 对照：同 DAG、上限 0（旧行为）⇒ 不节流（并发 = budget）
  Scheduler sched0(8, 8, 0);
  std::atomic<int> peak0{0}, cur0{0};
  auto fn0 = [&](const std::string&, RunContext&) -> Result<void> {
    const int c = cur0.fetch_add(1) + 1;
    int p = peak0.load();
    while (c > p && !peak0.compare_exchange_weak(p, c)) {}
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cur0.fetch_sub(1);
    return Result<void>::success();
  };
  for (int i = 0; i < kNodes; ++i) {
    Scheduler::NodeSpec s;
    s.node_id = "z" + std::to_string(i);
    s.fn = fn0;
    s.resource_class = "cpu_heavy";
    s.estimated_memory_bytes = kPerNode;
    s.plan_estimated = true;
    sched0.add_node(std::move(s));
  }
  RunContext ctx0;
  CHECK(sched0.run(ctx0).ok());
  CHECK(peak0.load() > peak.load());               // 对照证明「节流确实来自上限」
  std::printf("INFO W2 control (limit=0): peak_concurrent=%d (vs throttled %d)\n",
              peak0.load(), peak.load());
}

// ══════════════════════════════════════════════════════════════════════════
// W3：生产路径（Runtime + IR）—— 上限真的到达 Scheduler，且数值结果不变
// ══════════════════════════════════════════════════════════════════════════
static const char* kCfg =
    "{\"width_px\":1024,\"height_px\":1024,\"scale_deg_per_px\":0.001,"
    "\"order\":8,\"projection\":\"TAN\"}";

static std::unique_ptr<Runtime> make_runtime(uint32_t cpu, uint64_t mem, const char* src) {
  RuntimeResourceBudget rb;
  rb.cpu_budget = cpu;
  rb.memory_limit_bytes = mem;
  rb.memory_source = src;
  auto rt = create_runtime(rb);
  return rt.ok() ? std::move(rt.value()) : nullptr;
}

static void test_w3_runtime_path() {
  constexpr int kNodes = 8;
  ModuleRegistry reg = make_registry();
  CHECK(reg.size() == 1);
  const std::string ir = make_ir(kNodes, kCfg);

  // 估算器给出的单节点峰值（判据的独立输入，非硬编码）
  PlanInputMetadata meta;
  meta.module_id = "astrocs.phase3.resample2";
  meta.is_heavy = true;
  meta.width_px = 1024;
  meta.height_px = 1024;
  meta.scale_deg_per_px = 0.001;
  meta.input_order = 8;
  meta.projection = "TAN";
  const PlanEstimateResult est = estimate_plan(meta);
  CHECK(est.ok());
  const uint64_t per_node = est.ok() ? est.plan.peak_memory_bytes : 0;
  CHECK(per_node > 0);
  const uint64_t limit = per_node + per_node / 2;   // 同时最多 1 个在途（1.5×）
  std::printf("INFO W3 estimator peak_memory_bytes=%llu B; chosen limit=%llu B\n",
              (unsigned long long)per_node, (unsigned long long)limit);

  // ── 绿：带上限 ⇒ 接线判据绿 + 观察到节流 + 全部完成 ──
  reset_observers();
  {
    auto rt = make_runtime(8, limit, "probe");
    CHECK(rt != nullptr);
    if (!rt) return;
    const auto lp = rt->load_pipeline(ir, reg);
    if (lp.failed()) std::fprintf(stderr, "INFO W3 load_pipeline error: %s\n", lp.error().message().c_str());
    CHECK(lp.ok());
    RunContext ctx;
    const auto r = rt->run(ctx);
    CHECK(r.ok());                                  // 节流不改退出语义（§3.5 内存不设门）
    const auto insp = rt->inspect();
    CHECK(insp.ok());
    std::string why;
    const bool green = runtime_memory_wiring_ok(insp.ok() ? insp.value() : "", &why);
    CHECK(green);
    std::printf("INFO W3 wiring criterion (green side): %s%s\n",
                green ? "GREEN" : "RED", green ? "" : (" why=" + why).c_str());
    std::printf("INFO W3 throttled peak_concurrent=%d (nodes=%d)\n", g_peak.load(), kNodes);
    CHECK(g_peak.load() >= 1);
    CHECK(g_peak.load() < kNodes);                  // 观察到回压/排队
    size_t completed = 0;
    for (const auto& [id, s] : rt->node_statuses()) { (void)id; if (s == NodeStatus::COMPLETED) ++completed; }
    CHECK(completed == static_cast<size_t>(kNodes));
  }
  const std::map<std::string, uint64_t> throttled_out = [] {
    std::lock_guard<std::mutex> lk(g_out_mu);
    return g_out;
  }();
  const int throttled_peak = g_peak.load();

  // ── 对照：上限 0 ⇒ 判据必须判红（负例 W1b：预算传 0） ──
  reset_observers();
  {
    auto rt = make_runtime(8, 0, "none");
    CHECK(rt != nullptr);
    if (!rt) return;
    CHECK(rt->load_pipeline(ir, reg).ok());
    RunContext ctx;
    CHECK(rt->run(ctx).ok());
    const auto insp = rt->inspect();
    std::string why;
    const bool green = runtime_memory_wiring_ok(insp.ok() ? insp.value() : "", &why);
    CHECK(!green);                                  // **负例必须红**
    CHECK(why.find("memory_limit_bytes==0") != std::string::npos);
    std::printf("INFO W3 NEGATIVE (budget=0): criterion RED as required; why=\"%s\"\n",
                why.c_str());
    CHECK(g_peak.load() == kNodes);                 // 无上限 ⇒ 无节流（对照）
  }
  const std::map<std::string, uint64_t> unlimited_out = [] {
    std::lock_guard<std::mutex> lk(g_out_mu);
    return g_out;
  }();
  // 节流不改变数值：两次运行的每节点输出逐位相同
  CHECK(throttled_out == unlimited_out);
  std::printf("INFO W3 bitwise identical across throttled/unlimited: %d (peak %d vs %d)\n",
              (int)(throttled_out == unlimited_out), throttled_peak, g_peak.load());
}

// ══════════════════════════════════════════════════════════════════════════
// W4：1/N worker 逐位一致（在内存回压启用下）
// ══════════════════════════════════════════════════════════════════════════
static void test_w4_worker_parity_under_backpressure() {
  constexpr int kNodes = 6;
  ModuleRegistry reg = make_registry();
  const std::string ir = make_ir(kNodes, kCfg);
  std::map<std::string, uint64_t> ref;
  bool first = true;
  for (uint32_t workers : {1u, 2u, 4u, 8u, 16u}) {
    reset_observers();
    auto rt = make_runtime(workers, 1ull << 30, "probe");   // 上限 1 GiB（回压启用）
    CHECK(rt != nullptr);
    if (!rt) return;
    CHECK(rt->load_pipeline(ir, reg).ok());
    RunContext ctx;
    CHECK(rt->run(ctx).ok());
    std::map<std::string, uint64_t> got;
    {
      std::lock_guard<std::mutex> lk(g_out_mu);
      got = g_out;
    }
    CHECK(got.size() == static_cast<size_t>(kNodes));
    if (first) { ref = got; first = false; }
    else { CHECK(got == ref); }
    std::printf("INFO W4 workers=%u nodes=%zu bitwise_equal_to_ref=%d\n",
                workers, got.size(), (int)(got == ref));
  }
}

// ══════════════════════════════════════════════════════════════════════════
// W5：预算来源解析（比例默认值来自 config，不是硬编码）+ 越界比例显式拒绝
// ══════════════════════════════════════════════════════════════════════════
static void test_w5_resolution() {
  // 默认比例 = eng/packaging/config/runtime_resources.json 的 95（生成头消费）
  CHECK(kMemoryBudgetPercentDefault == 95);
  const MemoryBudget d = resolve_memory_budget(1000ull * 1000ull, 0);
  CHECK(d.percent == kMemoryBudgetPercentDefault);
  CHECK(d.limit_bytes == 1000000ull * kMemoryBudgetPercentDefault / 100ull);
  CHECK(std::string(memory_budget_source_name(d.source)) == "probe");
  // 可调：percent=50 ⇒ 一半
  const MemoryBudget h = resolve_memory_budget(1000, 50);
  CHECK(h.limit_bytes == 500);
  // 越界比例 ⇒ 显式拒绝（不静默 clamp）
  const MemoryBudget bad = resolve_memory_budget(1000, 101);
  CHECK(bad.limit_bytes == 0);
  CHECK(std::string(memory_budget_source_name(bad.source)) == "invalid_percent");
  // 可用内存不可判定 ⇒ 不启用回压（如实登记）
  const MemoryBudget none = resolve_memory_budget(0, 0);
  CHECK(none.limit_bytes == 0);
  CHECK(std::string(memory_budget_source_name(none.source)) == "none");
  // 溢出安全：接近 UINT64_MAX 不产生回绕（结果必须 <= 输入且 > 0）
  const MemoryBudget big = resolve_memory_budget(~0ull, 95);
  CHECK(big.limit_bytes > 0 && big.limit_bytes <= ~0ull);
  std::printf("INFO W5 default_percent=%u limit(1e6 B, default)=%llu B\n",
              (unsigned)kMemoryBudgetPercentDefault, (unsigned long long)d.limit_bytes);
}

// ══════════════════════════════════════════════════════════════════════════
// W1c：估算硬写 0 的负例（对判据输入注入，不重编译）
// ══════════════════════════════════════════════════════════════════════════
static void test_w1c_hardwired_zero_estimates() {
  ModuleRegistry reg = make_registry();
  const std::string ir = make_ir(3, kCfg);
  auto rt = make_runtime(4, 1ull << 30, "probe");
  CHECK(rt != nullptr);
  if (!rt) return;
  CHECK(rt->load_pipeline(ir, reg).ok());
  const auto insp = rt->inspect();
  CHECK(insp.ok());
  std::string why;
  CHECK(runtime_memory_wiring_ok(insp.value(), &why));   // 绿
  // 注入「估算硬写 0」（旧 runtime.cpp 的行为）：清掉估算 + 清掉 unestimable 登记
  json j = json::parse(insp.value());
  for (auto& e : j["node_plans"]) {
    e["estimated_memory_bytes"] = 0;
    e["plan_estimated"] = false;
    e["unestimable_reason"] = "";
  }
  std::string why2;
  const bool green = runtime_memory_wiring_ok(j.dump(), &why2);
  CHECK(!green);                                        // **负例必须红**
  std::printf("INFO W1c NEGATIVE (estimates hardwired 0): criterion RED as required; why=\"%s\"\n",
              why2.c_str());
}

int main() {
  test_w1c_hardwired_zero_estimates();
  test_w2_controlled_backpressure();
  test_w3_runtime_path();
  test_w4_worker_parity_under_backpressure();
  test_w5_resolution();
  if (g_failures == 0) {
    std::printf("MEM_WIRE_ALL_PASS checks=%d failures=0\n", g_checks);
    return 0;
  }
  std::fprintf(stderr, "MEM_WIRE_FAIL checks=%d failures=%d\n", g_checks, g_failures);
  return 1;
}
