// lib/algorithms/projection/p3_projection.h — legacy registry v1（TAN/SIN/CAR/AIT）
// 统一签名头 — P3-001
//
// ⚠ **状态: RETIRED（SCI-FIX-PROJ 2026-09-16，ALG-P3-PROJ-IMPL-001 §15.9）**
//   本 registry 已退场：唯一在役 registry = v6 线 p3_proj_v6.h/.cpp
//   （kProjectionRegistryVersion=3，CAR/AIT CRVAL2 旋转 + AIT A≤1 + CAR native 极行
//   fail-closed）。本头文件仅为 (a) 历史测试面、(b) legacy 偏差对照证据门
//   （eng/tests/unit/v6_p3_proj/p3_proj_legacy_deviation.py 的 LEGACY_DEVIATION_TABLE）
//   保留，**禁止新消费方引用**，其行为不得再变（偏差集合只减不增）。
//
// v1 已登记偏差（相对 FITS WCS Paper II 标准；四项均保留为对照，不在此修正）:
//   D1 CAR/AIT 的 CRVAL2 不进映射（world(CRPIX) ≠ CRVAL）；
//   D2 CAR 用 Y=−θ（declination 反号）；
//   D3 AIT 缺 Paper II γ 的 √2 因子（平面尺度差 √2）；
//   D4 AIT 域判据 A<2（正确为 A≤1；接受 |X_v1|>2 rad 的折叠环带）。
//
// 权威依据（与 v6 线一致）:
//   * ASTROCS_DESIGN.md §5.3 八投影冻结集合（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）；
//   * Calabretta & Greisen (2002) FITS WCS Paper II §2.1/§2.2（旋转三 Euler 角、
//     LONPOLE 默认、各投影 native 层）；
//   * ALG-P3-PROJ-IMPL-001 §15（v1 偏差表 + v3 冻结口径）。
//   * fail-closed：未知投影码/越界 id/非法参数/空指针显式拒绝，无静默默认。
#ifndef ASTROCS_P3_PROJECTION_H
#define ASTROCS_P3_PROJECTION_H

#include <string>

