// ============================================================================
// p1psf_fitcost_probe.cpp — PSF-DIAG-001 测量探针（落盘测量的执行面）
// ----------------------------------------------------------------------------
// 上游: AGENTS.md §3（重计算前先 df -h + 套 mem_guard；先落盘测量再谈优化）、
//       ENGINEERING_SPEC.md §7（运行产物区登记）。
// 目的: 用**生产同一份** PSF 拟合源（lib/algorithms/psf/src/dpsf_psf.cpp，
//       仅多编一个 DPSF_FIT_DIAG 宏）逐候选落盘：
//         · 候选数 / 拟合调用数（= 全部候选，节点传 n_fit_limit=0）
//         · 逐候选耗时、LM 迭代数、退化阶段、局部背景与峰值
//         · 批拟合总墙钟、n_valid
//       并落盘检测侧（LM 无关）的候选质量字段：矩 FWHM / 椭率 / 检测 SNR /
//       质量位 —— 用于把「候选本身是不是星」与「拟合器快不快」分开判定。
// 被测面同源: lib/infrastructure/scheduler/src/module_adapters.cpp 的
//       p1_op_star_psf_impl 盲路径（acsd::phase1::StarDetector(5.0).detect()
//       → dets[N,6] → dpsf_fit_batch_f64(..., nullptr, ...)）。
// 注意: 本探针**不是**门；它只落盘测量，不做判据。判据在 p1psf 测试组。
// ============================================================================

#include "dynamic_psf.h"

#include "star_detector.h"

#include "astro_image_io.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Args {
    std::string image;
    std::string out;          // 输出前缀（空 = 只打印摘要）
    std::string cands_in;     // 已有候选 CSV（重放）
    double det_sigma = 5.0;
    bool serial = false;      // 逐候选串行拟合（单线程耗时分布）
    bool detect_only = false;
    int inject = 0;           // 注入 N 个合成的 PSF 真值源（阳性对照）
    double inj_amp = 400.0;
    double inj_fwhm = 2.5;
};

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    size_t p = 0;
    while (p <= s.size()) {
        size_t q = s.find(',', p);
        if (q == std::string::npos) { out.push_back(s.substr(p)); break; }
        out.push_back(s.substr(p, q - p));
        p = q + 1;
    }
    return out;
}

struct Cand {
    double x = 0.0, y = 0.0, flux = 0.0, mag = 0.0;
    double sat = 0.0, has_sat = 0.0;
    double fwhm_px = 0.0, ell = 0.0, snr = 0.0, quality = 0.0;
};

struct InjTruth {
    double x = 0.0, y = 0.0, A = 0.0, sx = 0.0, sy = 0.0, theta = 0.0, B = 0.0;
};

inline double now_s() {
    return std::chrono::duration<double>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

}  // namespace

