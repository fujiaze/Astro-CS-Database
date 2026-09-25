// ============================================================================
// drizzle_pf_sb_gate.cpp — 产品级门: pixfrac<1 时两个 HiPS 直写末端的面亮度归一
//
// 缺陷 (本轮修复前): write_hips_direct (hips_profile=0, 生产管线
// orchestrator -> module drz -> hp_drizzle_run_hips 走的就是这一支) 把
// sumFlux/sumArea/sumVarNum 直接交给 AIO writer, 而 writer 的落盘口径是
// sig = flux_sum/covered_area、variance = var_num_sum/covered_area²;
// write_hips_phase1 (hips_profile=1) 则先乘 k = sumArea/sumNorm。
// ⇒ 同一帧在 pixfrac<1 下, profile=0 的 signal 偏 1/pf²、variance 偏 1/pf⁴
// (默认 drizzle.pixfrac=0.8 ⇒ signal +56%、variance +144%)。
//
// 本门 (产品级, 读回落盘 FITS) 锁定:
//   (1) 两个末端的 signal 都必须是**面亮度** S_p = sumFlux/sumNorm
//       (docs/science/DRIZZLE.md §5:50), variance 都必须是
//       sumVarNum/sumNorm² (§5:83);
//   (2) 两末端在 pixfrac<1 上互相一致 (parity);
//   (3) 判据非退化: 覆盖率口径 (sumFlux/sumArea = S_p/pf²) 必被本判据拒绝.
//
// 累加器按**解析真值**构造 (等价于常量面亮度场 B_j=B0 的 n 个等权源):
//     sumArea   = D_p                (覆盖面积)
//     sumNorm   = D_p/pf²            (N_p = Σ_j w_jp·A_pixel,j, A_pixel≡1 px²)
//     sumFlux   = B0·D_p/pf²         (常量面亮度场 ⇒ S_p = sumFlux/sumNorm = B0)
//     sumVarNum = σ²·D_p²/(n·pf⁴)    (n 个等权源, 每源方差 σ², Σ_j w_jp² 展开)
// ⇒ 期望发布值: signal = B0, variance = σ²/(n·A²) = σ²/n (A=1 px²)。
// 累加器语义由 drizzle_engine.h TileLeafAccumulatorT 定义, 其**引擎侧**产生
// 过程由 variance_propagation_test 的 pixfrac 扫描段承载 (本门只测 writer 侧
// 的归一发布口径, 两者共同覆盖"引擎 -> writer -> 产品"全链)。
//
// 用法: drizzle_pf_sb_gate [work_dir]
// 环境: ASTROCS_DRZ_SB_FAULT=legacy_dp_normalization
//       → 复现修复前缺陷 (直写漏乘 k) ⇒ 本门必须判红 (负例 ctest)。
// ============================================================================
#include "astro_sphere_sink.h"
#include "drizzle_engine.h"

#include "astro_image_io.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace drizzle;

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool cond, const std::string& msg) {
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", msg.c_str());
    if (cond) ++g_pass; else ++g_fail;
}

constexpr double kPi = 3.14159265358979323846;
constexpr uint32_t kTileWidth = 512;
constexpr size_t kLeafPerTile = 512u * 512u;

// 读一个 512×512 HiPS 图 tile 的像素 (FP64 优先, 退回 FP32)。
bool read_tile(const std::string& path, std::vector<double>* out) {
    AIOImageData* im = aio_read(path.c_str());
    if (!im) return false;
    const int w = aio_get_width(im), h = aio_get_height(im);
    if (w != (int)kTileWidth || h != (int)kTileWidth) { aio_free_image_data(im); return false; }
    const size_t n = (size_t)w * (size_t)h;
    out->assign(n, 0.0);
    const double* p64 = aio_get_pixel_data_f64(im);
    if (p64) {
        std::memcpy(out->data(), p64, n * sizeof(double));
    } else {
        const float* p32 = aio_get_pixel_data(im);
        if (!p32) { aio_free_image_data(im); return false; }
        for (size_t i = 0; i < n; ++i) (*out)[i] = (double)p32[i];
    }
    aio_free_image_data(im);
    return true;
}

// 递归收集 <root>/<plane> 下的图 tile (排除 Moc/metadata/properties)。
void read_plane(const fs::path& root, const char* plane,
                std::vector<std::vector<double>>* out) {
    out->clear();
    const fs::path base = root / plane;
    std::error_code ec;
    if (!fs::exists(base, ec)) return;
    for (fs::recursive_directory_iterator it(base, ec), end; it != end; it.increment(ec)) {
        std::error_code fec;
        if (!it->is_regular_file(fec)) continue;
        const fs::path p = it->path();
        const std::string fn = p.filename().string();
        if (fn == "Moc.fits" || fn == "metadata.fits" || fn == "properties") continue;
        if (p.extension() != ".fits") continue;
        std::vector<double> v;
        if (read_tile(p.string(), &v)) out->push_back(std::move(v));
    }
}

