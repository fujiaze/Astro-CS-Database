// lib/acr/tools/acr_classic_runner/main.cpp — Phase H 经典实验运行器
// CLI 工具：运行 E01-E21 全部经典实验，输出 JSON 报告
//
// 用法:
//   acr-classic-runner                          # 运行全部，JSON 输出到 stdout
//   acr-classic-runner --output report.json     # 写入文件
//   acr-classic-runner -e E01,E03               # 仅运行 E01, E03
//   acr-classic-runner --list                   # 列出所有实验 ID
//   acr-classic-runner --summary                # 仅输出摘要（不含 cases 明细）
//   acr-classic-runner --help                   # 帮助
//
// 退出码: 0=全部 PASS, 1=存在 FAIL/SKIPPED, 2=参数错误
#include "classic_common.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute;
using namespace astro::compute::classic;

// ===== run_eXX 函数声明（定义在 acr_classic_experiments 静态库）=====
extern "C" std::vector<CaseResult> run_e01();
extern "C" std::vector<CaseResult> run_e02();
extern "C" std::vector<CaseResult> run_e03();
extern "C" std::vector<CaseResult> run_e04();
extern "C" std::vector<CaseResult> run_e05();
extern "C" std::vector<CaseResult> run_e06();
extern "C" std::vector<CaseResult> run_e07();
extern "C" std::vector<CaseResult> run_e08();
extern "C" std::vector<CaseResult> run_e09();
extern "C" std::vector<CaseResult> run_e10();
extern "C" std::vector<CaseResult> run_e11();
extern "C" std::vector<CaseResult> run_e12();
extern "C" std::vector<CaseResult> run_e13();
extern "C" std::vector<CaseResult> run_e14();
extern "C" std::vector<CaseResult> run_e15();
extern "C" std::vector<CaseResult> run_e16();
extern "C" std::vector<CaseResult> run_e17();
extern "C" std::vector<CaseResult> run_e18();
extern "C" std::vector<CaseResult> run_e19();
extern "C" std::vector<CaseResult> run_e20();
extern "C" std::vector<CaseResult> run_e21();

// ===== 实验元数据 =====
struct ExperimentMeta {
    const char* id;
    const char* name;
    std::vector<CaseResult> (*fn)();
};

static const ExperimentMeta kExperiments[] = {
    {"E01", "Memory Copy/Read/Write/Triad",      run_e01},
    {"E02", "AXPY/FMA",                          run_e02},
    {"E03", "Dot/Reduction Family",              run_e03},
    {"E04", "Tiled Matrix Transpose",            run_e04},
    {"E05", "2D Convolution",                    run_e05},
    {"E06", "Bilinear Affine Resampling",        run_e06},
    {"E07", "Histogram 256 bins",                run_e07},
    {"E08", "Prefix Scan",                       run_e08},
    {"E09", "Gather/Scatter",                    run_e09},
    {"E10", "Branch Divergence (Mandelbrot)",    run_e10},
    {"E11", "GEMM Mature Library Adaptation",    run_e11},
    {"E12", "FFT Round-trip",                    run_e12},
    {"E13", "CPU+GPU Mixed Partition",           run_e13},
    {"E14", "Resource Utilization Controller",   run_e14},
    {"E15", "Failure and Fallback",              run_e15},
    {"E16", "Concurrency/Cancellation/Lifetime", run_e16},
    {"E17", "Hardware Profile Model Fitting",    run_e17},
    {"E18", "Dynamic CPU+GPU Workpool",          run_e18},
    {"E19", "Resource Utilization Control",      run_e19},
    {"E20", "Fault and Fallback",                run_e20},
    {"E21", "Persistence and Concurrency",       run_e21},
};
static constexpr std::size_t kNumExperiments = sizeof(kExperiments) / sizeof(kExperiments[0]);

