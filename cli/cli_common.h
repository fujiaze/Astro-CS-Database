// cli/cli_common.h — RT-008 CLI 拆分共享头
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

struct CmdRule { const char* path; std::vector<std::string> allowed; };
extern const CmdRule kRules[];
extern const std::set<std::string> kGroups;
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
int validate_config_full(const std::string& path, nlohmann::json* doc_out);
int validate_cpu_profile(const std::string& path, nlohmann::json* prof_out);

// RT-009: 当前 git HEAD 短 SHA（sidecar source_commit；无 git 环境返回 nullopt）。
// 不 shell-out：读 .git/HEAD + refs（工作区可运行；无敏感路径）。
std::optional<std::string> git_head_sha();

// ───────────────────── 命令实现声明 ─────────────────────
// cmd_* 实现细节在 cli/commands.cpp（内部链接）；仅 dispatch 与入口对外可见。

int dispatch(const Parsed& p);
int real_main(int argc, char** argv_utf8);

// 版本生成头（构建期）
#ifndef ASTROCS_VERSION_STRING
#include "version_generated.h"
#endif
