// 实验/SCI-C/code/upm_probe.cpp
//
// SCI-C 仓内实测驱动：直接链接生产 Phase2 UPM（只读，不修改任何生产代码）。
//   - UPM 段（scenario 含 "obs"）：p2_upm_build / p2_upm_calibrate_block /
//     p2_upm_evaluate_c / p2_upm_convergence / p2_upm_materialize_dense /
//     p2_upm_dense_read_block / p2_upm_save；
//   - MA 段（scenario 含 "ma_obs"）：p2_upm_ma_build / p2_upm_ma_solution /
//     p2_upm_ma_info / p2_upm_ma_provenance（乘性/加性分离求解器）。
// 两段可同时出现，也可只出现其一。
//
// 用法: upm_probe <scenario.json> <out.json>
#include "astro/phase2/upm.h"
#include "healpix/healpix_core.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using nlohmann::json;

namespace {

constexpr int kShift = 9;   // leaf order = target_order + 9

std::uint64_t cell_center_leaf(int gx, int gy, std::uint64_t tile) {
    const int cell_side = 512 / 8;
    const std::uint64_t local = astrocs::healpix::xy_to_nested_local(
        (std::uint32_t)(gx * cell_side + cell_side / 2),
        (std::uint32_t)(gy * cell_side + cell_side / 2), (std::uint32_t)kShift);
    return (tile << (2u * (unsigned)kShift)) | local;
}

void run_ma(const json& sc, json& out) {
    if (!sc.contains("ma_obs")) return;
    std::vector<P2UpmMaObservation> mo;
    for (const auto& o : sc.at("ma_obs")) {
        P2UpmMaObservation r{};
        r.frame_id     = o.at("frame_id").get<std::uint64_t>();
        r.control_id   = o.at("control_id").get<std::uint64_t>();
        r.value        = o.at("value").get<double>();
        r.control_ivar = o.at("control_ivar").get<double>();
        mo.push_back(r);
    }
    P2UpmMaConfig mc{};
    const json& jm = sc.value("ma_cfg", json::object());
    mc.min_frames = jm.value("min_frames", 2);
    mc.rank_rtol  = jm.value("rank_rtol", 1e-10);
    mc.kappa_max  = jm.value("kappa_max", 1e6);
    mc.gauge_mode = jm.value("gauge_mode", 0);
    mc.allow_additive_only_single_frame = jm.value("allow_additive_only_single_frame", 0);
    mc.c_in_has_unrepresented_shared_terms = 0;
    mc.huber_delta = jm.value("huber_delta", 1.345);
    mc.max_iterations = jm.value("max_iterations", 100);
    mc.tolerance = jm.value("tolerance", 1e-6);
    mc.sigma_floor = jm.value("sigma_floor", 1e-3);
    mc.zero_anchor_weight = jm.value("zero_anchor_weight", 0.0);
    mc.k_corr = jm.value("k_corr", 0.0);
    mc.k_corr_applicability_domain = nullptr;
    mc.k_corr_calibration_run_id = nullptr;
    mc.flux_conservation_factor = nullptr;
    void* mam = nullptr;
    const int rcma = p2_upm_ma_build(mo.data(), (std::uint64_t)mo.size(), &mc, &mam);
    out["ma_rc"] = rcma;
    if (rcma != 0 || mam == nullptr) return;
    P2UpmMaInfo mi{};
    p2_upm_ma_info(mam, &mi);
    out["ma_info"] = {{"n_controls", mi.n_controls}, {"n_frames", mi.n_frames},
                      {"n_components", mi.n_components}, {"n_params", mi.n_params},
                      {"rank", mi.rank}, {"kappa", mi.kappa},
                      {"iterations", mi.iterations},
                      {"additive_only_components", mi.additive_only_components},
                      {"model_hash", std::string(mi.model_hash)}};
    json jg = json::object(), jb = json::object(), js = json::object();
    std::vector<std::uint64_t> frames;
    for (const auto& f : sc.at("frames")) frames.push_back(f.get<std::uint64_t>());
    std::set<std::uint64_t> cids;
    for (const auto& o : sc.at("ma_obs")) cids.insert(o.at("control_id").get<std::uint64_t>());
    for (std::uint64_t f : frames) {
        double gv = 0.0, bv = 0.0;
        if (p2_upm_ma_solution(mam, f, *cids.begin(), &gv, &bv, nullptr) == 0) {
            jg[std::to_string(f)] = gv;
            jb[std::to_string(f)] = bv;
        }
    }
    for (std::uint64_t c : cids) {
        double sv = 0.0;
        if (!frames.empty() &&
            p2_upm_ma_solution(mam, frames[0], c, nullptr, nullptr, &sv) == 0)
            js[std::to_string(c)] = sv;
    }
    out["ma_g"] = jg;
    out["ma_b"] = jb;
    out["ma_s"] = js;
    std::vector<char> prov(8192, 0);
    if (p2_upm_ma_provenance(mam, prov.data(), prov.size()) == 0)
        out["ma_provenance"] = std::string(prov.data());
    p2_upm_ma_close(mam);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: upm_probe <scenario.json> <out.json>\n");
        return 2;
    }
    std::ifstream fin(argv[1]);
    if (!fin) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    json sc;
    fin >> sc;

