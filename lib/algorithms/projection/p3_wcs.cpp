// lib/algorithms/projection/p3_wcs.cpp — TAN(gnomonic) WCS 实现 (ALG-P3-002/003) — P3-002
// W4-A9 批次 1: 原址 lib/phase3_session/p3_wcs.cpp (ASTROCS_DESIGN §7.1「projection」);
// 源逐字节迁移, 命名空间/公式/容差零改动 (ENGINEERING_SPEC §3 架构重构不改科学语义)。
// 数学: Calabretta & Greisen (2002) 标准球面三角公式(RA wrap 经 atan2+fmod 归一)。
#include "p3_wcs.h"

#include "p3_projection_registry.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace astrocs::phase3 {

namespace {
constexpr double kDeg = 180.0 / M_PI;
constexpr double kRad = M_PI / 180.0;
constexpr double kMaxAbsDec = 85.0;       // SCI/API/session 单一条件: abs(dec)<=85°
// 最大尺寸来自配置/资源合同 (PHASE3_API_V1 §2 默认 20000), 可被
// 编译期配置覆盖; 不硬编码业务值 (最大尺寸来自资源/配置合同)。
#ifndef ASTROCS_P3_MAX_SIDE
constexpr int kMaxSide = 20000;
#else
constexpr int kMaxSide = ASTROCS_P3_MAX_SIDE;
#endif

void normalize_ra(double* ra) {
    *ra = std::fmod(*ra, 360.0);
    if (*ra < 0) *ra += 360.0;
}

// ---------------------------------------------------------------------------
// STD-F1 / 前台裁决 R-02(方案 b) 导出边界桥接 —— **全文件唯一 +1 点**
//
// 口径 (合同冻结, 见 docs/science/ASTROMETRY.md 与
// docs/standards/STANDARDS_REGISTRY.md 的 STD-F1 行):
//   - 内层 (lib/algorithms/platesolve ipv 迭代反演) 保持既有 0-based 自洽约定;
//   - FITS 导出层以 FITS WCS Paper I §2.1.1 为基础: CRPIX 为 1-based 参考像素,
//     像素坐标 xp = x + 1, 中间坐标 (xi,eta) = CD·(xp − CRPIX);
//   - 桥接责任方 = **Phase3 导出边界 (本文件)**; 除本文件之外不得再施加一次 +1
//     (双重桥接 = 恒定 1px 系统偏移), 由负向注入用例锁定:
//     eng/tests/unit/p3_wcs_test.cpp (九宫格 + 双桥接/无桥接必败) 与
//     eng/tests/unit/p1wcs/p1wcs_std_f1_bridge_cross.py (astropy 四向桥接扫描)。
// 数学内容不变: 纯原点平移 (标量 +1), 不改 CD/SIP/CRVAL/CRPIX 任何数值,
// 不放宽任何容差; 冻结门 1e-4 px 与 CRPIX=w/2+0.5 不变量均不在此处变更。
// ---------------------------------------------------------------------------
constexpr double kFitsPixelOrigin = 1.0;   // FITS Paper I §2.1.1: xp = x + 1

// 0-based 像素 → FITS 1-based 像素 (唯一桥接函数: 全文件仅此一处出现 +1)
inline double fits_pixel_1based(double x0based) {
    return x0based + kFitsPixelOrigin;
}

// FITS 1-based 像素 → 0-based 像素 (逆桥接: 复用同一常量, 不引入第二处字面量)
inline double fits_pixel_0based(double x1based) {
    return x1based - kFitsPixelOrigin;
}
}  // namespace

// 请求层字段合法性 (B2-A4/A5 单一机器源)。语义与 p3_session.cpp parse_request
// 冻结拒清单一致; 未实现/未注册投影在此显式拒绝, 绝不放行也不静默改写为 TAN。
P3WcsStatus p3_wcs_validate_request(const char* projection, const char* frame,
                                    const char* coverage_output, std::string* why) {
    // 投影: 唯一语义源 = 产品声明注册表（p3_projection_registry.h, DESIGN §5.3）。
    // 未实现/未注册码显式报「不支持」+ 已支持清单; 绝不静默回落 TAN。
    const P3WcsStatus pst = p3_proj_declare(projection, why);
    if (pst != P3_WCS_OK) return pst;
    if (frame) {
        const std::string fr(frame);
        if (fr != "icrs" && fr != "ICRS") {
            if (why) *why = "frame must be icrs";
            return P3_WCS_UNSUPPORTED;
        }
    }
    if (coverage_output) {
        if (std::string(coverage_output) != "mask") {
            if (why) *why = "coverage_output must be mask";
            return P3_WCS_PARAM;
        }
    }
    return P3_WCS_OK;
}