// ===== CLI 参数解析 =====
struct CliArgs {
    std::string output_path;          // --output / -o
    std::set<std::string> selected;   // --experiment / -e (空=全部)
    bool list_only{false};            // --list
    bool summary_only{false};         // --summary
    bool help{false};                 // --help / -h
};

static void print_help() {
    std::cerr << "acr-classic-runner — ACR Phase H 经典实验运行器\n\n";
    std::cerr << "用法:\n";
    std::cerr << "  acr-classic-runner [选项]\n\n";
    std::cerr << "选项:\n";
    std::cerr << "  -o, --output <file>     JSON 报告写入文件（默认 stdout）\n";
    std::cerr << "  -e, --experiment <ids>  仅运行指定实验，逗号分隔（如 E01,E03）\n";
    std::cerr << "      --list              列出所有实验 ID 后退出\n";
    std::cerr << "      --summary           仅输出摘要（不含 cases 明细）\n";
    std::cerr << "  -h, --help              显示此帮助\n\n";
    std::cerr << "退出码: 0=全部 PASS, 1=存在 FAIL/SKIPPED, 2=参数错误\n";
}

static bool parse_args(int argc, char** argv, CliArgs& args) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            args.help = true;
            return true;
        } else if (a == "--list") {
            args.list_only = true;
        } else if (a == "--summary") {
            args.summary_only = true;
        } else if (a == "-o" || a == "--output") {
            if (i + 1 >= argc) {
                std::cerr << "错误: " << a << " 需要参数\n";
                return false;
            }
            args.output_path = argv[++i];
        } else if (a == "-e" || a == "--experiment") {
            if (i + 1 >= argc) {
                std::cerr << "错误: " << a << " 需要参数\n";
                return false;
            }
            std::string spec = argv[++i];
            // 逗号分隔，转大写
            std::string token;
            std::istringstream iss(spec);
            while (std::getline(iss, token, ',')) {
                // trim 空白
                std::size_t b = 0, e = token.size();
                while (b < e && (token[b] == ' ' || token[b] == '\t')) ++b;
                while (e > b && (token[e-1] == ' ' || token[e-1] == '\t')) --e;
                if (b >= e) continue;
                token = token.substr(b, e - b);
                // 转大写
                for (auto& c : token) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                args.selected.insert(token);
            }
            if (args.selected.empty()) {
                std::cerr << "错误: --experiment 参数为空\n";
                return false;
            }
        } else {
            std::cerr << "错误: 未知参数 '" << a << "'（用 --help 查看用法）\n";
            return false;
        }
    }
    return true;
}

// ===== ISO8601 时间戳 =====
static std::string iso8601_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tm);
    return std::string(buf);
}

// ===== JSON 报告生成 =====
struct ExperimentSummary {
    std::string id;
    std::string name;
    std::size_t cases{0};
    std::size_t passed{0};
    std::size_t failed{0};
    std::size_t skipped{0};
};

