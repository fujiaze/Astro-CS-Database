// ============================================================================
// kcorr_matrix_test.cpp — K_CORR_DOMAIN
//
// 测 k_corr 对 Drizzle 参数的适用域：
// pixfrac ∈ {0.5, 0.8, 1.0}
// input/output sampling ratio 2 档（像素角尺度 300"/px 与 600"/px）
// patch retained N 至少 2 档（输出 patch 尺度）
// 结论落入选项 A（差异可忽略/共同因子在 per-control normalization 消去，
// 并强制 Phase2 group 的 Drizzle 参数一致）或选项 B（k_corr 作
// per-frame quantity）——证据 JSON 由本测试写出至
// 实验/engineering-evidence/science/kcorr_matrix.json（judge 判定 + 六格测量
// 全量 + 非退化负例面结果）。
//
// 非退化判据纪律（判据有牙）：
//   judge() 对空集/缺基线格/任一格 k_corr 非正或非有限/零 N 判红（返回 1），
//   main 里判红即 return 1；负例自测（judge 的红绿两面）先行，任一不符
//   return 1。测量链路坏 = 红，与 A/B 科学结论方向无关。
// ============================================================================
#include "drizzle_engine.h"
#include "fits_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

using namespace drizzle;

namespace {

constexpr int W = 20, H = 20;
constexpr int NSIDE = 512;
constexpr double SKY = 1000.0;
constexpr double SIGMA = 10.0;
constexpr std::size_t EXPECTED_CELLS = 6;  // 3 pixfrac × 2 scale
const char* EVIDENCE_JSON = "实验/engineering-evidence/science/kcorr_matrix.json";

void setup_wcs(FitsImage& im, double deg_per_px) {
    im.width = W;
    im.height = H;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = 10.0;
    im.wcs.crval[1] = 20.0;
    im.wcs.crpix[0] = (double)W * 0.5 + 0.5;
    im.wcs.crpix[1] = (double)H * 0.5 + 0.5;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
}

double median_of(std::vector<double> v) {
    if (v.empty()) return 0.0;
    const std::size_t n = v.size();
    const std::size_t mid = n / 2;
    std::nth_element(v.begin(), v.begin() + mid, v.end());
    if (n % 2 == 1) return v[mid];
    const double a = v[mid];
    const double b = *std::max_element(v.begin(), v.begin() + mid);
    return 0.5 * (a + b);
}

struct MatrixCell {
    double pixfrac;
    double scale_arcsec;
    double k_corr;
    double n_retained;
    double n_eff;
};

// A/B 判定 + 非退化判据。返回 0 = 测量有效（给出 *max_dev_out），1 = 退化。
// 退化面：格数 != 6（含空集）/ 基线格 (0.8, 300") 缺失 / 任一格
// k_corr 非正或非有限 / 任一格 n_retained 或 n_eff 非正。
int judge(const std::vector<MatrixCell>& cells, double* max_dev_out) {
    if (cells.size() != EXPECTED_CELLS) return 1;
    const MatrixCell* base = nullptr;
    for (const auto& c : cells)
        if (c.pixfrac == 0.8 && c.scale_arcsec == 300.0) base = &c;
    if (!base) return 1;
    for (const auto& c : cells) {
        if (!std::isfinite(c.k_corr) || !(c.k_corr > 0.0)) return 1;
        if (!(c.n_retained > 0.0) || !(c.n_eff > 0.0)) return 1;
    }
    double max_dev = 0.0;
    for (const auto& c : cells)
        max_dev = std::max(max_dev,
                           std::fabs(c.k_corr - base->k_corr) / base->k_corr);
    if (max_dev_out) *max_dev_out = max_dev;
    return 0;
}

// judge 的红绿两面自测（判据非退化的机器证据）。返回 0 = 判据两面全对。
int judge_selftest() {
    struct NegCase {
        const char* name;
        std::vector<MatrixCell> cells;
    };
    const double nan_v = std::nan("");
    std::vector<NegCase> negs = {
        {"empty-set", {}},
        {"missing-baseline", {{0.5, 300.0, 1.05, 400.0, 100.0},
                              {1.0, 600.0, 1.06, 400.0, 99.0}}},
        {"nonpositive-k", {{0.8, 300.0, 0.0, 400.0, 100.0},
                           {0.5, 300.0, 1.05, 400.0, 99.0}}},
        {"nonfinite-k", {{0.8, 300.0, nan_v, 400.0, 100.0},
                         {0.5, 300.0, 1.05, 400.0, 99.0}}},
        {"zero-n-retained", {{0.8, 300.0, 1.05, 0.0, 100.0},
                             {0.5, 300.0, 1.05, 400.0, 99.0}}},
        {"zero-n-eff", {{0.8, 300.0, 1.05, 400.0, 0.0},
                        {0.5, 300.0, 1.05, 400.0, 99.0}}},
    };
    for (const auto& nc : negs) {
        if (judge(nc.cells, nullptr) == 0) {
            std::printf("[NEG-FAIL] judge must reject degenerate case: %s\n",
                        nc.name);
            return 1;
        }
    }
    // 正例：6 格、基线在位、k 全正有限（偏差 2.4% → 选项 A 面）。
    std::vector<MatrixCell> good = {
        {0.5, 300.0, 1.024, 400.0, 100.0},
        {0.8, 300.0, 1.000, 400.0, 100.0},
        {1.0, 300.0, 1.012, 400.0, 100.0},
        {0.5, 600.0, 1.005, 400.0, 100.0},
        {0.8, 600.0, 0.995, 400.0, 100.0},
        {1.0, 600.0, 1.018, 400.0, 100.0},
    };
    double md = -1.0;
    if (judge(good, &md) != 0 || !(md >= 0.0) || md > 0.10) {
        std::printf("[NEG-FAIL] judge must accept well-formed 6-cell set\n");
        return 1;
    }
    return 0;
}

}  // namespace

