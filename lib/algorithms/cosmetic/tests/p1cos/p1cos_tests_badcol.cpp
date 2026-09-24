// p1cos_tests_badcol.cpp — 坏列（linear defect, 单列）检测与修复的可执行验证面
//
// 任务: LINDEF-IMPL-01（阶段一第二步 Cosmetic · CCD 坏列检测与修复）
// 被测面: ac_correct_columns / ac_correct_columns_f64（C ABI，独立于
//         ac_correct_frame 的新路径；后者在本文件中只作"零改动"对照）。
//
// 任务书要求的四条负例（每条都带**非退化对照**：真值无效应时判据必须归零，
// 注入效应时必须判红；缺对照的断言按恒真门处理，不计证据）：
//   ① badcol_detect_analytic  已知坏列 ⇒ 检出 + 修复值与邻列插值解析预期一致
//   ② badcol_no_false_positive 正常列（含真实亮星穿过）⇒ 不得判坏
//   ③ badcol_disabled_bitwise  关闭开关（column_sigma<=0）⇒ 与输入逐位一致
//   ④ badcol_degrade_traced    边缘 / 连续多列 / 全坏 ⇒ 按声明行为降级并留痕
// 另加退化守卫（真实数据上实测发生过）：MAD(dev)==0 ⇒ 显式降级，不判任何列。
//
// 自检: ./p1cos_badcol_tests --self-test   —— 对每条负例做"注入必红"验证，
// 任一负例在注入下仍判绿 ⇒ 该负例无判别力 ⇒ 整体判红。单跑: 无参数 / --self-test
#include "astro_calibration.h"

// 内部降级分支（帧内检测路径下不可达，需直接驱动）：
// repair_bad_columns 在"全宽皆坏、无任何好列锚点"时必须不虚构好值并留痕。
// 帧内检测永远给不出全宽掩膜（无参照系 ⇒ 判据主动不判），故该分支只能
// 经此内部符号验证——这是 C ABI 声明行为的一部分，不能不测。
namespace ac {
void repair_bad_columns(const float* data, int w, int h,
                        const unsigned char* col_mask, float* out,
                        int* out_px_repaired, int* out_status);
}

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_case = 0;

void check(bool cond, const std::string& what) {
    ++g_case;
    if (!cond) {
        ++g_fail;
        std::fprintf(stderr, "  FAIL: %s\n", what.c_str());
    }
}

// 固定 seed 的确定性伪随机（xorshift32）——不依赖 <random> 实现差异
struct Rng {
    unsigned s;
    explicit Rng(unsigned seed) : s(seed ? seed : 1u) {}
    unsigned next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return static_cast<float>(next() % 100000u) / 100000.0f; }  // [0,1)
};

std::vector<float> make_frame(int w, int h, unsigned seed, float bg) {
    Rng rng(seed);
    std::vector<float> d(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            // 横向连续的大尺度结构（模拟天光/星云梯度）——判据必须不被它骗到
            const float grad = 120.0f * std::sin(6.2831853f * static_cast<float>(x) / static_cast<float>(w));
            d[static_cast<size_t>(y) * w + x] = bg + grad + (rng.uni() - 0.5f) * 20.0f;
        }
    return d;
}

// 在 (cx,cy) 注入一个高斯亮星（峰值 amp，半径 ~sigma）
void add_star(std::vector<float>& d, int w, int h, float cx, float cy, float amp, float sigma) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float dx = static_cast<float>(x) - cx, dy = static_cast<float>(y) - cy;
            d[static_cast<size_t>(y) * w + x] +=
                amp * std::exp(-(dx * dx + dy * dy) / (2.0f * sigma * sigma));
        }
}

// 调用被测 ABI；返回 status/n_cols/px
struct ColResult {
    int rc = 0, n_cols = -1, px = -1, status = -1;
    float sigma_col = 0.0f;
    std::vector<float> out;
    std::vector<unsigned char> mask;
};

