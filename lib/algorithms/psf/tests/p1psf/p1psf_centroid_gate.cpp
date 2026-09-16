// ============================================================================
// p1psf_centroid_gate.cpp - G-P1-CENTROID-1 生产路径绝对位置门
// ----------------------------------------------------------------------------
// 任务: SCI-FIX-PSF 第 2 项 (R-3 §4.1「门应为 X」落地)。
// 缺陷背景 (R-3 §2.9/§3.4 实测): 同一颗星 sdet 报 truth(+0.5 系), dpsf 报
// truth-0.5 (index-is-center 系); 编排写端对 PSF 支路再施加 -0.5 ⇒ PSF 支路
// 较 fallback 支路系统性偏低恰 0.5000 px, 而 ipv 去重阈值是严格 < 0.5 px
// ⇒ 同一颗星双份进入 ipv。修复前无任何门链接 sdet + dpsf (batchH 只测桥算术,
// 前提断言实测为假且未入门禁)。
//
// 本门链接 **真实生产源** (不 mock, 不重写公式):
//   - sdet: lib/algorithms/star_detection/src/sdet_api.cpp (+同目录 TU)
//   - dpsf: lib/algorithms/psf/src/dpsf_psf.cpp (+dpsf_log.cpp)
//   - 生产坐标契约: lib/infrastructure/pipeline/orchestrator/cpp/include/
//     star_coord_contract.h (orchestrator 写端/读端唯一事实源)
//
// 真值域: 解析 Moffat4(beta=4) 合成场, 星心按 (x+0.5) 采样 = 真实像素中心
//   (连续系/truthFITS); 峰值 SNR=50/300, FWHM=3.0 px, B=120 ADU, sigma_n=8 ADU,
//   256x256, 40 星, 固定 seed (与 R-3 探针同构构型)。
//
// 判据 (docs/science/GATES_AND_TOLERANCES.md G-P1-CENTROID-1 行, 阈值来源 R-3):
//   [P0] 匹配星数 >= 25 (防恒真: 0 星也能"通过")
//   [P1] fallback 支路输出与 sdet 原始坐标逐点一致 (写读桥为精确换算)
//   [P2] sdet 支路绝对位置: median|astro_det - truth| <= 0.1 px 且 p95 <= 0.3 px
//   [P3] PSF  支路绝对位置: 同判据 (两条支路分别断言, x/y 两轴均断)
//   [P4] 双支路同系: |median(PSF-truth) - median(fallback-truth)| <= 0.05 px
//        (修复前该量恒 = 0.5000 px)
//   [P5] dpsf 输出系断言: |median(dpsf_cx - truthIndex)| <= 0.05 px
//        (index-is-center; 与 star_coord_contract.h 的恒等映射互为证据)
//
// 负例 (能红能绿, ENGINEERING_SPEC §8):  argv[1] == "negative-injection" 时用
//   修复前的映射 (对 PSF 支路再 -0.5, 即 star_measurement_from_sdet(dpsf_cx))
//   重算, 必须使 [P4] 判据失败 (分离 >= 0.4 px) 且本进程报告
//   "NEGATIVE INJECTION DETECTED" 并 rc=0; 若注入未被检出 (rc=1) 说明门已失效。
//
// 注册: lib/algorithms/psf/tests/p1psf/CMakeLists.txt
//   ctest -R p1psf_centroid_gate          (正例, 必 PASS)
//   ctest -R p1psf_centroid_gate_neg      (负例注入, 必被检出)
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "dynamic_psf.h"
#include "star_coord_contract.h"
#include "star_detector.h"

namespace coord = astrocs::p1::coord;

namespace {

constexpr double kMoffat4FwhmFactor = 1.230310;  // dpsf MOFFAT4_FWHM_FACTOR
constexpr double kGateMedianPx = 0.1;            // G-P1-CENTROID-1 [P2]/[P3]
constexpr double kGateP95Px = 0.3;               // G-P1-CENTROID-1 [P2]/[P3]
constexpr double kGateSeparationPx = 0.05;       // G-P1-CENTROID-1 [P4]
constexpr double kGateDpsfFramePx = 0.05;        // G-P1-CENTROID-1 [P5]
constexpr double kInjectionMinSeparationPx = 0.4;  // 负例注入必须 >= 此分离

int g_checks = 0;
int g_failures = 0;

void check(bool ok, const std::string& msg) {
    ++g_checks;
    if (ok) {
        std::printf("  [PASS] %s\n", msg.c_str());
    } else {
        ++g_failures;
        std::printf("  [FAIL] %s\n", msg.c_str());
    }
}

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return std::nan("");
    std::sort(v.begin(), v.end());
    const double idx = p * static_cast<double>(v.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(idx));
    const size_t hi = static_cast<size_t>(std::ceil(idx));
    return v[lo] + (v[hi] - v[lo]) * (idx - static_cast<double>(lo));
}

