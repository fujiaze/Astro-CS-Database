// lib/algorithms/projection/tests/p3wcs/p3_projection_registry_test.cpp
// FIX-205 共址测试: 产品声明注册表 + 显式「不支持」 + 适用域 + 机器判据
//
// 规范依据:
//   * ASTROCS_DESIGN.md §5.3（8 投影冻结; 未实现必须显式报「不支持」, 禁止声称
//     支持; 每种投影声明适用域, 违反 ⇒ 拒绝）;
//   * MOD-01 🔴P17 / ARCH-01 🔴14（文档说 1、代码说 4 ⇒ 以可运行验证为准）;
//   * ENGINEERING_SPEC §8（每项检查能红能绿; 可执行负例入口 --self-test）。
//
// 覆盖:
//   C1 机器判据: 声明集 D == 实现集 I == 实际可运行集 R（R = 生产路径黑盒实跑）;
//   C2 负例: 8 冻结码中未实现者请求 ⇒ 显式「不支持」+ 已支持清单, 不静默回落 TAN;
//   C3 正例: 已实现投影（TAN）往返误差 < 1e-6 px + CTYPE 面;
//   C4 适用域: |dec|≤85 / FOV≤20 / det(CD)<0 / CRPIX FITS 1-based / 往返<1e-6,
//      违反 ⇒ 拒绝（逐项能红能绿）;
//   C5 跨注册表一致: v6 内核 registry 行不得被冒充为产品可声明;
//   C6 --self-test: 判据函数在变异输入下必红（非退化证明）。
#include "p3_projection_registry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "p3_proj_v6.h"
#include "p3_wcs.h"

namespace {

int failures = 0;

#define CHECK_MSG(cond, msg)                                       \
    do {                                                           \
        if (!(cond)) {                                             \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,     \
                         __LINE__, msg);                           \
            ++failures;                                            \
        }                                                          \
    } while (0)

using astrocs::phase3::P3ProjFrozenEntry;
using astrocs::phase3::P3ProjProductStatus;
using astrocs::phase3::P3WcsApplicability;
using astrocs::phase3::P3WcsDescriptor;
using astrocs::phase3::P3WcsStatus;

constexpr double kRoundtripTolPx = 1e-6;   // SCI-P3-001 §7 冻结容差

std::vector<std::string> sorted(std::vector<std::string> v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

std::vector<std::string> declared_set() {
    int n = 0;
    const char* const* c = astrocs::phase3::p3_proj_declared_codes(&n);
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) out.push_back(c[i]);
    return sorted(out);
}

std::vector<std::string> implemented_set() {
    int n = 0;
    const char* const* c = astrocs::phase3::p3_proj_implemented_codes(&n);
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) out.push_back(c[i]);
    return sorted(out);
}

// R: 实际可运行集 —— 对 8 冻结码逐一黑盒实跑生产路径（不看声明表）。
std::vector<std::string> runnable_set() {
    int n = 0;
    const P3ProjFrozenEntry* tab = astrocs::phase3::p3_proj_frozen_table(&n);
    std::vector<std::string> out;
    for (int i = 0; i < n; ++i) {
        std::string detail;
        if (astrocs::phase3::p3_proj_probe(tab[i].code, &detail) ==
            P3WcsStatus::P3_WCS_OK) {
            out.push_back(tab[i].code);
        }
    }
    return sorted(out);
}

// 机器判据本体: D == I == R（集合相等）。独立成函数以便 --self-test 变异。
bool criterion_holds(const std::vector<std::string>& d,
                     const std::vector<std::string>& i,
                     const std::vector<std::string>& r) {
    return sorted(d) == sorted(i) && sorted(i) == sorted(r);
}