P3WcsStatus p3_wcs_make(double centre_ra_deg, double centre_dec_deg,
                        double scale_deg_per_px, int width_px, int height_px,
                        const char* parity, double rotation_pa_deg,
                        P3WcsDescriptor* out, const char* projection) {
    if (!out) return P3_WCS_PARAM;
    // 投影先于一切数值构造显式校验 (fail-closed; 未实现投影不产半成品 descriptor)
    {
        std::string perr;
        const P3WcsStatus pst = p3_wcs_validate_request(projection, nullptr, nullptr, &perr);
        if (pst != P3_WCS_OK) return pst;
    }
    // 声明/实现分离守卫: 声明表放行但生产路径无内核 ⇒ 拒绝（禁静默回落 TAN）
    const char* canon = p3_proj_canonical_code(projection ? projection : "TAN");
    if (canon == nullptr) return P3_WCS_UNSUPPORTED;
    if (!p3_proj_is_implemented(canon)) return P3_WCS_UNSUPPORTED;
    *out = P3WcsDescriptor{};
    if (parity == nullptr) parity = "east_left";
    const std::string par = parity;
    if (par != "east_left" && par != "east_right") return P3_WCS_PARAM;
    if (std::fabs(centre_dec_deg) > kMaxAbsDec) return P3_WCS_PARAM;
    if (!(scale_deg_per_px > 0.0)) return P3_WCS_PARAM;
    if (width_px < 1 || width_px > kMaxSide || height_px < 1 || height_px > kMaxSide)
        return P3_WCS_PARAM;

    // 全部构造落在 tmp: 任一门失败 ⇒ *out 保持零初始化(不产半成品)。
    P3WcsDescriptor tmp{};
    tmp.crval_ra_deg = centre_ra_deg;
    tmp.crval_dec_deg = centre_dec_deg;
    tmp.crpix_x = (width_px + 1) / 2.0;    // pixel-center(FITS 1-based): (W+1)/2
    tmp.crpix_y = (height_px + 1) / 2.0;
    tmp.width_px = width_px;
    tmp.height_px = height_px;
    tmp.projection = canon;   // 已校验投影的冻结码字面量(静态存储, 非硬编码路径)
    // G1 (ALG-P3-002, docs/algorithms/PHASE3_RESAMPLE.md §2) 冻结输出 WCS 构造
    // (FITS 1-based, CD-only, 对角, PA=0 精确形式):
    //   east_left:  CD = diag(−s, +s)   (x 增 → RA 减, 北朝上)
    //   east_right: CD = diag(+s, −s)   (x 增 → RA 增, y 增 → Dec 减)
    // PA≠0 推广 = G1 对角形式与图像平面旋转的一致复合:
    //   CD = R(−PA)·diag(sgn_x·s, sgn_y·s),
    //   R(−PA) = [[cos PA, sin PA], [−sin PA, cos PA]]
    //   (sgn_x, sgn_y): east_left=(−1,+1), east_right=(+1,−1)。
    // 推导: TAN 切平面中间坐标 (ξ,η) 沿 (东,北) 为右手系 (det>0); 输出图像
    // 两种 parity 均要求 det(CD) = sgn_x·sgn_y·s² = −s² <0 (平面映像镜像一次,
    // 保持天球手性, 与 SCI-P3-001 §9a-4 收紧 CD1_1 符号后的 G1 一致)。
    // 展开式:
    //   CD1_1 = sgn_x·s·cosPA;  CD1_2 = sgn_y·s·sinPA
    //   CD2_1 = −sgn_x·s·sinPA; CD2_2 = sgn_y·s·cosPA
    // P0 修复 (bughunt_p0_wcs): 旧实现 east_right 分支误用 sgn_y=+1,
    // 使 PA=0 时 CD=diag(+s,+s), 违反 G1 冻结的 diag(+s,−s) (y 镜像错误)。
    // PA=0 时 cos=1/sin=0 精确退化到 G1 对角形式; PA 语义不变 (天北相对
    // +y 的位置角, 逆时针为正), east_left 分支与旧实现逐元素 bitwise 一致。
    const double pa = rotation_pa_deg * kRad;
    const double sgn_x = (par == "east_left") ? -1.0 : 1.0;
    const double sgn_y = -sgn_x;
    const double s = scale_deg_per_px;
    const double cp = std::cos(pa);
    const double sp = std::sin(pa);
    tmp.cd[0][0] = sgn_x * s * cp;
    tmp.cd[0][1] = sgn_y * s * sp;
    tmp.cd[1][0] = -sgn_x * s * sp;
    tmp.cd[1][1] = sgn_y * s * cp;

    // 输出四角同半球守卫(四角 world 变换全部成功)
    const double corners[4][2] = {{0, 0}, {double(width_px - 1), 0},
                                  {0, double(height_px - 1)},
                                  {double(width_px - 1), double(height_px - 1)}};
    for (const auto& c : corners) {
        double ra, dec;
        const P3WcsStatus st = p3_wcs_pix2world(&tmp, c[0], c[1], &ra, &dec);
        if (st != P3_WCS_OK) return st;
    }
    // 适用域门(DESIGN §5.3「违反 ⇒ 拒绝」): |CRVAL2|≤85° / FOV≤20° /
    // det(CD)<0 / CRPIX=(W+1)/2 FITS 1-based 像素中心 / 往返 < 合同容差
    // （kTanApplicability.roundtrip_tol_px = 1e-8 px，FIX-406 Oracle 冻结）。
    {
        std::string aerr;
        const P3WcsStatus ast = p3_wcs_check_applicability(&tmp, &aerr);
        if (ast != P3_WCS_OK) return ast;   // *out 保持零初始化
    }
    *out = tmp;
    return P3_WCS_OK;
}

