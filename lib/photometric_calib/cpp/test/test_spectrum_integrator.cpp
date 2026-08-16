// test_spectrum_integrator.cpp - 光谱积分 golden 比对测试程序
//
// 用途: 验证 spectrum_integrator.cpp 的 compute_f_syn / compute_f_syn_cached
// 在 修改后无回归. 仅改 star_matcher.cpp, 不触及
// spectrum_integrator.cpp; 本程序通过端到端调用确认数值稳定.
//
// 算法:
// 1. 内部硬编码生成 5 条 Planck 黑体合成 Gaia BP/RP uint8 光谱
// (T = 3500K, 4500K, 5800K, 7500K, 10000K), 采样到 [336, 338, ..., 1020] nm
// 2. 通过 argv 接收 filter/QE 数据文件 (简单 wl val 文本格式)
// 3. 对每条光谱 + 每种 filter/QE 组合:
// - 调用 compute_f_syn (无缓存版, 1nm 步长)
// - 调用 prepare_filter_cache + compute_f_syn_cached (缓存版, spectrum_wl 网格)
// 4. 输出 JSON 数组到 stdout (供 Python 比对)
//
// 编译:
// g++ -O2 -std=c++17 -Iinclude -Isrc \
// test/test_spectrum_integrator.cpp src/spectrum_integrator.cpp \
// -o test/test_spectrum_integrator.exe -lm
//
// 运行:
// test_spectrum_integrator.exe <filter_file> <qe_file|none> <mag_g>
//
// 输入文件格式 (filter/qe):
// 第一行: n_points
// 后续 n_points 行: <wl_nm> <value>
//
// 输出 (stdout, JSON 数组):
// [{"T": 3500.0, "mag_g": <mag>, "f_syn_uncached": <val>, "f_syn_cached": <val>}, ...]

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>

#include "spectrum_integrator.h"

using namespace photo_calib;

// ----------------------------------------------------------------------------
// 物理常数 (SI)
// ----------------------------------------------------------------------------
static constexpr double kPlanckH = 6.62607015e-34;   // J·s
static constexpr double kSpeedC = 2.99792458e8;     // m/s
static constexpr double kBoltzK = 1.380649e-23;     // J/K

// ----------------------------------------------------------------------------
// Planck 黑体辐射 B(λ, T) (W·sr^{-1}·m^{-2}·Hz^{-1}, 但用作相对形状)
// 公式: B(λ,T) = (2hc²/λ⁵) / (exp(hc/(λkT)) - 1)
// λ 单位: m
// ----------------------------------------------------------------------------
static double planck_radiance(double wl_nm, double T_kelvin) {
    double wl_m = wl_nm * 1e-9;
    double wl5 = std::pow(wl_m, 5);
    double exponent = (kPlanckH * kSpeedC) / (wl_m * kBoltzK * T_kelvin);
    if (exponent > 700.0) return 0.0; // exp 溢出保护
    double denom = std::exp(exponent) - 1.0;
    if (denom <= 0.0) return 0.0;
    return (2.0 * kPlanckH * kSpeedC * kSpeedC) / (wl5 * denom);
}

// ----------------------------------------------------------------------------
// 构造 Gaia BP/RP uint8 光谱 (343 点, 336~1020nm, 步长 2nm)
// 按最大值归一化到 [0, 255]
// ----------------------------------------------------------------------------
static std::vector<uint8_t> make_blackbody_spectrum(double T_kelvin,
                                                    std::vector<double>& wl_out) {
    const int n_points = 343;
    const double wl_start = 336.0;
    const double wl_step = 2.0;

    wl_out.resize(n_points);
    std::vector<double> b_vals(n_points);
    double b_max = 0.0;

    for (int i = 0; i < n_points; ++i) {
        double wl = wl_start + i * wl_step;
        wl_out[i] = wl;
        double b = planck_radiance(wl, T_kelvin);
        b_vals[i] = b;
        if (b > b_max) b_max = b;
    }

    std::vector<uint8_t> spec(n_points, 0);
    if (b_max <= 0.0) return spec;
    for (int i = 0; i < n_points; ++i) {
        double norm = (b_vals[i] / b_max) * 255.0;
        if (norm < 0.0) norm = 0.0;
        if (norm > 255.0) norm = 255.0;
        spec[i] = (uint8_t)std::round(norm);
    }
    return spec;
}

// ----------------------------------------------------------------------------
// 读取 wl val 文本文件 (第一行 n_points, 后续每行 wl val)
// ----------------------------------------------------------------------------
static bool read_wl_val_file(const char* path,
                             std::vector<double>& wl,
                             std::vector<double>& val) {
    FILE* f = std::fopen(path, "r");
    if (!f) {
        std::fprintf(stderr, "[ERROR] 无法打开文件: %s\n", path);
        return false;
    }
    int n_points = 0;
    if (std::fscanf(f, "%d", &n_points) != 1 || n_points <= 0) {
        std::fprintf(stderr, "[ERROR] 文件 %s 第一行应为点数\n", path);
        std::fclose(f);
        return false;
    }
    wl.clear();
    val.clear();
    wl.reserve(n_points);
    val.reserve(n_points);
    for (int i = 0; i < n_points; ++i) {
        double w, v;
        if (std::fscanf(f, "%lf %lf", &w, &v) != 2) {
            std::fprintf(stderr, "[ERROR] 文件 %s 第 %d 行格式错误\n", path, i + 2);
            std::fclose(f);
            return false;
        }
        wl.push_back(w);
        val.push_back(v);
    }
    std::fclose(f);
    return true;
}

