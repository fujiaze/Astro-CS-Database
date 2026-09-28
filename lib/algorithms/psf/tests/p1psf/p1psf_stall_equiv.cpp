// ============================================================================
// P1-PSF-TEST · PSF-PERF-001 验收面（批路径"无进展"提前退出；DISP-PSF-003 整改）
// ----------------------------------------------------------------------------
// 合同锚（沿索引链读到的权威）:
//   · docs/science/algorithms/STAR_PSF_ALGORITHMS.md §11.2（拟合失败语义，冻结）
//     —— 批接口对 status!=OK 一律写 NaN、不计 valid、out_status=FIT_FAILED
//        ⇒ **批产物只取决于"是否 status==0"**，这是本文件等价判据的依据。
//   · 同文档 §11.3 DISP-PSF-002/003（登记不改码的不一致，整改归 P1-PSF-IMPL）:
//     前向差分雅可比破坏二阶收敛 + maxIter/tolerance 为死参数 ⇒ 不得在
//     "已无进展"的候选上继续耗满 200 次迭代。
//   · 同文档 §9 冻结容差: FP64 LM 位置 0.05 px、FWHM 1%。
//
// 双向负例（"丢的确实丢、留的确实留"）:
//   [留] positive 组: 合成 Moffat4 星（真值由 p1psf_oracle 独立生成，先于被测
//        函数存在）在批路径下必须拟合成功且达冻结容差 ⇒ 提前退出没吃掉真星。
//   [丢] equiv/junk 组: 对同一候选面，**批路径（开提前退出）** 与 **单星路径
//        （不开）** 必须给出逐位相同的 OK 集合与 9 参数 ⇒ 优化不改变产物。
//        junk 组另加"非空断言"：语料里必须同时存在两条路径都 OK 与都非 OK 的
//        候选，否则等价比较是空断言（禁止）。
//
// 能红能绿（ENGINEERING_SPEC §8 / AGENTS §9）: 同一份源码编两个 target —
//   · p1psf_stall_equiv        : 默认阈值 60 ⇒ 断言"等价成立" ⇒ 绿；
//   · p1psf_stall_equiv_inject : -DP1PSF_STALL_INJECT -DDPSF_BATCH_STALL_ITERS=2
//     ⇒ 阈值压到 2 使提前退出真的截断正常收敛星 ⇒ 断言"不等价被检出" ⇒
//     打印 NEGATIVE INJECTION DETECTED（未检出则判红，证明该判据能红）。
// ============================================================================
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "dynamic_psf.h"
#include "dpsf_psf.h"

// 生产源 dpsf_log.cpp 的日志追加写经 aio 机制原语 (header-only) ⇒ 需 aio/src include 面
// （与 lib/algorithms/psf/tests/p1psf/CMakeLists.txt 的 include_directories 同口径）
#include "p1psf_fixtures.hpp"
#include "p1psf_oracle.hpp"
#include "p1psf_test_main.hpp"