int main() {
    if (judge_selftest() != 0) return 1;
    std::printf("[NEG] judge 红绿两面自测通过（6 退化负例判红 + 1 正例判绿）\n");

    const int NMC = 1000;
    const double pi = 3.14159265358979323846;
    const double pixfracs[] = {0.5, 0.8, 1.0};
    const double scales[] = {300.0, 600.0};   // "/px（2 档采样比）
    std::vector<MatrixCell> cells;

    for (double pf : pixfracs) {
        for (double sc : scales) {
            const double deg_per_px = sc / 3600.0;
            std::vector<double> meds, sigs;
            std::vector<int> ns;
            meds.reserve(NMC); sigs.reserve(NMC); ns.reserve(NMC);
            DrizzleEngine eng;
            DrizzleConfig cfg;
            cfg.nside = NSIDE;
            cfg.nested = true;
            cfg.pixfrac = pf;
            cfg.threads = 1;
            cfg.apply_photometry = true;
            cfg.photometry_applied_upstream = true;
            cfg.tile_depth = 9;
            for (int r = 0; r < NMC; ++r) {
                FitsImage im;
                setup_wcs(im, deg_per_px);
                im.pixels.assign((std::size_t)H * W, (float)SKY);
                std::mt19937 rng((unsigned)(20260816 + (int)(pf * 100) +
                                            (int)sc + r));
                std::normal_distribution<double> nd(0.0, SIGMA);
                for (auto& v : im.pixels) v += (float)nd(rng);
                std::vector<TileAccumulatorT<float>> tiles;
                DrizzleStats st;
                std::string err;
                if (!eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                                      tiles, st, err)) {
                    std::printf("[FAIL] drizzleTiled 返回失败（pf=%.1f "
                                "sc=%.0f err=%s）\n", pf, sc, err.c_str());
                    return 1;
                }
                std::vector<double> patch;
                for (const auto& tile : tiles) {
                    if (tile.touched.empty()) continue;
                    for (uint32_t local : tile.touched) {
                        const auto& a = tile.pixels[(size_t)local];
                        if (a.sumArea <= 0.0) continue;
                        patch.push_back((double)a.sumFlux / (double)a.sumArea);
                    }
                    break;
                }
                if (patch.size() < 4) continue;
                const double med = median_of(patch);
                std::vector<double> dev;
                for (double v : patch) dev.push_back(std::fabs(v - med));
                meds.push_back(med);
                sigs.push_back(1.482602218505602 * median_of(std::move(dev)));
                ns.push_back((int)patch.size());
            }
            if (meds.size() < 200) {
                std::printf("[FAIL] 组合 pf=%.1f sc=%.0f 有效 MC 样本 %zu < 200"
                            "（测量退化）\n", pf, sc, meds.size());
                return 1;
            }
            double mean = 0.0;
            for (double m : meds) mean += m;
            mean /= (double)meds.size();
            double var_emp = 0.0;
            for (double m : meds) var_emp += (m - mean) * (m - mean);
            var_emp /= (double)(meds.size() - 1);
            std::sort(sigs.begin(), sigs.end());
            std::sort(ns.begin(), ns.end());
            const double sig_med = sigs[sigs.size() / 2];
            const double n_med = (double)ns[ns.size() / 2];
            const double baseline = 0.5 * pi * sig_med * sig_med / n_med;
            if (!(sig_med > 0.0) || !(n_med > 0.0) || !(baseline > 0.0) ||
                !(var_emp > 0.0)) {
                std::printf("[FAIL] 组合 pf=%.1f sc=%.0f 基线量退化"
                            "（sig_med=%.3g n_med=%.3g var_emp=%.3g）\n",
                            pf, sc, sig_med, n_med, var_emp);
                return 1;
            }
            const double k = var_emp / baseline;
            cells.push_back({pf, sc, k, n_med,
                             0.5 * pi * sig_med * sig_med / var_emp});
            std::printf("k_corr: pixfrac=%.1f scale=%.0f\" N=%.0f "
                        "k=%.4f N_eff=%.1f\n",
                        pf, sc, n_med, k, cells.back().n_eff);
        }
    }

    // 适用域判定（有牙）：judge 判红 = 测量退化 = 测试失败。
    double max_dev = 0.0;
    const int jd = judge(cells, &max_dev);
    if (jd != 0) {
        std::printf("[FAIL] judge 判红：cells=%zu（预期 6），测量链路退化\n",
                    cells.size());
        return 1;
    }
    const MatrixCell* base_cell = nullptr;
    for (const auto& c : cells)
        if (c.pixfrac == 0.8 && c.scale_arcsec == 300.0) base_cell = &c;
    const char* verdict = (max_dev <= 0.10) ? "A" : "B";
    std::printf("k_corr 基线(0.8/300\")=%.4f 最大相对偏差=%.1f%%\n",
                base_cell->k_corr, 100.0 * max_dev);
    if (verdict[0] == 'A')
        std::printf("[PASS] 差异<=10%% → 选项A：共同因子在 per-control "
                    "normalization 消去；Phase2 group 必须同 Drizzle 参数\n");
    else
        std::printf("[INFO] 差异>10%% → 选项B：k_corr 作 per-frame 量\n");

    // 证据 JSON 写出（证据链补全；写失败 = 红）。
    std::error_code ec;
    std::filesystem::create_directories(
        "实验/engineering-evidence/science", ec);
    std::ofstream of(EVIDENCE_JSON);
    if (!of.is_open()) {
        std::printf("[FAIL] 证据 JSON 无法打开：%s\n", EVIDENCE_JSON);
        return 1;
    }
    of << "{\n"
       << "  \"task\": \"kcorr-matrix\",\n"
       << "  \"nmc\": " << NMC << ",\n"
       << "  \"nside\": " << NSIDE << ",\n"
       << "  \"sky_e\": " << SKY << ",\n"
       << "  \"sigma_e\": " << SIGMA << ",\n"
       << "  \"cells\": [\n";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto& c = cells[i];
        of << "    {\"pixfrac\": " << c.pixfrac
           << ", \"scale_arcsec\": " << c.scale_arcsec
           << ", \"k_corr\": " << c.k_corr
           << ", \"n_retained\": " << c.n_retained
           << ", \"n_eff\": " << c.n_eff << "}"
           << (i + 1 < cells.size() ? "," : "") << "\n";
    }
    of << "  ],\n"
       << "  \"baseline\": {\"pixfrac\": 0.8, \"scale_arcsec\": 300.0"
       << ", \"k_corr\": " << base_cell->k_corr << "},\n"
       << "  \"max_rel_dev\": " << max_dev << ",\n"
       << "  \"verdict\": \"" << verdict << "\",\n"
       << "  \"non_degeneracy\": {\"judge_selftest\": \"6 negative red + 1 positive green\", \"cells_required\": 6, \"k_positive_finite_required\": true}\n"
       << "}\n";
    of.close();
    if (!of.good()) {
        std::printf("[FAIL] 证据 JSON 写入失败：%s\n", EVIDENCE_JSON);
        return 1;
    }
    std::printf("[EVIDENCE] %s\n", EVIDENCE_JSON);
    return 0;
}