    json out;
    const bool has_obs = sc.contains("obs");

    if (!has_obs) {
        run_ma(sc, out);
        std::ofstream fo(argv[2]);
        fo << out.dump(2);
        return 0;
    }

    P2UpmBuildConfig cfg{};
    const json& jc = sc.value("cfg", json::object());
    cfg.robust_loss        = jc.value("robust_loss", 0);
    cfg.snr_weight_mode    = jc.value("snr_weight_mode", 0);
    cfg.huber_delta        = jc.value("huber_delta", 1.345);
    cfg.smoothing_lambda   = jc.value("smoothing_lambda", 0.0);
    cfg.zero_anchor_weight = jc.value("zero_anchor_weight", 1e-3);
    cfg.max_iterations     = jc.value("max_iterations", 100);
    cfg.tolerance          = jc.value("tolerance", 1e-6);
    cfg.target_order       = jc.value("target_order", 0);
    cfg.sigma_floor        = jc.value("sigma_floor", 1e-3);
    cfg.support_power      = jc.value("support_power", 1.0);
    cfg.quality_mode       = jc.value("quality_mode", 0);
    cfg.use_ivar_weight    = jc.value("use_ivar_weight", 1);
    cfg.control_reliability= jc.value("control_reliability", 1.0);
    cfg.input_manifest_hash= nullptr;
    cfg.cpu_workers        = jc.value("cpu_workers", 1);
    cfg.gs_damping         = jc.value("gs_damping", 1.0);
    cfg.m_full_frame       = jc.value("m_full_frame", 0);
    cfg.final_gauge        = jc.value("final_gauge", 0);
    cfg.tolerance_relative = jc.value("tolerance_relative", 0);
    cfg.grid               = 8;

    std::vector<P2ControlObservation> obs;
    for (const auto& o : sc.at("obs")) {
        P2ControlObservation r{};
        r.frame_id         = o.at("frame_id").get<std::uint64_t>();
        r.control_id       = o.at("control_id").get<std::uint64_t>();
        r.leaf_ipix        = o.contains("leaf_ipix")
                               ? o.at("leaf_ipix").get<std::uint64_t>()
                               : cell_center_leaf(o.at("cell_gx").get<int>(),
                                                  o.at("cell_gy").get<int>(),
                                                  o.value("tile", (std::uint64_t)0));
        r.ra_deg           = o.value("ra_deg", 0.0);
        r.dec_deg          = o.value("dec_deg", 0.0);
        r.value            = o.at("value").get<double>();
        r.uncertainty      = o.value("uncertainty", 1.0);
        r.snr              = o.value("snr", 10.0);
        r.ivar             = o.value("ivar", 0.0);
        r.control_variance = o.value("control_variance", 1.0);
        r.control_ivar     = o.at("control_ivar").get<double>();
        r.snr_available    = o.value("snr_available", 1);
        r.support          = o.value("support", 1.0);
        r.quality_flags    = o.value("quality_flags", 1u);
        obs.push_back(r);
    }
    std::vector<std::uint64_t> frames;
    for (const auto& f : sc.at("frames")) frames.push_back(f.get<std::uint64_t>());

    const json& jp = sc.at("probe");
    const std::uint64_t tile = jp.value("tile", (std::uint64_t)0);
    const int x0 = jp.value("x0", 0), y0 = jp.value("y0", 0);
    const int nx = jp.value("nx", 512), ny = jp.value("ny", 512);
    std::vector<std::uint64_t> leaf((std::size_t)nx * (std::size_t)ny);
    for (int y = 0; y < ny; ++y)
        for (int x = 0; x < nx; ++x) {
            const std::uint64_t local = astrocs::healpix::xy_to_nested_local(
                (std::uint32_t)(x0 + x), (std::uint32_t)(y0 + y), (std::uint32_t)kShift);
            leaf[(std::size_t)y * (std::size_t)nx + (std::size_t)x] =
                (tile << (2u * (unsigned)kShift)) | local;
        }
    {
        std::uint32_t rx = 0, ry = 0;
        astrocs::healpix::nested_local_to_xy(leaf[0], (std::uint32_t)kShift, rx, ry);
        if ((int)rx != x0 || (int)ry != y0) {
            std::fprintf(stderr, "leaf round-trip mismatch\n");
            return 3;
        }
    }

