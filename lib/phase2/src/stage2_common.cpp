// lib/phase2/src/stage2_common.cpp — Stage2 生产共享函数（R1/V4）
#include "astro/phase2/stage2_common.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <string>

bool p2_stage2_parse_config(const nlohmann::json& j, P2Stage2Config* cfg, std::string* err) {
    try {
        if (!j.contains("inputs") || !j["inputs"].contains("hips")) {
            *err = "missing inputs.hips";
            return false;
        }
        for (const auto& p : j["inputs"]["hips"])
            cfg->hips.push_back(p.get<std::string>());
        if (cfg->hips.size() < 2) {
            *err = "inputs.hips must have >= 2 entries";
            return false;
        }
        if (j["inputs"].contains("target_order")) {
            const auto& to = j["inputs"]["target_order"];
            if (to.is_string()) {
                cfg->target_order_spec = to.get<std::string>();
                if (cfg->target_order_spec != "auto") {
                    *err = "target_order 只支持 'auto' 或整数";
                    return false;
                }
            } else if (to.is_number_integer()) {
                cfg->target_order = to.get<int>();
                if (cfg->target_order < 0 || cfg->target_order > 29) {
                    *err = "target_order 必须在 0..29";
                    return false;
                }
            } else {
                *err = "target_order 类型错误（'auto' 或整数）";
                return false;
            }
        }
        if (j.contains("model")) {
            const auto& m = j["model"];
            cfg->control_grid_per_tile = m.value("control_grid_per_tile", 8);
            if (cfg->control_grid_per_tile < 1 ||
                cfg->control_grid_per_tile > 64) {
                *err = "control_grid_per_tile 必须在 1..64";
                return false;
            }
            // patch_radius_pixels: auto → 2
            if (m.contains("patch_radius_pixels")) {
                const auto& pr = m["patch_radius_pixels"];
                if (pr.is_string()) {
                    if (pr.get<std::string>() != "auto") {
                        *err = "patch_radius_pixels 只支持 'auto' 或整数";
                        return false;
                    }
                    cfg->patch_radius_leaf = 2;
                } else if (pr.is_number_integer()) {
                    cfg->patch_radius_leaf = pr.get<int>();
                    if (cfg->patch_radius_leaf < 0 ||
                        cfg->patch_radius_leaf > 64) {
                        *err = "patch_radius_pixels 必须在 0..64";
                        return false;
                    }
                } else {
                    *err = "patch_radius_pixels 类型错误";
                    return false;
                }
            }
            cfg->min_samples = m.value("min_samples", 5);
            if (cfg->min_samples < 1) {
                *err = "min_samples 必须 >= 1";
                return false;
            }
            cfg->snr_search_radius_deg =
                m.value("snr_search_radius_deg", 0.05);
            if (cfg->snr_search_radius_deg <= 0.0) {
                *err = "snr_search_radius_deg 必须 > 0";
                return false;
            }
            cfg->huber_delta = m.value("huber_delta", 1.345);
            if (cfg->huber_delta <= 0.0) {
                *err = "huber_delta 必须 > 0";
                return false;
            }
            // smoothing: auto → 0.1（R1 空间平滑默认）
            if (m.contains("smoothing")) {
                const auto& sm = m["smoothing"];
                if (sm.is_string()) {
                    if (sm.get<std::string>() != "auto") {
                        *err = "smoothing 只支持 'auto' 或 number";
                        return false;
                    }
                    cfg->smoothing_lambda = 0.1;
                } else if (sm.is_number()) {
                    cfg->smoothing_lambda = sm.get<double>();
                    if (cfg->smoothing_lambda < 0.0) {
                        *err = "smoothing 必须 >= 0";
                        return false;
                    }
                } else {
                    *err = "smoothing 类型错误";
                    return false;
                }
            }
            cfg->zero_anchor_weight =
                m.value("zero_anchor_weight", 1e-3);
            cfg->max_irls_iterations =
                m.value("max_irls_iterations", 100);
            cfg->tolerance = m.value("tolerance", 1e-6);
            const std::string rl =
                m.value("robust_loss", std::string("huber"));
            if (rl != "huber") {
                *err = "unsupported robust_loss: " + rl;
                return false;
            }
            const std::string sw = m.value(
                "snr_weight_mode", std::string("snr2_normalized"));
            if (sw != "snr2_normalized") {
                *err = "unsupported snr_weight_mode: " + sw;
                return false;
            }
            cfg->sigma_floor = m.value("sigma_floor", 1e-3);
            if (cfg->sigma_floor <= 0.0) {
                *err = "sigma_floor 必须 > 0";
                return false;
            }
            cfg->support_power = m.value("support_power", 1.0);
            if (cfg->support_power < 0.0) {
                *err = "support_power 必须 >= 0";
                return false;
            }
        }
        if (j.contains("integration")) {
            const auto& in = j["integration"];
            const std::string prec =
                in.value("precision", std::string("fp32"));
            if (prec != "fp32" && prec != "fp64") {
                *err = "precision 只支持 fp32/fp64";
                return false;
            }
            cfg->precision = (prec == "fp64") ? 1 : 0;
            cfg->memory_limit_mb =
                in.value("memory_limit_mb", (std::uint64_t)24576);
            if (cfg->memory_limit_mb < 1) {
                *err = "memory_limit_mb 必须 >= 1";
                return false;
            }
            if (in.contains("rejection")) {
                const auto& rj = in["rejection"];
                const std::string method =
                    rj.value("method", std::string("winsorized_sigma"));
                if (method == "none") cfg->reject_method = P2_REJECT_NONE;
                else if (method == "sigma") cfg->reject_method = P2_REJECT_SIGMA;
                else if (method == "winsorized_sigma")
                    cfg->reject_method = P2_REJECT_WINSORIZED_SIGMA;
                else if (method == "averaged_sigma")
                    cfg->reject_method = P2_REJECT_AVERAGED_SIGMA;
                else if (method == "linear_fit")
                    cfg->reject_method = P2_REJECT_LINEAR_FIT;
                else if (method == "generalized_esd")
                    cfg->reject_method = P2_REJECT_GENERALIZED_ESD;
                else if (method == "rcr") cfg->reject_method = P2_REJECT_RCR;
                else {
                    *err = "unsupported rejection method: " + method;
                    return false;
                }
                cfg->sigma_low = -std::fabs(rj.value("low", 4.0));
                cfg->sigma_high = std::fabs(rj.value("high", 3.0));
                cfg->reject_max_iterations =
                    rj.value("max_iterations", 8);
                cfg->reject_min_samples = rj.value("min_samples", 2);
                if (cfg->reject_min_samples < 1) {
                    *err = "rejection.min_samples 必须 >= 1";
                    return false;
                }
            }
        const std::string wm =
            in.value("weight_mode", std::string("auto"));
            if (wm == "auto" || wm == "support_x_snr2") {
                cfg->weight_mode = 0;   // R5 冻结默认 support_x_snr2
            } else if (wm == "equal") {
                cfg->weight_mode = 1;
            } else {
                *err = "weight_mode 只支持 auto/equal/support_x_snr2";
                return false;
            }
            cfg->acr_route = in.value("acr_route", std::string("auto"));
            if (cfg->acr_route != "auto" && cfg->acr_route != "cpu") {
                *err = "acr_route 只支持 auto/cpu";
                return false;
            }
        }
        if (!j.contains("output") || !j["output"].contains("hips")) {
            *err = "missing output.hips";
            return false;
        }
        cfg->out_hips = j["output"]["hips"].get<std::string>();
        if (j.contains("diagnostics"))
            cfg->diagnostics = j["diagnostics"].value("enabled", true);
    } catch (const std::exception& e) {
        *err = std::string("config parse 失败: ") + e.what();
        return false;
    }
    return true;
}

P2UpmBuildConfig p2_stage2_make_upm_cfg(const P2Stage2Config& cfg,
                                        int target_order,
                                        const char* input_manifest_hash) {
    P2UpmBuildConfig mcfg{};
    mcfg.robust_loss = cfg.robust_loss;
    mcfg.snr_weight_mode = cfg.snr_weight_mode;
    mcfg.huber_delta = cfg.huber_delta;
    mcfg.smoothing_lambda = cfg.smoothing_lambda;
    mcfg.zero_anchor_weight = cfg.zero_anchor_weight;
    mcfg.sigma_floor = cfg.sigma_floor;
    mcfg.support_power = cfg.support_power;
    mcfg.quality_mode = 0;
    mcfg.control_reliability = 1.0;
    mcfg.input_manifest_hash = input_manifest_hash;
    mcfg.max_iterations = cfg.max_irls_iterations;
    mcfg.tolerance = cfg.tolerance;
    mcfg.target_order = target_order;
    return mcfg;
}