// 一个「常量面亮度场 + n 个等权源」的解析累加器 (单 tile, parent_ipix=0)。
// q: support 的 uint8 量化档位 (uint8 = lround(255*sumArea/a_cell))。
std::vector<TileAccumulatorT<double>> make_tile(uint32_t nside, double pixfrac,
                                                double B0, double sigma2, int n_src,
                                                int q, double* D_p_out, double* a_cell_out) {
    const double a_cell = 4.0 * kPi / (12.0 * (double)nside * (double)nside);
    const double D_p = ((double)q / 255.0) * a_cell;   // 令 u8 量化恰好表示 D_p
    const double pf2 = pixfrac * pixfrac;
    const double A_pixel = 1.0;                        // 源像素面积 ≡ 1 px²

    std::vector<TileAccumulatorT<double>> tiles(1);
    tiles[0].parent_ipix = 0;
    // 同一 tile 内 4 个叶, 覆盖 D_p=touched 的校验面
    const uint32_t locals[1] = {0};
    for (uint32_t local : locals) {
        TileLeafAccumulatorT<double>& acc = tiles[0].leaf(local);
        acc.sumArea = D_p;
        acc.sumNorm = D_p / pf2;
        acc.sumFlux = B0 * D_p / pf2;
        // Σ_j w_jp² = n·(a_jp/(pf²A))² with a_jp = D_p/n
        const double w = D_p / ((double)n_src * pf2 * A_pixel);
        acc.sumVarNum = sigma2 * (double)n_src * w * w;
        acc.nContrib = (uint32_t)n_src;
    }
    if (D_p_out) *D_p_out = D_p;
    if (a_cell_out) *a_cell_out = a_cell;
    return tiles;
}

bool rel_close(double got, double want, double tol) {
    const double den = std::fabs(want);
    if (den <= 0.0) return std::fabs(got) <= tol;
    return std::fabs(got - want) / den <= tol;
}

struct Published {
    double sig = std::numeric_limits<double>::quiet_NaN();
    double var = std::numeric_limits<double>::quiet_NaN();
    double sup = std::numeric_limits<double>::quiet_NaN();
    int n_covered = 0;
};

// 取唯一有覆盖叶的发布值 (support>0 即覆盖; 本门只写一个叶)。
Published published_of(const fs::path& root) {
    Published p;
    std::vector<std::vector<double>> sigs, sups, vars;
    read_plane(root, "signal", &sigs);
    read_plane(root, "support", &sups);
    read_plane(root, "variance", &vars);
    if (sigs.size() != 1 || sups.size() != 1) return p;
    for (size_t i = 0; i < sigs[0].size(); ++i) {
        const double sup = sups[0][i];
        if (!(sup > 0.0) || !std::isfinite(sup)) continue;
        ++p.n_covered;
        p.sup = sup;
        p.sig = sigs[0][i];
        if (!vars.empty() && i < vars[0].size()) p.var = vars[0][i];
    }
    return p;
}

} // namespace