double median_abs(const std::vector<double>& v) {
    std::vector<double> a;
    a.reserve(v.size());
    for (double e : v) a.push_back(std::fabs(e));
    return percentile(a, 0.5);
}

double p95_abs(const std::vector<double>& v) {
    std::vector<double> a;
    a.reserve(v.size());
    for (double e : v) a.push_back(std::fabs(e));
    return percentile(a, 0.95);
}

// 解析 Moffat4(beta=4) 场: 星心 (cx,cy) 定义在"像素中心=索引+0.5"系
std::vector<double> synth_field(int w, int h, double bkg, double amp, double s,
                                const std::vector<std::pair<double, double>>& truth,
                                double noise, unsigned seed) {
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> nd(0.0, noise);
    std::vector<double> img(static_cast<size_t>(w) * h, bkg);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double acc = 0.0;
            for (const auto& t : truth) {
                const double dx = (x + 0.5) - t.first;
                const double dy = (y + 0.5) - t.second;
                const double q = 0.5 * (dx * dx + dy * dy) / (s * s);
                acc += amp / std::pow(1.0 + q, 4.0);
            }
            img[static_cast<size_t>(y) * w + x] = bkg + acc + nd(rng);
        }
    }
    return img;
}

// 每轴 (0=x, 1=y) 的偏差样本
struct AxisSamples {
    std::vector<double> sdet;         // fallback 支路 astro_det - truth
    std::vector<double> psf;          // PSF 支路 astro_det - truth (生产映射)
    std::vector<double> psf_legacy;   // PSF 支路 astro_det - truth (修复前映射)
    std::vector<double> dpsf_frame;   // dpsf 原始输出 - truthIndex
};

struct FieldStats {
    int matched = 0;
    int n_detected = 0;
    double fallback_exactness = 0.0;  // max|fallback 输出 - sdet 原始坐标|
    AxisSamples ax[2];
};

