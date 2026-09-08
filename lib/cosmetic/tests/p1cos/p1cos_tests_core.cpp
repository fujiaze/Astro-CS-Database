// P1-COS-TEST · 单元/性质/负面/cosmetic 测试组 (单执行器 core)
//
// 合同锚: docs/algorithms/COSMETIC_ALGORITHMS.md §9 TEST-COS-DESIGN-001
// (P1-COS-DOC 冻结, 2026-09-07, wave W1) + §1-§4 ALG-COS-001..005 +
// §10 DISP-COS-002/003/004/011 现状行为断言; SCI-CAL-001 §9a/§11。
//
// 组结构 (模块 tests/{unit,properties,oracle,negative} 语义落位; performance
// 与故障注入自检为独立可执行, 见 CMakeLists.txt):
//   units      : FIX-COS-A 恒等 + FIX-COS-B 检测/修复 oracle + FIX-COS-F 双中位
//   properties : I1/I2/I3/I5/I6 不变量 + I4 确定性 (1/2/4 线程)
//   negative   : C ABI 参数域 / method=2 现状 / max_size<=0 / NaN/Inf / f64 降级
//   cosmetic   : FIX-COS-C 结构过滤 + FIX-COS-E IDW 方向性 + 小帧镜像 + 1-N worker
//
// 被测面: 现状唯一生产实现 ac_correct_frame(+_f64) C ABI
// (lib/calibration/src/cosmetic_corrector.cpp + ac_api.cpp, CMake
// astrocs_calibration); 期望值一律由 p1cos_oracle.hpp 独立 oracle 推导。
#include "astro_calibration.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "p1cos_fixtures.hpp"
#include "p1cos_oracle.hpp"
#include "p1cos_test_main.hpp"

using namespace p1cos;

namespace {

// ---- 共享小工具 -----------------------------------------------------------

struct RunResult {
    int rc = -999;
    int n_hot = -1, n_cold = -1;
    std::vector<float> out;
};

inline RunResult run_correct(const std::vector<float>& data, int w, int h,
                             const std::vector<float>* dark, const std::vector<float>* bias,
                             float hot_sigma, float cold_sigma, int method, int max_size,
                             int* override_hot = nullptr, int* override_cold = nullptr) {
    RunResult r;
    r.out.assign(static_cast<std::size_t>(w) * h, -12345.0f);
    int nh = 0, nc = 0;
    r.rc = ac_correct_frame(data.data(), w, h,
                            dark ? dark->data() : nullptr,
                            bias ? bias->data() : nullptr,
                            r.out.data(),
                            hot_sigma, cold_sigma, method, max_size,
                            override_hot ? override_hot : &nh,
                            override_cold ? override_cold : &nc);
    r.n_hot = override_hot ? *override_hot : nh;
    r.n_cold = override_cold ? *override_cold : nc;
    return r;
}

inline bool frame_bit_equal(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!bit_equal(a[i], b[i])) return false;
    return true;
}

// ---- units ----------------------------------------------------------------