std::string join(const std::vector<std::string>& v) {
    std::string s;
    for (size_t k = 0; k < v.size(); ++k) {
        if (k) s += ",";
        s += v[k];
    }
    return s;
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// ---- C1: 注册表声明集 == 实际可运行集 -------------------------------------
void test_registry_equals_runnable() {
    const std::vector<std::string> d = declared_set();
    const std::vector<std::string> i = implemented_set();
    const std::vector<std::string> r = runnable_set();
    std::fprintf(stderr, "[registry] declared={%s} implemented={%s} runnable={%s}\n",
                 join(d).c_str(), join(i).c_str(), join(r).c_str());
    CHECK_MSG(criterion_holds(d, i, r),
              "机器判据: 声明集 == 实现集 == 实际可运行集");
    CHECK_MSG(d.size() == 1 && d[0] == "TAN",
              "声明集当前 = {TAN}（DESIGN §5.3 当前登记仅 TAN）");
    CHECK_MSG(astrocs::phase3::p3_proj_registry_selfcheck() == 0,
              "registry 自检全过");
}

// ---- C2: 负例（未实现投影显式「不支持」+ 已支持清单, 无静默回落）----------
void test_unsupported_negative() {
    int n = 0;
    const P3ProjFrozenEntry* tab = astrocs::phase3::p3_proj_frozen_table(&n);
    CHECK_MSG(n == 8, "冻结表 8 行（DESIGN §5.3）");
    int n_neg = 0;
    for (int i = 0; i < n; ++i) {
        const char* code = tab[i].code;
        if (astrocs::phase3::p3_proj_is_declared(code)) continue;
        ++n_neg;
        // (a) 声明门: 显式不支持 + 原因 + 已支持清单
        std::string why;
        const P3WcsStatus ds = astrocs::phase3::p3_proj_declare(code, &why);
        CHECK_MSG(ds == P3WcsStatus::P3_WCS_UNSUPPORTED, "未声明码 → UNSUPPORTED");
        CHECK_MSG(contains(why, code), "拒绝原因含请求码");
        CHECK_MSG(contains(why, "unsupported"), "拒绝原因显式报「不支持」");
        CHECK_MSG(contains(why, "supported projections: TAN"),
                  "拒绝原因列出已支持清单");
        // (b) 请求面门（生产路径唯一语义源）
        std::string vwhy;
        CHECK_MSG(astrocs::phase3::p3_wcs_validate_request(code, nullptr, nullptr,
                                                           &vwhy) ==
                      P3WcsStatus::P3_WCS_UNSUPPORTED,
                  "p3_wcs_validate_request 拒绝未实现投影");
        CHECK_MSG(contains(vwhy, code) && contains(vwhy, "unsupported"),
                  "请求面拒绝原因显式");
        // (c) 构造面门: 不产半成品, 不静默回落 TAN
        P3WcsDescriptor d{};
        d.crval_ra_deg = 123.0;   // 哨兵: 失败不得改写
        const P3WcsStatus ms = astrocs::phase3::p3_wcs_make(
            150.0, 2.0, 0.0001389, 64, 64, "east_left", 0.0, &d, code);
        CHECK_MSG(ms == P3WcsStatus::P3_WCS_UNSUPPORTED,
                  "p3_wcs_make 拒绝未实现投影");
        CHECK_MSG(d.crval_ra_deg == 123.0, "失败不改写 *out（无 TAN 半成品）");
        // (d) 未实现码绝不产生 CTYPE 声明
        std::string detail;
        CHECK_MSG(astrocs::phase3::p3_proj_probe(code, &detail) !=
                      P3WcsStatus::P3_WCS_OK,
                  "实跑探针: 未实现码跑不通");
    }
    CHECK_MSG(n_neg == 7, "8 冻结码中 7 个未实现（逐一负例）");
    // 未冻结/大小写变体: 显式「未知投影码」
    const char* unknown[3] = {"ZZZ", "tan", ""};
    for (const char* c : unknown) {
        std::string why;
        CHECK_MSG(astrocs::phase3::p3_proj_declare(c, &why) ==
                      P3WcsStatus::P3_WCS_UNSUPPORTED,
                  "未冻结/大小写变体 → UNSUPPORTED");
        CHECK_MSG(contains(why, "supported projections: TAN"),
                  "未知码拒绝原因也列出已支持清单");
    }
    std::string zwhy;
    astrocs::phase3::p3_proj_declare("ZZZ", &zwhy);
    CHECK_MSG(contains(zwhy, "unknown projection code"),
              "未冻结码标注为未知码（与冻结未实现区分）");
    std::string swhy;
    astrocs::phase3::p3_proj_declare("SIN", &swhy);
    CHECK_MSG(contains(swhy, "not product-declarable"),
              "冻结未实现码标注为不可声明（原因可追溯）");
    // 缺省 = TAN（非静默回落: 缺省是冻结语义）
    std::string dwhy;
    CHECK_MSG(astrocs::phase3::p3_proj_declare(nullptr, &dwhy) ==
                  P3WcsStatus::P3_WCS_OK,
              "缺省投影 = TAN");
}

// ---- C3: 正例（TAN 往返 < 1e-6 px）---------------------------------------
void test_implemented_positive() {
    const double scales[4] = {0.0001389, 0.001, 0.005, 0.02};
    for (double s : scales) {
        P3WcsDescriptor d{};
        CHECK_MSG(astrocs::phase3::p3_wcs_make(150.0, 2.0, s, 97, 89, "east_left",
                                               0.0, &d, "TAN") ==
                      P3WcsStatus::P3_WCS_OK,
                  "TAN make 成功");
        double max_err = -1.0;
        CHECK_MSG(astrocs::phase3::p3_wcs_roundtrip_max_error_px(&d, &max_err) ==
                      P3WcsStatus::P3_WCS_OK,
                  "往返检查可执行");
        CHECK_MSG(max_err >= 0.0 && max_err < kRoundtripTolPx,
                  "TAN 往返误差 < 1e-6 px（SCI §7 冻结）");
        CHECK_MSG(astrocs::phase3::p3_wcs_check_applicability(&d, nullptr) ==
                      P3WcsStatus::P3_WCS_OK,
                  "TAN 适用域检查通过");
        const std::string kw = astrocs::phase3::p3_wcs_fits_keywords(&d);
        CHECK_MSG(contains(kw, "RA---TAN") && contains(kw, "DEC--TAN"),
                  "CTYPE 面 = RA---TAN / DEC--TAN");
        const double det = d.cd[0][0] * d.cd[1][1] - d.cd[0][1] * d.cd[1][0];
        CHECK_MSG(det < 0.0, "手性 det(CD) < 0");
        CHECK_MSG(d.crpix_x == (97 + 1) / 2.0 && d.crpix_y == (89 + 1) / 2.0,
                  "CRPIX = FITS 1-based 像素中心 (W+1)/2");
        CHECK_MSG(astrocs::phase3::p3_wcs_fov_deg(s, 97, 89) <= 20.0,
                  "FOV 在适用域内");
    }
    // 往返判据非退化: 双桥接（+1px）注入必被 1e-6 px 门捕获
    P3WcsDescriptor d{};
    astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.001, 97, 89, "east_left", 0.0, &d,
                                 "TAN");
    double ra = 0.0, dec = 0.0, x = 0.0, y = 0.0;
    astrocs::phase3::p3_wcs_pix2world(&d, 32.0, 32.0, &ra, &dec);
    astrocs::phase3::p3_wcs_world2pix(&d, ra, dec, &x, &y);
    CHECK_MSG(std::hypot(x - 32.0, y - 32.0) < kRoundtripTolPx,
              "无注入: 往返 < 1e-6 px");
    CHECK_MSG(std::hypot(x - 33.0, y - 32.0) > kRoundtripTolPx,
              "注入 1px 桥接偏差: 判据必红（非退化）");
}

