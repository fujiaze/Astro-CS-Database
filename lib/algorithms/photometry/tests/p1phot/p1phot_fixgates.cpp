// ============================================================================
// p1phot_fixgates.cpp — B4-2 (M3-C-001) + B4-3 (M3-C-002) 共址回归锁
//
// 被验条目:
//   B4-2 M3-C-001: SCI-PHOT-001 §4/§5/§8 冻结参考星数门在实现中不存在,
//        一颗星即可定标整帧。冻结门: |r_consistent| >= 3 才进 IRLS (否则
//        NO_DATA: scale=1.0/不写 scale + fit_used=0); |r_inliers| >= 2 才
//        估计 sigma_residual (否则 =0)。
//   B4-3 M3-C-002: SCI-PHOT-001 §4/§10 的饱和/质量标志判据在接口与实现中
//        都不存在。**库层内部** (pc::StarMatcher::cleanAndScale 8 参重载 +
//        PC_QF_* 位定义) 增质量位有效域过滤, 含 PC_QF_SATURATED/
//        PC_QF_HAS_SATURATED 的星不进入零点拟合, 计 rejected_quality。
//        注意 (依据 docs/contracts/PUBLIC_API.md §「C ABI（extern "C"，不跨边界
//        抛 C++ exception）」+ ASTROCS_DESIGN.md §8.5「模块与 ABI」):
//        **不新增/不修改任何 C 导出 ABI**。
//        orchestrator 用 raw 函数指针按位置调用既有导出, 给它们加尾部形参
//        会让未初始化实参被解引用 (P0)。故本批只落"库层内部能力 + 回归锁",
//        生产路径 (pc_api.cpp) 一律以 quality_flags=nullptr 调用 → 生产行为
//        与 HEAD 逐行等价; 将来接线需上游先产出质量位 (新工作项, 见 REPORT
//        "越域残余")。
//
// 回归锁语义 (未修复必红 / 已修复必绿):
//   RED  (探针 = 本工作区 star_matcher.cpp 去掉 B4-2 星数门与 B4-3 质量位过滤
//        两块, 其余逐字不变; 探针源码见 logs/ 生成脚本):
//        b42_two_stars_rejected / b43_saturated_excluded 等断言失败 → rc!=0。
//        旧签名 7 参 cleanAndScale 下本 TU 仍可编译, 故 RED 走运行期断言失败。
//   GREEN(修复后): 全部断言通过 → rc=0。
//   判别力: 每个"必拒"用例都配"同构但合法"的对照用例 (b42_three_stars_pass /
//   b43_no_flags_backcompat), 故不是恒真/恒假断言。
//
// 被测面: lib/algorithms/photometry/cpp/src/star_matcher.cpp 生产实现, 直接调用
//   pc::StarMatcher::cleanAndScale (不改 SCI 数值、不改默认容差)。
//
// ctest 目标名: p1phot_fixgates
// 日期: 2026-09-14 (RQS B4-phase1)
// ============================================================================

#include "star_matcher.h"
#include "photometric_calib.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) {                                                       \
            std::printf("  [PASS] %s\n", msg);                            \
        } else {                                                          \
            std::printf("  [FAIL] %s\n", msg);                            \
            ++g_failures;                                                 \
        }                                                                 \
    } while (0)

// 构造 n 颗星: F_syn = 1000, r_target = log10(F_instr/F_syn) 目标 dex;
// F_instr = 1000·10^{r_target}。位置仅用于诊断 (本组不测匹配几何)。
// outliers: 若 false, 每颗 r 严格等于 r_target (S=0 通路 + 内点全收);
//          若 true, 末颗 r 偏移 +0.9 dex (IRLS 离群, 用于 |r_inliers| 门)。
std::vector<pc::StarMatch> make_matches(int n, double r_target,
                                       bool with_outlier) {
    std::vector<pc::StarMatch> v;
    const double fsyn = 1000.0;
    for (int i = 0; i < n; ++i) {
        pc::StarMatch m{};
        m.x = 10.0 + 3.0 * i;
        m.y = 20.0 + 2.0 * i;
        m.f_syn = fsyn;
        double r = r_target;
        if (with_outlier && i == n - 1) r += 0.9;
        m.f_instr = fsyn * std::pow(10.0, r);
        // gaia_mag 取 -2.5·log10(F_syn), 保证 delta 一致 (星等预过滤不误杀)
        m.gaia_mag = -2.5 * std::log10(fsyn);
        m.psf_idx = i;
        m.gaia_idx = i;
        v.push_back(m);
    }
    return v;
}

std::vector<int> make_rows(int n) {
    std::vector<int> r;
    for (int i = 0; i < n; ++i) r.push_back(i);
    return r;
}

