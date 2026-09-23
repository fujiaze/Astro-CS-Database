// lib/algorithms/resample/tests/p3rsmp/p3_nan_mask_test.cpp
// ── NAN-SAMPLE-MASK-COVERAGE-NAN 内核 Oracle + 负例 (ALG-P3-003 §2 G4 / §4) ──
//
// 权威 (逐字同口径三处; 分歧以 DATA-002 §2a 为准):
//   · docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md §2a invalid_handling:
//     样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数 (count_field=n_rejected_nonfinite);
//   · docs/standards/NUMERIC_STANDARD.md §MUST「NaN/Inf 契约」;
//   · docs/algorithms/PHASE3_RESAMPLE.md §2 G4 / §4 (禁「零填」替代语义)。
//
// 判据 (非退化; 真值无效应/实现退回旧口径时必判红):
//   T1 harness 自检: fixture 的 FITS 序索引与生产 read_leaf 映射一致 (前提; 失败即前提失效)
//   T2 掩膜+重归一: 1/2/3 个 ¬isfinite 邻域样本 ⇒ S == 剩余合格样本**重归一**加权和
//      (解析真值, 相对容差 4·eps_f32); 被剔除样本生效权重**恰为 0**; Σc=1 (合格集上)
//   T3 零合格样本: 4 个全 ¬isfinite ⇒ S=NaN 且 C=1 (覆盖级 NaN; 禁零填/禁哨兵)
//   T4 ±Inf 与 NaN 同类 (含混合、含单 -Inf)
//   T5 强制计数: n_rejected_nonfinite 按原因分类正确暴露 (计数 0 与「字段缺失」可区分)
//   T6 方差项: 剔除后按**重归一权重**传播 var_out==Σc'_k²u_k (FP64 1e-12); 零合格 ⇒ NaN
//   T7 非退化锚: T2/T6 的真值与「等权平均」「沿用原几何权重」两种错值均有 >1% / >5% 差
//
// Oracle 独立性: 期望值由本文件用 fixture 已知值与暴露的生效权重**独立复算**, 不调用
// 生产聚合路径; 邻域选择 (哪 4 个 leaf) 不在本用例判据内 (非本规则面, 由 T1 自检其映射)。
#include "p3_resample.h"

#include "aio_atomic_file.h"
#include "healpix_core.h"
#include "p1sess_fixtures.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using astrocs::phase3::P3_RS_OK;
using astrocs::phase3::P3SampleRejection;
using astrocs::phase3::P3Sampler;
using astrocs::phase3::P3UncPixelState;
using astrocs::phase3::P3UncertaintySource;

static int failures = 0;
#define CHECK(cond)                                                              \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                                \
    }                                                                            \
  } while (0)