// ---- 适用域（ASTROCS_DESIGN.md §5.3）----
namespace {

// TAN 适用域声明（SCI §9a-12 alpha 冻结 + Paper I §2.1.1 + 往返门分层）。
//
// 往返门分层依据（推导: run/GATE-DERIVE-01/REPORT.md；落地裁决: run/GATE-WCS-01）:
//   * K2 **截断项恒等于 0**: TAN 闭式实现下 G∘F = I 可逐式证明（精确算术），
//     往返误差 **100% 来自 FP64 舍入**。判别实验: 80-bit 扩展精度复算误差随
//     eps 线性缩小（实测比值 1792–2238，理论 eps_FP64/eps_FP80 = 2048）。
//   * K3 **主项解析式**: ε ≈ C_env·u·sec²Δ/s_rad，u = 2⁻⁵³，
//     C_env = 78（AD 一阶包络实测 max）/ 128（本表采用的设计常数），
//     sec²Δ = 1 + r_plane² ≤ 1 + (FOV_rad/2)² = 1.03046（FOV ≤ 20°）。
//   * K4 **1e-8 px 不是全域保守门**: 仅在 s ≥ 0.179″/px（实测常数）/
//     0.293″/px（设计常数）时它才 ≥ 最坏情况包络（sec²Δ≈1 形式；按 FOV=20° 的
//     sec²Δ=1.03046 取最坏则为 0.184″/px / 0.302″/px）；FIX-406 自己用的最坏工况
//     0.18″/px 上余量仅 1.01×（独立区间算术包络 1.78e-8 px ⇒ 0.56× ⇒ 会误拒）。
//   ⇒ 紧门必须配适用域下限 min_scale_arcsec = 0.9″/px（覆盖仓内最小真实尺度
//     0.9586″/px，即 T2/T3 档）。尺度低于下限时**该门不适用**（明确报「超出
//     适用域」），**不判红**——判红会误拒合法几何（GATE-WCS-01 裁决 1）。
//   * 全域保守门 1e-6 px（SCI-WCS-001 §11 STD-F1）保留: 保守性下界
//     s ≥ 1.79e-3″/px（实测常数）/ 2.93e-3″/px（设计常数）⇒ 覆盖所有真实仪器；
//     代价是判据力弱（0.18″/px 处相对包络余量 101×）。两门用途不同, **不合并**。
//   * 合成规则 = **最坏情况包络（线性相加）**，不用 RSS（实测包络/RSS = 2.47×，
//     用 RSS 设门必然误拒；RSS 只回答「误差通常多大」）。
//
// roundtrip_tol_px = 1e-8 px —— FIX-406 Oracle 实验表（冻结）+ GATE-DERIVE-01 复核:
//   * 规范来源: ASTROCS_DESIGN §5.3「每种投影必须声明适用域（含往返误差上界），
//     违反 ⇒ 拒绝」；上界必须由实验确定（GAP_AUDIT G3-6 要求经 SCI 复核后冻结）。
//   * TAN 全域实测（本生产实现自身，880 组几何 × 密集逐像素 = 8.31e6 次往返 +
//     FOV=20° 适用域边界 512²/1024²/2048² = 2.42e7 次往返）:
//     max = 2.437e-9 px（最坏工况 **0.18″/px**（= 5e-5 deg/px）、|CRVAL2|=85°、
//     PA=30°、129²）。**更正**: 本注释旧版把该工况写成「0.05″/px」，与 FIX-406
//     扫描表的 5e-5 deg/px 差 3.6×，且 0.18″/px 恰是 K4 的临界尺度 ⇒ 必须更正
//     （GATE-DERIVE-01 §6.4 / GATE-WCS-01 裁决 6）。
//   * 独立 Oracle 可达精度: astropy 7.0.1 / WCSLIB 8.4 与独立切基式 ~2.5e-10 px
//     （对拍同一 WCS；SIN 内核同类问题的判定见 run/FIX-406/SIN_ROUNDTRIP_ORACLE.md）。
//   * 冻结值 1e-8 px = 实测最坏 4.1× / Oracle 可达 40×；相对旧值 1e-6 px 是
//     **收紧**（旧门在实测面前过松 ~400×），不是放宽。
//   * 余量（解析包络 C_env=128，FOV≤20°）: 0.9″/px → 2.98×；0.9586″/px（仓内最小
//     真实尺度）→ 3.17×；0.18″/px（临界）→ 0.60× ⇒ 正是适用域下限的由来。
//   * 证据与复跑: run/FIX-406/SIN_ROUNDTRIP_ORACLE.md、evidence/kernel_roundtrip.json、
//     run/FIX-406/probe_sin_roundtrip.cpp、run/FIX-406/oracle_sin_roundtrip.py；
//     推导与数值实验: run/GATE-DERIVE-01/REPORT.md（27 条自洽断言全 PASS）。
const P3WcsApplicability kTanApplicability = {
    "TAN",   // projection
    85.0,    // max_abs_crval_dec_deg（SCI/API/session 单一条件）
    20.0,    // max_fov_deg（SCI §9a-12 alpha 冻结, 禁放宽）
    true,    // require_negative_det_cd（G1/SCI §9a-4 手性冻结）
    true,    // crpix_fits_1based_pixel_center（Paper I §2.1.1）
    1e-8,    // roundtrip_tol_px（紧门; 适用域 = scale ≥ min_scale_arcsec）
    1e-6,    // roundtrip_tol_global_px（全域保守门 SCI-WCS-001 §11 STD-F1）
    0.9,     // min_scale_arcsec（紧门适用域下限; 覆盖仓内最小真实尺度 0.9586″/px）
    128.0,   // envelope_c_env（解析包络设计常数; AD 一阶包络实测 max = 78）
};

// FP64 单位舍入 u = 2⁻⁵³（round-to-nearest 的相对误差上界 = ½ulp；Higham 2002）。
constexpr double kUnitRoundoff = 1.1102230246251565e-16;   // 2^-53
// rad ↔ arcsec 换算（与 GATE-DERIVE-01 §1.1 同值）。
constexpr double kArcsecPerRad = 206264.80624709636;
constexpr double kArcsecPerDeg = 3600.0;

}  // namespace

