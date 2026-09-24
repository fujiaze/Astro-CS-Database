// lib/algorithms/coverage/include/astro/phase2/stage2_common.h — Stage2 生产共享函数
//
// G2 production wiring gate 要求同一生产 parse+build path。
// astrocs-stage2 与 gate 测试共用此处的配置解析与 UPM 配置构造。
#pragma once

#include "astro/phase2/upm.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/execution_options.h"
#include "astro/phase2/sky_plane.h"   // CHAIN-WIRE-ADAPT-01: 节点间距几何导出的类型面

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

// ── CONFORM-FIX-B-009: smoothing 键 "auto" 的唯一解析值（单一来源）──
// 语义权威：docs/development/CONFIG_SCHEMA.md:19 「smoothing(auto→0.1)」；
// 负责人裁决 GAP_AUDIT §9.39 A5「smoothing_lambda 不能为 0」（λ>0 必须）。
// **不是**生产取值裁决面：具体生产 λ 由 SMOOTH-LAMBDA 分片扫描后裁决
// （本常量只定义 "auto" 这个键值解析成什么，不定义生产链路的 λ）。
// 注意与 docs/algorithms/PHASE2_UPM_IMPL.md:382「0.0（默认关闭平滑）」的
// 冲突：该表自述「冻结面，任何修改必须走 SCI/合同变更」⇒ 本实现不擅自改
// 冻结面，冲突已登记上呈（见 reports/RELEASE-02/conform-fix-b.md §009）。
constexpr double P2_SMOOTHING_LAMBDA_AUTO = 0.1;