#define CHECK_MSG(cond, ...)                                                     \
  do {                                                                           \
    if (!(cond)) {                                                               \
      std::fprintf(stderr, "CHECK failed %s:%d: %s -- ", __FILE__, __LINE__, #cond); \
      std::fprintf(stderr, __VA_ARGS__);                                         \
      std::fprintf(stderr, "\n");                                                \
      ++failures;                                                                \
    }                                                                            \
  } while (0)

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 512;
constexpr size_t kPx = static_cast<size_t>(kW) * kW;
constexpr float kBase = 1000.0f;   // 非角点像素常量
constexpr double kEpsF32 = static_cast<double>(std::numeric_limits<float>::epsilon());

// 逐像素值表 (FITS 序 = write_fits_file 的 i 序)
struct PxTable {
    std::vector<float> sig;
    std::vector<float> var;
};
const PxTable* g_tab = nullptr;
float gen_sig(int i, void*) { return g_tab->sig[static_cast<size_t>(i)]; }
float gen_var(int i, void*) { return g_tab->var[static_cast<size_t>(i)]; }

// leaf → tile0 内 FITS 序索引 (经 healpix_core 权威映射, 非第二套数学)。
// fixture 为 order-0 HiPS ⇒ 叶级 order = 0 + log2(512) = 9 (nside 512),
// 与生产 read_leaf 的 (leaf_order = s->order + 9) 逐字一致。
constexpr uint32_t kLeafOrder = 9;
constexpr uint32_t kLeafNside = 512;
uint64_t leaf_to_fits_index(uint64_t leaf) {
    const uint64_t tip = astrocs::healpix::leaf_to_tile_nest(leaf, kLeafOrder, 0);
    if (tip != 0ull) return ~0ull;
    const uint64_t first = astrocs::healpix::tile_to_leaf_nest(tip, 0, kLeafOrder);
    return astrocs::healpix::nested_local_to_fits_index(leaf - first, 9, kW);
}

std::string hips_properties_text(const char* bunit, bool with_pixel_semantics) {
    std::string s;
    s += "hips_order = 0\n";
    s += "hips_tile_width = 512\n";
    s += "hips_tile_format = fits\n";
    s += "hips_frame = equatorial\n";   // IVOA REC-HIPS-1.0 §4.4.1 标准值
    s += "dataproduct_type = image\n";
    s += "hips_version = 1.0\n";
    if (bunit) s += std::string("BUNIT = ") + bunit + "\n";
    if (with_pixel_semantics) {
        s += "ASTROCS_PIXEL_SEMANTICS = surface_brightness\n";
        s += "ASTROCS_PIXEL_AREA_POWER = -2\n";
    }
    return s;
}

// 手写子产品: <root>/<sub>/properties + Norder0/Dir0/Npix0.fits (逐像素完全可控,
// 含 NaN/±Inf —— 生产 writer 会把非有限 flux 归成 NaN, 无法注入 Inf)
bool write_sub(const std::string& root, const char* sub, const char* bunit,
               float (*gen)(int, void*), bool with_pixel_semantics) {
    const std::string dir = root + "/" + sub + "/Norder0/Dir0";
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) return false;
    {
        std::ofstream p(root + "/" + sub + "/properties", std::ios::binary);
        if (!p) return false;
        p << hips_properties_text(bunit, with_pixel_semantics);
    }
    return p1sess::write_fits_file(dir + "/Npix0.fits", static_cast<int>(kW),
                                   static_cast<int>(kW), gen, nullptr) == 0;
}

// FIX-401 §10 完成清单: 无清单的产品根被 aio_hips_open fail-closed
bool write_manifest(const std::string& root) {
    const std::string body =
        "{\n  \"format_version\": 1,\n  \"product\": \"HiPS\",\n"
        "  \"hips_order\": 0,\n  \"hips_tile_width\": 512,\n"
        "  \"data_type\": \"float32\",\n  \"n_leaf_tiles\": 1,\n"
        "  \"products\": [\"signal\", \"variance\"]\n}\n";
    std::string aerr;
    return aio_atomic::write_file_atomic(root + "/manifest.json", body, &aerr) == 0;
}

bool build_fixture(const std::string& root, const PxTable& tab) {
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec) return false;
    g_tab = &tab;
    if (!write_sub(root, "signal", "ADU/sr", gen_sig, true)) return false;
    if (!write_sub(root, "variance", "ADU^2/sr^2", gen_var, false)) return false;
    return write_manifest(root);
}

// 采样中心: order-0 tile 0 中心 (四角同落 tile0 ⇒ 覆盖恒 1)
void tile_center(double* ra, double* dec) {
    astrocs::healpix::pix2ang_nest(1, 0ull, *ra, *dec);
}

// 找一条采样方向: c==1、四角 leaf 互异、且**角 0 权重占优** (w[0] >= 0.5)。
// 判据非退化前提: 掩膜注入点固定在角 0 ⇒ 角 0 权重大时「重归一 vs 沿用原几何
// 权重」在方差项上差 (1/(1-w0))^2 >= 4 倍, 「重归一 vs 等权平均」在值上也显著。
bool find_asymmetric_dir(P3Sampler* s, double* ra_out, double* dec_out,
                         uint64_t lf[4], double w[4]) {
    double ra0 = 0, dec0 = 0;
    tile_center(&ra0, &dec0);
    for (int i = 1; i <= 200; ++i) {
        const double d = 0.001 * static_cast<double>(i);
        const double cand[8][2] = {{ra0 + d, dec0}, {ra0 - d, dec0},
                                   {ra0, dec0 + d}, {ra0, dec0 - d},
                                   {ra0 + d, dec0 + d}, {ra0 - d, dec0 - d},
                                   {ra0 + d, dec0 - d}, {ra0 - d, dec0 + d}};
        for (int c = 0; c < 8; ++c) {
            float v = 0; int cov = 0;
            double ww[4] = {0, 0, 0, 0};
            uint64_t ll[4] = {0, 0, 0, 0};
            if (astrocs::phase3::p3_sample_bilinear_ex(s, cand[c][0], cand[c][1], &v, &cov,
                                                      ww, ll) != P3_RS_OK)
                continue;
            if (cov != 1 || std::isnan(v)) continue;
            if (ll[0] == ll[1] || ll[0] == ll[2] || ll[0] == ll[3] || ll[1] == ll[2] ||
                ll[1] == ll[3] || ll[2] == ll[3])
                continue;
            if (ww[0] < 0.5) continue;   // 角 0 (注入点) 权重占优 ⇒ 判据非退化
            *ra_out = cand[c][0]; *dec_out = cand[c][1];
            for (int k = 0; k < 4; ++k) { lf[k] = ll[k]; w[k] = ww[k]; }
            return true;
        }
    }
    return false;
}

