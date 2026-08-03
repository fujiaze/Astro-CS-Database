// ============================================================================
// cli_command.h - 单次命令执行模式
// 功能: 解析命令行参数并执行单次任务 (非交互式)
// 用途: 供脚本/批处理调用, 输出 JSON 格式结果到 stdout
//
// 支持命令:
//   orchestrator run <fits> [--config <json>] [--threads <N>] [--fresh]
//   orchestrator run-batch <dir> [--config <json>] [--threads <N>] [--fresh]
//   orchestrator stage1 --frame <fits> --output <hiss> [options]  (spec §2.3.3)
//   orchestrator stage2 --frames <dir> --output <hcsd> [options]  (spec §2.3.3)
//   orchestrator inspect --request <file>   (P04-001: 检查配置, 输出 effective_config)
//   orchestrator inspect --hiss <file>      (P04-003: 检查 HISS 文件元数据)
//   orchestrator inspect --hcsd <file>      (P04-003: 检查 HCSD 文件元数据)
//   orchestrator inspect --frame <file>     (P04-003: 检查 FITS 帧元数据)
//   orchestrator capabilities               (P04-001: 查询能力, P04-003 扩展 modules)
//   orchestrator status
//   orchestrator --help
//
// P04-001: --request <file> 模式 (request JSON)
//   - 支持 stage1/stage2/run 命令的 --request <file> 参数
//   - 配置优先级: CLI 命令行 > request.overrides > request.config > 内置默认值
//   - 生成 effective_config 快照 + SHA-256 hash
//   - stdout 输出 JSONL 事件流 (accepted/stage_started/stage_completed/completed/failed)
//   - stderr 输出人类可读日志 (由 Logger 负责)
//
// P04-003: inspect --hiss/--hcsd/--frame 模式
//   - 检查 HISS/HCSD/FITS 文件元数据, 不执行实际任务
//   - HISS/HCSD: 优先调用 AIO DLL 的 aio_hiss_read/aio_hcsd_read 获取完整元数据;
//                DLL 不可用时降级读取二进制头 (magic/长度前缀)
//   - FITS: 直接读取 2880 字节头块, 解析关键字 (不依赖 DLL)
//   - stdout 输出 JSONL 事件 (result 事件含文件元数据)
//   - stderr 输出人类可读日志
// ============================================================================

#pragma once

#include <string>
#include <map>
#include "orchestrator.h"

// Effective config 快照 (P04-001)
struct EffectiveConfig {
    std::string command;                 // stage1/stage2/run/capabilities/inspect
    std::string job_id;                  // 作业 ID
    std::string config_json;             // 合并后的最终配置 JSON (规范格式, key 排序)
    std::string effective_config_hash;   // SHA-256 hash (64 位小写十六进制)
    std::map<std::string, std::string> sources;  // 参数来源 (key -> cli/overrides/config/default)
    std::string created_at;              // ISO 8601 UTC 时间戳
    std::string request_path;            // --request 文件路径 (可选)
    std::string config_path;             // 配置文件路径 (可选)
};

class CliCommand {
public:
    // 解析命令行参数并执行
    // 返回: 进程退出码 (0=成功, 非0=失败)
    static int execute(int argc, char* argv[]);

private:
    // 单次命令处理
    static int cmd_run(const std::string& fits_path,
                       const std::string& config_path,
                       int threads,
                       bool fresh,
                       const std::string& log_level = "");
    static int cmd_run_batch(const std::string& dir_path,
                             const std::string& config_path,
                             int threads,
                             bool fresh,
                             const std::string& log_level = "");
    static int cmd_status();