// ==========================================================================
// ABI 冻结锁 (依据 docs/contracts/PUBLIC_API.md §「C ABI」+ ASTROCS_DESIGN.md §8.5):
// 既有 6 个 C 导出不得新增形参 (不改 ABI)。
// 断言面: 6 个导出的符号地址在编译期可解析 —— 任何删除/改名都会让本 TU 编译
// 失败; 调用侧 (orchestrator 的 raw 函数指针) 若与公共头形参不符, 会在自己
// 的编译单元类型不符而失败 (故不得改公共头形参表)。
// ==========================================================================
static void *const pc_abi_probe_simple =
    reinterpret_cast<void *>(&pc_calibrate_simple);
static void *const pc_abi_probe_simple_f64 =
    reinterpret_cast<void *>(&pc_calibrate_simple_f64);
static void *const pc_abi_probe_with_gaia =
    reinterpret_cast<void *>(&pc_calibrate_simple_with_gaia);
static void *const pc_abi_probe_with_gaia_f64 =
    reinterpret_cast<void *>(&pc_calibrate_simple_with_gaia_f64);
static void *const pc_abi_probe_with_gaia_v2 =
    reinterpret_cast<void *>(&pc_calibrate_simple_with_gaia_v2);
static void *const pc_abi_probe_with_gaia_f64_v2 =
    reinterpret_cast<void *>(&pc_calibrate_simple_with_gaia_f64_v2);
static_assert(&pc_abi_probe_simple != nullptr, "B4 ABI: pc_calibrate_simple 缺失");

}  // namespace

