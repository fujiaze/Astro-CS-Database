// AstroCS Core Contracts — RT-001 唯一 Runtime 公共合同 (ARCH-001 §1)
//
// 冻结接口：Runtime create/run/cancel/inspect；ModuleDescriptor/ModulePlan/IModule；
// typed Port/DataArtifact；ThreadBudget/ThreadLease；logger/metrics/checkpoint。
//
// 边界约定（全部参数逐项注释 ownership / nullable / lifetime / thread-safety /
// blocking / unit / schema）：
// - C ABI 不抛异常；C++ 边界返回 Result/Error。
// - 不得把算法实现塞进 ABI；模块不得拿全局 scheduler。
// - 本文件是接口冻结点：签名/语义改动必须走新任务与 ABI layout 测试。
#pragma once

#ifndef ASTROCS_CORE_RUNTIME_H
#define ASTROCS_CORE_RUNTIME_H

#include "astrocs/core/artifact.h"
#include "astrocs/core/contracts.h"
#include "astrocs/core/context.h"
#include "astrocs/core/module.h"
#include "astrocs/core/pipeline.h"
#include "astrocs/core/scheduler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astrocs::core {

// ── ModulePlan: 模块执行前的资源/工作量计划 (RT-005) ──
// plan() 不得修改输入；只输出工作量、内存、I/O、并行轴与 kernel 请求。
struct ModulePlan {
  std::string node_id;                  // 计划所属 node（非空）
  uint64_t work_units = 0;              // 工作量（tile/row-band/样本数）
  uint64_t estimated_memory_bytes = 0;  // 峰值内存估计（NodePlan 预算用）
  std::vector<std::string> parallel_axes;  // 并行轴（"tile"/"row"/"sample"…）
  std::vector<std::string> kernel_ids;     // 请求的 provider kernel IDs
  bool cpu_heavy = false;               // execution class 判定
};

// ── IModule: 模块生命周期接口 (RT-005) ──
// 生命周期: describe → validate_config → plan → create → execute → inspect → destroy。
// execute 只使用 Runtime 提供的 lease/context/artifacts；inspect 不重执行科学计算。
class IModule {
 public:
  virtual ~IModule() = default;

  // 静态描述（descriptor；非空、不得与注册合同矛盾）
  virtual const ModuleDescriptor& descriptor() const noexcept = 0;

  // 校验 config JSON（input: config schema JSON 文本, 非空, 拥有方=调用者,
  // lifetime=本次调用有效; output err 可空, 由调用者持有）
  virtual Result<void> validate_config(const std::string& config_json) = 0;

  // 生成执行计划（input: node_id, config; output: ModulePlan）。
  // 禁止修改任何输入；线程安全：可并发调用。
  virtual Result<ModulePlan> plan(const std::string& node_id,
                                  const std::string& config_json) = 0;

  // 执行（input: RunContext& 引用, 拥有方=Runtime, 非空, 仅当前线程调用；
  // 输出 Result<void>；取消/异常必须归还 lease）
  virtual Result<void> execute(RunContext& ctx) = 0;

  // 结构化诊断（output JSON 文本；不重执行科学计算；线程安全：可并发）
  virtual Result<std::string> inspect() = 0;
};

// ── Runtime: 唯一生产调度入口 (ARCH-001 §1; 全进程唯一 owner) ──
// 禁止第二套调度器；模块不建私有 pool；ACR 不注册不链接。
class Runtime {
 public:
  virtual ~Runtime() = default;

  // 解析并校验 PipelineIR → 构建 DAG。
  // input: ir_json（pipeline_ir schema, 非空, 拥有方=调用者）;
  //        registry（ModuleRegistry&, 非空, 由调用者持有, 线程安全=调用期不并发写）。
  // output: Result<void>。
  virtual Result<void> load_pipeline(const std::string& ir_json,
                                     ModuleRegistry& registry) = 0;

  // 运行整条 pipeline（同步阻塞直至完成/失败/取消）。
  // input: RunContext&（由 Runtime 创建并持有; 非空）; profile_hint（可空, provider 提示）。
  // output: Result<void>；任何节点失败 → FAILED/SKIPPED 传播。
  virtual Result<void> run(RunContext& ctx) = 0;

  // 请求取消（线程安全：任意线程可调用；幂等）。
  virtual void cancel() noexcept = 0;

  // 结构化 inspect（output JSON 文本；不含科学计算；线程安全：可并发）。
  virtual Result<std::string> inspect() = 0;

  // 当前 DAG 节点状态快照（node_id → status；线程安全）。
  virtual std::vector<std::pair<std::string, NodeStatus>> node_statuses() const = 0;
};

// ── 工厂（C++ 边界；ownership: 调用者独占销毁） ──
// create_runtime(budget): 创建唯一 Runtime 实例；budget=有效 CPU 配额（>0）。
// 失败返回 Error(RESOURCE)。
Result<std::unique_ptr<Runtime>> create_runtime(uint32_t budget) noexcept;

}  // namespace astrocs::core

#endif  // ASTROCS_CORE_RUNTIME_H
