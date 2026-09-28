// ACSD Core — 节点并行预算实现（X1）。唯一数值源 = 运行期配额 + 受控配置上限键。
#include "cpu_budget.h"

#include <algorithm>
#include <atomic>
#include <thread>

#include "runtime_resources_generated.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sched.h>
#endif

namespace astrocs::core {
namespace {

std::atomic<std::uint32_t> g_process_budget{0};

}  // namespace

std::uint32_t affinity_cpu_count() noexcept {
#if defined(_WIN32)
  // Windows: 有效处理器数（与 lib/infrastructure/cli/commands.cpp 原实现同口径）。
  const DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
  if (n > 0) return static_cast<std::uint32_t>(n);
#else
  cpu_set_t set;
  CPU_ZERO(&set);
  if (sched_getaffinity(0, sizeof(set), &set) == 0) {
    std::uint32_t n = 0;
    for (int i = 0; i < CPU_SETSIZE; ++i)
      if (CPU_ISSET(i, &set)) ++n;
    if (n > 0) return n;
  }
#endif
  const unsigned hw = std::thread::hardware_concurrency();
  return hw == 0 ? 1u : static_cast<std::uint32_t>(hw);
}

std::uint32_t cpu_budget_cap() noexcept {
  return static_cast<std::uint32_t>(astrocs::runtime_resources::kCpuBudgetMax);
}

void set_process_cpu_budget(std::uint32_t workers) noexcept {
  g_process_budget.store(workers, std::memory_order_relaxed);
}

std::uint32_t process_cpu_budget() noexcept {
  std::uint32_t n = g_process_budget.load(std::memory_order_relaxed);
  if (n == 0u) n = affinity_cpu_count();
  const std::uint32_t cap = cpu_budget_cap();
  if (cap > 0u) n = std::min(n, cap);
  return n == 0u ? 1u : n;
}

std::uint32_t node_thread_budget(bool lease_present,
                                 std::uint32_t lease_workers) noexcept {
  if (lease_present) return std::max<std::uint32_t>(1u, lease_workers);
  return process_cpu_budget();
}

}  // namespace astrocs::core