namespace astrocs::phase3proj {

// ---- registry 版本（版本化 projection registry，DESIGN §5.3）----
// v1: 首批四投影 TAN/SIN/CAR/AIT（**RETIRED**，见文件头；四项偏差登记于
//     ALG-P3-PROJ-IMPL-001 §15.9）。表内容或语义不得再变；在役版本 = v6 线 v3。
constexpr int kP3ProjectionRegistryVersion = 1;
constexpr bool kP3ProjectionRegistryRetired = true;

enum class P3ProjectionId : int {
    TAN = 0,   // gnomonic 局部切平面（SCI-P3-001 alpha 唯一投影，冻结逐式）
    SIN = 1,   // orthographic（zenithal，中等视场）
    CAR = 2,   // plate carrée（cylindrical，简单圆柱）
    AIT = 3    // Aitoff（pseudo-cylindrical，宽场/全天空展示）
};

// registry 状态码（值域与 lib/phase3_session P3WcsStatus 冻结对齐，0..3）
enum class P3ProjectionStatus {
    P3_PROJ_OK = 0,
    P3_PROJ_PARAM = 1,         // 参数非法（parity/|dec|>85°/scale/尺寸/空指针/CAR |dec|>90°）
    P3_PROJ_UNSUPPORTED = 2,   // 未知投影码 / 越界 id（registry 未注册）
    P3_PROJ_HEMISPHERE = 3     // 中间坐标越投影域（TAN r≥π/2 / SIN ρ>1 / AIT D²≤0）
};

// ---- registry 记录（编译期冻结表，每投影一条）----
// 六要素声明（DESIGN §5.3）：code/ctype（经纬方向+CTYPE 规则）、
// max_abs_crval_dec_deg（适用天区/极点奇点守卫）、max_fov_deg（合法 FOV
// 声明，非 make 硬门——FOV 裁决属会话层合同，见 ALG §15）、
// pix2world/world2pix 函数指针（实现不得散落 switch，registry 驱动 dispatch）。
struct P3ProjectionDescriptor;

struct P3ProjectionSpec {
    P3ProjectionId id;
    const char* code;             // "TAN"|"SIN"|"CAR"|"AIT"（registry 键，精确匹配）
    const char* ctype1;           // FITS CTYPE1（"RA---<code>"）
    const char* ctype2;           // FITS CTYPE2（"DEC--<code>"）
    double max_abs_crval_dec_deg; // 中心 |dec| 守卫（单一条件，四投影统一 85.0 保守冻结）
    double max_fov_deg;           // 合法 FOV 声明（TAN=20 SCI §9a-12 冻结；其余 ALG §15 claim）
    P3ProjectionStatus (*pix2world)(const P3ProjectionDescriptor*, double, double,
                                    double*, double*);
    P3ProjectionStatus (*world2pix)(const P3ProjectionDescriptor*, double, double,
                                    double*, double*);
};

// ---- 投影 descriptor（与 P3WcsDescriptor 同构 + registry id）----
struct P3ProjectionDescriptor {
    double crval_ra_deg = 0;    // ICRS 中心 RA（v1 偏差 D1：CAR/AIT 仅 CRVAL1 进映射）
    double crval_dec_deg = 0;
    double crpix_x = 0;         // FITS 1-based pixel-center = (W+1)/2
    double crpix_y = 0;
    double cd[2][2] = {{0, 0}, {0, 0}};  // deg/px, FITS 顺序 CD[i][j]，CD-only（禁 PC+CDELT）
    int width_px = 0;
    int height_px = 0;
    P3ProjectionId projection = P3ProjectionId::TAN;  // registry id（非字符串——杜绝 CLI 散落）
};

// ---- registry 查询（唯一入口，fail-closed）----
// 冻结表首指针与行数（*count==4）；table 非空恒成立。
const P3ProjectionSpec* p3_projection_registry_table(int* count);
// 精确匹配投影码；未注册返回 nullptr（不 fallback，不静默）。
const P3ProjectionSpec* p3_projection_registry_find(const char* code);
// id 直查；越界返回 nullptr。
const P3ProjectionSpec* p3_projection_registry_find_id(P3ProjectionId id);
// registry 完整性自检（行数=4/码互异/CTYPE 非空/函数指针非空/版本常量=1 且
// RETIRED 标记为真）
// 全过返回 0，否则返回首个失败项的行索引+1（测试与启动自检共用）。
int p3_projection_registry_selfcheck();

// ---- 统一操作面（dispatch 经 registry 函数指针，无 CLI 散落）----
// make: 参数校验序与 TAN 冻结序一致（out 非空→parity→|dec|≤85°→scale>0→
//       W,H∈[1,20000]→G1 CD 构造→四角投影域守卫），失败时 *out 保持零初始化。
P3ProjectionStatus p3_projection_make(P3ProjectionId id,
                                      double centre_ra_deg, double centre_dec_deg,
                                      double scale_deg_per_px,
                                      int width_px, int height_px,
                                      const char* parity, double rotation_pa_deg,
                                      P3ProjectionDescriptor* out);
P3ProjectionStatus p3_projection_pix2world(const P3ProjectionDescriptor* d,
                                           double x, double y,
                                           double* ra_deg, double* dec_deg);
P3ProjectionStatus p3_projection_world2pix(const P3ProjectionDescriptor* d,
                                           double ra_deg, double dec_deg,
                                           double* x, double* y);
// FITS 关键词文本（CTYPE 经 registry spec 解析；行格式与既有
// p3_wcs_fits_keywords 同族，每行 ≤80 字节）；空 descriptor → 空串。
std::string p3_projection_fits_keywords(const P3ProjectionDescriptor* d);

}  // namespace astrocs::phase3proj

#endif  // ASTROCS_P3_PROJECTION_H
