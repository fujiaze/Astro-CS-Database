// lib/acr/tools/acr_benchmark/main.cpp — acr-benchmark CLI
// Phase E：执行微基准并生成 hardware-profile.json。
//
// 用法：
//   acr-benchmark [--profile quick|standard|full] [--output hardware-profile.json] [--gpu] [--no-gpu]
//
// 默认：--profile standard --output hardware-profile.json --no-gpu
// CLI 解析手写（避免引入 CLI11 依赖，与 hardware_report.cpp 风格一致）
#include "benchmark_driver.hpp"
#include "profile_generator.hpp"
#include "profile_schema.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "astro/compute/acr.hpp"

namespace {

void print_usage() {
    std::fprintf(stderr,
        "acr-benchmark: ACR 微基准工具\n"
        "用法: acr-benchmark [options]\n"
        "Options:\n"
        "  --profile <kind>   标定档位: quick | standard | full (默认 standard)\n"
        "  --output <path>    输出 hardware-profile.json 路径 (默认 hardware-profile.json)\n"
        "  --gpu              启用 GPU benchmark (默认禁用，CPU-only 构建强制禁用)\n"
        "  --no-gpu           禁用 GPU benchmark (默认)\n"
        "  --help, -h         显示帮助\n");
}

struct Args {
    astro::compute::qualification::ProfileKind kind{
        astro::compute::qualification::ProfileKind::Standard};
    std::string output{"hardware-profile.json"};
    bool enable_gpu{false};
    bool help{false};
};

bool parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { out.help = true; return true; }
        if (a == "--gpu") { out.enable_gpu = true; continue; }
        if (a == "--no-gpu") { out.enable_gpu = false; continue; }
        if (a == "--profile") {
            if (++i >= argc) { std::fprintf(stderr, "error: --profile 缺少参数\n"); return false; }
            if (!astro::compute::qualification::parse_profile_kind(argv[i], out.kind)) {
                std::fprintf(stderr, "error: 无效 profile kind: %s\n", argv[i]);
                return false;
            }
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

    // 初始化 ACR runtime（benchmark 用 parallel_for）
    astro::compute::runtime_init();

    // 配置 benchmark driver
    astro::compute::qualification::BenchmarkDriver driver;
    auto cfg = astro::compute::qualification::make_default_config(args.kind, args.enable_gpu);
    driver.configure(cfg);

    // 运行 benchmark
    auto results = driver.run();
    std::fprintf(stdout, "[acr-benchmark] 采集 %zu 条结果记录\n", results.size());

    // 25 号计划 §3.2：导出原始记录 JSON（每次原始耗时）
    const std::string raw_path = "raw_benchmark_records.json";
    if (!astro::compute::qualification::BenchmarkDriver::write_raw_records_json(
            raw_path, results)) {
        std::fprintf(stderr, "error: 无法写入 %s\n", raw_path.c_str());
        astro::compute::runtime_shutdown();
        return 2;
    }
    std::fprintf(stdout, "[acr-benchmark] 原始记录已写入 %s\n", raw_path.c_str());

    // 生成 HardwareProfile（hardware-profile.json，新权威路径）
    astro::compute::qualification::ProfileGenerator gen;
    auto hp = gen.generate_hardware_profile(results, args.kind);

    // 写入 hardware-profile.json
    if (!astro::compute::qualification::ProfileGenerator::write_hardware_profile_to_file(args.output, hp)) {
        std::fprintf(stderr, "error: 无法写入 %s\n", args.output.c_str());
        astro::compute::runtime_shutdown();
        return 2;
    }
    std::fprintf(stdout, "[acr-benchmark] hardware-profile 已写入 %s\n", args.output.c_str());
    std::fprintf(stdout, "[acr-benchmark] 指纹 sha256: %s\n", hp.fingerprint_sha256.c_str());
    std::fprintf(stdout, "[acr-benchmark] 设备数: %zu\n", hp.devices.size());

    astro::compute::runtime_shutdown();
    return 0;
}