// ----------------------------------------------------------------------------
// 主程序
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
            "Usage: %s <filter_file> <qe_file|none> <mag_g>\n"
            "\n"
            "  filter_file: 滤光片数据文件 (第一行点数, 后续每行 wl val)\n"
            "  qe_file:     QE 数据文件 (同上格式), 'none' 表示无 QE (Q=1)\n"
            "  mag_g:       Gaia G 星等 (浮点)\n"
            "\n"
            "输出 (stdout): JSON 数组, 每元素含 T/mag_g/f_syn_uncached/f_syn_cached\n",
            argv[0]);
        return 1;
    }

    const char* filter_path = argv[1];
    const char* qe_path = argv[2];
    double mag_g = std::atof(argv[3]);

    // ---- 读取 filter ----
    std::vector<double> filter_wl, filter_trans;
    if (!read_wl_val_file(filter_path, filter_wl, filter_trans)) {
        return 2;
    }
    int filter_count = (int)filter_wl.size();
    std::fprintf(stderr, "[INFO] filter: %d 点, 范围 [%.1f, %.1f] nm\n",
                 filter_count, filter_wl.front(), filter_wl.back());

    // ---- 读取 QE (可选) ----
    std::vector<double> qe_wl, qe_trans;
    bool has_qe = (std::strcmp(qe_path, "none") != 0);
    int qe_count = 0;
    if (has_qe) {
        if (!read_wl_val_file(qe_path, qe_wl, qe_trans)) {
            return 3;
        }
        qe_count = (int)qe_wl.size();
        std::fprintf(stderr, "[INFO] QE: %d 点, 范围 [%.1f, %.1f] nm\n",
                     qe_count, qe_wl.front(), qe_wl.back());
    } else {
        std::fprintf(stderr, "[INFO] QE: none (Q(λ)=1.0)\n");
    }

    // ---- 5 条黑体合成光谱 ----
    const double temps[] = {3500.0, 4500.0, 5800.0, 7500.0, 10000.0};
    const int n_temps = sizeof(temps) / sizeof(temps[0]);

    // ---- 准备 filter cache (用于 compute_f_syn_cached) ----
    // 注意: cache 与光谱波长网格绑定, 这里所有 5 条光谱共享同一波长网格
    std::vector<double> spec_wl_dummy;
    auto dummy_spec = make_blackbody_spectrum(temps[0], spec_wl_dummy);
    int spec_count = (int)spec_wl_dummy.size();

    SpectrumIntegratorCache cache = prepare_filter_cache(
        filter_wl.data(), filter_trans.data(), filter_count,
        has_qe ? qe_wl.data() : nullptr,
        has_qe ? qe_trans.data() : nullptr,
        qe_count,
        spec_wl_dummy.data(), spec_count);

    if (cache.spectrum_wl.empty()) {
        std::fprintf(stderr, "[ERROR] prepare_filter_cache 失败\n");
        return 4;
    }
    std::fprintf(stderr, "[INFO] cache 就绪: spectrum_wl %zu 点, filter_trans %zu 点, weighted_wl %zu 点\n",
                 cache.spectrum_wl.size(), cache.filter_trans.size(), cache.weighted_wl.size());

    // ---- 输出 JSON 数组 ----
    std::printf("[");
    for (int t_idx = 0; t_idx < n_temps; ++t_idx) {
        double T = temps[t_idx];
        std::vector<double> spec_wl;
        std::vector<uint8_t> spec = make_blackbody_spectrum(T, spec_wl);

        // 验证光谱波长网格与 cache 一致
        if ((int)spec_wl.size() != spec_count) {
            std::fprintf(stderr, "[ERROR] T=%.0fK 光谱波长数 %zu != %d\n",
                         T, spec_wl.size(), spec_count);
            return 5;
        }

        // ---- compute_f_syn (无缓存版, 1nm 步长) ----
        double f_syn_uncached = compute_f_syn(
            spec.data(), spec_count,
            spec_wl.data(), spec_count,
            filter_wl.data(), filter_trans.data(), filter_count,
            has_qe ? qe_wl.data() : nullptr,
            has_qe ? qe_trans.data() : nullptr,
            qe_count,
            mag_g);

        // ---- compute_f_syn_cached (缓存版, spectrum_wl 网格) ----
        double f_syn_cached = compute_f_syn_cached(cache, spec.data(), spec_count, mag_g);

        // ---- 输出 ----
        std::printf("%s{\n", t_idx > 0 ? "," : "");
        std::printf("  \"T_kelvin\": %.1f,\n", T);
        std::printf("  \"mag_g\": %.6f,\n", mag_g);
        std::printf("  \"spec_count\": %d,\n", spec_count);
        std::printf("  \"f_syn_uncached\": %.10e,\n", f_syn_uncached);
        std::printf("  \"f_syn_cached\": %.10e\n", f_syn_cached);
        std::printf("}");
    }
    std::printf("]\n");

    return 0;
}