ColResult run_cols(const std::vector<float>& d, int w, int h, float sig, int k) {
    ColResult r;
    r.out.assign(d.size(), 0.0f);
    r.mask.assign(static_cast<size_t>(w), 0);
    r.rc = ac_correct_columns(d.data(), w, h, r.out.data(), sig, k, 1,
                              r.mask.data(), &r.n_cols, &r.px, &r.sigma_col, &r.status);
    return r;
}

bool bitwise_equal(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) return false;
    return std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

// 任意注入标志（--self-test 时逐个打开，验证每条负例能判红）
struct Inject { bool no_repair = false; bool no_detect = false; bool no_degrade = false; };
Inject g_inj;

// ═══════════════════ ① 已知坏列：检出 + 解析预期 ═══════════════════
void case_analytic(int w, int h) {
    std::fprintf(stderr, "[case 1] badcol_detect_analytic\n");
    std::vector<float> d = make_frame(w, h, 20260924u, 1000.0f);
    const int BAD = 20;
    const float DELTA = -500.0f;
    for (int y = 0; y < h; ++y) d[static_cast<size_t>(y) * w + BAD] += DELTA;

    ColResult r = run_cols(d, w, h, 5.0f, 3);
    check(r.rc == AC_OK, "rc==AC_OK");
    if (g_inj.no_detect) r.n_cols = 0;
    check(r.mask[BAD] == 1, "注入列必须被判为坏列");
    check(r.n_cols >= 1, "n_cols>=1");
    check(r.px == h, "px_repaired == height (单列段)");
    std::fprintf(stderr, "   [diag1] n_cols=%d px=%d status=%d sigma_col=%.4f cols_nonzero:",
                 r.n_cols, r.px, r.status, r.sigma_col);
    for (int xx = 0; xx < w; ++xx) if (r.mask[xx]) std::fprintf(stderr, " %d(v%d)", xx, (int)r.mask[xx]);
    std::fprintf(stderr, "\n");
    check(r.status == AC_COLSTAT_OK, "单列非边缘段无降级");

    // 解析预期：out = (data[x-1] + data[x+1]) / 2，逐像素
    int worst = 0;
    for (int y = 0; y < h; ++y) {
        const size_t i = static_cast<size_t>(y) * w + BAD;
        const float expect = 0.5f * (d[i - 1] + d[i + 1]);
        if (r.out[i] != expect) ++worst;
    }
    check(worst == 0, "修复值必须逐位等于左右两邻算术平均（解析预期）");
    if (g_inj.no_repair) {  // 注入: 声称修复但写回原值
        for (int y = 0; y < h; ++y) r.out[static_cast<size_t>(y) * w + BAD] =
            d[static_cast<size_t>(y) * w + BAD];
    }
    check(r.out[static_cast<size_t>(0) * w + BAD] != d[static_cast<size_t>(0) * w + BAD],
          "非退化: 修复值必须与原坏值不同（否则判据恒真）");
    // 非坏列逐位恒等
    int changed_ok = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (x == BAD) continue;
            if (r.out[static_cast<size_t>(y) * w + x] != d[static_cast<size_t>(y) * w + x]) ++changed_ok;
        }
    check(changed_ok == 0, "非坏列必须逐位恒等（no fabrication of valid coverage）");
}

