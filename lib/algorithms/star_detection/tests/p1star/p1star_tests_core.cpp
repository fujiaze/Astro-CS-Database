// ============================================================================
// p1star_tests_core.cpp — P1-STAR-TEST core 组 (units/properties/oracle/negative)
// ----------------------------------------------------------------------------
// 合同锚: §11.4 TEST-STAR-DESIGN-001 F1-F6; §2 ALG-STARDET-001。
// 被测面: lib/algorithms/star_detection 生产 sdet_* C API (sdet_detect_ex_f64 主面 +
// sdet_detect_ex u16 量化面); 与 eng/tests/unit/p1_stars_test.cpp (P1-003 桥接层
// astrocs::phase1::StarDetector) 互补不重复。
//
// 负面组 OOM 注入: LD_PRELOAD sdet_oom_interposer.so (第 N 次 malloc 失败),
// 注入点以 fork+exec(/proc/self/exe oom-child) 子进程扫描分类 — 批次 R
// (run/local/bughunt_batchR/REPORT.md §B) 建议的 sdet_detect_ex 输出数组/
// extras 事务化 malloc 负例落地。子进程分类:
//   rc==1 + "OOMCHILD rc=-1"          → 事务化检查点 (断言输出全 NULL)
//   rc==0                              → 非必需分配间隙 (断言 count 正常)
//   rc==134 (std::bad_alloc 终止)     → STL/GSL 内部定义性终止 (登记, 不算
//                                        被测缺陷: 无静默坏数据发布)
// ============================================================================

#include "star_detector.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unistd.h>
#include <vector>

#include <dlfcn.h>
#include <sys/wait.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "p1star_fixtures.hpp"
#include "p1star_oracle.hpp"
#include "p1star_test_main.hpp"

using namespace p1star;

namespace {

// ---- 探测输出聚合 ----------------------------------------------------------

struct Det {
    double* x = nullptr;
    double* y = nullptr;
    float* flux = nullptr;
    float* mag = nullptr;
    int* sat = nullptr;
    int* hsat = nullptr;
    int count = -1;
    void free_all() {
        sdet_free_detect_ex(x, y, flux, sat, mag, hsat, nullptr, 0);
        x = y = nullptr; flux = mag = nullptr; sat = hsat = nullptr; count = -1;
    }
};

int run_f64(StarDetectorHandle h, const std::vector<double>& img, int w, int hh, Det* d) {
    return sdet_detect_ex_f64(h, img.data(), w, hh, &d->x, &d->y, &d->flux, &d->sat,
                              &d->mag, &d->hsat, &d->count, nullptr, 0, nullptr);
}

int run_u16(StarDetectorHandle h, const std::vector<uint16_t>& img, int w, int hh, Det* d) {
    return sdet_detect_ex(h, img.data(), w, hh, &d->x, &d->y, &d->flux, &d->sat,
                          &d->mag, &d->hsat, &d->count, nullptr, 0, nullptr);
}

void serialize(const Det& d, std::vector<char>* blob) {
    blob->clear();
    const std::size_t need = (std::size_t)std::max(d.count, 0) *
                             (sizeof(double) * 2 + sizeof(float) * 2 + sizeof(int) * 2);
    blob->resize(need);
    std::size_t off = 0;
    if (d.count > 0) {
        std::memcpy(blob->data() + off, d.x, (std::size_t)d.count * 8); off += (std::size_t)d.count * 8;
        std::memcpy(blob->data() + off, d.y, (std::size_t)d.count * 8); off += (std::size_t)d.count * 8;
        std::memcpy(blob->data() + off, d.flux, (std::size_t)d.count * 4); off += (std::size_t)d.count * 4;
        std::memcpy(blob->data() + off, d.mag, (std::size_t)d.count * 4); off += (std::size_t)d.count * 4;
        std::memcpy(blob->data() + off, d.sat, (std::size_t)d.count * 4); off += (std::size_t)d.count * 4;
        std::memcpy(blob->data() + off, d.hsat, (std::size_t)d.count * 4);
    }
}

// ctest 负面注册要求的单线程 malloc 序确定性 (SDT-NEG-2 前置)
int read_omp_env_threads() {
    const char* t = std::getenv("OMP_NUM_THREADS");
    if (!t || !*t) return 0;
    return std::atoi(t);
}

// ---- F1-TB 过渡带判据 (docs/algorithms/STAR_DETECTION_ALGORITHMS.md §11.4 F1) --
// 分区逐星按逐档实测 99% 召回阈表 (p1star_fixtures.hpp F1TB_THR99):
//   POS   SNR_peak ≥ thr99(σ)         判据声明在域内 ⇒ 必须召回 ≥99%
//   TB    10 ≤ SNR_peak < thr99(σ)    过渡带负例 ⇒ 必须存在且必须判红
//   BELOW SNR_peak < 10               旧冻结声明的域外
// 判据 = 四个合取子句; 变体只改判据输入侧 (真值表或检出表), 单变量隔离。

struct F1TbField {
    const std::vector<SynthStar>* truth = nullptr;
    double noise = 1.0;
    const double* dx = nullptr;
    const double* dy = nullptr;
    int count = 0;
};

enum class F1TbVariant {
    Honest,               // 诚实场 + 诚实检出
    PosAbsent,            // 过渡带真星全部缺失 (真值表侧)
    PosUndetected,        // 过渡带真星全部未检出 (检出表侧)
    TransitionBandAbsent  // 过渡带负例全部缺失 (真值表侧)
};

struct F1TbTally {
    int n_pos = 0, n_pos_det = 0;
    int n_tb = 0, n_tb_det = 0;
    int n_below = 0, n_below_det = 0;
    int n_ge10 = 0, n_ge20 = 0;
    int band_hit = 0;
    bool band_seen[6] = {false, false, false, false, false, false};
    int pos_band_hit = 0;
    bool pos_band_seen[6] = {false, false, false, false, false, false};
    // σ_psf=1.0 px 档按区间 [LO,HI] 标定: 即使按最保守读法 (只用区间上端 HI),
    // 该档也必须有真星且必须全部检出 —— 区间不得掩盖「该档无真星」的退化
    int n_s10_hi = 0, n_s10_hi_det = 0;

