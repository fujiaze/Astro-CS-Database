// 实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/upm_sweep.cpp
// RELEASE-02 A5（SMOOTH-LAMBDA）λs 扫描 Oracle。
// 链接**真实** lib/algorithms/coverage/src/upm.cpp（非复刻），生产配置标志。
// 输入：UPMB v1 二进制（obs+nodes，由 Python 生成：真实 L4 p2_samples.json 或合成场景）。
// 输出：每 λs 一个 .bin（C 场 + 校准场 + w_cell）+ 一个 summary.jsonl。
// 零 ninja/cmake/ctest；g++ 独立构建。
#include "astro/phase2/upm.h"
#include "astro/phase2/sampler.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Reader {
    FILE* f = nullptr;
    bool ok = true;
    template <typename T> T rd() {
        T v{};
        if (ok && std::fread(&v, sizeof(T), 1, f) != 1) ok = false;
        return v;
    }
    void rdv(void* p, std::size_t n) {
        if (ok && std::fread(p, 1, n, f) != n) ok = false;
    }
};

struct ObsLite {
    std::uint64_t frame_id, control_id, leaf_ipix;
    double ra, dec, value, uncertainty, snr, ivar, control_variance,
        control_ivar, support;
    std::int32_t snr_available;
    std::uint32_t quality_flags;
};

struct NodeLite {
    std::uint64_t control_id, tile_ipix;
    std::int32_t gx, gy;
    double ra, dec;
    std::uint64_t leaf_ipix;
};

}  // namespace

