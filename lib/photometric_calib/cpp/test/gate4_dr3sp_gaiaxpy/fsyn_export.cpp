// fsyn_export.cpp - Gate 4 (Phase1 v2/v3): 生产 compute_f_syn 导出工具
//
// 用途: 供 Python 验证 numpy 移植版 (fsyn_astrocs.py) 与生产 C++
//       compute_f_syn_cached / compute_f_syn_cached_xpsd 数值一致
//       (同一 uint8 光谱 + flux_min/flux_mul + 同一 filter/QE)。
//
// 用法:
//   fsyn_export.exe <filter_file> <qe_file|none> <mag_g>
//  stdin: 343 个 uint8 (空格分隔, 即 XPSD 光谱网格 336-1020nm @2nm)
//  stdout: f_syn (double)
//
//   fsyn_export.exe xpsd <filter_file> <qe_file|none> <flux_min> <flux_mul>
//  stdin: 343 个 uint8; stdout: f_syn (官方线性解码 + 绝对单位积分)
//
// filter/qe 文件格式 (与 test_spectrum_integrator 一致):
//   第一行: n_points
//   后续: <wl_nm> <value>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#include "spectrum_integrator.h"

using namespace photo_calib;

static bool load_curve(const char* path, std::vector<double>& wl,
                       std::vector<double>& val) {
    FILE* f = std::fopen(path, "r");
    if (!f) return false;
    int n = 0;
    if (std::fscanf(f, "%d", &n) != 1 || n <= 0) { std::fclose(f); return false; }
    wl.resize((size_t)n);
    val.resize((size_t)n);
    for (int i = 0; i < n; ++i) {
        if (std::fscanf(f, "%lf %lf", &wl[(size_t)i], &val[(size_t)i]) != 2) {
            std::fclose(f);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

int main(int argc, char** argv) {
    bool xpsd_mode = (argc >= 2 && std::string(argv[1]) == "xpsd");
    int arg_off = xpsd_mode ? 1 : 0;
    if (xpsd_mode ? (argc < 6) : (argc < 4)) {
        std::fprintf(stderr,
            "usage: fsyn_export <filter_file> <qe_file|none> <mag_g>\n"
            "       fsyn_export xpsd <filter_file> <qe_file|none> <flux_min> <flux_mul>\n");
        return 2;
    }
    std::vector<double> filter_wl, filter_trans, qe_wl, qe_trans;
    if (!load_curve(argv[1 + arg_off], filter_wl, filter_trans)) {
        std::fprintf(stderr, "filter load failed: %s\n", argv[1 + arg_off]);
        return 3;
    }
    bool has_qe = std::string(argv[2 + arg_off]) != "none";
    if (has_qe && !load_curve(argv[2 + arg_off], qe_wl, qe_trans)) {
        std::fprintf(stderr, "qe load failed: %s\n", argv[2 + arg_off]);
        return 4;
    }
    double mag_g = xpsd_mode ? 0.0 : std::atof(argv[3 + arg_off]);
    double flux_min = 0.0, flux_mul = 0.0;
    if (xpsd_mode) {
        flux_min = std::atof(argv[3 + arg_off]);
        flux_mul = std::atof(argv[4 + arg_off]);
    }

    // 光谱波长网格 (与 XPSD / spectrum_wl 一致)
    const int kSpectrumCount = 343;
    std::vector<double> spectrum_wl((size_t)kSpectrumCount);
    for (int i = 0; i < kSpectrumCount; ++i) {
        spectrum_wl[(size_t)i] = 336.0 + 2.0 * i;
    }

    std::vector<uint8_t> spec((size_t)kSpectrumCount);
    for (int i = 0; i < kSpectrumCount; ++i) {
        int v = 0;
        if (std::scanf("%d", &v) != 1) {
            std::fprintf(stderr, "stdin: 期望 %d 个 uint8\n", kSpectrumCount);
            return 5;
        }
        spec[(size_t)i] = (uint8_t)v;
    }

    SpectrumIntegratorCache cache = prepare_filter_cache(
        filter_wl.data(), filter_trans.data(), (int)filter_wl.size(),
        has_qe ? qe_wl.data() : nullptr,
        has_qe ? qe_trans.data() : nullptr,
        has_qe ? (int)qe_wl.size() : 0,
        spectrum_wl.data(), kSpectrumCount);
    if (cache.spectrum_wl.empty()) {
        std::fprintf(stderr, "cache prep failed\n");
        return 6;
    }
    double f_syn = xpsd_mode
        ? compute_f_syn_cached_xpsd(cache, spec.data(), kSpectrumCount, flux_min, flux_mul)
        : compute_f_syn_cached(cache, spec.data(), kSpectrumCount, mag_g);
    std::printf("%.17g\n", f_syn);
    return 0;
}
