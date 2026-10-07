// lib/algorithms/coverage/src/stage2_common.cpp — Stage2 生产共享函数
#include "astro/phase2/stage2_common.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <initializer_list>
#include <string>

namespace {

// 未知键拒绝门（GOVERN-08 A6）。
// 为什么必须有：`json::value(key, default)` 对**不存在的键**与**存在但名字拼错
// 的键**返回同一个默认值。stage2.json 是人工编辑的科学配置面；把
// `model.min_samples` 写成 `model.min_smaples` 之后，运行期看到的是「少了一个
// 键」，而「多了一个键」这件事在 JSON 里不留任何痕迹 ⇒ 打错的键静默取默认，
// 且这份配置在运行期与「故意不设该键」**完全不可分辨**。本门是唯一能把两者
// 分开的地方：把「键名不在读取集内」变成显式错误。
//
// 白名单是**本文件读取面的逐字镜像**（下面每道门的键集 = 对应代码块里
// `contains`/`value`/`[...]` 出现的键）。单一来源纪律：新增配置键必须同时
// 加进对应门的键集，否则「配置面写了、消费面没读」会以静默 no-op 的形态回来。
// 退役键（integration.weight_mode / integration.legacy_allow_weight_fallback /
// rejection.{low,high,max_iterations,min_samples}）**有意不在**白名单里，
// 但它们各自有一道更具体的拒绝面（在各自的退役说明处），那道面优先于本门。
bool reject_unknown_keys(const nlohmann::json& obj, const char* path,
                         std::initializer_list<const char*> allowed,
                         std::string* err) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        bool known = false;
        for (const char* k : allowed) {
            if (it.key() == k) { known = true; break; }
        }
        if (!known) {
            *err = std::string("unknown key: ") + path + it.key() +
                   "（键名以解析面读取集为准；打错的键不会被静默忽略，也不会"
                   "静默取默认值）";
            return false;
        }
    }
    return true;
}

}  // namespace