const P3WcsApplicability* p3_wcs_applicability(const char* projection) {
    const char* c = projection ? projection : "TAN";
    if (std::strcmp(c, "TAN") == 0) return &kTanApplicability;
    return nullptr;   // 未声明适用域 ⇒ 不可作产品声明（fail-closed）
}

double p3_wcs_fov_deg(double scale_deg_per_px, int width_px, int height_px) {
    if (!(scale_deg_per_px > 0.0) || width_px < 1 || height_px < 1) return -1.0;
    const double w = static_cast<double>(width_px);
    const double h = static_cast<double>(height_px);
    return scale_deg_per_px * std::sqrt(w * w + h * h);
}

double p3_wcs_scale_arcsec_per_px(const P3WcsDescriptor* d) {
    if (!d) return -1.0;
    const double det = d->cd[0][0] * d->cd[1][1] - d->cd[0][1] * d->cd[1][0];
    const double s = std::sqrt(std::fabs(det)) * kArcsecPerDeg;   // ″/px
    if (!(s > 0.0) || !std::isfinite(s)) return -1.0;
    return s;
}

double p3_wcs_roundtrip_envelope_px(double scale_arcsec_per_px, double fov_deg) {
    if (!(scale_arcsec_per_px > 0.0) || !(fov_deg > 0.0)) return 0.0;
    const P3WcsApplicability* ap = p3_wcs_applicability("TAN");
    const double c_env = (ap != nullptr) ? ap->envelope_c_env : 0.0;
    if (!(c_env > 0.0)) return 0.0;
    // sec²Δ = 1 + r_plane², r_plane ≤ FOV_rad/2（GATE-DERIVE-01 §1.3 精确式）
    const double fov_rad = fov_deg * kRad;
    const double half_fov_rad = 0.5 * fov_rad;
    const double sec2 = 1.0 + half_fov_rad * half_fov_rad;         // ≤ 1.03046 @FOV=20°
    const double s_rad = scale_arcsec_per_px / kArcsecPerRad;      // rad/px
    return c_env * kUnitRoundoff * sec2 / s_rad;                   // px
}

