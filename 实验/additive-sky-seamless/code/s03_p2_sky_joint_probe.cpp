// run/ACSDSDOC-01/T08/code/p2_sky_joint_probe.cpp
//
// T08 A0 生产链只读驱动：静态链接 build/linux-control/libacsd_phase2.a，
// 调用**生产** fit 入口 p2_sky_plane_build_joint（§5.11 联合解：B_ref + δ_poly + F_k，
// 去跨帧公共模）与**生产** apply 入口 p2_sky_plane_eval_delta_block（δ_k 求值）。
//
// 只读链接，不修改任何生产源码。用法:
//   p2_sky_joint_probe <scenario.json> <out.json>
//
// scenario.json:
//   cfg:      P2SkyPlaneConfig 逐字段覆盖（可选；geometry 见下）
//   geometry: {overlap_band_width_deg, pointing_spacing_deg, sample_pitch_deg,
//              pixel_scale_arcsec}（node_spacing_deg<=0 时必需）
//   adaptive: P2SkyPlaneAdaptiveConfig 覆盖（可选）
//   samples:  [{frame_id, control_id, ra_deg, dec_deg, value, variance,
//               snr, flags}]
//   frames:   [frame_id, ...]
//   probe:    {ra_deg:[...], dec_deg:[...]}
//   out_bin:  可选；写 [n_frames][n_probe] float64 δ_k，帧序 = frames 输入序
#include "astro/phase2/sky_plane.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

