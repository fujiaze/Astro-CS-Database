// ============================================================================
// reverse_api_test.cpp - 正式 DLL 反向 Drizzle C ABI 验收 (REV-101)
//
// 动态加载 healpix_drizzle.dll, 验证:
//   1. hp_drizzle_reverse_version / capability;
//   2. hp_drizzle_reverse_run (FP64 输出): 非零信号、统计字段、守恒上界;
//   3. 真实 FP32 输入 → FP32 输出 (REV-104 回归): 非全零;
//   4. 非法输入返回明确错误码 (pixfrac=0 / 双 signal / WCS 无效)。
// ============================================================================
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "hp_drizzle_api.h"
#include "spherical_overlap.h"
#include "wcs_sip.h"
#include "healpix_core.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

using namespace drizzle;
using Vec3 = spherical::Vec3;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static const double PI = 3.14159265358979323846;

typedef int (*ReverseRunFn)(const HpReverseDrizzleInput*, void*, void*,
                            HpReverseDrizzleResult*);
typedef uint32_t (*CapFn)(void);
typedef const char* (*VerFn)(void);

int main(int argc, char** argv) {
    printf("=== 反向 Drizzle 正式 DLL C ABI 验收 ===\n");
    const char* dll_path = (argc > 1) ? argv[1]
        : "lib/healpix_db/healpix_drizzle/healpix_drizzle.dll";

#ifdef _WIN32
    HMODULE h = LoadLibraryA(dll_path);
    if (!h) {
        printf("  [FAIL] LoadLibrary 失败: %s (error=%lu)\n",
               dll_path, (unsigned long)GetLastError());
        return 1;
    }
    fprintf(stderr, "[probe] DLL loaded\n"); fflush(stderr);
    auto fn_run = (ReverseRunFn)GetProcAddress(h, "hp_drizzle_reverse_run");
    auto fn_cap = (CapFn)GetProcAddress(h, "hp_drizzle_reverse_capability");
    auto fn_ver = (VerFn)GetProcAddress(h, "hp_drizzle_reverse_version");
#else
    void* h = nullptr;
    ReverseRunFn fn_run = nullptr; CapFn fn_cap = nullptr; VerFn fn_ver = nullptr;
#endif
    CHECK(fn_run && fn_cap && fn_ver, "DLL 导出 hp_drizzle_reverse_run/capability/version");
    if (!fn_run || !fn_cap || !fn_ver) return 1;
    fprintf(stderr, "[probe] symbols ok\n"); fflush(stderr);

    const char* ver = fn_ver();
    uint32_t cap = fn_cap();
    char msg[256];
    snprintf(msg, sizeof(msg), "version=%s capability=0x%02x "
             "(球面面积+FP32+FP64+support+严格校验)", ver, cap);
    CHECK(ver && ver[0] && (cap & 0x01u) && (cap & 0x02u) &&
          (cap & 0x04u) && (cap & 0x08u) && (cap & 0x20u), msg);

    // ---- 输入: nside=1024, 64x64 @ 30\"/px, 常数 leaf signal ----
    const int NSIDE = 1024, W = 64, H = 64;
    WcsParams w;
    w.has_wcs = true;
    std::strncpy(w.ctype1, "RA---TAN-SIP", 15);
    std::strncpy(w.ctype2, "DEC--TAN-SIP", 15);
    w.crval[0] = 272.886595; w.crval[1] = -23.254083;
    w.crpix[0] = 32.5; w.crpix[1] = 32.5;
    double s = 30.0 / 3600.0;
    w.cd[0] = -s; w.cd[1] = 0.0; w.cd[2] = 0.0; w.cd[3] = s;

    healpix::HealpixCore hp(NSIDE, true);
    std::vector<int64_t> cand = hp.queryDisc(w.crval[0], w.crval[1],
        1200.0 + hp.pixelResolutionArcsec() * 4.0);
    std::vector<uint64_t> ipix;
    std::vector<float> sig32;
    std::vector<double> sig64;
    std::vector<double> support;
    for (int64_t p : cand) {
        // 仅保留中心在图像覆盖圆附近的 leaf
        double ra, dec;
        hp.pix2radec((int64_t)p, &ra, &dec);
        double d = std::acos(std::max(-1.0, std::min(1.0,
            std::sin(dec*PI/180.0) * std::sin(w.crval[1]*PI/180.0) +
            std::cos(dec*PI/180.0) * std::cos(w.crval[1]*PI/180.0) *
            std::cos((ra - w.crval[0])*PI/180.0))));
        if (d > (1200.0 / 3600.0 * PI / 180.0)) continue;
        ipix.push_back((uint64_t)p);
        sig64.push_back(1.0);
        sig32.push_back(1.0f);
        support.push_back(1.0);
    }
    fprintf(stderr, "[probe] leaves=%zu\n", ipix.size()); fflush(stderr);

    HpReverseDrizzleInput in;
    std::memset(&in, 0, sizeof(in));
    in.nside = NSIDE; in.nested = 1;
    in.target_width = W; in.target_height = H;
    in.pixfrac = 0.8; in.output_fp64 = 1; in.no_data_as_zero = 1;
    in.crval[0] = w.crval[0]; in.crval[1] = w.crval[1];
    in.crpix[0] = w.crpix[0]; in.crpix[1] = w.crpix[1];
    in.cd[0] = w.cd[0]; in.cd[1] = w.cd[1]; in.cd[2] = w.cd[2]; in.cd[3] = w.cd[3];
    in.sip_order = 0; in.sip_ap_order = 0;
    in.leaf_ipix = ipix.data(); in.n_leaf = (int64_t)ipix.size();
    in.leaf_signal_f64 = sig64.data();
    in.leaf_support = support.data();

    std::vector<double> sig_out((size_t)W*H, 0.0), cov_out((size_t)W*H, 0.0);
    HpReverseDrizzleResult res;
    int rc = fn_run(&in, sig_out.data(), cov_out.data(), &res);
    fprintf(stderr, "[probe] fp64 run rc=%d\n", rc); fflush(stderr);
    double sum = 0; int nz = 0;
    for (double v : sig_out) { sum += v; if (v > 0) nz++; }
    snprintf(msg, sizeof(msg),
             "FP64 run rc=%d n_leaf=%lld touched=%lld overlaps=%lld "
             "Σsignal=%.6g nz=%d", rc, (long long)res.n_source_leaf,
             (long long)res.n_target_pixel_touched, (long long)res.n_overlaps,
             sum, nz);
    CHECK(rc == 0 && nz > 0 && res.n_source_leaf == (int64_t)ipix.size() &&
          res.n_target_pixel_touched > 0 && res.n_overlaps > 0 &&
          res.total_signal_out <= res.total_signal_in * (1.0 + 1e-12),
          msg);
    CHECK(res.total_signal_in > 0 && res.total_covered_area_in > 0 &&
          res.total_covered_area_out > 0, "统计字段 total_signal/covered_area 填充");

    // ---- 真实 FP32 输入 → FP32 输出 (REV-104) ----
    in.output_fp64 = 0;
    in.leaf_signal_f64 = nullptr;
    in.leaf_signal_f32 = sig32.data();
    std::vector<float> sig32_out((size_t)W*H, 0.0f), cov32_out((size_t)W*H, 0.0f);
    rc = fn_run(&in, sig32_out.data(), cov32_out.data(), &res);
    fprintf(stderr, "[probe] fp32 run rc=%d\n", rc); fflush(stderr);
    float sum32 = 0; int nz32 = 0;
    for (float v : sig32_out) { sum32 += v; if (v > 0) nz32++; }
    snprintf(msg, sizeof(msg),
             "FP32→FP32 rc=%d Σsignal_f32=%.6g nz=%d (非全零, 真实 float 路径)",
             rc, sum32, nz32);
    CHECK(rc == 0 && nz32 > 0 && sum32 > 0, msg);

    // ---- 非法输入 → 明确错误码 ----
    in.output_fp64 = 1; in.leaf_signal_f32 = nullptr; in.leaf_signal_f64 = sig64.data();
    HpReverseDrizzleInput bad = in;
    bad.pixfrac = 0.0;
    int rc_bad = fn_run(&bad, sig_out.data(), cov_out.data(), &res);
    snprintf(msg, sizeof(msg), "pixfrac=0 拒绝 rc=%d err=%s",
             rc_bad, res.error_msg);
    CHECK(rc_bad != 0, msg);
    bad = in; bad.leaf_signal_f32 = sig32.data();  // f64 仍非空 → 双 signal
    rc_bad = fn_run(&bad, sig_out.data(), cov_out.data(), &res);
    CHECK(rc_bad != 0, "双 signal 提供 → 拒绝");
    bad = in; bad.crval[1] = 91.0;   // DEC 超出 [-90,90] → 拒绝
    rc_bad = fn_run(&bad, sig_out.data(), cov_out.data(), &res);
    CHECK(rc_bad != 0, "无效 WCS → 拒绝");

#ifdef _WIN32
    FreeLibrary(h);
#endif
    printf("== 反向 Drizzle DLL API: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
