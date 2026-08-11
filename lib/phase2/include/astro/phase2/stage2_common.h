// lib/phase2/include/astro/phase2/stage2_common.h — Stage2 生产共享函数
//
// R1（V4）：G2 production wiring gate 要求同一生产 parse+build path。
// astrocs-stage2 与 gate 测试共用此处的配置解析与 UPM 配置构造。
#pragma once

#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <vector>

struct P2Stage2Config {
    std::vector<std::string> hips;
    std::string target_order_spec = "auto";
    int target_order = -1;
    // model
    int control_grid_per_tile = 8;
    int patch_radius_leaf = 2;
    int min_samples = 5;
    double snr_search_radius_deg = 0.05;
    int robust_loss = 0;
    int snr_weight_mode = 0;
    double huber_delta = 1.345;
    double smoothing_lambda = 0.0;
    double zero_anchor_weight = 1e-3;
    double sigma_floor = 1e-3;
    double support_power = 1.0;
    int max_irls_iterations = 100;
    double tolerance = 1e-6;
    // integration
    int precision = 0;
    std::uint64_t memory_limit_mb = 24576;
    int reject_method = 2;          // P2_REJECT_WINSORIZED_SIGMA
    double sigma_low = -4.0;
    double sigma_high = 3.0;
    int reject_max_iterations = 8;
    int reject_min_samples = 2;
    int weight_mode = 0;            // support_x_snr2（auto）
    std::string acr_route = "auto";
    // output
    std::string out_hips;
    bool diagnostics = true;
};

// 解析生产 Stage2 JSON（异常安全；非法输入返回 false + 清晰 err）
bool p2_stage2_parse_config(const nlohmann::json& j, P2Stage2Config* cfg,
                            std::string* err);

// 由 P2Stage2Config 构造生产 UPM 构建配置（显式传递全部科学字段）
P2UpmBuildConfig p2_stage2_make_upm_cfg(const P2Stage2Config& cfg,
                                        int target_order,
                                        const char* input_manifest_hash);