// ---- C4: 适用域违反 ⇒ 拒绝 ------------------------------------------------
void test_applicability_rejections() {
    P3WcsDescriptor d{};
    // |CRVAL2| > 85°
    CHECK_MSG(astrocs::phase3::p3_wcs_make(150.0, 85.1, 0.001, 64, 64,
                                           "east_left", 0.0, &d, "TAN") ==
                  P3WcsStatus::P3_WCS_PARAM,
              "|CRVAL2|>85° → 拒绝");
    // FOV > 20°（0.05 deg/px × √(512²+512²) = 36.2°）
    CHECK_MSG(astrocs::phase3::p3_wcs_fov_deg(0.05, 512, 512) > 20.0,
              "FOV 计算超 20°");
    CHECK_MSG(astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.05, 512, 512,
                                           "east_left", 0.0, &d, "TAN") ==
                  P3WcsStatus::P3_WCS_PARAM,
              "FOV>20° → 拒绝（SCI §9a-12）");
    CHECK_MSG(astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.02, 512, 512,
                                           "east_left", 0.0, &d, "TAN") ==
                  P3WcsStatus::P3_WCS_OK,
              "FOV=14.5° 边界内 → 接受（门非恒红）");
    // 手性 det(CD) >= 0（手工 descriptor: 正向 east_right 注入）
    P3WcsDescriptor bad{};
    astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.001, 64, 64, "east_left", 0.0, &bad,
                                 "TAN");
    bad.cd[0][0] = std::fabs(bad.cd[0][0]);
    bad.cd[1][1] = std::fabs(bad.cd[1][1]);
    std::string why;
    CHECK_MSG(astrocs::phase3::p3_wcs_check_applicability(&bad, &why) ==
                  P3WcsStatus::P3_WCS_PARAM,
              "det(CD)>0 → 拒绝");
    CHECK_MSG(contains(why, "chirality"), "手性拒绝原因可读");
    // CRPIX 非 FITS 1-based 像素中心
    P3WcsDescriptor bad2{};
    astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.001, 64, 64, "east_left", 0.0,
                                 &bad2, "TAN");
    bad2.crpix_x = 0.0;
    CHECK_MSG(astrocs::phase3::p3_wcs_check_applicability(&bad2, &why) ==
                  P3WcsStatus::P3_WCS_PARAM,
              "CRPIX 非 (W+1)/2 → 拒绝");
    CHECK_MSG(contains(why, "CRPIX"), "CRPIX 拒绝原因可读");
    // 未声明适用域的投影 → UNSUPPORTED（fail-closed）
    CHECK_MSG(astrocs::phase3::p3_wcs_applicability("SIN") == nullptr,
              "SIN 无适用域声明 → nullptr");
    CHECK_MSG(astrocs::phase3::p3_wcs_applicability("TAN") != nullptr,
              "TAN 有适用域声明");
    const P3WcsApplicability* ap = astrocs::phase3::p3_wcs_applicability("TAN");
    CHECK_MSG(ap->max_abs_crval_dec_deg == 85.0 && ap->max_fov_deg == 20.0 &&
                  ap->require_negative_det_cd &&
                  ap->crpix_fits_1based_pixel_center &&
                  ap->roundtrip_tol_px == 1e-6,
              "TAN 适用域声明 = 85/20/det<0/1-based/1e-6");
}

