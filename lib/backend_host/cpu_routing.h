// lib/backend_host/cpu_routing.h — CPU-004 (G3) 逐 kernel 自适应路由
// 规格: V6.1 控制包 CPU-004; 08_CPU_RESOURCE_ACCEPTANCE.md §4
// Runtime 按 kernel ID + workload class 读取 profile; 核对 arch/CPU/XCR0/OS ABI/quota/
// runtime/provider/benchmark schema/build IDs。无 profile、stale、损坏或 unsupported
// provider 时逐 kernel 回 baseline; 保守意味着最低 ISA, 不意味着单线程。
#ifndef ASTROCS_CPU_ROUTING_H
#define ASTROCS_CPU_ROUTING_H

#include <cstdint>
#include <string>

namespace astrocs::backend_host {

// 单 kernel 路由决策
struct KernelRoute {
    std::string kernel_id;        // 如 "calibration-pixel-transform"
    std::string provider;         // "baseline"|"avx2"|"avx512"
    uint32_t workers = 1;         // ≥1; available≥2 时 heavy 不退 1
    uint64_t block = 1;
    std::string fallback_reason;  // 空=profile 驱动; 非空=回退原因(诊断)
    std::string self_test_sha256; // 该 provider 的 self-test hash(64hex)
};

// profile 校验结果: 整体是否可信任
struct ProfileVerdict {
    bool valid = false;           // schema/host/build 一致
    std::string stale_reason;     // 非空=stale/损坏原因
};

/* 校验 v2 profile 与当前机器的一致性(CPU-004 规格):
 *  - schema == astrocs.cpu-profile/v2; 必填字段齐全(verify_profile_v2 已做结构校验);
 *  - host.arch == amd64; logical_available 与 quota_signature 与硬件 inspect 实测一致;
 *  - build.source_commit 与当前 commit 一致;
 *  - 任一不匹配 → valid=false + stale_reason(逐 kernel 回 baseline)。
 * 不抛异常; threadsafe=yes。 */
ProfileVerdict validate_profile_v2_for_machine(const std::string& profile_json,
                                               const std::string& current_commit,
                                               const std::string& hw_json);

/* 从 profile 读取单 kernel 路由决策。
 *  - profile 无效 → 全 kernel 回 baseline + workers=有效可用核(≥1, 禁退 1);
 *  - profile 有效 → 按 kernel_id 查 kernels[].provider/workers/block;
 *  - 目标 provider 不可用(如 avx512 但 CPU 不支持) → 回 baseline + fallback_reason。
 * 返回 false 表示 profile 整体不可用(调用方应按无 profile 处理)。 */
bool route_kernel_from_profile(const std::string& profile_json,
                               const std::string& kernel_id,
                               const std::string& hw_json,
                               KernelRoute* out_route);

/* 无 profile 时的保守路由: baseline + 有效可用核(available≥2 不得退 1)。 */
KernelRoute conservative_route(const std::string& kernel_id, uint32_t available_cpus);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_CPU_ROUTING_H