P3WcsRoundtripGate p3_wcs_roundtrip_gate(const P3WcsDescriptor* d) {
    P3WcsRoundtripGate g{};
    g.status = P3_WCS_RT_GATE_OUT_OF_DOMAIN;
    const P3WcsApplicability* ap = p3_wcs_applicability(
        (d && d->projection && *d->projection) ? d->projection : "TAN");
    if (d == nullptr || ap == nullptr) return g;
    const double scale = p3_wcs_scale_arcsec_per_px(d);
    if (!(scale > 0.0)) return g;
    const double fov = p3_wcs_fov_deg(std::sqrt(std::fabs(
                                          d->cd[0][0] * d->cd[1][1] -
                                          d->cd[0][1] * d->cd[1][0])),
                                      d->width_px, d->height_px);
    g.scale_arcsec_per_px = scale;
    g.min_scale_arcsec = ap->min_scale_arcsec;
    g.envelope_px = p3_wcs_roundtrip_envelope_px(scale, fov);
    if (scale >= ap->min_scale_arcsec) {
        g.status = P3_WCS_RT_GATE_TIGHT;
        g.tol_px = ap->roundtrip_tol_px;
    } else if (g.envelope_px <= ap->roundtrip_tol_global_px) {
        // 紧门超出适用域, 但全域保守门仍 ≥ 包络 ⇒ 退回全域门（不放空判据）
        g.status = P3_WCS_RT_GATE_GLOBAL;
        g.tol_px = ap->roundtrip_tol_global_px;
    } else {
        g.tol_px = 0.0;   // 两门均无保守性证据 ⇒ 报「超出适用域」, 不判红
    }
    g.margin = (g.tol_px > 0.0 && g.envelope_px > 0.0) ? g.tol_px / g.envelope_px
                                                       : 0.0;
    return g;
}

P3WcsStatus p3_wcs_roundtrip_max_error_px(const P3WcsDescriptor* d,
                                          double* max_err_px) {
    if (!d || !max_err_px) return P3_WCS_PARAM;
    if (d->width_px < 1 || d->height_px < 1) return P3_WCS_PARAM;
    const double xs[3] = {0.0, (d->width_px - 1) / 2.0,
                          static_cast<double>(d->width_px - 1)};
    const double ys[3] = {0.0, (d->height_px - 1) / 2.0,
                          static_cast<double>(d->height_px - 1)};
    double worst = 0.0;
    int n_ok = 0;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double ra = 0.0, dec = 0.0, x = 0.0, y = 0.0;
            P3WcsStatus st = p3_wcs_pix2world(d, xs[i], ys[j], &ra, &dec);
            if (st != P3_WCS_OK) return st;   // 采样点越投影域(半球) ⇒ fail-closed
            // 世界域外采样点(|dec|>85°, world2pix 冻结守卫, 如中心恰在 85° 时
            // 帧上缘越过 85°)按定义不可往返, 不计入误差; 域内点仍逐点判定。
            // 参考像素恒映射到 CRVAL2(|CRVAL2|≤85°) ⇒ 恒有域内采样点。
            if (std::fabs(dec) > kMaxAbsDec) continue;
            st = p3_wcs_world2pix(d, ra, dec, &x, &y);
            if (st != P3_WCS_OK) return st;
            ++n_ok;
            const double e = std::hypot(x - xs[i], y - ys[j]);
            if (e > worst) worst = e;
        }
    }
    if (n_ok < 1) return P3_WCS_PARAM;   // 无域内采样点 ⇒ fail-closed(兜底)
    *max_err_px = worst;
    return P3_WCS_OK;
}

namespace {

// 密集域扫描参数（确定性、无全局状态）:
//   * 全帧均匀网格 ≤ 128×128（覆盖帧内任意位置, 含最坏像素邻域）
//   * 四边各 64 点 + 四角 + 中心（9 点集是密集域的子集 ⇒ dense ≥ nine）
//   * 1024 个固定 seed 伪随机点（xorshift64*, seed 固定 ⇒ 逐位可复现）
constexpr int kDenseGridMaxSide = 128;
constexpr int kDenseEdgeSamples = 64;
constexpr int kDenseRandomSamples = 1024;

inline std::uint64_t dense_xorshift64(std::uint64_t* s) {
    std::uint64_t x = *s;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *s = x;
    return x;
}

inline double dense_uniform01(std::uint64_t* s) {
    return static_cast<double>(dense_xorshift64(s) >> 11) * (1.0 / 9007199254740992.0);
}

}  // namespace

