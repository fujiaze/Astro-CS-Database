// lib/phase2/tests/execution_options_test.cpp — CON-002 global worker budget contract tests
#include "astro/phase2/execution_options.h"
#include "astro/phase2/stage2_common.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <thread>

using namespace astro::phase2;

static int hw() {
    const unsigned h = std::thread::hardware_concurrency();
    return h > 0 ? static_cast<int>(h) : 1;
}

static nlohmann::json minimal() {
    return nlohmann::json::parse(R"({
      "version":1,
      "inputs":{"hips":["a","b"]},
      "model":{},
      "integration":{},
      "output":{"hips":"out"}
    })");
}

TEST(ExecutionOptions, DefaultIsHardwareConcurrency) {
    ExecutionOptions e = default_execution_options();
    EXPECT_EQ(e.cpu_workers, hw());
    EXPECT_EQ(e.cpu_workers, std::max(1, hw()));
    EXPECT_GE(e.io_workers, 1);
    EXPECT_EQ(e.gpu_route, "auto");
    EXPECT_TRUE(e.deterministic);
    EXPECT_EQ(e.memory_budget_bytes, 0u);
    EXPECT_EQ(effective_cpu_workers(e), hw());
}

TEST(ExecutionOptions, ParseAppliesOverrides) {
    P2Stage2Config cfg;
    auto j = minimal();
    j["execution"] = {{"cpu_workers", 4}, {"io_workers", 2}, {"gpu_route", "cpu"}, {"deterministic", false}, {"memory_budget_bytes", 65536}};
    std::string err;
    ASSERT_TRUE(p2_stage2_parse_config(j, &cfg, &err)) << err;
    EXPECT_EQ(cfg.exec.cpu_workers, 4);
    EXPECT_EQ(cfg.exec.io_workers, 2);
    EXPECT_EQ(cfg.exec.gpu_route, "cpu");
    EXPECT_FALSE(cfg.exec.deterministic);
    EXPECT_EQ(cfg.exec.memory_budget_bytes, 65536u);
}

TEST(ExecutionOptions, ParseWithoutExecutionBlockDefaults) {
    P2Stage2Config cfg;
    std::string err;
    ASSERT_TRUE(p2_stage2_parse_config(minimal(), &cfg, &err)) << err;
    EXPECT_EQ(cfg.exec.cpu_workers, hw());
    EXPECT_EQ(cfg.exec.gpu_route, "auto");
    EXPECT_TRUE(cfg.exec.deterministic);
}

TEST(ExecutionOptions, ParseRejectsInvalidCpuWorkers) {
    P2Stage2Config cfg;
    auto j = minimal();
    j["execution"] = {{"cpu_workers", 5000}};
    std::string err;
    EXPECT_FALSE(p2_stage2_parse_config(j, &cfg, &err));
    EXPECT_NE(err.find("cpu_workers"), std::string::npos);
}

TEST(ExecutionOptions, ParseRejectsInvalidGpuRoute) {
    P2Stage2Config cfg;
    auto j = minimal();
    j["execution"] = {{"gpu_route", "cuda-x"}};
    std::string err;
    EXPECT_FALSE(p2_stage2_parse_config(j, &cfg, &err));
    EXPECT_NE(err.find("gpu_route"), std::string::npos);
}

TEST(ExecutionOptions, EffectiveCounters) {
    ExecutionOptions e;
    e.cpu_workers = 6;   // explicit
    EXPECT_EQ(effective_cpu_workers(e), 6);
    EXPECT_EQ(effective_io_workers(e), 3);
    e.io_workers = 0;    // auto
    EXPECT_EQ(effective_io_workers(e), 3);
    e.cpu_workers = 0;   // auto
    EXPECT_EQ(effective_cpu_workers(e), hw());
    EXPECT_EQ(effective_io_workers(e), hw() > 1 ? hw() / 2 : 1);
}