    // spec §2.3.3 两段流水线 CLI 命令
    // stage1: 单帧预处理 (FITS -> .hiss, stage 0-7)
    // cancel_on_signal: P04-004 标志, true 时注册 SIGINT 处理器触发取消
    // WP-H 步骤14: 新增参数将合并到 config_json 传给 run_stage1, 不修改 run_stage1 签名
    //   nside_override: 显式 NSIDE (0=自动)
    //   pixfrac: 像素收缩因子 (-1.0=未设置, 后续用 1.0)
    //   dark_file/flat_file/bias_file: 校准文件路径
    //   photscal: 测光定标系数 (默认 1.0)
    //   apply_photometry: 是否应用测光定标
    //   threads: 线程数 (0=自动)
    // R10: precision_mode: 精度模式 ("fp32"/"fp64", 默认 "fp32")
    static int cmd_stage1(const std::string& fits_path,
                          const std::string& output_hiss,
                          const std::string& config_path,
                          const std::string& log_level = "",
                          bool cancel_on_signal = false,
                          int nside_override = 0,
                          double pixfrac = -1.0,
                          const std::string& dark_file = "",
                          const std::string& flat_file = "",
                          const std::string& bias_file = "",
                          double photscal = 1.0,
                          bool apply_photometry = false,
                          int threads = 0,
                          const std::string& precision_mode = "fp32");
    // stage2: 多帧合并 (.hiss -> .hcsd, stage 8-9)
    // cancel_on_signal: P04-004 标志, true 时注册 SIGINT 处理器触发取消
    static int cmd_stage2(const std::string& hiss_dir,
                          const std::string& output_hcsd,
                          const std::string& config_path,
                          const std::string& log_level = "",
                          bool cancel_on_signal = false);

    // P04-001: inspect 子命令 (检查配置, 输出 effective_config)
    static int cmd_inspect(const std::string& request_path);

    // P04-003: inspect --hiss <file> - 检查 HISS 文件元数据
    static int cmd_inspect_hiss(const std::string& hiss_path);

    // P04-003: inspect --hcsd <file> - 检查 HCSD 文件元数据
    static int cmd_inspect_hcsd(const std::string& hcsd_path);

    // P04-003: inspect --frame <file> - 检查 FITS 帧元数据
    static int cmd_inspect_frame(const std::string& fits_path);

    // P04-001: capabilities 子命令 (查询 CLI 支持的能力)
    static int cmd_capabilities();

    // P04-001: --request 模式入口 (解析 request JSON, 合并配置, 输出 JSONL 事件)
    // command: stage1/stage2/run (从 request JSON 读取)
    // cli_overrides: CLI 命令行参数的覆盖 (如 --nside, --pixfrac, key=参数路径, value=字符串值)
    static int cmd_request(const std::string& request_path,
                           const std::map<std::string, std::string>& cli_overrides);

    // 输出 JSON 结果到 stdout (旧版多行 JSON, 向后兼容)
    static void output_json_result(const TaskResult& result);
    static void output_json_batch(const std::vector<TaskResult>& results);

    // P04-001: 输出 JSONL 事件到 stdout (每行一个 JSON 事件)
    // event_type: accepted/stage_started/stage_completed/completed/failed
    static void output_jsonl_event(const std::string& event_type,
                                   const std::string& job_id,
                                   const std::string& stage = "",
                                   double progress = -1.0,
                                   const std::string& message = "",
                                   const std::string& result_json = "",
                                   const std::string& error_json = "");

    // P04-002: 扩展 JSONL 事件输出 (含数字 exit_code + 持续时间 + 关键指标)
    // 输出字段: schema_version/type/job_id/timestamp/stage/duration_ms/status/
    //           progress/message/result/error/exit_code/effective_config_hash
    // 用于 stage_start/stage_end/result/error/warning/progress 事件
    // exit_code: -1 表示不输出该字段; >=0 时输出 (error/failed 事件)
    // duration_ms: -1 表示不输出; >=0 时输出 (stage_end/stage_completed)
    // status: "" 表示不输出; "ok"/"failed"/"degraded" 时输出
    // extra_json: 额外字段 JSON 片段 (如 ",\"rms_arcsec\":0.33,\"n_pairs\":45")
    static void output_jsonl_event_ex(const std::string& event_type,
                                      const std::string& job_id,
                                      const std::string& stage = "",
                                      double progress = -1.0,
                                      const std::string& message = "",
                                      const std::string& result_json = "",
                                      const std::string& error_json = "",
                                      int exit_code = -1,
                                      double duration_ms = -1.0,
                                      const std::string& status = "",
                                      const std::string& extra_json = "");

    // P04-001: 计算有效配置 (合并 default + config + overrides + cli)
    // 返回 EffectiveConfig (含 SHA-256 hash)
    static EffectiveConfig compute_effective_config(
        const std::string& command,
        const std::string& job_id,
        const std::string& config_source,        // 配置文件路径或内联 JSON
        const std::string& overrides_json,        // request.overrides JSON 对象
        const std::map<std::string, std::string>& cli_overrides,
        const std::string& request_path = "",
        const std::string& config_path = "");

    // 辅助
    static void print_usage();
};
