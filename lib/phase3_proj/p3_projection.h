// lib/phase3_proj/p3_projection.h — Phase3 版本化 projection registry + 冻结四投影
// (TAN/SIN/CAR/AIT) 统一签名头 — P3-001 (ASTROCS-CONSTITUTION-001 §7.3/§18.1)
//
// 冻结口径（ALG-P3-PROJ-IMPL-001 §15，docs/algorithms/PHASE3_PROJ_IMPL.md）:
//   * registry 只注册宪章 §18.1 冻结首批四投影 TAN/SIN/CAR/AIT；新增投影
//     必须经本 registry 注册并附独立往返 Oracle（宪章 §7.3 原文）。
//   * TAN(gnomonic) 数学内核逐式沿用 lib/phase3_session/p3_wcs.cpp 冻结
//     生产事实（G1 CD 构造 + G2 球面三角正反映射，SCI-P3-001 FROZEN 零改动）。
//   * SIN/CAR/AIT 为宪章 §18.1 负责人裁决新增 claim，公式 = Calabretta &
//     Greisen (2002) FITS WCS Paper II 标准定义（§15 逐式冻结），共享
//     native↔celestial 旋转核 + 各投影 native 层；TAN 为 zenithal 冻结
//     逐式路径（保证与既有生产实现 bitwise 对拍）。
//   * 每投影声明适用天区/奇点/经纬方向/CRPIX/CRVAL/CD/CTYPE 规则/合法
//     FOV/独立往返 Oracle（宪章 §7.3 六要素，spec 字段 + ALG §15）。
//   * fail-closed：未知投影码/越界 id/非法参数/空指针显式拒绝，无静默默认。
#ifndef ASTROCS_P3_PROJECTION_H
#define ASTROCS_P3_PROJECTION_H

#include <string>

namespace astrocs::phase3proj {

// ---- registry 版本（版本化 projection registry，宪章 §7.3）----
// v1: 首批冻结四投影 TAN/SIN/CAR/AIT（§18.1）。表内容或语义变化必须
//     递增此版本并在 ALG-P3-PROJ-IMPL-001 §15 登记变更 claim。
constexpr int kP3ProjectionRegistryVersion = 1;

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
// 六要素声明（宪章 §7.3）：code/ctype（经纬方向+CTYPE 规则）、
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
    double crval_ra_deg = 0;    // ICRS 中心 RA（CAR/AIT: CRVAL1=中央经线；CRVAL2 记录但不进入映射，θ₀=+90° 天球惯例）
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
// registry 完整性自检（行数=4/码互异/CTYPE 非空/函数指针非空/版本常量）
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
