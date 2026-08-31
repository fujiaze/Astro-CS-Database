// RT-001/RT-006 唯一 Runtime 实现:
// load_pipeline 用 PipelineIRParser 解析 + ModuleRegistry 校验，构建 Scheduler DAG；
// run 经 Scheduler 调度（依赖就绪/取消/失败传播/内存回压）；模块经 IModule 工厂执行。
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
  gmtime_r(&t, &tm);
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return buf;
}

class RuntimeImpl final : public Runtime {
 public:
  explicit RuntimeImpl(uint32_t budget) : budget_(budget) {}
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
      spec.fn = [this, n](const std::string& node_id, RunContext& ctx) -> Result<void> {
        auto t_start = std::chrono::steady_clock::now();
        const std::string started_utc = utc_now();
        std::string error_msg;
        auto m = registry_.create(n.module_id);
        if (m.failed()) {
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "node " + node_id + ": create module " + n.module_id + " failed: " +
              m.error().message()));
        }
        ctx.log(LogLevel::INFO, "runtime",
                "execute node " + node_id + " module " + n.module_id);
        // RT-008: plan 先下发 config（SessionModule 存 config 供 execute 用）
        auto pl = m.value()->plan(node_id, n.config_json);
        if (pl.failed()) {
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "node " + node_id + ": plan failed: " + pl.error().message()));
        }
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
        }
        // RT-009: 记录节点 trace（时间/状态/provider/workers）
        const auto t_end = std::chrono::steady_clock::now();
        Runtime::NodeTrace tr;
        tr.node_id = node_id;
        tr.started_utc = started_utc;
        tr.ended_utc = utc_now();
        tr.duration_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
        tr.workers = budget_;
        tr.provider = "baseline";  // CPU-001 前唯一 provider；G3 后按 profile 路由
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