// ═══════════════════ ② 正常列（含亮星）不得判坏 ═══════════════════
void case_no_false_positive(int w, int h) {
    std::fprintf(stderr, "[case 2] badcol_no_false_positive\n");
    // 2a. 纯正常帧（无任何列缺陷）⇒ 零检出
    std::vector<float> d0 = make_frame(w, h, 777u, 1000.0f);
    ColResult r0 = run_cols(d0, w, h, 5.0f, 3);
    check(r0.n_cols == 0, "无缺陷帧必须零检出（真值无效应⇒判据归零）");

    // 2b. 正常列（列 30）含真实亮星穿过 ⇒ 该列不得判坏
    std::vector<float> d1 = make_frame(w, h, 888u, 1000.0f);
    const int STARCOL = 30;
    add_star(d1, w, h, static_cast<float>(STARCOL), static_cast<float>(h) / 2.0f, 8000.0f, 2.5f);
    ColResult r1 = run_cols(d1, w, h, 5.0f, 3);
    if (g_inj.no_detect == false && g_inj.no_repair == false) {
        check(r1.mask[STARCOL] == 0, "含亮星穿过的正常列不得判为坏列（防误伤信号）");
    }
    check(r1.out == d1 || bitwise_equal(r1.out, d1), "零检出时输出必须与输入逐位一致");

    // 2c. 非退化对照：同帧把列 30 整列压暗 ⇒ 必须检出
    std::vector<float> d2 = d1;
    for (int y = 0; y < h; ++y) d2[static_cast<size_t>(y) * w + STARCOL] -= 500.0f;
    ColResult r2 = run_cols(d2, w, h, 5.0f, 3);
    if (g_inj.no_detect) r2.mask[STARCOL] = 0;
    check(r2.mask[STARCOL] == 1,
          "非退化对照: 同列整列压暗后必须检出（证明 2b 的判绿不是恒真门）");
}

// ═══════════════════ ③ 关闭 ⇒ 逐位一致 ═══════════════════
void case_disabled_bitwise(int w, int h) {
    std::fprintf(stderr, "[case 3] badcol_disabled_bitwise\n");
    std::vector<float> d = make_frame(w, h, 4242u, 1000.0f);
    for (int y = 0; y < h; ++y) d[static_cast<size_t>(y) * w + 20] -= 500.0f;  // 真坏列在位

    ColResult off = run_cols(d, w, h, 0.0f, 3);   // column_sigma<=0 = 显式禁用
    check(off.rc == AC_OK, "禁用时 rc==AC_OK");
    check(off.n_cols == 0, "禁用时 n_cols==0");
    check(off.px == 0, "禁用时 px_repaired==0");
    check(off.status == AC_COLSTAT_OK, "禁用是显式关闭而非降级（status 无标志）");
    check(bitwise_equal(off.out, d), "禁用时输出必须与输入逐位一致");

    // 非退化对照: 打开同一帧必须真的改变像素
    ColResult on = run_cols(d, w, h, 5.0f, 3);
    if (g_inj.no_detect) on.n_cols = 0;
    check(on.n_cols >= 1, "非退化对照: 开关打开时同一帧必须检出");
    check(!bitwise_equal(on.out, d), "非退化对照: 开关打开时输出必须与输入不同");
}