bool p2_stage2_parse_config(const nlohmann::json& j, P2Stage2Config* cfg, std::string* err) {
    try {
        // ── 格式版本门 ──────────────────────────────────────────────────
        // `version` 是配置面**写出**的键（仓内 8 份 stage2 模板全部写
        // "version": 1），但解析面读 0 次 ⇒ 一份按 v2 语义写的 stage2.json 会被
        // 按 v1 逐字解析且无任何提示。按错误的版本解释配置比拒绝它危险得多：
        // 结果是一份「看起来跑完了」的、参数含义全错的集成。
        if (!j.contains("version") || !j["version"].is_number_integer()) {
            *err = "missing version (stage2.json format version; "
                   "this parser implements version 1)";
            return false;
        }
        {
            const int ver = j["version"].get<int>();
            if (ver != 1) {
                *err = "unsupported version " + std::to_string(ver) +
                       " (this parser implements version 1)";
                return false;
            }
        }
        if (!reject_unknown_keys(j, "", {"version", "inputs", "model",
                                         "sky_plane", "integration", "output",
                                         "diagnostics", "execution"}, err)) {
            return false;
        }
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
        if (!reject_unknown_keys(j["inputs"], "inputs.",
                                 {"hips", "target_order"}, err)) {
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
            if (!m.is_object()) { *err = "model 必须是对象"; return false; }
            if (!reject_unknown_keys(m, "model.",
                                     {"control_grid_per_tile", "patch_radius_pixels",
                                      "min_samples", "snr_search_radius_deg",
                                      "background_patch_radius", "background_clip_sigma",
                                      "background_clip_iters",
                                      "background_max_contamination",
                                      "background_contamination_sigma",
                                      "background_min_retained_fraction",
                                      "background_tolerance", "background_neighbor_radius",
                                      "background_catalog_veto",
                                       // task-5 sampler 透传 10 键（CONFIG.md 掩膜三组键文法；
                                       // 既有三键 control_k_corr/star_mask_* +
                                       // 新七键 halo 五键/seam 回退两键）
                                       "control_k_corr",
                                       "star_mask_snr_factor", "star_mask_radius_deg",
                                       "halo_mag_thresh", "halo_r8", "halo_a",
                                       "halo_r_min", "halo_r_max",
                                       "seam_fallback_factor", "seam_fallback_r_max",
                                       "huber_delta",
                                      "smoothing", "zero_anchor_weight",
                                      "max_irls_iterations", "tolerance",
                                      "robust_loss", "snr_weight_mode", "sigma_floor",
                                      "support_power", "use_ivar_weight"}, err)) {
                return false;
            }
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
            // task-5 sampler 透传 10 键：缺键回 sampler 默认（与
            // p2_sampler_default_config 同值），非法值 fail-closed。
            // 既有三键 control_k_corr/star_mask_*：与编排 p2_sample_cfg_from_doc
            // 同口径（>0），消除 CONFIG.md「现状分叉声明」的工具/编排分叉。
            cfg->control_k_corr = m.value("control_k_corr", 1.4);
            if (!(cfg->control_k_corr > 0.0) ||
                !std::isfinite(cfg->control_k_corr)) {
                *err = "control_k_corr 必须 > 0 且有限";
                return false;
            }
            cfg->star_mask_snr_factor = m.value("star_mask_snr_factor", 10.0);
            if (!(cfg->star_mask_snr_factor > 0.0) ||
                !std::isfinite(cfg->star_mask_snr_factor)) {
                *err = "star_mask_snr_factor 必须 > 0 且有限";
                return false;
            }
            cfg->star_mask_radius_deg = m.value("star_mask_radius_deg", 0.012);
            if (!(cfg->star_mask_radius_deg > 0.0) ||
                !std::isfinite(cfg->star_mask_radius_deg)) {
                *err = "star_mask_radius_deg 必须 > 0 且有限";
                return false;
            }
            // 新七键 halo 五键 + seam 回退两键（CONFIG.md「掩膜三组键文法」；
            // 与 sampler.cpp 入口修补同口径：halo_mag_thresh 须有限，
            // halo_r8/r_min/r_max 须>0 有限且 r_min<=r_max，halo_a 须>1 有限，
            // seam_fallback_factor 须>=1 有限，seam_fallback_r_max 须>0 有限）。
            // 编排 p2_sample_cfg_from_doc 同口径解析（工具与编排一致）。
            cfg->halo_mag_thresh = m.value("halo_mag_thresh", 8.0);
            if (!std::isfinite(cfg->halo_mag_thresh)) {
                *err = "halo_mag_thresh 须为有限数";
                return false;
            }
            cfg->halo_r8 = m.value("halo_r8", 150.0);
            if (!(cfg->halo_r8 > 0.0) || !std::isfinite(cfg->halo_r8)) {
                *err = "halo_r8 必须 > 0 且有限";
                return false;
            }
            cfg->halo_a = m.value("halo_a", 1.5);
            if (!(cfg->halo_a > 1.0) || !std::isfinite(cfg->halo_a)) {
                *err = "halo_a 必须 > 1 且有限";
                return false;
            }
            cfg->halo_r_min = m.value("halo_r_min", 30.0);
            cfg->halo_r_max = m.value("halo_r_max", 300.0);
            if (!(cfg->halo_r_min > 0.0) || !std::isfinite(cfg->halo_r_min) ||
                !(cfg->halo_r_max > 0.0) || !std::isfinite(cfg->halo_r_max) ||
                cfg->halo_r_min > cfg->halo_r_max) {
                *err = "halo_r_min/halo_r_max 必须 > 0 有限且 r_min<=r_max";
                return false;
            }
            cfg->seam_fallback_factor = m.value("seam_fallback_factor", 1.5);
            if (!(cfg->seam_fallback_factor >= 1.0) ||
                !std::isfinite(cfg->seam_fallback_factor)) {
                *err = "seam_fallback_factor 必须 >= 1 且有限";
                return false;
            }
            cfg->seam_fallback_r_max = m.value("seam_fallback_r_max", 450.0);
            if (!(cfg->seam_fallback_r_max > 0.0) ||
                !std::isfinite(cfg->seam_fallback_r_max)) {
                *err = "seam_fallback_r_max 必须 > 0 且有限";
                return false;
            }
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
            if (!reject_unknown_keys(sp, "sky_plane.",
                                     {"enabled", "spline_degree", "node_spacing_deg",
                                      "frame_gradient_order", "gauge_mode",
                                      "weight_mode", "rank_rtol", "frame_gain"},
                                     err)) {
                return false;
            }
            cfg->sky_plane_enabled = sp.value("enabled", cfg->sky_plane_enabled);
            cfg->sky_plane_spline_degree = sp.value("spline_degree", cfg->sky_plane_spline_degree);
            cfg->sky_plane_node_spacing_deg =
                sp.value("node_spacing_deg", cfg->sky_plane_node_spacing_deg);
            cfg->sky_plane_gradient_order =
                sp.value("frame_gradient_order", cfg->sky_plane_gradient_order);
            cfg->sky_plane_gauge_mode = sp.value("gauge_mode", cfg->sky_plane_gauge_mode);
            cfg->sky_plane_weight_mode = sp.value("weight_mode", cfg->sky_plane_weight_mode);
            cfg->sky_plane_rank_rtol = sp.value("rank_rtol", cfg->sky_plane_rank_rtol);
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
                // rejection 面的未知键门。位置在退役键拒绝面之后（理由同
                // integration 面）：退役键必须先命中自己那道带迁移提示的门。
                if (!reject_unknown_keys(rj, "integration.rejection.",
                                         {"method", "profile", "underdetermined_n",
                                          "normalization", "normalization_floor",
                                          "large_scale", "robust_mad_clip",
                                          "winsorized_sigma", "averaged_sigma",
                                          "linear_fit", "generalized_esd",
                                          "percentile", "median_sigma", "minmax",
                                          "rcr"}, err)) {
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
                // FZ-REJ-NO-MINMAX（docs/science/algorithms/PHASE2_REJECTION.md「逐公式定义（算法级，与 SCI §5 同构；单位见 §2）」一节）:
                // min/max 在合同层不可选。phase_config schema 的
                // algorithm_rejection_method 枚举已删 minmax，但本键
                // （integration.rejection.method）不在该 schema 的键集内
                // （mosaic_config.additionalProperties=false）⇒ schema 枚举**不覆盖**
                // 这条旁路，必须在此同面 fail-closed 封堵，否则生产可达
                // reject_minmax_impl。
                else if (method == "minmax") {
                    *err = "rejection.method=minmax 不可选 (FZ-REJ-NO-MINMAX): "
                           "min/max 不得用于生产；AUTO 路由值域恒为 "
                           "{percentile, winsorized_sigma, linear_fit}。"
                           "见 docs/science/algorithms/PHASE2_REJECTION.md 的「逐公式定义（算法级，与 SCI §5 同构；单位见 §2）」一节。";
                    return false;
                }
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
                // acsd_adaptive_pixel（FIX-SCI-SNR-CANON-001 /
                // SCI REJECTION §4/§5/§7、CONFIG_SCHEMA.md:25、
                // contracts provenance:67 同值）。wbpp_2_9_1 为**对照档**、
                // wbpp_current 为 migration alias（规范化到 wbpp_2_9_1）。
                // 与 node chain（module_adapters.cpp p2_op_reject 缺省）一致。
                cfg->reject_profile = rj.value(
                    "profile",
                    std::string(P2_PROFILE_ACSD_ADAPTIVE_PIXEL));
                if (cfg->reject_profile == "wbpp_current")
                    cfg->reject_profile = "wbpp_2_9_1";   // alias 规范化
                if (cfg->reject_profile != "wbpp_2_9_1" &&
                    cfg->reject_profile != "acsd_adaptive" &&
                    cfg->reject_profile != "acsd_adaptive_pixel") {
                    *err = "rejection.profile 只支持 wbpp_2_9_1（冻结）/ "
                           "acsd_adaptive / acsd_adaptive_pixel";
                    return false;
                }
                // CONFORM-FIX-B-008：underdetermined_n 默认值**单一来源** =
                // p2_reject_plan_resolve 的 profile/request 规则（rejection.h:230-234
                // 冻结：0 = 按 profile 默认；wbpp/adaptive=2；
                // acsd_adaptive_pixel=3（n<=3 保守 none）；显式
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
                // rejection normalization 独立命名（acsd_*_v1；
                // 旧 median_center/median_scale 为 migration alias）
                cfg->reject_normalization = rj.value(
                    "normalization",
                    std::string("acsd_median_center_v1"));
                if (cfg->reject_normalization == "median_center")
                    cfg->reject_normalization = "acsd_median_center_v1";
                else if (cfg->reject_normalization == "median_scale")
                    cfg->reject_normalization = "acsd_median_scale_v1";
                if (cfg->reject_normalization != "none" &&
                    cfg->reject_normalization != "acsd_median_center_v1" &&
                    cfg->reject_normalization != "acsd_median_scale_v1") {
                    *err = "rejection.normalization 只支持 none/"
                           "acsd_median_center_v1/acsd_median_scale_v1";
                    return false;
                }
                cfg->reject_normalization_floor =
                    rj.value("normalization_floor", 1e-12);
                // large_scale_rejection.v1（connected-component grow）
                if (rj.contains("large_scale")) {
                    const auto& ls = rj["large_scale"];
                    if (!reject_unknown_keys(ls, "integration.rejection.large_scale.",
                                             {"enabled", "min_structure_pixels",
                                              "low_grow_radius_pixels",
                                              "high_grow_radius_pixels"}, err)) {
                        return false;
                    }
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
                    cfg->reject_normalization != "acsd_median_center_v1") {
                    *err = "rejection: percentile 必须 normalization="
                           "acsd_median_center_v1（负值科学域安全）";
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
                    if (!reject_unknown_keys(
                            s, "integration.rejection.robust_mad_clip.",
                            {"lower_sigma", "upper_sigma", "max_iterations"}, err)) {
                        return false;
                    }
                    cfg->sigma_lower = s.value("lower_sigma", 4.0);
                    cfg->sigma_upper = s.value("upper_sigma", 3.0);
                    cfg->sigma_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("winsorized_sigma")) {
                    const auto& s = rj["winsorized_sigma"];
                    if (!reject_unknown_keys(
                            s, "integration.rejection.winsorized_sigma.",
                            {"lower_sigma", "upper_sigma", "max_iterations"}, err)) {
                        return false;
                    }
                    cfg->winsor_lower = s.value("lower_sigma", 4.0);
                    cfg->winsor_upper = s.value("upper_sigma", 3.0);
                    cfg->winsor_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("averaged_sigma")) {
                    const auto& s = rj["averaged_sigma"];
                    if (!reject_unknown_keys(
                            s, "integration.rejection.averaged_sigma.",
                            {"lower_sigma", "upper_sigma", "max_iterations"}, err)) {
                        return false;
                    }
                    cfg->avg_lower = s.value("lower_sigma", 4.0);
                    cfg->avg_upper = s.value("upper_sigma", 3.0);
                    cfg->avg_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("linear_fit")) {
                    const auto& s = rj["linear_fit"];
                    if (!reject_unknown_keys(
                            s, "integration.rejection.linear_fit.",
                            {"lower", "upper", "max_iterations"}, err)) {
                        return false;
                    }
                    cfg->linfit_lower = s.value("lower", 5.0);   // WBPP Light
                    cfg->linfit_upper = s.value("upper", 3.5);
                    cfg->linfit_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("generalized_esd")) {
                    const auto& s = rj["generalized_esd"];
                    if (!reject_unknown_keys(
                            s, "integration.rejection.generalized_esd.",
                            {"alpha", "max_outliers"}, err)) {
                        return false;
                    }
                    cfg->esd_alpha = s.value("alpha", 0.05);
                    cfg->esd_max_outliers = s.value("max_outliers", 10);
                }
                if (rj.contains("percentile")) {
                    const auto& s = rj["percentile"];
                    if (!reject_unknown_keys(
                            s, "integration.rejection.percentile.",
                            {"low_fraction", "high_fraction"}, err)) {
                        return false;
                    }
                    cfg->pct_low_fraction =
                        s.value("low_fraction", 0.2);  // WBPP Light
                    cfg->pct_high_fraction =
                        s.value("high_fraction", 0.1);
                }
                if (rj.contains("median_sigma")) {
                    const auto& s = rj["median_sigma"];
                    if (!reject_unknown_keys(
                            s, "integration.rejection.median_sigma.",
                            {"lower_sigma", "upper_sigma", "max_iterations"}, err)) {
                        return false;
                    }
                    cfg->medsig_lower = s.value("lower_sigma", 4.0);
                    cfg->medsig_upper = s.value("upper_sigma", 3.0);
                    cfg->medsig_max_iterations =
                        s.value("max_iterations", 8);
                }
                if (rj.contains("minmax")) {
                    const auto& s = rj["minmax"];
                    if (!reject_unknown_keys(
                            s, "integration.rejection.minmax.",
                            {"reject_low_count", "reject_high_count", "min_kept"},
                            err)) {
                        return false;
                    }
                    cfg->minmax_low_count =
                        s.value("reject_low_count", 1);
                    cfg->minmax_high_count =
                        s.value("reject_high_count", 1);
                    cfg->minmax_min_kept = s.value("min_kept", 4);
                }
                if (rj.contains("rcr")) {
                    const auto& s = rj["rcr"];
                    if (!reject_unknown_keys(s, "integration.rejection.rcr.",
                                             {"technique"}, err)) {
                        return false;
                    }
                    cfg->rcr_technique =
                        s.value("technique", std::string("ss_median_dl"));
                    if (cfg->rcr_technique != "ss_median_dl") {
                        *err = "rcr.technique 只支持 ss_median_dl（V15 冻结）";
                        return false;
                    }
                }
            }
        // ── legacy 整数权重模式域与其 token 已删除 ──────────
        // 规范依据（权威，只读）。只引节号、不引行锚：正文增删会让行锚漂移，
        // 行锚漂移后锚点即悬空，读者回不到任何在册条款。
        //   · docs/ACSD_DESIGN.md §3.1（数据对象）「全链没有「权重模式」这一可选概念」；
        //   · docs/ACSD_DESIGN.md §5.3（信噪比重建与逆方差叠加）承载换算式
        //     w = SNR²/F_ref² = 1/σ_F²（该节只作引用，F_ref/σ_F 口径定义处是 §2.2）；
        //   · docs/science/unified/UNIFIED_SCIENCE_MODEL.md §4（单一权重口径）：
        //     「**没有可选择的口径**：不存在口径选择键、口径枚举、口径配置项或口径产物」。
        // 原实现把 integration.weight_mode ∈ {auto,ivar,equal,support_x_snr2} 映射为
        // 整数域 {2,2,1,0}：equal 直接开等权、support_x_snr2 开 support×snr²
        // （无量纲、非信号/噪声之比）——两者都与「没有可选择项」直接冲突。
        // ⇒ 该键**既不能被设、也不能被读**：出现即 fail-closed 拒绝（退役对象的
        //   拒绝面必须存活，不得静默忽略或静默取默认值）。
        if (in.contains("weight_mode")) {
            *err = "integration.weight_mode 已删除：不存在「权重模式」"
                   "（docs/ACSD_DESIGN.md §3.1（数据对象）：全链没有「权重模式」这一可选概念；"
                   "docs/science/unified/UNIFIED_SCIENCE_MODEL.md §4（单一权重口径）：没有可选择的口径）。"
                   "权重是阶段二按天球像素对应的输入帧集合现场算出的派生量 "
                   "w = SNR^2/F_ref^2 = 1/sigma_F^2（docs/ACSD_DESIGN.md §5.3）；请删除该键。";
            return false;
        }
        // legacy_allow_weight_fallback **已删除** ——
        // 该键曾允许「ivar 缺失 → 降级 support/equal」，而 support/equal 都不是
        // 信号/噪声之比（docs/ACSD_DESIGN.md §3.1：叠加权重是 mosaic 集成时现场换算的派生量）。
        // ⇒ 该键**既不能被设、也不能被读**：出现即 fail-closed 拒绝。
        if (in.contains("legacy_allow_weight_fallback")) {
            *err = "integration.legacy_allow_weight_fallback 已删除："
                   "它允许用无量纲 support 或等权降级冒充逆方差权重，与 "
                   "docs/ACSD_DESIGN.md §3.1（数据对象）叠加权重是 mosaic 集成时"
                   "现场换算的派生量、"
                   "全链没有「权重模式」这一可选概念冲突。唯一降级面 = 帧级 SNR 逆方差链 "
                   "w = SNR^2/F_ref^2（由数据可用性自动决定，不是用户开关；"
                   "docs/ACSD_DESIGN.md §5.3）；"
                   "权重链未闭合即显式 science 错误。请删除该键。";
            return false;
        }
        // integration.acr_route **已删除** ——
        // 该键曾在 auto/cpu 之间选择「集成执行路由」，把一个已整体退场的加速后端
        // 重新接回配置面；生产计算后端恒为纯 CPU。
        //   · docs/ACSD_DESIGN.md §1.3（非目标）首条：「GPU 与 CPU/GPU 混合生产路由」；
        //   · docs/ACSD_DESIGN.md §9（CPU 后端与资源）首条：「生产仅纯 CPU」。
        // ⇒ 该键**既不能被设、也不能被读**：出现即 fail-closed 拒绝（退役对象的
        //   拒绝面必须存活，不得静默忽略或静默取默认值）。
        if (in.contains("acr_route")) {
            *err = "integration.acr_route 已删除："
                   "该键只用于在 auto/cpu 之间选择集成执行路由，"
                   "而生产计算后端恒为纯 CPU（docs/ACSD_DESIGN.md §9（CPU 后端与资源）："
                   "生产仅纯 CPU）；GPU 与 CPU/GPU 混合生产路由属非目标"
                   "（docs/ACSD_DESIGN.md §1.3（非目标））。请删除该键。";
            return false;
        }
        // integration 面的未知键门。位置在三道退役键拒绝面**之后**：那三道带
        // 各自的规范引用与迁移提示，必须先被命中；本门只兜住其余拼错的键。
        if (!reject_unknown_keys(in, "integration.",
                                 {"precision", "memory_limit_mb", "rejection"},
                                 err)) {
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
        if (!reject_unknown_keys(j["output"], "output.", {"hips"}, err)) {
            return false;
        }
        if (j.contains("diagnostics")) {
            if (!reject_unknown_keys(j["diagnostics"], "diagnostics.",
                                     {"enabled"}, err)) {
                return false;
            }
            cfg->diagnostics = j["diagnostics"].value("enabled", true);
        }
        // CON-002 global worker budget contract (execution block)
        if (j.contains("execution")) {
            const auto& ex = j["execution"];
            if (!reject_unknown_keys(ex, "execution.",
                                     {"cpu_workers", "io_workers", "gpu_route",
                                      "deterministic", "memory_budget_bytes"}, err)) {
                return false;
            }
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
    // 删除 legacy 整数权重模式域后，生产**只剩**一条权重口径：
    // 逐样本逆方差（原 weight_mode=2 语义）。TRACEABILITY ACR-IVAR-001 冻结：
    // 「ivar science 模式必须走 CPU canonical path」；异构执行面亦对
    // cell-ivar 权重显式拒绝（ACR 与逐像素 ivar 不等价）。
    // ⇒ 该条件对本仓**恒成立** ⇒ 本函数恒 false：ACR 块在生产不可达
    //   （docs/ACSD_DESIGN.md §2「纯 CPU 生产，ACR 生产不可达」）。
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