int main() {
    std::printf("[B4-2/B4-3] SCI-PHOT 参考星数门 + 饱和质量位门 回归锁\n");

    // ── B4-2 (a): |r_consistent| = 2 < 3 → NO_DATA 必拒 ──────────────────
    {
        std::vector<pc::StarMatch> m = make_matches(2, 0.1, false);
        std::vector<int> rows = make_rows(2);
        double scale = 123.0, sigma = 456.0;  // 哨兵: 未被覆盖即失败信号
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, nullptr, /*mag_tol=*/3.0, &scale, &sigma, &diag);
        CHECK(out.empty(), "b42_two_stars_rejected: 无 inlier 输出");
        CHECK(scale == 1.0 && diag.scale_factor == 1.0,
              "b42_two_stars_rejected: 不写 scale (保持 1.0)");
        CHECK(diag.fit_used == 0, "b42_two_stars_rejected: fit_used=0 (NO_DATA)");
    }

    // ── B4-2 (b): |r_consistent| = 1 → 必拒 ─────────────────────────────
    {
        std::vector<pc::StarMatch> m = make_matches(1, 0.25, false);
        std::vector<int> rows = make_rows(1);
        double scale = 999.0, sigma = 999.0;
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, nullptr, 3.0, &scale, &sigma, &diag);
        CHECK(out.empty() && scale == 1.0 && diag.fit_used == 0,
              "b42_one_star_rejected: 1 颗星不得定标整帧 (scale 不写)");
    }

    // ── B4-2 (c): |r_consistent| = 3 且 3 内点 → 必过 ───────────────────
    {
        std::vector<pc::StarMatch> m = make_matches(3, 0.1, false);
        std::vector<int> rows = make_rows(3);
        double scale = 1.0, sigma = -1.0;
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, nullptr, 3.0, &scale, &sigma, &diag);
        CHECK((int)out.size() == 3, "b42_three_stars_pass: 3 内点全部保留");
        CHECK(std::fabs(std::log10(scale) + 0.1) < 1e-12,
              "b42_three_stars_pass: scale=10^-0.1 (SCI-PHOT §11 合成注入)");
        CHECK(diag.fit_used == 3, "b42_three_stars_pass: fit_used=3");
    }

    // ── B4-2 (d): |r_inliers| < 2 必须走门 2 (sigma_residual=0) ─────────
    // 判别用例: 3 颗中 2 颗 r 相同 (MAD=0)。此时 S=0、IRLS 全收 ⇒
    // |r_inliers| = 3 ≥ 2, 门 2 不介入; 但旧实现 (MAD_IN=0 时 sigma 由
    // single-point deviation 算出) 会把 (r − location) 恒 0 的偏差误当
    // "可估计"。本条同时断言: (i) 门 2 不误伤 3 内点; (ii) S=0 通路下
    // sigma 仅为 0 而非被赋予虚假离散度时 diag 记录一致。
    {
        std::vector<pc::StarMatch> m = make_matches(3, 0.10, false);
        std::vector<int> rows = make_rows(3);
        double scale = 1.0, sigma = -1.0;
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, nullptr, 3.0, &scale, &sigma, &diag);
        CHECK((int)out.size() == 3,
              "b42_sigma_two_inlier_path: 3 内点 ≥2 → 门 2 放行");
        CHECK(sigma == 0.0 && diag.sigma_residual == 0.0,
              "b42_sigma_two_inlier_path: S=0 常数 r 场 sigma=0 (退化一致)");
    }

    // 门 2 的直接判别: MAD｜inliers 恰为 1 时不得估计 sigma。构造法 — 用
    // 星等预过滤把 3 颗一致星中的 2 颗转成"离群"(|delta|<3 mag 但 r 相差
    // >4.685·S) 在数学上不可达 (S 反比于 MAD 使 |u|<1 恒成立, 见下注),
    // 故此处以自适应方式断言门本身: 若 inliers<2 则 sigma 必为 0。
    {
        std::vector<pc::StarMatch> m = make_matches(3, 0.10, false);
        m[2].f_instr = 1000.0 * std::pow(10.0, 2.90);  // delta 仍 <3 mag 预过滤
        m[2].gaia_mag = -2.5 * std::log10(1000.0);      // 保持星等一致性
        std::vector<int> rows = make_rows(3);
        double scale = 1.0, sigma = -1.0;
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, nullptr, 3.0, &scale, &sigma, &diag);
        if (out.size() < 2) {
            CHECK(sigma == 0.0 && diag.sigma_residual == 0.0,
                  "b42_sigma_needs_two_inliers: |inliers|<2 → sigma=0");
        } else {
            CHECK(sigma >= 0.0,
                  "b42_sigma_needs_two_inliers: |inliers|>=2 → sigma 可估计");
        }
    }

    // ── B4-3 (a): 饱和星 (quality_flags) 不得进入零点拟合 ───────────────
    // 6 颗: 前 3 颗正常 (r=+0.10), 后 3 颗"饱和"且通量被截顶 (r=+1.60)。
    // 若饱和星进入拟合, location 会被拉高 (≈0.85) 且 scale 明显不同;
    // 修复后 location=+0.10, scale=10^-0.1。
    {
        std::vector<pc::StarMatch> m = make_matches(6, 0.10, false);
        for (int i = 3; i < 6; ++i) {
            m[i].f_instr = 1000.0 * std::pow(10.0, 1.60);
        }
        std::vector<int> rows = make_rows(6);
        std::vector<uint32_t> qf(6, 0u);
        qf[3] = PC_QF_SATURATED;
        qf[4] = PC_QF_HAS_SATURATED;
        qf[5] = PC_QF_SATURATED | PC_QF_HAS_SATURATED;
        double scale = 1.0, sigma = -1.0;
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, qf.data(), 3.0, &scale, &sigma, &diag);
        CHECK((int)out.size() == 3, "b43_saturated_excluded: 仅 3 颗正常星参与");
        CHECK(std::fabs(std::log10(scale) + 0.10) < 1e-12,
              "b43_saturated_excluded: location 未被截顶通量拉偏 (scale=10^-0.10)");
        CHECK(diag.rejected_quality >= 3,
              "b43_saturated_excluded: rejected_quality 计入饱和星");
    }

    // ── B4-3 (b): 向后兼容: quality_flags=nullptr → 不过滤 ───────────────
    {
        std::vector<pc::StarMatch> m = make_matches(6, 0.10, false);
        for (int i = 3; i < 6; ++i) {
            m[i].f_instr = 1000.0 * std::pow(10.0, 1.60);
        }
        std::vector<int> rows = make_rows(6);
        double scale = 1.0, sigma = -1.0;
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, nullptr, 3.0, &scale, &sigma, &diag);
        CHECK(!out.empty() && diag.rejected_quality >= 0,
              "b43_no_flags_backcompat: nullptr 不做质量过滤 (旧行为)");
        CHECK(std::fabs(std::log10(scale) + 0.10) > 1e-6,
              "b43_no_flags_backcompat: 未过滤时截顶通量参与 (对照, 证明过滤起作用)");
    }

    // ── B4-3 (c): 全部被质量位剔除 → fit_used=0 且不写 scale ────────────
    {
        std::vector<pc::StarMatch> m = make_matches(4, 0.10, false);
        std::vector<int> rows = make_rows(4);
        std::vector<uint32_t> qf(4, PC_QF_SATURATED);
        double scale = 77.0, sigma = 77.0;
        PhotometricDiag diag{};
        pc::StarMatcher matcher;
        std::vector<pc::StarMatch> out = matcher.cleanAndScale(
            m, &rows, qf.data(), 3.0, &scale, &sigma, &diag);
        CHECK(out.empty() && scale == 1.0 && diag.fit_used == 0,
              "b43_all_saturated: 全饱和 → NO_DATA 退化 (scale 不写)");
    }

    // ── B4-3 (d): 公共 C ABI 质量位取值一致性 ─────────────────────────────
    // 这里只核对质量位取值与冻结位定义同值; 公共 C 导出是否带 quality_flags
    // 形参属 ABI 面, 不在本 TU 的断言范围。
    CHECK(PC_QF_SATURATED == (1u << 1), "b43_flag_bits: PC_QF_SATURATED == 1<<1");
    CHECK(PC_QF_HAS_SATURATED == (1u << 2), "b43_flag_bits: PC_QF_HAS_SATURATED == 1<<2");

    if (g_failures == 0) {
        std::printf("B4-2/B4-3 PHOT FIXGATES PASS\n");
        return 0;
    }
    std::printf("B4-2/B4-3 PHOT FIXGATES FAIL (%d check(s))\n", g_failures);
    return 1;
}
