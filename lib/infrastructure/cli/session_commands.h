// lib/infrastructure/cli/session_commands.h — 命令层 ↔ 会话层路由（CLI-001）
//
// 「用户命令名 → 内部会话」的机械映射唯一落点。三个用户命令（normalize /
// mosaic / export）是**平级独立命令**（ASTROCS_DESIGN §1.2）：本表只做名字
// 映射，不表达顺序、不串接、不共享进程状态；每个命令各起一个进程、各自
// 校验、各自恢复。
//
// 数值会话号与既有 pipeline/session 内部编号一致（内部指代）；用户可见字符串
// 见 kSessionCommands。本文件不出现任何旧用户命令名。
#pragma once

#include <string>
#include <vector>

#include "command_tree.h"

namespace astrocs::cli::cmd {

struct SessionCommand {
    const char* name;    // 用户可见命令名
    SessionId   session; // 内部会话号（1/2/3）
    const char* op_hint; // 会话内部操作族（诊断用；不参与用户命令面）
};

inline const std::vector<SessionCommand>& session_commands() {
    static const std::vector<SessionCommand> k = {
        {"normalize", SESSION_NORMALIZE, "calibration+normalize"},
        {"mosaic",    SESSION_MOSAIC,    "mosaic+integration"},
        {"export",    SESSION_EXPORT,    "projection+export"},
    };
    return k;
}

inline bool is_session_command(const std::string& name) {
    for (const auto& s : session_commands())
        if (name == s.name) return true;
    return false;
}

inline SessionId session_of(const std::string& name) {
    for (const auto& s : session_commands())
        if (name == s.name) return s.session;
    return static_cast<SessionId>(0);
}

// 会话内部名称（诊断/事件 phase 字段用）。仅在需要人类可读的会话标识时使用；
// 用户命令名一律用 SessionCommand::name。
inline std::string session_internal_name(SessionId s) {
    switch (s) {
        case SESSION_NORMALIZE: return "session1";
        case SESSION_MOSAIC:    return "session2";
        case SESSION_EXPORT:    return "session3";
        default:                return "session?";
    }
}

// 配置模板（§6.1 --template）：必要参数 + 路径，可直接改后运行。科学默认值
// 不写进模板（默认参数唯一家在程序根 config/defaults.json）。
inline std::string config_template(SessionId s) {
    switch (s) {
        case SESSION_NORMALIZE:
            return std::string("{\n") +
                   "  \"schema_version\": \"1\",\n" +
                   "  \"input_lights\": [],\n" +
                   "  \"output_dir\": \".\",\n" +
                   "  \"drizzle\": {\"nside\": 512, \"nested\": 1, \"pixfrac\": 1.0},\n" +
                   "  \"filter_passband\": \"\"\n" +
                   "}\n";
        case SESSION_MOSAIC:
            return std::string("{\n") +
                   "  \"schema_version\": \"1\",\n" +
                   "  \"hips_paths\": [],\n" +
                   "  \"output_dir\": \".\",\n" +
                   "  \"weight_mode\": 2\n" +
                   "}\n";
        case SESSION_EXPORT:
            return std::string("{\n") +
                   "  \"schema_version\": \"1\",\n" +
                   "  \"source\": \"\",\n" +
                   "  \"output_dir\": \".\",\n" +
                   "  \"center\": [0.0, 0.0],\n" +
                   "  \"width_px\": 1024,\n" +
                   "  \"height_px\": 1024,\n" +
                   "  \"scale_deg_per_px\": 0.001\n" +
                   "}\n";
        default:
            return std::string("{}\n");
    }
}

// 运行确认页（§6.1 / §3.5）：绿色 correct / 橘色 optimize / 红色 error 三级。
// 本层只做**预检提示与确认**；科学判定不在这里（薄入口）。
struct CheckLine { std::string level; std::string text; };

inline bool has_error(const std::vector<CheckLine>& checks) {
    for (const auto& c : checks)
        if (c.level == "error") return true;
    return false;
}

inline std::string render_checks(const std::vector<CheckLine>& checks) {
    std::string out;
    for (const auto& c : checks) {
        const char* mark = (c.level == "error")     ? "[error]"
                           : (c.level == "optimize") ? "[optimize]"
                                                     : "[correct]";
        out += std::string("  ") + mark + " " + c.text + "\n";
    }
    return out;
}

}  // namespace astrocs::cli::cmd
