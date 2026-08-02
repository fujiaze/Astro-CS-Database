// lib/acr/tools/acr_status/main.cpp — acr-status CLI
// Phase E：显示当前硬件指纹 + 路由状态。
//
// 用法：
//   acr-status [--profile <path>] [--json]
//
// 默认 --profile ./routes.json
#include "static_router.hpp"
#include "route_profile.hpp"

#include "astro/compute/acr.hpp"
#include "astro/compute/topology.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void print_usage() {
    std::fprintf(stderr,
        "acr-status: ACR 路由状态查询\n"
        "用法: acr-status [options]\n"
        "Options:\n"
        "  --profile <path>   routes.json 路径 (默认 routes.json)\n"
        "  --json             输出 JSON 格式\n"
        "  --help, -h         显示帮助\n");
}

struct Args {
    std::string profile{"routes.json"};
    bool json{false};
    bool help{false};
};

bool parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { out.help = true; return true; }
        if (a == "--json") { out.json = true; continue; }
        if (a == "--profile") {
            if (++i >= argc) { std::fprintf(stderr, "error: --profile 缺少参数\n"); return false; }
            out.profile = argv[i];
            continue;
        }
        std::fprintf(stderr, "error: 未知参数: %s\n", a.c_str());
        return false;
    }
    return true;
}

} // anonymous namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, args)) return 1;
    if (args.help) { print_usage(); return 0; }

    // 硬件报告
    std::string hw = astro::compute::generate_hardware_report();

    // 路由状态
    astro::compute::routing::StaticRouteResolver resolver;
    resolver.set_profile_path(args.profile);
    // 触发一次 resolve 以加载 profile
    resolver.resolve(astro::compute::KernelId::Custom);
    std::string status = resolver.status_json();

    if (args.json) {
        std::printf("{\"hardware\":%s,\"routing\":%s}\n", hw.c_str(), status.c_str());
    } else {
        std::printf("=== ACR Status ===\n");
        std::printf("\n[Hardware]\n%s\n", hw.c_str());
        std::printf("\n[Routing]\n%s\n", status.c_str());
    }
    return 0;
}
