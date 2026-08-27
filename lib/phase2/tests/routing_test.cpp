// lib/phase2/tests/routing_test.cpp — CON-003 production CLI->module routing test
// 验证 stage2 CLI 与 gate 测试共享同一生产 parse+build 路径（非 mock），
// 且 worker 预算(CON-002)与 ACR cpu 路由在到达模块前保持正确。
#include "astro/phase2/stage2_common.h"
#include "astro/phase2/execution_options.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace astro::phase2;

// 生产风格配置：与 lib/phase2/configs/stage2_real_3frame.json 同结构。
static nlohmann::json production_config() {
    return nlohmann::json::parse(R"({
      "version":1,
      "inputs":{"hips":["a","b","c"]},
      "model":{"control_grid_per_tile":8,"robust_loss":"huber","snr_weight_mode":"snr2_normalized"},
      "integration":{"precision":"fp32","memory_limit_mb":8192,"weight_mode":"auto",
                    "acr_route":"cpu",
                    "rejection":{"method":"sigma","underdetermined_n":2,"robust_mad_clip":{"lower_sigma":4.0,"upper_sigma":3.0,"max_iterations":8}}},
      "output":{"hips":"out"}
    })");
}

TEST(Phase2Routing, ProductionParseBuildSharedPath) {
    P2Stage2Config cfg;
    std::string err;
    ASSERT_TRUE(p2_stage2_parse_config(production_config(), &cfg, &err)) << err;
    // CLI 与 gate 测试共用同一生产 parse+build 路径：
    P2UpmBuildConfig m = p2_stage2_make_upm_cfg(cfg, cfg.target_order, "deadbeef");
    EXPECT_EQ(m.robust_loss, 0);            // huber
    EXPECT_EQ(m.snr_weight_mode, 0);        // snr2_normalized
    EXPECT_EQ(std::string(m.input_manifest_hash), "deadbeef");
    // 集成路由字段：cpu 路由 + weight_mode 保持
    EXPECT_EQ(cfg.acr_route, "cpu");
    EXPECT_EQ(cfg.weight_mode, 2);          // 默认 auto(ivar)
}

TEST(Phase2Routing, WorkerBudgetPropagates) {
    P2Stage2Config cfg;
    std::string err;
    auto j = production_config();
    j["execution"] = {{"cpu_workers",4},{"io_workers",2},{"gpu_route","cpu"},{"deterministic",false},{"memory_budget_bytes",4096}};
    ASSERT_TRUE(p2_stage2_parse_config(j, &cfg, &err)) << err;
    EXPECT_EQ(effective_cpu_workers(cfg.exec), 4);
    EXPECT_EQ(effective_io_workers(cfg.exec), 2);
    EXPECT_EQ(cfg.exec.gpu_route, "cpu");
    EXPECT_FALSE(cfg.exec.deterministic);
    // CLI 等价覆盖：设置后生效值随之变化
    cfg.exec.cpu_workers = 6;
    EXPECT_EQ(effective_cpu_workers(cfg.exec), 6);
}

TEST(Phase2Routing, AcrCpuRouteStaysCpuNoSilentGpu) {
    P2Stage2Config cfg;
    std::string err;
    ASSERT_TRUE(p2_stage2_parse_config(production_config(), &cfg, &err)) << err;
    // CON-007: 生产默认 weight_mode=ivar(2)，ACR kernel 与逐像素 ivar 不等价，
    // 因此即使 acr_route=cpu 也必须走 CPU canonical path，不得进入 ACR 块。
    const bool use_acr_block =
        p2_acr_block_eligible(cfg, true, P2_REJECT_SIGMA, false);
    EXPECT_FALSE(use_acr_block);
}

TEST(Phase2Routing, AcrCpuRouteEntersCpuAcrBlockForLegacyWeightMode) {
    P2Stage2Config cfg;
    std::string err;
    ASSERT_TRUE(p2_stage2_parse_config(production_config(), &cfg, &err)) << err;
    // 非 ivar 的显式 sigma 配置下，acr_route=cpu 必须允许进入 ACR 的 CPU launcher
    // （CON-007 修复点：此前 route=cpu 被直接绕到 legacy 串行参考路径）。
    cfg.weight_mode = 0;   // support×snr² legacy/ablation
    EXPECT_TRUE(p2_acr_block_eligible(cfg, true, P2_REJECT_SIGMA, false));
    // auto 在 Linux 无 CUDA 时同样落回同一 CPU ACR launcher（fallback_reason 由运行日志记录）。
    cfg.acr_route = "auto";
    EXPECT_TRUE(p2_acr_block_eligible(cfg, true, P2_REJECT_SIGMA, false));
    // weight_mode=ivar 仍然禁止 ACR 块。
    cfg.weight_mode = 2;
    EXPECT_FALSE(p2_acr_block_eligible(cfg, true, P2_REJECT_SIGMA, false));
    // large_scale 两遍仍保持 CPU canonical。
    cfg.weight_mode = 0;
    EXPECT_FALSE(p2_acr_block_eligible(cfg, true, P2_REJECT_SIGMA, true));
}
