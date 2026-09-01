// tests/backend/phase2_fixture_main.cpp — 合成 HiPS fixture (CLI-005)
// 用法: phase2_fixture --make <dir>   生成 <dir>/F1.hips 与 <dir>/F2.hips(order0, 12 基元, 常量域)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "aio_hips.h"
#include "healpix_core.h"

namespace {
constexpr uint32_t TW = 512;  // nside=512 → 叶级 order 0(tile 全域)
constexpr float F1 = 1.00f, F2 = 1.25f, AREA = 1.0e-8f;
}

static bool write_frame(const std::string& path, float flux) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        path.c_str(), TW, TW, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/test", "CLI-005 synthetic", "R", 60.0, "2026-08-28T00:00:00Z", 0);
    if (!ps) { std::fprintf(stderr, "begin failed: %s\n", aio_hips_last_error()); return false; }
    std::vector<float> sig(TW * TW, flux), area(TW * TW, AREA);
    for (uint64_t ipix = 0; ipix < 12; ++ipix) {   // nside=512 NESTED 基元 index=ipix/... 用 0..11 作为 order0 tile 父单元
        AstroSphereTileView v{};
        v.parent_ipix = ipix;   // K=0: parent=nside3 基元... 由 writer 映射
        v.leaf_order = 9;   // nside=512 → leaf L=9, tile_order=0
        v.width = TW;
        v.data_type = AIO_HIPS_FLOAT32;
        v.flux_sum = sig.data();
        v.covered_area = area.data();
        v.valid_mask = nullptr;
        if (aio_hips_write_signal_support_tile(ps, &v) != 0) {
            std::fprintf(stderr, "tile write failed ipix=%llu\n", (unsigned long long)ipix);
            aio_hips_abort(ps);
            return false;
        }
    }
    if (aio_hips_finalize(ps) != 0) {
        std::fprintf(stderr, "finalize failed: %s\n", aio_hips_last_error());
        return false;
    }
    return true;
}


// ── P2-003 (G5): --make-seam 三块 mini HiPS 生成器 ──
// 背景: 常量/线性梯度/低阶平滑; 不同加性偏移; 恒星(高斯) + 扩展结构;
// mask/low support 区域(右下 128x128 support=0)。三块共享 WCS 使 overlap 存在。
static bool write_seam_frame(const std::string& path, int mode, int offset) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        path.c_str(), TW, TW, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/seam", "P2-003 seam", "R", 60.0, "2026-08-28T00:00:00Z", 0);
    if (!ps) { std::fprintf(stderr, "begin failed: %s\n", aio_hips_last_error()); return false; }
    std::vector<float> sig(TW * TW), area(TW * TW, AREA);
    for (uint64_t ipix = 0; ipix < 12; ++ipix) {
        for (uint32_t y = 0; y < TW; ++y) {
            for (uint32_t x = 0; x < TW; ++x) {
                const uint32_t i = y * TW + x;
                float bg = 0.0f;
                if (mode == 0) bg = 1.0f + 0.001f * (float)offset;                       // 常量
                else if (mode == 1) bg = 1.0f + 0.0002f * (float)x + 0.001f * (float)offset;  // 线性梯度
                else if (mode == 3) bg = 1.0f + 0.00005f * (float)x + 0.00003f * (float)y
                               + 0.0000001f * (float)x * (float)y
                               + ((x < 256u) ? +0.008f : -0.008f) + 0.001f * (float)offset;      // 平滑 + 空间偏移(seam)
                else bg = 1.0f + 0.00005f * (float)x + 0.00003f * (float)y
                               + 0.0000001f * (float)x * (float)y + 0.001f * (float)offset;     // 低阶平滑
                // 恒星(2 个高斯 σ=2 峰值 500) + 扩展结构(1 个 σ=40 峰值 200)
                const float dx1 = (float)x - 100.0f, dy1 = (float)y - 150.0f;
                const float dx2 = (float)x - 300.0f, dy2 = (float)y - 250.0f;
                const float dxe = (float)x - 400.0f, dye = (float)y - 100.0f;
                const float stars = 2.0f * std::exp(-(dx1 * dx1 + dy1 * dy1) / 8.0f)
                                  + 2.0f * std::exp(-(dx2 * dx2 + dy2 * dy2) / 8.0f);
                const float ext = 0.5f * std::exp(-(dxe * dxe + dye * dye) / 3200.0f);
                sig[i] = bg + stars + ext;
                area[i] = AREA;
                // mask/low support: 右下 128x128 区域 support=0
                if (x >= 384u && y >= 384u) area[i] = 0.0f;
            }
        }
        AstroSphereTileView v{};
        v.parent_ipix = ipix;
        v.leaf_order = 9;
        v.width = TW;
        v.data_type = AIO_HIPS_FLOAT32;
        v.flux_sum = sig.data();
        v.covered_area = area.data();
        v.valid_mask = nullptr;
        if (aio_hips_write_signal_support_tile(ps, &v) != 0) {
            std::fprintf(stderr, "seam tile write failed ipix=%llu\n", (unsigned long long)ipix);
            aio_hips_abort(ps);
            return false;
        }
    }
    if (aio_hips_finalize(ps) != 0) {
        std::fprintf(stderr, "seam finalize failed: %s\n", aio_hips_last_error());
        return false;
    }
    return true;
}

