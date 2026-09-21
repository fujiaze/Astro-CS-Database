// ============================================================================
// drizzle_nonfinite_test.cpp — 非有限样本处置合同测试
//   （FIX-405 G3-5 反转；原 P1-DRZ-NONFINITE「不掩膜/传播」断言已作废）
//
// 冻结合同（唯一口径文字 = docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md
// §2a；rule_id = NAN-SAMPLE-MASK-COVERAGE-NAN，EXP-202 定案）:
//   合格样本 = isfinite(x_j) ∧ isfinite(V_j) ∧ V_j > 0；
//   不合格样本 → 样本级掩膜（从 F_p/D_p/Var_p 三项一并剔除 ⇒ 重归一）;
//   仅 D_p = 0 时输出 signal = NaN ∧ support ≤ 0（覆盖级 NaN）;
//   **强制计数** n_rejected_nonfinite（按原因: 值非有限 / 方差非有限 / 权重非正）;
//   禁止: 单个坏样本让整像素变 NaN；禁止: 静默剔除（无计数）。
// 注意: docs/science/DRIZZLE.md §8 :96 的「不掩膜」行为已被同一 rule_id 反转，
// 本文件断言与 DATA-002 §2a 对齐（文档面订正属 DOC 域，见任务回执）。
//
// 覆盖:
// 1. pixel NaN → 掩膜: 无 leaf 被污染 + 计数=1 + nSourcePixels 减 1
// 2. pixel +Inf → 同上掩膜 + 计数
// 3. snr NaN → 不影响 (snr 仅元数据, 不进 F_p/variance; 非合格样本判据)
// 4. weight NaN → 掩膜 (权重非正/非有限) + 计数
// 5. weight +Inf → 同上
// 6. variance NaN → 掩膜 (方差非有限) + 计数
// 7. 对照: weight==0 → 掩膜并计数（权重非正）; variance==0 → 合法数据边界跳过
//    （不计入非有限计数; 零方差面=无方差贡献语义）
// 8. 重归一: 同一常值场去掉一个 NaN 样本后, 各 leaf 面亮度 F_p/D_p 与全有限场
//    逐 leaf 相等（掩膜样本不得把权重留在分母 ⇒ 无偏）—— 传播语义下此断言必红
// 9. 链路断言: 非有限样本经掩膜后不再出现在产品面; 覆盖级 NaN（D_p=0）喂给下游
//    积分 p2_integrate_pixel → P2_INTEGRATE_INVALID_INPUT (SCI-INT, integrate.cpp)
//
// 编译 (eng/tests/ 目录, Linux):
// g++ -O2 -std=c++17 -Wall -Wextra -fopenmp -I.. -I../../../astro_image_io/include
//   -o drizzle_nonfinite_test drizzle_nonfinite_test.cpp
//   ../../../coverage/src/integrate.cpp -lastrocs_drizzle -lastrocs_phase2 -lm
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
    printf("=== Drizzle 非有限样本掩膜合同测试 (DATA-002 §2a / NAN-SAMPLE-MASK-COVERAGE-NAN) ===\n");
    const float kNan = std::numeric_limits<float>::quiet_NaN();
    const float kInf = std::numeric_limits<float>::infinity();

    DrizzleEngine eng;
    DrizzleConfig cfg = make_cfg();

    // ---- 1. pixel NaN → 样本级掩膜 + 强制计数, run 成功 ----
    {
        FitsImage im = make_synth();
        im.pixels[5] = kNan;
        im.pixels_f64[5] = (double)kNan;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                                   tiles, st, err);
        CHECK(ok, "pixel NaN: drizzleTiled 返回 true (掩膜, 不拒绝整幅)");
        CHECK(!any_nan_flux(tiles),
              "pixel NaN: 样本级掩膜 → 无 leaf 被污染 (禁传播)");
        CHECK(st.n_rejected_nonfinite_value == 1,
              "pixel NaN: 值非有限计数 == 1 (强制计数, 禁静默)");
        CHECK(st.n_rejected_nonfinite == 1, "pixel NaN: 合计计数 == 1");
        CHECK(st.nSourcePixels == (int64_t)W * H - 1,
              "pixel NaN: 被掩膜样本不进管线 (nSourcePixels 减 1)");
        if (!ok) printf("    (err=%s)\n", err.c_str());
    }

    // ---- 2. pixel +Inf → 掩膜 + 计数 ----
    {
        FitsImage im = make_synth();
        im.pixels[5] = kInf;
        im.pixels_f64[5] = (double)kInf;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, nullptr, nullptr,
                                   tiles, st, err);
        CHECK(ok, "pixel +Inf: drizzleTiled 返回 true");
        CHECK(!any_nan_flux(tiles), "pixel +Inf: 掩膜 → 无 leaf 被污染");
        CHECK(st.n_rejected_nonfinite_value == 1 && st.n_rejected_nonfinite == 1,
              "pixel +Inf: 值非有限计数 == 1");
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

    // ---- 4. weight NaN → 掩膜 (权重非正/非有限) + 计数 ----
    {
        FitsImage im = make_synth();
        std::vector<float> w((std::size_t)W * H, 1.0f);
        w[9] = kNan;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, w.data(), nullptr,
                                   tiles, st, err);
        CHECK(ok, "weight NaN: drizzleTiled 返回 true");
        CHECK(!any_nan_flux(tiles), "weight NaN: 掩膜 → 无 leaf 被污染");
        CHECK(st.n_rejected_nonpositive_weight == 1,
              "weight NaN: 权重非正计数 == 1");
        CHECK(st.nSourcePixels == (int64_t)W * H - 1,
              "weight NaN: 被掩膜样本不进管线 (nSourcePixels 减 1)");
    }

    // ---- 5. weight +Inf → 掩膜 + 计数 ----
    {
        FitsImage im = make_synth();
        std::vector<float> w((std::size_t)W * H, 1.0f);
        w[9] = kInf;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, w.data(), nullptr,
                                   tiles, st, err);
        CHECK(ok, "weight +Inf: drizzleTiled 返回 true");
        CHECK(st.n_rejected_nonpositive_weight == 1 &&
              st.nSourcePixels == (int64_t)W * H - 1,
              "weight +Inf: 权重非正计数 == 1 且不进管线");
    }

    // ---- 6. variance NaN → 掩膜 (方差非有限) + 计数 ----
    {
        FitsImage im = make_synth();
        std::vector<float> var((std::size_t)W * H, 100.0f);
        var[11] = kNan;
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, nullptr, var.data(),
                                   tiles, st, err);
        CHECK(ok, "variance NaN: drizzleTiled 返回 true");
        CHECK(!any_nan_flux(tiles), "variance NaN: 掩膜 → 无 leaf 被污染");
        CHECK(st.n_rejected_nonfinite_variance == 1,
              "variance NaN: 方差非有限计数 == 1");
        CHECK(st.nSourcePixels == (int64_t)W * H - 1,
              "variance NaN: 被掩膜样本不进管线 (nSourcePixels 减 1)");
    }

    // ---- 7. 对照: weight==0 → 掩膜并计数（权重非正）;
    //          variance==0 → 合法数据边界跳过（不计非有限） ----
    {
        FitsImage im = make_synth();
        std::vector<float> w((std::size_t)W * H, 1.0f);
        std::vector<float> var((std::size_t)W * H, 100.0f);
        w[13] = 0.0f;         // 零权重: 不合格样本（权重非正）→ 掩膜+计数
        var[14] = 0.0f;       // 零方差: 合法数据边界（无方差贡献）→ 跳过不计数
        std::vector<TileAccumulatorT<float>> tiles;
        DrizzleStats st; std::string err;
        bool ok = eng.drizzleTiled(im, cfg, nullptr, w.data(), var.data(),
                                   tiles, st, err);
        CHECK(ok, "weight==0/variance==0: drizzleTiled 返回 true");
        CHECK(st.nSourcePixels == (int64_t)W * H - 2,
              "weight==0 与 variance==0: 两像素均不进管线 (nSourcePixels 减 2)");
        CHECK(st.n_rejected_nonpositive_weight == 1,
              "weight==0: 计入权重非正 (强制计数)");
        CHECK(st.n_rejected_nonfinite == 1,
              "variance==0 不计入非有限计数 (合法边界, 非非有限类)");
        bool finite_flux = true;
        for (const auto& t : tiles)
            for (uint32_t local : t.touched)
                if (!std::isfinite((double)t.pixels[local].sumFlux)) finite_flux = false;
        CHECK(finite_flux, "weight==0/variance==0: 输出保持有限 (无非有限污染)");
    }

    // ---- 8. 重归一（本任务核心语义）: 常值场去掉一个 NaN 样本后,
    //      各 leaf 的面亮度 F_p/D_p 必须与全有限场逐 leaf 相等 ----
    // 传播语义下 sumFlux 会变 NaN ⇒ 此断言必红; 掩膜但**不**从分母剔除权重
    // （半掩膜）会使 F_p/D_p 偏高 ⇒ 也会红。故本断言同时锁死「掩膜 + 重归一」。
    {
        std::vector<TileAccumulatorT<float>> t_clean, t_nan;
        DrizzleStats s_clean, s_nan;
        std::string e1, e2;
        FitsImage im_clean = make_synth();
        FitsImage im_nan = make_synth();
        im_nan.pixels[5] = kNan;
        im_nan.pixels_f64[5] = (double)kNan;
        bool ok1 = eng.drizzleTiled(im_clean, cfg, nullptr, nullptr, nullptr,
                                    t_clean, s_clean, e1);
        bool ok2 = eng.drizzleTiled(im_nan, cfg, nullptr, nullptr, nullptr,
                                    t_nan, s_nan, e2);
        CHECK(ok1 && ok2, "重归一: 两跑均成功");
        std::size_t n_cmp = 0, n_bad = 0;
        double worst_rel = 0.0;
        for (std::size_t i = 0; i < t_nan.size() && i < t_clean.size(); ++i) {
            const auto& a = t_nan[i];
            const auto& b = t_clean[i];
            for (uint32_t local : a.touched) {
                const auto& pa = a.pixels[local];
                const auto& pb = b.pixels[local];
                if (!(pa.sumArea > 0.0) || !(pb.sumArea > 0.0)) continue;
                const double sa = (double)pa.sumFlux / (double)pa.sumArea;
                const double sb = (double)pb.sumFlux / (double)pb.sumArea;
                const double rel = std::fabs(sa - sb) / std::fabs(sb);
                worst_rel = rel > worst_rel ? rel : worst_rel;
                ++n_cmp;
                // 门 = 1e-4: float 累加器 (sumFlux/sumArea 为 float) 在剔除
                // 一个样本后重算比值, 舍入噪声实测 ~1e-5; 而"半掩膜"(剔除分子
                // 却把权重留在分母) 的系统偏差 ~1/n ≈ 4e-2, 传播语义则为 NaN
                // ⇒ 1e-4 与两者都差 2 个数量级以上 (非退化判据)。
                if (!(rel < 1e-4)) ++n_bad;
            }
        }
        CHECK(n_cmp > 0, "重归一: 存在可比 leaf (非退化)");
        CHECK(n_bad == 0, "重归一: 掩膜后 F_p/D_p 逐 leaf 不变 (无偏)");
        printf("    (重归一: 比较 %zu leaf, worst_rel=%.3e)\n", n_cmp, worst_rel);
        CHECK(s_nan.n_rejected_nonfinite_value == 1,
              "重归一: 该跑恰好剔除 1 个非有限样本");
    }

    // ---- 9. 链路断言: 覆盖级 NaN（D_p=0）→ 下游积分 INVALID_INPUT (SCI-INT) ----
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
