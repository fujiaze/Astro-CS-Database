// lib/acr/tools/acr_report/main.cpp — acr-report CLI
// Phase E：生成 JSON 报告（hardware + routing + qualification 摘要）。
//
// 用法：
//   acr-report [--profile <path>] [--output <path>]
//
// 默认 --profile ./routes.json --output -（stdout）
#include "static_router.hpp"
#include "route_profile.hpp"

#include "astro/compute/acr.hpp"
#include "astro/compute/topology.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void print_usage() {
    std::fprintf(stderr,
        "acr-report: 生成 ACR JSON 报告\n"
        "用法: acr-report [options]\n"
        "Options:\n"
        "  --profile <path>   routes.json 路径 (默认 routes.json)\n"
        "  --output <path>    输出路径 (默认 - = stdout)\n"
        "  --help, -h         显示帮助\n");
}

struct Args {
    std::string profile{"routes.json"};
    std::string output{"-"};
    bool help{false};
};

bool parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { out.help = true; return true; }
        if (a == "--profile") {
            if (++i >= argc) { std::fprintf(stderr, "error: --profile 缺少参数\n"); return false; }
            out.profile = argv[i];
            continue;
        }
        if (a == "--output") {
            if (++i >= argc) { std::fprintf(stderr, "error: --output 缺少参数\n"); return false; }
            out.output = argv[i];
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

    std::string hw = astro::compute::generate_hardware_report();
    astro::compute::routing::StaticRouteResolver resolver;
    resolver.set_profile_path(args.profile);
    resolver.resolve(astro::compute::KernelId::Custom);
    std::string routing = resolver.status_json();

    std::string report = "{\"schema\":\"acr.report.v1\",\"hardware\":";
    report += hw;
    report += ",\"routing\":";
    report += routing;
    report += "}";

    if (args.output == "-") {
        std::printf("%s\n", report.c_str());
    } else {
        std::ofstream f(args.output, std::ios::out | std::ios::trunc);
        if (!f.is_open()) {
            std::fprintf(stderr, "error: 无法写入 %s\n", args.output.c_str());
            return 2;
        }
        f << report;
        if (!f.good()) {
            std::fprintf(stderr, "error: 写入失败\n");
            return 3;
        }
        std::fprintf(stderr, "[acr-report] 已写入 %s\n", args.output.c_str());
    }
    return 0;
}