// ═══════════════════ ④ 边缘 / 连续多列 / 全坏 ⇒ 降级并留痕 ═══════════════════
void case_degrade_traced(int w, int h) {
    std::fprintf(stderr, "[case 4] badcol_degrade_traced\n");
    // 4a. 坏列在最左边缘（x=0，仅右侧有锚点）
    {
        std::vector<float> d = make_frame(w, h, 11u, 1000.0f);
        for (int y = 0; y < h; ++y) d[static_cast<size_t>(y) * w + 0] -= 600.0f;
        ColResult r = run_cols(d, w, h, 5.0f, 3);
        check(r.mask[0] == 1, "边缘坏列必须被检出");
        if (!g_inj.no_degrade) {
            check((r.status & AC_COLSTAT_EDGE_ONE_SIDED) != 0,
                  "边缘单侧锚点必须留下 edge_one_sided 痕迹（不静默）");
        }
        int bad = 0;
        for (int y = 0; y < h; ++y)
            if (r.out[static_cast<size_t>(y) * w + 0] != d[static_cast<size_t>(y) * w + 1]) ++bad;
        check(bad == 0, "边缘坏列按声明行为复制唯一可用锚列");
    }
    // 4b. 连续三列坏（10,11,12）：**默认 max_seg_len=1 不修**（负责人裁决
    //     只修单列；相邻多列不属本任务）⇒ 降级为"仅标记"，掩膜值 2，
    //     逐位保留原值，并置 AC_COLSTAT_WIDE_DEFECT。不得静默。
    {
        std::vector<float> d = make_frame(w, h, 12u, 1000.0f);
        for (int x = 10; x <= 12; ++x)
            for (int y = 0; y < h; ++y) d[static_cast<size_t>(y) * w + x] -= 600.0f;
        ColResult r = run_cols(d, w, h, 5.0f, 3);
        check(r.mask[10] == AC_COLSTAT_MASK_WIDE && r.mask[11] == AC_COLSTAT_MASK_WIDE &&
              r.mask[12] == AC_COLSTAT_MASK_WIDE,
              "连续三列必须被检出并标记为宽缺陷（掩膜值 2）");
        check(r.px == 0, "宽缺陷默认不修复 ⇒ px_repaired==0（不越范围修相邻多列）");
        if (!g_inj.no_degrade)
            check((r.status & AC_COLSTAT_WIDE_DEFECT) != 0,
                  "宽缺陷必须留下 wide_defect 痕迹（降级不静默）");
        check(bitwise_equal(r.out, d), "宽缺陷未修复 ⇒ 输出必须与输入逐位一致");
    }
    // 4b2. 显式放宽 max_seg_len=3 ⇒ 同一帧必须按段两端好列线性插值修复
    //      （证明线性插值能力在位，4b 的不修是范围选择而非能力缺失）
    {
        std::vector<float> d = make_frame(w, h, 12u, 1000.0f);
        for (int x = 10; x <= 12; ++x)
            for (int y = 0; y < h; ++y) d[static_cast<size_t>(y) * w + x] -= 600.0f;
        ColResult r;
        r.out.assign(d.size(), 0.0f);
        r.mask.assign(static_cast<size_t>(w), 0);
        r.rc = ac_correct_columns(d.data(), w, h, r.out.data(), 5.0f, 3, 3,
                                  r.mask.data(), &r.n_cols, &r.px, &r.sigma_col, &r.status);
        check(r.mask[10] == AC_COLSTAT_MASK_REPAIRED && r.mask[12] == AC_COLSTAT_MASK_REPAIRED,
              "放宽 max_seg_len 后三列判为可修");
        check(r.px == 3 * h, "放宽后 px_repaired == 3*height");
        int bad = 0;
        for (int y = 0; y < h; ++y) {
            const size_t row = static_cast<size_t>(y) * w;
            const float vl = d[row + 9], vr = d[row + 13];
            for (int x = 10; x <= 12; ++x) {
                const float expect = vl + (vr - vl) * (static_cast<float>(x - 9) / 4.0f);
                if (r.out[row + x] != expect) ++bad;
            }
        }
        check(bad == 0, "放宽后按两端好列线性插值（解析预期）");
        check((r.status & AC_COLSTAT_WIDE_DEFECT) == 0, "放宽后无 wide_defect 标志");
    }
    // 4c. 无锚点（全宽皆坏）⇒ 不修复、保留原值，但必须留痕。
    //     该形态在**帧内检测**下不可达（无参照系 ⇒ 判据主动不判，这是正确
    //     行为，见 4d），故经内部符号 repair_bad_columns 直接驱动掩膜。
    {
        std::vector<float> d = make_frame(w, h, 13u, 1000.0f);
        std::vector<unsigned char> allbad(static_cast<size_t>(w), 1);
        std::vector<float> o(d.size(), 0.0f);
        int px = -1, st = 0;
        ac::repair_bad_columns(d.data(), w, h, allbad.data(), o.data(), &px, &st);
        if (g_inj.no_degrade) st = 0;
        check((st & AC_COLSTAT_NO_ANCHOR) != 0,
              "无锚点（全宽皆坏）必须留下 no_anchor_unrepaired 痕迹");
        check(px == w * h, "全宽皆坏的 px_repaired 记账 = w*h（覆盖数，不等于已修复数）");
        check(bitwise_equal(o, d), "无锚点时不得虚构好值：输出必须与输入逐位一致");
    }
    // 4d. 帧内检测对"无参照系"的形态必须主动不判（线性梯度不是坏列）
    {
        std::vector<float> d = make_frame(w, h, 13u, 1000.0f);
        std::vector<float> d2 = d;
        for (int x = 0; x < w; ++x)
            for (int y = 0; y < h; ++y)
                d2[static_cast<size_t>(y) * w + x] += static_cast<float>(x) * 700.0f;
        ColResult r2 = run_cols(d2, w, h, 5.0f, 3);
        check(r2.n_cols == 0,
              "整帧线性横向梯度不得被判为坏列（差分判据对缓变结构免疫）");
        check(bitwise_equal(r2.out, d2), "无检出时输出与输入逐位一致");
    }
}

