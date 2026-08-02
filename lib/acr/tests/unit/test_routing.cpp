// lib/acr/tests/unit/test_routing.cpp — Phase E routing 单元测试
// 覆盖：
//   - profile_state_str
//   - parse_route_profile 合法/损坏 JSON
//   - fingerprint_matches（sha256 比较 + 关键字段退化）
//   - StaticRouteResolver 三态处理（Missing/Stale/Corrupt/Valid）
//   - StaticRouteResolver 只读性（无修改 API）
//   - StaticRouteResolver invalidate_cache
//   - StaticRouteResolver status_json
#include <gtest/gtest.h>

#include "static_router.hpp"
#include "route_profile.hpp"

#include <cstdio>
#include <fstream>
#include <string>

#include "astro/compute/acr.hpp"

using namespace astro::compute::routing;

namespace {

// 写测试用 routes.json
bool write_test_profile(const std::string& path,
                        const std::string& sha256 = "validsha256",
                        const std::string& schema = "acr.route_profile.v1") {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "{"
      << "\"schema_version\":\"" << schema << "\","
      << "\"generated_at\":\"20260802T120000Z\","
      << "\"profile_kind\":\"standard\","
      << "\"fingerprint\":{"
      <<   "\"cpu_model\":\"TestCPU\","
      <<   "\"cpu_cores\":8,"
      <<   "\"isa_mask\":255,"
      <<   "\"gpu_name\":\"\","
      <<   "\"gpu_memory_bytes\":0,"
      <<   "\"gpu_driver_version\":\"\","
      <<   "\"sha256\":\"" << sha256 << "\""
      << "},"
      << "\"routes\":["
      <<   "{\"kernel_id\":3,\"kernel_name\":\"AXPY\",\"precision\":\"fp32\","
      <<    "\"preferred_backend\":\"cpu\",\"expected_throughput_gbps\":10.5,\"reason\":\"only-avail\"}"
      << "]"
      << "}";
    return f.good();
}

} // anonymous namespace

// ============================================================================
// profile_state_str
// ============================================================================

TEST(RoutingState, StrRoundTrip) {
    EXPECT_STREQ(profile_state_str(ProfileState::Missing), "missing");
    EXPECT_STREQ(profile_state_str(ProfileState::Stale), "stale");
    EXPECT_STREQ(profile_state_str(ProfileState::Corrupt), "corrupt");
    EXPECT_STREQ(profile_state_str(ProfileState::Valid), "valid");
}

// ============================================================================
// parse_route_profile
// ============================================================================

TEST(RoutingParse, ValidJson) {
    std::string json = R"({
        "schema_version":"acr.route_profile.v1",
        "generated_at":"20260802T120000Z",
        "profile_kind":"standard",
        "fingerprint":{"cpu_model":"X","cpu_cores":4,"isa_mask":1,
                       "gpu_name":"","gpu_memory_bytes":0,
                       "gpu_driver_version":"","sha256":"deadbeef"},
        "routes":[{"kernel_id":1,"kernel_name":"Copy","precision":"fp32",
                   "preferred_backend":"cpu","expected_throughput_gbps":5.0,
                   "reason":"only-avail"}]
    })";
    RouteProfile p;
    ASSERT_TRUE(parse_route_profile(json, p));
    EXPECT_EQ(p.schema_version, "acr.route_profile.v1");
    EXPECT_EQ(p.profile_kind, "standard");
    EXPECT_EQ(p.fingerprint.cpu_model, "X");
    EXPECT_EQ(p.fingerprint.cpu_cores, 4u);
    EXPECT_EQ(p.routes.size(), 1u);
    EXPECT_EQ(p.routes[0].kernel_id, 1u);
    EXPECT_EQ(p.routes[0].preferred_backend, "cpu");
}

TEST(RoutingParse, MalformedJsonReturnsFalse) {
    RouteProfile p;
    EXPECT_FALSE(parse_route_profile("{not json", p));
    EXPECT_FALSE(parse_route_profile("", p));
    EXPECT_FALSE(parse_route_profile("}", p));
}

TEST(RoutingParse, UnknownFieldSkipped) {
    std::string json = R"({
        "schema_version":"acr.route_profile.v1",
        "unknown_str":"abc",
        "unknown_num":123,
        "unknown_obj":{"a":1},
        "unknown_arr":[1,2,3],
        "fingerprint":{"cpu_model":"Y","cpu_cores":2,"isa_mask":0,
                       "gpu_name":"","gpu_memory_bytes":0,
                       "gpu_driver_version":"","sha256":"z"},
        "routes":[]
    })";
    RouteProfile p;
    ASSERT_TRUE(parse_route_profile(json, p));
    EXPECT_EQ(p.fingerprint.cpu_model, "Y");
    EXPECT_EQ(p.routes.size(), 0u);
}

