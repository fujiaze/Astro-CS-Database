// lib/acr/qualification/focused/acr_benchmark_focused_main.cpp — 聚焦 Benchmark CLI
//
// 08 号计划 §4：只测目标像素 Operation 与基础传输，输出 OperationProfile。
//
// 用法：
//   acr-benchmark-focused [--profile quick|standard] [--gpu] [--no-gpu]
//                         [--output operation-profile.json]
#include "focused_benchmark.hpp"
#include "operation_profile.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "astro/compute/acr.hpp"

namespace {

struct Args {
    astro::compute::qualification::focused::FocusedProfileKind kind{
        astro::compute::qualification::focused::FocusedProfileKind::Standard};
    bool enable_gpu{false};
    std::string output{"operation-profile.json"};
    bool help{false};
};

bool parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { out.help = true; return true; }
        if (a == "--gpu") { out.enable_gpu = true; continue; }
        if (a == "--no-gpu") { out.enable_gpu = false; continue; }
        if (a == "--profile") {
            if (++i >= argc) return false;
            if (std::strcmp(argv[i], "quick") == 0) {
                out.kind = astro::compute::qualification::focused::
                    FocusedProfileKind::Quick;
            } else if (std::strcmp(argv[i], "standard") == 0) {
                out.kind = astro::compute::qualification::focused::
                    FocusedProfileKind::Standard;
            } else {
                std::fprintf(stderr, "error: invalid --profile: %s\n", argv[i]);
                return false;
            }
            continue;
        }
        if (a == "--output") {
            if (++i >= argc) return false;
            out.output = argv[i];
            continue;
        }
        std::fprintf(stderr, "error: unknown arg: %s\n", a.c_str());
        return false;
    }
    return true;
}

} // anonymous namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, args)) {
        std::fprintf(stderr,
            "usage: acr-benchmark-focused [--profile quick|standard] "
            "[--gpu] [--no-gpu] [--output <path>]\n");
        return 1;
    }
    if (args.help) {
        std::fprintf(stdout,
            "acr-benchmark-focused: 聚焦目标像素 Operation Benchmark\n"
            "  --profile quick|standard  档位（默认 standard）\n"
            "  --gpu / --no-gpu          启用/禁用 GPU（默认禁用）\n"
            "  --output <path>           输出 OperationProfile 路径\n");
        return 0;
    }

    astro::compute::runtime_init();
    astro::compute::qualification::focused::FocusedBenchmark bench;
    const std::size_t n = bench.run(args.kind, args.enable_gpu);
    if (n == 0) {
        std::fprintf(stderr, "focused benchmark failed\n");
        astro::compute::runtime_shutdown();
        return 2;
    }
    auto profile = bench.build_profile(args.kind);
    bench.qualify(args.kind, profile);
    std::string err;
    if (!astro::compute::qualification::focused::validate_operation_profile(
            profile, err)) {
        std::fprintf(stderr, "operation profile schema validation failed: %s\n",
                     err.c_str());
        astro::compute::runtime_shutdown();
        return 3;
    }
    if (!astro::compute::qualification::focused::
            write_operation_profile_to_file(args.output, profile)) {
        std::fprintf(stderr, "cannot write %s\n", args.output.c_str());
        astro::compute::runtime_shutdown();
        return 4;
    }
    std::fprintf(stdout,
                 "[acr-benchmark-focused] operations=%zu state=%s -> %s\n",
                 profile.operations.size(),
                 profile.profile_state.c_str(), args.output.c_str());
    astro::compute::runtime_shutdown();
    return 0;
}