    // C1: 召回场必须含过渡带真星, 且过渡带负例须覆盖 ≥4 个 σ 档
    //     (σ_psf=3.0 px 档阈 10.0 与域下限重合, 其过渡带结构性为空)
    bool c1_band_present() const { return n_tb >= 8 && band_hit >= 4; }
    // C2: 判据声明在域内的真星召回 ≥99%
    bool c2_in_domain_recall() const {
        return n_pos > 0 && (double)n_pos_det >= 0.99 * (double)n_pos;
    }
    // C3: 过渡带负例必须存在且必须判红 (度量非恒真)
    bool c3_negative_red() const {
        return n_tb > 0 && (double)n_tb_det < 0.99 * (double)n_tb;
    }
    // C4: §11.4 非退化指标 — snr10 档真星数必须与 snr20 档不同
    bool c4_doc_indicator() const { return n_ge10 != n_ge20; }
    // C5: 每一档都必须有域内真星, 否则该档的域内召回不被行使
    bool c5_pos_all_bands() const { return pos_band_hit >= 6; }
    // C6: 区间标定档 (σ_psf=1.0 px) 在区间**上端**读法下也必须被行使且全检出
    bool c6_interval_band_exercised() const {
        return !F1TB_THR99_IS_INTERVAL[0] || (n_s10_hi >= 2 && n_s10_hi_det == n_s10_hi);
    }
    bool ok() const {
        return c1_band_present() && c2_in_domain_recall() && c3_negative_red() &&
               c4_doc_indicator() && c5_pos_all_bands() && c6_interval_band_exercised();
    }
};

F1TbTally f1tb_tally(const std::vector<F1TbField>& fields, F1TbVariant v) {
    F1TbTally t;
    for (const auto& f : fields) {
        const auto m = oracle::match_nearest(*f.truth, f.dx, f.dy, f.count);
        for (std::size_t i = 0; i < f.truth->size(); ++i) {
            const SynthStar& s = (*f.truth)[i];
            const double snr = s.amp / f.noise;
            if (snr >= 10.0) t.n_ge10++;
            if (snr >= 20.0) t.n_ge20++;
            const double thr = f1tb_thr99(s.sigma);
            const int det = (m[i] >= 0) ? 1 : 0;
            if (snr >= thr) {
                if (v == F1TbVariant::PosAbsent) continue;
                t.n_pos++;
                if (v != F1TbVariant::PosUndetected) t.n_pos_det += det;
                for (int b = 0; b < 6; ++b)
                    if (std::fabs(s.sigma - F1TB_SIGMA_BANDS[b]) < 1e-9 && !t.pos_band_seen[b]) {
                        t.pos_band_seen[b] = true;
                        t.pos_band_hit++;
                    }
                // 区间标定档: 按区间上端 (保守读法) 单独计数
                if (std::fabs(s.sigma - F1TB_SIGMA_BANDS[0]) < 1e-9 &&
                    snr >= F1TB_THR99_HI[0]) {
                    t.n_s10_hi++;
                    if (v != F1TbVariant::PosUndetected) t.n_s10_hi_det += det;
                }
            } else if (snr >= F1TB_SNR_FLOOR) {
                if (v == F1TbVariant::TransitionBandAbsent) continue;
                t.n_tb++;
                t.n_tb_det += det;
                for (int b = 0; b < 6; ++b)
                    if (std::fabs(s.sigma - F1TB_SIGMA_BANDS[b]) < 1e-9 && !t.band_seen[b]) {
                        t.band_seen[b] = true;
                        t.band_hit++;
                    }
            } else {
                t.n_below++;
                t.n_below_det += det;
            }
        }
    }
    return t;
}

}  // namespace