// ---- C5: 跨注册表一致（v6 内核行不得冒充产品声明）------------------------
void test_cross_registry() {
    int nv = 0;
    const astrocs::phase3proj::v6::Spec* v6 =
        astrocs::phase3proj::v6::registry_table(&nv);
    CHECK_MSG(nv == 4, "v6 内核 registry 4 行（TAN/SIN/CAR/AIT）");
    for (int i = 0; i < nv; ++i) {
        const char* code = v6[i].code;
        CHECK_MSG(astrocs::phase3::p3_proj_is_frozen_code(code),
                  "v6 内核行 ∈ 冻结集");
        if (astrocs::phase3::p3_proj_is_declared(code)) continue;
        // 内核已实现但产品未接线: 冻结表必须如实标为 kKernelOnly
        int nf = 0;
        const P3ProjFrozenEntry* ft = astrocs::phase3::p3_proj_frozen_table(&nf);
        for (int k = 0; k < nf; ++k) {
            if (std::strcmp(ft[k].code, code) != 0) continue;
            CHECK_MSG(ft[k].status == P3ProjProductStatus::kKernelOnly,
                      "v6 已实现但未接线的码标为 kKernelOnly（不得声称支持）");
        }
        std::string why;
        CHECK_MSG(astrocs::phase3::p3_proj_declare(code, &why) !=
                      P3WcsStatus::P3_WCS_OK,
                  "v6 内核码不得经产品声明门放行");
    }
}

// ---- C6: --self-test（判据能红能绿）--------------------------------------
int self_test() {
    int bad = 0;
    const std::vector<std::string> d = declared_set();
    const std::vector<std::string> i = implemented_set();
    const std::vector<std::string> r = runnable_set();
    auto expect = [&](bool got, bool want, const char* what) {
        if (got != want) {
            std::fprintf(stderr, "SELF-TEST FAIL: %s (got %d want %d)\n", what,
                         (int)got, (int)want);
            ++bad;
        }
    };
    expect(criterion_holds(d, i, r), true, "真值输入必绿");
    expect(criterion_holds({"TAN", "SIN"}, {"TAN"}, {"TAN"}), false,
           "声明未实现（SIN）必红");
    expect(criterion_holds({}, {"TAN"}, {"TAN"}), false, "声明缺实现必红");
    expect(criterion_holds({"TAN"}, {"TAN", "SIN"}, {"TAN", "SIN"}), false,
           "实现未声明必红");
    expect(criterion_holds({"TAN"}, {"TAN"}, {"TAN", "SIN"}), false,
           "声明缺可运行必红");
    expect(criterion_holds({"TAN"}, {"TAN"}, {}), false, "可运行集为空必红");
    // 探针本体: TAN 可跑, 未实现码跑不通
    std::string detail;
    expect(astrocs::phase3::p3_proj_probe("TAN", &detail) == P3WcsStatus::P3_WCS_OK,
           true, "探针 TAN 可跑");
    expect(astrocs::phase3::p3_proj_probe("SIN", &detail) == P3WcsStatus::P3_WCS_OK,
           false, "探针 SIN 跑不通");
    if (bad == 0) {
        std::printf("FIX-205 SELF-TEST PASS: 判据在变异输入下必红, 真值输入必绿\n");
        return 0;
    }
    return 1;
}

