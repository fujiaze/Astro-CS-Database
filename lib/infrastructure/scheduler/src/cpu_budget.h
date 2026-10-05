// ACSD Core — 节点并行预算的唯一解析面（X1：帧内并行预算接到运行期预算）
//
// 依据（逐条）：
//   * docs/engineering/architecture/DATA_FLOW.md「并行轴分配」（冻结口径）：并行轴 = 帧轴 × 帧内轴，
//     两轴之积 ≤ 预算；**预算唯一来源 = 运行期配额**（禁硬编码线程数）；
//   * docs/engineering/contracts/SCHEDULER.md §3：线程预算由配置/资源门决定，禁硬编码常数；
//     §8.3:647「一个进程只有一个资源预算源（§9），线程池的唯一来源是调度器」；
//   * docs/engineering/CONCURRENCY_STANDARD.md「默认」节：线程数 = 外部可配置，
//     **默认 min(可用核, 配置上限)**；取值来源 = 配置（16 不作硬编码值）。
//
// 语义（唯一权威 = 运行期预算，**不是**使用方传参）：
//   ① 调度器注入的 lease（节点 config 的 __workers）**存在** → 以它为准（调用方 fail-closed：
//      只收紧不放大；1 显式表示串行 reference）；
//   ② lease **不存在**（直接调用 op 的非调度路径：门禁/单元/集成）→ 取「进程有效 CPU 预算」：
//       CLI 启动时注入的可用核（亲和性 ∩ cgroup）优先，未注入则现算同一口径
//       min(可用核, 配置上限) —— **缺省不是 1**。
// 缺陷背景（X1 根因）：旧实现在 ② 恒取 1 ⇒ 节点内 omp parallel for 继承 ICV=1 ⇒
//   整批科学内核单线程（PSF 域实测 1,086.0 s vs 16 线程探针 101.2 s，10.7×）。
#pragma once

#include <cstdint>

namespace acsd::core {

// 进程可用核数（与 CLI 同口径：Linux sched_getaffinity(0)，Windows GetActiveProcessorCount；
// 不可判定 → std::thread::hardware_concurrency()；下限 1）。
std::uint32_t affinity_cpu_count() noexcept;

// 配置上限（受控配置键 cpu_budget_max；0 = 不设上限）。
std::uint32_t cpu_budget_cap() noexcept;

// 进程级预算注入（CLI 启动时以可用核注入；0 = 清除注入）。只影响「未注入 lease」的路径。
void set_process_cpu_budget(std::uint32_t workers) noexcept;

// 进程有效 CPU 预算：注入值优先，否则 min(affinity_cpu_count(), cpu_budget_cap())。恒 ≥ 1。
std::uint32_t process_cpu_budget() noexcept;

// 节点线程预算解析（op 层唯一调用面）：lease_present=true 时返回 max(1, lease_workers)，
// 否则返回 process_cpu_budget()。
std::uint32_t node_thread_budget(bool lease_present, std::uint32_t lease_workers) noexcept;

}  // namespace acsd::core