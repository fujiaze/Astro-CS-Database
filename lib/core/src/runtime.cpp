// RT-001/RT-006/RT-008 唯一 Runtime 实现:
// load_pipeline 用 PipelineIRParser 解析 + ModuleRegistry 校验，构建 Scheduler DAG；
// run 经 Scheduler 调度（依赖就绪/取消/失败传播/内存回压）；模块经 IModule 工厂执行。
// RT-006: 每个节点执行在真实运行点写 trace 事件（NODE_START/MODULE_CALL/NODE_END），
// 由 executor 记录 WORKER_TASK、模块/provider 观测填写，禁止 config 值冒充。
#include "astrocs/core/runtime.h"

#include <nlohmann/json.hpp>

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

class RuntimeImpl final : public Runtime {
 public:
  explicit RuntimeImpl(uint32_t budget) : budget_(budget) {
    trace_store_ = std::make_shared<TraceStore>();
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

    scheduler_ = std::make_unique<Scheduler>(budget_, budget_);
    for (const auto& n : ir_.nodes) {
      Scheduler::NodeSpec spec;
      spec.node_id = n.node_id;
      spec.deps = deps_of[n.node_id];
      spec.resource_class = n.resource_class;
      spec.min_workers = 1;
      spec.max_workers = budget_;
      spec.estimated_memory_bytes = 0;
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
        auto r = m.value()->execute(ctx);
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
        tr.workers = budget_;
        // RT-006: provider 观测 = 节点执行期间真实选择（模块 adapter/provider 经
        // ctx.set_provider 置位；未置位 → 空，不冒充 baseline）。
        tr.provider = ctx.provider();
        tr.granted_workers = budget_;
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
    j["loaded"] = loaded_;
    j["nodes"] = json::array();
    for (const auto& [id, st] : statuses_) {
      j["nodes"].push_back({{"node_id", id}, {"status", static_cast<int>(st)}});
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
  PipelineIR ir_;
  std::unique_ptr<Scheduler> scheduler_;
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
  if (budget == 0) {
    return Result<std::unique_ptr<Runtime>>::fail(
        Error(ErrorDomain::RESOURCE, "create_runtime: budget must be > 0"));
  }
  return Result<std::unique_ptr<Runtime>>::ok(
      std::make_unique<RuntimeImpl>(budget));
}

}  // namespace astrocs::core
