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
    // V13 background-clean sampler
    int    background_patch_radius = 8;
    double background_clip_sigma = 3.0;
    int    background_clip_iters = 3;
    double background_max_contamination = 0.20;
    double background_contamination_sigma = 3.0;
    double background_min_retained_fraction = 0.60;
    double background_tolerance = 3.0;
    int    background_neighbor_radius = 2;
    int    background_catalog_veto = 1;
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
    int reject_method = P2_REJECT_AUTO;  // V15: production default = auto
    std::string reject_profile = "wbpp_current";  // 唯一支持（WBPP 2.9.1）
    std::uint32_t reject_underdetermined_n = 2;   // n<=2 → UNDERDETERMINED
    // V16：RejectionNormalizationPolicy（判定工作域；mask 应用回原始值）
    std::string reject_normalization = "median_center";
    double reject_normalization_floor = 1e-12;
    // V15 method-specific typed parameters（单语义单默认）
    double sigma_lower = 4.0;
    double sigma_upper = 3.0;
    int sigma_max_iterations = 8;
    double winsor_lower = 4.0;
    double winsor_upper = 3.0;
    int winsor_max_iterations = 8;
    double avg_lower = 4.0;
    double avg_upper = 3.0;
    int avg_max_iterations = 8;
    double linfit_lower = 5.0;   // WBPP Light linearFitLow
    double linfit_upper = 3.5;   // WBPP Light linearFitHigh
    int linfit_max_iterations = 8;
    double esd_alpha = 0.05;
    int esd_max_outliers = 10;
    double pct_low_fraction = 0.2;   // WBPP Light percentileLow
    double pct_high_fraction = 0.1;  // WBPP Light percentileHigh
    double medsig_lower = 4.0;
    double medsig_upper = 3.0;
    int medsig_max_iterations = 8;
    int minmax_low_count = 1;
    int minmax_high_count = 1;
    int minmax_min_kept = 4;
    std::string rcr_technique = "ss_median_dl";
    // legacy 别名（deprecated；parser 打印 deprecation warning）
    double sigma_low = -4.0;
    double sigma_high = 3.0;
    int reject_max_iterations = 8;
    int reject_min_samples = 2;
    std::vector<std::string> deprecation_warnings;
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
