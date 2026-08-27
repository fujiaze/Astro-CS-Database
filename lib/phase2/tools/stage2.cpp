// lib/phase2/tools/stage2.cpp — Phase2 正式入口（W8 完整 CPU 链闭合）
//
// 用法：astrocs-stage2 <stage2.json>（唯一参数，禁止长串 CLI 科学参数）
//
// 流程（ 34A532A2...B2EB308 EXECUTION_ORDER）：
// DISCOVER → VALIDATE → COVERAGE_UNION → CONTROL_SAMPLE → UPM_FIT →
// UPM_PERSIST → BLOCK_PLAN → BLOCK_CALIBRATE → REJECT_INTEGRATE →
// HIPS_WRITE → HIPS_VERIFY
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
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#if defined(_WIN32) && defined(_MSC_VER)
#include <windows.h>
#include <eh.h>
#endif

extern "C" {
#include "aio_hips.h"
#include "aio_hips_reader.h"
#include "aio_upm.h"
}

namespace {

std::ofstream g_log;

void log(const std::string& msg) {
    std::fprintf(stdout, "[stage2] %s\n", msg.c_str());
    std::fflush(stdout);
    if (g_log.is_open()) { g_log << msg << "\n"; g_log.flush(); }
}
inline void log_flush() { std::fflush(stdout); if (g_log.is_open()) g_log.flush(); }


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
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y%m%d", &tm);
    return std::string(buf);
}

} // namespace

