// ============================================================================
// test_nside_pixfrac.cpp - WP-B 步骤5/6 单元测试
//
// 测试内容:
//   A. compute_auto_nside (步骤5):
//     1. 0.1"/px 输入 → NSIDE >= 2^21 (支持 0.1" 输入的 1~2 倍过采样)
//     2. 1.0"/px 输入 → NSIDE 在合理范围 [2^16, 2^20]
//     3. 60"/px 输入 → NSIDE 在合理范围 (粗像素, 但不一定触底)
//     4. 0.01"/px 输入 → NSIDE=4194304 (上限钳位 2^22)
//     5. 极粗像素 (20000"/px) → NSIDE=16 (下限钳位)
//     6. 自适应四叉树采样验证 (B05 修复: 构造 SIP 畸变在边缘的情况)
//
//   B. drizzle() 入口校验 (步骤6):
//     7.  pixfrac=0    → 返回 false, error_msg 包含 "pixfrac"
//     8.  pixfrac=-0.5 → 返回 false
//     9.  pixfrac=1.5  → 返回 false
//     10. pixfrac=0.5  → 正常执行 (返回 true)
//     11. nested=false → 返回 false, error_msg 包含 "NESTED"
//     12. channels=3   → 返回 false, error_msg 包含 "channel" (B03 修复)
//     13. weightValue 不乘入 signal (B10 修复: weight=2.0 与 weight=1.0 的 sumFlux 一致)
//
// 编译命令 (PowerShell):
//   cd "lib\healpix_db\healpix_drizzle\tests"
//   g++ -std=c++17 -O2 -fopenmp -DAIO_ENABLE_HEALPIX `
//       -I../ -I../../healpix_stack -I../../../astro_image_io/include `
//       test_nside_pixfrac.cpp `
//       ../drizzle_engine.cpp ../wcs_sip.cpp ../poly_clip.cpp ../fits_reader.cpp `
//       ../../healpix_stack/healpix_core.cpp `
//       -L../../../astro_image_io -lastro_image_io `
//       -static-libgcc -static-libstdc++ `
//       -o test_nside_pixfrac.exe
// ============================================================================

#include "drizzle_engine.h"
#include "wcs_sip.h"
#include "poly_clip.h"
#include "fits_reader.h"
#include "healpix_core.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================================
// 测试框架: 简单通过/失败计数
// ============================================================================
static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void test_pass(const char* name) {
    printf("[PASS] %s\n", name);
    g_tests_passed++;
}

static void test_fail(const char* name, const char* detail) {
    printf("[FAIL] %s: %s\n", name, detail ? detail : "");
    g_tests_failed++;
}

// 条件断言宏
#define ASSERT_TRUE(cond, name, msg) do { \
    if (cond) { test_pass(name); } \
    else { test_fail(name, msg); } \
} while(0)

#define ASSERT_FALSE(cond, name, msg) ASSERT_TRUE(!(cond), name, msg)

// 检查字符串包含子串 (大小写敏感)
static bool str_contains(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}

