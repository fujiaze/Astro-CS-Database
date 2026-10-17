// Issue #12 —— 真实 Gaia XPSD × 真实滤镜/QE 的 F_syn / 零点 / 光子率回归。
//
// 只调用生产核 prepare_filter_cache + compute_f_syn_cached_xpsd（被测源码由 --impl 决定，
// 见 run_gaia_fsyn_regression.py），从**真实** gaia/GaiaDR3SP 锥形搜索取星与光谱、
// 用真实响应曲线文件（config/response_curves/*.json，经 Python 展平成两列文本）。
//
// 输出:
//   stdout  : 网格元数据 N/x0/x_last/min|Δx|/max|Δx|/max|Δx-h|/tau + cache 状态
//   CSV     : i,source_id,ra,dec,magG,flux_min,flux_mul,F_syn,zp_syn,Q_gamma,Q_gamma_trapezoid,rel
//
// 用法:
//   gaia_fsyn_regression <gaia_dir> <ra> <dec> <radius_deg> <mag_low> <mag_high> \
//                        <filter_txt> [qe_txt|-] <out_csv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "spectrum_integrator.h"

extern "C" {
#include "gaia_client.h"
}

using photo_calib::SpectrumIntegratorCache;
using photo_calib::compute_f_syn_cached_xpsd;
using photo_calib::prepare_filter_cache;