P3WcsStatus p3_wcs_roundtrip_dense_max_error_px(const P3WcsDescriptor* d,
                                                double* max_err_px,
                                                int* n_sampled) {
    if (!d || !max_err_px) return P3_WCS_PARAM;
    if (d->width_px < 1 || d->height_px < 1) return P3_WCS_PARAM;
    const double w = static_cast<double>(d->width_px);
    const double h = static_cast<double>(d->height_px);
    double worst = 0.0;
    int n_ok = 0;

    // 采样点累积器: 逐点往返并就地判定（不物化全部点, 省内存）
    auto probe = [&](double x, double y) -> P3WcsStatus {
        double ra = 0.0, dec = 0.0, x2 = 0.0, y2 = 0.0;
        P3WcsStatus st = p3_wcs_pix2world(d, x, y, &ra, &dec);
        if (st != P3_WCS_OK) return st;   // 采样点越投影域(半球) ⇒ fail-closed
        if (std::fabs(dec) > kMaxAbsDec) return P3_WCS_OK;   // 世界域外, 不可往返
        st = p3_wcs_world2pix(d, ra, dec, &x2, &y2);
        if (st != P3_WCS_OK) return st;
        ++n_ok;
        const double e = std::hypot(x2 - x, y2 - y);
        if (e > worst) worst = e;
        return P3_WCS_OK;
    };

    // 1) 全帧均匀网格（含四角/中心; 9 点集的角点与中心由此覆盖）
    const int nx = (d->width_px < kDenseGridMaxSide) ? d->width_px : kDenseGridMaxSide;
    const int ny = (d->height_px < kDenseGridMaxSide) ? d->height_px : kDenseGridMaxSide;
    for (int i = 0; i < nx; ++i) {
        const double x = (w - 1.0) * static_cast<double>(i) /
                         static_cast<double>(nx > 1 ? nx - 1 : 1);
        for (int j = 0; j < ny; ++j) {
            const double y = (h - 1.0) * static_cast<double>(j) /
                             static_cast<double>(ny > 1 ? ny - 1 : 1);
            const P3WcsStatus st = probe(x, y);
            if (st != P3_WCS_OK) return st;
        }
    }
    // 2) 四边（含 9 点集的边中点）+ 四角 + 中心（显式补齐, 保证超集性质）
    for (int i = 0; i <= kDenseEdgeSamples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kDenseEdgeSamples);
        const double xs[4] = {0.0, w - 1.0, t * (w - 1.0), t * (w - 1.0)};
        const double ys[4] = {t * (h - 1.0), t * (h - 1.0), 0.0, h - 1.0};
        for (int k = 0; k < 4; ++k) {
            const P3WcsStatus st = probe(xs[k], ys[k]);
            if (st != P3_WCS_OK) return st;
        }
    }
    {
        const double anchors[5][2] = {{(w - 1.0) / 2.0, (h - 1.0) / 2.0},
                                      {0.0, 0.0},
                                      {w - 1.0, 0.0},
                                      {0.0, h - 1.0},
                                      {w - 1.0, h - 1.0}};
        for (const auto& a : anchors) {
            const P3WcsStatus st = probe(a[0], a[1]);
            if (st != P3_WCS_OK) return st;
        }
    }
    // 3) 固定 seed 确定性伪随机点（覆盖网格未命中的位置）
    {
        std::uint64_t seed = 0x9E3779B97F4A7C15ULL;   // 固定 seed（逐位可复现）
        for (int k = 0; k < kDenseRandomSamples; ++k) {
            const double x = dense_uniform01(&seed) * (w - 1.0);
            const double y = dense_uniform01(&seed) * (h - 1.0);
            const P3WcsStatus st = probe(x, y);
            if (st != P3_WCS_OK) return st;
        }
    }
    if (n_ok < 1) return P3_WCS_PARAM;   // 无域内采样点 ⇒ fail-closed(兜底)
    *max_err_px = worst;
    if (n_sampled) *n_sampled = n_ok;
    return P3_WCS_OK;
}

