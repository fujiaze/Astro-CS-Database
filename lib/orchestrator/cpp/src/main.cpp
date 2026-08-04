// ============================================================================
// main.cpp - 唯一入口: orchestrator.exe <stage1.json>
// 功能:
//   - 无参数: 打印 usage
//   - --help / -h: 打印帮助
//   - --version: 打印版本
//   - --print-schema: 打印 stage1.schema.json
//   - --validate <json>: 仅验证 Schema, 不执行
//   - --inspect <hiss>: 输出 HISS 文件 metadata (含 precision_mode/signal_dtype)
//   - <stage1.json>: 解析配置 + Schema 验证 + 执行 stage1 流水线
//
// 设计说明:
//   所有输入、参数、输出、日志位置均来自 JSON 文件。
//   无 CLI 科学参数覆盖、无 REPL、无 run/run-batch。
// ============================================================================

#include <cstdio>
#include <cstring>
#include <string>
#include <csignal>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

#include "json_config.h"
#include "orchestrator.h"
#include "cli_command.h"  // for p04004_register_signal_handler / output_jsonl_event_ex
#include "logger.h"
#include "precision_context.h"  // PrecisionContext 全局精度上下文 (双精度 ABI)

#include <filesystem>
#include <nlohmann/json.hpp>  // --inspect: 解析 HISS metadata JSON

// ASTROCS_GIT_COMMIT: 编译时可由 -D 传入, 否则使用占位符
#ifndef ASTROCS_GIT_COMMIT
#define ASTROCS_GIT_COMMIT "unknown"
#endif

// ============================================================================
// 辅助输出
// ============================================================================
static void print_usage() {
    printf("Usage: orchestrator.exe <stage1.json>\n");
    printf("       orchestrator.exe --help\n");
    printf("       orchestrator.exe --version\n");
    printf("       orchestrator.exe --print-schema\n");
    printf("       orchestrator.exe --validate <stage1.json>\n");
    printf("       orchestrator.exe --inspect <file.hiss>\n");
}

static void print_help() {
    print_usage();
    printf("\nAstroCS Stage1 Orchestrator (Phase1 JSON entry)\n");
    printf("\n");
    printf("The only production entry point is:\n");
    printf("  orchestrator.exe path/to/stage1.json\n");
    printf("\n");
    printf("All inputs, parameters, outputs and log locations come from the JSON file.\n");
    printf("No CLI scientific parameter overrides, no REPL, no run/run-batch.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help, -h            Show this help message\n");
    printf("  --version             Show version information\n");
    printf("  --print-schema        Print the stage1 JSON schema to stdout\n");
    printf("  --validate <json>     Validate a stage1 JSON file (exit 0=VALID, 1=INVALID)\n");
    printf("  --inspect <hiss>      Inspect a HISS file (metadata, precision, tile info)\n");
}

static void print_version() {
    printf("AstroCS Orchestrator 2.0 (Phase1 JSON entry)\n");
    printf("git commit: %s\n", ASTROCS_GIT_COMMIT);
}

