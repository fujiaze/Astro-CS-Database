// RT-001/RT-006/RT-008 唯一 Runtime 实现:
// load_pipeline 用 PipelineIRParser 解析 + ModuleRegistry 校验，构建 Scheduler DAG；
// run 经 Scheduler 调度（依赖就绪/取消/失败传播/内存回压）；模块经 IModule 工厂执行。
// RT-006: 每个节点执行在真实运行点写 trace 事件（NODE_START/MODULE_CALL/NODE_END），
// 由 executor 记录 WORKER_TASK、模块/provider 观测填写，禁止 config 值冒充。
#include "astrocs/core/runtime.h"
#include "astrocs/core/context.h"        // B2-A18: 租约授予观测
#include "astrocs/core/plan_estimator.h"  // §8.3 静态预算（RT-005 估算器）

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <mutex>
#include <utility>

namespace astrocs::core {

using nlohmann::json;

namespace {

std::string utc_now() {
  char buf[32];
  std::time_t t = std::time(nullptr);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);   // MSVC: gmtime_r 不存在 (WIN-001)
#else
  gmtime_r(&t, &tm);
#endif
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

std::string utc_now_ms() {
  // RFC3339 UTC（毫秒精度）：观测时间戳（trace 事件 ts_utc 用）
  char buf[40];
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);   // MSVC: gmtime_r 不存在 (WIN-001)
#else
  gmtime_r(&t, &tm);
#endif
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch())
                      .count() %
                  1000;
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
  char out[48];
  std::snprintf(out, sizeof(out), "%s.%03lldZ", buf, static_cast<long long>(ms));
  return out;
}


// ── 从 IR 节点的**静态声明**构造 plan_estimator 的输入 metadata ──
// 依据 ASTROCS_DESIGN.md §8.3「静态预算：从输入数据（帧尺寸与类型、配置、模块声明）
// 静态估算每个模块的内存与 CPU 需求」。本函数**不做任何 IO**：只读 IR 节点自带的
// module_id / resource_class / config_json（含 CLI 展开的 phase 配置）。
// 键面口径（与生产 config 读取面一致，不新造同义键）：
//   width_px / height_px / scale_deg_per_px / projection —— p3 请求几何键
//     （module_adapters.cpp 的 p3 节点同键读取）
//   input_lights[]（数组长度）—— p1 帧数（n_frames）
//   order / target_order / n_tiles —— HiPS 层级与覆盖数（缺省 0 走估算器保守缺省）
// 帧尺寸在 p1 只存在于 FITS 头（核心层无 IO 面）⇒ 缺失时估算器显式拒绝，
// 本层如实登记 unestimable 原因，不用常量冒充估算值。
PlanInputMetadata node_plan_metadata(const PipelineNode& n) {
  PlanInputMetadata m;
  m.module_id = n.module_id;
  m.is_heavy = (n.resource_class == "cpu_heavy");
  m.n_frames = 1;
  if (n.config_json.empty()) return m;
  json c;
  try {
    c = json::parse(n.config_json);
  } catch (...) {
    return m;  // 配置不可解析 ⇒ 用保守缺省（估算器自行判定可否估算）
  }
  if (!c.is_object()) return m;
  auto u64 = [&](const char* k) -> uint64_t {
    if (!c.contains(k)) return 0;
    const auto& v = c.at(k);
    if (v.is_number_unsigned()) return v.get<uint64_t>();
    if (v.is_number_integer()) {
      const long long s = v.get<long long>();
      return s > 0 ? static_cast<uint64_t>(s) : 0;
    }
    return 0;
  };
  m.width_px = u64("width_px");
  m.height_px = u64("height_px");
  m.input_order = u64("order");
  m.output_order = u64("target_order");
  m.n_tiles_input = u64("n_tiles");
  if (c.contains("input_lights") && c.at("input_lights").is_array() &&
      !c.at("input_lights").empty()) {
    m.n_frames = static_cast<uint64_t>(c.at("input_lights").size());
  }
  if (c.contains("scale_deg_per_px") && c.at("scale_deg_per_px").is_number()) {
    m.scale_deg_per_px = c.at("scale_deg_per_px").get<double>();
  }
  if (c.contains("projection") && c.at("projection").is_string()) {
    m.projection = c.at("projection").get<std::string>();
  }
  return m;
}

