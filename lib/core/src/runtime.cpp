// RT-001 唯一 Runtime 实现 — 工厂与最小可用调度入口。
// 本文件冻结 Runtime 公共合同的可编译实现；完整 DAG/租约/回压在 RT-006 扩展。
#include "astrocs/core/runtime.h"

#include <utility>

namespace astrocs::core {

namespace {

class RuntimeImpl final : public Runtime {
 public:
  explicit RuntimeImpl(uint32_t budget) : budget_(budget) {}
  ~RuntimeImpl() override = default;

  Result<void> load_pipeline(const std::string& ir_json,
                             ModuleRegistry& registry) override {
    // RT-004 完整实现 schema 校验；此处只登记非空与 IR 解析基础
    if (ir_json.empty()) {
      return Result<void>::fail(Error(ErrorDomain::CONFIG, "runtime: empty ir_json"));
    }
    ir_json_ = ir_json;
    return Result<void>::success();
  }

  Result<void> run(RunContext& ctx) override {
    // RT-006 完整实现 DAG 调度；此处调用已注册节点的基础执行（若有）
    if (ir_json_.empty()) {
      return Result<void>::fail(Error(ErrorDomain::CONFIG, "runtime: no pipeline loaded"));
    }
    // 最小路径：允许子类/后续接入 scheduler；本阶段只做合同验证
    ctx.log(LogLevel::INFO, "runtime", "run: pipeline loaded, budget=" +
                                           std::to_string(budget_));
    return Result<void>::success();
  }

  void cancel() noexcept override {
    cancelled_.store(true, std::memory_order_release);
  }

  Result<std::string> inspect() override {
    return Result<std::string>::ok(
        std::string("{\"kind\":\"astrocs.runtime/v1\",\"budget\":") +
        std::to_string(budget_) + ",\"loaded\":" +
        (ir_json_.empty() ? "false" : "true") + "}");
  }

  std::vector<std::pair<std::string, NodeStatus>> node_statuses() const override {
    return {};
  }

 private:
  uint32_t budget_;
  std::string ir_json_;
  std::atomic<bool> cancelled_{false};
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
