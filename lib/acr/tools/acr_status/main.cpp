// lib/acr/tools/acr_status/main.cpp — acr-status CLI
// Phase E：显示当前硬件指纹 + 硬件画像状态。
//
// 用法：
//   acr-status [--profile <path>] [--json]
//
// 默认 --profile ./hardware-profile.json
#include "profile_reader.hpp"

#include "astro/compute/acr.hpp"
#include "astro/compute/topology.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void print_usage() {
    std::fprintf(stderr,
        "acr-status: ACR 硬件画像状态查询\n"
        "用法: acr-status [options]\n"
        "Options:\n"
        "  --profile <path>   hardware-profile.json 路径 (默认 hardware-profile.json)\n"
        "  --json             输出 JSON 格式\n"
        "  --help, -h         显示帮助\n");
}

struct Args {
    std::string profile{"hardware-profile.json"};
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

    // 硬件画像状态
    astro::compute::profile::HardwareProfileReader reader;
    reader.set_profile_path(args.profile);
    // 触发一次 get_profile 以加载画像
    (void)reader.get_profile();
    std::string status = reader.status_json();

    if (args.json) {
        std::printf("{\"hardware\":%s,\"profile\":%s}\n", hw.c_str(), status.c_str());
    } else {
        std::printf("=== ACR Status ===\n");
        std::printf("\n[Hardware]\n%s\n", hw.c_str());
        std::printf("\n[HardwareProfile]\n%s\n", status.c_str());
    }
    return 0;
}
