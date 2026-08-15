// lib/acr/utilization/staging_ledger.hpp — ACR staging reservation ledger
//
// 聚焦版 v2（08 §6 / 06 号规范 §4）：
// staging 不能用系统 RAM 总用量近似；由 ACR 自己的 reservation ledger
// 记录实际 staging 分配/释放。
#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

namespace astro::compute::utilization {

// ===== StagingLedger =====
// 线程安全。只记账（本轮同步语义下由调用方确保实际分配）；
// limit 来自 MemoryBudgetConfig.pinned_ratio + pinned_fixed_reserve。
class StagingLedger {
public:
    StagingLedger();
    ~StagingLedger();
    StagingLedger(const StagingLedger&) = delete;
    StagingLedger& operator=(const StagingLedger&) = delete;

    // 设置上限（字节）；0 表示未配置（默认不限制）
    void configure(std::size_t limit_bytes) noexcept;
    std::size_t limit() const noexcept;
    std::size_t used() const noexcept;

    // 尝试保留 bytes：成功返回 true 并记账；失败返回 false（不记账）
    bool reserve(std::size_t bytes) noexcept;

    // 释放 bytes（clamp 到已用）
    void release(std::size_t bytes) noexcept;

    // 状态 JSON（诊断/报告）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::utilization
