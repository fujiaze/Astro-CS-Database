// lib/algorithms/projection/p3_wcs.h — FITS-WCS 输出描述符 + TAN 投影正反变换 (ALG-P3-002) — P3-002
// 原址 lib/phase3_session/p3_wcs.h, 按 ASTROCS_DESIGN §7.1「projection」
// 行迁入本模块 (WCS 属投影域); 命名空间 astrocs::phase3 与全部公共符号名不变
// (架构重构不改科学语义/接口, ENGINEERING_SPEC §3)。
// 覆盖: CRPIX(pixel-center)/CD 关键字正确性、east_left|east_right、旋转 PA、
// RA wrap、TAN 半球守卫(输出四角同半球)、abs(dec)<=85° 极点守卫(单一条件)。
#ifndef ASTROCS_P3_WCS_H
#define ASTROCS_P3_WCS_H

#include <string>

namespace astrocs::phase3 {

struct P3WcsDescriptor {
    double crval_ra_deg = 0;      // ICRS 中心
    double crval_dec_deg = 0;
    double crpix_x = 0;           // FITS 1-based 参考像素(pixel-center)
    double crpix_y = 0;
    double cd[2][2] = {{0, 0}, {0, 0}};   // deg/px, FITS 顺序 CD[i][j]
    int width_px = 0;
    int height_px = 0;
    const char* projection = "TAN";
};

enum P3WcsStatus {
    P3_WCS_OK = 0,
    P3_WCS_PARAM = 1,          // abs(dec)>85°/W,H 越界/极点守卫
    P3_WCS_UNSUPPORTED = 2,    // projection≠TAN
    P3_WCS_HEMISPHERE = 3      // 输出跨 TAN 半球
};

/* 请求层投影/frame/coverage_output 合法性 (B2-A4/A5 单一机器源, CLI 配置面与
 * 节点面共用)。语义 = p3_session.cpp parse_request 冻结拒清单:
 *   projection   缺省 "TAN"; 仅 TAN 在本生产路径已实现, 其它(含未注册码)一律
 *                P3_WCS_UNSUPPORTED —— 不得静默映射为 TAN (ALG-P3-PROJ-IMPL-001 §15);
 *   frame        缺省 "icrs"; 非 icrs(大小写不敏感接受 "ICRS") → P3_WCS_UNSUPPORTED;
 *   coverage_output 缺省 "mask"; 非 mask → P3_WCS_PARAM。
 * 入参 nullptr = 键缺省; 非空串按值校验(空串非法)。why 可选填诊断文本。
 * 本函数不触碰任何 TAN 数值实现。 */
P3WcsStatus p3_wcs_validate_request(const char* projection, const char* frame,
                                    const char* coverage_output, std::string* why);

/* 构造描述符: scale 为 deg/px; parity: "east_left"(CD1_1<0, 默认)|"east_right"(CD1_1>0);
 * rotation_pa_deg: 天北方向相对 +y 的位置角(可选, 0=北朝上)。逐项守卫(ALG-P3-003)。
 * projection: 缺省 "TAN"(默认实参, 既有调用零改动); 非 TAN → P3_WCS_UNSUPPORTED,
 * *out 保持零初始化且不产半成品。 */
P3WcsStatus p3_wcs_make(double centre_ra_deg, double centre_dec_deg,
                        double scale_deg_per_px, int width_px, int height_px,
                        const char* parity, double rotation_pa_deg,
                        P3WcsDescriptor* out, const char* projection = "TAN");

/* pixel-center world 变换: (x,y) 为 0-based 像素坐标(FITS=+1);
 * 失败(半球外)返回非 0。 */
P3WcsStatus p3_wcs_pix2world(const P3WcsDescriptor* d, double x, double y,
                             double* ra_deg, double* dec_deg);

/* 反变换(world→pixel); abs(dec)<=85° 守卫内部隐含(切平面远离极点)。 */
P3WcsStatus p3_wcs_world2pix(const P3WcsDescriptor* d, double ra_deg, double dec_deg,
                             double* x, double* y);

/* FITS 关键字文本(CTYPE/CRPIX/CRVAL/CD/CUNIT; 含 END 前格式); 每行 80 字节内。 */
std::string p3_wcs_fits_keywords(const P3WcsDescriptor* d);

/* ---- 适用域声明（ASTROCS_DESIGN.md §5.3「每种投影必须声明适用域…违反 ⇒ 拒绝」）----
 * 声明项: |CRVAL2| 上界 / FOV 上界 / 手性 det(CD)<0 / CRPIX 用 FITS 1-based
 * 像素中心 / 往返误差上界(px) 及其**适用域**。未声明适用域的投影 → nullptr（fail-closed）。
 *
 * 往返门**分层 + 尺度感知**（依据 run/GATE-DERIVE-01/REPORT.md 的推导与
 * run/GATE-WCS-01 的落地裁决）:
 *   * roundtrip_tol_px = 1e-8 px —— **紧门** G-P1-WCS-BRIDGE。
 *     仅当 scale ≥ min_scale_arcsec 时它才 ≥ 最坏情况包络（保守性成立）；
 *     低于该尺度 ⇒ **本门不适用**（报「超出适用域」，不判红——判红会误拒）。
 *   * roundtrip_tol_global_px = 1e-6 px —— **全域保守门** G-P1-WCS-BRIDGE-GLOBAL
 *     （SCI-WCS-001 §11 STD-F1）。保守性下界 s ≥ 1.79e-3″/px（实测常数）
 *     / 2.93e-3″/px（设计常数）⇒ 覆盖所有真实仪器；代价是判据力弱。
 *   * min_scale_arcsec —— 紧门适用域下限（覆盖仓内最小真实尺度 0.9586″/px）。
 *   * envelope_c_env —— 解析包络设计常数 C_env（ε ≈ C_env·u·sec²Δ/s_rad）。
 * 两门**用途不同、不合并为一个数**；各自适用域由 p3_wcs_roundtrip_gate() 给出。 */
struct P3WcsApplicability {
    const char* projection;                  // 投影码（冻结码字面量）
    double max_abs_crval_dec_deg;            // TAN: 85.0（SCI/API/session 单一条件）
    double max_fov_deg;                      // TAN: 20.0（SCI §9a-12 alpha 冻结）
    bool require_negative_det_cd;            // true: 手性 det(CD)<0（SCI §9a-4/G1）
    bool crpix_fits_1based_pixel_center;     // true: CRPIX=(W+1)/2,(H+1)/2（Paper I §2.1.1）
    double roundtrip_tol_px;                 // 1e-8 px 紧门（适用域 = scale ≥ min_scale_arcsec；
                                             // 实测最坏 2.437e-9 px @0.18″/px）
    double roundtrip_tol_global_px;          // 1e-6 px 全域保守门（覆盖所有真实尺度）
    double min_scale_arcsec;                 // 0.9″/px 紧门适用域下限（仓内最小真实尺度 0.9586）
    double envelope_c_env;                   // 128 解析包络设计常数（实测 max 78）
};
const P3WcsApplicability* p3_wcs_applicability(const char* projection);

/* ---- 往返门适用域判定（尺度感知 + 分层；GATE-DERIVE-01 / GATE-WCS-01）----
 * 门选择（按 descriptor 的 |det(CD)| 与 FOV 判定）:
 *   scale ≥ min_scale_arcsec                       → TIGHT（紧门 1e-8 px 适用）
 *   scale <  min_scale_arcsec 且包络 ≤ 全域门值     → GLOBAL（紧门超出适用域，
 *                                                    退回全域保守门 1e-6 px）
 *   两者皆不成立                                    → OUT_OF_DOMAIN（**不判红**：
 *                                                    两门均无保守性证据，明确报
 *                                                    「超出适用域」）
 * 说明: OUT_OF_DOMAIN 不是失败——闭式 TAN 在该尺度仍然正确，只是**没有**可用的
 * 保守门；把它判红会误拒合法几何（实测区间包络在 0.18″/px 处 1.78e-8 px > 1e-8 px）。 */
enum P3WcsRoundtripGateStatus {
    P3_WCS_RT_GATE_TIGHT = 0,         // 紧门适用（scale ≥ min_scale_arcsec）
    P3_WCS_RT_GATE_GLOBAL = 1,        // 紧门超出适用域 ⇒ 退回全域保守门
    P3_WCS_RT_GATE_OUT_OF_DOMAIN = 2  // 两门均超出适用域 ⇒ 报「超出适用域」，不判红
};

struct P3WcsRoundtripGate {
    int status;                      // P3WcsRoundtripGateStatus
    double tol_px;                   // 适用门值(px)；OUT_OF_DOMAIN 时 = 0.0（不参与判定）
    double scale_arcsec_per_px;      // 像素尺度 = 3600·√|det(CD)|
    double min_scale_arcsec;         // 紧门适用域下限（声明值）
    double envelope_px;              // 解析包络 C_env·u·sec²Δ/s_rad（诊断量，非门）
    double margin;                   // tol_px / envelope_px；OUT_OF_DOMAIN 时 = 0.0
};

/* 像素尺度(″/px) = 3600·√|det(CD)|；descriptor 退化(null/非有限) → -1.0。 */
double p3_wcs_scale_arcsec_per_px(const P3WcsDescriptor* d);

/* 解析包络(px): ε_env = C_env·u·sec²Δ/s_rad（u=2⁻⁵³；sec²Δ = 1+(FOV_rad/2)²）。
 * 依据 GATE-DERIVE-01 §1.3/§2.2；TAN 闭式无截断项，误差 100% 来自 FP64 舍入。
 * 入参非法 → 0.0。 */
double p3_wcs_roundtrip_envelope_px(double scale_arcsec_per_px, double fov_deg);

/* 往返门适用域判定（门选择 + 适用门值 + 包络 + 余量）。d 为 null → OUT_OF_DOMAIN。 */
P3WcsRoundtripGate p3_wcs_roundtrip_gate(const P3WcsDescriptor* d);

/* FOV 冻结实现口径(deg): scale_deg_per_px × √(W²+H²) —— 帧对角全视场
 * （一阶角距上界, 取帧内最大角距; 保守于单边跨度, 用于 20° 适用域门）。 */
double p3_wcs_fov_deg(double scale_deg_per_px, int width_px, int height_px);

/* 往返最大误差(px): **9 点采样**(四角/边中点/中心) pixel→world→pixel。
 * ⚠ 本采样系统性低估密集域最坏值 1.41–5.57×（GATE-DERIVE-01 §5.1 A6）⇒
 * **不得单独作门证据**；门的证据面用 p3_wcs_roundtrip_dense_max_error_px()。
 * 采样点越投影域(半球) → 返回该状态且 *max_err_px 不变（fail-closed）。
 * 映射后 |dec|>85°（world2pix 冻结守卫, 世界域外）的采样点不计入误差 ——
 * 该点按定义不可往返, 非往返误差; 域内采样点逐点判定（参考像素恒在域内）。 */
P3WcsStatus p3_wcs_roundtrip_max_error_px(const P3WcsDescriptor* d,
                                          double* max_err_px);

/* 往返最大误差(px): **密集域扫描** —— 全帧均匀网格(≤128×128) + 四边(各 64 点)
 * + 四角 + 中心 + 1024 个固定 seed 确定性伪随机点；**9 点集是其子集** ⇒
 * dense_max ≥ nine_point_max（超集性质由 p3_projection_registry 测试锁定）。
 * 用于 G-P1-WCS-BRIDGE 的证据面（9 点采样低估最坏值 1.41–5.57×）。
 * 采样点越投影域(半球) → 返回该状态且 *max_err_px 不变（fail-closed）；
 * 映射后 |dec|>85° 的采样点按定义不可往返, 不计入误差（与 9 点版同语义）。
 * n_sampled（可空）回填**计入误差**的采样点数；n_sampled < 1 → P3_WCS_PARAM。 */
P3WcsStatus p3_wcs_roundtrip_dense_max_error_px(const P3WcsDescriptor* d,
                                                double* max_err_px,
                                                int* n_sampled);

/* 适用域检查(对已构造 descriptor): |CRVAL2|≤85°、FOV≤20°、det(CD)<0、
 * CRPIX=(W+1)/2 FITS 1-based 像素中心、往返 < **该尺度下适用的门值**
 * （p3_wcs_roundtrip_gate() 选择紧门 1e-8 px / 全域保守门 1e-6 px）。
 * 违规 → P3_WCS_PARAM（why 填具体项）; 未声明适用域的投影 → P3_WCS_UNSUPPORTED。
 * **例外（不判红）**: 尺度低于紧门适用域下限且全域保守门亦无保守性证据时，
 * 往返门**不适用** ⇒ 返回 P3_WCS_OK 并在 why 写明「超出适用域」（机器可读判定
 * 用 p3_wcs_roundtrip_gate()）。理由: 判红会误拒合法几何（GATE-WCS-01 裁决 1）。
 * p3_wcs_make 在返回前调用本函数: 违反适用域 ⇒ 拒绝且 *out 保持零初始化。 */
P3WcsStatus p3_wcs_check_applicability(const P3WcsDescriptor* d,
                                       std::string* why);

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_WCS_H
