// lib/backend_host/hardware_inspect.h — 硬件画像采集 — BENCH-001
// 输出 JSON 与 cpu_profile.schema.json 的 hardware 对象同构(vendor/family/model/stepping/
// feature_bits/xcr0/available_logical_cpus/affinity 等); available_cpus 受 affinity∩cgroup
// 约束(禁 hardware_concurrency/nproc 单独作为 worker 数)。
#ifndef ASTROCS_HARDWARE_INSPECT_H
#define ASTROCS_HARDWARE_INSPECT_H

#include <string>

namespace astrocs::backend_host {

/* 采集硬件画像并返回 JSON 文本(单文档 UTF-8)。
 * build_id: AstroCS 版本串(X.Y.Z-alpha.N+g<hash12>), 由 CLI 注入。
 * reentrant=yes; threadsafe=yes; internal_parallel=none。 */
std::string hardware_inspect_json_v1(const std::string& build_id);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_HARDWARE_INSPECT_H