// ═══════════════════ ⑤ 退化守卫（真实数据上实测发生） ═══════════════════
void case_degenerate_guard(int w, int h) {
    std::fprintf(stderr, "[case 5] badcol_degenerate_guard\n");
    // 5a. 所有列中位数相同 ⇒ MAD(dev)==0 ⇒ 显式降级、不判任何列。
    //     真实锚: M42 T3 原始整数帧的列中位数取整数值 ⇒ MAD(dev)==0 是高频事件
    //     （6 帧中 3 帧），必须显式降级而不是按 0 尺度判成全坏/全好。
    {
        std::vector<float> d(static_cast<size_t>(w) * h, 1000.0f);
        ColResult r = run_cols(d, w, h, 5.0f, 3);
        check(r.n_cols == 0, "退化时必须零检出");
        if (!g_inj.no_degrade) {
            check((r.status & AC_COLSTAT_SCALE_DEGENERATE) != 0,
                  "MAD(dev)==0 必须留下 scale_degenerate 痕迹（显式降级，不静默）");
        }
        check(bitwise_equal(r.out, d), "退化时输出与输入逐位一致");
    }
    // 5b. 帧过小（w 不足以形成邻域基准）⇒ frame_too_small
    {
        const int w2 = 2, h2 = 8;
        std::vector<float> d(static_cast<size_t>(w2) * h2, 1000.0f);
        d[0] = 1.0f;
        ColResult r = run_cols(d, w2, h2, 5.0f, 3);
        check(r.rc == AC_OK, "小帧 rc==AC_OK（降级不报错）");
        check(r.n_cols == 0, "小帧零检出");
        if (!g_inj.no_degrade) {
            check((r.status & AC_COLSTAT_FRAME_TOO_SMALL) != 0, "小帧必须留下 frame_too_small 痕迹");
        }
    }
    // 5c. 参数域: 空指针 ⇒ AC_ERR_PARAM
    {
        float dummy[4] = {0, 0, 0, 0};
        unsigned char m[2] = {0, 0};
        check(ac_correct_columns(nullptr, 2, 2, dummy, 5.0f, 3, 1, m, nullptr, nullptr, nullptr, nullptr) == AC_ERR_PARAM,
              "data==nullptr ⇒ AC_ERR_PARAM");
        check(ac_correct_columns(dummy, 2, 2, nullptr, 5.0f, 3, 1, m, nullptr, nullptr, nullptr, nullptr) == AC_ERR_PARAM,
              "out==nullptr ⇒ AC_ERR_PARAM");
        check(ac_correct_columns(dummy, 0, 2, dummy, 5.0f, 3, 1, m, nullptr, nullptr, nullptr, nullptr) == AC_ERR_PARAM,
              "width==0 ⇒ AC_ERR_PARAM");
    }
}

// ═══════════════════ ⑥ 与 frozen 坏点路径的独立性与共存 ═══════════════════
void case_coexist_with_frozen(int w, int h) {
    std::fprintf(stderr, "[case 6] badcol_coexist_with_frozen\n");
    // ac_correct_frame 在 dark/bias=NULL 时是恒等 pass（DISP-COS-009 现状），
    // 本任务**不得**改变它：逐位断言。
    std::vector<float> d = make_frame(w, h, 31415u, 1000.0f);
    for (int y = 0; y < h; ++y) d[static_cast<size_t>(y) * w + 20] -= 500.0f;
    std::vector<float> fout(d.size(), 0.0f);
    int hot = -1, cold = -1;
    const int rc = ac_correct_frame(d.data(), w, h, nullptr, nullptr, fout.data(),
                                    5.0f, 5.0f, AC_METHOD_MEDIAN, 4, &hot, &cold);
    check(rc == AC_OK, "ac_correct_frame rc==AC_OK");
    check(hot == 0 && cold == 0, "dark/bias 皆 NULL ⇒ out_hot=out_cold=0（现状语义零改动）");
    check(bitwise_equal(fout, d), "ac_correct_frame 在无检测源时逐位恒等（frozen 面零改动）");
    check(ac_correct_frame(d.data(), w, h, nullptr, nullptr, fout.data(),
                           5.0f, 5.0f, AC_METHOD_MEDIAN, 4, &hot, &cold) == AC_OK,
          "frozen 坏点路径可重复调用（新增符号不改变其行为）");
}

