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

// ModulePlan / IModule 定义于 module.h（RT-005 冻结合同；此处不再重复）。

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

  // RT-008: 每个节点最近一次执行的 session manifest 摘要（node_id → JSON 文本）。
  // 供 CLI 收集跨阶段 artifact（ArtifactStore 绑定语义）。
  virtual std::vector<std::pair<std::string, std::string>> node_manifests() const = 0;

  // RT-009: 节点级运行 trace（node_id → 结构化记录；线程安全：run 完成后调用）。
  // 供 CLI 生成 observed graph 与 CHK-002 双向比较。每条记录含:
  //   status(COMPLETED/FAILED/CANCELLED/SKIPPED), started_utc, ended_utc (RFC3339),
  //   duration_ms, workers(实际租约), provider(当前 provider ID), error(失败消息, 可空)。
  struct NodeTrace {
    std::string node_id;
    std::string status;      // 终态字符串
    std::string started_utc; // RFC3339
    std::string ended_utc;   // RFC3339
    double duration_ms = 0.0;
    uint32_t workers = 0;
    std::string provider;    // 当前 provider ID（baseline/avx2/avx512）
    std::string error;       // 失败消息（成功时为空）
  };
  virtual std::vector<NodeTrace> node_trace() const = 0;
};

// ── 工厂（C++ 边界；ownership: 调用者独占销毁） ──
// create_runtime(budget): 创建唯一 Runtime 实例；budget=有效 CPU 配额（>0）。
// 失败返回 Error(RESOURCE)。
Result<std::unique_ptr<Runtime>> create_runtime(uint32_t budget) noexcept;

}  // namespace astrocs::core

#endif  // ASTROCS_CORE_RUNTIME_H
