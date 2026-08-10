// lib/phase2/src/coverage.cpp — Phase2 W3 coverage union 实现
#include "astro/phase2/coverage.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>

namespace {

// 简化的 NESTED 层级 parent（order 降低一层）
inline std::uint64_t parent_ipix(std::uint64_t ipix) { return ipix >> 2u; }

} // namespace

extern "C" {

int p2_coverage_build(const char* const* hips_paths,
                      std::uint64_t n_inputs,
                      P2CoverageResult* out) {
    if (out == nullptr) return 1;
    std::memset(out, 0, sizeof(*out));
    if (hips_paths == nullptr || n_inputs == 0) {
        std::strncpy(out->error, "no inputs", sizeof(out->error) - 1);
        return 1;
    }
    // 注：完整实现经 AIO HiPS reader 读取每帧 properties/tiles 计算 MOC。
    // 当前为接口冻结占位：接受合成/已缓存输入列表并做最小校验。
    // W3 完整实现随 W4 一起接入 AIO（避免先造重复 I/O 层）。
    out->n_inputs = n_inputs;
    out->target_order = -1;
    for (std::uint64_t i = 0; i < n_inputs && i < out->n_inputs; ++i) {
        if (out->inputs != nullptr) {
            P2HipsInputInfo& info = out->inputs[i];
            std::strncpy(info.hips_path, hips_paths[i],
                         sizeof(info.hips_path) - 1);
        }
    }
    out->status = 0;
    return 0;
}

int p2_coverage_free(P2CoverageResult* out) {
    if (out == nullptr) return 0;
    std::memset(out, 0, sizeof(*out));
    return 0;
}

} // extern "C"