int main(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : std::string(); };
        if (k == "--image") a.image = next();
        else if (k == "--out") a.out = next();
        else if (k == "--cands-in") a.cands_in = next();
        else if (k == "--det-sigma") a.det_sigma = std::atof(next().c_str());
        else if (k == "--serial") a.serial = true;
        else if (k == "--detect-only") a.detect_only = true;
        else if (k == "--inject") a.inject = std::atoi(next().c_str());
        else if (k == "--inj-amp") a.inj_amp = std::atof(next().c_str());
        else if (k == "--inj-fwhm") a.inj_fwhm = std::atof(next().c_str());
        else { std::fprintf(stderr, "unknown arg: %s\n", k.c_str()); return 2; }
    }
    if (a.image.empty()) { std::fprintf(stderr, "usage: --image <fits>\n"); return 2; }

    // ── ① 读帧（与节点同一读入口 aio_read）──────────────────────────────
    const double t_read0 = now_s();
    AIOImageData* img = aio_read(a.image.c_str());
    const double t_read1 = now_s();
    if (!img) { std::fprintf(stderr, "aio_read failed: %s\n", a.image.c_str()); return 3; }
    const int W = aio_get_geometry(img).width;
    const int H = aio_get_geometry(img).height;
    float* fpx = aio_get_pixel_data(img);
    const size_t NP = static_cast<size_t>(W) * static_cast<size_t>(H);
    std::vector<double> dbuf(NP);
    for (size_t i = 0; i < NP; ++i) dbuf[i] = static_cast<double>(fpx[i]);
    std::fprintf(stdout,
                 "PROBE image=%s %dx%d read_s=%.3f\n", a.image.c_str(), W, H, t_read1 - t_read0);

    // ── ② 可选: 注入合成 PSF 真值源（阳性对照，按 PSF.md §16 sigma 口径）──
    std::vector<InjTruth> truth;
    if (a.inject > 0) {
        const double sigma = a.inj_fwhm / 1.230310;   // FWHM=1.230310*sigma（冻结系数）
        double bg = 0.0;
        {   // 用中位数粗估背景（仅用于注入，不参与判据）
            std::vector<double> v(dbuf.begin(), dbuf.end());
            std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
            bg = v[v.size() / 2];
        }
        unsigned long long seed = 20260930ULL;
        auto rnd = [&]() { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                           return static_cast<double>((seed >> 11) & 0xFFFFFF) / 16777216.0; };
        for (int k = 0; k < a.inject; ++k) {
            // 网格 + 抖动: 保证注入源彼此 3x3 去重后仍独立
            const int gx = 8 + (k % 40) * 100;
            const int gy = 8 + (k / 40) * 100;
            if (gx + 20 >= W || gy + 20 >= H) break;
            InjTruth t;
            t.x = gx + 8.0 * rnd();
            t.y = gy + 8.0 * rnd();
            t.A = a.inj_amp;
            t.sx = sigma; t.sy = sigma; t.theta = 0.0;
            t.B = bg;
            truth.push_back(t);
            for (int y = gy; y < gy + 17; ++y)
                for (int x = gx; x < gx + 17; ++x) {
                    const double dx = x - t.x, dy = y - t.y;
                    const double Q = (dx * dx + dy * dy) / (2.0 * t.sx * t.sx);
                    const double v = t.B + t.A / std::pow(1.0 + Q, 4.0);
                    dbuf[static_cast<size_t>(y) * W + x] += (v - t.B);
                }
        }
        std::fprintf(stdout, "PROBE injected=%zu amp=%.1f fwhm=%.3f\n", truth.size(), a.inj_amp, a.inj_fwhm);
    }

    // ── ③ 候选：盲检测（与节点同参）或重放已有候选 ──────────────────────
    std::vector<Cand> cands;
    double bg = 0.0, sigma = 0.0;
    if (!a.cands_in.empty()) {
        FILE* f = std::fopen(a.cands_in.c_str(), "r");
        if (!f) { std::fprintf(stderr, "open cands failed\n"); return 3; }
        char line[1024];
        bool first = true;
        while (std::fgets(line, sizeof(line), f)) {
            if (first) { first = false; if (std::strstr(line, "x,y") != nullptr) continue; }
            auto tok = split_csv(line);
            if (tok.size() < 6) continue;
            Cand c;
            c.x = std::atof(tok[0].c_str());  c.y = std::atof(tok[1].c_str());
            c.flux = std::atof(tok[2].c_str());
            if (tok.size() > 8) { c.fwhm_px = std::atof(tok[8].c_str()); }
            if (tok.size() > 9) { c.ell = std::atof(tok[9].c_str()); }
            if (tok.size() > 10) { c.snr = std::atof(tok[10].c_str()); }
            cands.push_back(c);
        }
        std::fclose(f);
    } else {
        const double t0 = now_s();
        acsd::phase1::StarDetector det(a.det_sigma);
        auto r = det.detect(fpx, W, H);
        const double t1 = now_s();
        if (r.failed()) { std::fprintf(stderr, "detect failed\n"); return 3; }
        const auto& cat = r.value();
        bg = cat.background; sigma = cat.noise_sigma;
        for (const auto& s : cat.sources) {
            Cand c;
            c.x = s.x; c.y = s.y; c.flux = s.flux;
            c.mag = (s.flux > 0.0) ? -2.5 * std::log10(s.flux) : 99.0;
            c.sat = (s.quality & 1) ? 1.0 : 0.0;
            c.has_sat = (cat.n_saturated > 0) ? 1.0 : 0.0;
            c.fwhm_px = s.fwhm_px; c.ell = s.ellipticity; c.snr = s.snr;
            c.quality = static_cast<double>(s.quality);
            cands.push_back(c);
        }
        std::fprintf(stdout,
                     "PROBE detect_s=%.3f n_detected=%zu bg=%.4f noise_sigma=%.4f thr=%.4f\n",
                     t1 - t0, cands.size(), bg, sigma, bg + a.det_sigma * sigma);
    }
    const size_t N = cands.size();

    // ── ④ 落盘候选（含检测侧 LM 无关字段）───────────────────────────────
    if (!a.out.empty() && a.cands_in.empty()) {
        FILE* f = std::fopen((a.out + ".cands.csv").c_str(), "w");
        if (f) {
            std::fprintf(f, "x,y,flux,mag,sat,has_sat,fwhm_px_det,ell_det,snr_det,quality\n");
            for (const auto& c : cands)
                std::fprintf(f, "%.6f,%.6f,%.6f,%.6f,%.1f,%.1f,%.6f,%.6f,%.6f,%.0f\n",
                             c.x, c.y, c.flux, c.mag, c.sat, c.has_sat,
                             c.fwhm_px, c.ell, c.snr, c.quality);
            std::fclose(f);
        }
    }
    if (a.detect_only || N == 0) {
        aio_free_image_data(img);
        std::fprintf(stdout, "PROBE done (detect-only) n=%zu\n", N);
        return 0;
    }

    // ── ⑤ dets[N,6]（与节点逐字段同构）─────────────────────────────────
    std::vector<double> dets(N * 6, 0.0);
    for (size_t k = 0; k < N; ++k) {
        dets[k * 6 + 0] = cands[k].x;
        dets[k * 6 + 1] = cands[k].y;
        dets[k * 6 + 2] = cands[k].flux;
        dets[k * 6 + 3] = cands[k].mag;
        dets[k * 6 + 4] = cands[k].sat;
        dets[k * 6 + 5] = cands[k].has_sat;
    }

    // ── ⑥ 批拟合（生产入口，params=nullptr ⇒ fitRadius=8/200 迭代/1e-8）──
    std::vector<double> psf_params(N * 9, 0.0);
    std::vector<int> status(N, DPSF_PSF_STATUS_FIT_FAILED);
    int n_valid = 0;
    const double tb0 = now_s();
    const int rc = dpsf_fit_batch_f64(dbuf.data(), W, H, dets.data(), static_cast<int>(N),
                                      nullptr, psf_params.data(), &n_valid, status.data());
    const double tb1 = now_s();
    std::fprintf(stdout, "PROBE batch rc=%d n=%zu n_valid=%d wall_s=%.3f cand_per_s=%.1f\n",
                 rc, N, n_valid, tb1 - tb0,
                 static_cast<double>(N) / (tb1 - tb0 + 1e-9));

    // ── ⑦ 逐候选状态落盘 ────────────────────────────────────────────────
    if (!a.out.empty()) {
        FILE* f = std::fopen((a.out + ".batch.csv").c_str(), "w");
        if (f) {
            std::fprintf(f, "idx,x,y,status\n");
            for (size_t k = 0; k < N; ++k)
                std::fprintf(f, "%zu,%.6f,%.6f,%d\n", k, cands[k].x, cands[k].y, status[k]);
            std::fclose(f);
        }
    }

    // ── ⑧ 可选: 逐候选串行拟合（单线程真实耗时；调用方须 OMP_NUM_THREADS=1）──
    if (a.serial) {
        std::vector<double> one(9, 0.0);
        std::vector<int> st1(1, 0);
        int nv1 = 0;
        double worst = 0.0, total = 0.0;
        std::vector<double> per(N, 0.0);
        for (size_t k = 0; k < N; ++k) {
            const double t0 = now_s();
            (void)dpsf_fit_batch_f64(dbuf.data(), W, H, dets.data() + k * 6, 1,
                                     nullptr, one.data(), &nv1, st1.data());
            const double dt = now_s() - t0;
            per[k] = dt; total += dt; if (dt > worst) worst = dt;
        }
        std::vector<double> srt(per);
        std::sort(srt.begin(), srt.end());
        auto q = [&](double p) { return srt[static_cast<size_t>(p * (srt.size() - 1))]; };
        std::fprintf(stdout,
                     "PROBE serial n=%zu total_s=%.3f mean_ms=%.3f p50_ms=%.3f p90_ms=%.3f "
                     "p99_ms=%.3f max_ms=%.3f\n",
                     N, total, 1e3 * total / static_cast<double>(N), 1e3 * q(0.50),
                     1e3 * q(0.90), 1e3 * q(0.99), 1e3 * worst);
        if (!a.out.empty()) {
            FILE* f = std::fopen((a.out + ".serial.csv").c_str(), "w");
            if (f) {
                std::fprintf(f, "idx,ms\n");
                for (size_t k = 0; k < N; ++k) std::fprintf(f, "%zu,%.6f\n", k, 1e3 * per[k]);
                std::fclose(f);
            }
        }
    }

    // ── ⑨ 注入真值的回收对照（阳性对照；只看注入位置附近是否有 OK 拟合）──
    if (!truth.empty()) {
        int hit = 0, fit_ok = 0;
        for (const auto& t : truth) {
            size_t best = 0; double bestd = 1e30;
            for (size_t k = 0; k < N; ++k) {
                const double d = std::hypot(cands[k].x - t.x, cands[k].y - t.y);
                if (d < bestd) { bestd = d; best = k; }
            }
            if (bestd <= 1.5) {
                ++hit;
                if (status[best] == DPSF_PSF_STATUS_OK) ++fit_ok;
            }
        }
        std::fprintf(stdout, "PROBE inject_truth n=%zu matched=%d fit_ok=%d\n",
                     truth.size(), hit, fit_ok);
    }

    aio_free_image_data(img);
    std::fprintf(stdout, "PROBE done n=%zu n_valid=%d\n", N, n_valid);
    return 0;
}