struct P2Stage2Config {
    // CON-002 全局执行预算唯一来源（cpu/io workers、gpu route、deterministic、memory budget）。
    astro::phase2::ExecutionOptions exec = astro::phase2::default_execution_options();
    std::vector<std::string> hips;
    std::string target_order_spec = "auto";
    int target_order = -1;
    // model
    int control_grid_per_tile = 8;
    int patch_radius_leaf = 2;
    int min_samples = 5;
    double snr_search_radius_deg = 0.05;
    // background-clean sampler
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
    // CONFORM-FIX-B-009: struct 默认 == parser 默认 == CONFIG_SCHEMA「auto」
    // 解析值（CONFIG_SCHEMA.md:3-4「C++ struct 默认值、parser 默认值…必须一致」）。
    double smoothing_lambda = P2_SMOOTHING_LAMBDA_AUTO;
    double zero_anchor_weight = 1e-3;
    double sigma_floor = 1e-3;
    double support_power = 1.0;
    // 1=science weight 用 control_ivar；
    // 0=legacy snr² ablation/诊断。默认 1。
    int use_ivar_weight = 1;
    int max_irls_iterations = 100;
    double tolerance = 1e-6;
    // FIX-A 稀疏天光面链（P0-08/09/10）：观测外采样点联合拟合
    // b_k(x)=B_ref(x)+delta_k(x)。FIX-GK 方案 B（负责人裁决）：施加
    // corrected=(raw-C-δ_k)/g_k，δ_k=b_k−B_ref（归一化到公共面 B_ref，
    // 保留真实天光亮度，只消除帧间差异；不再扣整个 b_k）。
    // 缺省关闭：保持 RELEASE-01 加性 C 场行为不变（基线 447 不回归）。
    // 生产 config 模板显式 sky_plane.enabled=true / frame_gain=true 启用目标模型；
    // 由 FIX-C/E2E 启用并重验 L4。代码路径已完整接线，非空壳。
    bool   sky_plane_enabled = true;
    int    sky_plane_spline_degree = 3;
    // CHAIN-WIRE-ADAPT-01 / W5（PHASE2_UPM 7a:185-189「禁止在配置里留一个
    // 『默认节点间距』标定值」）：**0 = 未给出** ⇒ 由输入几何经
    // p2_sky_plane_derive_node_spacing 导出（stage2 工具与生产编排同一路径）。
    // 旧默认 1.0 是一个标定常数，且比 M42 上规则 1 的上界（0.0752 度）粗 13.3 倍。
    // 显式 >0 时逐字沿用（可审计覆盖）。
    double sky_plane_node_spacing_deg = 0.0;
    // 导出所需的输入几何。默认全 0 = 未给出 ⇒ stage2 由 control 观测集自行导出
    // （见 p2_sky_plane_geometry_from_controls）；显式给出时优先。
    P2SkyPlaneGeometry sky_plane_geometry{};
    // 源像素角尺度（角秒/像素）。0 = 由 <hips>/p1_stack.json 的
    // finest_input_arcsec 取（产品自身 provenance）。
    double sky_plane_pixel_scale_arcsec = 0.0;
    // B_ref 系数上限（内存护栏）。依据同生产编排：M42 上科学上更优的
    // h = 0.0376 度需要 4814 个节点，默认 2048 会让自适应 fail-closed 返回 rc=5。
    int    sky_plane_max_nodes = 8192;
    int    sky_plane_gradient_order = 1;
    int    sky_plane_gauge_mode = 0;
    int    sky_plane_weight_mode = 0;
    double sky_plane_roughness_penalty = 1e-3;
    // 乘法响应 g_k（v6 UPM MA 求解器）接入主链；build 失败时显式 g=1 降级。
    bool   frame_gain_enabled = true;
    // integration
    int precision = 0;
    std::uint64_t memory_limit_mb = 24576;
    int reject_method = P2_REJECT_AUTO;  // production default = auto
    // CONFORM-FIX-B-007: 生产默认 profile = AstroCS 自研档
    // （负责人裁决 FIX-SCI-SNR-CANON-001 / GAP_AUDIT §9.40 C2「自研的 ⇒ 改文档
    // 对齐代码」；docs/science/REJECTION.md:21,47、CONFIG_SCHEMA.md:25、
    // eng/contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67 同值）。
    // 旧默认 wbpp_2_9_1 是**对照档**，使工具链与交付 node chain 的排异方法
    // 在 n=3/6..7/≥16 边界全部分叉。
    std::string reject_profile = P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL;
    // CONFORM-FIX-B-008: 0 = 「按 profile/request 默认」，唯一权威 =
    // p2_reject_plan_resolve（rejection.h:230-234 冻结：wbpp/adaptive=2；
    // astrocs_adaptive_pixel=3；显式 extreme_prior opt-in=1）。
    // 旧默认 2 是本文件对同一默认值的第二份拷贝，且与 resolver 在
    // extreme_prior 档分叉（tool=3 vs resolver=1）。
    std::uint32_t reject_underdetermined_n = 0;
    // RejectionNormalizationPolicy（判定工作域；mask 应用回原始值）
    std::string reject_normalization = "astrocs_median_center_v1";
    double reject_normalization_floor = 1e-12;
    // astrocs.large_scale_rejection.v1（WBPP 大尺度拒绝的 AstroCS
    // 自有实现；默认关闭 = WBPP largeScaleClipLow/High 默认一致）
    bool large_scale_enabled = false;
    int large_scale_min_structure_pixels = 8;
    int large_scale_low_grow_pixels = 2;
    int large_scale_high_grow_pixels = 2;
    // method-specific typed parameters（单语义单默认）
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
    // §9.73 裁决 A44（ASTROCS_DESIGN.md §3.1:175「权重的产生链固定为两步、
    // **没有可选择项**」；docs/science/PSF_SIGNAL_WEIGHT.md §4:72「没有可选择的
    // 口径：不存在口径选择键、口径枚举、口径配置项或口径产物」）：
    // 原 legacy 整数权重模式域 int weight_mode{0,1,2} 与其字符串 token
    // （auto/ivar/equal/support_x_snr2）**已删除** —— 单一权重口径 =
    // 阶段二按天球像素对应的输入帧集合现场算出的逆方差
    // w = SNR^2 / F_ref^2 = 1/sigma_F^2，无模式选择。
    // §9.73 裁决 A44（同批清理）：原 legacy_allow_weight_fallback 开关**已删除** ——
    // 它允许「ivar 产品缺失时降级 support/equal」，而 support 是无量纲几何量、
    // equal 是等权，二者都不是信号/噪声之比 ⇒ 与 ASTROCS_DESIGN.md §3.1:173
    // 「权重只能来自纯净信号与噪声之比」及 §3.1:175「没有可选择项」冲突。
    // 唯一降级面 = 帧级 SNR 逆方差链（w = SNR^2/F_ref^2），且**由数据可用性自动决定**，
    // 不是用户可选的开关；权重链未闭合 ⇒ 显式 science 错误（fail-closed）。
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

// CON-007 + TRACEABILITY ACR-IVAR-001: ACR 块路由资格。
// §9.73 裁决 A44 删除了 legacy 整数权重模式域后，生产**只剩**一条权重口径
// （逐样本逆方差，等价于原 weight_mode=2）⇒ ACR-IVAR-001「ivar science 模式
// 必须走 CPU canonical path」对本仓**恒成立** ⇒ 本函数恒 false（ACR 块生产不可达；
// ASTROCS_DESIGN.md §2「纯 CPU 生产，ACR 生产不可达」）。
// 保留函数与 ACR 接线（不删 kernel）：将来若重开 ACR，必须先按变更流程取得
// 与逐像素 ivar 等价的证明，不得以本函数返回 true 的方式绕过。
bool p2_acr_block_eligible(const P2Stage2Config& cfg,
                           bool acr_registered,
                           int reject_method,
                           bool large_scale_active);

// ═══════════════════════════════════════════════════════════════════════════
// CHAIN-WIRE-ADAPT-01（docs/science/PHASE2_UPM.md §7a）
// 天光面节点间距所需的**输入几何**：由产品自身的控制点/观测集合导出。
//
// §7a 正向约束：「节点间距必须由输入自适应导出（不得是标定常数）…节点间距 ≤
// （该产品实际约束帧间改正量的最小尺度）/2；该尺度是**重叠带宽度**与**指向间距**
// 两者中的较小者…由 manifest/输入几何逐产品算出。**禁止**在配置里留一个
// 「默认节点间距」标定值。」
//
// 本函数是两条消费路径的**单一来源**（同 CONFORM-FIX-B-009 的「工具与 node chain
// 同语义」纪律，避免第二份策略副本）：
//   ① 生产编排 lib/infrastructure/scheduler/src/module_adapters.cpp 的 upm-fit 节点；
//   ② 工具 lib/algorithms/coverage/tools/stage2.cpp（astrocs-stage2）。
//
// 量纲（逐项，全部是角量或像素角尺度；换仪器后度数不变、像素数按比例变）：
//   pointing_spacing_deg   相邻**指向**（同一天区的一组曝光）中心的天球角距离 [deg]
//   overlap_band_width_deg 相邻指向的公共控制区在「指向连线方向」上的投影宽度 [deg]
//   sample_pitch_deg       控制点自身的角间距 [deg] = 数据自己的分辨率极限
//   pixel_scale_arcsec     源像素角尺度 [″/px]，由调用方从产品 provenance 取
//                          （生产取 <hips>/p1_stack.json 的 finest_input_arcsec）
//
// **无标定常数**：唯一的数据驱动切分用 §5d① 同款自校准栅栏 `gap > spread`
// （乘数恒为 1；两侧都取自样本自身），把「同指向多次曝光（抖动 ≪ 指向间距）」
// 与「不同指向」分开。判据不武装（样本分布无显著间隙）⇒ 退化为单一指向，
// 此时指向间距取帧角尺寸（同一天区不可能约束比一帧更大的尺度）。
// 重叠带的宽度用 2%–98% 的**稳健宽度**（对单个越界采样点不敏感），同时登记
// 全幅宽度供审计；两者都是**由数据算出的长度**，不是可调阈值。
// ═══════════════════════════════════════════════════════════════════════════

struct P2SkyPlaneGeometryInputs {
    // 观测（P2ControlObservation 的列视图）：frame_id + control_id + 天球方向
    const double* obs_ra_deg = nullptr;
    const double* obs_dec_deg = nullptr;
    const std::uint64_t* obs_frame_id = nullptr;
    const std::uint64_t* obs_control_id = nullptr;
    std::size_t n_obs = 0;
    // 控制点节点（P2ControlNode 的列视图）：tile_ipix + 天球方向
    const double* ctrl_ra_deg = nullptr;
    const double* ctrl_dec_deg = nullptr;
    const std::uint64_t* ctrl_tile_ipix = nullptr;
    std::size_t n_ctrl = 0;
    // 源像素角尺度（调用方从产品 provenance 取；<=0/非有限 ⇒ 显式失败）
    double pixel_scale_arcsec = 0.0;
};

// 导出过程的全部中间可观测量（写入 provenance，供 §7a 的「触发过」复核）。
struct P2SkyPlaneGeometryProvenance {
    std::size_t n_obs = 0;
    std::size_t n_frames = 0;
    std::size_t n_pointings = 0;
    std::size_t n_ctrl = 0;
    double frame_separation_fence_deg = 0.0;   // §5d① 自校准切分点（同/异指向）
    double frame_extent_median_deg = 0.0;      // 帧角尺寸中位数（单指向退化时用）
    double pointing_spacing_deg = 0.0;
    double pointing_spacing_min_deg = 0.0;
    double pointing_spacing_max_deg = 0.0;
    double overlap_band_trimmed_deg = 0.0;     // 2%–98% 稳健宽度（导出用）
    double overlap_band_full_deg = 0.0;        // 全幅宽度（审计用）
    std::size_t n_overlap_pairs = 0;           // 参与中位数的指向对数
    double sample_pitch_deg = 0.0;
    std::size_t n_pitch_samples = 0;
};

namespace p2_geo_detail {

inline double kDegPerRad() { return 57.29577951308232; }

inline double sep_deg(double r1, double d1, double r2, double d2) {
    const double c = std::sin(d1) * std::sin(d2) +
                     std::cos(d1) * std::cos(d2) * std::cos(r2 - r1);
    return std::acos(std::max(-1.0, std::min(1.0, c))) * kDegPerRad();
}

// 切平面（gnomonic）坐标，单位度；原点 = (ra0, dec0)，输入均为弧度。
inline void tangent_uv(double ra0, double dec0, double ra, double dec,
                       double* u_deg, double* v_deg) {
    const double dl = ra - ra0;
    const double cd = std::cos(dec), sd = std::sin(dec);
    const double cd0 = std::cos(dec0), sd0 = std::sin(dec0);
    const double cosc = sd0 * sd + cd0 * cd * std::cos(dl);
    if (!(std::fabs(cosc) > 1e-12)) { *u_deg = 0.0; *v_deg = 0.0; return; }
    const double u = cd * std::sin(dl) / cosc;
    const double v = (cd0 * sd - sd0 * cd * std::cos(dl)) / cosc;
    *u_deg = u * kDegPerRad();
    *v_deg = v * kDegPerRad();
}

inline double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t m = v.size() / 2;
    return (v.size() % 2) ? v[m] : 0.5 * (v[m - 1] + v[m]);
}

// §5d① 同款自校准栅栏（乘数恒为 1）：升序样本从最大端向下扫，第一个满足
// `v[k] − v[k−1] > v[k−1] − v[0]` 的间隙即切点（保留 v[0..k−1]）。
// 始终不满足 ⇒ 判据不武装，返回 v.back()（全部保留 = 单一族）。
inline double selfcal_fence_low_mode(const std::vector<double>& sorted) {
    if (sorted.empty()) return 0.0;
    for (std::size_t k = sorted.size(); k-- > 1;) {
        if (sorted[k] - sorted[k - 1] > sorted[k - 1] - sorted[0])
            return sorted[k - 1];
    }
    return sorted.back();
}

inline void set_err(char* err, std::size_t n, const std::string& s) {
    if (err && n) std::snprintf(err, n, "%s", s.c_str());
}

}  // namespace p2_geo_detail