static std::string build_report(const std::vector<CaseResult>& all_cases,
                                const std::vector<ExperimentSummary>& exp_summaries,
                                bool include_cases,
                                std::size_t worker_count) {
    std::size_t total = all_cases.size();
    std::size_t passed = 0, failed = 0, skipped = 0;
    for (const auto& c : all_cases) {
        if (c.status == "PASS") ++passed;
        else if (c.status == "FAIL") ++failed;
        else ++skipped;
    }
    double pass_rate = total > 0 ? static_cast<double>(passed) / static_cast<double>(total) : 0.0;

    std::ostringstream os;
    os << "{";
    os << "\"schema\":\"acr.classic_report.v1\",";
    os << "\"generated_at\":\"" << iso8601_now() << "\",";
    os << "\"seed\":\"0xA57C5AC20260802\",";
    os << "\"backend\":\"cpu\",";
    os << "\"device\":\"cpu\",";
    os << "\"runtime_workers\":" << worker_count << ",";

    // summary
    os << "\"summary\":{";
    os << "\"total\":" << total << ",";
    os << "\"passed\":" << passed << ",";
    os << "\"failed\":" << failed << ",";
    os << "\"skipped\":" << skipped << ",";
    os << "\"pass_rate\":" << pass_rate << ",";
    os << "\"experiments\":" << exp_summaries.size();
    os << "},";

    // experiments
    os << "\"experiments\":[";
    for (std::size_t i = 0; i < exp_summaries.size(); ++i) {
        const auto& e = exp_summaries[i];
        if (i > 0) os << ",";
        os << "{";
        os << "\"id\":\"" << json_escape(e.id) << "\",";
        os << "\"name\":\"" << json_escape(e.name) << "\",";
        os << "\"cases\":" << e.cases << ",";
        os << "\"passed\":" << e.passed << ",";
        os << "\"failed\":" << e.failed << ",";
        os << "\"skipped\":" << e.skipped;
        os << "}";
    }
    os << "]";

    // cases 明细
    if (include_cases) {
        os << ",\"cases\":[";
        for (std::size_t i = 0; i < all_cases.size(); ++i) {
            if (i > 0) os << ",";
            os << to_json(all_cases[i]);
        }
        os << "]";
    }

    os << "}";
    return os.str();
}

// ===== main =====
int main(int argc, char** argv) {
    CliArgs args;
    if (!parse_args(argc, argv, args)) {
        return 2;
    }
    if (args.help) {
        print_help();
        return 0;
    }
    if (args.list_only) {
        for (std::size_t i = 0; i < kNumExperiments; ++i) {
            std::cout << kExperiments[i].id << "  " << kExperiments[i].name << "\n";
        }
        return 0;
    }

    // 初始化 runtime
    runtime_init();
    std::size_t worker_count = runtime_worker_count();

    // 运行选中的实验
    std::vector<CaseResult> all_cases;
    std::vector<ExperimentSummary> exp_summaries;

    for (std::size_t i = 0; i < kNumExperiments; ++i) {
        const auto& meta = kExperiments[i];
        // 过滤：如果指定了 --experiment，只运行选中的
        if (!args.selected.empty() && args.selected.find(meta.id) == args.selected.end()) {
            continue;
        }

        std::cerr << "[runner] 运行 " << meta.id << " " << meta.name << " ... ";
        std::vector<CaseResult> cases;
        try {
            cases = meta.fn();
        } catch (const std::exception& ex) {
            std::cerr << "异常: " << ex.what() << "\n";
            ExperimentSummary es;
            es.id = meta.id;
            es.name = meta.name;
            es.cases = 0;
            es.failed = 1;
            exp_summaries.push_back(std::move(es));
            continue;
        }

        ExperimentSummary es;
        es.id = meta.id;
        es.name = meta.name;
        es.cases = cases.size();
        for (const auto& c : cases) {
            if (c.status == "PASS") ++es.passed;
            else if (c.status == "FAIL") ++es.failed;
            else ++es.skipped;
        }
        std::cerr << es.passed << "/" << es.cases << " PASS\n";
        exp_summaries.push_back(std::move(es));

        for (auto& c : cases) all_cases.push_back(std::move(c));
    }

    runtime_shutdown();

    // 生成 JSON 报告
    std::string report = build_report(all_cases, exp_summaries, !args.summary_only, worker_count);

    // 输出
    if (args.output_path.empty()) {
        std::cout << report << "\n";
    } else {
        std::ofstream f(args.output_path, std::ios::binary);
        if (!f.is_open()) {
            std::cerr << "错误: 无法写入 " << args.output_path << "\n";
            return 2;
        }
        f << report;
        f.close();
        std::cerr << "[runner] 报告已写入 " << args.output_path << "\n";
    }

    // 退出码
    for (const auto& c : all_cases) {
        if (c.status != "PASS") return 1;
    }
    return 0;
}
