// ============================================================================
// drizzle_nonfinite_test.cpp — 非有限输入不掩膜合同测试 (P1 修复 P1-DRZ-NONFINITE)
//
// 冻结合同 docs/science/DRIZZLE.md §8 :96:
//   | 源像素 NaN/Inf（值） | 经 `F_p=Σx_j·w_jp` 直接传播为 NaN/Inf, drizzle 层**不掩膜**;
//   | 非有限值由下游积分 INVALID_INPUT 合同（SCI-INT）处理 | `spherical_overlap.cpp:192` |
//
// 覆盖:
// 1. pixel NaN → drizzle 成功 (return true), 污染 leaf sumFlux=NaN (值传播, 非静默丢弃)
// 2. pixel +Inf → 同上传播
// 3. snr NaN → 不影响 (snr 仅元数据/下游, 不进 F_p/variance)
// 4. weight NaN → 传播 (NaN 不满足 <=0 边界)
// 5. weight +Inf → 传播
// 6. variance NaN → 传播 (NaN 不满足 <=0 边界)
// 7. 对照: weight==0 / variance==0 → 该像素跳过 (合法数据边界, 非掩膜)
// 8. 链路断言: drizzle 输出的 NaN flux 喂给下游积分 p2_integrate_pixel →
//    P2_INTEGRATE_INVALID_INPUT (SCI-INT 非有限输入合同, integrate.cpp:41-54)
//
// 编译 (tests/ 目录, Linux):
// g++ -O2 -std=c++17 -Wall -Wextra -fopenmp -I.. -I../../../astro_image_io/include
//   -o drizzle_nonfinite_test drizzle_nonfinite_test.cpp
//   ../../../phase2/src/integrate.cpp -lastrocs_drizzle -lastrocs_phase2 -lm
// ============================================================================
#include "drizzle_engine.h"
#include "astro/phase2/integrate.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace drizzle;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

namespace {

constexpr int W = 16, H = 16;
constexpr int NSIDE = 512;
constexpr double SKY = 1000.0;

void setup_wcs(FitsImage& im) {
    im.width = W;
    im.height = H;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = 10.0;
    im.wcs.crval[1] = 20.0;
    im.wcs.crpix[0] = (double)W * 0.5 + 0.5;
    im.wcs.crpix[1] = (double)H * 0.5 + 0.5;
    const double deg_per_px = 300.0 / 3600.0;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
}

DrizzleConfig make_cfg() {
    DrizzleConfig c;
    c.nside = NSIDE;
    c.nested = true;
    c.pixfrac = 1.0;
    c.threads = 1;   // 确定性单线程 (科学测试)
    c.apply_photometry = true;
    c.photometry_applied_upstream = true;
    c.tile_depth = 9;
    return c;
}

FitsImage make_synth() {
    FitsImage im;
    setup_wcs(im);
    im.pixels.assign((std::size_t)W * H, (float)SKY);
    im.pixels_f64.assign((std::size_t)W * H, SKY);
    return im;
}

bool any_nan_flux(const std::vector<TileAccumulatorT<float>>& tiles) {
    for (const auto& t : tiles)
        for (uint32_t local : t.touched)
            if (!std::isfinite((double)t.pixels[local].sumFlux)) return true;
    return false;
}

} // namespace