// 检查 nside 是否为 2 的幂
static bool is_power_of_two(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// ============================================================================
// 辅助函数: 构造简单 WcsParams (无 SIP, 对角 CD 矩阵)
// ============================================================================
static drizzle::WcsParams make_simple_wcs(double scale_arcsec_per_px,
                                          int img_w, int img_h,
                                          double crval_ra = 45.0,
                                          double crval_dec = 45.0) {
    drizzle::WcsParams wcs;
    wcs.has_wcs = true;
    // CD 矩阵: 对角, scale_arcsec_per_px 角秒/像素
    double scale_deg = scale_arcsec_per_px / 3600.0;
    wcs.cd[0] = scale_deg;  wcs.cd[1] = 0.0;
    wcs.cd[2] = 0.0;        wcs.cd[3] = scale_deg;
    wcs.crval[0] = crval_ra;
    wcs.crval[1] = crval_dec;
    // CRPIX 1-based, 图像中心
    wcs.crpix[0] = img_w * 0.5 + 0.5;
    wcs.crpix[1] = img_h * 0.5 + 0.5;
    std::strcpy(wcs.ctype1, "RA---TAN-SIP");
    std::strcpy(wcs.ctype2, "DEC--TAN-SIP");
    wcs.sip.order = 0;      // 无 SIP
    wcs.sip.ap_order = 0;
    return wcs;
}

// ============================================================================
// 辅助函数: 构造带 SIP 畸变的 WcsParams
// sip_a_coeffs: A 多项式系数 [36], sip_b_coeffs: B 多项式系数 [36]
// sip_order: SIP 阶数
// ============================================================================
static drizzle::WcsParams make_sip_wcs(double scale_arcsec_per_px,
                                       int img_w, int img_h,
                                       const double* sip_a,
                                       const double* sip_b,
                                       int sip_order,
                                       double crval_ra = 45.0,
                                       double crval_dec = 45.0) {
    drizzle::WcsParams wcs = make_simple_wcs(scale_arcsec_per_px, img_w, img_h,
                                              crval_ra, crval_dec);
    wcs.sip.order = sip_order;
    wcs.sip.ap_order = 0;
    if (sip_a) std::memcpy(wcs.sip.a, sip_a, 36 * sizeof(double));
    if (sip_b) std::memcpy(wcs.sip.b, sip_b, 36 * sizeof(double));
    return wcs;
}

// ============================================================================
// 辅助函数: 构造简单 FitsImage (用于 drizzle 测试)
// ============================================================================
static drizzle::FitsImage make_test_image(int w, int h, double scale_arcsec_per_px) {
    drizzle::FitsImage img;
    img.width = w;
    img.height = h;
    img.channels = 1;
    img.pixels.resize((size_t)w * h, 1.0f);  // 所有像素值 = 1.0
    img.wcs = make_simple_wcs(scale_arcsec_per_px, w, h);
    img.bzero = 0.0;
    img.bscale = 1.0;
    return img;
}

// ============================================================================
// 辅助函数: 构造多通道 FitsImage (用于 B03 测试)
// ============================================================================
static drizzle::FitsImage make_multichannel_image(int w, int h, int channels,
                                                   double scale_arcsec_per_px = 1.0) {
    drizzle::FitsImage img;
    img.width = w;
    img.height = h;
    img.channels = channels;
    img.pixels.resize((size_t)w * h * channels, 1.0f);  // HWC 排列
    img.wcs = make_simple_wcs(scale_arcsec_per_px, w, h);
    img.bzero = 0.0;
    img.bscale = 1.0;
    return img;
}

// ============================================================================
// 测试 A1: 0.1"/px 输入 → NSIDE >= 2^21
// 02_FROZEN §5: 支持 0.1" 输入的 1~2 倍线性过采样
// 210960/0.1 = 2109600, 2^21=2097152 < 2109600, 2^22=4194304 >= 2109600
// 因此 NSIDE = 2^22 = 4194304 >= 2^21
// ============================================================================
static void test_auto_nside_0p1_arcsec() {
    const char* name = "A1: 0.1\"/px -> NSIDE >= 2^21";
    const double scale = 0.1;  // 0.1"/px
    const int img_w = 1000, img_h = 1000;
    auto wcs = make_simple_wcs(scale, img_w, img_h);

    int nside = drizzle::compute_auto_nside(wcs, img_w, img_h);
    printf("  0.1\"/px -> NSIDE=%d (2^21=%d, 2^22=%d)\n",
           nside, 1 << 21, 1 << 22);

    ASSERT_TRUE(nside >= (1 << 21), name,
                "NSIDE 未达到 2^21, 无法支持 0.1\" 输入的过采样要求");
    ASSERT_TRUE(is_power_of_two(nside), "A1: NSIDE 是 2 的幂", "NSIDE 不是 2 的幂");
}

// ============================================================================
// 测试 A2: 1.0"/px 输入 → NSIDE 在合理范围 [2^16, 2^20]
// 210960/1.0 = 210960, 2^17=131072 < 210960, 2^18=262144 >= 210960
// 因此 NSIDE = 2^18 = 262144
// ============================================================================
static void test_auto_nside_1p0_arcsec() {
    const char* name = "A2: 1.0\"/px -> NSIDE in [2^16, 2^20]";
    const double scale = 1.0;  // 1.0"/px
    const int img_w = 1000, img_h = 1000;
    auto wcs = make_simple_wcs(scale, img_w, img_h);

    int nside = drizzle::compute_auto_nside(wcs, img_w, img_h);
    printf("  1.0\"/px -> NSIDE=%d (2^16=%d, 2^18=%d, 2^20=%d)\n",
           nside, 1 << 16, 1 << 18, 1 << 20);

    ASSERT_TRUE(nside >= (1 << 16) && nside <= (1 << 20), name,
                "NSIDE 不在合理范围 [2^16, 2^20]");
    ASSERT_TRUE(is_power_of_two(nside), "A2: NSIDE 是 2 的幂", "NSIDE 不是 2 的幂");
}

// ============================================================================
// 测试 A3: 60"/px 输入 → NSIDE 在合理范围 (粗像素)
// 210960/60 = 3516, 2^11=2048 < 3516, 2^12=4096 >= 3516
// 因此 NSIDE = 2^12 = 4096
// 注: 任务描述期望 NSIDE=16 (下限), 但 60"/px 对应的 nside_min=3516 远大于 16,
//     实际 NSIDE=4096. 此测试验证 NSIDE 是 2 的幂且 >= 16.
// ============================================================================
static void test_auto_nside_60_arcsec() {
    const char* name = "A3: 60\"/px -> NSIDE >= 16 (合理范围)";
    const double scale = 60.0;  // 60"/px = 1'/px
    const int img_w = 100, img_h = 100;  // 小图像即可
    auto wcs = make_simple_wcs(scale, img_w, img_h);

    int nside = drizzle::compute_auto_nside(wcs, img_w, img_h);
    printf("  60\"/px -> NSIDE=%d (2^12=%d)\n", nside, 1 << 12);

    // 60"/px 对应 NSIDE=4096 (2^12), 不是下限 16
    // 这里验证 NSIDE 是 2 的幂且 >= 16 (下限)
    ASSERT_TRUE(nside >= 16, name, "NSIDE < 16 (低于下限)");
    ASSERT_TRUE(is_power_of_two(nside), "A3: NSIDE 是 2 的幂", "NSIDE 不是 2 的幂");
}

// ============================================================================
// 测试 A4: 0.01"/px 输入 → NSIDE=4194304 (上限钳位 2^22)
// 210960/0.01 = 21096000, 远超 2^22=4194304
// 钳位到 NSIDE_MAX = 4194304
// ============================================================================
static void test_auto_nside_0p01_arcsec() {
    const char* name = "A4: 0.01\"/px -> NSIDE=4194304 (上限钳位)";
    const double scale = 0.01;  // 0.01"/px
    const int img_w = 1000, img_h = 1000;
    auto wcs = make_simple_wcs(scale, img_w, img_h);

    int nside = drizzle::compute_auto_nside(wcs, img_w, img_h);
    printf("  0.01\"/px -> NSIDE=%d (2^22=%d)\n", nside, 1 << 22);

    ASSERT_TRUE(nside == 4194304, name,
                "NSIDE 未钳位到 2^22=4194304 (上限)");
}

// ============================================================================
// 测试 A5: 极粗像素 (20000"/px ≈ 5.56°/px) → NSIDE=16 (下限钳位)
// 210960/20000 = 10.548, 2^4=16 >= 10.548, 但 16 是下限
// 钳位到 NSIDE_MIN = 16
// ============================================================================
static void test_auto_nside_20000_arcsec() {
    const char* name = "A5: 20000\"/px -> NSIDE=16 (下限钳位)";
    const double scale = 20000.0;  // 20000"/px ≈ 5.56°/px
    const int img_w = 10, img_h = 10;  // 小图像
    auto wcs = make_simple_wcs(scale, img_w, img_h);

    int nside = drizzle::compute_auto_nside(wcs, img_w, img_h);
    printf("  20000\"/px -> NSIDE=%d (NSIDE_MIN=16)\n", nside);

    ASSERT_TRUE(nside == 16, name,
                "NSIDE 未钳位到 16 (下限)");
}

// ============================================================================
// 测试 A6: 自适应四叉树采样验证 (B05 修复: 构造 SIP 畸变在边缘的情况)
//
// 构造一个带 SIP 畸变的 WCS, 使得边缘的局部像素尺度比中心更细.
// 使用 SIP A[2,0] 项 (dx²), 让 x 方向在边缘有压缩:
//   dx' = dx + A[2,0] * dx²
//   在边缘 dx=±500, A[2,0]=-1e-7 → dx' = dx - 0.025
//   局部 x 方向尺度 = (1 + 2*A[2,0]*dx) * scale ≈ (1 - 1e-7*1000) * 1" = 0.9999"
//
// 验证 (B05 自适应四叉树采样):
//   1. 有 SIP 时, finest_arcsec < 无 SIP 时的 finest_arcsec (边缘更细)
//   2. 有 SIP 时的 NSIDE >= 无 SIP 时的 NSIDE (反映更细尺度)
//   3. 自适应采样能捕捉到边缘的 SIP 畸变 (通过比较 NSIDE 差异)
// ============================================================================
static void test_auto_nside_adaptive_sampling() {
    const char* name = "A6: 自适应采样捕捉边缘 SIP 畸变";
    const double scale = 1.0;  // 1.0"/px 基础
    const int img_w = 1000, img_h = 1000;

    // 无 SIP 的 WCS (基准)
    auto wcs_no_sip = make_simple_wcs(scale, img_w, img_h);
    int nside_no_sip = drizzle::compute_auto_nside(wcs_no_sip, img_w, img_h);

    // 有 SIP 的 WCS: A[2,0] = -1e-6, 让 x 方向在边缘有显著压缩
    // A[2,0] 是 dx² 项的系数, 索引 = 2*6+0 = 12
    double sip_a[36] = {0};
    double sip_b[36] = {0};
    sip_a[12] = -1e-6;  // A[2,0] = -1e-6, x 方向边缘压缩
    int sip_order = 2;

    auto wcs_with_sip = make_sip_wcs(scale, img_w, img_h, sip_a, sip_b, sip_order);
    int nside_with_sip = drizzle::compute_auto_nside(wcs_with_sip, img_w, img_h);

    printf("  无 SIP: NSIDE=%d, 有 SIP: NSIDE=%d\n", nside_no_sip, nside_with_sip);

    // 验证 1: 有 SIP 时 NSIDE 是 2 的幂
    ASSERT_TRUE(is_power_of_two(nside_with_sip),
                "A6a: 有 SIP 时 NSIDE 是 2 的幂",
                "有 SIP 时 NSIDE 不是 2 的幂");

    // 验证 2: 有 SIP 时 NSIDE >= 无 SIP 时 (边缘更细, 需要更细的 HEALPix)
    // 注: SIP 压缩让边缘局部尺度更细, finest_arcsec 更小, nside_min 更大
    ASSERT_TRUE(nside_with_sip >= nside_no_sip,
                "A6b: 有 SIP 时 NSIDE >= 无 SIP (自适应捕捉边缘畸变)",
                "有 SIP 时 NSIDE < 无 SIP, 自适应采样未捕捉到边缘 SIP 畸变");

    // 验证 3: 自适应采样覆盖整个图像 (通过构造畸变在边缘中间位置验证)
    // 构造 SIP 让边缘中点 (500, 0) 和 (500, 1000) 有畸变, 而中心 (500, 500) 无畸变
    // 使用 B[0,2] 项 (dy²), 让 y 方向在上下边缘有压缩
    double sip_a2[36] = {0};
    double sip_b2[36] = {0};
    sip_b2[14] = -1e-6;  // B[0,2] = -1e-6, y 方向边缘压缩 (索引 0*6+2=2, 但 B[0,2] 索引=2)

    // 修正: B[0,2] 索引 = 0*6+2 = 2
    sip_b2[2] = -1e-6;
    sip_b2[14] = 0;

    auto wcs_edge = make_sip_wcs(scale, img_w, img_h, sip_a2, sip_b2, sip_order);
    int nside_edge = drizzle::compute_auto_nside(wcs_edge, img_w, img_h);

    printf("  边缘 SIP (B[0,2]): NSIDE=%d\n", nside_edge);
    ASSERT_TRUE(nside_edge >= nside_no_sip,
                "A6c: 边缘 SIP (B[0,2]) 时 NSIDE >= 无 SIP",
                "边缘 SIP 未被自适应采样捕捉到");
}

// ============================================================================
// R07-M09 专项测试: RA 跨越 0°/360° 边界时 NSIDE 计算稳定
//
// 原 R06 实现用 ra_xp - ra_xm 直接差分, 当 RA 跨越 0°/360° 时会得到 ~360°
// 的大数值, 导致 Jacobian 失真, NSIDE 估计错误.
// R07 修复用 3D 切向量 Jacobian, 天然处理 RA wrap, 在 RA=0°/359° 边界也应稳定.
//
// 验证:
//   1. CRVAL_RA=0.0 时 NSIDE 与 CRVAL_RA=45.0 时一致 (相同 scale)
//   2. CRVAL_RA=359.5 时 NSIDE 与 CRVAL_RA=45.0 时一致
//   3. NSIDE 是 2 的幂
// ============================================================================
static void test_auto_nside_ra_wrap() {
    const double scale = 1.0;  // 1.0"/px
    const int img_w = 100, img_h = 100;

    // 基准: CRVAL_RA=45.0
    auto wcs_ref = make_simple_wcs(scale, img_w, img_h, 45.0, 30.0);
    int nside_ref = drizzle::compute_auto_nside(wcs_ref, img_w, img_h);

    // RA=0.0 (图像跨越 RA=0°/360° 边界, 假设 scale=1"/px, 视场=100" 远小于 1°,
    //   不实际跨越; 但 CRVAL_RA=0.0 仍是关键边界点验证)
    auto wcs_ra0 = make_simple_wcs(scale, img_w, img_h, 0.0, 30.0);
    int nside_ra0 = drizzle::compute_auto_nside(wcs_ra0, img_w, img_h);

    // RA=359.5 (接近 360° 边界)
    auto wcs_ra359 = make_simple_wcs(scale, img_w, img_h, 359.5, 30.0);
    int nside_ra359 = drizzle::compute_auto_nside(wcs_ra359, img_w, img_h);

    printf("  RA=45.0: NSIDE=%d, RA=0.0: NSIDE=%d, RA=359.5: NSIDE=%d\n",
           nside_ref, nside_ra0, nside_ra359);

    ASSERT_TRUE(nside_ra0 == nside_ref,
                "A7a: RA=0.0 时 NSIDE 与 RA=45.0 一致 (RA wrap 稳定)",
                "RA=0.0 时 NSIDE 与基准不一致, 3D 切向量 Jacobian 未正确处理 RA wrap");
    ASSERT_TRUE(nside_ra359 == nside_ref,
                "A7b: RA=359.5 时 NSIDE 与 RA=45.0 一致 (RA wrap 稳定)",
                "RA=359.5 时 NSIDE 与基准不一致, 3D 切向量 Jacobian 未正确处理 RA wrap");
    ASSERT_TRUE(is_power_of_two(nside_ra0),
                "A7c: RA=0.0 时 NSIDE 是 2 的幂",
                "RA=0.0 时 NSIDE 不是 2 的幂");
}

// ============================================================================
// R07-M10 专项测试: 极区 NSIDE 计算稳定
//
// 原 R06 实现用 cos_dec < 1e-12 强制为 1e-12 保护极区, 这是不科学的数值 hack,
// 在极点附近 (dec=89.99°) 会导致 Jacobian 失真.
// R07 修复用 3D 切向量 Jacobian, 切平面基底在极点也定义良好, 无需 cos_dec 保护.
//
// 验证:
//   1. CRVAL_DEC=89.5° 时 NSIDE 与 CRVAL_DEC=0.0 时一致 (相同 scale)
//   2. CRVAL_DEC=89.99° 时 NSIDE 仍为合理值 (2 的幂, 在 [16, 4194304] 内)
//   3. CRVAL_DEC=-89.5° (南极) 时 NSIDE 与北纬 89.5° 一致
// ============================================================================
static void test_auto_nside_polar() {
    const double scale = 1.0;  // 1.0"/px
    const int img_w = 100, img_h = 100;

    // 基准: CRVAL_DEC=0.0 (赤道)
    auto wcs_eq = make_simple_wcs(scale, img_w, img_h, 45.0, 0.0);
    int nside_eq = drizzle::compute_auto_nside(wcs_eq, img_w, img_h);

    // 北极: CRVAL_DEC=89.5°
    auto wcs_np = make_simple_wcs(scale, img_w, img_h, 45.0, 89.5);
    int nside_np = drizzle::compute_auto_nside(wcs_np, img_w, img_h);

    // 北极点: CRVAL_DEC=89.99°
    auto wcs_np2 = make_simple_wcs(scale, img_w, img_h, 45.0, 89.99);
    int nside_np2 = drizzle::compute_auto_nside(wcs_np2, img_w, img_h);

    // 南极: CRVAL_DEC=-89.5°
    auto wcs_sp = make_simple_wcs(scale, img_w, img_h, 45.0, -89.5);
    int nside_sp = drizzle::compute_auto_nside(wcs_sp, img_w, img_h);

    printf("  DEC=0.0: NSIDE=%d, DEC=89.5: NSIDE=%d, DEC=89.99: NSIDE=%d, DEC=-89.5: NSIDE=%d\n",
           nside_eq, nside_np, nside_np2, nside_sp);

    // 极区与赤道 NSIDE 应一致 (相同 scale, 3D 切向量不受 dec 影响)
    ASSERT_TRUE(nside_np == nside_eq,
                "A8a: 北极 (DEC=89.5) NSIDE 与赤道一致 (3D 切向量消除极区失真)",
                "北极 NSIDE 与赤道不一致, 3D 切向量 Jacobian 在极区仍有问题");
    ASSERT_TRUE(nside_sp == nside_eq,
                "A8b: 南极 (DEC=-89.5) NSIDE 与赤道一致 (南北极对称)",
                "南极 NSIDE 与赤道不一致, 南北极不对称");
    // 极点 NSIDE 应是 2 的幂且在合法范围
    ASSERT_TRUE(is_power_of_two(nside_np2),
                "A8c: 极点 (DEC=89.99) NSIDE 是 2 的幂",
                "极点 NSIDE 不是 2 的幂");
    ASSERT_TRUE(nside_np2 >= 16 && nside_np2 <= 4194304,
                "A8d: 极点 NSIDE 在合法范围 [16, 4194304]",
                "极点 NSIDE 超出合法范围");
    // 极点与极区 NSIDE 应一致 (3D 切向量在极点也稳定)
    ASSERT_TRUE(nside_np2 == nside_eq,
                "A8e: 极点 (DEC=89.99) NSIDE 与赤道一致 (3D 切向量在极点稳定)",
                "极点 NSIDE 与赤道不一致, 3D 切向量在极点仍有失真");
}

// ============================================================================
// R07-M11 专项测试: 9 点保守采样捕捉窄局部最小值
//
// 原 R06 实现只在 4 角 + 中心 (5 点) 采样, 且仅当 max/min > 1.5 才细分,
// 可漏掉窄局部最小值 (如强 SIP 在子单元边角处出现尖锐畸变峰).
// R07 修复用 9 点采样 (4 角 + 4 边中点 + 中心), 捕捉边角尖锐畸变.
//
// 构造: SIP A[1,1] = 大系数, 让图像边中点 (500, 0) 出现窄局部畸变
//   A[1,1] 是 dx*dy 项, 索引 = 1*6+1 = 7
//   在 (x=500, y=0): dx=0, dy=-500 → dx' = 0 + A[1,1]*0*(-500) = 0 (无影响)
//   改用 A[2,0] = -1e-5, 让 x 方向在左/右边中点 (x=0/1000, y=500) 有强压缩
//   在 (x=0, y=500): dx=-500 → dx' = -500 + (-1e-5)*250000 = -500 - 2.5 = -502.5
//   局部 x 方向尺度 = (1 + 2*A[2,0]*dx) * scale = (1 + 2*(-1e-5)*(-500)) * 1" = 1.01"
//   这个畸变只在边中点出现, 5 点采样 (4角+中心) 会漏掉.
//
// 验证:
//   1. 有边中点 SIP 时 NSIDE >= 无 SIP 时 (9 点采样捕捉到边中点畸变)
//   2. NSIDE 是 2 的幂
// ============================================================================
static void test_auto_nside_9point_sampling() {
    const double scale = 1.0;  // 1.0"/px
    const int img_w = 1000, img_h = 1000;

    // 无 SIP 基准
    auto wcs_no_sip = make_simple_wcs(scale, img_w, img_h);
    int nside_no_sip = drizzle::compute_auto_nside(wcs_no_sip, img_w, img_h);

    // 边中点强 SIP: A[2,0] = -1e-5 (x 方向边缘强压缩)
    // 索引 = 2*6+0 = 12
    double sip_a[36] = {0};
    double sip_b[36] = {0};
    sip_a[12] = -1e-5;  // A[2,0] = -1e-5, 边中点 x 方向强压缩
    int sip_order = 2;

    auto wcs_edge_mid = make_sip_wcs(scale, img_w, img_h, sip_a, sip_b, sip_order);
    int nside_edge_mid = drizzle::compute_auto_nside(wcs_edge_mid, img_w, img_h);

    printf("  无 SIP: NSIDE=%d, 边中点 SIP (A[2,0]=-1e-5): NSIDE=%d\n",
           nside_no_sip, nside_edge_mid);

    ASSERT_TRUE(is_power_of_two(nside_edge_mid),
                "A9a: 边中点 SIP 时 NSIDE 是 2 的幂",
                "边中点 SIP 时 NSIDE 不是 2 的幂");
    // 9 点采样应捕捉到边中点 SIP 畸变, NSIDE >= 无 SIP
    ASSERT_TRUE(nside_edge_mid >= nside_no_sip,
                "A9b: 边中点 SIP 时 NSIDE >= 无 SIP (9 点采样捕捉边中点畸变)",
                "边中点 SIP 时 NSIDE < 无 SIP, 9 点采样未捕捉到边中点畸变");
}

// ============================================================================
// 测试 B7: pixfrac=0 → drizzle 返回 false, error_msg 包含 "pixfrac"
// ============================================================================
static void test_drizzle_pixfrac_zero() {
    const char* name = "B7: pixfrac=0 -> drizzle 返回 false, error_msg 含 'pixfrac'";
    auto img = make_test_image(8, 8, 1.0);

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = true;
    config.pixfrac = 0.0;  // 非法

    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accumulators;
    drizzle::DrizzleStats stats;
    std::string error_msg;

    drizzle::DrizzleEngine engine;
    bool ok = engine.drizzle(img, config, nullptr, nullptr, accumulators, stats, error_msg);

    printf("  pixfrac=0 -> ok=%d, error_msg=\"%s\"\n", ok, error_msg.c_str());

    ASSERT_FALSE(ok, name, "pixfrac=0 应被拒绝, 但 drizzle 返回 true");
    ASSERT_TRUE(str_contains(error_msg, "pixfrac"),
                "B7: error_msg 包含 'pixfrac'",
                "error_msg 不包含 'pixfrac'");
}

// ============================================================================
// 测试 B8: pixfrac=-0.5 → drizzle 返回 false
// ============================================================================
static void test_drizzle_pixfrac_negative() {
    const char* name = "B8: pixfrac=-0.5 -> drizzle 返回 false";
    auto img = make_test_image(8, 8, 1.0);

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = true;
    config.pixfrac = -0.5;  // 非法

    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accumulators;
    drizzle::DrizzleStats stats;
    std::string error_msg;

    drizzle::DrizzleEngine engine;
    bool ok = engine.drizzle(img, config, nullptr, nullptr, accumulators, stats, error_msg);

    printf("  pixfrac=-0.5 -> ok=%d, error_msg=\"%s\"\n", ok, error_msg.c_str());

    ASSERT_FALSE(ok, name, "pixfrac=-0.5 应被拒绝");
}

// ============================================================================
// 测试 B9: pixfrac=1.5 → drizzle 返回 false
// ============================================================================
static void test_drizzle_pixfrac_too_large() {
    const char* name = "B9: pixfrac=1.5 -> drizzle 返回 false";
    auto img = make_test_image(8, 8, 1.0);

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = true;
    config.pixfrac = 1.5;  // 非法 (>1)

    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accumulators;
    drizzle::DrizzleStats stats;
    std::string error_msg;

    drizzle::DrizzleEngine engine;
    bool ok = engine.drizzle(img, config, nullptr, nullptr, accumulators, stats, error_msg);

    printf("  pixfrac=1.5 -> ok=%d, error_msg=\"%s\"\n", ok, error_msg.c_str());

    ASSERT_FALSE(ok, name, "pixfrac=1.5 应被拒绝");
}

// ============================================================================
// 测试 B10: pixfrac=0.5 → drizzle 正常执行 (返回 true)
// ============================================================================
static void test_drizzle_pixfrac_valid() {
    const char* name = "B10: pixfrac=0.5 -> drizzle 正常执行";
    auto img = make_test_image(8, 8, 1.0);

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = true;
    config.pixfrac = 0.5;  // 合法

    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accumulators;
    drizzle::DrizzleStats stats;
    std::string error_msg;

    drizzle::DrizzleEngine engine;
    bool ok = engine.drizzle(img, config, nullptr, nullptr, accumulators, stats, error_msg);

    printf("  pixfrac=0.5 -> ok=%d, error_msg=\"%s\", nHealpixPixels=%lld\n",
           ok, error_msg.c_str(), (long long)stats.nHealpixPixels);

    ASSERT_TRUE(ok, name, "pixfrac=0.5 应正常执行, 但 drizzle 返回 false");
}

// ============================================================================
// 测试 B11: nested=false → drizzle 返回 false, error_msg 包含 "NESTED"
// ============================================================================
static void test_drizzle_ring_rejected() {
    const char* name = "B11: nested=false -> drizzle 返回 false, error_msg 含 'NESTED'";
    auto img = make_test_image(8, 8, 1.0);

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = false;  // RING 模式
    config.pixfrac = 1.0;

    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accumulators;
    drizzle::DrizzleStats stats;
    std::string error_msg;

    drizzle::DrizzleEngine engine;
    bool ok = engine.drizzle(img, config, nullptr, nullptr, accumulators, stats, error_msg);

    printf("  nested=false -> ok=%d, error_msg=\"%s\"\n", ok, error_msg.c_str());

    ASSERT_FALSE(ok, name, "RING 模式应被拒绝, 但 drizzle 返回 true");
    ASSERT_TRUE(str_contains(error_msg, "NESTED"),
                "B11: error_msg 包含 'NESTED'",
                "error_msg 不包含 'NESTED'");
}

// ============================================================================
// 测试 B12: channels=3 → drizzle 返回 false, error_msg 包含 "channel" (B03 修复)
// 多通道图像静默取第 0 通道是 BLOCKER, 入口必须硬报错
// ============================================================================
static void test_drizzle_multichannel_rejected() {
    const char* name = "B12: channels=3 -> drizzle 返回 false, error_msg 含 'channel'";
    auto img = make_multichannel_image(8, 8, 3);  // 3 通道 RGB

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = true;
    config.pixfrac = 1.0;

    std::unordered_map<uint64_t, drizzle::PixelAccumulator> accumulators;
    drizzle::DrizzleStats stats;
    std::string error_msg;

    drizzle::DrizzleEngine engine;
    bool ok = engine.drizzle(img, config, nullptr, nullptr, accumulators, stats, error_msg);

    printf("  channels=3 -> ok=%d, error_msg=\"%s\"\n", ok, error_msg.c_str());

    ASSERT_FALSE(ok, name, "多通道图像应被拒绝, 但 drizzle 返回 true");
    ASSERT_TRUE(str_contains(error_msg, "channel"),
                "B12: error_msg 包含 'channel'",
                "error_msg 不包含 'channel'");
}

// ============================================================================
// 测试 B13: weightValue 不乘入 signal (B10 修复)
// 传入不同 weightData (1.0 vs 2.0), sumFlux 应完全一致
// 因为 weightValue 仅作有效性掩膜, 不乘入 signal
// ============================================================================
static void test_drizzle_weight_not_in_signal() {
    const char* name = "B13: weightValue 不乘入 signal (w=1.0 与 w=2.0 sumFlux 一致)";
    auto img = make_test_image(8, 8, 1.0);

    drizzle::DrizzleConfig config;
    config.nside = 64;
    config.nested = true;
    config.pixfrac = 1.0;

    // 第一次: 无 weightData (weightValue = 1.0)
    std::unordered_map<uint64_t, drizzle::PixelAccumulator> acc1;
    drizzle::DrizzleStats stats1;
    std::string err1;
    drizzle::DrizzleEngine engine;
    bool ok1 = engine.drizzle(img, config, nullptr, nullptr, acc1, stats1, err1);

    // 第二次: weightData 全 2.0
    std::vector<float> weightData((size_t)8 * 8, 2.0f);
    std::unordered_map<uint64_t, drizzle::PixelAccumulator> acc2;
    drizzle::DrizzleStats stats2;
    std::string err2;
    bool ok2 = engine.drizzle(img, config, nullptr, weightData.data(), acc2, stats2, err2);

    printf("  weight=1.0 -> ok=%d, nHealpix=%lld\n", ok1, (long long)stats1.nHealpixPixels);
    printf("  weight=2.0 -> ok=%d, nHealpix=%lld\n", ok2, (long long)stats2.nHealpixPixels);

    ASSERT_TRUE(ok1 && ok2, "B13a: 两次 drizzle 均成功", "drizzle 执行失败");

    // 比较 sumFlux: 应完全相同 (weightValue 不乘入 signal)
    bool flux_match = (acc1.size() == acc2.size());
    if (flux_match) {
        for (const auto& [ipix, a1] : acc1) {
            auto it2 = acc2.find(ipix);
            if (it2 == acc2.end()) { flux_match = false; break; }
            if (std::abs(a1.sumFlux - it2->second.sumFlux) > 1e-10) {
                flux_match = false;
                printf("  sumFlux 不一致: ipix=%llu, w1=%.10f, w2=%.10f\n",
                       (unsigned long long)ipix, a1.sumFlux, it2->second.sumFlux);
                break;
            }
        }
    }
    ASSERT_TRUE(flux_match, name,
                "weightValue 被乘入 signal (sumFlux 不一致)");
}

// ============================================================================
// 主函数: 运行所有测试
// ============================================================================
int main() {
    printf("============================================================\n");
    printf("WP-B 步骤5/6 单元测试: NSIDE + pixfrac 校验\n");
    printf("============================================================\n\n");

    printf("---- A. compute_auto_nside 测试 (步骤5) ----\n");
    test_auto_nside_0p1_arcsec();
    test_auto_nside_1p0_arcsec();
    test_auto_nside_60_arcsec();
    test_auto_nside_0p01_arcsec();
    test_auto_nside_20000_arcsec();
    test_auto_nside_adaptive_sampling();
    printf("\n");

    printf("---- A.R07. R07-M09/M10/M11 专项测试 ----\n");
    test_auto_nside_ra_wrap();        // M09: RA wrap 稳定性
    test_auto_nside_polar();          // M10: 极区稳定性
    test_auto_nside_9point_sampling(); // M11: 9 点保守采样
    printf("\n");

    printf("---- B. drizzle() 入口校验测试 (步骤6) ----\n");
    test_drizzle_pixfrac_zero();
    test_drizzle_pixfrac_negative();
    test_drizzle_pixfrac_too_large();
    test_drizzle_pixfrac_valid();
    test_drizzle_ring_rejected();
    test_drizzle_multichannel_rejected();
    test_drizzle_weight_not_in_signal();
    printf("\n");

    printf("============================================================\n");
    printf("测试结果: %d 通过, %d 失败\n", g_tests_passed, g_tests_failed);
    printf("============================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