// 解析 oracle: 合格样本上重归一后的加权和 (FP64, 与生产同 k 序)
double oracle_value(const double w[4], const float val[4], int* n_rej_out) {
    double wsum = 0.0; int nrej = 0;
    for (int k = 0; k < 4; ++k) { if (std::isfinite(val[k])) wsum += w[k]; else ++nrej; }
    if (n_rej_out) *n_rej_out = nrej;
    if (nrej == 4 || !(wsum > 0.0)) return std::nan("");
    double acc = 0.0;
    for (int k = 0; k < 4; ++k) {
        if (!std::isfinite(val[k])) continue;
        acc += (w[k] / wsum) * static_cast<double>(val[k]);
    }
    return acc;
}

// 错值 A: 剩余合格样本**等权平均** (忽略权重)
double wrong_unweighted_mean(const float val[4]) {
    double s = 0; int n = 0;
    for (int k = 0; k < 4; ++k) if (std::isfinite(val[k])) { s += val[k]; ++n; }
    return n ? s / n : std::nan("");
}

bool close_rel(double got, double want, double rtol) {
    const double sc = std::max(1.0, std::fabs(want));
    return std::fabs(got - want) <= rtol * sc;
}

}  // namespace

// ─────────────────────────── 用例主体 ───────────────────────────

// 一个掩膜用例: 四角注入值 + 期望
struct Case {
    const char* name;
    float corner[4];      // 四角信号值 (角序 [0][0],[1][0],[0][1],[1][1])
};

