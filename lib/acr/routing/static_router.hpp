// lib/acr/routing/static_router.hpp — 静态路由解析器（只读 profile）
// Phase E：KernelId → backend 选择，基于 routes.json 档案。
//
// 设计（控制包 06_STATIC_ROUTING_SPEC.md）：
//   1. 三态处理：
//      - Missing（无 routes.json）：纯 CPU 运行 + 警告 "未标定，使用 CPU baseline"
//      - Stale（指纹不匹配）：警告 "profile 过期" + 继续运行（不强制重新 benchmark）
//      - Corrupt（JSON 解析失败）：警告 "profile 损坏" + 纯 CPU 运行
//   2. 路由档案只读：StaticRouteResolver 不提供修改 profile 的接口
//   3. 不在线学习：每次 resolve 都从 profile 查表，不缓存"经验"
//   4. 公共 API 不暴露第三方类型
//   5. lazy load：首次 resolve 时加载 routes.json，之后内存缓存（线程安全）
#pragma once

#include "route_profile.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace astro::compute {
enum class KernelId : std::uint32_t;
}

namespace astro::compute::routing {

// ===== 路由解析结果 =====
struct RouteResolution {
    std::string backend;             // "cpu" / "cuda:0" / ...
    std::string reason;              // "profile" / "missing-profile" / "stale" / "corrupt" / "fallback"
    ProfileState profile_state{ProfileState::Missing};
    bool stale{false};               // profile 存在但指纹不匹配
    bool corrupt{false};             // profile JSON 解析失败
    bool missing{false};             // 无 routes.json
};

// ===== StaticRouteResolver =====
// 线程安全（内部 mutex 保护 lazy load）。profile 加载后只读。
class StaticRouteResolver {
public:
    StaticRouteResolver();
    ~StaticRouteResolver();

    // 指定 routes.json 路径（默认 "./routes.json"）
    // 必须在 resolve 前调用；首次 resolve 后再设置无效（返回 false）
    void set_profile_path(const std::string& path);

    // 强制重新加载 profile（用于 acr-invalidate 后重新 resolve）
    // 之后 resolve 会重新读 routes.json
    void invalidate_cache();

    // 解析 KernelId → backend
    // profile 三态在 RouteResolution 中体现
    RouteResolution resolve(KernelId kid, const std::string& precision = "fp32");

    // 当前 profile 状态（不触发加载，仅在已加载时返回 Valid；否则返回 Missing）
    ProfileState current_state() const noexcept;

    // 获取已加载 profile 的只读视图（未加载返回 nullptr）
    const RouteProfile* loaded_profile() const noexcept;

    // 生成 status JSON（acr-status 工具用）
    std::string status_json() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace astro::compute::routing