using nlohmann::json;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: p2_sky_joint_probe <scenario.json> <out.json>\n");
        return 2;
    }
    std::ifstream fin(argv[1]);
    if (!fin) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    json sc; fin >> sc;

    P2SkyPlaneConfig cfg = p2_sky_plane_default_config();
    const json& jc = sc.value("cfg", json::object());
    cfg.spline_degree         = jc.value("spline_degree", cfg.spline_degree);
    cfg.node_spacing_deg      = jc.value("node_spacing_deg", cfg.node_spacing_deg);
    cfg.frame_gradient_order  = jc.value("frame_gradient_order", cfg.frame_gradient_order);
    cfg.own_surface_enabled   = jc.value("own_surface_enabled", cfg.own_surface_enabled);
    cfg.own_surface_step_deg  = jc.value("own_surface_step_deg", cfg.own_surface_step_deg);
    cfg.joint_outer_iterations= jc.value("joint_outer_iterations", cfg.joint_outer_iterations);
    cfg.huber_delta           = jc.value("huber_delta", cfg.huber_delta);
    cfg.max_iterations        = jc.value("max_iterations", cfg.max_iterations);
    cfg.tolerance             = jc.value("tolerance", cfg.tolerance);
    cfg.gauge_mode            = jc.value("gauge_mode", cfg.gauge_mode);
    cfg.weight_mode           = jc.value("weight_mode", cfg.weight_mode);
    cfg.rank_rtol             = jc.value("rank_rtol", cfg.rank_rtol);
    cfg.min_samples           = jc.value("min_samples", cfg.min_samples);
    cfg.min_samples_per_frame = jc.value("min_samples_per_frame", cfg.min_samples_per_frame);
    cfg.max_nodes             = jc.value("max_nodes", cfg.max_nodes);
    cfg.max_extrapolation_deg = jc.value("max_extrapolation_deg", cfg.max_extrapolation_deg);
    if (sc.contains("geometry")) {
        const json& g = sc.at("geometry");
        cfg.geometry.overlap_band_width_deg = g.value("overlap_band_width_deg", 0.0);
        cfg.geometry.pointing_spacing_deg   = g.value("pointing_spacing_deg", 0.0);
        cfg.geometry.sample_pitch_deg       = g.value("sample_pitch_deg", 0.0);
        cfg.geometry.pixel_scale_arcsec     = g.value("pixel_scale_arcsec", 0.0);
    }

    P2SkyPlaneAdaptiveConfig ad = p2_sky_plane_default_adaptive_config();
    const json& ja = sc.value("adaptive", json::object());
    ad.max_attempts           = ja.value("max_attempts", ad.max_attempts);
    ad.max_node_refinements   = ja.value("max_node_refinements", ad.max_node_refinements);
    ad.max_node_coarsenings   = ja.value("max_node_coarsenings", ad.max_node_coarsenings);
    ad.residual_improve_ratio = ja.value("residual_improve_ratio", ad.residual_improve_ratio);

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
    out["cfg_echo"] = {{"own_surface_enabled", cfg.own_surface_enabled},
                       {"own_surface_step_deg", cfg.own_surface_step_deg},
                       {"joint_outer_iterations", cfg.joint_outer_iterations},
                       {"frame_gradient_order", cfg.frame_gradient_order},
                       {"node_spacing_deg", cfg.node_spacing_deg},
                       {"gauge_mode", cfg.gauge_mode}};

    void* model = nullptr;
    std::vector<char> err(8192, 0);
    P2SkyPlaneAdaptiveReport rep{};
    const int rc = p2_sky_plane_build_joint(s.data(), (std::uint64_t)s.size(), &cfg, &ad,
                                            &model, &rep, err.data(), err.size());
    out["rc_build_joint"] = rc;
    out["err"] = std::string(err.data());
    out["adaptive"] = {{"n_attempts", rep.n_attempts},
                       {"n_node_refinements", rep.n_node_refinements},
                       {"n_node_coarsenings", rep.n_node_coarsenings},
                       {"node_adaptive_used", rep.node_adaptive_used},
                       {"representation_limited", rep.representation_limited},
                       {"node_spacing_deg", rep.node_spacing_deg},
                       {"kappa", rep.kappa},
                       {"rank", rep.rank},
                       {"n_params", rep.n_params},
                       {"n_unidentified", rep.n_unidentified},
                       {"identifiable", rep.identifiable},
                       {"chi2_red", rep.chi2_red}};
    if (rc != 0 || model == nullptr) {
        std::ofstream fo(argv[2]); fo << out.dump(2);
        std::fprintf(stderr, "p2_sky_plane_build_joint rc=%d err=%s\n", rc, err.data());
        return 4;
    }

    P2SkyPlaneInfo info{};
    p2_sky_plane_info(model, &info);
    out["n_used"] = info.n_used;
    out["n_frames"] = info.n_frames;
    out["n_nodes"] = info.n_nodes;
    out["n_params"] = info.n_params;
    out["n_params_full"] = info.n_params_full;
    out["rank"] = info.rank;
    out["kappa"] = info.kappa;
    out["rms_weighted"] = info.rms_weighted;
    out["rms_unweighted"] = info.rms_unweighted;
    out["chi2_red"] = info.chi2_red;
    out["iterations"] = info.iterations;
    out["gauge_shift"] = info.gauge_shift;
    out["node_spacing_deg"] = info.node_spacing_deg;
    out["model_hash"] = std::string(info.model_hash);

    // 生产 apply：δ_k 在探针网格上求值
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
        std::vector<double> dv(NP, 0.0);
        std::vector<std::uint8_t> st(NP, 0);
        const int rcd = p2_sky_plane_eval_delta_block(model, f, ra.data(), dec.data(),
                                                      (std::uint64_t)NP, dv.data(), st.data());
        long nbad = 0;
        for (std::size_t i = 0; i < NP; ++i) if (st[i] != P2_SKY_EVAL_OK) ++nbad;
        stat[std::to_string(f)] = {{"rc_delta", rcd}, {"n_bad", nbad}};
        if (fb) std::fwrite(dv.data(), sizeof(double), NP, fb);
    }
    if (fb) std::fclose(fb);
    out["probe_stat"] = stat;
    out["out_bin"] = bin_path;
    out["n_probe"] = NP;
    out["frames"] = frames;

    // 逐帧 δ_k 多项式系数（供独立复核）
    json jdelta = json::object();
    for (std::uint64_t f : frames) {
        std::uint64_t n = 0;
        p2_sky_plane_frame_delta(model, f, nullptr, 0, &n);
        std::vector<double> co(n, 0.0);
        const int rcd = p2_sky_plane_frame_delta(model, f, co.data(), n, &n);
        jdelta[std::to_string(f)] = (rcd == 0) ? json(co) : json(nullptr);
    }
    out["frame_delta_coeffs"] = jdelta;

    p2_sky_plane_close(model);
    std::ofstream fo(argv[2]); fo << out.dump(2);
    return 0;
}
