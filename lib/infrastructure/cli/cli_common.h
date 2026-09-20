// lib/infrastructure/cli/cli_common.h — RT-008 CLI 拆分共享头
// parser 结构、命令白名单、帮助器、共享命令声明。
// 本头不 include 任何 session/CFITSIO/AIO/Drizzle 科学内部头（CHK-001 验收）。
#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace astrocs {

// ── JSONL 事件发射器 ──
class JsonlEmitter;  // 定义见 jsonl.h

// B8-P1-2: benchmark cpu verdict 推导(单一实现; dispatch 与共址单测共用)。
// 规则(与 tools/validate_cpu_profile.py "oracle 失败 → verdict FAIL" 语义一致):
//   任一 kernels[*].correctness_test != "oracle:pass" → "FAIL"(正确性筛选优先,
//   恒不静默放行); 全部 oracle:pass → "PASS"; kernels 缺失/非对象/空 → "FAIL"
//   (无正确性证据不判定成功)。输出 JSON 顶层登记 verdict 字段
//   (v2 profile 无独立 schema 文档; 字段语义由本函数与 tests/cli 单测固化)。
inline std::string benchmark_profile_verdict(const nlohmann::json& profile) {
    if (!profile.is_object() || !profile.contains("kernels") ||
        !profile["kernels"].is_object() || profile["kernels"].empty())
        return "FAIL";
    for (auto it = profile["kernels"].begin(); it != profile["kernels"].end(); ++it) {
        if (!it.value().is_object()) return "FAIL";
        if (it.value().value("correctness_test", "") != "oracle:pass") return "FAIL";
    }
    return "PASS";
}

// B13-R13-4: WideCharToMultiByte 两段式换码的目标缓冲计算 (单一实现;
// main.cpp wmain 与 commands.cpp cli_exe_dir 共用; 共址单测固化数学)。
// Win32 惯用法: 第一次调用 (cbMultiByte=0) 返回含 NUL 终止符的字节数 n;
// 第二次转换必须传"恰好 n"的容量 (NUL 一并写入)。历史缺陷: 分配 n-1 字节
// 却传 cbMultiByte=n → 尾 NUL 越界写 1 字节 (堆损坏/栈粉碎)。
// 正确顺序: 先分配 n 字节 → 以 cbMultiByte=n 转换 → resize 到 final_len 去 NUL。
// n<=0 (转换失败) / n==1 (仅 NUL, 空串) → 最终空串, 不得执行二次转换。
inline size_t utf8_from_wide_final_len(int n) noexcept {
    return n > 1 ? static_cast<size_t>(n) - 1 : 0;
}
inline bool utf8_from_wide_should_convert(int n) noexcept {
    return n > 1;
}
inline size_t utf8_from_wide_alloc_bytes(int n) noexcept {
    return n > 1 ? static_cast<size_t>(n) : 0;
}

}  // namespace astrocs

// ───────────────────────── parser ─────────────────────────

struct ParseError : std::runtime_error {
    explicit ParseError(const std::string& m) : std::runtime_error(m) {}
};

struct Parsed {
    std::vector<std::string> cmd;                // 子命令 token (e.g. {"config","init"}) 或 {"--help"}
    std::map<std::string, std::string> values;   // 带值旗标
    std::set<std::string> flags;                 // 布尔旗标
    std::string join() const {
        std::string s;
        for (const auto& c : cmd) { if (!s.empty()) s += ' '; s += c; }
        return s;
    }
};

extern const std::set<std::string> kBoolFlags;
extern const std::set<std::string> kValueFlags;

// CLI-001: 命令树不再由本头声明 —— 唯一事实源是
// lib/infrastructure/cli/command_tree.h（path + 旗标白名单 + help 文本）。
// kHelp 仍在此暴露（parser.cpp 由命令树生成），便于 main.cpp 打印。
extern const char* kHelp;

[[noreturn]] void parse_fail(const std::string& msg);
Parsed parse_args(int argc, char** argv_utf8);

// ───────────────────── 参数校验帮助器 ─────────────────────

std::string need_value(const Parsed& p, const std::string& flag);
std::string sanitize(const std::string& s);
// RT-009: 运行图路径脱敏 — 绝对路径 → "<root>/<末2组件>"；相对路径原样。
std::string sanitize_path(const std::string& p);
std::string file_sha256(const std::string& u8path, bool* ok);
std::string local_cpu_signature();

