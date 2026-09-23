// lib/algorithms/coverage/src/stage2_common.cpp — Stage2 生产共享函数
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
                    cfg->smoothing_lambda = P2_SMOOTHING_LAMBDA_AUTO;
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
        // FIX-A 稀疏天光面（可选；缺省启用，保持设计"标准行为"）。
        if (j.contains("sky_plane")) {
            const auto& sp = j["sky_plane"];
            if (!sp.is_object()) { *err = "sky_plane 必须是对象"; return false; }
            cfg->sky_plane_enabled = sp.value("enabled", cfg->sky_plane_enabled);
            cfg->sky_plane_spline_degree = sp.value("spline_degree", cfg->sky_plane_spline_degree);
            cfg->sky_plane_node_spacing_deg =
                sp.value("node_spacing_deg", cfg->sky_plane_node_spacing_deg);
            cfg->sky_plane_gradient_order =
                sp.value("frame_gradient_order", cfg->sky_plane_gradient_order);
            cfg->sky_plane_gauge_mode = sp.value("gauge_mode", cfg->sky_plane_gauge_mode);
            cfg->sky_plane_weight_mode = sp.value("weight_mode", cfg->sky_plane_weight_mode);
            cfg->sky_plane_roughness_penalty =
                sp.value("roughness_penalty", cfg->sky_plane_roughness_penalty);
            cfg->frame_gain_enabled = sp.value("frame_gain", cfg->frame_gain_enabled);
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
                           "删除（V17）。请用 eng/tools/migrate_stage2_config.py "
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
                // FIX-REJ n=2 档: 已知先验 σ 的极值检验（显式方法）
                else if (method == "extreme_value_clip_prior_sigma")
                    cfg->reject_method = P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA;
                else if (method == "auto")
                    cfg->reject_method = P2_REJECT_AUTO;
                else {
                    *err = "unsupported rejection method: " + method;
                    return false;
                }
                // 版本化 profile。CONFORM-FIX-B-007：生产默认 = 自研档
                // astrocs_adaptive_pixel（负责人裁决 FIX-SCI-SNR-CANON-001 /
                // GAP_AUDIT §9.40 C2；SCI REJECTION §4/§5/§7、CONFIG_SCHEMA.md:25、
                // contracts provenance:67 同值）。wbpp_2_9_1 为**对照档**、
                // wbpp_current 为 migration alias（规范化到 wbpp_2_9_1）。
                // 与 node chain（module_adapters.cpp p2_op_reject 缺省）一致。
                cfg->reject_profile = rj.value(
                    "profile",
                    std::string(P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL));
                if (cfg->reject_profile == "wbpp_current")
                    cfg->reject_profile = "wbpp_2_9_1";   // alias 规范化
                if (cfg->reject_profile != "wbpp_2_9_1" &&
                    cfg->reject_profile != "astrocs_adaptive" &&
                    cfg->reject_profile != "astrocs_adaptive_pixel") {
                    *err = "rejection.profile 只支持 wbpp_2_9_1（冻结）/ "
                           "astrocs_adaptive / astrocs_adaptive_pixel";
                    return false;
                }
                // CONFORM-FIX-B-008：underdetermined_n 默认值**单一来源** =
                // p2_reject_plan_resolve 的 profile/request 规则（rejection.h:230-234
                // 冻结：0 = 按 profile 默认；wbpp/adaptive=2；
                // astrocs_adaptive_pixel=3（n<=3 保守 none）；显式
                // extreme_prior opt-in=1）。旧实现在此复制了 2/3 两份字面量，
                // 与 resolver 在 extreme_prior 档分叉（tool 恒 3 vs resolver 1）
                // ⇒ 同一配置两条链路得到不同 underdetermined_n。现缺键时向
                // 权威 resolver 查询，不再复制默认值。
                if (rj.contains("underdetermined_n")) {
                    cfg->reject_underdetermined_n =
                        rj.value("underdetermined_n", 0u);
                    if (cfg->reject_underdetermined_n < 1) {
                        *err = "rejection.underdetermined_n 必须 >= 1";
                        return false;
                    }
                } else {
                    P2RejectionPlanRequest dreq{};
                    dreq.request = cfg->reject_method;
                    dreq.nominal_contributors = (std::uint32_t)cfg->hips.size();
                    dreq.profile = cfg->reject_profile.c_str();
                    dreq.underdetermined_n = 0;   // 0 = profile/request 默认
                    P2RejectionPlan dplan{};
                    char derr[160] = {0};
                    if (p2_reject_plan_resolve(&dreq, &dplan, derr,
                                               sizeof(derr)) != 0) {
                        *err = std::string(
                                   "rejection underdetermined_n 默认解析失败: ") +
                               derr;
                        return false;
                    }
                    cfg->reject_underdetermined_n = dplan.underdetermined_n;
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
        // ── §9.73 裁决 A44：legacy 整数权重模式域与其 token 已删除 ──────────
        // 规范依据（权威，只读）：
        //   · ASTROCS_DESIGN.md §3.1:171「全程只有 SNR，没有"权重模式"这个概念」；
        //   · ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、**没有可选择项**」；
        //   · docs/science/PSF_SIGNAL_WEIGHT.md §4:62/72「单一权重口径（无模式选择）」
        //     「**没有可选择的口径**：不存在口径选择键、口径枚举、口径配置项或口径产物」；
        //   · docs/ci/01_CHECKS.md CHK-NO-WEIGHT-MODE-CODE（FZ-WEIGHT-SINGLE-PATH）。
        // 原实现把 integration.weight_mode ∈ {auto,ivar,equal,support_x_snr2} 映射为
        // 整数域 {2,2,1,0}：equal 直接开等权、support_x_snr2 开 support×snr²
        // （无量纲、非信号/噪声之比）——两者都与「没有可选择项」直接冲突。
        // ⇒ 该键**既不能被设、也不能被读**：出现即 fail-closed 拒绝（退役对象的
        //   拒绝面必须存活，不得静默忽略或静默取默认值）。
        if (in.contains("weight_mode")) {
            *err = "integration.weight_mode 已按 §9.73 裁决 A44 删除：不存在「权重模式」"
                   "（ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、没有可选择项」；"
                   "docs/science/PSF_SIGNAL_WEIGHT.md §4「单一权重口径（无模式选择）」）。"
                   "权重是阶段二按天球像素对应的输入帧集合现场算出的派生量 "
                   "w = SNR^2/F_ref^2 = 1/sigma_F^2；请删除该键。";
            return false;
        }
        // §9.73 裁决 A44（同批清理）：legacy_allow_weight_fallback **已删除** ——
        // 该键曾允许「ivar 缺失 → 降级 support/equal」，而 support/equal 都不是
        // 信号/噪声之比（ASTROCS_DESIGN.md §3.1:173/175）。
        // ⇒ 该键**既不能被设、也不能被读**：出现即 fail-closed 拒绝。
        if (in.contains("legacy_allow_weight_fallback")) {
            *err = "integration.legacy_allow_weight_fallback 已按 §9.73 裁决 A44 删除："
                   "它允许用无量纲 support 或等权降级冒充逆方差权重，与 "
                   "ASTROCS_DESIGN.md §3.1:173「权重只能来自纯净信号与噪声之比」及 "
                   "§3.1:175「没有可选择项」冲突。唯一降级面 = 帧级 SNR 逆方差链 "
                   "w = SNR^2/F_ref^2（由数据可用性自动决定，不是用户开关）；"
                   "权重链未闭合即显式 science 错误。请删除该键。";
            return false;
        }
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

bool p2_acr_block_eligible(const P2Stage2Config& cfg,
                           bool acr_registered,
                           int reject_method,
                           bool large_scale_active) {
    // §9.73 裁决 A44 删除 legacy 整数权重模式域后，生产**只剩**一条权重口径：
    // 逐样本逆方差（原 weight_mode=2 语义）。TRACEABILITY ACR-IVAR-001 冻结：
    // 「ivar science 模式必须走 CPU canonical path」；acr_kernels.cpp 亦对
    // cell-ivar 权重显式 throw（ACR 与逐像素 ivar 不等价）。
    // ⇒ 该条件对本仓**恒成立** ⇒ 本函数恒 false：ACR 块在生产不可达
    //   （ASTROCS_DESIGN.md §2「纯 CPU 生产，ACR 生产不可达」）。
    // 这不是"关掉一个开关"，而是唯一路径的推论：可进入 ACR 的前提（非 ivar
    // 权重口径）已被冻结口径删除，故不存在任何合法配置能进入 ACR 块。
    (void)cfg;
    (void)acr_registered;
    (void)reject_method;
    (void)large_scale_active;
    return false;
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
    mcfg.grid = cfg.control_grid_per_tile;     // M7-C-001: UPM G 必须 == 采样器 G（UPM 侧校验）
    return mcfg;
}
