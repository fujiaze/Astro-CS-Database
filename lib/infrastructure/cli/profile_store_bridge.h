// lib/infrastructure/cli/profile_store_bridge.h
//
// 桥接头：让命令层在**同一个 TU** 里同时使用
//   · lib/infrastructure/benchmark/backend_host/backend_loader.h（ISA provider 预检/装载）
//   · lib/infrastructure/benchmark/backend_host/profile_store.h（cpu_profile 原子写）
// 两份公开头曾各自定义一个**同名同型别 `LoadResult`**（前者=装载裁决，后者=读取裁决），
// 同一 TU 同时包含即 "redefinition of struct acsd::backend_host::LoadResult" 编译失败。
// 读取侧已按语义改名为 `ProfileLoadResult`（见 profile_store.h），两类型
// 现可在该命名空间共存；本头**不再需要**宏改名收容（原 `#define LoadResult ...` 段已删）。
#pragma once

#include "profile_store.h"

#include <string>

namespace acsd::cli {

// cpu_profile 落盘结果（命令层可见面；不把 backend_host 的类型语义泄漏给调用方）。
struct ProfileSaveOutcome {
    bool ok = false;
    std::string path;     // 实际落盘路径
    std::string reason;   // ok=false 时的精确原因
};

// 原子写（唯一实现 = save_profile_atomic_v1：写临时 → 校验 → rename）。
// 目录不存在时由实现侧逐级创建；位于源码树内/不可写 → ok=false + reason（不静默换落点）。
inline ProfileSaveOutcome save_cpu_profile_atomic(const std::string& json_text,
                                                 const std::string& hw_json,
                                                 const std::string& current_commit,
                                                 const std::string& target_path) {
    const acsd::backend_host::SaveResult sr =
        acsd::backend_host::save_profile_atomic_v1(json_text, hw_json, current_commit,
                                                     target_path);
    ProfileSaveOutcome out;
    out.ok = sr.ok;
    out.path = sr.path;
    out.reason = sr.reason;
    return out;
}

// 用户级降级落点（CPU-007 口径：XDG / LOCALAPPDATA）。空串 = 不可判定。
inline std::string user_cpu_profile_path() {
    const acsd::backend_host::PathResult pr =
        acsd::backend_host::default_profile_path_v1();
    return pr.ok ? pr.path : std::string();
}

}  // namespace acsd::cli