namespace {

constexpr int kFitRadius = 8;   // 与 DPSFFitParams 生产默认一致

struct Candidate {
    double x = 0.0, y = 0.0;
    int    kind = 2;            // 0 = 合成 Moffat4 星 / 1 = 单像素脉冲 / 2 = 纯噪声位
    double sx_true = 0.0, sy_true = 0.0, theta_true = 0.0;
};

struct Corpus {
    int W = 0, H = 0;
    std::vector<double> image;  // [H,W]
    std::vector<Candidate> cands;
};

// 合成语料: 高斯噪声底 + 12 颗 Moffat4 星 + 8 个单像素脉冲 + 8 个纯噪声位置。
// 固定 seed（splitmix64 谱系）⇒ 任何平台/优化级别逐位可复现。
Corpus build_corpus() {
    Corpus c;
    c.W = 192; c.H = 192;
    c.image.assign((size_t)c.W * c.H, 0.0);
    uint64_t st = 20260929ull;
    const double B = 100.0, noise = 5.0;
    for (auto& v : c.image) v = B + noise * p1psf::splitmix_gauss(st);

    for (int gy = 0; gy < 3; ++gy)
        for (int gx = 0; gx < 4; ++gx) {
            Candidate d;
            d.kind = 0;
            d.x = 30.0 + gx * 44.0 + 6.0 * p1psf::splitmix_uniform(st);
            d.y = 30.0 + gy * 60.0 + 6.0 * p1psf::splitmix_uniform(st);
            d.sx_true = 2.0 + 0.4 * p1psf::splitmix_uniform(st);
            d.sy_true = 1.5 + 0.4 * p1psf::splitmix_uniform(st);
            d.theta_true = 0.4 * p1psf::splitmix_uniform(st);
            const double A = 3000.0 + 2000.0 * p1psf::splitmix_uniform(st);
            for (int y = 0; y < c.H; ++y)
                for (int x = 0; x < c.W; ++x)
                    c.image[(size_t)y * c.W + x] += p1psf::moffat4_eval(
                        0.0, A, d.x, d.y, d.sx_true, d.sy_true, d.theta_true, (double)x, (double)y);
            c.cands.push_back(d);
        }
    for (int k = 0; k < 8; ++k) {          // 单像素脉冲（热像元/宇宙线形态）
        Candidate d;
        d.kind = 1;
        d.x = 20.0 + 20.0 * k + 2.0 * p1psf::splitmix_uniform(st);
        d.y = 170.0;
        c.image[(size_t)(int)d.y * c.W + (int)d.x] += 5000.0;
        c.cands.push_back(d);
    }
    for (int k = 0; k < 8; ++k) {          // 纯噪声位（无源）
        Candidate d;
        d.kind = 2;
        d.x = 20.0 + 20.0 * k + 2.0 * p1psf::splitmix_uniform(st);
        d.y = 110.0;
        c.cands.push_back(d);
    }
    return c;
}

std::vector<double> detections_of(const Corpus& c) {
    std::vector<double> d((size_t)c.cands.size() * 6, 0.0);   // star_det v1 [N,6]
    for (size_t i = 0; i < c.cands.size(); ++i) {
        d[i * 6 + 0] = c.cands[i].x;
        d[i * 6 + 1] = c.cands[i].y;
    }
    return d;
}

// 单星参考路径 = moffat4_fit_d 公开 ABI（**不开**提前退出）。
// patch 裁剪与坐标口径逐位复刻批路径（dpsf_psf.cpp dpsf_fit_batch_f64 循环体:
// x0=max(0,cx-r) … x1=min(W,cx+r+1)；拟合后 status∈{OK,ITER_LIMIT} 时 cx+=x0）。
struct SingleFit {
    int    status = -1;
    double v[9] = {0};   // B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y（图像坐标）
};

SingleFit fit_single_star(const Corpus& c, const Candidate& d) {
    SingleFit out;
    int x0 = std::max(0, (int)d.x - kFitRadius);
    int y0 = std::max(0, (int)d.y - kFitRadius);
    int x1 = std::min(c.W, (int)d.x + kFitRadius + 1);
    int y1 = std::min(c.H, (int)d.y + kFitRadius + 1);
    const int rw = x1 - x0, rh = y1 - y0;
    if (rw <= 0 || rh <= 0) { out.status = DPSF_FIT_INVALID_PARAMS; return out; }
    std::vector<double> patch((size_t)rw * rh);
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            patch[(size_t)(y - y0) * rw + (x - x0)] = c.image[(size_t)y * c.W + x];
    DPSFFitResult res;
    std::memset(&res, 0, sizeof(res));
    out.status = moffat4_fit_d(patch.data(), rw, rh, d.x - x0, d.y - y0, 0, 0, rw, rh, &res);
    if (res.status == DPSF_FIT_OK || res.status == DPSF_FIT_ITERATION_LIMIT) {
        res.cx += x0;
        res.cy += y0;
    }
    out.v[0] = res.B; out.v[1] = res.A; out.v[2] = res.cx; out.v[3] = res.cy;
    out.v[4] = res.sx; out.v[5] = res.sy; out.v[6] = res.theta;
    out.v[7] = res.fwhm_x; out.v[8] = res.fwhm_y;
    return out;
}

struct BatchOut {
    int rc = -1, n_valid = 0;
    std::vector<int>    status;   // [N] DPSF_PSF_STATUS_*
    std::vector<double> params;   // [N,9] 原位；最终 compact 到前 n_valid 行
};

BatchOut run_batch(const Corpus& c, int* rc_out) {
    BatchOut b;
    const std::vector<double> dets = detections_of(c);
    const int n = (int)c.cands.size();
    b.status.assign(n, -1);
    b.params.assign((size_t)n * 9, 0.0);
    if (rc_out) {
        *rc_out = dpsf_fit_batch_f64(c.image.data(), c.W, c.H, dets.data(), n, nullptr,
                                     b.params.data(), &b.n_valid, b.status.data());
    }
    return b;
}

// 逐候选比较两条路径；返回"不等价"的候选数（0 = 等价）。
int compare_paths(const Corpus& c, const BatchOut& b, int* n_both_ok, int* n_both_fail) {
    int diffs = 0, both_ok = 0, both_fail = 0, k = 0;
    for (size_t i = 0; i < c.cands.size(); ++i) {
        const SingleFit s = fit_single_star(c, c.cands[i]);
        const bool s_ok = (s.status == DPSF_FIT_OK);
        const bool b_ok = (b.status[(int)i] == DPSF_PSF_STATUS_OK);
        if (s_ok != b_ok) {
            std::fprintf(stderr,
                "[stall-equiv] 候选 %zu (kind=%d x=%.2f y=%.2f) OK 集合不一致: 单星=%d 批=%d\n",
                i, c.cands[i].kind, c.cands[i].x, c.cands[i].y, (int)s_ok, (int)b_ok);
            ++diffs;
            continue;
        }
        if (!s_ok) { ++both_fail; continue; }
        ++both_ok;
        const double* brow = &b.params[(size_t)k * 9];
        if (std::memcmp(brow, s.v, 9 * sizeof(double)) != 0) {
            std::fprintf(stderr, "[stall-equiv] 候选 %zu 参数非按位相同:\n", i);
            static const char* nm[9] = {"B","A","cx","cy","sx","sy","theta","fwhm_x","fwhm_y"};
            for (int j = 0; j < 9; ++j)
                if (std::memcmp(&brow[j], &s.v[j], sizeof(double)) != 0)
                    std::fprintf(stderr, "    %-6s 批=%.17g 单星=%.17g\n", nm[j], brow[j], s.v[j]);
            ++diffs;
        }
        ++k;
    }
    if (k != b.n_valid) {
        std::fprintf(stderr, "[stall-equiv] n_valid 不一致: 批=%d 命中成功行=%d\n", b.n_valid, k);
        ++diffs;
    }
    if (n_both_ok) *n_both_ok = both_ok;
    if (n_both_fail) *n_both_fail = both_fail;
    return diffs;
}

// ── 组 1（留的确实留）: 合成星必须拟合成功且达冻结容差 ──────────────────────
int group_positive() {
    p1psf::CheckState cs;
    const Corpus c = build_corpus();
    int rc = -1;
    const BatchOut b = run_batch(c, &rc);
    P1PSF_CHECK_EQ(cs, rc, 0);
    if (rc != 0) return cs.failures ? 1 : 0;

    int n_star = 0, n_star_ok = 0, k = 0;
    double worst_pos = 0.0, worst_fwhm_rel = 0.0;
    for (size_t i = 0; i < c.cands.size(); ++i) {
        if (c.cands[i].kind != 0) continue;
        ++n_star;
        if (b.status[(int)i] != DPSF_PSF_STATUS_OK) {
            std::fprintf(stderr, "[stall-equiv] 合成星 %zu 未拟合成功 (status=%d)\n",
                         i, b.status[(int)i]);
            continue;
        }
        ++n_star_ok;
        const double* r = &b.params[(size_t)k * 9];
        ++k;
        worst_pos = std::max(worst_pos, std::max(std::fabs(r[2] - c.cands[i].x),
                                                std::fabs(r[3] - c.cands[i].y)));
        // θ 简并: 比较 FWHM 的**无序对**（θ↔θ+π/2 与 sx↔sy 互换是同一条曲线）
        double gf[2] = {r[7], r[8]}, wf[2] = {p1psf::oracle_fwhm(c.cands[i].sx_true),
                                              p1psf::oracle_fwhm(c.cands[i].sy_true)};
        std::sort(gf, gf + 2); std::sort(wf, wf + 2);
        for (int j = 0; j < 2; ++j)
            worst_fwhm_rel = std::max(worst_fwhm_rel, std::fabs(gf[j] - wf[j]) / wf[j]);
    }
    P1PSF_CHECK_MSG(cs, n_star == 12, nullptr, "合成星数=%d 期望 12", n_star);
    P1PSF_CHECK_MSG(cs, n_star_ok >= 11, nullptr,
                    "合成星拟合成功 %d/12（提前退出不得吃掉真星）", n_star_ok);
    P1PSF_CHECK_MSG(cs, worst_pos <= 0.05, nullptr,
                    "位置最差误差 %.4f px > 0.05 px（§9 冻结容差）", worst_pos);
    P1PSF_CHECK_MSG(cs, worst_fwhm_rel <= 0.01, nullptr,
                    "FWHM 最差相对误差 %.4f > 1%%（§9 冻结容差）", worst_fwhm_rel);
    std::printf("[stall-equiv] positive: 星 %d/12 成功, 最差位置 %.4f px, 最差 FWHM %.4f%%\n",
                n_star_ok, worst_pos, worst_fwhm_rel * 100.0);
    return cs.failures == 0 ? 0 : 1;
}

// ── 组 2/3（丢的确实丢 + 等价）───────────────────────────────────────────────
int run_equiv_group(p1psf::CheckState& cs, const char* tag) {
    const Corpus c = build_corpus();
    int rc = -1;
    const BatchOut b = run_batch(c, &rc);
    P1PSF_CHECK_EQ(cs, rc, 0);
    if (rc != 0) return cs.failures == 0 ? 0 : 1;

    int both_ok = 0, both_fail = 0;
    const int diffs = compare_paths(c, b, &both_ok, &both_fail);
    // 非空断言: 两侧都必须有样本, 否则等价比较是空断言（禁止）
    P1PSF_CHECK_MSG(cs, both_ok > 0, nullptr, "%s: 无两侧皆 OK 的候选 ⇒ 空断言", tag);
    P1PSF_CHECK_MSG(cs, both_fail > 0, nullptr, "%s: 无两侧皆失败的候选 ⇒ 空断言", tag);
    P1PSF_CHECK_MSG(cs, diffs == 0, nullptr,
                    "%s: 批路径(提前退出) 与 单星路径 有 %d 处不等价", tag, diffs);
    std::printf("[stall-equiv] %s: 候选 %zu（两侧皆 OK %d / 皆失败 %d），不等价 %d 处\n",
                tag, c.cands.size(), both_ok, both_fail, diffs);
    // 形态表征（只报不断言, 供报告引用）: 单像素脉冲类候选的结局分布
    int imp = 0, imp_ok = 0;
    for (size_t i = 0; i < c.cands.size(); ++i) {
        if (c.cands[i].kind != 1) continue;
        ++imp;
        if (b.status[(int)i] == DPSF_PSF_STATUS_OK) ++imp_ok;
    }
    std::printf("[stall-equiv] %s: 单像素脉冲候选 %d 个, 其中 %d 个仍被判 OK（表征, 非判据）\n",
                tag, imp, imp_ok);
    return diffs;
}

int group_equiv() {
    p1psf::CheckState cs;
    const int diffs = run_equiv_group(cs, "equiv");
#ifdef P1PSF_STALL_INJECT
    // 注入相: 阈值压到 2 ⇒ 提前退出**必然**截断正常收敛星 ⇒ 必须检出不等价
    if (diffs > 0) {
        std::printf("NEGATIVE INJECTION DETECTED: 提前退出阈值注入使 %d 处产物改变"
                    "（判据能红, 非恒真）\n", diffs);
        return 0;
    }
    std::fprintf(stderr, "注入未生效: 阈值注入下产物仍等价 ⇒ 判据无鉴别力\n");
    return 1;
#else
    return cs.failures == 0 ? 0 : 1;
#endif
}

int group_junk() {
    p1psf::CheckState cs;
    const int diffs = run_equiv_group(cs, "junk");
    (void)diffs;
    return cs.failures == 0 ? 0 : 1;
}

const p1psf::TestGroup kGroups[] = {
    {"positive", group_positive},
    {"equiv",    group_equiv},
    {"junk",     group_junk},
};

}  // namespace

int main(int argc, char** argv) {
    return p1psf::run_all_groups(kGroups, sizeof(kGroups) / sizeof(kGroups[0]), argc, argv);
}