// ============================================================================
// --inspect <hiss_file>: 输出 HISS 文件完整 metadata (HISS-002: 含 precision)
// 非正式命令 (类似 --validate), 仅用于诊断/调试, 不在正式流水线中使用.
// 加载 AIO DLL, 调用 aio_hiss_inspect, 输出 precision_mode/signal_dtype 等字段.
// ============================================================================
static int inspect_hiss_file(const std::string& hiss_path) {
    namespace fs = std::filesystem;

    if (!fs::exists(hiss_path)) {
        fprintf(stderr, "Error: HISS file not found: %s\n", hiss_path.c_str());
        return 1;
    }

    // 自动推导项目根目录 (与 Orchestrator::init_dlls 一致)
    // orchestrator.exe 位于 <root>/lib/orchestrator/cpp/, 向上 4 级得到 <root>
    std::string base_dir;
#ifdef _WIN32
    char exe_path[MAX_PATH] = {0};
    if (GetModuleFileNameA(nullptr, exe_path, MAX_PATH) > 0) {
        std::string ep(exe_path);
        size_t pos = std::string::npos;
        for (int i = 0; i < 4; ++i) {
            pos = ep.find_last_of("\\/", pos == std::string::npos ? std::string::npos : pos - 1);
            if (pos == std::string::npos) break;
        }
        if (pos != std::string::npos) {
            base_dir = ep.substr(0, pos);
        }
    }
#endif
    if (base_dir.empty()) base_dir = ".";

    // 加载 AIO DLL (astro_image_io.dll)
    DllLoader loader;
    if (!loader.load_module(ModuleId::AIO, base_dir)) {
        fprintf(stderr, "Error: Failed to load AIO DLL from %s\n", base_dir.c_str());
        fprintf(stderr, "  %s\n", loader.get_error(ModuleId::AIO).c_str());
        return AstroCsExitCode::DLL_LOAD_FAILED;
    }

    // 获取 aio_hiss_inspect / aio_hio_free 函数指针
    using InspectFn = int (*)(const char*, uint32_t*, uint32_t*, uint32_t*,
                               uint32_t*, uint64_t*, uint64_t*, char**, uint64_t**);
    using FreeFn = void (*)(void*);
    auto fn_inspect = loader.get_function<InspectFn>(ModuleId::AIO, "aio_hiss_inspect");
    auto fn_free = loader.get_function<FreeFn>(ModuleId::AIO, "aio_hio_free");

    if (!fn_inspect || !fn_free) {
        fprintf(stderr, "Error: AIO DLL does not export aio_hiss_inspect/aio_hio_free\n");
        return AstroCsExitCode::GENERIC_ERROR;
    }

    uint32_t nside = 0, tile_nside = 0, depth = 0, n_leaf_per_tile = 0;
    uint64_t n_tiles = 0, n_pix_total = 0;
    char* meta_json = nullptr;
    uint64_t* tile_ipix_list = nullptr;

    int ret = fn_inspect(hiss_path.c_str(), &nside, &tile_nside,
                          &depth, &n_leaf_per_tile, &n_tiles,
                          &n_pix_total, &meta_json, &tile_ipix_list);
    if (ret != 0) {
        fprintf(stderr, "Error: aio_hiss_inspect failed (rc=%d): %s\n", ret, hiss_path.c_str());
        return 1;
    }

    // 解析 metadata JSON 提取 precision_mode / signal_dtype (HISS-002)
    uint8_t precision_mode = 0;
    uint8_t signal_dtype = 0;
    bool has_precision = false;
    if (meta_json && meta_json[0] != '\0') {
        try {
            nlohmann::json j = nlohmann::json::parse(meta_json);
            if (j.contains("precision_mode")) {
                precision_mode = j["precision_mode"].get<uint8_t>();
                has_precision = true;
            }
            if (j.contains("signal_dtype")) {
                signal_dtype = j["signal_dtype"].get<uint8_t>();
            }
        } catch (...) {
            // JSON 解析失败, 仅输出原始 JSON
        }
    }

    // 输出完整 metadata
    printf("=== HISS Inspect ===\n");
    printf("file:            %s\n", hiss_path.c_str());
    printf("nside:           %u\n", nside);
    printf("tile_nside:      %u\n", tile_nside);
    printf("depth:           %u\n", depth);
    printf("n_leaf_per_tile: %u\n", n_leaf_per_tile);
    printf("n_tiles:         %llu\n", (unsigned long long)n_tiles);
    printf("n_pix_total:     %llu\n", (unsigned long long)n_pix_total);
    printf("precision_mode:  %u (%s)\n", precision_mode,
           has_precision ? (precision_mode == 1 ? "FP64" : "FP32") : "not set (default FP32)");
    printf("signal_dtype:    %u (%s)\n", signal_dtype,
           signal_dtype == 1 ? "float64" : "float32");
    if (meta_json && meta_json[0] != '\0') {
        printf("--- metadata JSON ---\n");
        printf("%s\n", meta_json);
    }

    // 释放 inspect 分配的内存
    if (meta_json) fn_free(meta_json);
    if (tile_ipix_list) fn_free(tile_ipix_list);

    return 0;
}