static bool write_frame_custom(const std::string& path, bool per_tile,
                               bool all_nan) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        path.c_str(), 512, 512, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/test", "P3 field", "R", 60.0, "2026-08-28T00:00:00Z", 0);
    if (!ps) return false;
    std::vector<float> sig(TW * TW), area(TW * TW, AREA);
    for (uint64_t ipix = 0; ipix < 12; ++ipix) {
        const float v = all_nan ? std::nanf("")
                                : (per_tile ? static_cast<float>(ipix + 1)   // 1..12, f32 精确
                                            : 1.0f);
        std::fill(sig.begin(), sig.end(), v);
        AstroSphereTileView view{};
        view.parent_ipix = ipix;
        view.leaf_order = 9;
        view.width = TW;
        view.data_type = AIO_HIPS_FLOAT32;
        view.flux_sum = sig.data();
        view.covered_area = area.data();
        view.valid_mask = nullptr;
        if (aio_hips_write_signal_support_tile(ps, &view) != 0) { aio_hips_abort(ps); return false; }
    }
    if (aio_hips_finalize(ps) != 0) { aio_hips_abort(ps); return false; }
    return true;
}

// P3-004: 解析球面场 cos²(dec)(独立 Oracle 用; 每像素按 tile 中心 RA/Dec 计算)
static bool write_analytic_frame(const std::string& path) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        path.c_str(), 512, 512, AIO_HIPS_FLOAT32,
        AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT,
        "ivo://astrocs/test", "P3 analytic cos2dec", "R", 60.0,
        "2026-08-28T00:00:00Z", 0);
    if (!ps) return false;
    std::vector<float> sig(TW * TW), area(TW * TW, AREA);
    for (uint64_t ipix = 0; ipix < 12; ++ipix) {
        // 逐像素 cos²(dec): tile order=0, tile 内 512² 像素 → leaf nside=512(2^9)
        const uint64_t leaf_nside = 512u;
        for (uint64_t p = 0; p < TW * TW; ++p) {
            const uint64_t leaf = (ipix << 18u) | p;   // order0 tile → leaf 位移 18
            double ra = 0, dec = 0;
            astrocs::healpix::pix2ang_nest(static_cast<uint32_t>(leaf_nside), leaf, ra, dec);
            const double v = std::cos(dec * M_PI / 180.0) * std::cos(dec * M_PI / 180.0);
            sig[p] = static_cast<float>(v);
        }
        AstroSphereTileView view{};
        view.parent_ipix = ipix;
        view.leaf_order = 9;
        view.width = TW;
        view.data_type = AIO_HIPS_FLOAT32;
        view.flux_sum = sig.data();
        view.covered_area = area.data();
        view.valid_mask = nullptr;
        if (aio_hips_write_signal_support_tile(ps, &view) != 0) { aio_hips_abort(ps); return false; }
    }
    if (aio_hips_finalize(ps) != 0) { aio_hips_abort(ps); return false; }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: --make <dir> | --make-field <dir> | --make-nan <dir> | --make-seam <dir> | --make-analytic <dir>\n");
        return 2;
    }
    const std::string mode = argv[1], dir = argv[2];
    if (mode == "--make") {
        if (!write_frame(dir + "/F1.hips", F1)) return 3;
        if (!write_frame(dir + "/F2.hips", F2)) return 3;
    } else if (mode == "--make-field") {
        if (!write_frame_custom(dir + "/FIELD.hips", true, false)) return 3;
    } else if (mode == "--make-nan") {
        if (!write_frame_custom(dir + "/NAN.hips", false, true)) return 3;
    } else if (mode == "--make-analytic") {
        if (!write_analytic_frame(dir + "/ANALYTIC.hips")) return 3;
    } else if (mode == "--make-const") {
        if (!write_frame(dir + "/CONST.hips", 2.5f)) return 3;
    } else if (mode == "--make-seam") {
        // P2-003: 三块 mini HiPS(常量/线性梯度/低阶平滑背景 + 不同偏移 + 星 + 扩展 + mask + 断连)
        // 三块共享低阶平滑背景, 偏移不同; SEAM1 含空间偏移(左+8 右-8)使 UPM 空间场有内容
        if (!write_seam_frame(dir + "/SEAM0.hips", 2, +10)) return 3;
        if (!write_seam_frame(dir + "/SEAM1.hips", 3, -5)) return 3;
        if (!write_seam_frame(dir + "/SEAM2.hips", 2, +3)) return 3;
    } else if (mode == "--make-seam6") {
        // P2-007 (G5): 六块 seam HiPS(≥10s production workload 联合门)
        // 模式轮换(平滑/空间偏移/常量/梯度), 偏移交替, 保证 UPM 可求解且 run ≥10s
        const int offs[6] = {+10, -5, +3, -8, +6, -2};
        const int modes[6] = {2, 3, 0, 1, 3, 2};
        char name[64];
        for (int i = 0; i < 6; ++i) {
            std::snprintf(name, sizeof(name), "SEAM%d.hips", i);
            if (!write_seam_frame(dir + "/" + name, modes[i], offs[i])) return 3;
        }
    } else {
        return 2;
    }
    std::printf("HIPS_FIXTURES_OK\n");
    return 0;
}