    out["n_obs"] = obs.size();
    void* model = nullptr;
    const int rc = p2_upm_build(obs.data(), (std::uint64_t)obs.size(), &cfg, &model);
    out["rc_build"] = rc;
    if (rc != 0 || model == nullptr) {
        run_ma(sc, out);
        std::ofstream fo(argv[2]);
        fo << out.dump(2);
        std::fprintf(stderr, "p2_upm_build rc=%d\n", rc);
        return 4;
    }
    P2ModelInfo info{};
    p2_upm_info(model, &info);
    out["control_count"]     = info.control_count;
    out["observation_count"] = info.observation_count;
    out["component_count"]   = info.component_count;
    out["model_hash"]        = std::string(info.model_hash);
    {
        std::uint64_t iters = 0; double obj = 0.0; int conv = 0;
        p2_upm_convergence(model, &iters, &obj, &conv);
        out["iterations"] = iters;
        out["objective"]  = obj;
        out["converged"]  = conv;
    }

    const std::size_t NP = (std::size_t)nx * (std::size_t)ny;
    std::vector<double> zeros(NP, 0.0), corr(NP, 0.0);
    const std::string bin_path = sc.value("out_bin", std::string(""));
    std::FILE* fb = nullptr;
    if (!bin_path.empty()) {
        fb = std::fopen(bin_path.c_str(), "wb");
        if (!fb) { std::fprintf(stderr, "cannot write %s\n", bin_path.c_str()); return 5; }
        std::int32_t hdr[2] = {(std::int32_t)frames.size(), (std::int32_t)NP};
        std::fwrite(hdr, sizeof(std::int32_t), 2, fb);
    }
    double cmax = 0.0, corrmax = 0.0;
    for (std::uint64_t f : frames) {
        const int rc2 = p2_upm_calibrate_block(model, f, leaf.data(), zeros.data(),
                                               corr.data(), (std::uint64_t)NP);
        if (rc2 != 0) { out["rc_calibrate"] = rc2; break; }
        std::vector<double> cfield(NP, 0.0);
        for (std::size_t i = 0; i < NP; ++i) {
            cfield[i] = p2_upm_evaluate_c(model, f, leaf[i]);
            if (!std::isfinite(cfield[i])) out["nonfinite_C"] = true;
            cmax = std::max(cmax, std::fabs(cfield[i]));
            corrmax = std::max(corrmax, std::fabs(corr[i]));
        }
        if (fb) {
            std::fwrite(corr.data(), sizeof(double), NP, fb);
            std::fwrite(cfield.data(), sizeof(double), NP, fb);
        }
    }
    if (fb) std::fclose(fb);
    out["out_bin"] = bin_path;
    out["probe"] = {{"tile", tile}, {"x0", x0}, {"y0", y0}, {"nx", nx}, {"ny", ny}};
    out["frames"] = frames;
    out["max_abs_corr"] = corrmax;
    out["max_abs_C"] = cmax;

    const std::string sparse_path = sc.value("sparse_model_path", std::string(""));
    if (!sparse_path.empty()) {
        out["rc_save"] = p2_upm_save(model, sparse_path.c_str());
        std::ifstream fs(sparse_path, std::ios::binary | std::ios::ate);
        out["sparse_bytes"] = fs ? (long long)fs.tellg() : -1LL;
    }
    const std::string dcache = sc.value("dense_cache", std::string(""));
    if (!dcache.empty()) {
        const int rcm = p2_upm_materialize_dense(model, cfg.target_order, dcache.c_str());
        out["rc_materialize_dense"] = rcm;
        if (rcm == 0) {
            std::ifstream fd(dcache, std::ios::binary | std::ios::ate);
            out["dense_bytes"] = fd ? (long long)fd.tellg() : -1LL;
            double maxdiff = 0.0;
            std::uint64_t ncmp = 0;
            const std::uint64_t blk = 4096;
            std::vector<double> din(blk, 0.0), dout(blk, 0.0), sout(blk, 0.0);
            for (std::uint64_t f : frames) {
                for (std::uint64_t s = 0; s < NP; s += blk) {
                    const std::uint64_t n = std::min<std::uint64_t>(blk, NP - s);
                    const int rcd = p2_upm_dense_read_block(model, dcache.c_str(), f,
                                                            leaf.data() + s, din.data(),
                                                            dout.data(), n);
                    if (rcd != 0) { out["rc_dense_read"] = rcd; break; }
                    const int rcs = p2_upm_calibrate_block(model, f, leaf.data() + s,
                                                           din.data(), sout.data(), n);
                    if (rcs != 0) { out["rc_calibrate"] = rcs; break; }
                    for (std::uint64_t i = 0; i < n; ++i) {
                        maxdiff = std::max(maxdiff, std::fabs(dout[i] - sout[i]));
                        ++ncmp;
                    }
                }
            }
            out["dense_vs_sparse_max_abs"] = maxdiff;
            out["dense_vs_sparse_n"] = ncmp;
        }
    }
    p2_upm_close(model);
    run_ma(sc, out);
    std::ofstream fo(argv[2]);
    fo << out.dump(2);
    return 0;
}
