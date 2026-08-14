// lib/phase2/tools/stage2.cpp — Phase2 正式入口（W8 完整 CPU 链闭合）
//
// 用法：astrocs-stage2 <stage2.json>（唯一参数，禁止长串 CLI 科学参数）
//
// 流程（控制包 34A532A2...B2EB308 EXECUTION_ORDER）：
//   DISCOVER → VALIDATE → COVERAGE_UNION → CONTROL_SAMPLE → UPM_FIT →
//   UPM_PERSIST → BLOCK_PLAN → BLOCK_CALIBRATE → REJECT_INTEGRATE →
//   HIPS_WRITE → HIPS_VERIFY
//
// 科学语义全部来自 lib/phase2（CPU reference 权威），HiPS 读写走唯一 AIO。
#include "astro/phase2/upm.h"
#include "astro/phase2/stage2_common.h"
#include "astro/phase2/coverage.h"
#include "astro/phase2/sampler.h"
#include "astro/phase2/rejection.h"
#include "astro/phase2/integrate.h"
#include "astro/phase2/block.h"
#include "astro/phase2/acr_kernels.h"

#include "healpix/healpix_core.h"
#include "crypto/sha256.h"
#include "astro/compute/kernel_registry.hpp"
#include "astro/compute/task_traits.hpp"
#include "cuda_bridge_api.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

extern "C" {
#include "aio_hips.h"
#include "aio_hips_reader.h"
}

