// ACSD Core — 内存静态预算的**来源解析**（纯函数，无 IO、无平台依赖）
//
// 规范依据（逐条）：
//   * 负责人裁决 2026-09-22（逐字）：「我是不设置上限，有多少资源吃多少资源。（内存
//     最高吃掉空闲的 95% 避免卡死，且**这个参数配置在 config 里面可调，默认 95**）」
//     ⇒ **不设固定上限**：预算动态取自**当前可用内存** × **可配置比例**（默认 95%）。
//     目的是避免卡死，不是省内存；比例的唯一数值源 = eng/packaging/config/
//     runtime_resources.json（经 CMake configure_file 生成 runtime_resources_generated.h，
//     实现侧零字面量 —— 与 G-RES-01 的 resource_gate_thresholds_generated.h 同款）。
//   * ASTROCS_DESIGN.md §8.3「静态预算：从输入数据……静态估算每个模块的内存与 CPU 需求；
//     该估算是调度决策的输入」+ 编排策略「**内存占用永不越界**」；越界处置按该节原文为
//     「**可中断排队**」/「**可丢弃重跑**」—— 即**回压（节流）**，不是拒绝任务。
//   * docs/contracts/SCHEDULER_CONTRACT.md §3「内存上限（峰值工作集）……**由配置/资源门
//     决定**，禁止硬编码」。
//   * ASTROCS_DESIGN.md §9「一个进程一个资源调度器与线程预算源」—— 内存与 CPU 预算同源
//     （机器绑定配置 + 实测探测）。
//   * ASTROCS_DESIGN.md §3.5 / ENGINEERING_SPEC.md §12 / eng/contracts/resource_gate_v1.json
//     的 disk_gate.no_gate = [memory, cpu, threads]「资源门只管磁盘」⇒ 本预算是**调度准入
//     输入**，**不是门禁判据**：不产生退出码、不阻断运行、不让 run 失败。
//     （§3.5 与 §8.3 不互斥：前者约束「门禁语义」，后者约束「调度准入语义」。）
//
// 本文件只做一件事：把「可用内存」与「比例」折成一个预算值 + 一个可观测的来源标签。
// **不含任何内存上限字面量**。
#pragma once

#ifndef ASTROCS_CORE_MEMORY_BUDGET_H
#define ASTROCS_CORE_MEMORY_BUDGET_H

#include <cstdint>

namespace astrocs::core {

// 比例默认值（唯一数值源 = eng/packaging/config/runtime_resources.json；
// 定义在 memory_budget.cpp，经生成头 runtime_resources_generated.h 引入）。
extern const std::uint32_t kMemoryBudgetPercentDefault;

// 预算来源标签（观测/证据用；稳定字符串，随 Runtime::inspect()/stderr 落盘）。
//   "probe"           —— 运行期可用内存探测（aio 边界的唯一实现）
//   "none"            —— 可用内存不可判定（探测返回 0）⇒ limit=0 = 不启用回压（如实登记）
//   "invalid_percent" —— 配置比例越界（不在 1..100）⇒ limit=0 + 显式拒绝静默降级
enum class MemoryBudgetSource : std::uint8_t {
  NONE = 0,
  PROBE = 1,
  INVALID_PERCENT = 2,
};

const char* memory_budget_source_name(MemoryBudgetSource s) noexcept;

struct MemoryBudget {
  std::uint64_t limit_bytes = 0;        // 峰值工作集上限；0 = 不启用内存回压
  MemoryBudgetSource source = MemoryBudgetSource::NONE;
  std::uint64_t available_bytes = 0;    // 探测到的可用内存（输入回显；证据面）
  std::uint32_t percent = 0;            // **实际采用**的比例（输入回显；证据面）
};

// 预算 = floor(available_bytes × percent / 100)。纯函数、无副作用、可并发调用；
// 同输入必同输出。
//   percent == 0            ⇒ 未配置，取 kMemoryBudgetPercentDefault（默认 95）；
//   percent ∉ [1,100]       ⇒ INVALID_PERCENT + limit=0（显式拒绝，不静默 clamp）；
//   available_bytes == 0    ⇒ NONE + limit=0（不可判定；调用方须 fail-closed 登记）。
MemoryBudget resolve_memory_budget(std::uint64_t available_bytes,
                                   std::uint32_t percent) noexcept;

}  // namespace astrocs::core

#endif  // ASTROCS_CORE_MEMORY_BUDGET_H
