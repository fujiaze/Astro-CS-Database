// ============================================================================
// cli_command.h - JSONL 事件输出 + SIGINT 信号处理工具
//
// Phase1 JSON 入口重构后, 所有命令解析逻辑已移至 main.cpp。
// 本文件仅保留:
// - CliCommand::output_jsonl_event_ex: JSONL 事件输出 (供 main.cpp 调用)
// - p04004_register_signal_handler / p04004_unregister_signal_handler:
// P04-004 SIGINT 信号处理 (Ctrl+C 触发取消)
// - sha256_impl::sha256: SHA-256 实现 (供 json_config.cpp 调用, 见 cpp)
// ============================================================================

#pragma once

#include <string>
#include "orchestrator.h"

// P04-004: SIGINT 信号处理 (Ctrl+C 触发取消)
// 在 stage1/stage2 执行前注册, 完成后注销
void p04004_register_signal_handler(Orchestrator* orch, bool enable_cancel_on_signal);
void p04004_unregister_signal_handler();

class CliCommand {
public:
    // P04-002: 扩展 JSONL 事件输出 (含数字 exit_code + 持续时间 + 关键指标)
    // 输出字段: schema_version/type/job_id/timestamp/stage/duration_ms/status/
    // progress/message/result/error/exit_code/effective_config_hash
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
};