// 生产链: sdet 检测 -> dpsf 拟合 -> 写端契约 -> 读端契约
FieldStats run_field(unsigned seed, int n_star, double snr, bool verbose) {
    const int W = 256, H = 256;
    const double B = 120.0, NOISE = 8.0;
    const double S = 3.0 / kMoffat4FwhmFactor;  // FWHM ~ 3 px
    const double A = snr * NOISE;

    std::mt19937_64 rng(seed ^ 0x5deece66dULL);
    std::uniform_real_distribution<double> ud(20.0, static_cast<double>(W - 20));
    std::vector<std::pair<double, double>> truth;
    for (int i = 0; i < n_star; ++i) truth.emplace_back(ud(rng), ud(rng));

    const std::vector<double> img = synth_field(W, H, B, A, S, truth, NOISE, seed);

    FieldStats st;

    // ---- 真实 sdet 检测 (生产 FP64 入口) ----
    StarDetectorHandle h = sdet_create(nullptr);
    if (!h) {
        check(false, "sdet_create 返回 NULL");
        return st;
    }
    double *x = nullptr, *y = nullptr;
    float *fl = nullptr, *mg = nullptr;
    int *sat = nullptr, *hsat = nullptr;
    int cnt = 0;
    const int rc = sdet_detect_ex_f64(h, img.data(), W, H, &x, &y, &fl, &sat, &mg,
                                      &hsat, &cnt, nullptr, 0, nullptr);
    if (rc != 0) {
        check(false, "sdet_detect_ex_f64 rc != 0");
        sdet_destroy(h);
        return st;
    }
    st.n_detected = cnt;

    // ---- 真实 dpsf 拟合 (生产 FP64 批量入口), 初值 = sdet 坐标 ----
    DPSFFitParams p;
    p.fitRadius = 8;
    p.maxIter = 200;
    p.tolerance = 1e-8;

    std::vector<char> truth_used(truth.size(), 0);
    for (int j = 0; j < cnt; ++j) {
        int best = -1;
        double bd = 4.0;  // 4 px 配对半径
        for (size_t i = 0; i < truth.size(); ++i) {
            if (truth_used[i]) continue;
            const double d = std::hypot(x[j] - truth[i].first, y[j] - truth[i].second);
            if (d < bd) { bd = d; best = static_cast<int>(i); }
        }
        if (best < 0) continue;

        const double det[6] = {x[j], y[j], 0.0, 0.0, 0.0, 0.0};
        double out9[9] = {0};
        int n_valid = 0, status = -1;
        const int r2 = dpsf_fit_batch_f64(img.data(), W, H, det, 1, &p, out9, &n_valid, &status);
        if (r2 != 0 || n_valid != 1 || !std::isfinite(out9[2]) || !std::isfinite(out9[3])) continue;

        truth_used[best] = 1;
        ++st.matched;
        const double tx = truth[best].first, ty = truth[best].second;
        const double truth_index[2] = {tx - 0.5, ty - 0.5};
        const double sdet_px[2] = {x[j], y[j]};
        const double dpsf_px[2] = {out9[2], out9[3]};
        const double truth_px[2] = {tx, ty};

        for (int a = 0; a < 2; ++a) {
            // 写端 (star_measurements 统一契约)
            const double sm_fallback = coord::star_measurement_from_sdet(sdet_px[a]);
            const double sm_psf = coord::star_measurement_from_dpsf(dpsf_px[a]);
            const double sm_psf_legacy = coord::star_measurement_from_sdet(dpsf_px[a]);
            // 读端 (ipv detections 接口契约)
            const double astro_fallback = coord::ipv_detection_from_star_measurement(sm_fallback);
            const double astro_psf = coord::ipv_detection_from_star_measurement(sm_psf);
            const double astro_psf_legacy =
                coord::ipv_detection_from_star_measurement(sm_psf_legacy);

            st.fallback_exactness =
                std::max(st.fallback_exactness, std::fabs(astro_fallback - sdet_px[a]));
            st.ax[a].sdet.push_back(astro_fallback - truth_px[a]);
            st.ax[a].psf.push_back(astro_psf - truth_px[a]);
            st.ax[a].psf_legacy.push_back(astro_psf_legacy - truth_px[a]);
            st.ax[a].dpsf_frame.push_back(dpsf_px[a] - truth_index[a]);
        }
    }

    if (verbose) {
        std::printf("  field seed=%u snr=%.0f: detected=%d matched=%d (truth=%d)\n", seed, snr,
                    cnt, st.matched, n_star);
    }

    sdet_free_detect_ex(x, y, fl, sat, mg, hsat, nullptr, 0);
    sdet_destroy(h);
    return st;
}

struct Case {
    unsigned seed;
    int n_star;
    double snr;
};

const char* kAxisName[2] = {"x", "y"};

}  // namespace