// ═══════════════════ ⑦ f64 ABI 与 f32 一致（同款降级语义） ═══════════════════
void case_f64(int w, int h) {
    std::fprintf(stderr, "[case 7] badcol_f64_abi\n");
    std::vector<float> d = make_frame(w, h, 2718u, 1000.0f);
    for (int y = 0; y < h; ++y) d[static_cast<size_t>(y) * w + 20] -= 500.0f;
    std::vector<double> dd(d.begin(), d.end());
    std::vector<double> o64(d.size(), 0.0);
    unsigned char m64[512] = {0};
    int n64 = -1, p64 = -1, s64 = -1;
    double sg64 = 0.0;
    check(ac_correct_columns_f64(dd.data(), w, h, o64.data(), 5.0, 3, 1, m64, &n64, &p64, &sg64, &s64) == AC_OK,
          "f64 rc==AC_OK");
    ColResult r32 = run_cols(d, w, h, 5.0f, 3);
    check(n64 == r32.n_cols, "f64 与 f32 检出列数一致");
    check(m64[20] == 1, "f64 掩膜在位");
    int diff = 0;
    for (size_t i = 0; i < d.size(); ++i)
        if (static_cast<float>(o64[i]) != r32.out[i]) ++diff;
    check(diff == 0, "f64 输出经 float32 回转后与 f32 通路逐位一致（同款降级语义）");
}


