// lib/infrastructure/cli/subcommand.h — 三个平级子命令的共用实现（CLI-001）
//
// 薄入口（ASTROCS_DESIGN §6.1）职责边界，本文件是这条边界的落点：
//   参数读取 / 配置模板填充 / 运行前预检 / 运行确认 / 退出码映射
// 科学计算一律不在本层：执行、校验、计划、检视全部委托会话层
// （lib/infrastructure/cli/commands.cpp 的 session_dispatch → runtime_client/算法模块）。
//
// 三个子命令（normalize/mosaic/export）等价地只是「命令名 + 会话号」不同的同一
// 个薄适配器；不存在任何跨命令调用点，因而不可能隐式串接（§1.2）。
#pragma once

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "command_tree.h"
#include "session_commands.h"

#include "cli_common.h"   // Parsed / need_value / parse_fail / session_dispatch
#include "exit_codes.h"
#include "jsonl.h"

namespace astrocs::cli::cmd {

// 运行确认（§6.1 / §3.5）。返回 true = 继续运行。
// 非交互 stdin（EOF）→ false：绝不把「无人确认」当成 yes（fail-closed）。
inline bool confirm_run(const std::string& command_name, const std::string& checks_text) {
    std::fputs(checks_text.c_str(), stderr);
    std::fprintf(stderr, "astrocs: %s ready to run — type 'yes' to continue "
                         "(-y skips this confirmation): ", command_name.c_str());
    std::fflush(stderr);
    std::string answer;
    if (!std::getline(std::cin, answer)) return false;   // EOF → 不运行
    return answer == "yes" || answer == "y";
}

// 配置预检（薄入口只给「可见性」，不做科学判定）：
//   error    — 运行前即可判定的输入/路径问题（缺 input 列表、output_dir 非目录…）
//   correct  — 结构可达
// 与 §3.5 的绿/橘/红三级对应；橘色项（可优化项）由会话层给出，本层不臆造。
inline std::vector<CheckLine> precheck_config(SessionId session, const nlohmann::json& doc) {
    std::vector<CheckLine> checks;
    const std::string out_dir = doc.value("output_dir", std::string());
    if (out_dir.empty())
        checks.push_back({"error", "output_dir 缺失或为空（运行产物只落 output_dir）"});
    else
        checks.push_back({"correct", "output_dir = " + out_dir});

    const char* list_key = (session == SESSION_NORMALIZE) ? "input_lights"
                           : (session == SESSION_MOSAIC)  ? "hips_paths"
                                                          : "source";
    if (doc.contains(list_key)) {
        const auto& v = doc[list_key];
        const std::size_t n = v.is_array() ? v.size() : (v.is_string() ? 1 : 0);
        if (n == 0)
            checks.push_back({"error", std::string(list_key) + " 为空：没有输入产品"});
        else
            checks.push_back({"correct", std::string(list_key) + " 条目数 = " + std::to_string(n)});
    } else if (session == SESSION_EXPORT) {
        checks.push_back({"error", "source 缺失（export 不假设输入来自 mosaic）"});
    } else {
        checks.push_back({"error", std::string(list_key) + " 缺失"});
    }
    return checks;
}

// 子命令描述符：命令名是外部唯一可见字符串；会话号是内部指代。
struct Subcommand {
    const char*  name;
    SessionId    session;

