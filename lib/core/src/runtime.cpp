// RT-001/RT-006 唯一 Runtime 实现:
// load_pipeline 用 PipelineIRParser 解析 + ModuleRegistry 校验，构建 Scheduler DAG；
// run 经 Scheduler 调度（依赖就绪/取消/失败传播/内存回压）；模块经 IModule 工厂执行。
#include "astrocs/core/runtime.h"

#include <nlohmann/json.hpp>

#include <map>
#include <utility>

namespace astrocs::core {

using nlohmann::json;

namespace {

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
      // 每个节点执行: 创建模块实例 → execute（模块内部走 session/lease）
      spec.fn = [this, n](const std::string& node_id, RunContext& ctx) -> Result<void> {
        auto m = registry_.create(n.module_id);
        if (m.failed()) {
          return Result<void>::fail(Error(ErrorDomain::DATA,
              "node " + node_id + ": create module " + n.module_id + " failed: " +
              m.error().message()));
        }
        ctx.log(LogLevel::INFO, "runtime",
                "execute node " + node_id + " module " + n.module_id);
        auto r = m.value()->execute(ctx);
        if (r.failed()) {
          ctx.log(LogLevel::ERROR, "runtime",
                  "node " + node_id + " failed: " + r.error().message());
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

  void set_registry(ModuleRegistry* reg) { registry_ = *reg; }

 private:
  uint32_t budget_;
  PipelineIR ir_;
  std::unique_ptr<Scheduler> scheduler_;
  ModuleRegistry registry_;
  std::vector<std::pair<std::string, NodeStatus>> statuses_;
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
