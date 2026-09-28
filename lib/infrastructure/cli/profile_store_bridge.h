// lib/infrastructure/cli/profile_store_bridge.h
//
// 桥接头（DYN-740 / R-29）：让命令层在**同一个 TU** 里同时使用
//   · lib/infrastructure/benchmark/backend_host/backend_loader.h（ISA provider 预检/装载）
//   · lib/infrastructure/benchmark/backend_host/profile_store.h（cpu_profile 原子写）
// 两者都在 astrocs::backend_host 里定义了一个**同名类型 LoadResult**（前者=装载裁决，
// 后者=读取裁决），直接共存会 "redefinition of struct LoadResult" 编译失败——这不是
// 使用方的错，是两份公开头在同一命名空间重名的缺陷（已登记；正解是改名，属对方域）。
//
// 本头的**收容**手段：包含 profile_store.h 之前把它的 LoadResult 宏改名为
// ProfileLoadResult，包含后立刻 undef。安全性：非模板函数的**返回值类型不参与
// 名字修饰**，故改名只影响本 TU 的类型名解析，不改变任何符号/ABI/链接结果；
// 且宏作用域仅限本头内部的这一次包含。
#pragma once

#define LoadResult ProfileLoadResult
#include "profile_store.h"
#undef LoadResult

#include <string>

namespace astrocs::cli {

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
    const astrocs::backend_host::SaveResult sr =
        astrocs::backend_host::save_profile_atomic_v1(json_text, hw_json, current_commit,
                                                     target_path);
    ProfileSaveOutcome out;
    out.ok = sr.ok;
    out.path = sr.path;
    out.reason = sr.reason;
    return out;
}

// 用户级降级落点（CPU-007 口径：XDG / LOCALAPPDATA）。空串 = 不可判定。
inline std::string user_cpu_profile_path() {
    const astrocs::backend_host::PathResult pr =
        astrocs::backend_host::default_profile_path_v1();
    return pr.ok ? pr.path : std::string();
}

}  // namespace astrocs::cli