// ============================================================================
// units 组 — F1 召回/虚警/精度 + F2 饱和/混合 + F5 合同负例
// ============================================================================
// 组函数置于 namespace p1star (外部链接): p1star_selfcheck_test 共享本 TU,
// baseline 面直接复用 test_units/test_oracle (结构先例 p1hips_tests_selfcheck)。
namespace p1star {

int test_units() {
    CheckState cs;

    // ---- FIX-STAR-A (F1): 含噪场召回/虚警/质心/FWHM/振幅 ------------------
    {
        const FixStarA fx = fix_star_a_f1();
        StarDetectorHandle h = sdet_create(nullptr);
        Det d;
        const int rc = run_f64(h, fx.img, fx.w, fx.h, &d);
        P1STAR_CHECK_EQ(cs, rc, 0, "f1_detect_rc");
        P1STAR_CHECK(cs, d.x != nullptr && d.count > 0, "f1_outputs_nonnull");

        // SNR 分谱: snr = amp/noise (峰值像素近似振幅)
        int snr20 = 0, snr10 = 0;
        for (const auto& s : fx.truth) {
            const double snr = s.amp / fx.noise;
            if (snr >= 20.0) snr20++;
            if (snr >= 10.0) snr10++;
        }
        const auto matched = oracle::match_nearest(fx.truth, d.x, d.y, d.count);
        int rec20 = 0, rec10 = 0;
        double sum_dc = 0.0, max_dc = 0.0;
        for (std::size_t i = 0; i < fx.truth.size(); ++i) {
            if (matched[i] < 0) continue;
            const double snr = fx.truth[i].amp / fx.noise;
            const double dc = std::hypot(d.x[matched[i]] - fx.truth[i].cx,
                                         d.y[matched[i]] - fx.truth[i].cy);
            sum_dc += dc;
            if (dc > max_dc) max_dc = dc;
            if (snr >= 20.0) { rec20++; P1STAR_CHECK(cs, dc <= 0.3, "f1_centroid_0p3px_snr20"); }
            if (snr >= 10.0) rec10++;
        }
        const double recall20 = (double)rec20 / std::max(1, snr20);
        const double recall10 = (double)rec10 / std::max(1, snr10);
        int falsepos = 0;
        for (int j = 0; j < d.count; ++j) {
            bool used = false;
            for (std::size_t i = 0; i < matched.size(); ++i) if (matched[i] == j) used = true;
            if (!used) falsepos++;
        }
        const double fp_per_kpx = 1000.0 * falsepos / ((double)fx.w * fx.h);

        std::printf("[f1] truth=%zu snr20=%d/%d snr10=%d/%d count=%d fp=%d (%.3f/kpx) mean_dc=%.4f max_dc=%.4f\n",
                    fx.truth.size(), rec20, snr20, rec10, snr10, d.count, falsepos, fp_per_kpx,
                    sum_dc / std::max(1, rec10), max_dc);
        P1STAR_CHECK(cs, recall20 >= 0.99, "f1_recall_snr20_ge99");
        // 「SNR≥10 召回≥99%」平坦门由 §11.4 F1 分档判据取代: 该门在其声明域内不成立
        // (σ_psf=1.0 档 99% 阈实测 46.0), 且本场 snr10==snr20==32 使其恒绿。
        // recall10 保留为实测打印值, 断言移至下方 F1-TB 分档块。
        P1STAR_CHECK(cs, fp_per_kpx <= 0.1, "f1_falsepos_le0p1_per_kpx");

        // extras 复取: fwhm_x / amplitude (F1 FWHM≤10% 相对误差)
        const char* names[2] = {"fwhm_x", "amplitude"};
        double *x2, *y2; float *fl2, *mg2; int *sa2, *hs2; int c2; float** extras = nullptr;
        const int rc2 = sdet_detect_ex_f64(h, fx.img.data(), fx.w, fx.h, &x2, &y2, &fl2,
                                           &sa2, &mg2, &hs2, &c2, names, 2, &extras);
        P1STAR_CHECK_EQ(cs, rc2, 0, "f1_extras_rc");
        P1STAR_CHECK(cs, extras != nullptr && extras[0] != nullptr && extras[1] != nullptr,
                     "f1_extras_rows_nonnull");
        if (extras && extras[0] && extras[1]) {
            const auto m2 = oracle::match_nearest(fx.truth, x2, y2, c2);
            int n_ok = 0; double s_fw = 0.0, s_amp = 0.0;
            for (std::size_t i = 0; i < fx.truth.size(); ++i) {
                if (m2[i] < 0) continue;
                if (fx.truth[i].amp / fx.noise < 20.0) continue;  // FWHM 锚限 SNR≥20
                const double fw_true = oracle::gaussian_fwhm(fx.truth[i].sigma);
                s_fw += std::fabs((double)extras[0][m2[i]] - fw_true) / fw_true;
                s_amp += std::fabs((double)extras[1][m2[i]] - fx.truth[i].amp) / fx.truth[i].amp;
                n_ok++;
            }
            std::printf("[f1] extras n=%d mean_fwhm_rel=%.4f mean_amp_rel=%.4f\n",
                        n_ok, s_fw / std::max(1, n_ok), s_amp / std::max(1, n_ok));
            P1STAR_CHECK(cs, n_ok >= 15, "f1_extras_matches");
            P1STAR_CHECK(cs, s_fw / std::max(1, n_ok) <= 0.10, "f1_fwhm_rel_le10pct");
        }
        sdet_free_detect_ex(x2, y2, fl2, sa2, mg2, hs2, extras, 2);
        sdet_destroy(h);
        d.free_all();
    }

    // ---- FIX-STAR-H (F1-TB): 过渡带真星 + 过渡带负例 + 判据判别力自检 -------
    // 合同锚: §11.4 F1「判据式 + 判据非退化要求」。补夹具前 F1 召回场只有
    // FIX-STAR-A, 其真星 SNR 空档 [10,32.5) ⇒ 分档判据的过渡带部分行使不到。
    {
        const FixStarA fa = fix_star_a_f1();
        StarDetectorHandle ha = sdet_create(nullptr);
        Det da;
        const int rca = run_f64(ha, fa.img, fa.w, fa.h, &da);
        P1STAR_CHECK_EQ(cs, rca, 0, "f1tb_a_rc");

        const FixStarH fh = fix_star_h_f1_transition_band();
        StarDetectorHandle hh = sdet_create(nullptr);
        Det dh;
        const int rch = run_f64(hh, fh.img, fh.w, fh.h, &dh);
        P1STAR_CHECK_EQ(cs, rch, 0, "f1tb_h_rc");

        const auto mh = oracle::match_nearest(fh.truth, dh.x, dh.y, dh.count);
        std::printf("[f1tb] H field %dx%d truth=%zu count=%d\n", fh.w, fh.h,
                    fh.truth.size(), dh.count);
        // 逐档实测 (判据域内 POS / 过渡带 TB)
        for (int b = 0; b < 6; ++b) {
            int np = 0, npd = 0, nt = 0, ntd = 0;
            for (std::size_t i = 0; i < fh.truth.size(); ++i) {
                if (std::fabs(fh.truth[i].sigma - F1TB_SIGMA_BANDS[b]) > 1e-9) continue;
                const double snr = fh.truth[i].amp / fh.noise;
                const int det = (mh[i] >= 0) ? 1 : 0;
                if (snr >= F1TB_THR99[b]) { np++; npd += det; }
                else if (snr >= F1TB_SNR_FLOOR) { nt++; ntd += det; }
            }
            if (F1TB_THR99_IS_INTERVAL[b])
                std::printf("[f1tb] sigma=%.2f thr99=[%.1f, %.1f] (区间标定, 引用须同报区间) "
                            "| POS %d/%d (%.4f) | TB %d/%d (%.4f)\n",
                            F1TB_SIGMA_BANDS[b], F1TB_THR99[b], F1TB_THR99_HI[b], npd, np,
                            np ? (double)npd / np : 0.0, ntd, nt, nt ? (double)ntd / nt : 0.0);
            else
                std::printf("[f1tb] sigma=%.2f thr99=%.1f | POS %d/%d (%.4f) | TB %d/%d (%.4f)\n",
                            F1TB_SIGMA_BANDS[b], F1TB_THR99[b], npd, np,
                            np ? (double)npd / np : 0.0, ntd, nt, nt ? (double)ntd / nt : 0.0);
        }

        const std::vector<F1TbField> pre = {{&fa.truth, fa.noise, da.x, da.y, da.count}};
        const std::vector<F1TbField> post = {{&fa.truth, fa.noise, da.x, da.y, da.count},
                                             {&fh.truth, fh.noise, dh.x, dh.y, dh.count}};
        const F1TbTally t_pre = f1tb_tally(pre, F1TbVariant::Honest);
        const F1TbTally t_post = f1tb_tally(post, F1TbVariant::Honest);
        const F1TbTally t_pos_absent = f1tb_tally(post, F1TbVariant::PosAbsent);
        const F1TbTally t_pos_undet = f1tb_tally(post, F1TbVariant::PosUndetected);
        const F1TbTally t_tb_absent = f1tb_tally(post, F1TbVariant::TransitionBandAbsent);

        const F1TbTally* tally[5] = {&t_pre, &t_post, &t_pos_absent, &t_pos_undet, &t_tb_absent};
        const char* vname[5] = {"pre_gap(A only)", "post_gap(honest)", "pos_absent",
                                "pos_undetected", "tb_absent"};
        for (int k = 0; k < 5; ++k) {
            const F1TbTally& t = *tally[k];
            std::printf("[f1tb] %-16s pos=%d/%d tb=%d/%d below=%d/%d ge10=%d ge20=%d "
                        "tbBands=%d posBands=%d s10hi=%d/%d | C1=%d C2=%d C3=%d C4=%d "
                        "C5=%d C6=%d => %s\n",
                        vname[k], t.n_pos_det, t.n_pos, t.n_tb_det, t.n_tb,
                        t.n_below_det, t.n_below, t.n_ge10, t.n_ge20, t.band_hit,
                        t.pos_band_hit, t.n_s10_hi_det, t.n_s10_hi,
                        (int)t.c1_band_present(), (int)t.c2_in_domain_recall(),
                        (int)t.c3_negative_red(), (int)t.c4_doc_indicator(),
                        (int)t.c5_pos_all_bands(), (int)t.c6_interval_band_exercised(),
                        t.ok() ? "GREEN" : "RED");
        }

        // 补夹具后的诚实场: 四个子句全绿
        P1STAR_CHECK(cs, t_post.c1_band_present(), "f1tb_band_present");
        P1STAR_CHECK(cs, t_post.c2_in_domain_recall(), "f1tb_in_domain_recall_ge99");
        P1STAR_CHECK(cs, t_post.c3_negative_red(), "f1tb_negative_red");
        P1STAR_CHECK(cs, t_post.c4_doc_indicator(), "f1tb_doc_indicator_snr10_ne_snr20");
        P1STAR_CHECK(cs, t_post.c5_pos_all_bands(), "f1tb_every_sigma_band_has_in_domain");
        P1STAR_CHECK(cs, t_post.c6_interval_band_exercised(),
                     "f1tb_interval_band_exercised_at_upper_end");

        // 判别力自检 (能红能绿, AGENTS.md §5): 变体结论必须与诚实场不同
        P1STAR_CHECK(cs, !t_pre.ok(), "f1tb_pregap_field_red");
        P1STAR_CHECK(cs, !t_pos_absent.ok(), "f1tb_pos_absent_red");
        P1STAR_CHECK(cs, !t_pos_undet.ok(), "f1tb_pos_undetected_red");
        P1STAR_CHECK(cs, !t_tb_absent.ok(), "f1tb_tb_absent_red");
        P1STAR_CHECK(cs, t_post.ok() && !t_pos_absent.ok() && !t_pre.ok() &&
                         !t_pos_undet.ok() && !t_tb_absent.ok(),
                     "f1tb_verdicts_differ");

        sdet_destroy(ha);
        sdet_destroy(hh);
        da.free_all();
        dh.free_all();
    }

    // ---- FIX-STAR-G (F1): 纯噪声空场虚警 ---------------------------------
    {
        const FixStarG fx = fix_star_g_noise_only();
        StarDetectorHandle h = sdet_create(nullptr);
        Det d;
        const int rc = run_f64(h, fx.img, fx.w, fx.h, &d);
        P1STAR_CHECK_EQ(cs, rc, 0, "g_noise_rc");
        const double fp_per_kpx = 1000.0 * std::max(d.count, 0) / ((double)fx.w * fx.h);
        std::printf("[f1g] noise-only count=%d fp/kpx=%.3f\n", d.count, fp_per_kpx);
        P1STAR_CHECK(cs, fp_per_kpx <= 0.1, "g_noise_falsepos_le0p1_per_kpx");
        sdet_destroy(h);
        d.free_all();
    }

    // ---- FIX-STAR-B (F2): u16 饱和平台星 ---------------------------------
    {
        const FixStarB fx = fix_star_b_f2_saturated();
        StarDetectorHandle h = sdet_create(nullptr);
        Det d;
        const int rc = run_u16(h, fx.img, fx.w, fx.h, &d);
        P1STAR_CHECK_EQ(cs, rc, 0, "f2_satplat_rc");
        std::printf("[f2b] nclip=%d count=%d\n", fx.nclip, d.count);
        P1STAR_CHECK(cs, fx.nclip >= 3, "f2_platform_ge3px_constructed");
        int found = 0;
        for (int i = 0; i < d.count; ++i)
            if (std::hypot(d.x[i] - 48.5, d.y[i] - 47.5) <= 1.5) {
                found++;
                P1STAR_CHECK_EQ(cs, d.sat[i], 1, "f2_saturated_flag_eq1");
                P1STAR_CHECK(cs, std::fabs((double)d.flux[i] - 90000.0) / 90000.0 <= 0.01,
                             "f2_saturated_flux");
            }
        P1STAR_CHECK_EQ(cs, found, 1, "f2_satplat_detected");
        sdet_destroy(h);
        d.free_all();
    }

    // ---- FIX-STAR-C (F2): 饱和混合对 (d<2px 保饱和星) --------------------
    {
        const FixStarC fx = fix_star_c_f2_blend();
        StarDetectorHandle h = sdet_create(nullptr);
        Det d;
        const int rc = run_u16(h, fx.img, fx.w, fx.h, &d);
        P1STAR_CHECK_EQ(cs, rc, 0, "f2_blend_rc");
        int near_bright = 0, bright_sat = -1;
        for (int i = 0; i < d.count; ++i) {
            const double dc = std::hypot(d.x[i] - 40.0, d.y[i] - 40.0);
            if (dc <= 2.0) { near_bright++; if (d.sat[i] == 1) bright_sat = i; }
        }
        std::printf("[f2c] count=%d near_bright=%d bright_sat_idx=%d\n", d.count, near_bright, bright_sat);
        P1STAR_CHECK(cs, near_bright >= 1, "f2_blend_star_present");
        P1STAR_CHECK(cs, bright_sat >= 0, "f2_blend_saturated_kept");
        sdet_destroy(h);
        d.free_all();
    }

    // ---- F5 合同负例 (入口校验先行) ---------------------------------------
    {
        StarDetectorHandle h = sdet_create(nullptr);
        P1STAR_CHECK(cs, h != nullptr, "f5_create_ok");
        std::vector<double> img((std::size_t)16 * 16, 300.0);
        double* x; double* y; float* fl; int* sa; float* mg; int* hs; int cnt;

        int rc = sdet_detect_ex_f64(nullptr, img.data(), 16, 16, &x, &y, &fl, &sa, &mg, &hs, &cnt,
                                    nullptr, 0, nullptr);
        P1STAR_CHECK_EQ(cs, rc, -1, "f5_null_handle_rc_m1");

        rc = sdet_detect_ex_f64(h, nullptr, 16, 16, &x, &y, &fl, &sa, &mg, &hs, &cnt,
                                nullptr, 0, nullptr);
        P1STAR_CHECK_EQ(cs, rc, -1, "f5_null_image_rc_m1");

        rc = sdet_detect_ex_f64(h, img.data(), 0, 16, &x, &y, &fl, &sa, &mg, &hs, &cnt,
                                nullptr, 0, nullptr);
        P1STAR_CHECK_EQ(cs, rc, -1, "f5_width0_rc_m1");

        rc = sdet_detect_ex_f64(h, img.data(), 16, 0, &x, &y, &fl, &sa, &mg, &hs, &cnt,
                                nullptr, 0, nullptr);
        P1STAR_CHECK_EQ(cs, rc, -1, "f5_height0_rc_m1");

        rc = sdet_detect_ex_f64(h, img.data(), 16, 16, nullptr, &y, &fl, &sa, &mg, &hs, &cnt,
                                nullptr, 0, nullptr);
        P1STAR_CHECK_EQ(cs, rc, -1, "f5_null_out_x_rc_m1");

        rc = sdet_detect_ex_f64(h, img.data(), 16, 16, &x, &y, &fl, &sa, &mg, &hs, nullptr,
                                nullptr, 0, nullptr);
        P1STAR_CHECK_EQ(cs, rc, -1, "f5_null_count_rc_m1");

        // params=NULL → 默认参数 (合同; 等价 sdet_create(nullptr) 默认)
        StarDetectorHandle h2 = sdet_create(nullptr);
        P1STAR_CHECK_EQ(cs, (h2 != nullptr), 1, "f5_default_params_create_ok");
        sdet_destroy(h2);
        sdet_destroy(h);
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1star (units)

// ============================================================================
// properties 组 — F3 确定性: OMP 1/2/4 bitwise + 排序全序 + maxStars + u16/F4
// ============================================================================
namespace p1star {

int test_properties() {
    CheckState cs;
    const FixStarD fx = fix_star_d_f3();

    // ---- OMP 1/2/4 线程 bitwise (F3) --------------------------------------
    {
        std::vector<char> blob[3];
        int counts[3] = {0, 0, 0};
        for (int t = 0; t < 3; ++t) {
#ifdef _OPENMP
            omp_set_num_threads(1 << t);
#endif
            StarDetectorHandle h = sdet_create(nullptr);
            Det d;
            const int rc = run_f64(h, fx.img, fx.w, fx.h, &d);
            P1STAR_CHECK_EQ(cs, rc, 0, "f3_thread_run_rc");
            counts[t] = d.count;
            serialize(d, &blob[t]);
            sdet_destroy(h);
            d.free_all();
        }
        std::printf("[f3] counts=%d,%d,%d\n", counts[0], counts[1], counts[2]);
        P1STAR_CHECK_EQ(cs, counts[0], counts[1], "f3_count_1v2");
        P1STAR_CHECK_EQ(cs, counts[0], counts[2], "f3_count_1v4");
        P1STAR_CHECK(cs, blob[0] == blob[1], "f3_bitwise_1v2");
        P1STAR_CHECK(cs, blob[0] == blob[2], "f3_bitwise_1v4");
    }

    // ---- mag 升序全序 + NaN 恒末尾 (F3, FIX-STAR-D + NaN 注入场) ---------
    {
        StarDetectorHandle h = sdet_create(nullptr);
        Det d;
        run_f64(h, fx.img, fx.w, fx.h, &d);
        bool sorted_ok = true;
        for (int i = 1; i < d.count; ++i)
            if (oracle::mag_less(d.mag[i], d.mag[i - 1])) sorted_ok = false;
        P1STAR_CHECK(cs, sorted_ok, "f3_mag_ascending_total_order");
        // NaN 末尾: 前缀非 NaN (构造场无无效 flux, 断言全体有限 + 升序)
        bool finite_prefix = true;
        for (int i = 0; i < d.count; ++i)
            if (std::isnan((double)d.mag[i])) finite_prefix = false;
        P1STAR_CHECK(cs, finite_prefix, "f3_mag_all_finite_clean_field");

        // NaN 注入场: 无效像素区造成拟合失败 → mag NaN 必须排末尾
        std::vector<double> img = fx.img;
        const int w = fx.w, hh = fx.h;
        for (int y = 0; y < hh; ++y)
            for (int xx = 0; xx < 4; ++xx)
                img[(std::size_t)y * w + xx] = std::numeric_limits<double>::quiet_NaN();
        Det dn;
        const int rcn = run_f64(h, img, w, hh, &dn);
        P1STAR_CHECK_EQ(cs, rcn, 0, "f3_nan_field_rc");
        bool nan_tail = true, prefix_sorted = true;
        int n_nan = 0;
        for (int i = 0; i < dn.count; ++i) {
            if (std::isnan((double)dn.mag[i])) { n_nan++; continue; }
            if (n_nan > 0) nan_tail = false;  // 有限值出现在 NaN 之后
        }
        for (int i = 1; i < dn.count; ++i)
            if (oracle::mag_less(dn.mag[i], dn.mag[i - 1])) prefix_sorted = false;
        std::printf("[f3] nan-field count=%d n_nan_mag=%d tail_ok=%d\n", dn.count, n_nan, (int)nan_tail);
        P1STAR_CHECK(cs, nan_tail, "f3_nan_mag_tail");
        P1STAR_CHECK(cs, prefix_sorted, "f3_nan_field_order");
        sdet_destroy(h);
        d.free_all();
        dn.free_all();
    }

    // ---- maxStars 截断保最亮 (F3): 25 星场 maxStars=10 --------------------
    {
        SDetParams p;
        p.structureLayers = 5; p.hotPixelFilterRadius = 1; p.iterativeClipSigma = 9.0f;
        p.iterativeMaxRounds = 5; p.medianFilterDetail = 1;
        p.maxStars = 10; p.fitRadius = 6; p.fwhmClipSigma = 3.0f; p.maxAxisRatio = 2.0f;
        StarDetectorHandle h = sdet_create(&p);
        P1STAR_CHECK(cs, h != nullptr, "f3_maxstars_create");
        Det d;
        run_f64(h, fx.img, fx.w, fx.h, &d);
        std::printf("[f3] maxStars=10 count=%d\n", d.count);
        P1STAR_CHECK_EQ(cs, d.count, 10, "f3_maxstars_truncated");
        // 全场无截断跑一遍, 取 mag 升序前 10 对比 bitwise (保最亮)
        StarDetectorHandle hfull = sdet_create(nullptr);
        Det dfull;
        run_f64(hfull, fx.img, fx.w, fx.h, &dfull);
        P1STAR_CHECK(cs, dfull.count >= 10, "f3_maxstars_full_enough");
        bool keep_brightest = true;
        for (int i = 0; i < 10; ++i) {
            const bool same_xy = std::fabs(d.x[i] - dfull.x[i]) == 0.0 &&
                                 std::fabs(d.y[i] - dfull.y[i]) == 0.0;
            if (!same_xy) keep_brightest = false;
        }
        P1STAR_CHECK(cs, keep_brightest, "f3_maxstars_keep_brightest");
        sdet_destroy(hfull);
        sdet_destroy(h);
        d.free_all();
        dfull.free_all();
    }

    // ---- u16 量化面 |Δc|≤0.5px (F4 FP32 通道) -----------------------------
    {
        StarDetectorHandle h = sdet_create(nullptr);
        std::vector<double> img((std::size_t)64 * 64, 300.0);
        add_gaussian_to(img, 64, 64, 30.37, 20.61, 2000.0, 2.0);
        const std::vector<uint16_t> u = quantize_u16(img);
        Det df, du;
        run_f64(h, img, 64, 64, &df);
        run_u16(h, u, 64, 64, &du);
        P1STAR_CHECK_EQ(cs, df.count, du.count, "f4_u16_count_match");
        if (df.count == du.count && df.count == 1) {
            const double dc = std::hypot(df.x[0] - du.x[0], df.y[0] - du.y[0]);
            std::printf("[f4] u16 dc=%.4f\n", dc);
            P1STAR_CHECK(cs, dc <= 0.5, "f4_u16_centroid_le0p5px");
        }
        sdet_destroy(h);
        df.free_all();
        du.free_all();
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1star (properties)

// ============================================================================
// oracle 组 — F4 独立 oracle (构造参数即期望, 独立复算) + FIX-STAR-F 回归锚
// ============================================================================
namespace p1star {

int test_oracle() {
    CheckState cs;
    const FixStarE fx = fix_star_e_f4();
    StarDetectorHandle h = sdet_create(nullptr);
    const char* names[2] = {"amplitude", "background"};
    double *x2, *y2; float *fl2, *mg2; int *sa2, *hs2; int c2; float** extras = nullptr;
    const int rc = sdet_detect_ex_f64(h, fx.img.data(), fx.w, fx.h, &x2, &y2, &fl2,
                                      &sa2, &mg2, &hs2, &c2, names, 2, &extras);
    P1STAR_CHECK_EQ(cs, rc, 0, "f4_oracle_rc");
    P1STAR_CHECK(cs, extras != nullptr && extras[0] != nullptr && extras[1] != nullptr,
                 "f4_oracle_extras_nonnull");

    if (rc == 0 && extras && extras[0] && extras[1]) {
        const auto matched = oracle::match_nearest(fx.truth, x2, y2, c2, 1.0);
        int found = 0;
        double max_dc = 0.0, max_amp_rel = 0.0, max_bg_abs = 0.0;
        for (std::size_t i = 0; i < fx.truth.size(); ++i) {
            if (matched[i] < 0) continue;
            found++;
            const int j = matched[i];
            const double dc = std::hypot(x2[j] - fx.truth[i].cx, y2[j] - fx.truth[i].cy);
            max_dc = std::max(max_dc, dc);
            max_amp_rel = std::max(max_amp_rel,
                                   std::fabs((double)extras[0][j] - fx.truth[i].amp) / fx.truth[i].amp);
            max_bg_abs = std::max(max_bg_abs, std::fabs((double)extras[1][j] - fx.bg));
            // F4 容差 (§11.4, 不得放宽): |Δ中心|≤0.05px, A/B 相对误差≤1e-3
            P1STAR_CHECK(cs, dc <= 0.05, "f4_centroid_le0p05px");
            P1STAR_CHECK(cs, std::fabs((double)extras[0][j] - fx.truth[i].amp) / fx.truth[i].amp <= 1e-3,
                         "f4_amplitude_rel_le1e-3");
            P1STAR_CHECK(cs, std::fabs((double)extras[1][j] - fx.bg) / fx.bg <= 1e-3,
                         "f4_background_rel_le1e-3");
        }
        std::printf("[f4] oracle found=%d/20 max_dc=%.2e max_amp_rel=%.2e max_bg_abs=%.2e\n",
                    found, max_dc, max_amp_rel, max_bg_abs);
        P1STAR_CHECK_EQ(cs, found, 20, "f4_oracle_all20");

        // flux/mag oracle 交叉验证。口径冻结 (lib/algorithms/star_detection/README.md §5,
        // P1-STAR-DOC): 正常星 flux=Moffat4 振幅 A (FIX-STAR-B 标定 flux=89999.8
        // 佐证), mag=−2.5·log10(Σ_box(pixel−B_fit))。期望由构造参数独立复算:
        //   flux 期望 = amp (构造即期望, F4 A 容差 1e-3 同源)
        //   mag  期望 = −2.5·log10(amp·M_box(R)) (M_box=erf(R/σ√2)² 盒内质量占比,
        //        解析; 离散和 vs 连续积分经验界 0.05 mag — 弱哨兵, 非冻结容差)
        {
            const auto matched2 = oracle::match_nearest(fx.truth, x2, y2, c2, 1.0);
            int n_ok = 0; double s_rel = 0.0, max_mag_abs = 0.0;
            for (std::size_t i = 0; i < fx.truth.size(); ++i) {
                if (matched2[i] < 0) continue;
                const int j = matched2[i];
                const double rel = std::fabs((double)fl2[j] - fx.truth[i].amp) / fx.truth[i].amp;
                s_rel += rel; n_ok++;
                const double m_exp = oracle::magnitude(
                    fx.truth[i].amp * oracle::gaussian_flux_in_box(1.0, fx.truth[i].sigma, 6.0));
                const double mag_abs = std::fabs((double)mg2[j] - m_exp);
                if (mag_abs > max_mag_abs) max_mag_abs = mag_abs;
            }
            std::printf("[f4] flux-vs-amp mean_rel=%.2e (n=%d) max_mag_abs=%.4f\n",
                        s_rel / std::max(1, n_ok), n_ok, max_mag_abs);
            P1STAR_CHECK(cs, s_rel / std::max(1, n_ok) <= 1e-3, "f4_flux_equals_amplitude");
            // mag 哨兵 (弱, 非 §11.4 冻结容差): 生产 mag=−2.5log10(Σ_box(pixel−B_fit))
            // 受 B_fit 拟合残差 (ΔB·N_box) 主导, 解析期望不可精确预期; 冻结面为
            // 中心/A/B (上), 此处只断言无固定 0.8+ mag 系统偏移的病态回归
            P1STAR_CHECK(cs, max_mag_abs <= 1.5, "f4_mag_box_integral_weak");
        }
    }
    sdet_free_detect_ex(x2, y2, fl2, sa2, mg2, hs2, extras, 2);
    sdet_destroy(h);

    // ---- FIX-STAR-F (F6) 回归锚: 饱和平台核 + 3 伴星 ----------------------
    {
        const FixStarF f6 = fix_star_f_f6_anchor();
        StarDetectorHandle h6 = sdet_create(nullptr);
        Det d6;
        const int rc6 = run_u16(h6, f6.img, f6.w, f6.h, &d6);
        P1STAR_CHECK_EQ(cs, rc6, 0, "f6_anchor_rc");
        int core = -1;
        for (int i = 0; i < d6.count; ++i)
            if (std::hypot(d6.x[i] - 64.3, d6.y[i] - 63.7) <= 1.5) core = i;
        // 伴星锚 (构造位置已知)
        int n_companion = 0;
        const double refs[3][2] = {{30.2, 40.9}, {95.6, 88.4}, {40.1, 100.3}};
        for (int r = 0; r < 3; ++r) {
            for (int i = 0; i < d6.count; ++i)
                if (std::hypot(d6.x[i] - refs[r][0], d6.y[i] - refs[r][1]) <= 1.5) { n_companion++; break; }
        }
        std::printf("[f6] count=%d core_idx=%d sat=%d companions=%d\n",
                    d6.count, core, core >= 0 ? d6.sat[core] : -1, n_companion);
        P1STAR_CHECK(cs, core >= 0, "f6_core_detected");
        if (core >= 0) P1STAR_CHECK_EQ(cs, d6.sat[core], 1, "f6_core_saturated");
        P1STAR_CHECK_EQ(cs, n_companion, 3, "f6_companions_3");
        // 排序全序复核 (锚场亦须全序)
        bool sorted_ok = true;
        for (int i = 1; i < d6.count; ++i)
            if (oracle::mag_less(d6.mag[i], d6.mag[i - 1])) sorted_ok = false;
        P1STAR_CHECK(cs, sorted_ok, "f6_order_total");
        sdet_destroy(h6);
        d6.free_all();
    }

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1star (oracle)

// ============================================================================
// negative 组 — F5 扩展 + OOM 注入 (fork+exec 子进程扫描)
// ============================================================================
namespace p1star {

// 子进程入口: /proc/self/exe oom-child — 单次注入探测 (fork 后 exec 避免
// libgomp fork 状态问题; 需要 LD_PRELOAD=sdet_oom_interposer.so 已在环境中)。
// 注入锚定: dlsym RTLD_DEFAULT 调 interposer 导出 sdet_oom_arm(N) 在检测前
// 重置计数 — 不 arm 则小 N 全落在进程启动期分配 (libstdc++/stdio), 扫描
// 不触达 sdet 检测段。
int oom_child_main(const char* n_str) {
    const long n = std::atol(n_str);
    typedef void (*arm_fn)(long);
    arm_fn arm = (arm_fn)dlsym(RTLD_DEFAULT, "sdet_oom_arm");
    // arm 前置分配: img (128KB) 与 handle 建立后再锚定注入窗口 → N=1 落在
    // sdet_detect_impl 段内首个分配, 不落在进程启动/测试装置分配
    const int W = 128, H = 128;
    std::vector<double> img((std::size_t)W * H, 300.0);
    add_gaussian_to(img, W, H, 64.37, 51.61, 2000.0, 2.0);
    StarDetectorHandle h = sdet_create(nullptr);
    if (arm) arm(n);  // 未预载 interposer 时 arm 为空 (neg_interposer_loaded 会先败)
    double* x = nullptr; double* y = nullptr; float* fl = nullptr; float* mg = nullptr;
    int* sa = nullptr; int* hs = nullptr; int cnt = -1;
    const int rc = sdet_detect_ex_f64(h, img.data(), W, H, &x, &y, &fl, &sa, &mg, &hs, &cnt,
                                      nullptr, 0, nullptr);
    {
        // 失败现场: bad_alloc 终止时 g_seq 停在注入点 — 写文件定位异常注入源
        if (rc != 0 || cnt <= 0) {
            if (const char* sp = std::getenv("OOM_SEQ")) {
                FILE* f = std::fopen(sp, "w");
                if (f) { std::fprintf(f, "rc=%d cnt=%d\n", rc, cnt); std::fclose(f); }
            }
        }
    }
    const int nulls = (x == nullptr) + (y == nullptr) + (fl == nullptr) + (sa == nullptr) +
                      (mg == nullptr) + (hs == nullptr);
    std::printf("OOMCHILD rc=%d count=%d nulls=%d\n", rc, cnt, nulls);
    std::fflush(stdout);
    if (rc == 0) sdet_free_detect_ex(x, y, fl, sa, mg, hs, nullptr, 0);
    sdet_destroy(h);
    // 退出码传播检测结果 (rc!=0 → 1): 父进程扫描以 shell_rc==1 && rc==-1
    // 判定事务化检查点 (此前无条件 return 0, 检查点永远进不了 checkpoint 分支)
    return rc == 0 ? 0 : 1;
}

struct OomProbe {
    int shell_rc = -1;
    int rc = -999;
    int count = -999;
    int nulls = -1;
};

// fork+exec 单次探测 (捕获 stdout; total_path 非空时子进程 destructor 写
// OOM_TOTAL 文件, 供统计模式实测 detect 段分配总数)
OomProbe oom_probe_once(long fail_at, const char* total_path = nullptr) {
    int fds[2];
    if (pipe(fds) != 0) return {};
    const pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return {}; }
    if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], 1);
        close(fds[1]);
        if (total_path) setenv("OOM_TOTAL", total_path, 1);
        else unsetenv("OOM_TOTAL");
        char nbuf[32];
        std::snprintf(nbuf, sizeof(nbuf), "%ld", fail_at);
        char arg0[] = "p1star-tests";
        char a1[] = "oom-child";
        char* argv2[] = {arg0, a1, nbuf, nullptr};
        execv("/proc/self/exe", argv2);
        _exit(127);
    }
    close(fds[1]);
    std::string out;
    char buf[512];
    ssize_t k;
    while ((k = read(fds[0], buf, sizeof(buf))) > 0) out.append(buf, (std::size_t)k);
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    OomProbe p;
    p.shell_rc = WIFEXITED(status) ? WEXITSTATUS(status)
                                   : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1);
    const std::size_t tag = out.find("OOMCHILD ");
    if (tag != std::string::npos) {
        std::sscanf(out.c_str() + tag, "OOMCHILD rc=%d count=%d nulls=%d", &p.rc, &p.count, &p.nulls);
    }
    return p;
}

int test_negative() {
    CheckState cs;

    // malloc 序确定性前置: ctest 注册 ENVIRONMENT OMP_NUM_THREADS=1
    const int omp_env = read_omp_env_threads();
    std::printf("[neg] OMP_NUM_THREADS=%d\n", omp_env);
    P1STAR_CHECK_EQ(cs, omp_env, 1, "neg_omp_single_thread_env");

    // 被测可执行 interposer 真实加载验证 (dlsym 探针; LD_PRELOAD 环境串存在
    // ≠ 加载成功 — glibc ld.so 对含空格路径按空格分词逐段报错并忽略)
    {
        void* sym = dlsym(RTLD_DEFAULT, "sdet_oom_interposer_magic");
        const bool interposed =
            sym != nullptr && *(int*)sym == 0x53544152 /* "STAR" */;
        P1STAR_CHECK(cs, interposed, "neg_interposer_loaded");
    }

    // ---- OOM 注入扫描 (arm 锚定检测段) -----------------------------------
    // 策略: 步骤 1 统计模式 (N=-1) 实测 detect 段分配总数 G (含 sdet_log
    // 格式化缓冲; OMP=1 + 同 fixture 下确定性); 步骤 2 头部抽样 N=1..24
    // (STL/日志路径 → std::bad_alloc 定义性终止, 无静默坏数据) + 尾部发布
    // 窗口 [G-12, G+2] 逐号 (输出十数组 raw malloc 六连 → 批次 R 事务化
    // rc=-1 检查点)。输出数组是 detect 段最后的 raw malloc, 位于日志分配
    // 之后 — 仅尾部窗口可达事务化路径。
    std::vector<long> checkpoints;   // rc=-1 事务化检查点
    std::vector<long> gaps;          // rc=0 非必需分配
    std::vector<long> aborts;        // 134 定义性终止 (STL/GSL 内部)
    // OOM_TOTAL 落盘路径：不写死 "/tmp"（Windows 无此路径；宿主 /tmp 可能不可写 —— AGENTS §3）。
    // 用当前工作目录相对路径（ctest WORKING_DIRECTORY 保证可写），父子进程共享 cwd。
    char total_path[] = "astrocs_p1star_oom_total.txt";
    {
        OomProbe g = oom_probe_once(-1, total_path);
        P1STAR_CHECK_EQ(cs, g.shell_rc, 0, "oom_total_probe_rc");
        FILE* f = std::fopen(total_path, "r");
        long G = -1;
        if (f) { std::fscanf(f, "%ld", &G); std::fclose(f); }
        std::printf("[neg] detect-seg alloc total G=%ld\n", G);
        P1STAR_CHECK(cs, G > 24, "oom_total_sane");
        if (G > 24) {
            for (long n = 1; n <= 24; ++n) {
                const OomProbe p = oom_probe_once(n);
                if (p.shell_rc == 1 && p.rc == -1) {
                    checkpoints.push_back(n);
                    P1STAR_CHECK_EQ(cs, p.nulls, 6, "oom_txn_outputs_all_null");
                    P1STAR_CHECK_EQ(cs, p.count, -1, "oom_txn_count_m1");
                } else if (p.shell_rc == 0 && p.rc == 0) {
                    gaps.push_back(n);
                    P1STAR_CHECK(cs, p.count >= 0, "oom_gap_no_silent_corruption");
                } else {
                    aborts.push_back(n);
                }
            }
            const long lo = G - 12 > 25 ? G - 12 : 25;
            for (long n = lo; n <= G + 2; ++n) {
                const OomProbe p = oom_probe_once(n);
                if (p.shell_rc == 1 && p.rc == -1) {
                    checkpoints.push_back(n);
                    P1STAR_CHECK_EQ(cs, p.nulls, 6, "oom_txn_outputs_all_null");
                    P1STAR_CHECK_EQ(cs, p.count, -1, "oom_txn_count_m1");
                } else if (p.shell_rc == 0 && p.rc == 0) {
                    gaps.push_back(n);
                    P1STAR_CHECK(cs, p.count >= 0, "oom_gap_no_silent_corruption");
                } else {
                    aborts.push_back(n);
                }
            }
        }
    }
    std::printf("[neg] oom scan: checkpoints=%zu gaps=%zu aborts=%zu (ckpt N:",
                checkpoints.size(), gaps.size(), aborts.size());
    for (long n : checkpoints) std::printf(" %ld", n);
    std::printf(")\n");

    // 验收: 注入可失败 — 至少一个优雅 rc=-1 检查点
    P1STAR_CHECK(cs, !checkpoints.empty(), "oom_graceful_checkpoint_exists");
    // 间隙不得产生 count 异常 (上面逐个断言), 终止集仅登记
    std::printf("[neg] aborts (std::bad_alloc defines exit, logged): %zu\n", aborts.size());

    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1star (negative)

// main 与组注册表在 p1star_tests_main.cpp (selfcheck 目标共享本 TU, main
// 唯一性要求拆分 — 结构先例 p1hips_tests_main.cpp 同构)。

