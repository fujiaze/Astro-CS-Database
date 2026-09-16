// lib/infrastructure/cli/command_tree.h — 唯一命令树（ASTROCS_DESIGN §6.2）
//
// 本文件是「用户可见命令面」的唯一事实源：命令名、旗标白名单、help 文本都从
// 这里生成；根 lib/infrastructure/cli/parser.cpp 只消费本表，不再自带命令清单（旧表含 phase1/2/3
// 用户命令，已随 CLI-001 删除）。
//
// 三条不可回退的约束（改动本文件前先读）：
//   1) 命令树只登记 §6.2 的条目：normalize/mosaic/export ×(--json|--template|--help)
//      + help + --version + doctor + benchmark；旧 phase1/phase2/phase3 及其别名
//      不得再次出现（用户命令名解析失败 → exit 2）。
//   2) 三个命令平级独立（§1.2）：本表不表达任何顺序/依赖，禁止隐式串接。
//   3) 薄入口（§6.1）：本表只描述参数面，不含科学语义。
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace astrocs::cli::cmd {

// 会话标识 — 「内部指代」的唯一形式（数值 1/2/3，与既有 pipeline/session
// 内部编号一致）。用户可见字符串（normalize/mosaic/export）与之的对应关系
// 见 session_commands.h 的 kSessionCommands；两者都不出现在 help 文本里。
enum SessionId : int {
    SESSION_NORMALIZE = 1,
    SESSION_MOSAIC    = 2,
    SESSION_EXPORT    = 3,
};

// ── 旗标面 ──
inline const std::set<std::string>& boolean_flags() {
    static const std::set<std::string> k = {
        "--json",         // §6.3: 机器输出（stdout 恰一个 JSON 文档）
        "--template",     // §6.2: 生成配置模板
        "--help", "-h",   // §6.2: 子命令帮助与字段说明
        "-y", "--yes",    // §6.3: 跳过运行确认
        "-force",         // §6.3: 越过可强制项（缺失校准帧等）
        "--events-jsonl", // §6.3: 运行事件 JSONL 走 stdout
    };
    return k;
}

// 除 public_surface 之外仍被接受的内部/机器旗标（不写进 help，白名单仍在
// command_tree.h 内，保证「未知旗标 → 2」不被放宽）。cpu_profile 是机器绑定
// 配置（三类配置分离），由 benchmark 生成、运行时读取。
inline const std::set<std::string>& value_flags() {
    static const std::set<std::string> k = {
        "--json",         // 运行: <config.json> 路径；模板: 显式要求机器可读输出
        "--template",     // 模板写出路径（长格式，与 -o 等价）
        "-o",             // 模板写出路径（§6.2 短格式）
        "--output",       // 模板写出路径（长格式别名）
        "--cpu-profile",  // 机器绑定 cpu_profile 路径（可选）
        "--mode",         // 会话内部显式模式选择（mosaic）
        "--export-mode",  // 会话内部显式模式选择（export）
    };
    return k;
}

// 命令描述符。allowed 用的是「解析器旗标 token」而非 help 文本，例如 -o 与
// --output 是同一语义的两个 token，都在 allowed 里显式登记（不做别名展开，
// 保持「未知即 2」的强度）。
struct CommandDesc {
    const char* path;                 // 命令 token（空格分隔的多段命令）
    bool public_surface;              // 是否写进 help（§6.2 全为 true）
    std::vector<std::string> allowed; // 该命令允许的全部旗标 token
};

// §6.2 命令树 —— 顺序即 help 行顺序，逐行对照用。
inline const std::vector<CommandDesc>& commands() {
    static const std::vector<CommandDesc> k = {
        {"--version", true, {"--json"}},
        {"normalize", true, {"--json", "--template", "-o", "--output", "--help", "-h",
                             "-y", "--yes", "-force", "--events-jsonl", "--cpu-profile"}},
        {"mosaic",    true, {"--json", "--template", "-o", "--output", "--help", "-h",
                             "-y", "--yes", "-force", "--events-jsonl", "--cpu-profile",
                             "--mode"}},
        {"export",    true, {"--json", "--template", "-o", "--output", "--help", "-h",
                             "-y", "--yes", "-force", "--events-jsonl", "--cpu-profile",
                             "--export-mode"}},
        {"help",      true, {}},
        // 顶层 dash 命令（§6.2 的 help 等价形态）。不写进 help 文本
        // （help_text 里带 "-" 前缀的只保留 --version，避免把可选旗标行混进命令树）。
        {"--help",    false, {}},
        {"-h",        false, {}},
        {"doctor",    true, {"--json"}},
        {"benchmark", true, {}},
    };
    return k;
}

inline const CommandDesc* find(const std::string& path) {
    for (const auto& c : commands())
        if (path == c.path) return &c;
    return nullptr;
}

inline bool is_command(const std::string& path) { return find(path) != nullptr; }

// 逐段最长匹配用：返回以 path 为前缀的命令条数（含 path 自身）。
inline std::size_t count_with_prefix(const std::string& path) {
    std::size_t n = 0;
    for (const auto& c : commands()) {
        const std::string p = c.path;
        if (p == path || (p.size() > path.size() && p.compare(0, path.size(), path) == 0 &&
                          p[path.size()] == ' '))
            ++n;
    }
    return n;
}

// help 文本生成：命令树是唯一事实源，help 不允许手写副本（手写副本必然漂移，
// CLI-001 前身正是「help 与命令表各自维护」）。
inline std::string help_usage(const CommandDesc& c) {
    std::string line = std::string("astrocs ") + c.path;
    if (c.path == std::string("--version")) {
        line += " [--json]";
        return line;
    }
    if (c.path == std::string("doctor")) {
        line += " [--json]";
        return line;
    }
    if (c.path == std::string("benchmark")) return line;      // 无参数
    if (c.path == std::string("help")) return line;           // 直接输入即详细帮助
    // 三个子命令：--json 运行 / --template 生成模板 / --help 字段说明
    line += " (--json <config.json> | --template [-o <path>] | --help)";
    return line;
}

inline std::string help_text() {
    std::string out;
    for (const auto& c : commands()) {
        if (!c.public_surface) continue;
        // 顶层可选形态（--help/-h）不是独立命令行，不进 help 文本
        if (c.path[0] == '-' && c.path != std::string("--version")) continue;
        out += help_usage(c);
        out += '\n';
    }
    return out;
}

}  // namespace astrocs::cli::cmd
