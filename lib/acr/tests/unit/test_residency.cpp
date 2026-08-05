// lib/acr/tests/unit/test_residency.cpp — 数据驻留与内存预算测试
//
// 08 号计划 §6 / 06 号规范：
//   - ResidencyManager 状态流转（Host/Device/Both/dirty）
//   - 上传复用（同输入不重复整帧上传）
//   - 传输次数报告
//   - pinned staging 独立记账
#include <gtest/gtest.h>

#include "residency_manager.hpp"
#include "memory_budget.hpp"

#include <string>

using namespace astro::compute::scheduler;
using namespace astro::compute::utilization;

// ============================================================================
// 1. 状态流转
// ============================================================================
TEST(Residency, StateTransitions) {
    ResidencyManager m;
    m.register_buffer("img", 4096);
    EXPECT_EQ(m.state("img"), ResidencyState::HostValid);

    m.mark_uploaded("img");
    EXPECT_EQ(m.state("img"), ResidencyState::BothValid);
    EXPECT_TRUE(m.is_device_valid("img", "cuda:0"));
    EXPECT_FALSE(m.needs_upload("img", "cuda:0"));

    m.mark_device_dirty("img");
    EXPECT_EQ(m.state("img"), ResidencyState::DeviceDirty);
    EXPECT_TRUE(m.needs_download("img"));

    m.mark_downloaded("img");
    EXPECT_EQ(m.state("img"), ResidencyState::HostValid);
}

// ============================================================================
// 2. 上传复用：host 未变时 device 有效，不重复上传
// ============================================================================
TEST(Residency, UploadReuse) {
    ResidencyManager m;
    m.register_buffer("shared_input", 8192);
    EXPECT_TRUE(m.needs_upload("shared_input", "cuda:0"));
    m.mark_uploaded("shared_input");
    // 第二次块 claim：不再需要上传（复用）
    EXPECT_FALSE(m.needs_upload("shared_input", "cuda:0"));
    EXPECT_EQ(m.upload_count("shared_input"), 1u);
    // host 修改后需要重新上传
    m.mark_host_dirty("shared_input");
    EXPECT_TRUE(m.needs_upload("shared_input", "cuda:0"));
    m.mark_uploaded("shared_input");
    EXPECT_EQ(m.upload_count("shared_input"), 2u);
}

// ============================================================================
// 3. 报告
// ============================================================================
TEST(Residency, ReportCounts) {
    ResidencyManager m;
    m.register_buffer("a", 1024);
    m.register_buffer("b", 2048);
    m.mark_uploaded("a");
    m.mark_uploaded("b");
    m.mark_downloaded("a");
    EXPECT_EQ(m.total_uploads(), 2u);
    EXPECT_EQ(m.total_downloads(), 1u);
    std::string j = m.status_json();
    EXPECT_NE(j.find("\"total_uploads\":2"), std::string::npos);
    EXPECT_NE(j.find("\"uploads\":1"), std::string::npos);
}

// ============================================================================
// 4. pinned staging 独立记账
// ============================================================================
TEST(Residency, PinnedBudgetIndependent) {
    MemoryBudgetController c;
    MemoryBudgetConfig cfg;
    cfg.pinned_ratio = 0.5;
    cfg.pinned_fixed_reserve_bytes = 1024;
    c.configure(cfg);
    c.sample();  // 获取系统总量
    auto m = c.report_pinned(500, 4096);
    EXPECT_TRUE(m.pinned_valid);
    EXPECT_EQ(m.pinned_limit, 2048u);   // min(4096*0.5, 4096-1024)=2048
    EXPECT_FALSE(m.pinned_exceeded);
    auto m2 = c.report_pinned(3000, 4096);
    EXPECT_TRUE(m2.pinned_exceeded);
}