// 由输入几何导出四个量。返回 0 = ok；1 = 输入不足/几何不可用（err 写明原因）。
// **不回退任何标定常数**：任一导出量非正/非有限即显式失败。
inline int p2_sky_plane_geometry_from_controls(const P2SkyPlaneGeometryInputs& in,
                                               P2SkyPlaneGeometry* out,
                                               P2SkyPlaneGeometryProvenance* prov,
                                               char* err, std::size_t err_size) {
    using namespace p2_geo_detail;
    if (out) *out = P2SkyPlaneGeometry{};
    if (prov) *prov = P2SkyPlaneGeometryProvenance{};
    if (!out) { set_err(err, err_size, "geometry output pointer is null"); return 1; }
    if (!in.obs_ra_deg || !in.obs_dec_deg || !in.obs_frame_id ||
        !in.obs_control_id || in.n_obs == 0) {
        set_err(err, err_size,
                "geometry requires a non-empty observation set"
                " (ra/dec/frame_id/control_id)");
        return 1;
    }
    if (!in.ctrl_ra_deg || !in.ctrl_dec_deg || !in.ctrl_tile_ipix ||
        in.n_ctrl == 0) {
        set_err(err, err_size,
                "geometry requires a non-empty control-node set"
                " (ra/dec/tile_ipix): the sample pitch is the data's own"
                " resolution limit and cannot be assumed");
        return 1;
    }
    if (!(std::isfinite(in.pixel_scale_arcsec) && in.pixel_scale_arcsec > 0.0)) {
        set_err(err, err_size,
                "geometry requires a finite positive source pixel scale"
                " (arcsec/px) from the product provenance");
        return 1;
    }
    if (prov) {
        prov->n_obs = in.n_obs;
        prov->n_ctrl = in.n_ctrl;
    }

    // ── 1) 帧中心 = 该帧全部观测方向的球面均值方向 ────────────────────────
    std::map<std::uint64_t, std::size_t> fmap;
    std::vector<std::size_t> fidx(in.n_obs, 0);
    for (std::size_t i = 0; i < in.n_obs; ++i) {
        auto it = fmap.emplace(in.obs_frame_id[i], fmap.size()).first;
        fidx[i] = it->second;
    }
    const std::size_t nf = fmap.size();
    if (nf < 2) {
        set_err(err, err_size,
                "geometry requires at least 2 distinct frames (got " +
                std::to_string(nf) + ")");
        return 1;
    }
    std::vector<double> sx(nf, 0.0), sy(nf, 0.0), sz(nf, 0.0);
    for (std::size_t i = 0; i < in.n_obs; ++i) {
        const double r = in.obs_ra_deg[i] / kDegPerRad();
        const double d = in.obs_dec_deg[i] / kDegPerRad();
        sx[fidx[i]] += std::cos(d) * std::cos(r);
        sy[fidx[i]] += std::cos(d) * std::sin(r);
        sz[fidx[i]] += std::sin(d);
    }
    std::vector<double> fra(nf, 0.0), fdec(nf, 0.0);
    for (std::size_t f = 0; f < nf; ++f) {
        const double nrm = std::sqrt(sx[f] * sx[f] + sy[f] * sy[f] + sz[f] * sz[f]);
        if (!(nrm > 0.0)) {
            set_err(err, err_size, "degenerate frame centre (zero mean direction)");
            return 1;
        }
        fra[f] = std::atan2(sy[f], sx[f]);
        fdec[f] = std::asin(std::max(-1.0, std::min(1.0, sz[f] / nrm)));
    }
    if (prov) prov->n_frames = nf;

    // ── 2) 帧角尺寸（该帧观测在自身切平面上的包围盒中位数）────────────────
    {
        std::vector<double> umin(nf, 0.0), umax(nf, 0.0), vmin(nf, 0.0), vmax(nf, 0.0);
        std::vector<char> seen(nf, 0);
        for (std::size_t i = 0; i < in.n_obs; ++i) {
            const std::size_t f = fidx[i];
            double u = 0.0, v = 0.0;
            tangent_uv(fra[f], fdec[f], in.obs_ra_deg[i] / kDegPerRad(),
                       in.obs_dec_deg[i] / kDegPerRad(), &u, &v);
            if (!seen[f]) { umin[f] = umax[f] = u; vmin[f] = vmax[f] = v; seen[f] = 1; }
            else {
                umin[f] = std::min(umin[f], u); umax[f] = std::max(umax[f], u);
                vmin[f] = std::min(vmin[f], v); vmax[f] = std::max(vmax[f], v);
            }
        }
        std::vector<double> ext;
        ext.reserve(nf);
        for (std::size_t f = 0; f < nf; ++f)
            if (seen[f]) ext.push_back(std::max(umax[f] - umin[f], vmax[f] - vmin[f]));
        if (prov) prov->frame_extent_median_deg = median_of(ext);
    }

    // ── 3) §5d① 自校准切分 ⇒ 指向族（单链接）────────────────────────────
    // 切分样本 = **全部帧对**的中心间距（不是最近邻）：同指向的抖动帧彼此的
    // 距离都很小，最近邻统计里根本没有「大距离」那一族，判据不会武装；而帧对
    // 距离的分布是双峰的（同指向远小于指向间距），§5d① 的 gap > spread 栅栏
    // 正好落在两峰之间。判据不武装（单指向）⇒ 全部并成一族（由退化路径处理）。
    double fence = 0.0;
    {
        std::vector<double> pairs;
        pairs.reserve(nf * (nf - 1) / 2);
        for (std::size_t a = 0; a < nf; ++a)
            for (std::size_t b = a + 1; b < nf; ++b)
                pairs.push_back(sep_deg(fra[a], fdec[a], fra[b], fdec[b]));
        std::sort(pairs.begin(), pairs.end());
        fence = selfcal_fence_low_mode(pairs);
        if (prov) prov->frame_separation_fence_deg = fence;
    }
    std::vector<int> assign(nf, -1);
    int ncl = 0;
    for (std::size_t a = 0; a < nf; ++a) {
        if (assign[a] >= 0) continue;
        std::vector<std::size_t> stack{a};
        assign[a] = ncl;
        while (!stack.empty()) {
            const std::size_t x = stack.back();
            stack.pop_back();
            for (std::size_t b = 0; b < nf; ++b) {
                if (assign[b] >= 0) continue;
                if (sep_deg(fra[x], fdec[x], fra[b], fdec[b]) <= fence) {
                    assign[b] = ncl;
                    stack.push_back(b);
                }
            }
        }
        ++ncl;
    }
    if (prov) prov->n_pointings = static_cast<std::size_t>(ncl);

    // ── 4) 指向中心与指向间距 ────────────────────────────────────────────
    std::vector<double> pcx(ncl, 0.0), pcy(ncl, 0.0), pcz(ncl, 0.0);
    for (std::size_t f = 0; f < nf; ++f) {
        const int c = assign[f];
        pcx[c] += std::cos(fdec[f]) * std::cos(fra[f]);
        pcy[c] += std::cos(fdec[f]) * std::sin(fra[f]);
        pcz[c] += std::sin(fdec[f]);
    }
    std::vector<double> pcra(ncl, 0.0), pdec(ncl, 0.0);
    for (int c = 0; c < ncl; ++c) {
        const double nrm = std::sqrt(pcx[c] * pcx[c] + pcy[c] * pcy[c] + pcz[c] * pcz[c]);
        if (!(nrm > 0.0)) {
            set_err(err, err_size, "degenerate pointing centre");
            return 1;
        }
        pcra[c] = std::atan2(pcy[c], pcx[c]);
        pdec[c] = std::asin(std::max(-1.0, std::min(1.0, pcz[c] / nrm)));
    }
    double pointing_spacing = 0.0, pmin = 0.0, pmax = 0.0;
    if (ncl >= 2) {
        std::vector<double> pnn(ncl, 0.0);
        for (int a = 0; a < ncl; ++a) {
            double best = -1.0;
            for (int b = 0; b < ncl; ++b) {
                if (b == a) continue;
                const double d = sep_deg(pcra[a], pdec[a], pcra[b], pdec[b]);
                if (best < 0.0 || d < best) best = d;
            }
            pnn[a] = (best < 0.0) ? 0.0 : best;
        }
        pointing_spacing = median_of(pnn);
        pmin = *std::min_element(pnn.begin(), pnn.end());
        pmax = *std::max_element(pnn.begin(), pnn.end());
    } else {
        // 单一指向（自校准判据不武装）：同一天区不可能约束比一帧更大的尺度。
        pointing_spacing = prov ? prov->frame_extent_median_deg : 0.0;
        pmin = pmax = pointing_spacing;
    }

    // ── 5) 重叠带宽度：共享控制点的指向对，公共点在连线方向的投影宽度 ──────
    std::map<std::uint64_t, std::vector<std::size_t>> cobs;
    for (std::size_t i = 0; i < in.n_obs; ++i) cobs[in.obs_control_id[i]].push_back(i);
    std::vector<double> band_trim, band_full;
    for (int a = 0; a < ncl; ++a) {
        for (int b = a + 1; b < ncl; ++b) {
            std::vector<std::size_t> common;
            for (const auto& kv : cobs) {
                bool in_a = false, in_b = false;
                for (std::size_t i : kv.second) {
                    const int c = assign[fidx[i]];
                    if (c == a) in_a = true;
                    else if (c == b) in_b = true;
                    if (in_a && in_b) break;
                }
                if (in_a && in_b) common.push_back(kv.second.front());
            }
            if (common.empty()) continue;
            // 连线方向的切平面（原点 = 两指向中心的角平分点）
            const double ra0 = std::atan2(
                std::sin(pcra[a]) * std::cos(pdec[a]) + std::sin(pcra[b]) * std::cos(pdec[b]),
                std::cos(pcra[a]) * std::cos(pdec[a]) + std::cos(pcra[b]) * std::cos(pdec[b]));
            const double dec0 = std::asin(std::max(-1.0, std::min(1.0,
                (std::sin(pdec[a]) + std::sin(pdec[b])) / 2.0)));
            double u1 = 0.0, v1 = 0.0, u2 = 0.0, v2 = 0.0;
            tangent_uv(ra0, dec0, pcra[a], pdec[a], &u1, &v1);
            tangent_uv(ra0, dec0, pcra[b], pdec[b], &u2, &v2);
            const double du = u2 - u1, dv = v2 - v1;
            const double nlen = std::hypot(du, dv);
            if (!(nlen > 0.0)) continue;
            const double eu = du / nlen, ev = dv / nlen;
            std::vector<double> proj;
            proj.reserve(common.size());
            for (std::size_t i : common) {
                double u = 0.0, v = 0.0;
                tangent_uv(ra0, dec0, in.obs_ra_deg[i] / kDegPerRad(),
                           in.obs_dec_deg[i] / kDegPerRad(), &u, &v);
                proj.push_back(u * eu + v * ev);
            }
            std::sort(proj.begin(), proj.end());
            const std::size_t lo = static_cast<std::size_t>(0.02 * (double)proj.size());
            const std::size_t hi = static_cast<std::size_t>(0.98 * (double)proj.size());
            const std::size_t hic = (hi < proj.size()) ? hi : proj.size() - 1;
            band_trim.push_back(proj[hic] - proj[lo]);
            band_full.push_back(proj.back() - proj.front());
        }
    }
    // 单一指向（自校准判据不武装）：同一天区内各帧的公共控制区就是整幅足迹
    // ⇒ 重叠带宽度 = 帧角尺寸。这是**由数据算出的长度**（不是标定常数），且与
    // 上面同一物理含义（公共控制区在指向连线方向的宽度）。
    double overlap_band = median_of(band_trim);
    if (band_trim.empty() && prov && prov->frame_extent_median_deg > 0.0) {
        overlap_band = prov->frame_extent_median_deg;
        band_full.push_back(overlap_band);
    }
    if (prov) {
        prov->pointing_spacing_deg = pointing_spacing;
        prov->pointing_spacing_min_deg = pmin;
        prov->pointing_spacing_max_deg = pmax;
        prov->overlap_band_trimmed_deg = overlap_band;
        prov->overlap_band_full_deg = median_of(band_full);
        prov->n_overlap_pairs = band_trim.size();
    }

    // ── 6) 控制点角间距 = 同一 tile 内控制点最近邻角距离的中位数 ──────────
    {
        std::map<std::uint64_t, std::vector<std::size_t>> by_tile;
        for (std::size_t i = 0; i < in.n_ctrl; ++i)
            by_tile[in.ctrl_tile_ipix[i]].push_back(i);
        std::vector<double> pitch;
        for (const auto& kv : by_tile) {
            const std::vector<std::size_t>& idx = kv.second;
            for (std::size_t a = 0; a < idx.size(); ++a) {
                double best = -1.0;
                for (std::size_t b = 0; b < idx.size(); ++b) {
                    if (b == a) continue;
                    const double d = sep_deg(in.ctrl_ra_deg[idx[a]] / kDegPerRad(),
                                             in.ctrl_dec_deg[idx[a]] / kDegPerRad(),
                                             in.ctrl_ra_deg[idx[b]] / kDegPerRad(),
                                             in.ctrl_dec_deg[idx[b]] / kDegPerRad());
                    if (best < 0.0 || d < best) best = d;
                }
                if (best > 0.0) pitch.push_back(best);
            }
        }
        if (prov) {
            prov->sample_pitch_deg = median_of(pitch);
            prov->n_pitch_samples = pitch.size();
        }
    }

    const double sample_pitch = prov ? prov->sample_pitch_deg : 0.0;
    if (!(std::isfinite(pointing_spacing) && pointing_spacing > 0.0) ||
        !(std::isfinite(overlap_band) && overlap_band > 0.0) ||
        !(std::isfinite(sample_pitch) && sample_pitch > 0.0)) {
        set_err(err, err_size,
                "geometry derivation failed to produce finite positive scales"
                " (pointing_spacing=" + std::to_string(pointing_spacing) +
                ", overlap_band=" + std::to_string(overlap_band) +
                ", sample_pitch=" + std::to_string(sample_pitch) +
                ", n_pointings=" + std::to_string(ncl) +
                ", n_overlap_pairs=" + std::to_string(band_trim.size()) + ")");
        return 1;
    }
    out->pointing_spacing_deg = pointing_spacing;
    out->overlap_band_width_deg = overlap_band;
    out->sample_pitch_deg = sample_pitch;
    out->pixel_scale_arcsec = in.pixel_scale_arcsec;
    set_err(err, err_size, "");
    return 0;
}

