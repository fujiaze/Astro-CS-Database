// lib/backend_host/profile_gen.h — cpu_profile.json 生成 (06 §5) — BENCH-004/005
#ifndef ASTROCS_PROFILE_GEN_H
#define ASTROCS_PROFILE_GEN_H

#include <string>

namespace astrocs::backend_host {

/* 生成对 cpu_profile.schema.json 有效的 profile JSON 文本。
 * mode: "quick"|"full"; build_id: X.Y.Z-alpha.N+g<hash12>; commit: 40hex;
 * backend_sha: 主 backend 文件 hash(内置 baseline=可执行文件 hash)。
 * reentrant=yes; threadsafe=yes; internal_parallel=none(串行测量)。 */
std::string generate_profile_json(const std::string& mode, const std::string& build_id,
                                  const std::string& commit, const std::string& backend_sha);

}  // namespace astrocs::backend_host

#endif  // ASTROCS_PROFILE_GEN_H