// FIX-COS-A 常量场: mad=0 → 阈值=±med → 严格不等式无坏点; bitwise 恒等。
int check_fix_cos_a(CheckState& cs) {
    const struct { float C; int w, h; } cases[] = {
        {0.0f, 16, 12}, {100.0f, 33, 17}, {1000.0f, 16, 16},
    };
    for (const auto& c : cases) {
        std::vector<float> data, dark, bias;
        fix_cos_a_const_field(c.C, static_cast<std::size_t>(c.w), static_cast<std::size_t>(c.h),
                              &data, &dark, &bias);
        const RunResult r = run_correct(data, c.w, c.h, &dark, &bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK(cs, frame_bit_equal(r.out, data), "const_field_bitwise");
        P1COS_CHECK_EQ(cs, r.n_hot, 0);
        P1COS_CHECK_EQ(cs, r.n_cold, 0);
    }
    // ALG §1 边界: 1×1 单元素帧 (dark[0]==thr → 严格大于不触发)
    {
        std::vector<float> data{10.0f}, dark{5.0f}, bias{3.0f};
        const RunResult r = run_correct(data, 1, 1, &dark, &bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK(cs, frame_bit_equal(r.out, data), "const_field_bitwise");
        P1COS_CHECK_EQ(cs, r.n_hot, 0);
        P1COS_CHECK_EQ(cs, r.n_cold, 0);
    }
    return cs.failures;
}

// FIX-COS-B 解析注入坏点: 计数/修复/恒等 + 全帧 oracle 对照 (双变体)。
int check_fix_cos_b(CheckState& cs) {
    // 变体 1: 常量基场 → 修复值解析 10.0 bitwise + oracle 全帧一致
    {
        const FixCosB fx = fix_cos_b_spike_field(20260907ull, 32, 32, false);
        const RunResult r = run_correct(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, static_cast<int>(fx.spikes.size()));
        P1COS_CHECK_EQ(cs, r.n_cold, 0);
        for (const std::size_t idx : fx.spikes)
            P1COS_CHECK(cs, bit_equal(r.out[idx], 10.0f), "spike_repair_bitwise");
        // I1: 非坏点逐像素恒等 (bitwise)
        bool ident = true;
        std::vector<char> is_bad(fx.data.size(), 0);
        for (const std::size_t idx : fx.spikes) is_bad[idx] = 1;
        for (std::size_t i = 0; i < fx.data.size(); ++i)
            if (!is_bad[i] && !bit_equal(r.out[i], fx.data[i])) { ident = false; break; }
        P1COS_CHECK(cs, ident, "spike_identity_nonbad");
        // oracle 全帧对照 (FIX-COS-B 冻结容差 rtol=1e-6 / atol=1e-7)
        const CorrectOracle want = correct_oracle(fx.data, 32, 32, &fx.dark, &fx.bias,
                                                  5.0, 5.0, 0, 5);
        P1COS_CHECK_EQ(cs, want.n_hot, r.n_hot);
        bool ok = true;
        for (std::size_t i = 0; i < fx.data.size(); ++i)
            if (!close_enough(r.out[i], want.out[i])) { ok = false; break; }
        P1COS_CHECK(cs, ok, "spike_oracle_rtol");
    }
    // 变体 2: 梯度基场 → oracle double 域复算 rtol 对照 (修复值非常量路径)
    {
        const FixCosB fx = fix_cos_b_spike_field(20260907ull, 32, 32, true);
        const RunResult r = run_correct(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, static_cast<int>(fx.spikes.size()));
        const CorrectOracle want = correct_oracle(fx.data, 32, 32, &fx.dark, &fx.bias,
                                                  5.0, 5.0, 0, 5);
        bool ok = true;
        for (std::size_t i = 0; i < fx.data.size(); ++i)
            if (!close_enough(r.out[i], want.out[i])) { ok = false; break; }
        P1COS_CHECK(cs, ok, "spike_oracle_rtol");
    }
    return cs.failures;
}

// FIX-COS-F 双中位均值: even 24 好 → (hi+lo)*0.5; odd 23 好 → v[mid]。
int check_fix_cos_f(CheckState& cs) {
    {   // even: 窗内 12×10 + 12×20 → (20+10)*0.5 = 15.0 (bitwise)
        const FixCosF fx = fix_cos_f_twin_median(false);
        const RunResult r = run_correct(fx.data, 16, 16, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, 1);
        P1COS_CHECK(cs, bit_equal(r.out[fx.slot], 15.0f), "twin_median_even_bitwise");
    }
    {   // odd: 窗内 12×10 + 11×20 → 中值 = v[11] = 10.0 (bitwise)
        const FixCosF fx = fix_cos_f_twin_median(true);
        const RunResult r = run_correct(fx.data, 16, 16, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, 2);
        P1COS_CHECK(cs, bit_equal(r.out[fx.slot], 10.0f), "twin_median_odd_bitwise");
    }
    return cs.failures;
}

int test_units() {
    CheckState cs;
    check_fix_cos_a(cs);
    check_fix_cos_b(cs);
    check_fix_cos_f(cs);
    return cs.failures == 0 ? 0 : 1;
}

// ---- properties ------------------------------------------------------------

// I2 无检测条件恒等 (ALG §9 I2: dark/bias NULL 或 sigma<=0 → out==data bitwise)。
int check_i2_no_detect(CheckState& cs) {
    const FixCosB fx = fix_cos_b_spike_field(20260907ull, 32, 32, false);
    const int w = 32, h = 32;
    {   // dark/bias 全 NULL
        const RunResult r = run_correct(fx.data, w, h, nullptr, nullptr, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK(cs, frame_bit_equal(r.out, fx.data), "no_detect_identity");
        P1COS_CHECK_EQ(cs, r.n_hot, 0);
        P1COS_CHECK_EQ(cs, r.n_cold, 0);
    }
    {   // hot_sigma=0 → 热检测禁用 (correct_frame 参数门)
        const RunResult r = run_correct(fx.data, w, h, &fx.dark, &fx.bias, 0.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK(cs, frame_bit_equal(r.out, fx.data), "no_detect_identity");
        P1COS_CHECK_EQ(cs, r.n_hot, 0);
    }
    {   // cold_sigma<0 → 冷检测禁用
        const RunResult r = run_correct(fx.data, w, h, &fx.dark, &fx.bias, 5.0f, -1.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_cold, 0);
        P1COS_CHECK_EQ(cs, r.n_hot, static_cast<int>(fx.spikes.size()));
    }
    {   // sigma>0 但 dark=NULL → 热通道空转
        const RunResult r = run_correct(fx.data, w, h, nullptr, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK(cs, frame_bit_equal(r.out, fx.data), "no_detect_identity");
    }
    return cs.failures;
}

// I3 幂等: 修复帧再修 → 掩码不变 (检测只依赖 dark/bias)、修复值收敛。
int check_i3_idempotent(CheckState& cs) {
    const FixCosB fx = fix_cos_b_spike_field(20260907ull, 32, 32, false);
    const RunResult r1 = run_correct(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
    P1COS_CHECK_EQ(cs, r1.rc, AC_OK);
    const RunResult r2 = run_correct(r1.out, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
    P1COS_CHECK_EQ(cs, r2.rc, AC_OK);
    P1COS_CHECK(cs, frame_bit_equal(r2.out, r1.out), "idempotent_bitwise");
    P1COS_CHECK_EQ(cs, r2.n_hot, r1.n_hot);
    return cs.failures;
}

// I4 确定性: 1/2/4 线程 bitwise 一致 (判定/插值逐像素独立, 无归约顺序差异)。
int check_i4_determinism(CheckState& cs) {
    const FixCosB fx = fix_cos_b_spike_field(20260907ull, 64, 64, true);
    std::vector<float> ref;
    for (const int threads : {1, 2, 4}) {
        ac_set_num_threads(threads);
        const RunResult r = run_correct(fx.data, 64, 64, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        if (threads == 1) {
            ref = r.out;
        } else {
            P1COS_CHECK(cs, frame_bit_equal(r.out, ref), "determinism_bitwise");
        }
    }
    ac_set_num_threads(1);
    return cs.failures;
}

// I5 计数关系: out_hot/out_cold = 结构过滤后坏点数 ≤ 阈值掩码和。
int check_i5_counts(CheckState& cs) {
    const FixCosC fx = fix_cos_c_structure();
    // 阈值掩码 (未过滤) oracle: 8 坏点
    const std::vector<char> raw = detect_hot_oracle(fx.dark, 5.0);
    int raw_sum = 0;
    for (const char v : raw) raw_sum += v;
    P1COS_CHECK_EQ(cs, raw_sum, 8);
    // 过滤后 (max_size=4): L 形 5px 清除 → 3
    const RunResult r = run_correct(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 4);
    P1COS_CHECK_EQ(cs, r.rc, AC_OK);
    P1COS_CHECK_EQ(cs, r.n_hot, 3);
    P1COS_CHECK(cs, r.n_hot <= raw_sum, "count_mask_relation");
    P1COS_CHECK_EQ(cs, r.n_cold, 0);
    return cs.failures;
}

// I6 空邻域回退: 全图坏点 (hot∪cold 覆盖全场) → 5×5 窗/4 方向无好像素 →
// out==data bitwise (median 与 IDW 双路径; ALG §9 I6)。
int check_i6_empty_neighborhood(CheckState& cs) {
    const int w = 16, h = 16;
    const int n = w * h;
    std::vector<float> data(static_cast<std::size_t>(n));
    std::vector<float> dark(static_cast<std::size_t>(n));
    std::vector<float> bias(static_cast<std::size_t>(n));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::size_t i = static_cast<std::size_t>(y * w + x);
            data[i] = 3.25f * static_cast<float>(x) + static_cast<float>(y);
            dark[i] = (y < h / 2) ? 100.0f : 0.0f;   // 上半 → hot (0.1σ)
            bias[i] = (y < h / 2) ? 100.0f : 0.0f;   // 下半 → cold (0.1σ)
        }
    }
    const std::size_t nan_slot = 8 + 8 * static_cast<std::size_t>(w);
    const std::size_t inf_slot = 7 + 8 * static_cast<std::size_t>(w);
    data[nan_slot] = std::numeric_limits<float>::quiet_NaN();
    data[inf_slot] = std::numeric_limits<float>::infinity();
    // max_size = n+1 → 结构过滤不清除 (域 size n < n+1)
    const RunResult rm = run_correct(data, w, h, &dark, &bias, 0.1f, 0.1f, 0, n + 1);
    P1COS_CHECK_EQ(cs, rm.rc, AC_OK);
    P1COS_CHECK_EQ(cs, rm.n_hot, n / 2);
    P1COS_CHECK_EQ(cs, rm.n_cold, n / 2);
    P1COS_CHECK(cs, frame_bit_equal(rm.out, data), "empty_neighborhood_identity");
    const RunResult ri = run_correct(data, w, h, &dark, &bias, 0.1f, 0.1f, 1, n + 1);
    P1COS_CHECK_EQ(cs, ri.rc, AC_OK);
    P1COS_CHECK(cs, frame_bit_equal(ri.out, data), "empty_neighborhood_identity");
    return cs.failures;
}

int test_properties() {
    CheckState cs;
    check_i2_no_detect(cs);
    check_i3_idempotent(cs);
    check_i4_determinism(cs);
    check_i5_counts(cs);
    check_i6_empty_neighborhood(cs);
    return cs.failures == 0 ? 0 : 1;
}

// ---- negative --------------------------------------------------------------

// C ABI 参数域: NULL data/out、w<=0、h<=0 → AC_ERR_PARAM 且 out 不被写。
int check_negative_abi(CheckState& cs) {
    const FixCosB fx = fix_cos_b_spike_field(20260907ull, 8, 8, false);
    std::vector<float> out(64, -12345.0f);
    const float sentinel = -12345.0f;
    int nh = 0, nc = 0;

    const int rc_null_data = ac_correct_frame(nullptr, 8, 8, fx.dark.data(), fx.bias.data(),
                                              out.data(), 5.0f, 5.0f, 0, 5, &nh, &nc);
    P1COS_CHECK_EQ(cs, rc_null_data, AC_ERR_PARAM);
    P1COS_CHECK(cs, frame_bit_equal(out, std::vector<float>(64, sentinel)),
                "out_untouched_on_param_error");

    const int rc_null_out = ac_correct_frame(fx.data.data(), 8, 8, fx.dark.data(), fx.bias.data(),
                                             nullptr, 5.0f, 5.0f, 0, 5, &nh, &nc);
    P1COS_CHECK_EQ(cs, rc_null_out, AC_ERR_PARAM);

    const int rc_w0 = ac_correct_frame(fx.data.data(), 0, 8, fx.dark.data(), fx.bias.data(),
                                       out.data(), 5.0f, 5.0f, 0, 5, &nh, &nc);
    P1COS_CHECK_EQ(cs, rc_w0, AC_ERR_PARAM);
    const int rc_h0 = ac_correct_frame(fx.data.data(), 8, 0, fx.dark.data(), fx.bias.data(),
                                       out.data(), 5.0f, 5.0f, 0, 5, &nh, &nc);
    P1COS_CHECK_EQ(cs, rc_h0, AC_ERR_PARAM);
    const int rc_wneg = ac_correct_frame(fx.data.data(), -5, 8, fx.dark.data(), fx.bias.data(),
                                         out.data(), 5.0f, 5.0f, 0, 5, &nh, &nc);
    P1COS_CHECK_EQ(cs, rc_wneg, AC_ERR_PARAM);
    P1COS_CHECK(cs, frame_bit_equal(out, std::vector<float>(64, sentinel)),
                "out_untouched_on_param_error");

    // out_hot/out_cold = NULL → 不崩溃, 正常执行 (可选输出, 输出与给定对照一致)
    {
        std::vector<float> out_null(64, -12345.0f);
        const int rc_opt = ac_correct_frame(fx.data.data(), 8, 8, fx.dark.data(), fx.bias.data(),
                                            out_null.data(), 5.0f, 5.0f, 0, 5, nullptr, nullptr);
        P1COS_CHECK_EQ(cs, rc_opt, AC_OK);
        const RunResult r_ref = run_correct(fx.data, 8, 8, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK(cs, frame_bit_equal(out_null, r_ref.out), "optional_counts_ok");
    }
    return cs.failures;
}

// method 非 0 → 现状一律走 IDW 分支 (DISP-COS-003 现状断言)。
int check_negative_method(CheckState& cs) {
    const FixCosE fx = fix_cos_e_idw(false);
    const RunResult r1 = run_correct(fx.data, 24, 24, &fx.dark, &fx.bias, 5.0f, 5.0f, 1, 5);
    const RunResult r2 = run_correct(fx.data, 24, 24, &fx.dark, &fx.bias, 5.0f, 5.0f, 2, 5);
    P1COS_CHECK_EQ(cs, r1.rc, AC_OK);
    P1COS_CHECK_EQ(cs, r2.rc, AC_OK);
    P1COS_CHECK(cs, frame_bit_equal(r2.out, r1.out), "method2_is_idw");
    return cs.failures;
}

// max_size<=0 → 全部连通域 size>=max_size 清零 (全域清除语义现状断言)。
int check_negative_maxsize(CheckState& cs) {
    const FixCosC fx = fix_cos_c_structure();
    for (const int ms : {0, -4}) {
        const RunResult r = run_correct(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, ms);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, 0);
        P1COS_CHECK(cs, frame_bit_equal(r.out, fx.data), "maxsize_zero_purge");
    }
    return cs.failures;
}

// FIX-COS-D NaN/Inf (DISP-COS-002 现状): NaN dark → 统计非数值 → 判定全
// false; NaN/Inf data 非坏点位直接透传。
int check_negative_nan(CheckState& cs) {
    {   // NaN dark: 检测全禁 → out==data bitwise (NaN/Inf 槽位透传)
        const FixCosD fx = fix_cos_d_nan_inf(true);
        const RunResult r = run_correct(fx.data, 16, 16, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, 0);
        P1COS_CHECK_EQ(cs, r.n_cold, 0);
        P1COS_CHECK(cs, frame_bit_equal(r.out, fx.data), "nan_dark_no_detection");
    }
    {   // 对照: 正常 spike → 检测生效; NaN/Inf data 槽位仍透传
        const FixCosD fx = fix_cos_d_nan_inf(false);
        const RunResult r = run_correct(fx.data, 16, 16, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, 1);
        P1COS_CHECK(cs, std::isnan(r.out[fx.nan_slot]), "nan_data_passthrough");
        P1COS_CHECK(cs, bit_equal(r.out[fx.nan_slot], fx.data[fx.nan_slot]), "nan_data_passthrough");
        P1COS_CHECK(cs, bit_equal(r.out[fx.inf_slot], fx.data[fx.inf_slot]), "nan_data_passthrough");
        P1COS_CHECK(cs, bit_equal(r.out[4 + 4 * 16], 10.0f), "nan_data_passthrough");
    }
    return cs.failures;
}

// ac_correct_frame_f64: double 输入经 float32 降级执行 (DISP-COS-004 现状) →
// f64 输出位型 == f32 ABI 输出转 double。
int check_negative_f64(CheckState& cs) {
    const FixCosB fx = fix_cos_b_spike_field(20260907ull, 32, 32, false);
    const std::size_t npix = fx.data.size();
    std::vector<double> data64(npix), dark64(npix), bias64(npix), out64(npix, 0.0);
    for (std::size_t i = 0; i < npix; ++i) {
        data64[i] = static_cast<double>(fx.data[i]);
        dark64[i] = static_cast<double>(fx.dark[i]);
        bias64[i] = static_cast<double>(fx.bias[i]);
    }
    int nh64 = 0, nc64 = 0;
    const int rc = ac_correct_frame_f64(data64.data(), 32, 32, dark64.data(), bias64.data(),
                                        out64.data(), 5.0, 5.0, 0, 5, &nh64, &nc64);
    P1COS_CHECK_EQ(cs, rc, AC_OK);
    const RunResult r32 = run_correct(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
    P1COS_CHECK_EQ(cs, nh64, r32.n_hot);
    P1COS_CHECK_EQ(cs, nc64, r32.n_cold);
    bool ok = true;
    for (std::size_t i = 0; i < npix; ++i) {
        if (!bit_equal(static_cast<float>(out64[i]), r32.out[i])) { ok = false; break; }
    }
    P1COS_CHECK(cs, ok, "f64_parity_f32");
    // 非坏点: f64 输出 == (double)(float) 输入 (降级位型)
    bool ident = true;
    std::vector<char> is_bad(npix, 0);
    for (const std::size_t idx : fx.spikes) is_bad[idx] = 1;
    for (std::size_t i = 0; i < npix; ++i)
        if (!is_bad[i] && !bit_equal(static_cast<float>(out64[i]), fx.data[i])) { ident = false; break; }
    P1COS_CHECK(cs, ident, "f64_parity_f32");
    return cs.failures;
}

int test_negative() {
    CheckState cs;
    check_negative_abi(cs);
    check_negative_method(cs);
    check_negative_maxsize(cs);
    check_negative_nan(cs);
    check_negative_f64(cs);
    return cs.failures == 0 ? 0 : 1;
}

// ---- cosmetic --------------------------------------------------------------

// FIX-COS-C 连通域结构: 8 连通 oracle (deque BFS) 复算过滤语义 — L 形 5px
// 被清除保留原值, 2px 对与孤立单点被修复。
int check_fix_cos_c(CheckState& cs) {
    const FixCosC fx = fix_cos_c_structure();
    const RunResult r = run_correct(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 4);
    P1COS_CHECK_EQ(cs, r.rc, AC_OK);
    P1COS_CHECK_EQ(cs, r.n_hot, 3);
    // oracle: detect → filter → 期望掩码
    std::vector<char> want_mask = detect_hot_oracle(fx.dark, 5.0);
    P1COS_CHECK_EQ(cs, static_cast<int>(std::count(want_mask.begin(), want_mask.end(), 1)), 8);
    filter_structure_oracle(&want_mask, 32, 32, 4);
    P1COS_CHECK_EQ(cs, static_cast<int>(std::count(want_mask.begin(), want_mask.end(), 1)), 3);
    for (const std::size_t idx : fx.l_shape)
        P1COS_CHECK(cs, bit_equal(r.out[idx], 999.0f), "structure_filter_oracle");
    for (const std::size_t idx : fx.pair)
        P1COS_CHECK(cs, bit_equal(r.out[idx], 10.0f), "structure_repair_bitwise");
    for (const std::size_t idx : fx.single)
        P1COS_CHECK(cs, bit_equal(r.out[idx], 10.0f), "structure_repair_bitwise");
    // oracle 全帧对照
    const CorrectOracle want = correct_oracle(fx.data, 32, 32, &fx.dark, &fx.bias, 5.0, 5.0, 0, 4);
    P1COS_CHECK_EQ(cs, want.n_hot, r.n_hot);
    bool ok = true;
    for (std::size_t i = 0; i < fx.data.size(); ++i)
        if (!close_enough(r.out[i], want.out[i])) { ok = false; break; }
    P1COS_CHECK(cs, ok, "structure_filter_oracle");
    return cs.failures;
}

// FIX-COS-E IDW 方向性: 两构型中心 = 8.0 bitwise (2 幂零舍入解析) + rtol
// 变体 oracle 对照 + 端点 oracle 对照。
int check_fix_cos_e(CheckState& cs) {
    {   // bitwise 变体
        const FixCosE fx = fix_cos_e_idw(false);
        const RunResult r = run_correct(fx.data, 24, 24, &fx.dark, &fx.bias, 5.0f, 5.0f, 1, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, 6);
        P1COS_CHECK(cs, bit_equal(r.out[fx.col_mid], 8.0f), "idw_center_bitwise");
        P1COS_CHECK(cs, bit_equal(r.out[fx.row_mid], 8.0f), "idw_center_bitwise");
    }
    {   // rtol 变体: oracle double 域同序复算 (中心 + 端点)
        const FixCosE fx = fix_cos_e_idw(true);
        const RunResult r = run_correct(fx.data, 24, 24, &fx.dark, &fx.bias, 5.0f, 5.0f, 1, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        // oracle 期望掩码: detect + filter (6 孤立/成列坏点, max_size=5 全保留)
        std::vector<char> mask = detect_hot_oracle(fx.dark, 5.0);
        filter_structure_oracle(&mask, 24, 24, 5);
        const float col_want = idw_repair_oracle(fx.data, mask, 24, 24, fx.col_mid);
        const float row_want = idw_repair_oracle(fx.data, mask, 24, 24, fx.row_mid);
        P1COS_CHECK(cs, close_enough(r.out[fx.col_mid], col_want), "idw_oracle_rtol");
        P1COS_CHECK(cs, close_enough(r.out[fx.row_mid], row_want), "idw_oracle_rtol");
        // 端点 (dist=3 路径): 上端点 (col_u) / 左端点 (row_l)
        const float colu_want = idw_repair_oracle(fx.data, mask, 24, 24, fx.col_u);
        const float rowl_want = idw_repair_oracle(fx.data, mask, 24, 24, fx.row_l);
        P1COS_CHECK(cs, close_enough(r.out[fx.col_u], colu_want), "idw_endpoint_oracle");
        P1COS_CHECK(cs, close_enough(r.out[fx.row_l], rowl_want), "idw_endpoint_oracle");
    }
    return cs.failures;
}

// DISP-COS-011 小帧镜像自映射: 2×2 帧 5×5 镜像窗重复采样 → median 修复
// 解析 10.0; IDW 出界方向被 clamp 语义吸收 → 同值。
int check_small_frame_mirror(CheckState& cs) {
    std::vector<float> data{10.0f, 10.0f, 10.0f, 999.0f};
    std::vector<float> dark{0.0f, 0.0f, 0.0f, 1000.0f};
    std::vector<float> bias(4, 3.0f);
    const RunResult rm = run_correct(data, 2, 2, &dark, &bias, 5.0f, 5.0f, 0, 5);
    P1COS_CHECK_EQ(cs, rm.rc, AC_OK);
    P1COS_CHECK_EQ(cs, rm.n_hot, 1);
    P1COS_CHECK(cs, bit_equal(rm.out[3], 10.0f), "small_frame_mirror");
    const RunResult ri = run_correct(data, 2, 2, &dark, &bias, 5.0f, 5.0f, 1, 5);
    P1COS_CHECK_EQ(cs, ri.rc, AC_OK);
    P1COS_CHECK(cs, bit_equal(ri.out[3], 10.0f), "small_frame_mirror");
    P1COS_CHECK(cs, bit_equal(ri.out[0], 10.0f), "small_frame_mirror");
    return cs.failures;
}

// 串并行 1-N worker: 完整 FIX-COS-B 场 (含角/边/中心/seed 点) 1/2/4 线程
// bitwise 一致 + oracle 对照。
int check_worker_1_to_n(CheckState& cs) {
    const FixCosB fx = fix_cos_b_spike_field(20260907ull, 48, 48, true);
    std::vector<float> ref;
    const CorrectOracle want = correct_oracle(fx.data, 48, 48, &fx.dark, &fx.bias, 5.0, 5.0, 0, 5);
    for (const int threads : {1, 2, 4}) {
        ac_set_num_threads(threads);
        const RunResult r = run_correct(fx.data, 48, 48, &fx.dark, &fx.bias, 5.0f, 5.0f, 0, 5);
        P1COS_CHECK_EQ(cs, r.rc, AC_OK);
        P1COS_CHECK_EQ(cs, r.n_hot, static_cast<int>(fx.spikes.size()));
        if (threads == 1) {
            ref = r.out;
            bool ok = true;
            for (std::size_t i = 0; i < fx.data.size(); ++i)
                if (!close_enough(r.out[i], want.out[i])) { ok = false; break; }
            P1COS_CHECK(cs, ok, "worker_bitwise_1_2_4");
        } else {
            P1COS_CHECK(cs, frame_bit_equal(r.out, ref), "worker_bitwise_1_2_4");
        }
    }
    ac_set_num_threads(1);
    return cs.failures;
}

int test_cosmetic() {
    CheckState cs;
    check_fix_cos_c(cs);
    check_fix_cos_e(cs);
    check_small_frame_mirror(cs);
    check_worker_1_to_n(cs);
    return cs.failures == 0 ? 0 : 1;
}

}  // namespace

// selfcheck 重入入口 (带注入环境子进程直接跑指定组)
int p1cos_run_core_groups(int argc, char** argv) {
    const p1cos::TestGroup groups[] = {
        {"units", test_units},
        {"properties", test_properties},
        {"negative", test_negative},
        {"cosmetic", test_cosmetic},
    };
    return p1cos::run_all_groups(groups, sizeof(groups) / sizeof(groups[0]), argc, argv);
}
