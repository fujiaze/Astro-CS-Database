// ============================================================================
// cli_command.h - JSONL 事件输出 + SIGINT 信号处理工具
//
// Phase1 JSON 入口重构后, 所有命令解析逻辑已移至 main.cpp。
// 本文件仅保留:
// - CliCommand::output_jsonl_event_ex: JSONL 事件输出 (供 main.cpp 调用)
// - p04004_register_signal_handler / p04004_unregister_signal_handler:
// SIGINT 信号处理 (Ctrl+C 触发取消)
// - sha256_impl::sha256: deprecated shim 转发到 lib/common/crypto (B4-02 去重)
// [B4-24 锚点 — 不改语义]: 本头为纯 C++17 头(仅 string/orchestrator.h), 无 C ABI 导出; C ABI 边界由 orchestrator/dll_loader 统一约束(见 orchestrator.h B4-24 锚点).
// ============================================================================

#pragma once

#include <string>
#include "orchestrator.h"

// SIGINT 信号处理 (Ctrl+C 触发取消)
// 在 stage1/stage2 执行前注册, 完成后注销
void p04004_register_signal_handler(Orchestrator* orch, bool enable_cancel_on_signal);
void p04004_unregister_signal_handler();

class CliCommand {
public:
    // JSON 字符串转义 (Bug 狩猎 R5 P1-3 统一转义入口)
    // 转义: " \\ \n \r \t 及 <0x20 控制字符 (\uXXXX); 其余字节原样保留
    // (UTF-8 多字节序列不拆分, 输出仍为合法 JSON 字符串)。
    // 所有进入 JSONL 事件行的字符串字段 (含调用方手工拼接的 result_json/
    // error_json 内的字符串值) 必须经本函数转义 —— Windows 反斜杠路径、
    // 引号与错误消息文本不转义会产出非法 JSON, 破坏事件流解析。
    static std::string json_escape_string(const std::string& s);

    // 扩展 JSONL 事件输出 (含数字 exit_code + 持续时间 + 关键指标)
    // 输出字段: schema_version/type/job_id/timestamp/stage/duration_ms/status/
    // progress/message/result/error/exit_code/effective_config_hash
    // 用于 stage_start/stage_end/result/error/warning/progress 事件
    // exit_code: -1 表示不输出该字段; >=0 时输出 (error/failed 事件)
    // duration_ms: -1 表示不输出; >=0 时输出 (stage_end/stage_completed)
    // status: "" 表示不输出; "ok"/"failed"/"degraded" 时输出
    // extra_json: 额外字段 JSON 片段 (如 ",\"rms_arcsec\":0.33,\"n_pairs\":45")
    // 注意: result_json/error_json/extra_json 按"已序列化 JSON 对象文本"原样
    // 输出 —— 调用方必须用 json_escape_string 转义其中的字符串字面值后再拼接
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