int main(int argc, char** argv) {
    const fs::path work = (argc > 1) ? fs::path(argv[1])
                                     : (fs::temp_directory_path() / "drizzle_pf_sb_gate");
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);

    const char* fault = std::getenv("ASTROCS_DRZ_SB_FAULT");
    const bool injection = (fault && std::string(fault) == "legacy_dp_normalization");

    const uint32_t nside = 512;
    const double B0 = 7.5;         // 常量面亮度 [ADU/px²]
    const double sigma2 = 4.0;     // 每源像素方差 [ADU²]
    const int n_src = 4;
    const double var_expected = sigma2 / (double)n_src;   // σ²/(n·A²), A=1 px²
    const double pfs[] = {1.0, 0.8, 0.5, 0.25};

    std::printf("=== drizzle_pf_sb_gate: pixfrac<1 面亮度归一门 (B0=%.4g, "
                "sigma^2=%.4g, n_src=%d, var_expected=%.6g)%s ===\n",
                B0, sigma2, n_src, var_expected,
                injection ? " [故障注入 legacy_dp_normalization]" : "");

    for (double pf : pfs) {
        char tag[64];
        std::snprintf(tag, sizeof(tag), "pf=%.2f", pf);
        double D_p = 0.0, a_cell = 0.0;
        const int q = 64;   // support = 64/255 ≈ 0.251
        auto tiles = make_tile(nside, pf, B0, sigma2, n_src, q, &D_p, &a_cell);

        DrizzleConfig cfg;
        cfg.nside = (int)nside;
        cfg.nested = true;
        cfg.pixfrac = pf;
        cfg.tile_depth = 9;
        cfg.threads = 1;

        const fs::path dir_direct = work / (std::string("direct_") + tag);
        const fs::path dir_phase1 = work / (std::string("phase1_") + tag);

        DrizzleMeta meta;
        std::string err;
        const bool ok_direct =
            write_hips_direct<double>(tiles, cfg, meta, dir_direct.string(), {}, 1, err);
        check(ok_direct, std::string(tag) + " write_hips_direct (profile=0) ok: " + err);
        err.clear();
        const bool ok_phase1 =
            write_hips_phase1<double>(tiles, cfg, dir_phase1.string(), "", 1, err);
        check(ok_phase1, std::string(tag) + " write_hips_phase1 (profile=1) ok: " + err);
        if (!ok_direct || !ok_phase1) continue;

        const Published pd = published_of(dir_direct);
        const Published pp = published_of(dir_phase1);
        char msg[256];

        // (1) signal = 面亮度 S_p = sumFlux/sumNorm = B0
        std::snprintf(msg, sizeof(msg),
                      "%s profile=0 signal == B0=%.6g: got %.9g (rel=%.2e, tol 1e-6, n_cov=%d)",
                      tag, B0, pd.sig, std::fabs(pd.sig / B0 - 1.0), pd.n_covered);
        check(pd.n_covered > 0 && rel_close(pd.sig, B0, 1e-6), msg);
        std::snprintf(msg, sizeof(msg),
                      "%s profile=1 signal == B0=%.6g: got %.9g (rel=%.2e, tol 1e-5)",
                      tag, B0, pp.sig, std::fabs(pp.sig / B0 - 1.0));
        check(pp.n_covered > 0 && rel_close(pp.sig, B0, 1e-5), msg);

        // (2) variance = sumVarNum/sumNorm² = σ²/n
        std::snprintf(msg, sizeof(msg),
                      "%s profile=0 variance == sigma^2/n=%.6g: got %.9g (rel=%.2e, tol 1e-6)",
                      tag, var_expected, pd.var, std::fabs(pd.var / var_expected - 1.0));
        check(rel_close(pd.var, var_expected, 1e-6), msg);
        std::snprintf(msg, sizeof(msg),
                      "%s profile=1 variance == sigma^2/n=%.6g: got %.9g (rel=%.2e, tol 1e-5)",
                      tag, var_expected, pp.var, std::fabs(pp.var / var_expected - 1.0));
        check(rel_close(pp.var, var_expected, 1e-5), msg);

        // (3) 两末端 parity (同一累加器 ⇒ 同一面亮度)
        std::snprintf(msg, sizeof(msg), "%s parity signal(profile0)/signal(profile1)=%.9g "
                                        "(rel=%.2e, tol 1e-5)",
                      tag, pd.sig / pp.sig, std::fabs(pd.sig / pp.sig - 1.0));
        check(rel_close(pd.sig, pp.sig, 1e-5), msg);

        // (4) support 与 u8 量化档位一致 (证明确实写到了目标叶)
        std::snprintf(msg, sizeof(msg), "%s support == %d/255=%.6g: got %.6g (tol 1e-5)",
                      tag, q, (double)q / 255.0, pd.sup);
        check(rel_close(pd.sup, (double)q / 255.0, 1e-5), msg);

        // (5) 判据非退化: 覆盖率口径的期望值与面亮度口径相差 1/pf² / 1/pf⁴
        if (pf < 1.0) {
            const double sig_legacy = B0 / (pf * pf);
            const double var_legacy = var_expected / (pf * pf * pf * pf);
            std::snprintf(msg, sizeof(msg),
                          "%s 非退化: 覆盖率口径期望 sig=%.6g var=%.6g 与实测相差 "
                          "sig %.2e / var %.2e (>1e-3)",
                          tag, sig_legacy, var_legacy,
                          std::fabs(pd.sig / sig_legacy - 1.0),
                          std::fabs(pd.var / var_legacy - 1.0));
            check(std::fabs(pd.sig / sig_legacy - 1.0) > 1e-3 &&
                      std::fabs(pd.var / var_legacy - 1.0) > 1e-3, msg);
        }
        std::printf("        %s D_p=%.6g a_cell=%.6g (support=D_p/a_cell=%.6f)\n",
                    tag, D_p, a_cell, D_p / a_cell);
    }

    std::printf("\n== drizzle_pf_sb_gate: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    if (g_pass == 0) {
        std::printf("[FAIL] zero executed cases\n");
        return 2;
    }
    return g_fail == 0 ? 0 : 1;
}