int main(int argc, char** argv) {
    std::string in_path, out_prefix, lambdas_arg, tag = "run";
    int workers = 8, max_iter = 100;
    double tol = 1e-3, zero_anchor = 1e-3;
    int gs_damping = -1, m_full_frame = 1, final_gauge = 1, tol_rel = 1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto nxt = [&]() { return std::string(argv[++i]); };
        if (a == "--in") in_path = nxt();
        else if (a == "--out") out_prefix = nxt();
        else if (a == "--lambdas") lambdas_arg = nxt();
        else if (a == "--workers") workers = std::atoi(nxt().c_str());
        else if (a == "--max-iter") max_iter = std::atoi(nxt().c_str());
        else if (a == "--tol") tol = std::atof(nxt().c_str());
        else if (a == "--zero-anchor") zero_anchor = std::atof(nxt().c_str());
        else if (a == "--damping") gs_damping = std::atoi(nxt().c_str());
        else if (a == "--m-full-frame") m_full_frame = std::atoi(nxt().c_str());
        else if (a == "--final-gauge") final_gauge = std::atoi(nxt().c_str());
        else if (a == "--tol-rel") tol_rel = std::atoi(nxt().c_str());
        else if (a == "--tag") tag = nxt();
        else { std::fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (in_path.empty() || out_prefix.empty() || lambdas_arg.empty()) {
        std::fprintf(stderr, "usage: upm_sweep --in F --out PREFIX --lambdas a,b,c [opts]\n");
        return 2;
    }

    // ---- 读输入 ----
    FILE* f = std::fopen(in_path.c_str(), "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", in_path.c_str()); return 1; }
    Reader R{f};
    const std::uint32_t magic = R.rd<std::uint32_t>();
    const std::uint32_t ver = R.rd<std::uint32_t>();
    if (magic != 0x55504D31u || ver != 1) { std::fprintf(stderr, "bad magic/ver\n"); return 1; }
    const std::uint64_t n_obs = R.rd<std::uint64_t>();
    const std::uint64_t n_nodes = R.rd<std::uint64_t>();
    const std::uint64_t n_frames = R.rd<std::uint64_t>();
    std::vector<std::uint64_t> frames(n_frames);
    R.rdv(frames.data(), n_frames * 8);
    std::vector<ObsLite> ol(n_obs);
    for (std::uint64_t i = 0; i < n_obs && R.ok; ++i) {
        ObsLite& o = ol[i];
        o.frame_id = R.rd<std::uint64_t>(); o.control_id = R.rd<std::uint64_t>();
        o.leaf_ipix = R.rd<std::uint64_t>();
        o.ra = R.rd<double>(); o.dec = R.rd<double>();
        o.value = R.rd<double>(); o.uncertainty = R.rd<double>();
        o.snr = R.rd<double>(); o.ivar = R.rd<double>();
        o.control_variance = R.rd<double>(); o.control_ivar = R.rd<double>();
        o.snr_available = R.rd<std::int32_t>(); o.support = R.rd<double>();
        o.quality_flags = R.rd<std::uint32_t>();
    }
    std::vector<NodeLite> nl(n_nodes);
    for (std::uint64_t i = 0; i < n_nodes && R.ok; ++i) {
        NodeLite& n = nl[i];
        n.control_id = R.rd<std::uint64_t>(); n.tile_ipix = R.rd<std::uint64_t>();
        n.gx = R.rd<std::int32_t>(); n.gy = R.rd<std::int32_t>();
        n.ra = R.rd<double>(); n.dec = R.rd<double>();
        n.leaf_ipix = R.rd<std::uint64_t>();
    }
    std::fclose(f);
    if (!R.ok) { std::fprintf(stderr, "truncated input\n"); return 1; }
    std::fprintf(stderr, "[sweep] n_obs=%llu n_nodes=%llu n_frames=%llu\n",
                 (unsigned long long)n_obs, (unsigned long long)n_nodes,
                 (unsigned long long)n_frames);

    // ---- 转成 C ABI 结构 ----
    std::vector<P2ControlObservation> obs(n_obs);
    for (std::uint64_t i = 0; i < n_obs; ++i) {
        P2ControlObservation x{};
        const ObsLite& o = ol[i];
        x.frame_id = o.frame_id; x.control_id = o.control_id; x.leaf_ipix = o.leaf_ipix;
        x.ra_deg = o.ra; x.dec_deg = o.dec; x.value = o.value;
        x.uncertainty = o.uncertainty; x.snr = o.snr; x.ivar = o.ivar;
        x.control_variance = o.control_variance; x.control_ivar = o.control_ivar;
        x.snr_available = o.snr_available; x.support = o.support;
        x.quality_flags = o.quality_flags;
        obs[i] = x;
    }
    std::vector<P2ControlNode> nodes(n_nodes);
    for (std::uint64_t i = 0; i < n_nodes; ++i) {
        P2ControlNode n{};
        n.control_id = nl[i].control_id; n.tile_ipix = nl[i].tile_ipix;
        n.gx = nl[i].gx; n.gy = nl[i].gy; n.ra_deg = nl[i].ra;
        n.dec_deg = nl[i].dec; n.leaf_ipix = nl[i].leaf_ipix;
        nodes[i] = n;
    }
    // control_id -> node index
    std::vector<std::size_t> cid2node(n_nodes);
    {
        std::uint64_t mx = 0;
        for (auto& n : nl) mx = std::max(mx, n.control_id);
        cid2node.assign((std::size_t)mx + 1, (std::size_t)-1);
        for (std::size_t i = 0; i < n_nodes; ++i) cid2node[(std::size_t)nl[i].control_id] = i;
    }

    // ---- λs 列表 ----
    std::vector<double> lambdas;
    {
        std::string cur;
        for (char c : lambdas_arg + ",") {
            if (c == ',') { if (!cur.empty()) lambdas.push_back(std::atof(cur.c_str())); cur.clear(); }
            else cur.push_back(c);
        }
    }

    FILE* js = std::fopen((out_prefix + ".summary.jsonl").c_str(), "w");
    std::fprintf(js, "{\"tag\":\"%s\",\"n_obs\":%llu,\"n_nodes\":%llu,\"n_frames\":%llu,"
                     "\"workers\":%d,\"max_iter\":%d,\"tol\":%g,\"zero_anchor\":%g,"
                     "\"damping\":%d,\"m_full_frame\":%d,\"final_gauge\":%d,\"tol_rel\":%d}\n",
                 tag.c_str(), (unsigned long long)n_obs, (unsigned long long)n_nodes,
                 (unsigned long long)n_frames, workers, max_iter, tol, zero_anchor,
                 gs_damping, m_full_frame, final_gauge, tol_rel);

    for (double ls : lambdas) {
        P2UpmBuildConfig cfg{};
        cfg.robust_loss = 0;
        cfg.snr_weight_mode = 0;
        cfg.huber_delta = 1.345;
        cfg.smoothing_lambda = ls;
        cfg.zero_anchor_weight = zero_anchor;
        cfg.max_iterations = max_iter;
        cfg.tolerance = tol;
        cfg.target_order = 9;              // L4 生产值（coverage target_order=9）
        cfg.sigma_floor = 1e-3;
        cfg.support_power = 1.0;
        cfg.quality_mode = 0;
        cfg.use_ivar_weight = 1;
        cfg.control_reliability = 1.0;
        cfg.input_manifest_hash = nullptr;
        cfg.cpu_workers = workers;
        cfg.grid = 8;
        if (gs_damping >= 0) cfg.gs_damping = (gs_damping == 0) ? 0.5 : 1.0;
        cfg.m_full_frame = m_full_frame;
        cfg.final_gauge = final_gauge;
        cfg.tolerance_relative = tol_rel;

        const auto t0 = std::chrono::steady_clock::now();
        void* m = nullptr;
        const int rc = p2_upm_build_geo(obs.data(), n_obs, nodes.data(), n_nodes, &cfg, &m);
        const auto t1 = std::chrono::steady_clock::now();
        const double secs = std::chrono::duration<double>(t1 - t0).count();
        if (rc != 0) {
            std::fprintf(js, "{\"lambda\":%.17g,\"rc\":%d,\"seconds\":%.3f}\n", ls, rc, secs);
            std::fflush(js);
            std::fprintf(stderr, "[sweep] lambda=%g rc=%d FAILED\n", ls, rc);
            continue;
        }
        std::uint64_t it = 0; double obj = 0.0; int conv = -1;
        p2_upm_convergence(m, &it, &obj, &conv);

        // C 场与校准场（逐 frame × node）
        // Z = calibrate_block(frame, leaf, input=真实观测值) = value - C - G（生产语义）
        std::vector<double> C((std::size_t)n_frames * n_nodes, 0.0);
        std::vector<double> Z((std::size_t)n_frames * n_nodes, 0.0);
        std::vector<std::uint64_t> fidx(n_frames);
        std::vector<std::vector<double>> val_fk((std::size_t)n_frames,
                                                std::vector<double>(n_nodes, 0.0));
        std::vector<std::vector<char>> has_fk((std::size_t)n_frames,
                                              std::vector<char>(n_nodes, 0));
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t ck = cid2node[(std::size_t)obs[i].control_id];
            if (ck == (std::size_t)-1) continue;
            std::size_t fi = 0; bool found = false;
            for (std::uint64_t t = 0; t < n_frames; ++t)
                if (frames[t] == obs[i].frame_id) { fi = (std::size_t)t; found = true; break; }
            if (!found) continue;
            val_fk[fi][ck] = obs[i].value; has_fk[fi][ck] = 1;
        }
        for (std::uint64_t fi = 0; fi < n_frames; ++fi) {
            const std::uint64_t fid = frames[fi];
            std::vector<std::uint64_t> ip(n_nodes);
            std::vector<double> iv(n_nodes, 0.0), ov(n_nodes, 0.0);
            for (std::uint64_t k = 0; k < n_nodes; ++k) {
                ip[k] = nl[k].leaf_ipix;
                iv[k] = val_fk[fi][k];
            }
            p2_upm_calibrate_block(m, fid, ip.data(), iv.data(), ov.data(), n_nodes);
            for (std::uint64_t k = 0; k < n_nodes; ++k) {
                Z[(std::size_t)fi * n_nodes + k] = ov[k];
                C[(std::size_t)fi * n_nodes + k] = p2_upm_evaluate_c(m, fid, ip[k]);
            }
        }
        // w_cell（per-obs 归一化权重，pre-Huber；与求解器同源公式）
        std::vector<double> wcell(n_obs, 0.0);
        const int wrc = p2_upm_normalized_weights(obs.data(), n_obs, &cfg, wcell.data());

        // 统计
        double cmax = 0.0, csum2 = 0.0; std::size_t cn = 0;
        for (double v : C) { if (!std::isfinite(v)) continue; cmax = std::max(cmax, std::fabs(v)); csum2 += v * v; ++cn; }
        const double crms = cn ? std::sqrt(csum2 / (double)cn) : 0.0;
        // 逐帧空间梯度 RMS（相邻 node，同 tile 4 邻域）——过平滑直接观测量
        double grad2 = 0.0; std::size_t gn = 0;
        {
            std::vector<std::size_t> idx_of(n_nodes);
            for (std::size_t i = 0; i < n_nodes; ++i) idx_of[i] = i;
            // 用 (tile,gx,gy) 建 4 邻域
            std::vector<std::int64_t> key(n_nodes);
            for (std::size_t i = 0; i < n_nodes; ++i)
                key[i] = (std::int64_t)((nl[i].tile_ipix & 0xFFFFFF) * 64 + nl[i].gy * 8 + nl[i].gx);
            std::vector<std::size_t> ord(n_nodes);
            for (std::size_t i = 0; i < n_nodes; ++i) ord[i] = i;
            std::sort(ord.begin(), ord.end(), [&](std::size_t a, std::size_t b) { return key[a] < key[b]; });
            for (std::size_t ii = 0; ii + 1 < n_nodes; ++ii) {
                const std::size_t a = ord[ii], b = ord[ii + 1];
                if (nl[a].tile_ipix != nl[b].tile_ipix) continue;
                if (nl[a].gy != nl[b].gy || nl[b].gx != nl[a].gx + 1) continue;
                for (std::uint64_t fi = 0; fi < n_frames; ++fi) {
                    const double d = C[(std::size_t)fi * n_nodes + a] - C[(std::size_t)fi * n_nodes + b];
                    grad2 += d * d; ++gn;
                }
            }
        }
        const double grad_rms = gn ? std::sqrt(grad2 / (double)gn) : 0.0;

        char bin[1024];
        std::snprintf(bin, sizeof(bin), "%s_lam%s.bin", out_prefix.c_str(), [&]{ char b[64]; std::snprintf(b, 64, "%.10g", ls); return std::string(b); }().c_str());
        FILE* bf = std::fopen(bin, "wb");
        std::uint32_t mg = 0x55504D32u; std::uint32_t vv = 1;
        std::fwrite(&mg, 4, 1, bf); std::fwrite(&vv, 4, 1, bf);
        std::fwrite(&n_frames, 8, 1, bf); std::fwrite(&n_nodes, 8, 1, bf);
        std::fwrite(&n_obs, 8, 1, bf);
        std::fwrite(frames.data(), 8, n_frames, bf);
        std::fwrite(C.data(), 8, C.size(), bf);
        std::fwrite(Z.data(), 8, Z.size(), bf);
        std::fwrite(wcell.data(), 8, n_obs, bf);
        std::fclose(bf);

        std::fprintf(js, "{\"lambda\":%.17g,\"rc\":%d,\"seconds\":%.3f,\"iterations\":%llu,"
                         "\"converged\":%d,\"objective\":%.10g,\"c_rms\":%.10g,\"c_max\":%.10g,"
                         "\"c_grad_rms\":%.10g,\"wcell_rc\":%d,\"bin\":\"%s\"}\n",
                     ls, rc, secs, (unsigned long long)it, conv, obj, crms, cmax, grad_rms, wrc, bin);
        std::fflush(js);   // 逐点落盘：中断也能保留已完成点
        std::fprintf(stderr, "[sweep] lambda=%-10g it=%llu conv=%d obj=%.6g crms=%.4g grad=%.4g %.1fs\n",
                     ls, (unsigned long long)it, conv, obj, crms, grad_rms, secs);
        p2_upm_close(m);
    }
    std::fclose(js);
    return 0;
}