namespace {
constexpr double kHc = 6.62607015e-34 * 2.99792458e8;  // J·m

std::vector<double> read_two_col(const std::string& path, std::vector<double>& val) {
    std::vector<double> wl;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); return wl; }
    char line[512];
    while (std::fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        double a = 0.0, b = 0.0;
        if (std::sscanf(line, "%lf %lf", &a, &b) == 2) { wl.push_back(a); val.push_back(b); }
    }
    std::fclose(f);
    return wl;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 10) {
        std::fprintf(stderr,
            "usage: %s <gaia_dir> <ra> <dec> <radius> <mag_low> <mag_high> "
            "<filter_txt> <qe_txt|-> <out_csv>\n", argv[0]);
        return 2;
    }
    const std::string gaia_dir = argv[1];
    const double ra = std::atof(argv[2]);
    const double dec = std::atof(argv[3]);
    const double radius = std::atof(argv[4]);
    const double mag_low = std::atof(argv[5]);
    const double mag_high = std::atof(argv[6]);
    const std::string filter_txt = argv[7];
    const std::string qe_txt = argv[8];
    const std::string out_csv = argv[9];

    GaiaClient* client = gaia_client_create(gaia_dir.c_str());
    if (!client) { std::fprintf(stderr, "gaia_client_create failed: %s\n", gaia_dir.c_str()); return 3; }
    int wl_start = 0, wl_step = 0, wl_count = 0;
    const int prc = gaia_client_get_spectrum_params(client, &wl_start, &wl_step, &wl_count);
    if (prc != 1 || wl_count <= 0) {
        std::fprintf(stderr, "gaia_client_get_spectrum_params failed rc=%d\n", prc);
        gaia_client_destroy(client);
        return 3;
    }
    std::vector<double> spectrum_wl(static_cast<size_t>(wl_count));
    for (int i = 0; i < wl_count; ++i)
        spectrum_wl[static_cast<size_t>(i)] = static_cast<double>(wl_start + i * wl_step);

    // 真实网格元数据 (从产品读取, 不硬编码)
    const int n_int = wl_count - 1;
    const double h = (spectrum_wl.back() - spectrum_wl.front()) / static_cast<double>(n_int);
    double dmin = 1e300, dmax = 0.0, dev = 0.0;
    for (int i = 1; i < wl_count; ++i) {
        const double d = spectrum_wl[static_cast<size_t>(i)] - spectrum_wl[static_cast<size_t>(i - 1)];
        dmin = std::min(dmin, d);
        dmax = std::max(dmax, d);
        dev = std::max(dev, std::fabs(d - h));
    }
    const double eps = 2.220446049250313e-16;
    const double M = std::max(std::max(std::fabs(spectrum_wl.front()), std::fabs(spectrum_wl.back())), h);
    const double tau = std::min(1e-8 * h, 16.0 * eps * M);
    std::printf("grid: N=%d x0=%d x_last=%d step_nm=%d h=%.10g min_dx=%.10g max_dx=%.10g "
                "max_dev=%.3e tau=%.3e res16epsM=%.3e reltol=%.3e\n",
                wl_count, wl_start, wl_start + n_int * wl_step, wl_step, h, dmin, dmax, dev, tau,
                16.0 * eps * M, 1e-8 * h);

    std::vector<double> fwl, ftv, qwl, qtv;
    fwl = read_two_col(filter_txt, ftv);
    const bool has_qe = (qe_txt != "-");
    if (has_qe) qwl = read_two_col(qe_txt, qtv);
    std::printf("curves: filter=%zu pts qe=%zu pts (%s)\n", fwl.size(), qwl.size(),
                has_qe ? "on" : "off");
    if (fwl.size() < 2) { gaia_client_destroy(client); return 3; }

    const SpectrumIntegratorCache cache = prepare_filter_cache(
        fwl.data(), ftv.data(), static_cast<int>(fwl.size()),
        has_qe ? qwl.data() : nullptr, has_qe ? qtv.data() : nullptr,
        static_cast<int>(qwl.size()),
        spectrum_wl.data(), wl_count);
    if (cache.spectrum_wl.empty()) {
        std::fprintf(stderr, "prepare_filter_cache: INVALID_SPECTRAL_GRID (real Gaia grid rejected)\n");
        gaia_client_destroy(client);
        return 4;
    }

    GaiaSpectrumStar* stars = nullptr;
    uint8_t* spectra = nullptr;
    int n = 0;
    const int rc = gaia_client_cone_search_with_spectrum(
        client, ra, dec, radius, mag_low, mag_high, &stars, &spectra, &n);
    std::printf("cone_search rc=%d n=%d\n", rc, n);
    if (rc != 0 || stars == nullptr || spectra == nullptr || n <= 0) {
        if (stars) std::free(stars);
        if (spectra) std::free(spectra);
        gaia_client_destroy(client);
        return 5;
    }

    FILE* csv = std::fopen(out_csv.c_str(), "w");
    if (!csv) { std::free(stars); std::free(spectra); gaia_client_destroy(client); return 3; }
    std::fprintf(csv, "i,ra,dec,magG,flux_min,flux_mul,F_syn,zp_syn,Q_gamma,Q_gamma_trap,rel_trap\n");

    int n_ok = 0, n_nan = 0;
    for (int i = 0; i < n; ++i) {
        const uint8_t* sp = spectra + static_cast<size_t>(i) * static_cast<size_t>(wl_count);
        const double fsyn = compute_f_syn_cached_xpsd(
            cache, sp, wl_count, stars[i].flux_min, stars[i].flux_mul);
        // 独立梯形（本程序内实现, 不复用 simpson_integrate）核对单位/量级
        double trap = 0.0;
        for (int k = 0; k + 1 < wl_count; ++k) {
            const double f0 = static_cast<double>(sp[k]) * stars[i].flux_mul + stars[i].flux_min;
            const double f1 = static_cast<double>(sp[k + 1]) * stars[i].flux_mul + stars[i].flux_min;
            const double d = spectrum_wl[static_cast<size_t>(k + 1)] - spectrum_wl[static_cast<size_t>(k)];
            trap += 0.5 * d * (f0 * cache.weighted_wl[static_cast<size_t>(k)] +
                               f1 * cache.weighted_wl[static_cast<size_t>(k + 1)]);
        }
        const double q_gamma = std::isfinite(fsyn) ? fsyn * 1e-9 / kHc : std::nan("");
        const double q_trap = std::isfinite(trap) ? trap * 1e-9 / kHc : std::nan("");
        const double rel = (std::isfinite(q_gamma) && std::isfinite(q_trap) && q_trap != 0.0)
                               ? std::fabs(q_gamma - q_trap) / std::fabs(q_trap) : std::nan("");
        const double zp = (std::isfinite(fsyn) && fsyn > 0.0 && std::isfinite(stars[i].magG))
                              ? stars[i].magG + 2.5 * std::log10(fsyn) : std::nan("");
        std::fprintf(csv, "%d,%.10f,%.10f,%.6f,%.8g,%.8g,%.12g,%.12g,%.12g,%.12g,%.6g\n",
                     i, stars[i].ra, stars[i].dec, stars[i].magG,
                     stars[i].flux_min, stars[i].flux_mul, fsyn, zp, q_gamma, q_trap, rel);
        if (std::isfinite(fsyn)) ++n_ok; else ++n_nan;
    }
    std::fclose(csv);
    std::printf("stars: n=%d finite_fsyn=%d nan_fsyn=%d -> %s\n", n, n_ok, n_nan, out_csv.c_str());
    std::free(stars);
    std::free(spectra);
    gaia_client_destroy(client);
    return 0;
}