// ── CLI-MULTIBLOCK（GAP_AUDIT §9.68 负责人裁决 2026-09-20）：normalize 多数据块配置 ──
// 一个 JSON 内可并列多个数据块（block）：每块自带一组 input_lights + 一套母版
// （master_bias/master_dark/master_flat）+ 运行参数 + 块级 output_dir；
// 一块 = 一次运行（独立 output_dir / 独立 run manifest）。
// 平铺单块简写（input_lights/master_*/output_dir 顶层平铺）保留，两者互斥。
// 唯一实现 = lib/infrastructure/cli/parser.cpp（运行期 validate_config_full 与
// subcommand.h 预检同源，禁止各写一份 ⇒ 防预检/运行期分叉）。
bool config_has_blocks(const nlohmann::json& doc);
bool config_has_flat_session_keys(const nlohmann::json& doc);
// 退役的逐帧形态 {phase_name, config, inputs[]}（§9.68 否决项）判别 + 迁移提示
// （唯一实现 = parser.cpp；预检与运行期同文案、同退出码 3）。
// session_name ∈ {"normalize","mosaic","export"}：提示按会话给 —— normalize 的该形态是
// 「每帧一个对象」的旧写法；mosaic/export 的**合同**形态同为 {phase_name, config, inputs[]}
// （键名与 CLI 不一致，见 GAP_AUDIT §9.71 裁决 2 定案 3），其块化键名方案属**前台裁量面**
// （定案 4），落地前 CLI 明确拒绝并指向该裁决（不静默按 normalize 键集解释）。
bool config_is_retired_perframe_form(const nlohmann::json& doc);
std::string retired_perframe_form_message(const std::string& session_name);
// 多块形态结构校验（唯一实现）：返回诊断行（空 = 结构可达）；*exit_code = 2（结构/配置错）
// 或 3（块内未知键，与顶层 unknown key 同码）。
// include_unknown_keys=false ⇒ 只报结构错（预检页与运行期同序：结构错 2 优先，
// 未知键交由 validate_config_full 报 3，与平铺路径逐字同序）。
// session_name 决定块内**输入帧键**判据：normalize → input_lights[]（块 = 一组 light）；
// mosaic/export 的块内键名方案待 §9.71 定案 4 ⇒ 此前对二者**显式不支持**（rc=2，
// 不静默按 normalize 键集解释）。
std::vector<std::string> session_blocks_errors(const std::string& session_name,
                                               const nlohmann::json& doc, int* exit_code,
                                               bool include_unknown_keys = true);

int validate_config_full(const std::string& path, nlohmann::json* doc_out,
                         bool session_mode = false,
                         const std::string& session_name = "normalize");
int validate_cpu_profile(const std::string& path, nlohmann::json* prof_out);

// RT-009: 当前 git HEAD 短 SHA（sidecar source_commit；无 git 环境返回 nullopt）。
// 不 shell-out：读 .git/HEAD + refs（工作区可运行；无敏感路径）。
std::optional<std::string> git_head_sha();

// ───────────────────── 命令实现声明 ─────────────────────
// cmd_* 实现细节在 lib/infrastructure/cli/commands.cpp（内部链接）；仅 dispatch 与入口对外可见。

int dispatch(const Parsed& p);
int real_main(int argc, char** argv_utf8);

// ───────────────────── 会话命令（子命令实现） ─────────────────────
// 三个平级用户命令（normalize/mosaic/export）的实现入口在
// lib/infrastructure/cli/{normalize,mosaic,export}/；其背后的会话执行/校验/
// 计划/检视实现留在 lib/infrastructure/cli/commands.cpp（薄入口只做参数解析、预检、确认、退出码）。
// 这里的声明是「命令层 → 会话层」的唯一契约面。
enum class SessionOp { Run, Validate, Plan, Inspect };
int session_dispatch(int session, SessionOp op, const Parsed& p, astrocs::JsonlEmitter& ev);
std::string session_config_template(int session);
std::string session_run_message(int session);
bool session_has_flag(const Parsed& p, const char* flag);

// 版本生成头（构建期）
#ifndef ASTROCS_VERSION_STRING
#include "version_generated.h"
#endif