// ============================================================================
// main - 唯一入口
// ============================================================================
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    // 无参数: 打印 usage
    if (argc == 1) {
        print_usage();
        return 0;
    }

    std::string arg1 = argv[1];

    // --help / -h
    if (arg1 == "--help" || arg1 == "-h") {
        print_help();
        return 0;
    }

    // --version
    if (arg1 == "--version") {
        print_version();
        return 0;
    }

    // --print-schema
    if (arg1 == "--print-schema") {
        printf("%s\n", get_stage1_schema_json().c_str());
        return 0;
    }

    // --validate <json>
    if (arg1 == "--validate") {
        if (argc != 3) {
            fprintf(stderr, "Error: --validate requires a JSON file argument\n");
            print_usage();
            return AstroCsExitCode::CONFIG_ERROR;
        }
        std::string err;
        if (validate_stage1_schema(argv[2], err)) {
            printf("VALID\n");
            return 0;
        } else {
            printf("INVALID: %s\n", err.c_str());
            return 1;
        }
    }

    // --inspect <hiss_file> (HISS-002: 输出 precision_mode/signal_dtype 等完整 metadata)
    if (arg1 == "--inspect") {
        if (argc != 3) {
            fprintf(stderr, "Error: --inspect requires a HISS file argument\n");
            print_usage();
            return AstroCsExitCode::CONFIG_ERROR;
        }
        return inspect_hiss_file(argv[2]);
    }

    // 以 -- 开头的未知选项
    if (!arg1.empty() && arg1[0] == '-' && arg1.size() > 1) {
        fprintf(stderr, "Error: unknown option '%s'\n", arg1.c_str());
        print_usage();
        return AstroCsExitCode::CONFIG_ERROR;
    }

    // ========================================================================
    // 唯一正式入口: orchestrator.exe <stage1.json>
    // ========================================================================
    if (argc == 2) {
        // 1. 解析配置 + Schema 验证
        Stage1Config config;
        std::string err;
        int parse_ret = parse_stage1_config(arg1, config, err);
        if (parse_ret != 0) {
            fprintf(stderr, "Config error: %s\n", err.c_str());
            return AstroCsExitCode::CONFIG_ERROR;
        }

        // 1.5 接入 PrecisionContext (双精度 ABI):
        // 由 orchestrator 在启动阶段根据配置设置一次, 所有模块共享.
        // AIO (FITS/XISF 读取) 在此之后查询 PrecisionContext 决定 data/data_f64.
        if (config.precision == PrecisionMode::FP64) {
            PrecisionContext::instance().set_scalar_type(AstroScalarType::FP64);
            fprintf(stderr, "[main] PrecisionContext set to FP64 (float64, no downgrade)\n");
        } else {
            PrecisionContext::instance().set_scalar_type(AstroScalarType::FP32);
            fprintf(stderr, "[main] PrecisionContext set to FP32 (default)\n");
        }

        // 2. 初始化日志 (日志目录取 config.output.log 的父目录)
        if (!config.output.log.empty()) {
            std::filesystem::path log_path(config.output.log);
            std::string log_dir = log_path.parent_path().string();
            if (!log_dir.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(log_dir, ec);
                Logger::instance().set_log_dir(log_dir);
            }
        }

        // 3. 构建 job_id (基于原始 JSON SHA256 前 12 位)
        std::string job_id = "stage1_";
        job_id += config.original_json_sha256.substr(0, 12);

        LOG_INFO("main", "========== AstroCS Stage1 Orchestrator ==========");
        LOG_INFO("main", "配置文件: " + config.original_json_path);
        LOG_INFO("main", "原始 JSON SHA256: " + config.original_json_sha256);
        LOG_INFO("main", "规范化配置 SHA256: " + config.config_sha256);
        LOG_INFO("main", "job_id: " + job_id);

        // 4. 输出 accepted 事件
        CliCommand::output_jsonl_event_ex(
            "accepted", job_id, "", -1.0,
            "stage1 job accepted",
            std::string("{\"config_sha256\":\"") + config.config_sha256 + "\"}",
            "", -1, -1.0, "ok");

        // 5. 创建 Orchestrator 并注册 SIGINT 处理器
        Orchestrator orch;
        p04004_register_signal_handler(&orch, true);

        // 6. typed Stage1Config 直接驱动 (无 compat flat JSON 桥)
        orch.set_stage1_config(config);

        // 7. 执行 stage1 流水线
        LOG_INFO("main", "输入 FITS: " + config.input.light);
        LOG_INFO("main", "输出 HISS: " + config.output.hiss);

        TaskResult result = orch.run_stage1(config);

        // 8. 注销信号处理器
        p04004_unregister_signal_handler();

        // 9. 输出结果事件
        if (result.success) {
            CliCommand::output_jsonl_event_ex(
                "completed", job_id, "", 1.0,
                "stage1 completed successfully",
                std::string("{\"output_hiss\":\"") + result.output_hiss_path
                + "\",\"completed_to_gate\":\"" + result.completed_to_gate + "\"}",
                "", result.exit_code, -1.0, "ok");
        } else {
            std::string error_json = std::string("{\"code\":\"") +
                AstroCsExitCode::error_code_string(result.exit_code) +
                "\",\"message\":\"" + result.error_msg + "\"}";
            CliCommand::output_jsonl_event_ex(
                "failed", job_id, "", -1.0,
                "stage1 failed",
                "", error_json, result.exit_code, -1.0, "failed");
        }

        // 10. 返回退出码
        int exit_code = result.exit_code;
        if (exit_code == AstroCsExitCode::SUCCESS && !result.success) {
            exit_code = AstroCsExitCode::GENERIC_ERROR;
        }
        LOG_INFO("main", std::string("========== stage1 ") +
                 (result.success ? "完成 (成功)" : "失败") +
                 " exit_code=" + std::to_string(exit_code) + " ==========");
        return exit_code;
    }

    // 参数过多
    fprintf(stderr, "Error: too many arguments. Use --help for usage.\n");
    print_usage();
    return AstroCsExitCode::CONFIG_ERROR;
}
