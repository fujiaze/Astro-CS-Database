// 实验/additive-sky-seamless/code/sky_probe.cpp
//
// SCI-C 仓内实测驱动：直接链接生产 Phase2 稀疏天光面（只读），
// 调用 p2_sky_plane_build / p2_sky_plane_info / p2_sky_plane_eval /
// p2_sky_plane_eval_delta / p2_sky_plane_residuals / p2_sky_plane_frame_delta /
// p2_upm_control_variance。
//
// 用法: sky_probe <scenario.json> <out.json>
#include "astro/phase2/sky_plane.h"
#include "astro/phase2/upm.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using nlohmann::json;

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: sky_probe <scenario.json> <out.json>\n"); return 2; }
    std::ifstream fin(argv[1]);
    if (!fin) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    json sc; fin >> sc;

    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    const json& jc = sc.value("cfg", json::object());
    cfg.spline_degree        = jc.value("spline_degree", cfg.spline_degree);
    cfg.node_spacing_deg     = jc.value("node_spacing_deg", cfg.node_spacing_deg);
    cfg.frame_gradient_order = jc.value("frame_gradient_order", cfg.frame_gradient_order);
    cfg.roughness_penalty    = jc.value("roughness_penalty", cfg.roughness_penalty);
    cfg.huber_delta          = jc.value("huber_delta", cfg.huber_delta);
    cfg.max_iterations       = jc.value("max_iterations", cfg.max_iterations);
    cfg.tolerance            = jc.value("tolerance", cfg.tolerance);
    cfg.gauge_mode           = jc.value("gauge_mode", cfg.gauge_mode);
    cfg.weight_mode          = jc.value("weight_mode", cfg.weight_mode);
    cfg.kappa_max            = jc.value("kappa_max", cfg.kappa_max);
    cfg.rank_rtol            = jc.value("rank_rtol", cfg.rank_rtol);
    cfg.min_samples          = jc.value("min_samples", cfg.min_samples);
    cfg.min_samples_per_frame= jc.value("min_samples_per_frame", cfg.min_samples_per_frame);
    cfg.max_nodes            = jc.value("max_nodes", cfg.max_nodes);
    cfg.max_extrapolation_deg= jc.value("max_extrapolation_deg", cfg.max_extrapolation_deg);

    std::vector<P2SkySample> s;
    for (const auto& o : sc.at("samples")) {
        P2SkySample r{};
        r.frame_id   = o.at("frame_id").get<std::uint64_t>();
        r.control_id = o.at("control_id").get<std::uint64_t>();
        r.ra_deg     = o.at("ra_deg").get<double>();
        r.dec_deg    = o.at("dec_deg").get<double>();
        r.value      = o.at("value").get<double>();
        r.variance   = o.at("variance").get<double>();
        r.snr        = o.value("snr", 10.0);
        r.flags      = o.value("flags", 0u);
        s.push_back(r);
    }
    std::vector<std::uint64_t> frames;
    for (const auto& f : sc.at("frames")) frames.push_back(f.get<std::uint64_t>());

    json out;
    out["n_samples"] = s.size();
    out["weight_mode"] = cfg.weight_mode;
    out["gauge_mode"] = cfg.gauge_mode;
    void* model = nullptr;
    std::vector<char> err(8192, 0);
    const int rc = p2_sky_plane_build(s.data(), (std::uint64_t)s.size(), &cfg, &model,
                                      err.data(), err.size());
    out["rc_build"] = rc;
    out["err"] = std::string(err.data());
    if (rc != 0 || model == nullptr) {
        std::ofstream fo(argv[2]); fo << out.dump(2);
        std::fprintf(stderr, "p2_sky_plane_build rc=%d err=%s\n", rc, err.data());
        return 4;
    }
    P2SkyPlaneInfo info{};
    p2_sky_plane_info(model, &info);
    out["n_used"] = info.n_used;
    out["n_frames"] = info.n_frames;
    out["n_nodes"] = info.n_nodes;
    out["n_params"] = info.n_params;
    out["rank"] = info.rank;
    out["kappa"] = info.kappa;
    out["rms_weighted"] = info.rms_weighted;
    out["rms_unweighted"] = info.rms_unweighted;
    out["chi2_red"] = info.chi2_red;
    out["iterations"] = info.iterations;
    out["gauge_shift"] = info.gauge_shift;
    out["n_masked"] = info.n_masked;
    out["n_rejected"] = info.n_rejected;
    out["model_hash"] = std::string(info.model_hash);

    // 每帧 δ_k 系数
    json jdelta = json::object();
    for (std::uint64_t f : frames) {
        std::uint64_t n = 0;
        p2_sky_plane_frame_delta(model, f, nullptr, 0, &n);
        std::vector<double> co(n, 0.0);
        const int rcd = p2_sky_plane_frame_delta(model, f, co.data(), n, &n);
        jdelta[std::to_string(f)] = (rcd == 0) ? json(co) : json(nullptr);
    }
    out["frame_delta_coeffs"] = jdelta;

    // 残差复算（独立于模型内部缓存）
    {
        double rw = 0.0, ru = 0.0; std::uint64_t nu = 0;
        const int rcr = p2_sky_plane_residuals(model, s.data(), (std::uint64_t)s.size(),
                                               &rw, &ru, &nu);
        out["rc_residuals"] = rcr;
        out["residuals_rms_weighted"] = rw;
        out["residuals_rms_unweighted"] = ru;
        out["residuals_n_used"] = nu;
    }

    // 探针求值：out_bin 布局 [n_frames][n_probe] float64 delta, 然后 [n_frames][n_probe] b_k
    const json& jp = sc.at("probe");
    const std::size_t NP = jp.at("ra_deg").size();
    std::vector<double> ra(NP), dec(NP);
    for (std::size_t i = 0; i < NP; ++i) {
        ra[i]  = jp.at("ra_deg")[i].get<double>();
        dec[i] = jp.at("dec_deg")[i].get<double>();
    }
    const std::string bin_path = sc.value("out_bin", std::string(""));
    std::FILE* fb = nullptr;
    if (!bin_path.empty()) {
        fb = std::fopen(bin_path.c_str(), "wb");
        if (!fb) return 5;
        std::int32_t hdr[2] = {(std::int32_t)frames.size(), (std::int32_t)NP};
        std::fwrite(hdr, sizeof(std::int32_t), 2, fb);
    }
    json stat = json::object();
    for (std::uint64_t f : frames) {
        std::vector<double> dv(NP, 0.0), bv(NP, 0.0);
        std::vector<std::uint8_t> st(NP, 0);
        const int rcd = p2_sky_plane_eval_delta_block(model, f, ra.data(), dec.data(),
                                                      (std::uint64_t)NP, dv.data(), st.data());
        std::vector<std::uint8_t> st2(NP, 0);
        const int rcb = p2_sky_plane_eval_block(model, f, ra.data(), dec.data(),
                                                (std::uint64_t)NP, bv.data(), st2.data());
        long nbad = 0;
        for (std::size_t i = 0; i < NP; ++i) if (st[i] != P2_SKY_EVAL_OK || st2[i] != P2_SKY_EVAL_OK) ++nbad;
        stat[std::to_string(f)] = {{"rc_delta", rcd}, {"rc_b", rcb}, {"n_bad", nbad}};
        if (fb) {
            std::fwrite(dv.data(), sizeof(double), NP, fb);
            std::fwrite(bv.data(), sizeof(double), NP, fb);
        }
    }
    if (fb) std::fclose(fb);
    out["probe_stat"] = stat;
    out["out_bin"] = bin_path;
    out["n_probe"] = NP;

    // 生产 control_variance 公式对拍
    if (sc.contains("control_variance_check")) {
        const json& cv = sc.at("control_variance_check");
        double v = 0.0, iv = 0.0;
        const int rccv = p2_upm_control_variance(
            cv.at("k_corr").get<double>(), cv.at("sigma_bg").get<double>(),
            cv.at("n_retained").get<std::uint64_t>(),
            cv.value("applicability_domain", std::string("")).c_str(),
            cv.value("calibration_run_id", std::string("")).c_str(), &v, &iv);
        out["rc_control_variance"] = rccv;
        out["control_variance"] = v;
        out["control_ivar"] = iv;
    }

    p2_sky_plane_close(model);
    std::ofstream fo(argv[2]); fo << out.dump(2);
    return 0;
}