int main(int argc, char** argv) {
#if defined(_WIN32) && defined(_MSC_VER)
    // SEH→C++ 异常：AV 在 p2_sample 首次进入内触发时透出为 std::exception（仅 MSVC）
    _set_se_translator([](unsigned int code, struct _EXCEPTION_POINTERS* ep){
        char buf[128]; std::snprintf(buf,sizeof(buf),"SEH 0x%08X",code);
        std::fprintf(stderr,"[stage2] SEH 0x%08X at %p\n",code, ep?ep->ExceptionRecord->ExceptionAddress:nullptr); std::fflush(stderr);
        throw std::runtime_error(buf);
    });
    SetUnhandledExceptionFilter([](struct _EXCEPTION_POINTERS* ep)->LONG{
        std::fprintf(stderr,"[stage2] Unhandled SEH 0x%08lX at %p\n",
            ep?ep->ExceptionRecord->ExceptionCode:0,
            ep?ep->ExceptionRecord->ExceptionAddress:nullptr); std::fflush(stderr);
        if(g_log.is_open()){ g_log<<"Unhandled SEH\n"; g_log.flush(); }
        return EXCEPTION_EXECUTE_HANDLER;
    });
#endif
    // 全局未捕获异常透出（将 0xC0000005/SEH 转为可读日志，而非 EC:-1）
    try {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: astrocs-stage2 <stage2.json> [--cpu-workers N] [--io-workers N] [--gpu-route cpu|auto|cuda] [--deterministic 0|1]\n");
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
    // CON-002 CLI override of global worker budget (overrides config execution block)
    for (int ai = 2; ai + 1 < argc; ai += 2) {
        const std::string k = argv[ai];
        const std::string v = argv[ai + 1];
        try {
            if (k == "--cpu-workers") { const int n = std::stoi(v); if (n < 1 || n > 1024) { std::fprintf(stderr, "invalid --cpu-workers\n"); return 2; } cfg.exec.cpu_workers = n; }
            else if (k == "--io-workers") { const int n = std::stoi(v); if (n < 1 || n > 1024) { std::fprintf(stderr, "invalid --io-workers\n"); return 2; } cfg.exec.io_workers = n; }
            else if (k == "--gpu-route") { if (v != "cpu" && v != "auto" && v != "cuda") { std::fprintf(stderr, "invalid --gpu-route\n"); return 2; } cfg.exec.gpu_route = v; }
            else if (k == "--deterministic") { const int d = std::stoi(v); if (d != 0 && d != 1) { std::fprintf(stderr, "invalid --deterministic\n"); return 2; } cfg.exec.deterministic = (d == 1); }
            else { std::fprintf(stderr, "unknown option: %s\n", k.c_str()); return 2; }
        } catch (...) { std::fprintf(stderr, "bad value for %s\n", k.c_str()); return 2; }
    }

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
    std::fprintf(stderr, "[stage2] coverage done n_union=%llu target_order=%d\n",
        (unsigned long long)cov.n_union_cells, target_order); std::fflush(stderr);
    log_flush();
    mark("coverage");

    // frame_id 缓存（payload 敏感，DISCOVER 阶段一次计算）+
    // input manifest hash = canonical(frame identity + 关键元数据)
    std::vector<std::uint64_t> frame_id_cache(cfg.hips.size());
    std::vector<std::pair<std::uint64_t, std::string>> manifest_entries;
    for (std::size_t i = 0; i < cfg.hips.size(); ++i) {
        const std::uint64_t fid = p2_frame_id(cfg.hips[i].c_str());
        if (fid == 0) {
            log("frame_id 0 invalid: " + cfg.hips[i] + " (p2_frame_id failed: hash/open exception)");
            std::fprintf(stderr, "[stage2] frame_id 0 invalid: %s (p2_frame_id failed)\n", cfg.hips[i].c_str());
            std::fflush(stderr);
            return 4;
        }
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
    std::fprintf(stderr, "[stage2] manifest %s, before sampler probe n=%zu cells=%llu\n",
        input_manifest_hash.c_str(), cfg.hips.size(), (unsigned long long)cov.n_union_cells); std::fflush(stderr);
    log_flush();
    // 帧级 SNR median（whole-frame fallback 源；frame_id → 值映射）
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
    // background-clean 参数
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
    // CON-004: 并行 worker 预算唯一来源 = ExecutionOptions (CON-002)
    sccfg.cpu_workers = cfg.exec.cpu_workers;
    std::uint64_t n_obs = 0, n_ctrl = 0;
    char serr[512] = {0};
    P2SampleStats sstats{};
    // 714*32 规模：vector reserve 后检查 n_obs/n_ctrl 是否异常
    try {
    if (p2_sample_controls_cached(&cov, paths.data(), frame_id_cache.data(), &sccfg, nullptr, 0,
                           &n_obs, &n_ctrl, &sstats, nullptr, 0,
                           serr, sizeof(serr)) != 0) {
        log("sampler error: " + std::string(serr)); log_flush();
        return 4;
    }
    } catch (const std::exception& e) {
        log(std::string("sampler probe exception: ") + e.what()); log_flush();
        std::fprintf(stderr, "[stage2] sampler probe exception: %s\n", e.what());
        return 4;
    } catch (...) {
        log("sampler probe unknown exception"); log_flush();
        std::fprintf(stderr, "[stage2] sampler probe unknown exception\n");
        return 4;
    }
    log("sampler probe: n_obs=" + std::to_string(n_obs) + " n_ctrl=" + std::to_string(n_ctrl)); log_flush();
    // 边界：probe 后若 n_obs 为 0 且 diagnostics 需落盘，仍保证空 vector 不越界
    if (n_obs > (std::uint64_t)50 * 1000 * 1000 || n_ctrl > (std::uint64_t)10 * 1000 * 1000) {
        log("sampler probe counts unreasonable n_obs=" + std::to_string(n_obs) + " n_ctrl=" + std::to_string(n_ctrl)); log_flush();
        return 4;
    }
    std::vector<P2ControlObservation> obs; std::vector<P2ControlNode> ctrl_nodes;
    try { obs.resize((std::size_t)n_obs); ctrl_nodes.resize((std::size_t)n_ctrl); } catch (const std::exception& e) {
        log(std::string("sampler alloc failed: ") + e.what()); log_flush();
        return 4;
    }
    try {
    if (p2_sample_controls_cached(&cov, paths.data(), frame_id_cache.data(), &sccfg, obs.data(), n_obs,
                           &n_obs, &n_ctrl, &sstats, ctrl_nodes.data(),
                           ctrl_nodes.size(), serr, sizeof(serr)) != 0) {
        log("sampler error (fill): " + std::string(serr)); log_flush();
        return 4;
    }
    } catch (const std::exception& e) {
        log(std::string("sampler fill exception: ") + e.what()); log_flush();
        std::fprintf(stderr, "[stage2] sampler fill exception: %s\n", e.what());
        return 4;
    } catch (...) {
        log("sampler fill unknown exception"); log_flush();
        std::fprintf(stderr, "[stage2] sampler fill unknown exception\n");
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
    // accepted/rejected control 诊断（overlay 用）
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
        if (!ca_f) {
            log("V13 controls_accept write failed: " + cfg.out_hips + "/controls_accept.json");
            std::fprintf(stderr, "[stage2] controls_accept write failed\n"); std::fflush(stderr); log_flush();
        } else {
            ca_f.flush();
            if (!ca_f.good()) log("V13 controls_accept flush failed"); 
            std::error_code ec_ca;
            auto ca_sz = std::filesystem::file_size(cfg.out_hips + "/controls_accept.json", ec_ca);
            if (!ec_ca) log("[control_sample] V13 controls_accept size=" + std::to_string(ca_sz) + " bytes"); 
            log("V13 controls_accept written: " + cfg.out_hips +
                "/controls_accept.json"); log_flush();
        }
        // 磁盘可用诊断（后续 UPM dense 需 35-48GB）
        {
            std::error_code ec_sp0;
            auto sp0 = std::filesystem::space(cfg.out_hips, ec_sp0);
            if (!ec_sp0) log("[control_sample] disk avail after controls_accept=" + std::to_string(sp0.available) + " free=" + std::to_string(sp0.free));
        }
    }
    log("[control_sample] exit n_obs=" + std::to_string(n_obs) + " n_ctrl=" + std::to_string(n_ctrl) + " candidates=" + std::to_string(sstats.candidate_observations)); log_flush();
    mark("control_sample");
    log("[post_control] enter quality/snr maps"); log_flush();
    // quality fallback 统计（quality_flags==0 = QUALITY_FALLBACK_UNKNOWN）
    std::uint64_t quality_unknown = 0;
    for (const auto& o : obs)
        if (o.quality_flags == 0) ++quality_unknown;
    log("quality fallback unknown: " + std::to_string(quality_unknown) +
        " / " + std::to_string(n_obs)); log_flush();
    // 局部 SNR 映射（control cell 级 = 可证明的最近空间 catalogue
    // 区域）：(frame_id, tile, gx, gy) -> snr。像素权重优先局部，
    // 缺失才 fallback 整帧 SNR median 并计数。
    // 只有真实可用的局部 SNR 才进入 local_snr_map；无局部星点的
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
    // UPM 控制权重回退——无局部星点的观测用整帧 SNR median
    // （保持与像素级 fallback 相同策略；snr_available 位仍保留记录）。
    for (auto& o : obs) {
        if (!o.snr_available) {
            const auto it = frame_snr_by_id.find(o.frame_id);
            if (it != frame_snr_by_id.end()) o.snr = it->second;
        }
    }
    std::uint64_t local_snr_used = 0, frame_snr_fallback = 0;
    // 控制 cell 级 ivar (来自帧 ivar 产品)
    std::map<std::tuple<std::uint64_t, std::uint64_t, int, int>, double>
        local_ivar_map;
    std::uint64_t local_ivar_used = 0;
    for (const auto& o : obs) {
        if (o.ivar <= 0.0 || !std::isfinite(o.ivar)) continue;
        const std::uint64_t tile = o.leaf_ipix >> 18;
        const std::uint64_t local = o.leaf_ipix & ((1ULL << 18) - 1ULL);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, 9u, x, y);
        local_ivar_map[std::make_tuple(o.frame_id, tile, (int)(x / 64),
                                       (int)(y / 64))] = o.ivar;
    }
    log("local snr unavailable controls (fallback to frame median): " +
        std::to_string(local_snr_unavailable)); log_flush();
    log("[pre_upm] local_snr_map=" + std::to_string(local_snr_map.size()) + " local_ivar_map=" + std::to_string(local_ivar_map.size())); log_flush();
    log("[pre_upm] frame_snr_by_id=" + std::to_string(frame_snr_by_id.size()) + " obs=" + std::to_string(obs.size()) + " ctrl_nodes=" + std::to_string(ctrl_nodes.size())); log_flush();

    // ---- W4 UPM FIT ----
    // 生产共享 UPM 配置构造（与 gate 测试同一 path）
    log("[upm_fit] enter input_manifest_hash=" + input_manifest_hash + " obs=" + std::to_string(obs.size()) + " ctrl=" + std::to_string(ctrl_nodes.size())); log_flush();
    P2UpmBuildConfig mcfg =
        p2_stage2_make_upm_cfg(cfg, target_order,
                               input_manifest_hash.c_str());
    void* model = nullptr;
    if (p2_upm_build_geo(obs.data(), obs.size(), ctrl_nodes.data(),
                         ctrl_nodes.size(), &mcfg, &model) != 0) {
        log("UPM build failed");
        std::fprintf(stderr, "[stage2] UPM build failed (no detail)\n"); std::fflush(stderr);
        log_flush();
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
        log("[upm_persist] enter diagnostics=true, out=" + cfg.out_hips); log_flush();
        std::error_code ec_mk;
        std::filesystem::create_directories(cfg.out_hips, ec_mk);
        if (ec_mk) {
            log("UPM persist mkdir failed: " + ec_mk.message()); std::fprintf(stderr, "[stage2] mkdir failed: %s\n", ec_mk.message().c_str()); log_flush();
            p2_upm_close(model);
            return 5;
        }
        // 磁盘剩余检查：dense 预期 35-48GB (714*32*2MB fp64)，提前拒绝而非半写 EC:1
        {
            std::error_code ec_sp;
            auto sp = std::filesystem::space(cfg.out_hips, ec_sp);
            if (!ec_sp) {
                const std::uint64_t need = (std::uint64_t)714 * 32 * 512 * 512 * 8 + (1ull<<30); // +1GB 预留
                log("[upm_persist] disk avail=" + std::to_string(sp.available) + " need~" + std::to_string(need) + " bytes"); log_flush();
                if (sp.available < need) {
                    log("UPM persist disk space insufficient: avail=" + std::to_string(sp.available) + " need=" + std::to_string(need)); log_flush();
                    std::fprintf(stderr, "[stage2] disk insufficient avail=%llu need=%llu\n", (unsigned long long)sp.available, (unsigned long long)need); std::fflush(stderr);
                }
            }
        }
        log("[upm_persist] saving sparse json..."); log_flush();
        model_path = cfg.out_hips + "/upm_sparse.json";
        if (p2_upm_save(model, model_path.c_str()) != 0) {
            const char* e = aio_upm_last_error();
            log(std::string("UPM save failed: ") + (e ? e : "(no detail)")); std::fprintf(stderr, "[stage2] UPM save failed: %s\n", e ? e : "(no detail)"); log_flush();
            p2_upm_close(model);
            return 5;
        }
        log("[upm_persist] sparse saved, materializing dense cache..."); log_flush();
        const std::string cache = cfg.out_hips + "/upm_dense.cache";
        // 64-bit文件偏移诊断：714*32 规模下 offset 需 >2GB，fseek 必须为 fseeko/_fseeki64
        if (p2_upm_materialize_dense(model, target_order, cache.c_str()) != 0) {
            const char* e = aio_upm_last_error();
            log(std::string("UPM dense materialize failed: ") + (e ? e : "(no detail)")); std::fprintf(stderr, "[stage2] UPM dense materialize failed: %s\n", e ? e : "(no detail)"); std::fflush(stderr); log_flush();
            // 保留已写部分供复盘，不删除；但返回 EC 便于调用方感知
            p2_upm_close(model);
            return 5;
        }
        {
            std::error_code ec_sz;
            auto sz = std::filesystem::file_size(cache, ec_sz);
            if (!ec_sz) log("[upm_persist] dense cache size=" + std::to_string(sz) + " bytes");
        }
        log("UPM persisted: " + model_path + " + dense cache"); log_flush();
    } else {
        log("[upm_persist] skip (diagnostics disabled)"); log_flush();
    }
    mark("upm_persist");
    log("[block_plan] enter"); log_flush();

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
    if (plan.status != 0) {
        log(std::string("block plan failed: ") + (plan.error[0]?plan.error:"(unknown)")); std::fprintf(stderr, "[stage2] block plan failed status=%d\n", plan.status); log_flush();
        p2_upm_close(model);
        return 6;
    }
    log("block plan: tile_pixels=262144 est_peak=" +
        std::to_string(plan.estimated_peak_bytes) +
        " bytes micro_chunk=" + std::to_string(plan.micro_chunk_required)); log_flush();
    mark("block_plan");

    // ---- W8 REJECT + INTEGRATE + HIPS WRITE ----
    const int nside = 1 << (target_order + 9);
    const std::uint64_t n_leaf = 512ull * 512ull;
    const double A_cell =
        4.0 * 3.14159265358979323846 / (12.0 * (double)nside * (double)nside);
    const int dtype = cfg.precision ? AIO_HIPS_FLOAT64 : AIO_HIPS_FLOAT32;
    log("[hips_write] enter: nside=" + std::to_string(nside) + " dtype=" + std::to_string(dtype) + " frames=" + std::to_string(cfg.hips.size())); log_flush();
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
    // ivar 产品 (weight_mode=2 默认)
    // 整个 ivar 产品缺失时默认
    // → 显式 science/degraded 错误（无静默回退）；仅当显式配置
    // legacy_allow_weight_fallback=true 才降级 support 并计数标红。
    std::vector<AioHipsDataset*> ivr(cfg.hips.size(), nullptr);
    std::uint64_t ivar_product_missing = 0;
    if (cfg.weight_mode == 2) {
        for (std::size_t i = 0; i < cfg.hips.size(); ++i) {
            ivr[i] = aio_hips_open(cfg.hips[i].c_str(), AIO_HIPS_RD_IVAR);
            if (!ivr[i]) {
                ++ivar_product_missing;
                log("frame " + std::to_string(i) +
                    " 无 ivar 产品");
            }
        }
    }
    if (ivar_product_missing > 0 && cfg.weight_mode == 2) {
        if (!cfg.legacy_allow_weight_fallback) {
            log("weight_policy=ivar 且 ivar 产品缺失 " +
                std::to_string(ivar_product_missing) + " 帧 → 显式科学错误 "
                "(legacy_allow_weight_fallback=false)；拒绝继续，防止在 "
                "非逆方差语义下冒充 ivar coadd");
            for (std::size_t i = 0; i < ivr.size(); ++i)
                if (ivr[i]) aio_hips_close(ivr[i]);
            p2_upm_close(model);
            return 7;
        }
        log("legacy_allow_weight_fallback=true：ivar 缺失帧积分权重降级 "
            "support（diagnostics 标红）");
    }
    {
        std::error_code ec_sp2;
        auto sp2 = std::filesystem::space(cfg.out_hips, ec_sp2);
        if (!ec_sp2) {
            log("[hips_write] disk avail before product_begin=" + std::to_string(sp2.available)); log_flush();
        }
        std::error_code ec_exists;
        if (std::filesystem::exists(cfg.out_hips, ec_exists) && !std::filesystem::is_directory(cfg.out_hips, ec_exists)) {
            log("hips out path exists but not directory: " + cfg.out_hips); log_flush();
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
        const char* e = aio_hips_last_error();
        log(std::string("hips begin failed: ") + (e ? e : "(no detail)")); std::fprintf(stderr, "[stage2] hips begin failed: %s\n", e ? e : "(no detail)"); log_flush();
        p2_upm_close(model);
        return 6;
    }
    log("[hips_write] product_begin ok, entering tile loop n_union=" + std::to_string(cov.n_union_cells)); log_flush();

    std::uint64_t total_pixels = 0, total_rejected = 0, total_fallback = 0;
    std::uint64_t large_scale_grown = 0;   // grow 新增拒绝样本数
    std::uint64_t dbg_reject_px = 0, dbg_fallback_px = 0, dbg_zero_px = 0;
    std::uint64_t underdetermined_px = 0;  // REJECTION_UNDERDETERMINED
    std::uint64_t px_depth_0 = 0;  // mutually exclusive depth 诊断
    std::map<std::uint32_t, std::uint64_t> reject_hist;  // 每像素拒绝样本数分布
    std::map<std::uint32_t, std::string> resolved_methods;  // nominal depth → semantic id
    // P7-2：overlap topology 诊断
    std::uint64_t px_depth_1 = 0, px_depth_ge_2 = 0, px_integrated = 0;
    std::uint64_t tiles_written = 0;
    std::vector<std::uint8_t> valid(n_leaf);
    std::vector<float> fluxF(n_leaf), areaF(n_leaf);
    std::vector<double> fluxD(n_leaf), areaD(n_leaf);
    std::vector<float> t_sig_probe(512 * 512);
    const std::uint32_t nb = (std::uint32_t)cfg.hips.size();

    // ---- ACR 路由：按 tile 解析后的显式方法决定是否走
    // KernelRegistry（仅 robust_mad_clip/sigma）；GPU 可用则 CUDA，
    // 否则 CPU legacy（同一科学语义）。 ----
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* acr_reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    namespace bridge = astro::compute::cuda::bridge;
    bool gpu_ready = false;
    void* gpu_exec = nullptr;

    // wbpp_current = integration-group level 一次解析（nominal = 全部
    // 独立 exposure 数）；astrocs_adaptive 在 tile 层按 nominal depth 解析。
    const bool group_level =
        (cfg.reject_profile == "wbpp_2_9_1");
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
        log("wbpp_2_9_1 group plan: nominal=" +
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

        // planning 层解析 rejection
        // wbpp_2_9_1 → 使用 group-level 一次解析结果（tile 不重选）；
        // astrocs_adaptive→ 按 tile nominal geometric depth 解析（独立策略）。
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
            (cfg.reject_normalization == "astrocs_median_center_v1")
                ? P2_NORMALIZE_MEDIAN_CENTER
                : (cfg.reject_normalization == "astrocs_median_scale_v1")
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
        // large_scale_rejection.v1（connected-component grow 后处理）
        rplan.large_scale.enabled = cfg.large_scale_enabled ? 1 : 0;
        rplan.large_scale.min_structure_pixels =
            cfg.large_scale_min_structure_pixels;
        rplan.large_scale.low_grow_radius_pixels =
            cfg.large_scale_low_grow_pixels;
        rplan.large_scale.high_grow_radius_pixels =
            cfg.large_scale_high_grow_pixels;
        const bool large_scale_active = rplan.large_scale.enabled != 0;
        log("tile " + std::to_string(tile_ipix) + " rejection resolved: " +
            std::string(p2_rejection_semantic_id(rplan.method)) +
            " nominal=" + std::to_string(nominal_for_resolve) +
            " (tile_depth=" + std::to_string(depth) + ") profile=" +
            cfg.reject_profile + " norm=" + cfg.reject_normalization +
            " min_n=" + std::to_string(rplan.minimum_n) +
            " underdetermined_n=" +
            std::to_string(rplan.underdetermined_n) +
            " large_scale=" + std::to_string(large_scale_active));
        resolved_methods[depth] = p2_rejection_semantic_id(rplan.method);

        // 仅显式 sigma（robust_mad_clip）可走 ACR 块路径（同一 contract）
        // large_scale 激活时强制 CPU（per-frame mask 后处理在 CPU
        // reference 权威路径执行；ACR 只做逐像素 kernel，不做 grow）
        const bool use_acr_block =
            acr_reg != nullptr && rplan.method == P2_REJECT_SIGMA &&
            cfg.acr_route != "cpu" && !large_scale_active &&
            // weight_policy=ivar 时 ACR legacy
            // kernel 使用 control-cell ivar×support，与 CPU 逐像素 ivar
            // 不等价 → 强制 CPU canonical path（等价实现前不加速）。
            cfg.weight_mode != 2;
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

        // ----真实 N_B + planner 计算 chunk_pixels（micro-chunk 执行）----
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
        // 展平为单块 frame-major 连续缓冲，供统一 Eligibility collector）
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

        // large_scale 两遍路径缓冲（按全局帧 id 索引，覆盖
        // subset-tile 场景；cap = nb * n_leaf）
        std::vector<double> buf_val, buf_w, buf_sup;
        std::vector<std::uint8_t> buf_lo, buf_hi, buf_elig;
        std::vector<std::uint32_t> buf_nvalid;
        if (large_scale_active) {
            const std::size_t cap = (std::size_t)nb * n_leaf;
            buf_val.assign(cap, 0.0);
            buf_w.assign(cap, 0.0);
            buf_sup.assign(cap, 0.0);
            buf_lo.assign(cap, 0);
            buf_hi.assign(cap, 0);
            buf_elig.assign(cap, 0);
            buf_nvalid.assign(n_leaf, 0);
        }

        if (use_acr_block) {
            const int grid = 8;
            // weight_mode=2 → compact per-cell ivar (buffer3=ivar);
            // weight_mode=0 (legacy) → per-cell SNR
            std::vector<float> weight_compact(depth * grid * grid);
            for (std::uint32_t s = 0; s < depth; ++s) {
                const std::uint64_t fid =
                    frame_id_cache[frames[s]];
                const double fb = frame_snr[frames[s]];
                for (int gy = 0; gy < grid; ++gy)
                    for (int gx = 0; gx < grid; ++gx) {
                        const auto key =
                            std::make_tuple(fid, tile_ipix, gx, gy);
                        double v;
                        if (cfg.weight_mode == 2) {
                            const auto iit = local_ivar_map.find(key);
                            if (iit != local_ivar_map.end()) {
                                v = iit->second;
                                ++local_ivar_used;
                            } else {
                                v = (ivar_product_missing == 0) ? 1.0 : 0.0;
                            }
                        } else {
                            const auto sit = local_snr_map.find(key);
                            if (sit != local_snr_map.end()) {
                                v = sit->second;
                                ++local_snr_used;
                            } else {
                                v = fb;
                                ++frame_snr_fallback;
                            }
                        }
                        weight_compact[(std::size_t)(s * grid * grid +
                                                     gy * grid + gx)] =
                            (float)v;
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
                inv.buffers.add(3, weight_compact.data(),
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
                astro::compute::append_scalar(inv.scalars,
                                              int{cfg.weight_mode});
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
            // writer 约定 view 缓冲为 NESTED local 序
            // （ HIPS-IMG-001，与 drizzle 热路径一致）；stage2 集成缓冲为
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
        std::vector<float> t_ivar(512 * 512, 1.0f);        // 读缓冲
        // （PHASE2_IVAR_WIRING）：逐帧逐像素 ivar 缓冲（与 cal/supv
        // 同布局 depth×chunk_pixels）；ivar_valid 按原 frame slot 记录。
        std::vector<float> ivarv((std::size_t)depth * chunk_pixels, 1.0f);
        std::vector<std::uint8_t> ivar_valid(depth, 0);
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
                ivar_valid[s] = 0;
                if (cfg.weight_mode == 2 && ivr[f]) {
                    if (aio_hips_read_tile_f32(ivr[f], tile_ipix,
                                               t_ivar.data()) == 0) {
                        ivar_valid[s] = 1;
                        for (std::uint64_t i = 0; i < cnt; ++i)
                            ivarv[(std::size_t)s * chunk_pixels + i] =
                                t_ivar[(std::size_t)(p0 + i)];
                    }
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
                // 统一 EligibilityPolicy（与 ACR/compat 同一 collector；
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
                //compact eligible → 原始 frame slot 稳定映射
                std::vector<std::uint32_t> src_idx(depth);
                gout.source_indices = src_idx.data();
                std::uint32_t n_valid = 0;
                gout.eligible_count = &n_valid;
                if (p2_collect_candidate_stack(&gin, &gout) != 0) {
                    log("eligibility gather failed");
                    p2_upm_close(model);
                    return 6;
                }
                // 权重模式
                // mode 2 (ivar, 默认): 逐像素 ivar (帧 ivar 产品);
                // 产品缺失 → support (几何可靠性, 不伪造 ivar)
                // mode 0 (legacy): support × snr² (仅 ablation/诊断)
                // mode 1 (equal): weights 不填 (等权)
                if (cfg.weight_mode == 2) {
                    for (std::uint32_t s = 0; s < n_valid; ++s) {
                        // 用 source_indices（原 frame slot）取该帧该像素 ivar
                        const std::uint32_t orig = src_idx[s];
                        if (orig < depth && ivar_valid[orig]) {
                            const double iv = (double)ivarv[
                                (std::size_t)orig * chunk_pixels + i];
                            // 合同：nonfinite ivar → INVALID_INPUT
                            // （validator 拒绝）；ivar==0 → 合法零权重（不
                            // 贡献，ZERO_VALID_WEIGHT）。禁止静默换 support。
                            weights[s] = iv;
                            ++local_ivar_used;
                        } else {
                            // 缺 ivar 产品：仅显式 fallback 路径可达
                            // （打开时已 gate），降级 support 并计数。
                            weights[s] = support_v[s];
                        }
                    }
                } else if (cfg.weight_mode == 0) {
                    for (std::uint32_t s = 0; s < n_valid; ++s) {
                        //统一经 source_indices 取原 slot
                        const std::uint32_t orig = src_idx[s];
                        const std::uint64_t fid =
                            frame_id_cache[frames[orig]];
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
                            snr_v = frame_snr[frames[orig]];
                            ++frame_snr_fallback;
                        }
                        weights[s] = support_v[s] * snr_v * snr_v;
                    }
                } else {
                    std::fill(weights.begin(), weights.end(), 1.0);
                }
                // SNR lookup 后统一校验候选权重（非 finite/负 → fatal；诊断透出首 tile/像素）
                if (p2_validate_candidate_weights(weights.data(), n_valid) !=
                    0) {
                    std::string wdiag = "candidate weight validation failed: tile=" + std::to_string(tile_ipix) + " p=" + std::to_string(p) + " n_valid=" + std::to_string(n_valid) + " weights=[";
                    for (std::uint32_t wi = 0; wi < std::min<std::uint32_t>(n_valid, 8u); ++wi) wdiag += std::to_string(weights[wi]) + (wi+1<n_valid?",":"");
                    wdiag += "]";
                    log(wdiag); log_flush();
                    std::fprintf(stderr, "[stage2] %s\n", wdiag.c_str()); std::fflush(stderr);
                    // 额外透出 ivar/support 原值（ivarv 按 chunk_pixels 分块，p 为 tile 内全局索引需映射到 chunk 局部）
                    std::string extra = " weight diag: mode=" + std::to_string(cfg.weight_mode) + " ivar_valid=[";
                    for (std::uint32_t wi = 0; wi < std::min<std::uint32_t>(n_valid, 8u); ++wi) {
                        const std::uint32_t orig = src_idx[wi];
                        double supv_v = support_v[wi];
                        std::string ivs = "n/a";
                        if (orig < depth) {
                            // chunk 局部索引 i = p - p0 (p0 为本 chunk 起点)
                            const std::uint64_t iv_i = (p >= p0) ? (p - p0) : 0;
                            if (iv_i < chunk_pixels) {
                                double ivv = (double)ivarv[(std::size_t)orig * chunk_pixels + iv_i];
                                char ibuf[32]; std::snprintf(ibuf, sizeof(ibuf), "%.6g", ivv);
                                ivs = ibuf;
                                if (ivar_valid[orig]==0) ivs += "/no_ivar";
                            }
                        }
                        extra += "(" + ivs + "/" + std::to_string(supv_v) + ")" + (wi+1<n_valid?",":"");
                    }
                    extra += "]";
                    log(extra); std::fprintf(stderr, "[stage2] %s\n", extra.c_str()); std::fflush(stderr);
                    p2_upm_close(model);
                    return 6;
                }
                double signal_out = 0.0, support_out = 0.0;
                int st = 1;
                if (n_valid == 0) {
                    ++dbg_zero_px;
                    ++px_depth_0;
                } else {
                    if (n_valid == 1) ++px_depth_1;
                    else ++px_depth_ge_2;
                    // explicit plan kernel（auto 已在 planning 层解析）
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
                    // 只有 OK/UNDERDETERMINED 可继续；其余状态 hard fail
                    if (rdec.status != P2_STATUS_OK &&
                        rdec.status != P2_STATUS_UNDERDETERMINED) {
                        log("reject kernel invalid status=" +
                            std::to_string(rdec.status));
                        p2_upm_close(model);
                        return 6;
                    }
                    for (std::uint32_t s = 0; s < n_valid; ++s) {
                        acc[s] =
                            (rdec.reasons[s] == P2_REASON_ACCEPTED ||
                             rdec.reasons[s] == P2_REASON_UNDERDETERMINED)
                                ? 1 : 0;
                    }
                    if (large_scale_active) {
                        // 两遍路径——缓冲逐像素 rejection 结果（值/
                        // 权重/support + 低/高 mask），chunk 循环后统一 grow。
                        // 此分支只做诊断统计与缓冲，积分在 grow 之后进行。
                        for (std::uint32_t s = 0; s < n_valid; ++s) {
                            const std::uint32_t fs =
                                (std::uint32_t)fid_stack[s];
                            const std::size_t bi =
                                (std::size_t)fs * n_leaf + p;
                            buf_val[bi] = stack[s];
                            buf_w[bi] = weights[s];
                            buf_sup[bi] = support_v[s];
                            buf_elig[bi] = 1;
                            buf_lo[bi] =
                                (rdec.reasons[s] == P2_REASON_REJECTED_LOW)
                                    ? 1 : 0;
                            buf_hi[bi] =
                                (rdec.reasons[s] == P2_REASON_REJECTED_HIGH)
                                    ? 1 : 0;
                        }
                        buf_nvalid[p] = n_valid;
                        // pre-grow 基线统计（grow 后 phase 3 会重建最终分布）
                        total_rejected +=
                            rdec.rejected_low + rdec.rejected_high;
                        ++reject_hist[rdec.rejected_low +
                                      rdec.rejected_high];
                        if (rdec.status == P2_STATUS_UNDERDETERMINED) {
                            ++total_fallback;
                            ++dbg_fallback_px;
                            ++underdetermined_px;
                        } else {
                            ++px_integrated;
                            ++dbg_reject_px;
                        }
                        continue;
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
                    P2PixelResult pr{};
                    p2_integrate_pixel(&pi, &pr);
                    st = pr.status;
                    signal_out = pr.signal;
                    // support 唯一 canonical reducer（max accepted
                    // support）由 p2_integrate_pixel 计算，Stage2 只消费
                    support_out = pr.support;
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
        // ----large_scale connected-component grow + 二次积分 ----
        // 两遍路径（large_scale_active 时）：所有 chunk 的逐像素 rejection
        // 已缓冲到 tile 级 per-frame low/high mask；此处先 grow 再积分，
        // 保证"拒绝 mask 应用回原始 calibrated 科学值"的单一语义。
        if (large_scale_active) {
            if (p2_large_scale_apply(buf_lo.data(), buf_hi.data(), 512, 512,
                                     (int)nb, &rplan.large_scale) != 0) {
                log("large_scale apply failed");
                p2_upm_close(model);
                return 6;
            }
            const std::uint64_t pre_total = total_rejected;
            std::vector<double> st2(depth), w2(depth), sup2(depth);
            total_rejected = 0;
            reject_hist.clear();
            for (std::uint64_t p = 0; p < n_leaf; ++p) {
                std::uint32_t n_acc = 0;
                std::uint32_t rej_count = 0;
                for (std::uint32_t s = 0; s < depth; ++s) {
                    const std::uint32_t fs = frames[s];
                    const std::size_t bi = (std::size_t)fs * n_leaf + p;
                    if (buf_elig[bi]) {
                        if (buf_lo[bi] || buf_hi[bi]) {
                            ++rej_count;
                        } else {
                            st2[n_acc] = buf_val[bi];
                            w2[n_acc] = buf_w[bi];
                            sup2[n_acc] = buf_sup[bi];
                            ++n_acc;
                        }
                    }
                }
                total_rejected += rej_count;
                if (buf_nvalid[p] > 0)
                    ++reject_hist[rej_count];
                P2PixelStack pi{};
                pi.values = st2.data();
                pi.weights = w2.data();
                pi.support = sup2.data();
                pi.count = n_acc;
                P2PixelResult pr{};
                p2_integrate_pixel(&pi, &pr);
                const bool ok = (pr.status == 0);
                valid[p] = ok ? 1 : 0;
                const double area = ok ? pr.support * A_cell : 0.0;
                const double flux = ok ? pr.signal * area : 0.0;
                if (cfg.precision) {
                    fluxD[p] = flux;
                    areaD[p] = area;
                } else {
                    fluxF[p] = (float)flux;
                    areaF[p] = (float)area;
                }
                if (ok) ++total_pixels;
            }
            large_scale_grown =
                total_rejected > pre_total ? total_rejected - pre_total : 0;
            log("tile " + std::to_string(tile_ipix) +
                " large_scale: pre_rejected=" + std::to_string(pre_total) +
                " post_rejected=" + std::to_string(total_rejected) +
                " grown=" + std::to_string(large_scale_grown));
        }
        // 同 ACR 路径，FITS 行主序 -> NESTED local 序
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
    log("[hips_write] finalizing... tiles_written=" + std::to_string(tiles_written)); log_flush();
    if (aio_hips_finalize(ps) != 0) {
        const char* e = aio_hips_last_error();
        log(std::string("hips finalize failed: ") + (e ? e : "(no detail)")); std::fprintf(stderr, "[stage2] hips finalize failed: %s\n", e ? e : "(no detail)"); log_flush();
        p2_upm_close(model);
        return 6;
    }
    log("[hips_write] finalize ok"); log_flush();
    for (std::size_t i = 0; i < cfg.hips.size(); ++i) {
        if (sig[i]) aio_hips_close(sig[i]);
        if (sup[i]) aio_hips_close(sup[i]);
        if (ivr[i]) aio_hips_close(ivr[i]);
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
        // P7-2：overlap topology（control depth / pixel depth）
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
        diag["large_scale_enabled"] = cfg.large_scale_enabled;
        diag["large_scale_min_structure_pixels"] =
            cfg.large_scale_min_structure_pixels;
        diag["large_scale_low_grow_radius_pixels"] =
            cfg.large_scale_low_grow_pixels;
        diag["large_scale_high_grow_radius_pixels"] =
            cfg.large_scale_high_grow_pixels;
        diag["large_scale_grown_samples"] = large_scale_grown;
        diag["integrated_pixels"] = px_integrated;
        diag["quality_fallback_unknown"] = quality_unknown;
        diag["local_snr_used"] = local_snr_used;
        diag["frame_snr_median_fallback"] = frame_snr_fallback;
        diag["weight_mode"] = cfg.weight_mode;
        diag["local_ivar_used"] = local_ivar_used;
        diag["ivar_product_missing"] = ivar_product_missing;
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
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[stage2] unhandled exception: %s\n", e.what()); std::fflush(stderr);
        try { log(std::string("unhandled exception: ") + e.what()); log_flush(); } catch (...) {}
        return 1;
    } catch (...) {
        std::fprintf(stderr, "[stage2] unhandled unknown exception\n"); std::fflush(stderr);
        try { log("unhandled unknown exception"); log_flush(); } catch (...) {}
        return 1;
    }
}