// ---- 可运行矩阵（证据模式, 不参与断言）------------------------------------
// 逐一实跑 8 冻结码的**产品路径**与 **v6 内核路径**, 打印「跑得通/跑不通」,
// 用于回答「文档说 1、代码说 4」——以可运行验证为准（MOD-01 🔴P17）。
const char* v6_status_name(astrocs::phase3proj::v6::ProjStatus s) {
    switch (s) {
        case astrocs::phase3proj::v6::ProjStatus::kOk: return "OK";
        case astrocs::phase3proj::v6::ProjStatus::kParam: return "PARAM";
        case astrocs::phase3proj::v6::ProjStatus::kUnsupported: return "UNSUPPORTED";
        case astrocs::phase3proj::v6::ProjStatus::kHemisphere: return "HEMISPHERE";
    }
    return "?";
}

const char* p3_status_name(P3WcsStatus s) {
    switch (s) {
        case P3WcsStatus::P3_WCS_OK: return "OK";
        case P3WcsStatus::P3_WCS_PARAM: return "PARAM";
        case P3WcsStatus::P3_WCS_UNSUPPORTED: return "UNSUPPORTED";
        case P3WcsStatus::P3_WCS_HEMISPHERE: return "HEMISPHERE";
    }
    return "?";
}

int matrix_mode() {
    using astrocs::phase3proj::v6::Descriptor;
    using astrocs::phase3proj::v6::ProjStatus;
    int n = 0;
    const P3ProjFrozenEntry* tab = astrocs::phase3::p3_proj_frozen_table(&n);
    std::printf("%-4s %-10s %-11s %-22s %-12s\n", "code", "declarable",
                "implemented", "product_probe", "v6_kernel");
    int n_product_ok = 0;
    int n_kernel_ok = 0;
    for (int i = 0; i < n; ++i) {
        const char* code = tab[i].code;
        std::string detail;
        const P3WcsStatus ps = astrocs::phase3::p3_proj_probe(code, &detail);
        if (ps == P3WcsStatus::P3_WCS_OK) ++n_product_ok;
        // v6 内核路径（独立于产品路径）: registry 查表 + make + 往返
        const astrocs::phase3proj::v6::Spec* sp =
            astrocs::phase3proj::v6::registry_find(code);
        std::string kstatus = "not-implemented";
        if (sp != nullptr) {
            Descriptor d{};
            const ProjStatus ms = astrocs::phase3proj::v6::make(
                sp->id, 150.0, 2.0, 0.0001389, 64, 64, "east_left", 0.0, &d);
            if (ms != ProjStatus::kOk) {
                kstatus = v6_status_name(ms);
            } else {
                double ra = 0.0, dec = 0.0, x = 0.0, y = 0.0;
                const ProjStatus a = astrocs::phase3proj::v6::pix2world(
                    &d, 32.0, 32.0, &ra, &dec);
                const ProjStatus b = (a == ProjStatus::kOk)
                                         ? astrocs::phase3proj::v6::world2pix(
                                               &d, ra, dec, &x, &y)
                                         : a;
                if (b == ProjStatus::kOk && std::hypot(x - 32.0, y - 32.0) < 1e-6) {
                    kstatus = "OK(roundtrip<1e-6px)";
                    ++n_kernel_ok;
                } else {
                    kstatus = v6_status_name(b);
                }
            }
        }
        std::printf("%-4s %-10s %-11s %-22s %-12s\n", code,
                    astrocs::phase3::p3_proj_is_declared(code) ? "yes" : "no",
                    astrocs::phase3::p3_proj_is_implemented(code) ? "yes" : "no",
                    p3_status_name(ps), kstatus.c_str());
    }
    std::printf("product-runnable=%d/8  v6-kernel-runnable=%d/8\n", n_product_ok,
                n_kernel_ok);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--self-test") == 0) return self_test();
    if (argc > 1 && std::strcmp(argv[1], "--matrix") == 0) return matrix_mode();
    test_registry_equals_runnable();
    test_unsupported_negative();
    test_implemented_positive();
    test_applicability_rejections();
    test_cross_registry();
    if (failures == 0) {
        std::printf(
            "FIX-205 PROJ REGISTRY PASS（声明集==实现集==可运行集 + 未实现显式不支持 "
            "+ 适用域拒绝 + TAN 往返 <1e-6px）\n");
        return 0;
    }
    std::fprintf(stderr, "FIX-205 PROJ REGISTRY FAIL (%d)\n", failures);
    return 1;
}