// ============================================================================
// fingerprint_matches
// ============================================================================

TEST(RoutingFingerprint, MatchBySha256) {
    DeviceFingerprintView a, b;
    a.sha256 = "abc";
    b.sha256 = "abc";
    EXPECT_TRUE(fingerprint_matches(a, b));
    b.sha256 = "xyz";
    EXPECT_FALSE(fingerprint_matches(a, b));
}

TEST(RoutingFingerprint, FallbackToFields) {
    DeviceFingerprintView a, b;
    // sha256 为空，走字段比较
    a.cpu_model = "X"; a.cpu_cores = 4; a.isa_mask = 1;
    b.cpu_model = "X"; b.cpu_cores = 4; b.isa_mask = 1;
    EXPECT_TRUE(fingerprint_matches(a, b));
    b.cpu_cores = 8;
    EXPECT_FALSE(fingerprint_matches(a, b));
}

// ============================================================================
// StaticRouteResolver 三态处理
// ============================================================================

TEST(RoutingResolver, MissingProfileReturnsCpuBaseline) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_routes_12345.json");
    auto res = r.resolve(astro::compute::KernelId::AXPY);
    EXPECT_TRUE(res.missing);
    EXPECT_EQ(res.backend, "cpu");
    EXPECT_EQ(res.reason, "missing-profile");
    EXPECT_EQ(res.profile_state, ProfileState::Missing);
}

TEST(RoutingResolver, CorruptProfileReturnsCpuBaseline) {
    const char* path = "acr_test_corrupt_profile.json";
    {
        std::ofstream f(path);
        f << "{ this is not valid json >>>";
    }
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res = r.resolve(astro::compute::KernelId::AXPY);
    EXPECT_TRUE(res.corrupt);
    EXPECT_EQ(res.backend, "cpu");
    EXPECT_EQ(res.reason, "corrupt");
    EXPECT_EQ(res.profile_state, ProfileState::Corrupt);
    std::remove(path);
}

TEST(RoutingResolver, ValidProfileResolvesRoute) {
    const char* path = "acr_test_valid_profile.json";
    ASSERT_TRUE(write_test_profile(path));
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res = r.resolve(astro::compute::KernelId::AXPY);
    // 注意：profile 中指纹 sha256="validsha256" 与当前机器指纹大概率不匹配 → stale
    // 但仍按 profile 路由（不回退 CPU）
    EXPECT_FALSE(res.missing);
    EXPECT_FALSE(res.corrupt);
    EXPECT_EQ(res.backend, "cpu");
    if (res.stale) {
        EXPECT_EQ(res.reason, "stale");
    } else {
        EXPECT_EQ(res.reason, "profile");
    }
    std::remove(path);
}

TEST(RoutingResolver, UnknownKernelFallsBack) {
    const char* path = "acr_test_unknown_kernel_profile.json";
    ASSERT_TRUE(write_test_profile(path));
    StaticRouteResolver r;
    r.set_profile_path(path);
    // 用 profile 中没有的 kernel
    auto res = r.resolve(astro::compute::KernelId::Mandelbrot);
    EXPECT_EQ(res.backend, "cpu");
    EXPECT_EQ(res.reason, "fallback");
    std::remove(path);
}

TEST(RoutingResolver, InvalidateCacheReloadsProfile) {
    const char* path = "acr_test_invalidate_profile.json";
    ASSERT_TRUE(write_test_profile(path));
    StaticRouteResolver r;
    r.set_profile_path(path);
    auto res1 = r.resolve(astro::compute::KernelId::AXPY);
    EXPECT_FALSE(res1.missing);
    // 删除文件后 invalidate
    std::remove(path);
    r.invalidate_cache();
    auto res2 = r.resolve(astro::compute::KernelId::AXPY);
    EXPECT_TRUE(res2.missing);
}

TEST(RoutingResolver, StatusJsonContainsFields) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_status_test.json");
    r.resolve(astro::compute::KernelId::Custom);
    std::string s = r.status_json();
    EXPECT_NE(s.find("\"profile_state\""), std::string::npos);
    EXPECT_NE(s.find("\"profile_path\""), std::string::npos);
    EXPECT_NE(s.find("\"loaded\""), std::string::npos);
}

TEST(RoutingResolver, SetPathAfterResolveIgnored) {
    StaticRouteResolver r;
    r.set_profile_path("./nonexistent_first.json");
    r.resolve(astro::compute::KernelId::Custom);
    // 已经 load_attempted，再设置路径无效
    r.set_profile_path("./different_path.json");
    auto s = r.status_json();
    // 路径应保持首次设置
    EXPECT_NE(s.find("nonexistent_first.json"), std::string::npos);
}

// ============================================================================
// main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
