// ============================================================================
// ipv_params_abi_failclosed_test.cpp — IpvParams ABI 入口校验 fail-closed 锁
// (P18 / V2-N-01; 宪章 §8.6)
// ----------------------------------------------------------------------------
// 断言面 (全部为公共 C ABI 入口的真实运行时行为):
//   1) ipv_get_default_params 必须写入 struct_size=sizeof(IpvParams) 与
//      abi_version=IPV_PARAMS_ABI_VERSION;
//   2) 五个接受 const IpvParams* 的公共入口在 struct_size / abi_version
//      不匹配时必须 fail-closed: 返回 0 且 error_msg 非空并点名 ABI 不匹配;
//   3) 修复前 V2-N-01 型调用方 (ctypes 零初始化, struct_size=0) 必须被拒;
//   4) params==NULL (使用默认值) 与 ABI 自洽的 params 不得被误拒。
// 该测试与布局锁 (ipv_abi_layout_lock) 互补: 锁保证"布局不错位", 本测试保证
// "错位/旧调用方在入口即被明确拒绝", 不得静默按大结构体写小缓冲。
// ============================================================================
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ipv_api.h"

namespace {

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("  [FAIL] %s\n", what);
        ++g_failures;
    } else {
        std::printf("  [ok]   %s\n", what);
    }
}

bool has_abi_error(const IpvWcsResult& r) {
    return std::strstr(r.error_msg, "ABI") != nullptr;
}

}  // namespace

int main() {
    std::printf("=== P18 IpvParams ABI fail-closed ===\n");

    // 1) 默认值携带 ABI 自描述
    IpvParams def;
    std::memset(&def, 0, sizeof(def));
    ipv_get_default_params(&def);
    check(def.struct_size == sizeof(IpvParams),
          "ipv_get_default_params 写入 struct_size == sizeof(IpvParams)");
    check(def.abi_version == IPV_PARAMS_ABI_VERSION,
          "ipv_get_default_params 写入 abi_version == IPV_PARAMS_ABI_VERSION");
    std::printf("  sizeof(IpvParams)=%zu abi_version=%u\n",
                sizeof(IpvParams), static_cast<unsigned>(def.abi_version));

    void* solver = ipv_solve_create();
    check(solver != nullptr, "ipv_solve_create 返回非空句柄");
    if (solver == nullptr) return 1;

    const int W = 32, H = 32;
    std::vector<float> blankf(static_cast<size_t>(W) * H, 1.0f);
    std::vector<double> blankd(static_cast<size_t>(W) * H, 1.0);
    const double dets[6] = {1.0, 1.0, 1.0, 5.0, 0.0, 0.0};
    IpvWcsResult r;

    // 错配副本
    IpvParams bad_size = def;
    bad_size.struct_size = static_cast<uint32_t>(sizeof(IpvParams)) - 1u;
    IpvParams bad_abi = def;
    bad_abi.abi_version = static_cast<uint32_t>(IPV_PARAMS_ABI_VERSION) + 1u;
    IpvParams legacy;
    std::memset(&legacy, 0, sizeof(legacy));  // 修复前 ctypes 镜像 (未写 ABI 字段)

    // 2) NULL = 默认值: 不得被 ABI 门拒
    std::memset(&r, 0, sizeof(r));
    ipv_solve_from_memory(solver, blankf.data(), W, H, 180.0, 0.0, 800.0, 3.45,
                          nullptr, &r);
    check(!has_abi_error(r), "params==NULL 放行 (error_msg 非 ABI 拒绝)");

    // 3) 五个入口: struct_size 错 => fail-closed
    std::memset(&r, 0, sizeof(r));
    int rc = ipv_solve_from_memory(solver, blankf.data(), W, H, 180.0, 0.0,
                                   800.0, 3.45, &bad_size, &r);
    check(rc == 0 && has_abi_error(r), "ipv_solve_from_memory: struct_size 错 => rc=0 + ABI error");

    std::memset(&r, 0, sizeof(r));
    rc = ipv_solve(solver, "nonexistent.fits", 180.0, 0.0, 800.0, 3.45, &bad_size, &r);
    check(rc == 0 && has_abi_error(r), "ipv_solve: struct_size 错 => rc=0 + ABI error");

    std::memset(&r, 0, sizeof(r));
    rc = ipv_solve_from_detections_v1(solver, dets, 1, W, H, 180.0, 0.0, 800.0,
                                      3.45, &bad_size, &r);
    check(rc == 0 && has_abi_error(r),
          "ipv_solve_from_detections_v1: struct_size 错 => rc=0 + ABI error");

    std::memset(&r, 0, sizeof(r));
    rc = ipv_solve_from_memory_with_callback(solver, blankf.data(), W, H, 180.0,
                                             0.0, 800.0, 3.45, &bad_size, nullptr,
                                             nullptr, &r);
    check(rc == 0 && has_abi_error(r),
          "ipv_solve_from_memory_with_callback: struct_size 错 => rc=0 + ABI error");

    std::memset(&r, 0, sizeof(r));
    rc = ipv_solve_from_memory_with_callback_d(solver, blankd.data(), W, H, 180.0,
                                               0.0, 800.0, 3.45, &bad_size, nullptr,
                                               nullptr, &r);
    check(rc == 0 && has_abi_error(r),
          "ipv_solve_from_memory_with_callback_d: struct_size 错 => rc=0 + ABI error");

    // 4) abi_version 错 => fail-closed
    std::memset(&r, 0, sizeof(r));
    rc = ipv_solve_from_memory(solver, blankf.data(), W, H, 180.0, 0.0, 800.0,
                               3.45, &bad_abi, &r);
    check(rc == 0 && has_abi_error(r), "abi_version 错 => rc=0 + ABI error");

    // 5) 零初始化旧调用方 (V2-N-01 型) => fail-closed
    std::memset(&r, 0, sizeof(r));
    rc = ipv_solve_from_memory(solver, blankf.data(), W, H, 180.0, 0.0, 800.0,
                               3.45, &legacy, &r);
    check(rc == 0 && has_abi_error(r), "零初始化旧调用方 (struct_size=0) => rc=0 + ABI error");

    // 6) ABI 自洽的 params 必须通过门 (后续失败原因不得是 ABI)
    std::memset(&r, 0, sizeof(r));
    ipv_solve_from_memory(solver, blankf.data(), W, H, 180.0, 0.0, 800.0, 3.45,
                          &def, &r);
    check(!has_abi_error(r), "ABI 自洽 params 通过门 (未被误拒)");

    ipv_solve_destroy(solver);

    std::printf("=== P18 ABI fail-closed 结果: %s (%d failures) ===\n",
                g_failures == 0 ? "ALL PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