int main(int argc, char** argv) {
    const bool negative_injection =
        (argc > 1 && std::strcmp(argv[1], "negative-injection") == 0);

    std::printf("== G-P1-CENTROID-1 生产路径绝对位置门 (sdet + dpsf + 坐标契约)\n");
    std::printf("   模式: %s\n",
                negative_injection ? "NEGATIVE-INJECTION (修复前映射)" : "正向门");

    const Case cases[] = {{42u, 40, 50.0}, {7u, 40, 50.0}, {42u, 40, 300.0}};

    AxisSamples agg[2];
    int total_matched = 0, total_detected = 0, total_truth = 0;
    double fallback_exactness = 0.0;

    for (const Case& c : cases) {
        const FieldStats st = run_field(c.seed, c.n_star, c.snr, true);
        total_matched += st.matched;
        total_detected += st.n_detected;
        total_truth += c.n_star;
        fallback_exactness = std::max(fallback_exactness, st.fallback_exactness);
        for (int a = 0; a < 2; ++a) {
            agg[a].sdet.insert(agg[a].sdet.end(), st.ax[a].sdet.begin(), st.ax[a].sdet.end());
            agg[a].psf.insert(agg[a].psf.end(), st.ax[a].psf.begin(), st.ax[a].psf.end());
            agg[a].psf_legacy.insert(agg[a].psf_legacy.end(), st.ax[a].psf_legacy.begin(),
                                     st.ax[a].psf_legacy.end());
            agg[a].dpsf_frame.insert(agg[a].dpsf_frame.end(), st.ax[a].dpsf_frame.begin(),
                                     st.ax[a].dpsf_frame.end());
        }
    }

    std::printf("   汇总: detected=%d/%d matched=%d/%d\n", total_detected, total_truth,
                total_matched, total_truth);

    double sep_ok[2], sep_legacy[2], dpsf_frame_med[2], sdet_med[2], sdet_p95[2], psf_med[2],
        psf_p95[2];
    for (int a = 0; a < 2; ++a) {
        sdet_med[a] = median_abs(agg[a].sdet);
        sdet_p95[a] = p95_abs(agg[a].sdet);
        psf_med[a] = median_abs(agg[a].psf);
        psf_p95[a] = p95_abs(agg[a].psf);
        sep_ok[a] = percentile(agg[a].psf, 0.5) - percentile(agg[a].sdet, 0.5);
        sep_legacy[a] = percentile(agg[a].psf_legacy, 0.5) - percentile(agg[a].sdet, 0.5);
        dpsf_frame_med[a] = percentile(agg[a].dpsf_frame, 0.5);
        std::printf("   [%s 轴] fallback median|d|=%.4f p95|d|=%.4f | PSF median|d|=%.4f "
                    "p95|d|=%.4f\n",
                    kAxisName[a], sdet_med[a], sdet_p95[a], psf_med[a], psf_p95[a]);
        std::printf("   [%s 轴] 双支路 median 分离: 生产映射=%+.4f px; 修复前映射=%+.4f px | "
                    "dpsf-frame median=%+.4f px\n",
                    kAxisName[a], sep_ok[a], sep_legacy[a], dpsf_frame_med[a]);
    }
    std::printf("   [fallback] 与 sdet 原始坐标最大差 = %.3e px (写读桥精确性)\n",
                fallback_exactness);

    if (negative_injection) {
        // 修复前映射必须被本门判据检出 (否则本门对该缺陷无鉴别力)
        bool detected = true;
        for (int a = 0; a < 2; ++a) {
            detected = detected && std::fabs(sep_legacy[a]) >= kInjectionMinSeparationPx &&
                       std::fabs(sep_legacy[a]) > kGateSeparationPx;
        }
        check(detected, "负例注入 (PSF 支路再 -0.5) 被检出: 两轴分离 >= 0.4 px 且超出 0.05 px 门");
        if (detected) std::printf("NEGATIVE INJECTION DETECTED\n");
        std::printf("== 完成: %d 项检查, %d 项失败 ==\n", g_checks, g_failures);
        return g_failures == 0 ? 0 : 1;
    }

    check(total_matched >= 25, "匹配星数 >= 25 (防 0 星恒真)");
    check(fallback_exactness <= 1e-12, "fallback 支路输出 == sdet 原始坐标 (写读桥精确)");
    for (int a = 0; a < 2; ++a) {
        const std::string ax = kAxisName[a];
        check(sdet_med[a] <= kGateMedianPx, "sdet 支路 " + ax + " 轴 median <= 0.1 px");
        check(sdet_p95[a] <= kGateP95Px, "sdet 支路 " + ax + " 轴 p95 <= 0.3 px");
        check(psf_med[a] <= kGateMedianPx, "PSF 支路 " + ax + " 轴 median <= 0.1 px");
        check(psf_p95[a] <= kGateP95Px, "PSF 支路 " + ax + " 轴 p95 <= 0.3 px");
        check(std::fabs(sep_ok[a]) <= kGateSeparationPx,
              "双支路同系 " + ax + " 轴: |median 分离| <= 0.05 px (修复前 0.5000 px)");
        check(std::fabs(dpsf_frame_med[a]) <= kGateDpsfFramePx,
              "dpsf 原始输出为 index-is-center " + ax + " 轴 (|median| <= 0.05 px)");
    }

    std::printf("== 完成: %d 项检查, %d 项失败 ==\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
