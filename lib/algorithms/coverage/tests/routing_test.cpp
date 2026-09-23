// lib/algorithms/coverage/tests/routing_test.cpp — CON-003 production CLI->module routing test
// 验证 stage2 CLI 与 gate 测试共享同一生产 parse+build 路径（非 mock），
// 且 worker 预算(CON-002)与 ACR cpu 路由在到达模块前保持正确。
#include "astro/phase2/stage2_common.h"
#include "astro/phase2/execution_options.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace astro::phase2;

// 生产风格配置：与 lib/algorithms/coverage/configs/stage2_real_3frame.json 同结构。
static nlohmann::json production_config() {
    return nlohmann::json::parse(R"({
      "version":1,
      "inputs":{"hips":["a","b","c"]},
      "model":{"control_grid_per_tile":8,"robust_loss":"huber","snr_weight_mode":"snr2_normalized"},
      "integration":{"precision":"fp32","memory_limit_mb":8192,
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
    // 集成路由字段：cpu 路由保持；§9.73 裁决 A44 后不存在「权重模式」字段。
    EXPECT_EQ(cfg.acr_route, "cpu");
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

// ── §9.73 裁决 A44 后的 ACR 资格（收紧，不是放宽）──────────────────────────
// 删除 legacy 整数权重模式域后，生产**只剩**一条权重口径（逐样本逆方差）。
// TRACEABILITY ACR-IVAR-001 冻结「ivar science 模式必须走 CPU canonical path」，
// 该条对本仓恒成立 ⇒ 不存在任何合法配置能进入 ACR 块。
// 能红能绿：若把 p2_acr_block_eligible 改回「条件成立即 true」，本测试转红。
TEST(Phase2Routing, AcrBlockNeverEligibleUnderSingleWeightPath) {
    P2Stage2Config cfg;
    std::string err;
    ASSERT_TRUE(p2_stage2_parse_config(production_config(), &cfg, &err)) << err;
    // 生产默认路由（acr_route=cpu）：仍不得进入 ACR 块。
    EXPECT_FALSE(p2_acr_block_eligible(cfg, true, P2_REJECT_SIGMA, false));
    // 穷举路由与 reject method 的组合：没有任何组合能进 ACR 块。
    for (const char* route : {"cpu", "auto", "cuda"}) {
        for (const int method : {P2_REJECT_SIGMA, P2_REJECT_AUTO}) {
            for (const bool large : {false, true}) {
                cfg.acr_route = route;
                EXPECT_FALSE(p2_acr_block_eligible(cfg, true, method, large))
                    << "route=" << route << " method=" << method
                    << " large_scale=" << large;
            }
        }
    }
    // ACR 未注册时同样不可进入（对照组，证明判据非退化）。
    cfg.acr_route = "cpu";
    EXPECT_FALSE(p2_acr_block_eligible(cfg, false, P2_REJECT_SIGMA, false));
}

// ── 负例（能红）：legacy 整数权重模式域的任何形态都必须被拒绝 ────────────────
// 权威：ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、没有可选择项」；
// docs/science/PSF_SIGNAL_WEIGHT.md §4:72「没有可选择的口径：不存在口径选择键、
// 口径枚举、口径配置项或口径产物」。
TEST(Phase2Routing, LegacyWeightModeDomainRejected) {
    // 字符串 token（原 auto/ivar/equal/support_x_snr2 四值域）。
    for (const char* tok : {"auto", "ivar", "equal", "support_x_snr2", "snr2", ""}) {
        nlohmann::json j = production_config();
        j["integration"]["weight_mode"] = tok;
        P2Stage2Config cfg;
        std::string err;
        EXPECT_FALSE(p2_stage2_parse_config(j, &cfg, &err))
            << "token must be rejected: " << tok;
        EXPECT_NE(err.find("§9.73"), std::string::npos) << err;
    }
    // 整数形态（原 0/1/2 域，含 legacy support×snr² 与 equal）。
    for (const int v : {0, 1, 2, 3}) {
        nlohmann::json j = production_config();
        j["integration"]["weight_mode"] = v;
        P2Stage2Config cfg;
        std::string err;
        EXPECT_FALSE(p2_stage2_parse_config(j, &cfg, &err))
            << "integer must be rejected: " << v;
    }
    // 对照（非退化）：无该键时生产配置必须解析成功。
    P2Stage2Config cfg;
    std::string err;
    EXPECT_TRUE(p2_stage2_parse_config(production_config(), &cfg, &err)) << err;
}