namespace {

std::ofstream g_log;

void log(const std::string& msg) {
    std::fprintf(stdout, "[stage2] %s\n", msg.c_str());
    std::fflush(stdout);
    if (g_log.is_open()) g_log << msg << "\n";
}


// 帧级 SNR（Phase1 SNR Catalogue median；禁止重新检测星点）
std::vector<double> frame_snr_medians(const std::vector<std::string>& hips) {
    std::vector<double> out;
    for (const auto& p : hips) {
        AioHipsDataset* d = aio_hips_open(p.c_str(), AIO_HIPS_RD_SNR);
        double med = 1.0;
        if (d) {
            const int maxn = 1 << 16;
            std::vector<double> ra(maxn), dec(maxn), snr(maxn);
            const int got = aio_hips_read_snr_catalog(
                d, ra.data(), dec.data(), snr.data(), nullptr, nullptr,
                nullptr, maxn);
            if (got > 0) {
                snr.resize(got);
                std::nth_element(snr.begin(), snr.begin() + got / 2, snr.end());
                med = snr[got / 2];
            }
            aio_hips_close(d);
        }
        out.push_back(med);
    }
    return out;
}

std::string today_stamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y%m%d", &tm);
    return std::string(buf);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr,
                     "usage: astrocs-stage2 <stage2.json>\n"
                     "只允许一个 JSON 配置路径参数。\n");
        return 2;
    }
    nlohmann::json j;
    {
        std::ifstream f(argv[1]);
        if (!f) {
            std::fprintf(stderr, "cannot open config: %s\n", argv[1]);
            return 2;
        }
        try {
            f >> j;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "config parse error: %s\n", e.what());
            return 2;
        }
    }
    P2Stage2Config cfg;
    std::string err;
    if (!p2_stage2_parse_config(j, &cfg, &err)) {
        std::fprintf(stderr, "config error: %s\n", err.c_str());
        return 2;
    }
    for (const auto& w : cfg.deprecation_warnings)
        std::fprintf(stderr, "[stage2][deprecation] %s\n", w.c_str());

    // 日志：run/logs/phase2/<YYYYMMDD>/stage2.log
    const std::string log_dir =
        "run/logs/phase2/" + today_stamp();
    std::filesystem::create_directories(log_dir);
    g_log.open(log_dir + "/stage2.log", std::ios::app);
    log("AstroCS Stage2 (UPM + block rejection mosaic) start");
    log("inputs: " + std::to_string(cfg.hips.size()) + " HiPS");

    const auto t_start = std::chrono::steady_clock::now();
    auto t_stage = t_start;
    auto mark = [&](const char* name) {
        const auto now = std::chrono::steady_clock::now();
        const double s = std::chrono::duration<double>(now - t_stage).count();
        log(std::string("profile ") + name + "=" + std::to_string(s) + "s");
        t_stage = now;
        return s;
    };

    // ---- W3 DISCOVER / VALIDATE / COVERAGE UNION ----
    std::vector<const char*> paths;
    for (const auto& p : cfg.hips) paths.push_back(p.c_str());
    P2CoverageResult cov{};
    cov.n_inputs = cfg.hips.size();
    std::vector<P2HipsInputInfo> infos(cfg.hips.size());
    cov.inputs = infos.data();
    if (p2_coverage_build(paths.data(), paths.size(), &cov) != 0) {
        log("coverage error: " + std::string(cov.error));
        return 3;
    }
    std::vector<P2MocCell> cells(cov.n_union_cells);
    cov.union_cells = cells.data();
    if (p2_coverage_build(paths.data(), paths.size(), &cov) != 0) {
        log("coverage error (fill): " + std::string(cov.error));
        return 3;
    }
    const int target_order =
        (cfg.target_order >= 0) ? cfg.target_order : cov.target_order;
    if (target_order > cov.target_order) {
        log("target_order 高于输入最高 order，禁止插值伪装分辨率");
        return 3;
    }
    log("coverage: inputs=" + std::to_string(cov.n_inputs) +
        " union_cells=" + std::to_string(cov.n_union_cells) +
        " target_order=" + std::to_string(target_order));
    mark("coverage");

    // R2/R3：frame_id 缓存（payload 敏感，DISCOVER 阶段一次计算）+
    // input manifest hash = canonical(frame identity + 关键元数据)
    std::vector<std::uint64_t> frame_id_cache(cfg.hips.size());
    std::vector<std::pair<std::uint64_t, std::string>> manifest_entries;
    for (std::size_t i = 0; i < cfg.hips.size(); ++i) {
        const std::uint64_t fid = p2_frame_id(cfg.hips[i].c_str());
        frame_id_cache[i] = fid;
        std::string meta;
        if (i < infos.size()) {
            meta += std::string("filter=") + infos[i].filter_passband + ";";
            meta += "order=" + std::to_string(infos[i].max_leaf_order) + ";";
            meta += "frame=" + std::string(infos[i].frame_type) + ";";
        }
        manifest_entries.push_back({fid, meta});
    }
    std::sort(manifest_entries.begin(), manifest_entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    std::string manifest_payload;
    for (const auto& e : manifest_entries)
        manifest_payload += std::to_string(e.first) + "|" + e.second + ";";
    const std::string input_manifest_hash = astrocs::crypto::sha256_hex(
        manifest_payload.data(), manifest_payload.size());
    log("input_manifest_hash=" + input_manifest_hash);
    // V4 R6：帧级 SNR median（whole-frame fallback 源；frame_id → 值映射）
    const std::vector<double> frame_snr = frame_snr_medians(cfg.hips);
    std::map<std::uint64_t, double> frame_snr_by_id;
    for (std::size_t i = 0; i < cfg.hips.size(); ++i)
        frame_snr_by_id[frame_id_cache[i]] = frame_snr[i];

    // ---- W4 CONTROL SAMPLE ----
    P2SamplerConfig sccfg{};
    sccfg.control_grid_per_tile = cfg.control_grid_per_tile;
    sccfg.patch_radius_leaf = cfg.patch_radius_leaf;
    sccfg.min_samples = cfg.min_samples;
    sccfg.snr_search_radius_deg = cfg.snr_search_radius_deg;
    // V13 background-clean 参数
    sccfg.background_patch_radius = cfg.background_patch_radius;
    sccfg.background_clip_sigma = cfg.background_clip_sigma;
    sccfg.background_clip_iters = cfg.background_clip_iters;
    sccfg.background_max_contamination = cfg.background_max_contamination;
    sccfg.background_contamination_sigma = cfg.background_contamination_sigma;
    sccfg.background_min_retained_fraction =
        cfg.background_min_retained_fraction;
    sccfg.background_tolerance = cfg.background_tolerance;
    sccfg.background_neighbor_radius = cfg.background_neighbor_radius;
    sccfg.background_catalog_veto = cfg.background_catalog_veto;
    std::uint64_t n_obs = 0, n_ctrl = 0;
    char serr[512] = {0};
    P2SampleStats sstats{};
    if (p2_sample_controls(&cov, paths.data(), &sccfg, nullptr, 0,
                           &n_obs, &n_ctrl, &sstats, nullptr, 0,
                           serr, sizeof(serr)) != 0) {
        log("sampler error: " + std::string(serr));
        return 4;
    }
    std::vector<P2ControlObservation> obs(n_obs);
    std::vector<P2ControlNode> ctrl_nodes(n_ctrl);
    if (p2_sample_controls(&cov, paths.data(), &sccfg, obs.data(), n_obs,
                           &n_obs, &n_ctrl, &sstats, ctrl_nodes.data(),
                           ctrl_nodes.size(), serr, sizeof(serr)) != 0) {
        log("sampler error (fill): " + std::string(serr));
        return 4;
    }
    log("control sampling (V13 background-clean): controls=" +
        std::to_string(n_ctrl) + " observations=" + std::to_string(n_obs) +
        " candidates=" + std::to_string(sstats.candidate_observations) +
        " accepted=" + std::to_string(sstats.accepted_observations) +
        " rejected[support=" +
        std::to_string(sstats.rejected_insufficient_support) +
        " retained=" +
        std::to_string(sstats.rejected_insufficient_retained) +
        " tolerance=" + std::to_string(sstats.rejected_bright_tolerance) +
        " contamination=" + std::to_string(sstats.rejected_high_contamination) +
        " catalog=" + std::to_string(sstats.rejected_catalog_veto) +
        " lt2frames=" + std::to_string(sstats.rejected_lt_two_clean_frames) +
        "] accepted_controls=" + std::to_string(sstats.accepted_controls) +
        " overlap_controls=" + std::to_string(sstats.overlap_controls));
    // V13 (P13-6)：accepted/rejected control 诊断（overlay 用）
    if (cfg.diagnostics && !cfg.out_hips.empty()) {
        std::set<std::uint64_t> accepted_ids;
        for (const auto& o : obs) accepted_ids.insert(o.control_id);
        nlohmann::json ca = nlohmann::json::array();
        for (std::uint64_t i = 0; i < n_ctrl; ++i) {
            ca.push_back({{"control_id", i},
                          {"tile", ctrl_nodes[(size_t)i].tile_ipix},
                          {"ra", ctrl_nodes[(size_t)i].ra_deg},
                          {"dec", ctrl_nodes[(size_t)i].dec_deg},
                          {"accepted", accepted_ids.count(i) > 0}});
        }
        std::ofstream ca_f(cfg.out_hips + "/controls_accept.json");
        if (ca_f) ca_f << ca.dump();
        log("V13 controls_accept written: " + cfg.out_hips +
            "/controls_accept.json");
    }
    mark("control_sample");
    // R2：quality fallback 统计（quality_flags==0 = QUALITY_FALLBACK_UNKNOWN）
    std::uint64_t quality_unknown = 0;
    for (const auto& o : obs)
        if (o.quality_flags == 0) ++quality_unknown;
    log("quality fallback unknown: " + std::to_string(quality_unknown) +
        " / " + std::to_string(n_obs));
    // R8：局部 SNR 映射（control cell 级 = 可证明的最近空间 catalogue
    // 区域）：(frame_id, tile, gx, gy) -> snr。像素权重优先局部，
    // 缺失才 fallback 整帧 SNR median 并计数。
    // V4 R6：只有真实可用的局部 SNR 才进入 local_snr_map；无局部星点的
    // control observation 先回退整帧 SNR median（UPM 控制权重语义），
    // 且不允许以 snr=1.0 伪装 unknown。
    std::map<std::tuple<std::uint64_t, std::uint64_t, int, int>, double>
        local_snr_map;
    std::uint64_t local_snr_unavailable = 0;
    for (const auto& o : obs) {
        if (!o.snr_available) {
            ++local_snr_unavailable;
            continue;   // 不进入 local map（像素级回退整帧 median）
        }
        const std::uint64_t tile = o.leaf_ipix >> 18;
        const std::uint64_t local = o.leaf_ipix & ((1ULL << 18) - 1ULL);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, 9u, x, y);
        local_snr_map[std::make_tuple(o.frame_id, tile, (int)(x / 64),
                                      (int)(y / 64))] = o.snr;
    }
    // V4 R6：UPM 控制权重回退——无局部星点的观测用整帧 SNR median
    // （保持与像素级 fallback 相同策略；snr_available 位仍保留记录）。
    for (auto& o : obs) {
        if (!o.snr_available) {
            const auto it = frame_snr_by_id.find(o.frame_id);
            if (it != frame_snr_by_id.end()) o.snr = it->second;
        }
    }
    std::uint64_t local_snr_used = 0, frame_snr_fallback = 0;
    log("local snr unavailable controls (fallback to frame median): " +
        std::to_string(local_snr_unavailable));

    // ---- W4 UPM FIT ----
    // R1（V4）：生产共享 UPM 配置构造（与 gate 测试同一 path）
    P2UpmBuildConfig mcfg =
        p2_stage2_make_upm_cfg(cfg, target_order,
                               input_manifest_hash.c_str());
    void* model = nullptr;
    if (p2_upm_build_geo(obs.data(), obs.size(), ctrl_nodes.data(),
                         ctrl_nodes.size(), &mcfg, &model) != 0) {
        log("UPM build failed");
        return 5;
    }
    P2ModelInfo minfo{};
    p2_upm_info(model, &minfo);
    log("UPM: controls=" + std::to_string(minfo.control_count) +
        " obs=" + std::to_string(minfo.observation_count) +
        " components=" + std::to_string(minfo.component_count) +
        " hash=" + std::string(minfo.model_hash, 12) + "...");
    mark("upm_fit");

    // ---- W4 UPM PERSIST (diagnostics) ----
    std::string model_path;
    if (cfg.diagnostics) {
        std::filesystem::create_directories(cfg.out_hips);
        model_path = cfg.out_hips + "/upm_sparse.json";
        if (p2_upm_save(model, model_path.c_str()) != 0) {
            log("UPM save failed");
            p2_upm_close(model);
            return 5;
        }
        const std::string cache = cfg.out_hips + "/upm_dense.cache";
        if (p2_upm_materialize_dense(model, target_order, cache.c_str()) != 0) {
            log("UPM dense materialize failed");
            p2_upm_close(model);
            return 5;
        }
        log("UPM persisted: " + model_path + " + dense cache");
    }
    mark("upm_persist");

    // ---- W6 BLOCK PLAN（tile 级；报告峰值估算） ----
    P2BlockPlannerInput bp{};
    bp.output_pixels = 512ull * 512ull;
    bp.covering_frames = cov.n_inputs;
    bp.precision = cfg.precision;
    bp.memory_limit_bytes = cfg.memory_limit_mb * 1024ull * 1024ull;
    bp.safety_factor = 0.75;
    bp.scratch_bytes_per_sample = 16;
    bp.scratch_bytes_per_pixel = 16;
    bp.fixed_overhead = 1ull << 26;
    P2BlockPlan plan{};
    p2_block_plan(&bp, &plan);
    log("block plan: tile_pixels=262144 est_peak=" +
        std::to_string(plan.estimated_peak_bytes) +
        " bytes micro_chunk=" + std::to_string(plan.micro_chunk_required));
    mark("block_plan");

    // ---- W8 REJECT + INTEGRATE + HIPS WRITE ----
    const int nside = 1 << (target_order + 9);
    const std::uint64_t n_leaf = 512ull * 512ull;
    const double A_cell =
        4.0 * 3.14159265358979323846 / (12.0 * (double)nside * (double)nside);
    const int dtype = cfg.precision ? AIO_HIPS_FLOAT64 : AIO_HIPS_FLOAT32;
    const std::string filter = infos.empty() ? "" : infos[0].filter_passband;

    std::vector<AioHipsDataset*> sig(cfg.hips.size(), nullptr);
    std::vector<AioHipsDataset*> sup(cfg.hips.size(), nullptr);
    for (std::size_t i = 0; i < cfg.hips.size(); ++i) {
        sig[i] = aio_hips_open(cfg.hips[i].c_str(), AIO_HIPS_RD_SIGNAL);
        sup[i] = aio_hips_open(cfg.hips[i].c_str(), AIO_HIPS_RD_SUPPORT);
        if (!sig[i] || !sup[i]) {
            log("open frame " + std::to_string(i) + " failed: " +
                aio_hips_reader_last_error());
            for (std::size_t k = 0; k <= i; ++k) {
                if (sig[k]) aio_hips_close(sig[k]);
                if (sup[k]) aio_hips_close(sup[k]);
            }
            p2_upm_close(model);
            return 6;
        }
    }
    AioHipsProductSet* ps = aio_hips_product_begin(
        cfg.out_hips.c_str(), (std::uint32_t)nside, 512, dtype,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/phase2", "AstroCS Phase2 Mosaic",
        filter.empty() ? nullptr : filter.c_str(), 0.0, nullptr, 0);
    if (!ps) {
        log("hips begin failed: " + std::string(aio_hips_last_error()));
        return 6;
    }

    std::uint64_t total_pixels = 0, total_rejected = 0, total_fallback = 0;
    std::uint64_t dbg_reject_px = 0, dbg_fallback_px = 0, dbg_zero_px = 0;
    std::uint64_t underdetermined_px = 0;  // V15：REJECTION_UNDERDETERMINED
    std::uint64_t px_depth_0 = 0;  // V16：mutually exclusive depth 诊断
    std::map<std::uint32_t, std::uint64_t> reject_hist;  // 每像素拒绝样本数分布
    std::map<std::uint32_t, std::string> resolved_methods;  // nominal depth → semantic id
    // V7 P7-2：overlap topology 诊断
    std::uint64_t px_depth_1 = 0, px_depth_ge_2 = 0, px_integrated = 0;
    std::uint64_t tiles_written = 0;
    std::vector<std::uint8_t> valid(n_leaf);
    std::vector<float> fluxF(n_leaf), areaF(n_leaf);
    std::vector<double> fluxD(n_leaf), areaD(n_leaf);
    std::vector<float> t_sig_probe(512 * 512);
    const std::uint32_t nb = (std::uint32_t)cfg.hips.size();

    // ---- ACR 路由（G9/V15）：按 tile 解析后的显式方法决定是否走
    // KernelRegistry（仅 robust_mad_clip/sigma）；GPU 可用则 CUDA，
    // 否则 CPU legacy（同一科学语义）。 ----
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* acr_reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    namespace bridge = astro::compute::cuda::bridge;
    bool gpu_ready = false;
    void* gpu_exec = nullptr;

    // V16：wbpp_current = integration-group level 一次解析（nominal = 全部
    // 独立 exposure 数）；astrocs_adaptive 在 tile 层按 nominal depth 解析。
    const bool group_level =
        (cfg.reject_profile == "wbpp_current");
    P2RejectionPlan group_plan{};
    std::string group_method = "none";
    if (group_level) {
        P2RejectionPlanRequest g_req{};
        g_req.request = cfg.reject_method;
        g_req.nominal_contributors = (std::uint32_t)cfg.hips.size();
        g_req.profile = cfg.reject_profile.c_str();
        g_req.underdetermined_n = cfg.reject_underdetermined_n;
        char gerr[160] = {0};
        if (p2_reject_plan_resolve(&g_req, &group_plan, gerr, sizeof(gerr)) != 0) {
            log("rejection group plan resolve failed: " + std::string(gerr));
            p2_upm_close(model);
            return 6;
        }
        group_method = p2_rejection_semantic_id(group_plan.method);
        log("wbpp_current group plan: nominal=" +
            std::to_string(cfg.hips.size()) + " method=" + group_method);
    }
    for (std::uint64_t ci = 0; ci < cov.n_union_cells; ++ci) {
        const std::uint64_t tile_ipix = cells[ci].ipix;
        // 覆盖帧列表（探测读取，不预分配全帧缓冲）
        std::vector<std::uint32_t> frames;
        for (std::uint32_t f = 0; f < nb; ++f) {
            AioHipsDataset* d = sig[f];
            if (aio_hips_read_tile_f32(d, tile_ipix, t_sig_probe.data()) != 0)
                continue;
            frames.push_back(f);
        }
        if (frames.empty()) continue;
        const std::uint32_t depth = (std::uint32_t)frames.size();

        // V16：planning 层解析 rejection
        //   wbpp_current    → 使用 group-level 一次解析结果（tile 不重选）；
        //   astrocs_adaptive→ 按 tile nominal geometric depth 解析（独立策略）。
        P2RejectionPlan rplan = group_plan;
        std::uint32_t nominal_for_resolve = (std::uint32_t)cfg.hips.size();
        if (!group_level) {
            P2RejectionPlanRequest rreq{};
            rreq.request = cfg.reject_method;
            rreq.nominal_contributors = depth;
            rreq.profile = cfg.reject_profile.c_str();
            rreq.underdetermined_n = cfg.reject_underdetermined_n;
            char rperr[160] = {0};
            if (p2_reject_plan_resolve(&rreq, &rplan, rperr, sizeof(rperr)) != 0) {
                log("rejection plan resolve failed: " + std::string(rperr));
                p2_upm_close(model);
                return 6;
            }
            nominal_for_resolve = depth;
        }
        rplan.normalization =
            (cfg.reject_normalization == "median_center")
                ? P2_NORMALIZE_MEDIAN_CENTER
                : (cfg.reject_normalization == "median_scale")
                      ? P2_NORMALIZE_MEDIAN_SCALE
                      : P2_NORMALIZE_NONE;
        rplan.normalization_floor = cfg.reject_normalization_floor;
        // typed params（cfg 为唯一默认源）
        rplan.sigma.lower_sigma = cfg.sigma_lower;
        rplan.sigma.upper_sigma = cfg.sigma_upper;
        rplan.sigma.max_iterations = cfg.sigma_max_iterations;
        rplan.winsorized.lower_sigma = cfg.winsor_lower;
        rplan.winsorized.upper_sigma = cfg.winsor_upper;
        rplan.winsorized.max_iterations = cfg.winsor_max_iterations;
        rplan.averaged.lower_sigma = cfg.avg_lower;
        rplan.averaged.upper_sigma = cfg.avg_upper;
        rplan.averaged.max_iterations = cfg.avg_max_iterations;
        rplan.linear_fit.lower = cfg.linfit_lower;
        rplan.linear_fit.upper = cfg.linfit_upper;
        rplan.linear_fit.max_iterations = cfg.linfit_max_iterations;
        rplan.esd.alpha = cfg.esd_alpha;
        rplan.esd.max_outliers = cfg.esd_max_outliers;
        rplan.percentile.low_fraction = cfg.pct_low_fraction;
        rplan.percentile.high_fraction = cfg.pct_high_fraction;
        rplan.median_sigma.lower_sigma = cfg.medsig_lower;
        rplan.median_sigma.upper_sigma = cfg.medsig_upper;
        rplan.median_sigma.max_iterations = cfg.medsig_max_iterations;
        rplan.minmax.reject_low_count = cfg.minmax_low_count;
        rplan.minmax.reject_high_count = cfg.minmax_high_count;
        rplan.minmax.min_kept = cfg.minmax_min_kept;
        log("tile " + std::to_string(tile_ipix) + " rejection resolved: " +
            std::string(p2_rejection_semantic_id(rplan.method)) +
            " nominal=" + std::to_string(nominal_for_resolve) +
            " (tile_depth=" + std::to_string(depth) + ") profile=" +
            cfg.reject_profile + " norm=" + cfg.reject_normalization +
            " min_n=" + std::to_string(rplan.minimum_n) +
            " underdetermined_n=" +
            std::to_string(rplan.underdetermined_n));
        resolved_methods[depth] = p2_rejection_semantic_id(rplan.method);

        // 仅显式 sigma（robust_mad_clip）可走 ACR 块路径（同一 contract）
        const bool use_acr_block =
            acr_reg != nullptr && rplan.method == P2_REJECT_SIGMA &&
            cfg.acr_route != "cpu";
        if (use_acr_block && gpu_exec == nullptr) {
            bridge::ensure_bridge_loaded();
            if (bridge::api().loaded()) {
                const char* gerr = nullptr;
                gpu_exec = bridge::api().executor_create(0, 1u << 22,
                                                         1u << 18, &gerr);
                gpu_ready = (gpu_exec != nullptr);
                if (gpu_ready) bridge::set_tls_handle(gpu_exec);
            }
        }
        log("tile " + std::to_string(tile_ipix) + " ACR block: enabled=" +
            std::to_string(use_acr_block) + " gpu=" +
            std::to_string(gpu_ready));

        // ---- R3：真实 N_B + planner 计算 chunk_pixels（micro-chunk 执行）----
        P2BlockPlannerInput bp{};
        bp.output_pixels = n_leaf;
        bp.covering_frames = depth;              // 当前 tile 真实覆盖帧数
        bp.precision = cfg.precision;
        bp.memory_limit_bytes = cfg.memory_limit_mb * 1024ull * 1024ull;
        bp.safety_factor = 0.75;
        bp.scratch_bytes_per_sample = 8;
        bp.scratch_bytes_per_pixel = 8;
        bp.fixed_overhead = 1ull << 22;
        P2BlockPlan plan{};
        p2_block_plan(&bp, &plan);
        if (plan.status == 1) {
            log("block unfeasible: " + std::string(plan.error));
            p2_upm_close(model);
            return 6;
        }
        const std::uint64_t chunk_pixels =
            std::min<std::uint64_t>(n_leaf,
                                    std::max<std::uint64_t>(1, plan.block_pixels));
        const std::uint64_t n_chunk =
            (n_leaf + chunk_pixels - 1) / chunk_pixels;
        log("tile " + std::to_string(tile_ipix) + " N_B=" +
            std::to_string(depth) + " chunk_px=" +
            std::to_string(chunk_pixels) + " n_chunk=" +
            std::to_string(n_chunk) + " est_peak=" +
            std::to_string(plan.estimated_peak_bytes) +
            " working_bytes=" +
            std::to_string(
                // 实际分配的工作缓冲（非 RSS）：cal+supv、ACR 暂存、
                // tile 读取缓冲、输出缓冲
                depth * chunk_pixels * sizeof(double) * 2 +
                (use_acr_block
                     ? chunk_pixels * depth * sizeof(float) * 2 +
                           depth * 64 * sizeof(float) +
                           chunk_pixels * sizeof(float) * 4
                     : 0) +
                512 * 512 * sizeof(float) * 2 +
                n_leaf * (sizeof(float) * 2 + sizeof(double) * 2 +
                          sizeof(std::uint8_t))));

        // 每 chunk 的工作缓冲（按 chunk_pixels×depth，非 262144×all_frames；
        // V16：展平为单块 frame-major 连续缓冲，供统一 Eligibility collector）
        std::vector<double> cal((std::size_t)depth * chunk_pixels);
        std::vector<double> supv((std::size_t)depth * chunk_pixels);
    std::vector<std::vector<std::uint64_t>> chunk_leaves(n_chunk);
    std::vector<std::uint64_t> local_lut(n_leaf);
        for (std::uint64_t i = 0; i < n_leaf; ++i)
            local_lut[i] = astrocs::healpix::fits_index_to_nested_local(
                i, 9u, 512u);
        for (std::uint64_t c = 0; c < n_chunk; ++c) {
            const std::uint64_t p0 = c * chunk_pixels;
            const std::uint64_t p1 =
                std::min<std::uint64_t>(p0 + chunk_pixels, n_leaf);
            chunk_leaves[c].resize(p1 - p0);
            for (std::uint64_t i = p0; i < p1; ++i)
                chunk_leaves[c][i - p0] =
                    (tile_ipix << 18) + local_lut[(std::size_t)i];
        }
        std::vector<double> stack(depth), weights(depth), support_v(depth);
        std::vector<std::uint8_t> acc(depth);
        std::vector<std::uint64_t> fid_stack(depth);
        std::vector<std::uint8_t> reasons(depth);
        std::vector<std::uint64_t> frame_seq(depth);
        for (std::uint32_t s = 0; s < depth; ++s) frame_seq[s] = frames[s];

        // 全 tile 临时读取缓冲（512×512×2×4B，固定小）
        std::vector<float> t_sig(512 * 512), t_sup(512 * 512);

        if (use_acr_block) {
            // compact per-cell SNR（P0-10/R8：与 frames[s] 一一对应且按
            // control cell 提供局部 SNR；缺失 cell → 整帧 median fallback）
            const int grid = 8;
            std::vector<float> snr_compact(depth * grid * grid);
            for (std::uint32_t s = 0; s < depth; ++s) {
                const std::uint64_t fid =
                    frame_id_cache[frames[s]];
                const double fb = frame_snr[frames[s]];
                for (int gy = 0; gy < grid; ++gy)
                    for (int gx = 0; gx < grid; ++gx) {
                        const auto key =
                            std::make_tuple(fid, tile_ipix, gx, gy);
                        const auto sit = local_snr_map.find(key);
                        if (sit != local_snr_map.end()) {
                            snr_compact[(std::size_t)(s * grid * grid +
                                                      gy * grid + gx)] =
                                (float)sit->second;
                            ++local_snr_used;
                        } else {
                            snr_compact[(std::size_t)(s * grid * grid +
                                                      gy * grid + gx)] =
                                (float)fb;
                            ++frame_snr_fallback;
                        }
                    }
            }
            std::vector<float> frames_f32(chunk_pixels * depth);
            std::vector<float> sup_f32(chunk_pixels * depth);
            std::vector<float> out_sig_f32(chunk_pixels),
                out_sup_f32(chunk_pixels), out_rej_f32(chunk_pixels),
                out_valid_f32(chunk_pixels);
            for (std::uint64_t c = 0; c < n_chunk; ++c) {
                const std::uint64_t p0 = c * chunk_pixels;
                const std::uint64_t p1 = std::min<std::uint64_t>(
                    p0 + chunk_pixels, n_leaf);
                const std::uint64_t cnt = p1 - p0;
                // 读每帧 tile 并提取 chunk 段 + UPM 空间校准
                for (std::uint32_t s = 0; s < depth; ++s) {
                    const std::uint32_t f = frames[s];
                    if (aio_hips_read_tile_f32(sig[f], tile_ipix,
                                               t_sig.data()) != 0 ||
                        aio_hips_read_tile_f32(sup[f], tile_ipix,
                                               t_sup.data()) != 0) {
                        log("tile read failed");
                                    return 6;
                    }
                    std::vector<double> cal_v(cnt), sup_v(cnt);
                    for (std::uint64_t i = 0; i < cnt; ++i) {
                        const std::uint64_t g = p0 + i;
                        cal_v[i] = (double)t_sig[(std::size_t)g];
                        sup_v[i] = (double)t_sup[(std::size_t)g];
                    }
                    std::vector<double> out_v(cnt);
                    p2_upm_calibrate_block(
                        model, frame_id_cache[f],
                        chunk_leaves[c].data(), cal_v.data(), out_v.data(),
                        cnt);
                    for (std::uint64_t i = 0; i < cnt; ++i) {
                        frames_f32[(size_t)s * chunk_pixels + i] =
                            (float)out_v[i];
                        sup_f32[(size_t)s * chunk_pixels + i] =
                            (float)sup_v[i];
                    }
                }
                std::fill(out_sig_f32.begin(), out_sig_f32.end(), 0.0f);
                std::fill(out_sup_f32.begin(), out_sup_f32.end(), 0.0f);
                std::fill(out_rej_f32.begin(), out_rej_f32.end(), 0.0f);
                std::fill(out_valid_f32.begin(), out_valid_f32.end(), 0.0f);
                astro::compute::KernelInvocation inv;
                inv.id = astro::compute::phase2::kOpMosaicReject;
                inv.domain = astro::compute::WorkDomain{0, cnt};
                inv.buffers.add(0, out_sig_f32.data(), cnt, 1,
                                astro::compute::BufferRole::Output);
                inv.buffers.add(1, frames_f32.data(), cnt * depth, 1,
                                astro::compute::BufferRole::Input);
                inv.buffers.add(2, sup_f32.data(), cnt * depth, 1,
                                astro::compute::BufferRole::Input);
                inv.buffers.add(3, snr_compact.data(),
                                depth * grid * grid, 1,
                                astro::compute::BufferRole::Input);
                inv.buffers.add(4, out_sup_f32.data(), cnt, 1,
                                astro::compute::BufferRole::Output);
                inv.buffers.add(5, out_rej_f32.data(), cnt, 1,
                                astro::compute::BufferRole::Output);
                inv.buffers.add(6, out_valid_f32.data(), cnt, 1,
                                astro::compute::BufferRole::Output);
                astro::compute::append_scalar(inv.scalars, std::size_t{cnt});
                astro::compute::append_scalar(inv.scalars, std::size_t{depth});
                astro::compute::append_scalar(inv.scalars,
                                              int{rplan.method});
                astro::compute::append_scalar(inv.scalars,
                    static_cast<int>(rplan.underdetermined_n));
                astro::compute::append_scalar(inv.scalars,
                                              rplan.sigma.lower_sigma);
                astro::compute::append_scalar(inv.scalars,
                                              rplan.sigma.upper_sigma);
                astro::compute::append_scalar(inv.scalars,
                                              int{rplan.sigma.max_iterations});
                astro::compute::append_scalar(inv.scalars,
                                              std::size_t{p0});  // chunk tile 偏移
                try {
                    if (gpu_ready && acr_reg->cuda.has_value()) {
                        (*acr_reg->cuda)(inv, nullptr);
                    } else {
                        acr_reg->legacy_parallel(inv, nullptr);
                    }
                } catch (const std::exception& e) {
                    log("ACR block failed, fallback CPU: " +
                        std::string(e.what()));
                    acr_reg->legacy_parallel(inv, nullptr);
                }
                for (std::uint64_t i = 0; i < cnt; ++i) {
                    const std::uint64_t p = p0 + i;
                    const float nv = out_valid_f32[i];
                    const bool ok = out_sup_f32[i] > 0.0f;
                    valid[p] = ok ? 1 : 0;
                    const double area =
                        ok ? (double)out_sup_f32[i] * A_cell : 0.0;
                    const double flux =
                        ok ? (double)out_sig_f32[i] * area : 0.0;
                    if (cfg.precision) {
                        fluxD[p] = flux;
                        areaD[p] = area;
                    } else {
                        fluxF[p] = (float)flux;
                        areaF[p] = (float)area;
                    }
                    if (ok) ++total_pixels;
                    total_rejected += (std::uint64_t)out_rej_f32[i];
                    if (nv <= 0.0f) { ++dbg_zero_px; ++px_depth_0; }
                    else if (nv <= (float)rplan.underdetermined_n ||
                             nv < (float)rplan.minimum_n) {
                        if (nv == 1.0f) ++px_depth_1;
                        else ++px_depth_ge_2;
                        ++total_fallback;
                        ++dbg_fallback_px;
                        ++underdetermined_px;
                    } else {
                        ++px_depth_ge_2;
                        ++px_integrated;
                        ++dbg_reject_px;
                    }
                    if (out_rej_f32[i] > 0.0f)
                        ++reject_hist[(std::uint32_t)out_rej_f32[i]];
                }
            }
            // V12 (HIPS-IMG-002)：writer 约定 view 缓冲为 NESTED local 序
            // （V5 HIPS-IMG-001，与 drizzle 热路径一致）；stage2 集成缓冲为
            // FITS 行主序，写入前转换 buffer[i]=buf[fits_index(i)]，否则
            // tile 内像素被散射错排（表现为 16px 周期 comb/重复星点）。
            std::vector<float> flux_leaf(n_leaf), area_leaf(n_leaf);
            std::vector<std::uint8_t> valid_leaf(n_leaf);
            for (std::uint64_t i = 0; i < n_leaf; ++i) {
                const std::uint64_t fi =
                    astrocs::healpix::nested_local_to_fits_index(i, 9u, 512u);
                flux_leaf[(std::size_t)i] =
                    cfg.precision ? (float)fluxD[(std::size_t)fi]
                                  : fluxF[(std::size_t)fi];
                area_leaf[(std::size_t)i] =
                    cfg.precision ? (float)areaD[(std::size_t)fi]
                                  : areaF[(std::size_t)fi];
                valid_leaf[(std::size_t)i] = valid[(std::size_t)fi];
            }
            AstroSphereTileView view{};
            std::memset(&view, 0, sizeof(view));
            view.parent_ipix = tile_ipix;
            view.leaf_order = (std::uint32_t)(target_order + 9);
            view.width = 512;
            view.data_type = dtype;
            view.flux_sum = flux_leaf.data();
            view.covered_area = area_leaf.data();
            view.valid_mask = valid_leaf.data();
            if (aio_hips_write_signal_support_tile(ps, &view) != 0) {
                log("hips tile write failed: " +
                    std::string(aio_hips_last_error()));
                aio_hips_abort(ps);
                p2_upm_close(model);
                    return 6;
            }
            ++tiles_written;
            continue;
        }

        // CPU reference 路径：逐 chunk（micro-chunk）处理
        for (std::uint64_t c = 0; c < n_chunk; ++c) {
            const std::uint64_t p0 = c * chunk_pixels;
            const std::uint64_t p1 =
                std::min<std::uint64_t>(p0 + chunk_pixels, n_leaf);
            const std::uint64_t cnt = p1 - p0;
            // 读每帧 tile 并提取 chunk 段 + UPM 空间校准
            for (std::uint32_t s = 0; s < depth; ++s) {
                const std::uint32_t f = frames[s];
                if (aio_hips_read_tile_f32(sig[f], tile_ipix,
                                           t_sig.data()) != 0 ||
                    aio_hips_read_tile_f32(sup[f], tile_ipix,
                                           t_sup.data()) != 0) {
                    log("tile read failed");
                            return 6;
                }
                for (std::uint64_t i = 0; i < cnt; ++i) {
                    const std::uint64_t g = p0 + i;
                    cal[(std::size_t)s * chunk_pixels + i] =
                        (double)t_sig[(std::size_t)g];
                    supv[(std::size_t)s * chunk_pixels + i] =
                        (double)t_sup[(std::size_t)g];
                }
                std::vector<double> out_v(cnt);
                p2_upm_calibrate_block(
                    model, frame_id_cache[f], chunk_leaves[c].data(),
                    cal.data() + (std::size_t)s * chunk_pixels,
                    out_v.data(), cnt);
                for (std::uint64_t i = 0; i < cnt; ++i)
                    cal[(std::size_t)s * chunk_pixels + i] = out_v[i];
            }
            for (std::uint64_t i = 0; i < cnt; ++i) {
                const std::uint64_t p = p0 + i;
                // V16：统一 EligibilityPolicy（与 ACR/compat 同一 collector；
                // quality 为 control 级，像素级无 quality 数组 → nullptr）
                P2EligibilityGatherInput gin{};
                gin.values = cal.data();
                gin.value_stride = chunk_pixels;
                gin.value_dtype = 1;
                gin.support = supv.data();
                gin.support_stride = chunk_pixels;
                gin.frame_ids = frame_seq.data();
                gin.count = depth;
                gin.pixel = (std::uint32_t)i;
                gin.support_threshold = 0.0;
                P2EligibilityGatherOutput gout{};
                gout.values = stack.data();
                gout.support = support_v.data();
                gout.frame_ids = fid_stack.data();
                std::uint32_t n_valid = 0;
                gout.eligible_count = &n_valid;
                if (p2_collect_candidate_stack(&gin, &gout) != 0) {
                    log("eligibility gather failed");
                    p2_upm_close(model);
                    return 6;
                }
                // R8：局部 SNR（control cell 级）；缺失 → 整帧 median fallback
                for (std::uint32_t s = 0; s < n_valid; ++s) {
                    const std::uint64_t fid =
                        frame_id_cache[fid_stack[s]];
                    const int px = (int)(p % 512);
                    const int py = (int)(p / 512);
                    const auto key = std::make_tuple(
                        fid, tile_ipix, px / 64, py / 64);
                    const auto sit = local_snr_map.find(key);
                    double snr_v;
                    if (sit != local_snr_map.end()) {
                        snr_v = sit->second;
                        ++local_snr_used;
                    } else {
                        snr_v = frame_snr[fid_stack[s]];
                        ++frame_snr_fallback;
                    }
                    weights[s] = support_v[s] * snr_v * snr_v;
                }
                double signal_out = 0.0, support_out = 0.0;
                int st = 1;
                if (n_valid == 0) {
                    ++dbg_zero_px;
                    ++px_depth_0;
                } else {
                    if (n_valid == 1) ++px_depth_1;
                    else ++px_depth_ge_2;
                    // V15：explicit plan kernel（auto 已在 planning 层解析）
                    P2CandidateStack cstack{};
                    cstack.values = stack.data();
                    cstack.weights = weights.data();
                    cstack.frame_ids = fid_stack.data();
                    cstack.count = n_valid;
                    cstack.data_type = 1;
                    P2RejectionDecision rdec{};
                    rdec.reasons = reasons.data();
                    if (p2_reject_stack_ex(&cstack, &rplan, &rdec) != 0) {
                        log("reject kernel failed");
                        p2_upm_close(model);
                                    return 6;
                    }
                    for (std::uint32_t s = 0; s < n_valid; ++s) {
                        acc[s] =
                            (rdec.reasons[s] == P2_REASON_ACCEPTED ||
                             rdec.reasons[s] == P2_REASON_UNDERDETERMINED)
                                ? 1 : 0;
                    }
                    total_rejected +=
                        rdec.rejected_low + rdec.rejected_high;
                    ++reject_hist[rdec.rejected_low + rdec.rejected_high];
                    if (rdec.status == P2_STATUS_UNDERDETERMINED) {
                        ++total_fallback;
                        ++dbg_fallback_px;
                        ++underdetermined_px;
                    } else {
                        ++px_integrated;
                        ++dbg_reject_px;
                    }
                    P2PixelStack pi{};
                    pi.values = stack.data();
                    pi.weights = weights.data();
                    pi.support = support_v.data();
                    pi.accepted = acc.data();
                    pi.count = n_valid;
                    pi.weight_mode = cfg.weight_mode;
                    P2PixelResult pr{};
                    p2_integrate_pixel(&pi, &pr);
                    st = pr.status;
                    signal_out = pr.signal;
                    support_out = 0.0;
                    for (std::uint32_t s = 0; s < n_valid; ++s)
                        if (acc[s])
                            support_out = std::max(support_out, support_v[s]);
                }
                const bool ok = (st == 0);
                valid[p] = ok ? 1 : 0;
                const double area = ok ? support_out * A_cell : 0.0;
                const double flux = ok ? signal_out * area : 0.0;
                if (cfg.precision) {
                    fluxD[p] = flux;
                    areaD[p] = area;
                } else {
                    fluxF[p] = (float)flux;
                    areaF[p] = (float)area;
                }
                if (ok) ++total_pixels;
            }
        }
        // V12 (HIPS-IMG-002)：同 ACR 路径，FITS 行主序 -> NESTED local 序
        std::vector<float> flux_leaf(n_leaf), area_leaf(n_leaf);
        std::vector<std::uint8_t> valid_leaf(n_leaf);
        for (std::uint64_t i = 0; i < n_leaf; ++i) {
            const std::uint64_t fi =
                astrocs::healpix::nested_local_to_fits_index(i, 9u, 512u);
            flux_leaf[(std::size_t)i] =
                cfg.precision ? (float)fluxD[(std::size_t)fi]
                              : fluxF[(std::size_t)fi];
            area_leaf[(std::size_t)i] =
                cfg.precision ? (float)areaD[(std::size_t)fi]
                              : areaF[(std::size_t)fi];
            valid_leaf[(std::size_t)i] = valid[(std::size_t)fi];
        }
        AstroSphereTileView view{};
        std::memset(&view, 0, sizeof(view));
        view.parent_ipix = tile_ipix;
        view.leaf_order = (std::uint32_t)(target_order + 9);
        view.width = 512;
        view.data_type = dtype;
        view.flux_sum = flux_leaf.data();
        view.covered_area = area_leaf.data();
        view.valid_mask = valid_leaf.data();
        if (aio_hips_write_signal_support_tile(ps, &view) != 0) {
            log("hips tile write failed: " + std::string(aio_hips_last_error()));
            aio_hips_abort(ps);
            p2_upm_close(model);
            return 6;
        }
        ++tiles_written;
    }
    if (aio_hips_finalize(ps) != 0) {
        log("hips finalize failed: " + std::string(aio_hips_last_error()));
        p2_upm_close(model);
        return 6;
    }
    for (std::size_t i = 0; i < cfg.hips.size(); ++i) {
        if (sig[i]) aio_hips_close(sig[i]);
        if (sup[i]) aio_hips_close(sup[i]);
    }
    log("HiPS mosaic written: tiles=" + std::to_string(tiles_written) +
        " pixels=" + std::to_string(total_pixels) +
        " rejected=" + std::to_string(total_rejected) +
        " fallback=" + std::to_string(total_fallback) +
        " [reject_px=" + std::to_string(dbg_reject_px) +
        " fb_px=" + std::to_string(dbg_fallback_px) +
        " zero_px=" + std::to_string(dbg_zero_px) + "]");
    mark("tiles_process");

    // ---- HIPS_VERIFY（AIO reader 回读） ----
    {
        AioHipsDataset* vd = aio_hips_open(cfg.out_hips.c_str(), AIO_HIPS_RD_SIGNAL);
        if (!vd) {
            log("verify failed: " + std::string(aio_hips_reader_last_error()));
            p2_upm_close(model);
            return 7;
        }
        const int nt = aio_hips_tile_count(vd);
        log("verify: signal tiles=" + std::to_string(nt));
        aio_hips_close(vd);
        AioHipsDataset* spd = aio_hips_open(cfg.out_hips.c_str(), AIO_HIPS_RD_SUPPORT);
        if (spd) {
            const int ns = aio_hips_tile_count(spd);
            log("verify: support tiles=" + std::to_string(ns));
            aio_hips_close(spd);
        }
    }

    p2_upm_close(model);
    const auto t_end = std::chrono::steady_clock::now();
    const double secs =
        std::chrono::duration<double>(t_end - t_start).count();
    log("stage2 done in " + std::to_string(secs) + " s");

    // ---- G11 诊断 JSON（rejection 统计） ----
    if (cfg.diagnostics) {
        // V7 P7-2：overlap topology（control depth / pixel depth）
        std::map<std::uint64_t, std::set<std::uint64_t>> ctrl_frames;
        for (const auto& o : obs) ctrl_frames[o.control_id].insert(o.frame_id);
        std::uint64_t ctrl_depth_1 = 0, ctrl_depth_ge_2 = 0;
        for (const auto& kv : ctrl_frames) {
            if (kv.second.size() == 1)
                ++ctrl_depth_1;
            else
                ++ctrl_depth_ge_2;
        }
        nlohmann::json diag;
        diag["stage2_version"] = 1;
        diag["input_frames"] = (std::uint32_t)cfg.hips.size();
        diag["component_count"] = minfo.component_count;
        diag["observations"] = n_obs;
        diag["unique_controls"] = minfo.control_count;
        diag["controls_with_depth_1"] = ctrl_depth_1;
        diag["controls_with_depth_ge_2"] = ctrl_depth_ge_2;
        diag["tiles_written"] = tiles_written;
        diag["output_pixels"] = total_pixels;
        diag["rejected_samples"] = total_rejected;
        diag["fallback_pixels"] = total_fallback;
        diag["pixels_depth_0"] = px_depth_0;
        diag["pixels_depth_1"] = px_depth_1;
        diag["pixels_depth_ge_2"] = px_depth_ge_2;
        diag["reject_normalization"] = cfg.reject_normalization;
        diag["reject_profile"] = cfg.reject_profile;
        diag["reject_group_level"] = (int)group_level;
        diag["reject_group_method"] = group_method;
        diag["integrated_pixels"] = px_integrated;
        diag["quality_fallback_unknown"] = quality_unknown;
        diag["local_snr_used"] = local_snr_used;
        diag["frame_snr_median_fallback"] = frame_snr_fallback;
        diag["local_snr_unavailable_controls"] = local_snr_unavailable;
        diag["upm_sigma_floor"] = cfg.sigma_floor;
        diag["upm_support_power"] = cfg.support_power;
        diag["zero_coverage_pixels"] = dbg_zero_px;
        diag["underdetermined_pixels"] = underdetermined_px;
        diag["reject_method"] = cfg.reject_method;
        diag["reject_underdetermined_n"] = cfg.reject_underdetermined_n;
        nlohmann::json rm = nlohmann::json::object();
        for (const auto& kv : resolved_methods)
            rm[std::to_string(kv.first)] = kv.second;
        diag["rejection_resolved_methods"] = rm;
        diag["rejection_samples_per_pixel"] = reject_hist;
        diag["model_hash"] = std::string(minfo.model_hash);
        diag["runtime_seconds"] = secs;
        std::ofstream df(cfg.out_hips + "/diagnostics.json");
        if (df) df << diag.dump(2);
        log("diagnostics written: " + cfg.out_hips + "/diagnostics.json");
    }
    return 0;
}