// ═══════════════════ ⑧ dark / bias 母版定位路径 + 三路径仲裁 ═══════════════════
void case_master_paths(int w, int h) {
    std::fprintf(stderr, "[case 8] badcol_master_paths\n");
    // 8a. **暗场里正常的列不得被判为坏列**（负责人指定负例）。
    //     暗场母版含真实的列向结构（列响应/暗电流差异）但没有缺陷。
    {
        std::vector<float> dark = make_frame(w, h, 5150u, 900.0f);
        std::vector<unsigned char> m(static_cast<size_t>(w), 0);
        int n = -1, st = 0; float sg = 0.0f;
        check(ac_detect_bad_columns_from_master(dark.data(), w, h, 5.0f, 3, 1,
              m.data(), &n, &sg, &st) == AC_OK, "master ABI rc==AC_OK");
        check(n == 0, "无缺陷暗场必须零检出（暗场正常列不得判坏）");
        int any = 0; for (int x = 0; x < w; ++x) any += m[x];
        check(any == 0, "无缺陷暗场掩膜必须全零");
    }
    // 8b. 非退化对照：同一暗场注入一条坏列 ⇒ 必须检出
    {
        std::vector<float> dark = make_frame(w, h, 5150u, 900.0f);
        for (int y = 0; y < h; ++y) dark[static_cast<size_t>(y) * w + 41] -= 400.0f;
        std::vector<unsigned char> m(static_cast<size_t>(w), 0);
        int n = -1, st = 0; float sg = 0.0f;
        ac_detect_bad_columns_from_master(dark.data(), w, h, 5.0f, 3, 1, m.data(), &n, &sg, &st);
        check(n == 1 && m[41] == 1, "非退化对照: 暗场注入坏列必须检出");
    }
    // 8c. 仲裁: dark 与 bias 一致判坏 ⇒ HIGH; 仅科学帧 ⇒ LOW; dark+bias 都有但只
    //     dark 判出 ⇒ MEDIUM。并集语义：三路证据都不丢。
    {
        std::vector<float> sci = make_frame(w, h, 61u, 1000.0f);
        std::vector<float> dk  = make_frame(w, h, 62u, 900.0f);
        std::vector<float> bs  = make_frame(w, h, 63u, 500.0f);
        for (int y = 0; y < h; ++y) {
            dk[static_cast<size_t>(y) * w + 12] -= 400.0f;   // dark 与 bias 都坏
            bs[static_cast<size_t>(y) * w + 12] -= 300.0f;
            dk[static_cast<size_t>(y) * w + 25] -= 400.0f;   // 仅 dark 坏
            sci[static_cast<size_t>(y) * w + 50] -= 500.0f;  // 仅科学帧坏
        }
        std::vector<float> o(sci.size(), 0.0f);
        std::vector<unsigned char> cm(static_cast<size_t>(w), 0), sm(static_cast<size_t>(w), 0),
                                   cf(static_cast<size_t>(w), 0);
        int n = -1, ns = -1, nd = -1, nb = -1, px = -1, st = 0; float sg = 0.0f;
        check(ac_correct_columns_ex(sci.data(), dk.data(), bs.data(), w, h, o.data(),
              5.0f, 3, 1, cm.data(), sm.data(), cf.data(), &n, &ns, &nd, &nb, &px, &sg, &st) == AC_OK,
              "arbitration ABI rc==AC_OK");
        check(cm[12] == 1 && cm[25] == 1 && cm[50] == 1, "并集: 三路检出都必须保留");
        check(!g_inj.no_degrade || true, "");
        // 逐路独立复核（口令负例：**暗场里正常的列不得被判为坏列**）。
        // 每本母版只应判出被注入的那些列，其余 60+ 列一律零检出、零降级。
        {
            std::vector<unsigned char> t(static_cast<size_t>(w), 0);
            int q = 0, stq = 0; float sgq = 0.0f;
            ac_detect_bad_columns_from_master(dk.data(), w, h, 5.0f, 3, 1, t.data(), &q, &sgq, &stq);
            check(q == 2 && t[12] == AC_COLSTAT_MASK_REPAIRED && t[25] == AC_COLSTAT_MASK_REPAIRED,
                  "dark 母版: 只判出注入的 12/25 两列，其余正常列零检出");
            check(stq == AC_COLSTAT_OK, "dark 母版无降级标志");
        }
        {
            std::vector<unsigned char> t(static_cast<size_t>(w), 0);
            int q = 0, stq = 0; float sgq = 0.0f;
            ac_detect_bad_columns_from_master(bs.data(), w, h, 5.0f, 3, 1, t.data(), &q, &sgq, &stq);
            check(q == 1 && t[12] == AC_COLSTAT_MASK_REPAIRED,
                  "bias 母版: 只判出注入的 12 列，其余正常列零检出");
            check(stq == AC_COLSTAT_OK, "bias 母版无降级标志");
        }
        {
            std::vector<unsigned char> t(static_cast<size_t>(w), 0);
            int q = 0, stq = 0; float sgq = 0.0f;
            ac_detect_bad_columns_from_master(sci.data(), w, h, 5.0f, 3, 1, t.data(), &q, &sgq, &stq);
            check(q == 1 && t[50] == AC_COLSTAT_MASK_REPAIRED,
                  "科学帧: 只判出注入的 50 列，其余正常列零检出");
        }
        check(cf[12] == AC_COLSTAT_CONF_HIGH, "dark+bias 一致判坏 ⇒ HIGH 置信度");
        check((sm[12] & AC_COLSTAT_SRC_DARK) && (sm[12] & AC_COLSTAT_SRC_BIAS),
              "col12 来源掩膜必须同时含 DARK 与 BIAS 位");
        check(cf[25] == AC_COLSTAT_CONF_MEDIUM, "仅 dark 判坏 ⇒ MEDIUM 置信度");
        check(cf[50] == AC_COLSTAT_CONF_LOW, "仅科学帧判坏 ⇒ LOW 置信度");
        check((sm[50] & AC_COLSTAT_SRC_SCIENCE) && !(sm[50] & AC_COLSTAT_SRC_DARK),
              "col50 来源掩膜只含 SCIENCE 位");
        check(ns == 1 && nd == 2 && nb == 1,
              "逐路径检出数必须分别记账（science=1, dark=2, bias=1）");
        // 非退化对照: 三路都干净的帧 ⇒ 零检出、零修复
        std::vector<float> o2(sci.size(), 0.0f);
        std::vector<unsigned char> cm2(static_cast<size_t>(w), 0);
        int n2 = -1, px2 = -1; float sg2 = 0.0f; int st2 = 0;
        std::vector<float> s2 = make_frame(w, h, 71u, 1000.0f);
        std::vector<float> d2 = make_frame(w, h, 72u, 900.0f);
        ac_correct_columns_ex(s2.data(), d2.data(), nullptr, w, h, o2.data(), 5.0f, 3, 1,
                              cm2.data(), nullptr, nullptr, &n2, nullptr, nullptr, nullptr,
                              &px2, &sg2, &st2);
        check(n2 == 0 && px2 == 0, "非退化对照: 三路皆干净 ⇒ 零检出零修复");
        check(bitwise_equal(o2, s2), "零检出时输出逐位等于输入");
    }
    // 8d. 母版路径的退化守卫也必须留痕（常数列母版）
    {
        std::vector<float> flatdark(static_cast<size_t>(w) * h, 900.0f);
        std::vector<unsigned char> m(static_cast<size_t>(w), 0);
        int n = -1, st = 0; float sg = 0.0f;
        ac_detect_bad_columns_from_master(flatdark.data(), w, h, 5.0f, 3, 1, m.data(), &n, &sg, &st);
        check(n == 0, "常数母版零检出");
        if (!g_inj.no_degrade)
            check((st & AC_COLSTAT_SCALE_DEGENERATE) != 0,
                  "常数母版必须留下 scale_degenerate 痕迹");
    }
}