class RuntimeImpl final : public Runtime {
 public:
  explicit RuntimeImpl(RuntimeResourceBudget rb)
      : budget_(rb.cpu_budget),
        memory_limit_bytes_(rb.memory_limit_bytes),
        memory_source_(std::move(rb.memory_source)) {
    trace_store_ = std::make_shared<TraceStore>();
    // ── MEMGOV-01: 内存压力治理器（ASTROCS_DESIGN.md §8.3:609-615 编排策略）──
    // 预算**复用** astrocs::core::resolve_memory_budget 的结果（本层不重算预算、
    // 不发明第二份口径）；压力分子来源由调用方注入。
    MemoryBudget mb;
    mb.limit_bytes = memory_limit_bytes_;
    mb.source = memory_budget_source_from_name(memory_source_);
    mb.available_bytes = rb.memory_available_bytes;
    mb.percent = rb.memory_budget_percent;
    governor_ = std::make_unique<MemoryPressureGovernor>(
        mb, pressure_policy_from_config(), rb.rss_probe);
    governor_->set_ledger_path(rb.pressure_ledger_path);
  }
  ~RuntimeImpl() override = default;

  Result<void> load_pipeline(const std::string& ir_json,
                             ModuleRegistry& registry) override {
    registry_ = registry;  // 拷贝：lambda 生命周期与 Runtime 一致，不悬垂引用
    PipelineIRParser parser;
    auto parsed = parser.parse(ir_json);
    if (parsed.failed()) return parsed.error();
    ir_ = parsed.value();

    // 静态验证（UNKNOWN_MODULE/MISSING_PORT/... 全量）
    auto issues = parser.validate(ir_, registry_);
    for (const auto& i : issues) {
      if (i.kind != IrError::NONE) {
        return Result<void>::fail(Error(ErrorDomain::DATA,
            "pipeline invalid: " + std::to_string(static_cast<int>(i.kind)) +
            " @" + i.node_id + ": " + i.detail));
      }
    }

    // 构图: artifact producer → 依赖边
    std::map<std::string, std::string> producer_of;   // artifact -> node_id
    std::map<std::string, std::vector<std::string>> deps_of;
    for (const auto& n : ir_.nodes) {
      for (const auto& [port, art] : n.outputs) producer_of[art] = n.node_id;
    }
    for (const auto& n : ir_.nodes) {
      for (const auto& [port, art] : n.inputs) {
        auto it = producer_of.find(art);
        if (it != producer_of.end()) deps_of[n.node_id].push_back(it->second);
      }
    }

    // 第三参 = 内存上限。
    // 修复前此处为 Scheduler(budget_, budget_) ⇒ memory_limit_bytes 取默认 0，
    // §8.3「静态预算 / 内存回压 / 内存永不越界」在生产路径整体失效（回压是死代码）。
    // 上限来源 = RuntimeResourceBudget（由调用方按 配置/profile + 实测探测 解析，
    // 见 astrocs/core/memory_budget.h；本层不发明数值、不硬编码）。
    scheduler_ = std::make_unique<Scheduler>(budget_, budget_, memory_limit_bytes_);
    // MEMGOV-01: 压力感知派发（就绪节点在压力高时不派发；帧轴经线程本地取用同一治理器）。
    scheduler_->set_memory_governor(governor_.get());
    for (const auto& n : ir_.nodes) {
      Scheduler::NodeSpec spec;
      spec.node_id = n.node_id;
      spec.deps = deps_of[n.node_id];
      spec.resource_class = n.resource_class;
      // P10-UTIL2-005: 由节点 plan() 自报真实 worker 需求，取代对全部节点填死的
      // (1, budget) 占位。plan() 只表达需求量（不含具体线程数，§10.4）：
      //   max_workers==0 -> 按可用预算回退 (budget)；否则取 min(budget, 声明值)。
      // 预算仍由 Scheduler 的唯一 ThreadBudget + P7 份额均分/租约语义分配。
      spec.min_workers = 1;
      spec.max_workers = budget_;
      uint64_t module_reported_memory = 0;  // §8.3「模块声明」面（ModulePlan 自报峰值）
      {
        auto mi = registry_.create(n.module_id);
        if (mi.ok()) {
          auto pl = mi.value()->plan(n.node_id, n.config_json);
          if (pl.ok()) {
            const uint32_t mn = pl.value().min_workers ? pl.value().min_workers : 1u;
            const uint32_t mx = pl.value().max_workers;
            spec.min_workers = mn;
            spec.max_workers =
                (mx == 0) ? budget_ : std::min(budget_, std::max(mx, mn));
            module_reported_memory = pl.value().estimated_memory_bytes;
          }
        }
      }
      // estimated_memory_bytes = §8.3「静态预算」的真实估算，
      // 不再硬写 0（硬写 0 会让内存回压恒不触发 —— 0+0 <= limit 恒真）。
      // 估算器 = astrocs::core::estimate_plan（RT-005，本任务纳入 astrocs_core 构建图），
      // 输入 metadata 全部取自 IR 节点静态声明（module_id / config / resources.class），
      // **不做任何 IO**（帧尺寸若不在 config 中则不可静态估算 —— 见下面 unestimable 登记）。
      {
        const PlanEstimateResult est = estimate_plan(node_plan_metadata(n));
        if (est.ok()) {
          spec.estimated_memory_bytes = est.plan.peak_memory_bytes;
          spec.plan_estimated = true;
          spec.plan_source = "estimator";
        } else {
          // 不可静态估算（如 P1 帧尺寸只在 FITS 头里、核心层无 IO 面）⇒ 保持 0
          // 并**显式登记原因**，绝不冒充「已估算为 0 字节」。
          spec.plan_unestimable_reason = est.error_message;
        }
        // §8.3 静态预算的第三个输入是「模块声明」：模块 plan() 自报的峰值工作集比通用
        // 估算器更具体（它知道自己的算法与缓存结构）⇒ 自报值 > 0 时优先。
        // ModulePlan.estimated_memory_bytes 此前是**无人写入的死字段**；本行起它是活的
        // （生产模块当前仍报 0 ⇒ 走估算器分支；填值属各模块域）。
        if (module_reported_memory > 0) {
          spec.estimated_memory_bytes = module_reported_memory;
          spec.plan_estimated = true;
          spec.plan_source = "module";
          spec.plan_unestimable_reason.clear();
        }
      }
      // 每个节点执行: 创建模块实例 → plan(config) → execute（模块内部走 session/lease）
      // RT-006: 在真实运行点写 trace 事件（禁止 config 值冒充观测）。
      spec.fn = [this, n](const std::string& node_id, RunContext& ctx) -> Result<void> {
        auto t_start = std::chrono::steady_clock::now();
        const std::string started_utc = utc_now_ms();
        std::string error_msg;
        // NODE_START 观测（调度器已把 store/run_id 注入 ctx；此处显式再确认）
        TraceEvent ev_start;
        ev_start.type = TraceEventType::NODE_START;
        ev_start.node_id = node_id;
        ev_start.ts_utc = started_utc;
        ev_start.status = "RUNNING";
        ev_start.module_id = n.module_id;
        ctx.record_trace(std::move(ev_start));
        auto m = registry_.create(n.module_id);
        if (m.failed()) {
          TraceEvent ev_err;
          ev_err.type = TraceEventType::ERROR;
          ev_err.node_id = node_id;
          ev_err.module_id = n.module_id;
          ev_err.status = "FAILED";
          ev_err.error_domain = "DATA";
          ev_err.error = "create module " + n.module_id + " failed: " +
                         m.error().message();
          ctx.record_trace(std::move(ev_err));
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "node " + node_id + ": create module " + n.module_id + " failed: " +
              m.error().message()));
        }
        ctx.log(LogLevel::INFO, "runtime",
                "execute node " + node_id + " module " + n.module_id);
        // RT-008: plan 先下发 config（SessionModule 存 config 供 execute 用）
        auto pl = m.value()->plan(node_id, n.config_json);
        if (pl.failed()) {
          TraceEvent ev_err;
          ev_err.type = TraceEventType::ERROR;
          ev_err.node_id = node_id;
          ev_err.module_id = n.module_id;
          ev_err.status = "FAILED";
          ev_err.error_domain = "DATA";
          ev_err.error = "plan failed: " + pl.error().message();
          ctx.record_trace(std::move(ev_err));
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "node " + node_id + ": plan failed: " + pl.error().message()));
        }
        // MODULE_CALL 观测：真实调用 module execute（call_count 由 store 汇总；
        // 本层每节点正常恰好一次 execute → call_count=1 为观测值）。
        TraceEvent ev_mod;
        ev_mod.type = TraceEventType::MODULE_CALL;
        ev_mod.node_id = node_id;
        ev_mod.module_id = n.module_id;
        if (!n.module_api.empty()) ev_mod.module_version = n.module_api;
        ev_mod.entry = m.value()->descriptor().module_id;  // 观测真实执行入口（模块 ID）
        ev_mod.call_count = 1;
        ctx.record_trace(std::move(ev_mod));
        // B2-A18: 节点执行前后快照真实租约观测 (不再用配置 budget_ 冒充)。
        const auto& obs_pre = granted_worker_observation();
        const uint64_t acq_before =
            obs_pre.acquired_total.load(std::memory_order_relaxed);
        auto r = m.value()->execute(ctx);
        const auto& obs_post = granted_worker_observation();
        const uint64_t acq_after =
            obs_post.acquired_total.load(std::memory_order_relaxed);
        // 本节点实际获得租约 → 峰值并行宽度; 无租约 = 0 (哨兵未观测)。
        const uint32_t node_granted =
            (acq_after > acq_before)
                ? obs_post.peak_lease.load(std::memory_order_relaxed)
                : 0u;
        // RT-008: 捕获节点 manifest（成功/失败都捕获；失败时含 error_kind 供 CLI 映射）
        auto man = m.value()->last_manifest();
        if (man.ok()) {
          std::lock_guard<std::mutex> lock(man_mu_);
          node_manifests_.push_back({node_id, man.value()});
        }
        if (r.failed()) {
          ctx.log(LogLevel::ERROR, "runtime",
                  "node " + node_id + " failed: " + r.error().message());
          error_msg = r.error().message();
          TraceEvent ev_err;
          ev_err.type = TraceEventType::ERROR;
          ev_err.node_id = node_id;
          ev_err.module_id = n.module_id;
          ev_err.status = "FAILED";
          ev_err.error_domain = error_domain_name(r.error().domain());
          ev_err.error = error_msg;
          ctx.record_trace(std::move(ev_err));
        }
        // RT-009: 记录节点 trace（时间/状态/provider/workers）
        const auto t_end = std::chrono::steady_clock::now();
        Runtime::NodeTrace tr;
        tr.node_id = node_id;
        tr.started_utc = started_utc;
        tr.ended_utc = utc_now();
        tr.duration_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        // B2-A18 (GAP-06/BASE-F3): workers/granted_workers 写真实观测的租约
        // 宽度，而非配置 budget_ (配置不得冒充观测, 宪章 §10.5/§17.6)。
        tr.workers = node_granted;
        // RT-006: provider 观测 = 节点执行期间真实选择（模块 adapter/provider 经
        // ctx.set_provider 置位；未置位 → 空，不冒充 baseline）。
        tr.provider = ctx.provider();
        tr.granted_workers = node_granted;
        tr.module_id = n.module_id;
        tr.entry = n.module_id;
        tr.call_count = 1;
        if (r.failed()) {
          tr.status = "FAILED";
          tr.error = error_msg;
        } else {
          tr.status = "COMPLETED";
        }
        {
          std::lock_guard<std::mutex> lock(trace_mu_);
          trace_.push_back(std::move(tr));
        }
        // NODE_END 观测
        TraceEvent ev_end;
        ev_end.type = TraceEventType::NODE_END;
        ev_end.node_id = node_id;
        ev_end.ts_utc = utc_now_ms();
        ev_end.module_id = n.module_id;
        ev_end.provider = tr.provider;
        ev_end.workers = static_cast<uint32_t>(tr.workers);
        ev_end.granted_workers = tr.granted_workers;
        ev_end.status = r.failed() ? "FAILED" : "COMPLETED";
        ev_end.wall_ms = tr.duration_ms;
        if (r.failed()) ev_end.error = error_msg;
        ctx.record_trace(std::move(ev_end));
        return r;
      };
      scheduler_->add_node(std::move(spec));
    }
    loaded_ = true;
    return Result<void>::success();
  }

  Result<void> run(RunContext& ctx) override {
    if (!loaded_ || !scheduler_) {
      return Result<void>::fail(Error(ErrorDomain::CONFIG, "runtime: no pipeline loaded"));
    }
    // RT-006: 每次 run 前清空上次观测（同一 Runtime 重复 run 的 trace 以本次为准）
    trace_store_->clear();
    {
      std::lock_guard<std::mutex> lock(trace_mu_);
      trace_.clear();
    }
    scheduler_->set_run_observation(trace_store_, run_id_);
    std::vector<std::pair<std::string, NodeStatus>> st;
    auto r = scheduler_->run(ctx, &st);
    statuses_ = std::move(st);
    return r;
  }

  void cancel() noexcept override {
    cancelled_.store(true, std::memory_order_release);
    if (scheduler_) scheduler_->cancel();
  }

  Result<std::string> inspect() override {
    json j;
    j["kind"] = "astrocs.runtime/v1";
    j["budget"] = budget_;
    // 资源预算观测面（CPU 与内存同源；SCHEDULER_CONTRACT §3）。
    // memory_limit_bytes = **Scheduler 实际生效**的上限（唯一权威：回压按它判定），
    // 而不是 Runtime 收到的请求值 —— 二者不等即「请求了但没接上」，判据据此判红。
    // memory_limit_bytes_requested = 请求值（证据面）；source="none" ⇒ 未启用回压。
    j["memory_limit_bytes"] = scheduler_ ? scheduler_->memory_limit() : 0ull;
    j["memory_limit_bytes_requested"] = memory_limit_bytes_;
    j["memory_limit_source"] = memory_source_;
    // MEMGOV-01: 压力治理观测面（压力实测值、水位、决策计数、台账路径）。
    // 只读快照；不参与任何判据（§3.5 内存不设门），供判据与报告核对"治理确实生效"。
    if (governor_) {
      j["memory_pressure"] = json::parse(governor_->status_json(), nullptr, false);
    }
    j["loaded"] = loaded_;
    j["nodes"] = json::array();
    for (const auto& [id, st] : statuses_) {
      j["nodes"].push_back({{"node_id", id}, {"status", static_cast<int>(st)}});
    }
    // 节点静态预算面（判据断言"估算确实来自估算器、未被硬写 0"的唯一机器可读来源）。
    j["node_plans"] = json::array();
    if (scheduler_) {
      for (const auto& np : scheduler_->node_plans()) {
        json e;
        e["node_id"] = np.node_id;
        e["estimated_memory_bytes"] = np.estimated_memory_bytes;
        e["plan_estimated"] = np.plan_estimated;
        e["plan_source"] = np.plan_source;
        e["unestimable_reason"] = np.unestimable_reason;
        e["resource_class"] = np.resource_class;
        j["node_plans"].push_back(std::move(e));
      }
    }
    return Result<std::string>::ok(j.dump());
  }

  std::vector<std::pair<std::string, NodeStatus>> node_statuses() const override {
    return statuses_;
  }

  // RT-008: 返回每个节点 execute 后捕获的 session manifest
  std::vector<std::pair<std::string, std::string>> node_manifests() const override {
    std::lock_guard<std::mutex> lock(man_mu_);
    return node_manifests_;
  }

  // RT-009: 节点级运行 trace（run 完成后调用；线程安全）
  std::vector<NodeTrace> node_trace() const override {
    std::lock_guard<std::mutex> lock(trace_mu_);
    return trace_;
  }

  // RT-006: 运行 trace 汇 JSONL 导出
  Result<std::string> trace_jsonl() const override {
    return Result<std::string>::ok(trace_store_->export_jsonl());
  }

  // RT-006: 运行 trace 重复/隐藏 session 检测
  std::vector<std::string> trace_violations() const override {
    return trace_store_->detect_repeated_calls();
  }

  void set_run_id(const std::string& run_id) override { run_id_ = run_id; }

  void set_registry(ModuleRegistry* reg) { registry_ = *reg; }

 private:
  uint32_t budget_;
  uint64_t memory_limit_bytes_ = 0;  // Scheduler 内存回压上限（来源见 memory_source_）
  std::string memory_source_ = "none";
  PipelineIR ir_;
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<MemoryPressureGovernor> governor_;  // MEMGOV-01（在 Scheduler 之前构造）
  ModuleRegistry registry_;
  std::vector<std::pair<std::string, NodeStatus>> statuses_;
  std::vector<std::pair<std::string, std::string>> node_manifests_;
  mutable std::mutex man_mu_;
  std::vector<NodeTrace> trace_;   // RT-009: 节点 trace
  mutable std::mutex trace_mu_;    // RT-009: trace 互斥
  std::atomic<bool> cancelled_{false};
  bool loaded_ = false;
  std::shared_ptr<TraceStore> trace_store_;  // RT-006: 运行 trace 事件汇
  std::string run_id_;                       // RT-006: 本次运行 ID
};

}  // namespace

Result<std::unique_ptr<Runtime>> create_runtime(uint32_t budget) noexcept {
  // 显式「未提供内存预算」的重载（memory_limit_bytes=0 ⇒ 不启用回压）。
  // 语义冻结，供单元测试/嵌入式调用方使用；生产路径用下面的 RuntimeResourceBudget 重载。
  RuntimeResourceBudget rb;
  rb.cpu_budget = budget;
  rb.memory_limit_bytes = 0;
  rb.memory_source = "none";
  return create_runtime(rb);
}

Result<std::unique_ptr<Runtime>> create_runtime(const RuntimeResourceBudget& rb) noexcept {
  if (rb.cpu_budget == 0) {
    return Result<std::unique_ptr<Runtime>>::fail(
        Error(ErrorDomain::RESOURCE, "create_runtime: budget must be > 0"));
  }
  return Result<std::unique_ptr<Runtime>>::ok(
      std::make_unique<RuntimeImpl>(rb));
}

}  // namespace astrocs::core
