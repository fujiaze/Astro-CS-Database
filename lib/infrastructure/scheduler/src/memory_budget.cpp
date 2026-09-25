// ACSD Core — 内存静态预算来源解析实现（纯函数）
// 层次合同与规范依据见 lib/include/astrocs/core/memory_budget.h。
#include "astrocs/core/memory_budget.h"

#include <string>

// 比例默认值的唯一数值源（CMake 从 eng/packaging/config/runtime_resources.json 生成）。
#include "runtime_resources_generated.h"

namespace astrocs::core {

const std::uint32_t kMemoryBudgetPercentDefault =
    astrocs::runtime_resources::kMemoryBudgetPercentDefault;

const char* memory_budget_source_name(MemoryBudgetSource s) noexcept {
  switch (s) {
    case MemoryBudgetSource::PROBE: return "probe";
    case MemoryBudgetSource::INVALID_PERCENT: return "invalid_percent";
    case MemoryBudgetSource::NONE: break;
  }
  return "none";
}

MemoryBudgetSource memory_budget_source_from_name(const std::string& name) noexcept {
  if (name == "probe") return MemoryBudgetSource::PROBE;
  if (name == "invalid_percent") return MemoryBudgetSource::INVALID_PERCENT;
  return MemoryBudgetSource::NONE;
}

MemoryBudget resolve_memory_budget(std::uint64_t available_bytes,
                                   std::uint32_t percent) noexcept {
  MemoryBudget b;
  b.available_bytes = available_bytes;
  std::uint32_t pct = percent;
  if (pct == 0) pct = kMemoryBudgetPercentDefault;   // 未配置 ⇒ 默认（95）
  b.percent = pct;
  if (pct < astrocs::runtime_resources::kMemoryBudgetPercentMin ||
      pct > astrocs::runtime_resources::kMemoryBudgetPercentMax) {
    b.limit_bytes = 0;                                // 越界 ⇒ 不静默 clamp
    b.source = MemoryBudgetSource::INVALID_PERCENT;
    return b;
  }
  if (available_bytes == 0) {
    b.limit_bytes = 0;                                // 不可判定 ⇒ 不启用回压
    b.source = MemoryBudgetSource::NONE;
    return b;
  }
  // floor(available × pct / 100) 的溢出安全整数式（available 可接近 UINT64_MAX，
  // 直接相乘会回绕；先除后乘 + 余数项，语义恒为 floor，无浮点）。
  const std::uint64_t whole = (available_bytes / 100u) * pct;
  const std::uint64_t rem = ((available_bytes % 100u) * pct) / 100u;
  b.limit_bytes = whole + rem;
  b.source = MemoryBudgetSource::PROBE;
  return b;
}

}  // namespace astrocs::core