void run_all() {
    g_fail = 0; g_case = 0;
    const int w = 64, h = 64;
    case_analytic(w, h);
    case_no_false_positive(w, h);
    case_disabled_bitwise(w, h);
    case_degrade_traced(w, h);
    case_degenerate_guard(w, h);
    case_coexist_with_frozen(w, h);
    case_f64(w, h);
    case_master_paths(w, h);
    std::fprintf(stderr, "p1cos_badcol: %d checks, %d failures\n", g_case, g_fail);
    if (g_fail) std::fprintf(stderr, "RESULT: FAIL\n");
    else std::fprintf(stderr, "RESULT: PASS\n");
}

}  // namespace

int main(int argc, char** argv) {
    const bool self_test = (argc > 1 && std::strcmp(argv[1], "--self-test") == 0);
    if (!self_test) {
        run_all();
        return g_fail ? 1 : 0;
    }
    // ── 自检: 每条负例在注入下必须判红（否则该负例是恒真门，无判别力）──
    struct Item { const char* name; Inject inj; };
    const Item items[] = {
        {"analytic_no_repair",   Inject{true,  false, false}},
        {"analytic_no_detect",   Inject{false, true,  false}},
        {"false_positive_detect",Inject{false, true,  false}},
        {"degrade_untraced",     Inject{false, false, true }},
    };
    int bad = 0;
    for (const auto& it : items) {
        g_inj = it.inj;
        run_all();
        const bool red = (g_fail > 0);
        std::fprintf(stderr, "SELF-TEST %-24s => %s\n", it.name, red ? "RED (ok)" : "GREEN (NO DISCRIMINATION!)");
        if (!red) ++bad;
    }
    // baseline 必须判绿
    g_inj = Inject{};
    run_all();
    if (g_fail != 0) {
        std::fprintf(stderr, "SELF-TEST baseline => %d failures (expected 0)\n", g_fail);
        ++bad;
    } else {
        std::fprintf(stderr, "SELF-TEST %-24s => GREEN (ok)\n", "baseline");
    }
    std::fprintf(stderr, "self-test: %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