int main() {
    const char* td = std::getenv("TMPDIR");
    if (!td || !*td) td = std::getenv("TMP");
    if (!td || !*td) td = "/tmp";
    const std::string root = fs::path(td).generic_string() + "/astrocs_p3_nan_mask";
    const std::string hips = root + "/hips";
    std::error_code ec;
    fs::create_directories(root, ec);

    // ── Pass A: 定位四角 (值 = kBase + fits_index, 可唯一反查映射) ──────────
    PxTable tab;
    tab.sig.resize(kPx); tab.var.assign(kPx, 0.25f);
    for (size_t i = 0; i < kPx; ++i) tab.sig[i] = kBase + static_cast<float>(i);
    CHECK(build_fixture(hips, tab));

    double ra = 0, dec = 0;
    uint64_t lf[4] = {0, 0, 0, 0};
    double w[4] = {0, 0, 0, 0};
    {
        P3Sampler s{};
        std::string err;
        CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s, nullptr, nullptr, &err) == P3_RS_OK);
        CHECK(find_asymmetric_dir(&s, &ra, &dec, lf, w));
        // ── T1 harness 自检: 生产 read_leaf 映射 == 本用例的 FITS 序索引 ────
        for (int k = 0; k < 4; ++k) {
            double cra = 0, cdec = 0;
            astrocs::healpix::pix2ang_nest(kLeafNside, lf[k], cra, cdec);
            float v = 0; int c = -1;
            CHECK(astrocs::phase3::p3_sample_nearest(&s, cra, cdec, &v, &c) == P3_RS_OK);
            const uint64_t fi = leaf_to_fits_index(lf[k]);
            CHECK_MSG(c == 1 && fi != ~0ull && v == kBase + static_cast<float>(fi),
                      "corner %d: v=%g fi=%llu", k, (double)v, (unsigned long long)fi);
        }
        astrocs::phase3::p3_sampler_close(&s);
    }
    // 非退化前提: 权重不对称 (等权平均判据才有力)
    double wmax = 0, wmin = 1;
    for (int k = 0; k < 4; ++k) { wmax = std::max(wmax, w[k]); wmin = std::min(wmin, w[k]); }
    CHECK_MSG(wmax >= 0.45 && wmin <= 0.15, "weights not asymmetric: max=%g min=%g", wmax, wmin);
    uint64_t fi[4];
    for (int k = 0; k < 4; ++k) fi[k] = leaf_to_fits_index(lf[k]);

    // ── 掩膜用例 (角点值注入; 其余像素保持 kBase) ───────────────────────────
    const float kNaN = std::nanf("");
    const float kPInf = std::numeric_limits<float>::infinity();
    const float kNInf = -std::numeric_limits<float>::infinity();
    const Case cases[] = {
        {"T2a-1NaN", {kNaN, 2000.0f, 4000.0f, 8000.0f}},
        {"T2b-2NaN", {kNaN, 2000.0f, kNaN, 8000.0f}},
        {"T2c-3NaN", {kNaN, kNaN, kNaN, 8000.0f}},
        {"T3-allNaN", {kNaN, kNaN, kNaN, kNaN}},
        {"T4a-plusInf", {kPInf, 2000.0f, 4000.0f, 8000.0f}},
        {"T4b-minusInf", {1000.0f, 2000.0f, 4000.0f, kNInf}},
        {"T4c-mixedInfNaN", {kPInf, kNInf, kNaN, 8000.0f}},
        {"T4d-allInf", {kPInf, kNInf, kPInf, kNInf}},
    };
    for (const Case& cs : cases) {
        PxTable t2;
        t2.sig.assign(kPx, kBase);
        t2.var.assign(kPx, 0.25f);
        for (int k = 0; k < 4; ++k) t2.sig[static_cast<size_t>(fi[k])] = cs.corner[k];
        CHECK(build_fixture(hips, t2));
        P3Sampler s{};
        std::string err;
        CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s, nullptr, nullptr, &err) == P3_RS_OK);
        float v = 0; int c = -1;
        double ww[4] = {0, 0, 0, 0};
        uint64_t ll[4] = {0, 0, 0, 0};
        P3SampleRejection rej{};
        CHECK(astrocs::phase3::p3_sample_bilinear_nanmask_ex(&s, ra, dec, &v, &c, ww, ll, &rej) == P3_RS_OK);
        int n_rej_expect = 0;
        const double want = oracle_value(w, cs.corner, &n_rej_expect);
        // 几何/邻域必须与 Pass A 一致 (前提; 失败即前提失效)
        CHECK_MSG(ll[0] == lf[0] && ll[1] == lf[1] && ll[2] == lf[2] && ll[3] == lf[3],
                  "%s: corner leaves drifted", cs.name);
        // ⑤ 计数 (强制计数; 按原因分类)
        CHECK_MSG(rej.n_rejected_nonfinite == n_rej_expect, "%s: n_rejected=%d want %d",
                  cs.name, rej.n_rejected_nonfinite, n_rej_expect);
        CHECK_MSG(rej.n_rejected_nonfinite_value == n_rej_expect,
                  "%s: value-class count=%d want %d", cs.name,
                  rej.n_rejected_nonfinite_value, n_rej_expect);
        CHECK_MSG(rej.n_rejected_nonfinite_variance == 0 &&
                  rej.n_rejected_nonpositive_weight == 0, "%s: class counts must be 0", cs.name);
        CHECK_MSG(rej.n_eligible == 4 - n_rej_expect, "%s: n_eligible=%d want %d", cs.name,
                  rej.n_eligible, 4 - n_rej_expect);
        // C 语义: 4 个 tile 均可读 ⇒ C=1 (值 NaN 不改 C)
        CHECK_MSG(c == 1, "%s: coverage=%d want 1", cs.name, c);
        if (n_rej_expect == 4) {
            // ③ 零合格样本 ⇒ 覆盖级 NaN (禁零填/禁哨兵)
            CHECK_MSG(std::isnan(v), "%s: zero-eligible must be NaN", cs.name);
            for (int k = 0; k < 4; ++k)
                CHECK_MSG(ww[k] == 0.0, "%s: weights must be 0 (k=%d)", cs.name, k);
        } else {
            // ①②④ 掩膜 + 重归一
            CHECK_MSG(!std::isnan(v), "%s: masked result must be finite", cs.name);
            CHECK_MSG(close_rel(static_cast<double>(v), want, 4.0 * kEpsF32),
                      "%s: v=%.9g want=%.9g (rel=%.3g)", cs.name, (double)v, want,
                      std::fabs((double)v - want) / std::max(1.0, std::fabs(want)));
            double wsum = 0.0, wsum_elig = 0.0;
            for (int k = 0; k < 4; ++k) {
                wsum += ww[k];
                if (std::isfinite(cs.corner[k])) wsum_elig += w[k];
            }
            for (int k = 0; k < 4; ++k) {
                if (!std::isfinite(cs.corner[k])) {
                    CHECK_MSG(ww[k] == 0.0, "%s: rejected weight must be exactly 0 (k=%d)",
                              cs.name, k);
                } else {
                    // 生效权重 = 几何权重在合格集上重归一
                    CHECK_MSG(close_rel(ww[k], w[k] / wsum_elig, 1e-12),
                              "%s: effective weight k=%d got %.17g want %.17g", cs.name, k,
                              ww[k], w[k] / wsum_elig);
                }
            }
            CHECK_MSG(close_rel(wsum, 1.0, 1e-12), "%s: sum(effective weights)=%.17g", cs.name, wsum);
            // T7 非退化锚: 真值与「等权平均」错值有 >1% 相对差
            // (仅当剩余合格样本 >= 2 时该锚才有判别力; 1 个合格样本时两者恒等)
            if (n_rej_expect <= 2) {
                const double wrong = wrong_unweighted_mean(cs.corner);
                CHECK_MSG(std::fabs(wrong - want) / std::max(1.0, std::fabs(want)) > 0.01,
                          "%s: unweighted-mean wrong value too close (%.9g vs %.9g)", cs.name,
                          wrong, want);
            }
        }
        // 全部合格时计数必须为 0 (计数 0 与「字段缺失」可区分: 结构已零初始化)
        if (n_rej_expect == 0) CHECK(rej.n_rejected_nonfinite == 0);
        astrocs::phase3::p3_sampler_close(&s);
    }

    // ── T6 方差项: 剔除后按重归一权重传播 ───────────────────────────────────
    // 角 0 的 signal 与 variance 同为 NaN (真实产品语义: support<=0 ⇒ 两平面 NaN),
    // 角 1..3 的 u 互异 ⇒ var_out 必须 == Σ c_k^2 u_k (重归一权重), 而非:
    //   (a) NaN (旧口径: NaN 传播) —— 那是「方差项未随掩膜剔除」的缺陷;
    //   (b) Σ w_k^2 u_k (沿用未重归一的几何权重) —— 剔除后权重变了, 系统性偏差。
    {
        const float u1 = 0.5f, u2 = 1.0f, u3 = 2.0f;
        PxTable t3;
        t3.sig.assign(kPx, kBase);
        t3.var.assign(kPx, 0.25f);
        t3.sig[static_cast<size_t>(fi[0])] = kNaN;
        t3.var[static_cast<size_t>(fi[0])] = kNaN;
        t3.var[static_cast<size_t>(fi[1])] = u1;
        t3.var[static_cast<size_t>(fi[2])] = u2;
        t3.var[static_cast<size_t>(fi[3])] = u3;
        CHECK(build_fixture(hips, t3));
        P3Sampler s{};
        P3Sampler u{};
        P3UncertaintySource src = astrocs::phase3::P3_UNC_NONE;
        std::string err;
        CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s, nullptr, nullptr, &err) == P3_RS_OK);
        CHECK(astrocs::phase3::p3_uncertainty_open(hips.c_str(), 0, &src, &u) == P3_RS_OK);
        CHECK(src == astrocs::phase3::P3_UNC_VARIANCE);
        float v = 0; int c = -1;
        double ww[4] = {0, 0, 0, 0};
        uint64_t ll[4] = {0, 0, 0, 0};
        P3SampleRejection rej{};
        CHECK(astrocs::phase3::p3_sample_bilinear_nanmask_ex(&s, ra, dec, &v, &c, ww, ll, &rej) == P3_RS_OK);
        CHECK_MSG(!std::isnan(v) && c == 1 && rej.n_rejected_nonfinite == 1,
                  "T6: signal path must mask 1 sample (n_rej=%d, v=%g)",
                  rej.n_rejected_nonfinite, (double)v);
        double u_out = 0;
        P3UncPixelState st = astrocs::phase3::P3_U_OK;
        CHECK(astrocs::phase3::p3_uncertainty_propagate(&u, ww, ll, 4, &u_out, &st) == P3_RS_OK);
        CHECK_MSG(st == astrocs::phase3::P3_U_OK && std::isfinite(u_out),
                  "T6: masked variance must be finite/OK (st=%d u=%g)", (int)st, u_out);
        const double wsum = ww[1] + ww[2] + ww[3];
        const double want = (ww[1] / wsum) * (ww[1] / wsum) * u1 +
                            (ww[2] / wsum) * (ww[2] / wsum) * u2 +
                            (ww[3] / wsum) * (ww[3] / wsum) * u3;
        CHECK_MSG(close_rel(u_out, want, 1e-12), "T6: u_out=%.17g want=%.17g", u_out, want);
        const double wrong_geom = w[1] * w[1] * u1 + w[2] * w[2] * u2 + w[3] * w[3] * u3;
        CHECK_MSG(std::fabs(wrong_geom - want) / std::max(1e-30, std::fabs(want)) > 0.05,
                  "T6: non-renormalised variance too close (%.9g vs %.9g)", wrong_geom, want);
        astrocs::phase3::p3_uncertainty_close(&u);
        astrocs::phase3::p3_sampler_close(&s);
    }

    // ── T6b 零合格样本: signal 与 variance 同为覆盖级 NaN (禁静默 0) ────────
    {
        PxTable t4;
        t4.sig.assign(kPx, kBase);
        t4.var.assign(kPx, 0.25f);
        for (int k = 0; k < 4; ++k) {
            t4.sig[static_cast<size_t>(fi[k])] = kNaN;
            t4.var[static_cast<size_t>(fi[k])] = kNaN;
        }
        CHECK(build_fixture(hips, t4));
        P3Sampler s{};
        P3Sampler u{};
        P3UncertaintySource src = astrocs::phase3::P3_UNC_NONE;
        std::string err;
        CHECK(astrocs::phase3::p3_sampler_open_ex(hips.c_str(), &s, nullptr, nullptr, &err) == P3_RS_OK);
        CHECK(astrocs::phase3::p3_uncertainty_open(hips.c_str(), 0, &src, &u) == P3_RS_OK);
        float v = 0; int c = -1;
        double ww[4] = {0, 0, 0, 0};
        uint64_t ll[4] = {0, 0, 0, 0};
        P3SampleRejection rej{};
        CHECK(astrocs::phase3::p3_sample_bilinear_nanmask_ex(&s, ra, dec, &v, &c, ww, ll, &rej) == P3_RS_OK);
        CHECK(std::isnan(v) && c == 1 && rej.n_rejected_nonfinite == 4 && rej.n_eligible == 0);
        double u_out = 123.0;
        P3UncPixelState st = astrocs::phase3::P3_U_OK;
        CHECK(astrocs::phase3::p3_uncertainty_propagate(&u, ww, ll, 4, &u_out, &st) == P3_RS_OK);
        CHECK_MSG(std::isnan(u_out) && st == astrocs::phase3::P3_U_NAN,
                  "T6b: zero-eligible variance must be NaN/NAN (st=%d u=%g)", (int)st, u_out);
        astrocs::phase3::p3_uncertainty_close(&u);
        astrocs::phase3::p3_sampler_close(&s);
    }

    fs::remove_all(root, ec);
    if (failures == 0) {
        std::printf("P3 NAN-SAMPLE-MASK PASS (掩膜+重归一+覆盖级 NaN+强制计数+方差重归一)\n");
        return 0;
    }
    std::fprintf(stderr, "P3 NAN-SAMPLE-MASK FAIL (%d checks)\n", failures);
    return 1;
}