    int run(const Parsed& p, astrocs::JsonlEmitter& ev) const {
        // 测试钩子（非用户接口）: 取消路径注入等待 / crash boundary 验证。
        // 与旧命令树同语义，仅存活于会话运行路径，不出现在 help 与命令表里。
        if (const char* ms_env = std::getenv("ASTROCS_TEST_SLEEP_MS")) {
            const long ms = std::strtol(ms_env, nullptr, 10);
            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::milliseconds(ms > 0 ? ms : 0);
            while (std::chrono::steady_clock::now() < deadline) {
                if (astrocs::is_cancelled()) {
                    std::fprintf(stderr, "astrocs: %s cancelled\n", name);
                    return astrocs::CANCELLED;             // 9
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        if (std::getenv("ASTROCS_TEST_CRASH")) throw std::runtime_error("cli-crash-hook");
        const std::string cfg = need_value(p, "--json");   // 缺 → ParseError → 2
        std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "astrocs: config not found '%s'\n", cfg.c_str());
            return astrocs::INPUT;                          // 3
        }
        nlohmann::json doc;
        try {
            doc = nlohmann::json::parse(std::string(std::istreambuf_iterator<char>(f),
                                                    std::istreambuf_iterator<char>()));
        } catch (const nlohmann::json::parse_error& e) {
            std::fprintf(stderr, "astrocs: config malformed JSON: %s\n", sanitize(e.what()).c_str());
            return astrocs::INPUT;                          // 3
        }
        if (!doc.is_object()) {
            std::fprintf(stderr, "astrocs: config is not a JSON object\n");
            return astrocs::INPUT;                          // 3
        }
        const std::vector<CheckLine> checks = precheck_config(session, doc);
        const std::string page = render_checks(checks);
        const bool forced = p.flags.count("-force") > 0;
        if (has_error(checks) && !forced) {
            std::fputs(page.c_str(), stderr);
            std::fprintf(stderr, "astrocs: %s blocked by precheck error(s); "
                                 "fix the config or pass -force\n", name);
            return astrocs::ARGS;                           // 2: 配置错
        }
        const bool assume_yes = p.flags.count("-y") > 0 || p.flags.count("--yes") > 0;
        if (!assume_yes && !confirm_run(name, page)) {
            std::fputs(page.c_str(), stderr);
            std::fprintf(stderr, "astrocs: %s not confirmed — aborting before any product write\n",
                         name);
            return astrocs::ARGS;                           // 2: 未确认
        }
        if (forced && has_error(checks))
            std::fprintf(stderr, "astrocs: %s running with -force over precheck error(s)\n", name);
        return session_dispatch(static_cast<int>(session), SessionOp::Run, p, ev);
    }

    int print_template(const Parsed& p) const {
        const std::string text = config_template(session);
        std::string out_path;
        if (p.values.count("-o")) out_path = p.values.at("-o");
        else if (p.values.count("--output")) out_path = p.values.at("--output");
        if (!out_path.empty()) {
            std::ofstream f(std::filesystem::u8path(out_path), std::ios::binary | std::ios::trunc);
            if (!f) {
                std::fprintf(stderr, "astrocs: cannot write template '%s'\n", out_path.c_str());
                return astrocs::IO;                         // 7
            }
            f << text;
            if (!f.good()) return astrocs::IO;
            return astrocs::OK;
        }
        std::fputs(text.c_str(), stdout);
        return astrocs::OK;
    }

    int print_help() const {
        const auto* c = find(name);
        std::printf("%s\n", c ? help_usage(*c).c_str() : name);
        return astrocs::OK;
    }

    // 子命令分派：只处理 §6.2 的三种形态（--json 运行 / --template / --help）。
    // 组合非法 → 2（不猜测、不回落）。
    int dispatch(const Parsed& p, astrocs::JsonlEmitter& ev) const {
        const bool wants_help = p.flags.count("--help") > 0 || p.flags.count("-h") > 0;
        const bool wants_template = p.flags.count("--template") > 0 ||
                                    p.values.count("--template") > 0;
        const bool wants_run = p.values.count("--json") > 0;
        if (wants_help) return print_help();
        if (wants_template) {
            if (wants_run) parse_fail(std::string(name) + ": --template cannot be combined with --json <path>");
            if (p.values.count("-o") && p.values.count("--output"))
                parse_fail(std::string(name) + ": -o and --output are the same flag");
            return print_template(p);
        }
        if (wants_run) return run(p, ev);
        parse_fail(std::string(name) + " requires --json <config.json>, --template or --help");
    }
};

}  // namespace astrocs::cli::cmd