P3WcsStatus p3_wcs_check_applicability(const P3WcsDescriptor* d,
                                       std::string* why) {
    if (!d) {
        if (why) *why = "applicability: null descriptor";
        return P3_WCS_PARAM;
    }
    const char* pj = (d->projection && *d->projection) ? d->projection : "TAN";
    const P3WcsApplicability* ap = p3_wcs_applicability(pj);
    if (ap == nullptr) {
        if (why)
            *why = std::string("applicability: projection '") + pj +
                   "' has no declared applicability domain";
        return P3_WCS_UNSUPPORTED;
    }
    if (!(std::fabs(d->crval_dec_deg) <= ap->max_abs_crval_dec_deg)) {
        if (why)
            *why = std::string("applicability: |CRVAL2| exceeds ") +
                   std::to_string(ap->max_abs_crval_dec_deg) + " deg";
        return P3_WCS_PARAM;
    }
    const double det = d->cd[0][0] * d->cd[1][1] - d->cd[0][1] * d->cd[1][0];
    if (!(std::fabs(det) > 1e-300)) {
        if (why) *why = "applicability: degenerate CD (det=0)";
        return P3_WCS_PARAM;
    }
    if (ap->require_negative_det_cd && !(det < 0.0)) {
        if (why) *why = "applicability: chirality violated (det(CD) >= 0)";
        return P3_WCS_PARAM;
    }
    const double scale = std::sqrt(std::fabs(det));
    const double fov = p3_wcs_fov_deg(scale, d->width_px, d->height_px);
    if (!(fov <= ap->max_fov_deg)) {
        if (why)
            *why = std::string("applicability: FOV ") + std::to_string(fov) +
                   " deg exceeds " + std::to_string(ap->max_fov_deg) + " deg";
        return P3_WCS_PARAM;
    }
    if (ap->crpix_fits_1based_pixel_center) {
        const double cx = (d->width_px + 1) / 2.0;
        const double cy = (d->height_px + 1) / 2.0;
        if (d->crpix_x != cx || d->crpix_y != cy) {
            if (why)
                *why = "applicability: CRPIX is not the FITS 1-based pixel center "
                       "(W+1)/2,(H+1)/2";
            return P3_WCS_PARAM;
        }
    }
    // 往返门: **尺度感知 + 分层**（紧门 1e-8 px / 全域保守门 1e-6 px /
    // 两门均超出适用域 ⇒ 不判红, 明确报「超出适用域」）。
    const P3WcsRoundtripGate gate = p3_wcs_roundtrip_gate(d);
    if (gate.status == P3_WCS_RT_GATE_OUT_OF_DOMAIN) {
        // 明确报「超出适用域」, 但**不是**失败: 闭式 TAN 在该尺度仍正确, 只是没有
        // 可用的保守门（判红会误拒; GATE-WCS-01 裁决 1）。
        if (why)
            *why = std::string("applicability: roundtrip gate OUT OF APPLICABILITY "
                               "DOMAIN: scale ") +
                   std::to_string(gate.scale_arcsec_per_px) +
                   " arcsec/px < min_scale " +
                   std::to_string(ap->min_scale_arcsec) +
                   " arcsec/px and analytic envelope " +
                   std::to_string(gate.envelope_px) +
                   " px exceeds the global conservative tolerance " +
                   std::to_string(ap->roundtrip_tol_global_px) +
                   " px (not a failure: no conservative gate exists at this scale)";
        return P3_WCS_OK;
    }
    double err = 0.0;
    int n_sampled = 0;
    const P3WcsStatus rst =
        p3_wcs_roundtrip_dense_max_error_px(d, &err, &n_sampled);
    if (rst != P3_WCS_OK) {
        if (why) *why = "applicability: roundtrip sample outside projection domain";
        return rst;
    }
    if (!(err < gate.tol_px)) {
        if (why)
            *why = std::string("applicability: roundtrip error ") +
                   std::to_string(err) + " px exceeds " +
                   std::to_string(gate.tol_px) + " px (" +
                   ((gate.status == P3_WCS_RT_GATE_TIGHT)
                        ? std::string("tight gate, scale >= min_scale")
                        : std::string("global conservative gate")) +
                   ", dense domain n=" + std::to_string(n_sampled) + ")";
        return P3_WCS_PARAM;
    }
    if (why) why->clear();
    return P3_WCS_OK;
}

