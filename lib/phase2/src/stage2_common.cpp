// lib/phase2/src/stage2_common.cpp — Stage2 生产共享函数
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
            // background-clean sampler 参数
            cfg->background_patch_radius =
                m.value("background_patch_radius", 8);
            if (cfg->background_patch_radius < 3) {
                *err = "background_patch_radius 必须 >= 3";
                return false;
            }
            cfg->background_clip_sigma =
                m.value("background_clip_sigma", 3.0);
            if (cfg->background_clip_sigma <= 0.0) {
                *err = "background_clip_sigma 必须 > 0";
                return false;
            }
            cfg->background_clip_iters =
                m.value("background_clip_iters", 3);
            if (cfg->background_clip_iters < 1) {
                *err = "background_clip_iters 必须 >= 1";
                return false;
            }
            cfg->background_max_contamination =
                m.value("background_max_contamination", 0.20);
            if (cfg->background_max_contamination <= 0.0 ||
                cfg->background_max_contamination >= 1.0) {
                *err = "background_max_contamination 必须在 (0,1)";
                return false;
            }
            cfg->background_contamination_sigma =
                m.value("background_contamination_sigma", 3.0);
            cfg->background_min_retained_fraction =
                m.value("background_min_retained_fraction", 0.60);
            if (cfg->background_min_retained_fraction <= 0.0 ||
                cfg->background_min_retained_fraction > 1.0) {
                *err = "background_min_retained_fraction 必须在 (0,1]";
                return false;
            }
            cfg->background_tolerance =
                m.value("background_tolerance", 3.0);
            if (cfg->background_tolerance <= 0.0) {
                *err = "background_tolerance 必须 > 0";
                return false;
            }
            cfg->background_neighbor_radius =
                m.value("background_neighbor_radius", 2);
            if (cfg->background_neighbor_radius < 1) {
                *err = "background_neighbor_radius 必须 >= 1";
                return false;
            }
            cfg->background_catalog_veto =
                m.value("background_catalog_veto", 1);
            cfg->huber_delta = m.value("huber_delta", 1.345);
            if (cfg->huber_delta <= 0.0) {
                *err = "huber_delta 必须 > 0";
                return false;
            }
            // smoothing: auto → 0.1（ 空间平滑默认）
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
            cfg->use_ivar_weight =
                m.value("use_ivar_weight", 1) != 0 ? 1 : 0;
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
                // 旧 config 别名（low/high/max_iterations/min_samples）
                // 已从 production runtime 删除——出现即要求显式迁移。
                if (rj.contains("low") || rj.contains("high") ||
                    rj.contains("max_iterations") ||
                    rj.contains("min_samples")) {
                    *err = "rejection.low/high/max_iterations/min_samples 已"
                           "删除（V17）。请用 tools/migrate_stage2_config.py "
                           "迁移到 typed params（rejection.<method>.* 与 "
                           "underdetermined_n）。";
                    return false;
                }
                const std::string method =
                    rj.value("method", std::string("auto"));
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
                else if (method == "percentile")
                    cfg->reject_method = P2_REJECT_PERCENTILE;
                else if (method == "median_sigma")
                    cfg->reject_method = P2_REJECT_MEDIAN_SIGMA;
                else if (method == "minmax")
                    cfg->reject_method = P2_REJECT_MINMAX;
                else if (method == "auto")
                    cfg->reject_method = P2_REJECT_AUTO;
                else {
                    *err = "unsupported rejection method: " + method;
                    return false;
                }
                // 版本化 profile（wbpp_2_9_1 冻结；wbpp_current 仅
                // migration alias，解析到 wbpp_2_9_1；astrocs_adaptive 独立）
                cfg->reject_profile =
                    rj.value("profile", std::string("wbpp_2_9_1"));
                if (cfg->reject_profile == "wbpp_current")
                    cfg->reject_profile = "wbpp_2_9_1";   // alias 规范化
                if (cfg->reject_profile != "wbpp_2_9_1" &&
                    cfg->reject_profile != "astrocs_adaptive") {
                    *err = "rejection.profile 只支持 wbpp_2_9_1（冻结）/ "
                           "astrocs_adaptive";
                    return false;
                }
                cfg->reject_underdetermined_n =
                    rj.value("underdetermined_n", (std::uint32_t)2);
                if (cfg->reject_underdetermined_n < 1) {
                    *err = "rejection.underdetermined_n 必须 >= 1";
                    return false;
                }
                // rejection normalization 独立命名（astrocs_*_v1；
                // 旧 median_center/median_scale 为 migration alias）
                cfg->reject_normalization = rj.value(
                    "normalization",
                    std::string("astrocs_median_center_v1"));
                if (cfg->reject_normalization == "median_center")
                    cfg->reject_normalization = "astrocs_median_center_v1";
                else if (cfg->reject_normalization == "median_scale")
                    cfg->reject_normalization = "astrocs_median_scale_v1";
                if (cfg->reject_normalization != "none" &&
                    cfg->reject_normalization != "astrocs_median_center_v1" &&
                    cfg->reject_normalization != "astrocs_median_scale_v1") {
                    *err = "rejection.normalization 只支持 none/"
                           "astrocs_median_center_v1/astrocs_median_scale_v1";
                    return false;
                }
                cfg->reject_normalization_floor =
                    rj.value("normalization_floor", 1e-12);
                // large_scale_rejection.v1（connected-component grow）
                if (rj.contains("large_scale")) {
                    const auto& ls = rj["large_scale"];
                    cfg->large_scale_enabled =
                        ls.value("enabled", false);
                    cfg->large_scale_min_structure_pixels =
                        ls.value("min_structure_pixels", 8);
                    if (cfg->large_scale_min_structure_pixels < 1) {
                        *err = "rejection.large_scale.min_structure_pixels"
                               " 必须 >= 1";
                        return false;
                    }
                    cfg->large_scale_low_grow_pixels =
                        ls.value("low_grow_radius_pixels", 2);
                    cfg->large_scale_high_grow_pixels =
                        ls.value("high_grow_radius_pixels", 2);
                    if (cfg->large_scale_low_grow_pixels < 0 ||
                        cfg->large_scale_high_grow_pixels < 0) {
                        *err = "rejection.large_scale.grow_radius_pixels"
                               " 必须 >= 0";
                        return false;
                    }
                }
                // 方法×normalization 合法性
                if (method == "percentile" &&
                    cfg->reject_normalization != "astrocs_median_center_v1") {
                    *err = "rejection: percentile 必须 normalization="
                           "astrocs_median_center_v1（负值科学域安全）";
                    return false;
                }
                if (method == "rcr" && cfg->reject_normalization != "none") {
                    *err = "rejection: rcr 必须 normalization=none"
                           "（官方 oracle 原始值域冻结）";
                    return false;
                }
                // method-specific typed params（单语义单默认）
                if (rj.contains("robust_mad_clip")) {
                    const auto& s = rj["robust_mad_clip"];
                    cfg->sigma_lower = s.value("lower_sigma", 4.0);
                    cfg->sigma_upper = s.value("upper_sigma", 3.0);
                    cfg->sigma_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("winsorized_sigma")) {
                    const auto& s = rj["winsorized_sigma"];
                    cfg->winsor_lower = s.value("lower_sigma", 4.0);
                    cfg->winsor_upper = s.value("upper_sigma", 3.0);
                    cfg->winsor_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("averaged_sigma")) {
                    const auto& s = rj["averaged_sigma"];
                    cfg->avg_lower = s.value("lower_sigma", 4.0);
                    cfg->avg_upper = s.value("upper_sigma", 3.0);
                    cfg->avg_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("linear_fit")) {
                    const auto& s = rj["linear_fit"];
                    cfg->linfit_lower = s.value("lower", 5.0);   // WBPP Light
                    cfg->linfit_upper = s.value("upper", 3.5);
                    cfg->linfit_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("generalized_esd")) {
                    const auto& s = rj["generalized_esd"];
                    cfg->esd_alpha = s.value("alpha", 0.05);
                    cfg->esd_max_outliers = s.value("max_outliers", 10);
                }
                if (rj.contains("percentile")) {
                    const auto& s = rj["percentile"];
                    cfg->pct_low_fraction =
                        s.value("low_fraction", 0.2);  // WBPP Light
                    cfg->pct_high_fraction =
                        s.value("high_fraction", 0.1);
                }
                if (rj.contains("median_sigma")) {
                    const auto& s = rj["median_sigma"];
                    cfg->medsig_lower = s.value("lower_sigma", 4.0);
                    cfg->medsig_upper = s.value("upper_sigma", 3.0);
                    cfg->medsig_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("minmax")) {
                    const auto& s = rj["minmax"];
                    cfg->minmax_low_count =
                        s.value("reject_low_count", 1);
                    cfg->minmax_high_count =
                        s.value("reject_high_count", 1);
                    cfg->minmax_min_kept = s.value("min_kept", 4);
                }
                if (rj.contains("rcr")) {
                    const auto& s = rj["rcr"];
                    cfg->rcr_technique =
                        s.value("technique", std::string("ss_median_dl"));
                    if (cfg->rcr_technique != "ss_median_dl") {
                        *err = "rcr.technique 只支持 ss_median_dl（V15 冻结）";
                        return false;
                    }
                }
            }
        const std::string wm =
            in.value("weight_mode", std::string("auto"));
        // 默认 ivar (逆方差)
        // w = 1/variance_p (Drizzle 传播); support 只作 validity/coverage。
        // support_x_snr2 保留为 legacy/diagnostic (SNR-008 语义退休)。
        if (wm == "auto" || wm == "ivar") {
            cfg->weight_mode = 2;   // 默认 ivar
        } else if (wm == "equal") {
            cfg->weight_mode = 1;
        } else if (wm == "support_x_snr2") {
            cfg->weight_mode = 0;   // legacy (仅 ablation/诊断)
        } else {
            *err = "weight_mode 只支持 auto/ivar/equal/support_x_snr2";
            return false;
        }
        // ivar 产品整体缺失时，
        // 默认 → 显式 science/degraded 错误；仅显式允许时才降级。
        cfg->legacy_allow_weight_fallback =
            in.value("legacy_allow_weight_fallback", false);
            cfg->acr_route = in.value("acr_route", std::string("auto")); // B4-28 ACR边界 ACR-IVAR-001: weight_mode=ivar时 ACR块禁用→CPU canonical (TRACEABILITY ACR-IVAR-001)
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
        // CON-002 global worker budget contract (execution block)
        if (j.contains("execution")) {
            const auto& ex = j["execution"];
            if (ex.contains("cpu_workers")) {
                const int cw = ex.value("cpu_workers", 0);
                if (cw < 0 || cw > 1024) { *err = "cpu_workers 必须在 0..1024 (0=auto)"; return false; }
                cfg->exec.cpu_workers = cw;
            }
            if (ex.contains("io_workers")) {
                const int iw = ex.value("io_workers", 0);
                if (iw < 0 || iw > 1024) { *err = "io_workers 必须在 0..1024 (0=auto)"; return false; }
                cfg->exec.io_workers = iw;
            }
            if (ex.contains("gpu_route")) {
                const std::string gr = ex.value("gpu_route", std::string("auto"));
                if (gr != "cpu" && gr != "auto" && gr != "cuda") { *err = "gpu_route 只支持 cpu/auto/cuda"; return false; }
                cfg->exec.gpu_route = gr;
            }
            if (ex.contains("deterministic"))
                cfg->exec.deterministic = ex.value("deterministic", true);
            if (ex.contains("memory_budget_bytes"))
                cfg->exec.memory_budget_bytes = ex.value("memory_budget_bytes", (std::uint64_t)0);
        }
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
    mcfg.use_ivar_weight = cfg.use_ivar_weight;   // 显式透传
    mcfg.quality_mode = 0;
    mcfg.control_reliability = 1.0;
    mcfg.input_manifest_hash = input_manifest_hash;
    mcfg.max_iterations = cfg.max_irls_iterations;
    mcfg.tolerance = cfg.tolerance;
    mcfg.target_order = target_order;
    mcfg.cpu_workers = cfg.exec.cpu_workers;   // CON-005: UPM build 并行 worker 预算(CON-002 唯一来源)
    return mcfg;
}
