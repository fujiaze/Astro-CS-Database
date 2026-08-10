// lib/acr/qualification/benchmarks/benchmark_main.cpp — Google Benchmark 入口
//
// 设计（05_OPEN_SOURCE_REUSE_PLAN.md §6 Google Benchmark）：
//   1. 驱动单设备基础微基准，参数化尺寸/精度/ISA/线程点
//   2. 提供 warm-up、重复、计数器和 JSON 输出基础
//   3. 启动时打印 "请确保系统空载" 提示，不替用户判断（06 §2）
//   4. 支持将 JSON 结果写入文件（--output <path> 或 Google Benchmark 原生 --benchmark_out=）
//   5. 不替 benchmark 做路由生成（profile_generator 负责）
//
// 用法：
//   acr-benchmark [--output <path>] [--benchmark_filter=...] [--benchmark_min_time=...]
//                 [--benchmark_out=<path>] [--benchmark_out_format=json]
//   --output 是 --benchmark_out= 的语法糖；两者都给定时 --benchmark_out 优先
#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// 打印空载提示（06 §2）
// 提示后继续，不替用户判断，不自动检查或关闭其他应用
void print_idle_hint() {
    std::fprintf(stderr,
        "\n========================================\n"
        "ACR Benchmark 即将执行本机计算性能标定。\n"
        "请停止其他 CPU/GPU 密集任务并保持系统空载。\n"
        "标定期间不要同时运行 AstroCS 计算任务。\n"
        "程序不会自动检查或关闭其他应用。\n"
        "========================================\n\n");
    std::fflush(stderr);
}

// 解析 --output <path> 并从 argv 移除该参数对，其余透传给 Google Benchmark
// 若同时发现 --benchmark_out= 已设置，则忽略 --output（避免冲突）
// 返回 [new_argv, output_path]
struct ParsedArgs {
    std::vector<char*> argv_ptrs;  // 指向原始 argv 字符串的指针（不拥有内存）
    std::string output_path;
    bool has_benchmark_out{false};
};

ParsedArgs parse_args(int argc, char** argv) {
    ParsedArgs result;
    // 保留 argv[0]
    result.argv_ptrs.push_back(argv[0]);
    std::string pending_output;
    bool skip_next = false;
    for (int i = 1; i < argc; ++i) {
        if (skip_next) { skip_next = false; continue; }
        std::string a = argv[i];
        if (a == "--output" && i + 1 < argc) {
            pending_output = argv[i + 1];
            skip_next = true;  // 跳过下一个参数（path）
            continue;
        }
        if (a.rfind("--benchmark_out=", 0) == 0) {
            result.has_benchmark_out = true;
        }
        result.argv_ptrs.push_back(argv[i]);
    }
    // 若用户已用 --benchmark_out=，则 --output 忽略；否则用 --output 作为输出
    if (!result.has_benchmark_out && !pending_output.empty()) {
        result.output_path = pending_output;
    }
    return result;
}

} // anonymous namespace

int main(int argc, char** argv) {
    print_idle_hint();

    ParsedArgs parsed = parse_args(argc, argv);

    // 若指定了 --output 且未传 --benchmark_out=，构造 --benchmark_out=<path> 参数
    // 通过临时字符串存储，确保生命周期延续到 Initialize 之后
    std::string benchmark_out_arg;
    std::vector<char*> final_argv = parsed.argv_ptrs;
    if (!parsed.output_path.empty()) {
        benchmark_out_arg = "--benchmark_out=" + parsed.output_path;
        final_argv.push_back(benchmark_out_arg.data());
        std::fprintf(stderr, "ACR Benchmark: JSON output → %s\n", parsed.output_path.c_str());
    }

    int final_argc = static_cast<int>(final_argv.size());
    char** final_argv_ptr = final_argv.data();

    // Google Benchmark 初始化（处理 --benchmark_filter / --benchmark_min_time / --benchmark_out 等）
    ::benchmark::Initialize(&final_argc, final_argv_ptr);

    if (::benchmark::ReportUnrecognizedArguments(final_argc, final_argv_ptr)) {
        return 1;
    }

    // 运行所有注册的 benchmark
    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}
