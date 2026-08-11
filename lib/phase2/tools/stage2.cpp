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
#include <string>
#include <tuple>
#include <vector>

extern "C" {
#include "aio_hips.h"
#include "aio_hips_reader.h"
}

namespace {

struct Stage2Config {
    std::vector<std::string> hips;
    std::string target_order_spec = "auto";
    int target_order = -1;
    // model
    int control_grid_per_tile = 8;
    int patch_radius_leaf = 2;
    int min_samples = 5;
    double snr_search_radius_deg = 0.05;
    int robust_loss = 0;         // huber
    int snr_weight_mode = 0;     // snr2_normalized
    double huber_delta = 1.345;
    double smoothing_lambda = 0.0;
    double zero_anchor_weight = 1e-3;
    double sigma_floor = 1e-3;
    double support_power = 1.0;
    int max_irls_iterations = 100;
    double tolerance = 1e-6;
    // integration
    int precision = 0;           // 0=fp32, 1=fp64
    std::uint64_t memory_limit_mb = 24576;
    int reject_method = P2_REJECT_WINSORIZED_SIGMA;
    double sigma_low = -4.0;
    double sigma_high = 3.0;
    int reject_max_iterations = 8;
    int reject_min_samples = 2;
    int weight_mode = 0;         // snr2_normalized
    std::string acr_route = "auto";   // auto | cpu（R9 证据用）
    // output
    std::string out_hips;
    bool diagnostics = true;
};

std::ofstream g_log;

void log(const std::string& msg) {
    std::fprintf(stdout, "[stage2] %s\n", msg.c_str());
    std::fflush(stdout);
    if (g_log.is_open()) g_log << msg << "\n";
}

bool parse_config(const nlohmann::json& j, Stage2Config* cfg, std::string* err) {
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
    Stage2Config cfg;
    std::string err;
    if (!parse_config(j, &cfg, &err)) {
        std::fprintf(stderr, "config error: %s\n", err.c_str());
        return 2;
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
    mark("coverage");

    // R3：input manifest hash = canonical(稳定 frame identity + 关键元数据)
    std::vector<std::pair<std::uint64_t, std::string>> manifest_entries;
    for (std::size_t i = 0; i < cfg.hips.size(); ++i) {
        const std::uint64_t fid = p2_frame_id(cfg.hips[i].c_str());
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

    // ---- W4 CONTROL SAMPLE ----
    P2SamplerConfig sccfg{};
    sccfg.control_grid_per_tile = cfg.control_grid_per_tile;
    sccfg.patch_radius_leaf = cfg.patch_radius_leaf;
    sccfg.min_samples = cfg.min_samples;
    sccfg.snr_search_radius_deg = cfg.snr_search_radius_deg;
    std::uint64_t n_obs = 0, n_ctrl = 0;
    char serr[512] = {0};
    if (p2_sample_controls(&cov, paths.data(), &sccfg, nullptr, 0,
                           &n_obs, &n_ctrl, serr, sizeof(serr)) != 0) {
        log("sampler error: " + std::string(serr));
        return 4;
    }
    std::vector<P2ControlObservation> obs(n_obs);
    if (p2_sample_controls(&cov, paths.data(), &sccfg, obs.data(), n_obs,
                           &n_obs, &n_ctrl, serr, sizeof(serr)) != 0) {
        log("sampler error (fill): " + std::string(serr));
        return 4;
    }
    log("control sampling: controls=" + std::to_string(n_ctrl) +
        " observations=" + std::to_string(n_obs));
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
    std::map<std::tuple<std::uint64_t, std::uint64_t, int, int>, double>
        local_snr_map;
    for (const auto& o : obs) {
        const std::uint64_t tile = o.leaf_ipix >> 18;
        const std::uint64_t local = o.leaf_ipix & ((1ULL << 18) - 1ULL);
        std::uint32_t x = 0, y = 0;
        astrocs::healpix::nested_local_to_xy(local, 9u, x, y);
        local_snr_map[std::make_tuple(o.frame_id, tile, (int)(x / 64),
                                      (int)(y / 64))] = o.snr;
    }
    std::uint64_t local_snr_used = 0, frame_snr_fallback = 0;

    // ---- W4 UPM FIT ----
    P2UpmBuildConfig mcfg{};
    mcfg.robust_loss = cfg.robust_loss;
    mcfg.snr_weight_mode = cfg.snr_weight_mode;
    mcfg.huber_delta = cfg.huber_delta;
    mcfg.smoothing_lambda = cfg.smoothing_lambda;
    mcfg.zero_anchor_weight = cfg.zero_anchor_weight;
    mcfg.sigma_floor = cfg.sigma_floor;
    mcfg.support_power = cfg.support_power;
    mcfg.quality_mode = 0;          // Phase1 quality 位映射（sampler 读取）
    mcfg.control_reliability = 1.0; // 显式默认（禁止依赖 {} 零初始化）
    mcfg.input_manifest_hash = input_manifest_hash.c_str();
    mcfg.max_iterations = cfg.max_irls_iterations;
    mcfg.tolerance = cfg.tolerance;
    mcfg.target_order = target_order;
    void* model = nullptr;
    if (p2_upm_build(obs.data(), obs.size(), &mcfg, &model) != 0) {
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
    const std::vector<double> frame_snr = frame_snr_medians(cfg.hips);

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
    std::map<std::uint32_t, std::uint64_t> reject_hist;  // 每像素拒绝样本数分布
    std::uint64_t tiles_written = 0;
    std::vector<std::uint8_t> valid(n_leaf);
    std::vector<float> fluxF(n_leaf), areaF(n_leaf);
    std::vector<double> fluxD(n_leaf), areaD(n_leaf);
    std::vector<float> t_sig_probe(512 * 512);
    const std::uint32_t nb = (std::uint32_t)cfg.hips.size();

    // ---- ACR 路由（G9）：sigma/winsorized 逐 tile 走 KernelRegistry，
    // GPU 可用则 CUDA，否则 CPU legacy（同一科学语义）。 ----
    astro::compute::phase2::register_phase2_acr_kernels();
    const astro::compute::KernelRegistration* acr_reg =
        astro::compute::global_kernel_registry().find(
            astro::compute::phase2::kOpMosaicReject);
    const bool use_acr_block =
        acr_reg != nullptr &&
        cfg.reject_method == P2_REJECT_SIGMA;   // R6：Winsorized 明确 CPU_ROUTE
    namespace bridge = astro::compute::cuda::bridge;
    bool gpu_ready = false;
    void* gpu_exec = nullptr;
    if (use_acr_block && cfg.acr_route != "cpu") {
        bridge::ensure_bridge_loaded();
        if (bridge::api().loaded()) {
            const char* gerr = nullptr;
            gpu_exec = bridge::api().executor_create(0, 1u << 22, 1u << 18,
                                                     &gerr);
            gpu_ready = (gpu_exec != nullptr);
            if (gpu_ready) bridge::set_tls_handle(gpu_exec);
        }
    }
    log("ACR block routing: enabled=" + std::to_string(use_acr_block) +
        " gpu=" + std::to_string(gpu_ready));
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

        // 每 chunk 的工作缓冲（按 chunk_pixels×depth，非 262144×all_frames）
        std::vector<std::vector<double>> cal(depth), supv(depth);
        for (auto& v : cal) v.resize(chunk_pixels);
        for (auto& v : supv) v.resize(chunk_pixels);
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

        // 全 tile 临时读取缓冲（512×512×2×4B，固定小）
        std::vector<float> t_sig(512 * 512), t_sup(512 * 512);

        if (use_acr_block) {
            // compact per-cell SNR（P0-10/R8：与 frames[s] 一一对应且按
            // control cell 提供局部 SNR；缺失 cell → 整帧 median fallback）
            const int grid = 8;
            std::vector<float> snr_compact(depth * grid * grid);
            for (std::uint32_t s = 0; s < depth; ++s) {
                const std::uint64_t fid =
                    p2_frame_id(cfg.hips[frames[s]].c_str());
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
                        model, p2_frame_id(cfg.hips[f].c_str()),
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
                                              int{cfg.reject_method});
                astro::compute::append_scalar(inv.scalars, cfg.sigma_low);
                astro::compute::append_scalar(inv.scalars, cfg.sigma_high);
                astro::compute::append_scalar(inv.scalars,
                                              cfg.reject_min_samples);
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
                    if (nv <= 0.0f) ++dbg_zero_px;
                    else if (nv < (float)cfg.reject_min_samples) {
                        ++total_fallback;
                        ++dbg_fallback_px;
                    } else ++dbg_reject_px;
                    if (out_rej_f32[i] > 0.0f)
                        ++reject_hist[(std::uint32_t)out_rej_f32[i]];
                }
            }
            AstroSphereTileView view{};
            std::memset(&view, 0, sizeof(view));
            view.parent_ipix = tile_ipix;
            view.leaf_order = (std::uint32_t)(target_order + 9);
            view.width = 512;
            view.data_type = dtype;
            view.flux_sum = cfg.precision ? (const void*)fluxD.data()
                                          : (const void*)fluxF.data();
            view.covered_area = cfg.precision ? (const void*)areaD.data()
                                              : (const void*)areaF.data();
            view.valid_mask = valid.data();
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
                    cal[s][i] = (double)t_sig[(std::size_t)g];
                    supv[s][i] = (double)t_sup[(std::size_t)g];
                }
                std::vector<double> out_v(cnt);
                p2_upm_calibrate_block(
                    model, p2_frame_id(cfg.hips[f].c_str()),
                    chunk_leaves[c].data(), cal[s].data(), out_v.data(), cnt);
                for (std::uint64_t i = 0; i < cnt; ++i) cal[s][i] = out_v[i];
            }
            for (std::uint64_t i = 0; i < cnt; ++i) {
                const std::uint64_t p = p0 + i;
                std::uint32_t n_valid = 0;
                for (std::uint32_t s = 0; s < depth; ++s) {
                    if (std::isfinite(cal[s][i]) && supv[s][i] > 0.0) {
                        stack[n_valid] = cal[s][i];
                        support_v[n_valid] = supv[s][i];
                        // R8：局部 SNR（control cell 级）；缺失 → 整帧
                        // median fallback（计数）
                        const std::uint64_t fid =
                            p2_frame_id(cfg.hips[frames[s]].c_str());
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
                            snr_v = frame_snr[frames[s]];
                            ++frame_snr_fallback;
                        }
                        weights[n_valid] = supv[s][i] * snr_v * snr_v;
                        ++n_valid;
                    }
                }
                double signal_out = 0.0, support_out = 0.0;
                int st = 1;
                if (n_valid == 0) {
                    ++dbg_zero_px;
                } else if (n_valid < (std::uint32_t)cfg.reject_min_samples) {
                    for (std::uint32_t s = 0; s < n_valid; ++s) acc[s] = 1;
                    ++total_fallback;
                    ++dbg_fallback_px;
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
                    for (std::uint32_t s = 0; s < n_valid; ++s)
                        support_out = std::max(support_out, support_v[s]);
                } else {
                    ++dbg_reject_px;
                    P2SampleStackView rv{};
                    rv.values = stack.data();
                    rv.valid = nullptr;
                    rv.count = n_valid;
                    rv.data_type = 1;
                    rv.method = cfg.reject_method;
                    rv.sigma_low = cfg.sigma_low;
                    rv.sigma_high = cfg.sigma_high;
                    rv.max_iterations = cfg.reject_max_iterations;
                    rv.min_samples = cfg.reject_min_samples;
                    P2RejectionResult rr{};
                    rr.accepted = acc.data();
                    p2_reject_stack(&rv, &rr);
                    total_rejected += rr.rejected_low + rr.rejected_high;
                    ++reject_hist[rr.rejected_low + rr.rejected_high];
                    if (rr.status == 1) ++total_fallback;
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
        AstroSphereTileView view{};
        std::memset(&view, 0, sizeof(view));
        view.parent_ipix = tile_ipix;
        view.leaf_order = (std::uint32_t)(target_order + 9);
        view.width = 512;
        view.data_type = dtype;
        view.flux_sum = cfg.precision ? (const void*)fluxD.data() : (const void*)fluxF.data();
        view.covered_area = cfg.precision ? (const void*)areaD.data() : (const void*)areaF.data();
        view.valid_mask = valid.data();
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
        nlohmann::json diag;
        diag["stage2_version"] = 1;
        diag["tiles_written"] = tiles_written;
        diag["output_pixels"] = total_pixels;
        diag["rejected_samples"] = total_rejected;
        diag["fallback_pixels"] = total_fallback;
        diag["quality_fallback_unknown"] = quality_unknown;
        diag["local_snr_used"] = local_snr_used;
        diag["frame_snr_median_fallback"] = frame_snr_fallback;
        diag["upm_sigma_floor"] = cfg.sigma_floor;
        diag["upm_support_power"] = cfg.support_power;
        diag["zero_coverage_pixels"] = dbg_zero_px;
        diag["reject_method"] = cfg.reject_method;
        diag["rejection_samples_per_pixel"] = reject_hist;
        diag["model_hash"] = std::string(minfo.model_hash);
        diag["runtime_seconds"] = secs;
        std::ofstream df(cfg.out_hips + "/diagnostics.json");
        if (df) df << diag.dump(2);
        log("diagnostics written: " + cfg.out_hips + "/diagnostics.json");
    }
    return 0;
}