P3WcsStatus p3_wcs_pix2world(const P3WcsDescriptor* d, double x, double y,
                             double* ra_deg, double* dec_deg) {
    if (!d || !ra_deg || !dec_deg) return P3_WCS_PARAM;
    // 中间坐标 ξ,η(deg): CD·(pix − crpix)
    const double dx = fits_pixel_1based(x) - d->crpix_x;   // 导出边界桥接 (STD-F1)
    const double dy = fits_pixel_1based(y) - d->crpix_y;
    const double xi = (d->cd[0][0] * dx + d->cd[0][1] * dy) * kRad;   // rad
    const double eta = (d->cd[1][0] * dx + d->cd[1][1] * dy) * kRad;
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double r = std::sqrt(xi * xi + eta * eta);
    if (r >= M_PI / 2.0) return P3_WCS_HEMISPHERE;    // 跨 TAN 半球
    const double theta = std::atan2(1.0, r);          // = atan(1/r)
    const double phi = std::atan2(-xi, eta);          // 自 +dec 轴向 −RA
    const double sd = std::sin(d0), cd = std::cos(d0);
    const double st = std::sin(theta), ct = std::cos(theta);
    const double sp = std::sin(phi), cp = std::cos(phi);
    const double dec = std::asin(st * sd + ct * cd * cp);
    // Calabretta & Greisen: Δα = arg(−cosθ·sinφ, sinθ·cosθ0 + cosθ·sinθ0·cosφ)
    const double dra = std::atan2(-ct * sp, cd * st - sd * ct * cp);
    double ra_deg_out = (a0 + dra) * kDeg;
    normalize_ra(&ra_deg_out);
    *ra_deg = ra_deg_out;
    *dec_deg = dec * kDeg;
    return P3_WCS_OK;
}

P3WcsStatus p3_wcs_world2pix(const P3WcsDescriptor* d, double ra_deg, double dec_deg,
                             double* x, double* y) {
    if (!d || !x || !y) return P3_WCS_PARAM;
    if (std::fabs(dec_deg) > kMaxAbsDec) return P3_WCS_PARAM;   // 极点邻域拒
    const double a0 = d->crval_ra_deg * kRad;
    const double d0 = d->crval_dec_deg * kRad;
    const double a = ra_deg * kRad;
    const double dd = dec_deg * kRad;
    const double denom = std::sin(d0) * std::sin(dd) + std::cos(d0) * std::cos(dd) *
                                                          std::cos(a - a0);
    if (denom <= 0.0) return P3_WCS_HEMISPHERE;       // 背面(跨半球)
    const double xi = std::cos(dd) * std::sin(a - a0) / denom;            // rad
    const double eta = (std::sin(dd) * std::cos(d0) -
                        std::cos(dd) * std::sin(d0) * std::cos(a - a0)) / denom;
    const double xid = xi * kDeg, etad = eta * kDeg;
    // 线性解 CD·δ = (ξ,η): δ = CD⁻¹·(ξ,η)
    const double det = d->cd[0][0] * d->cd[1][1] - d->cd[0][1] * d->cd[1][0];
    if (std::fabs(det) < 1e-300) return P3_WCS_PARAM;
    const double dx = (d->cd[1][1] * xid - d->cd[0][1] * etad) / det;
    const double dy = (-d->cd[1][0] * xid + d->cd[0][0] * etad) / det;
    *x = fits_pixel_0based(dx + d->crpix_x);   // 逆桥接 → 0-based 像素
    *y = fits_pixel_0based(dy + d->crpix_y);
    return P3_WCS_OK;
}

std::string p3_wcs_fits_keywords(const P3WcsDescriptor* d) {
    if (!d) return {};
    // 投影由已校验 descriptor 决定 (B2-A4): 未实现投影不产关键词 (fail-closed,
    // 绝不输出与请求不符的 CTYPE)。TAN 分支字节与旧硬编码完全一致。
    const char* pj = (d->projection && *d->projection) ? d->projection : "TAN";
    if (std::string(pj) != "TAN") return {};
    const std::string ctype1 = std::string("RA---") + pj;
    const std::string ctype2 = std::string("DEC--") + pj;
    char buf[128];
    std::string out;
    auto add = [&](const std::string& line) { out += line + "\n"; };
    add("CTYPE1= '" + ctype1 + "'");
    add("CTYPE2= '" + ctype2 + "'");
    add("CUNIT1 = 'deg'");
    add("CUNIT2 = 'deg'");
    std::snprintf(buf, sizeof(buf), "CRPIX1 = %.10f", d->crpix_x); add(buf);
    std::snprintf(buf, sizeof(buf), "CRPIX2 = %.10f", d->crpix_y); add(buf);
    std::snprintf(buf, sizeof(buf), "CRVAL1 = %.10f", d->crval_ra_deg); add(buf);
    std::snprintf(buf, sizeof(buf), "CRVAL2 = %.10f", d->crval_dec_deg); add(buf);
    std::snprintf(buf, sizeof(buf), "CD1_1 = %.12e", d->cd[0][0]); add(buf);
    std::snprintf(buf, sizeof(buf), "CD1_2 = %.12e", d->cd[0][1]); add(buf);
    std::snprintf(buf, sizeof(buf), "CD2_1 = %.12e", d->cd[1][0]); add(buf);
    std::snprintf(buf, sizeof(buf), "CD2_2 = %.12e", d->cd[1][1]); add(buf);
    return out;
}

}  // namespace astrocs::phase3