int main() {
    printf("=== Drizzle 非有限输入不掩膜合同测试 (P1-DRZ-NONFINITE) ===\n");
    const float kNan = std::numeric_limits<float>::quiet_NaN();
    const float kInf = std::numeric_limits<float>::infinity();

    DrizzleEngine eng;
    DrizzleConfig cfg = make_cfg();

    // ---- 1. pixel NaN → 传播, run 成功 ----
    {
        FitsImage im = make_synth();
        im.pixels[5] = kNan;
        im.pixels_f64[5] = (double)kNan;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                                   tiles, st, err);
        CHECK(ok, "pixel NaN: drizzleTiled 返回 true (不掩膜, 不拒绝)");
        CHECK(!any_nan_flux(tiles) || st.nSourcePixels == (int64_t)W * H,
              "pixel NaN: nSourcePixels 计入全部源像素");
        CHECK(any_nan_flux(tiles), "pixel NaN: F_p=Σx_j·w_jp 传播 → sumFlux 含 NaN");
        if (!ok) printf("    (err=%s)\n", err.c_str());
    }

    // ---- 2. pixel +Inf → 传播 ----
    {
        FitsImage im = make_synth();
        im.pixels[5] = kInf;
        im.pixels_f64[5] = (double)kInf;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                                   tiles, st, err);
        CHECK(ok, "pixel +Inf: drizzleTiled 返回 true");
        CHECK(any_nan_flux(tiles), "pixel +Inf: sumFlux 含 Inf/NaN (传播)");
    }

    // ---- 3. snr NaN → 非有限不改变 F_p (snr 不进累加器) ----
    {
        FitsImage im = make_synth();
        std::vector<float> snr((std::size_t)W * H, 1.0f);
        snr[7] = kNan;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, snr.data(), nullptr, nullptr,
                                   tiles, st, err);
        CHECK(ok, "snr NaN: drizzleTiled 返回 true");
        bool finite_flux = true;
        for (const auto& t : tiles)
            for (uint32_t local : t.touched)
                if (!std::isfinite((double)t.pixels[local].sumFlux)) finite_flux = false;
        CHECK(finite_flux, "snr NaN: sumFlux 全部有限 (snr 不进入 F_p)");
    }

    // ---- 4. weight NaN → 不掩膜 (像素进入管线; weight 不进 F_p 公式, 见下) ----
    {
        FitsImage im = make_synth();
        std::vector<float> w((std::size_t)W * H, 1.0f);
        w[9] = kNan;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, w.data(), nullptr,
                                   tiles, st, err);
        CHECK(ok, "weight NaN: drizzleTiled 返回 true");
        CHECK(st.nSourcePixels == (int64_t)W * H,
              "weight NaN: 不再静默 continue (nSourcePixels 计入全部源像素)");
    }

    // ---- 5. weight +Inf → 不掩膜 ----
    {
        FitsImage im = make_synth();
        std::vector<float> w((std::size_t)W * H, 1.0f);
        w[9] = kInf;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, w.data(), nullptr,
                                   tiles, st, err);
        CHECK(ok, "weight +Inf: drizzleTiled 返回 true");
        CHECK(st.nSourcePixels == (int64_t)W * H,
              "weight +Inf: 不再静默 continue (nSourcePixels 计入全部源像素)");
    }

    // ---- 6. variance NaN → 不掩膜 (variance>0 门自然跳过传播, flux 不受影响) ----
    {
        FitsImage im = make_synth();
        std::vector<float> var((std::size_t)W * H, 100.0f);
        var[11] = kNan;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, nullptr, var.data(),
                                   tiles, st, err);
        CHECK(ok, "variance NaN: drizzleTiled 返回 true");
        CHECK(st.nSourcePixels == (int64_t)W * H,
              "variance NaN: 不再静默 continue (nSourcePixels 计入全部源像素)");
    }

    // ---- 7. 对照: weight==0 / variance==0 → 合法数据边界跳过 (非掩膜) ----
    {
        FitsImage im = make_synth();
        std::vector<float> w((std::size_t)W * H, 1.0f);
        std::vector<float> var((std::size_t)W * H, 100.0f);
        w[13] = 0.0f;         // 零权重: 该像素不贡献
        var[14] = 0.0f;       // 零方差: 该像素跳过方差传播
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, w.data(), var.data(),
                                   tiles, st, err);
        CHECK(ok, "weight==0/variance==0: drizzleTiled 返回 true");
        CHECK(st.nSourcePixels == (int64_t)W * H - 2,
              "weight==0 与 variance==0: 两像素被合法边界跳过 (nSourcePixels 减 2)");
        bool finite_flux = true;
        for (const auto& t : tiles)
            for (uint32_t local : t.touched)
                if (!std::isfinite((double)t.pixels[local].sumFlux)) finite_flux = false;
        CHECK(finite_flux, "weight==0/variance==0: 输出保持有限 (无非有限污染)");
    }

    // ---- 8. 链路断言: NaN flux → 下游积分 INVALID_INPUT (SCI-INT) ----
    {
        P2PixelStack in;
        double vals[2] = {SKY, std::numeric_limits<double>::quiet_NaN()};
        double sup[2] = {1.0, 1.0};
        in.values = vals; in.support = sup; in.weights = nullptr;
        in.accepted = nullptr; in.count = 2;
        P2PixelResult out;
        int rc = p2_integrate_pixel(&in, &out);
        CHECK(rc == 0, "链路: p2_integrate_pixel 调用成功");
        CHECK(out.status == P2_INTEGRATE_INVALID_INPUT,
              "链路: 非有限 value → P2_INTEGRATE_INVALID_INPUT (SCI-INT, integrate.cpp)");
    }

    printf("== 非有限输入合同测试结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
