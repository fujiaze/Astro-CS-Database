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
#include "disk_gate.h"    // §9.74 裁决 10: 磁盘门（唯一资源判据；跑前 warn / 运行中 fail-closed）
#include "exit_codes.h"
#include "jsonl.h"

namespace astrocs::cli::cmd {

// 运行确认（§6.1 / §3.5）。返回 true = 继续运行。
// 非交互 stdin（EOF）→ false：绝不把「无人确认」当成 yes（fail-closed）。
inline bool confirm_run(const std::string& command_name, const std::string& checks_text) {
    std::fputs(checks_text.c_str(), stderr);
    std::fprintf(stderr, "acsd: %s ready to run — type 'yes' to continue "
                         "(-y skips this confirmation): ", command_name.c_str());
    std::fflush(stderr);
    std::string answer;
    if (!std::getline(std::cin, answer)) return false;   // EOF → 不运行
    return answer == "yes" || answer == "y";
}

// JSON 值类型名（诊断用；只报类型，不泄露值）。
inline std::string json_type_of(const nlohmann::json& v) {
    if (v.is_null()) return "null";
    if (v.is_string()) return "string";
    if (v.is_number()) return "number";
    if (v.is_boolean()) return "boolean";
    if (v.is_array()) return "array";
    if (v.is_object()) return "object";
    return "unknown";
}

// 配置「结构层」判定的唯一实现（键存在性 + JSON 类型 + 非空形态；不做科学值域判定）。
// 返回全部结构错（空 = 结构可达）。预检页的 [error] 行与运行前的硬门同源 ——
// 结构错不是 §3.5 的可强制项（-force 只越过「缺校准帧」一类），因此即使 -force
// 也必须 rc=2；否则运行期会在 nlohmann 取值处抛 type_error 逃逸到 70
// （SMOKE-001 D1: output_dir=123 时三命令全 rc=70 崩溃）。
// 判据字段仍取自 input_contract（CLI-002/GAP-034 单一来源）。
inline std::vector<std::string> config_structure_errors(SessionId session,
                                                        const nlohmann::json& doc) {
    std::vector<std::string> errs;
    // 防御：退役逐帧形态在 Subcommand::run 已提前拒绝（rc=3）；此处兜底避免
    // 后续 doc["output_dir"] 取值抛出（nlohmann const operator[] 对缺失键抛异常）。
    if (config_is_retired_perframe_form(doc)) {
        errs.push_back(retired_perframe_form_message(session_cli_name(session)));
        return errs;
    }
    // CLI-MULTIBLOCK（GAP_AUDIT §9.68/§9.71）：多块形态 —— 结构与运行期
    // 同源（唯一实现 = parser.cpp session_blocks_errors），此处不另造判据。
    // 多块形态自带块级 output_dir，故不要求顶层 output_dir。
    if (config_has_blocks(doc)) {
        int code = 0;
        for (const auto& e : session_blocks_errors(session_cli_name(session), doc, &code,
                                                   /*include_unknown_keys=*/false))
            errs.push_back(e);
        return errs;
    }
    if (!doc.contains("output_dir") || !doc["output_dir"].is_string()) {
        errs.push_back("output_dir 必须是非空字符串（收到 " +
                       (doc.contains("output_dir") ? json_type_of(doc["output_dir"])
                                                   : std::string("缺失")) +
                       "）；运行产物只落 output_dir");
    } else if (doc["output_dir"].get<std::string>().empty()) {
        errs.push_back("output_dir 缺失或为空（运行产物只落 output_dir）");
    }

    const InputContract& ic = input_contract(session);
    const std::string key = ic.key;
    if (!doc.contains(key)) {
        errs.push_back(key + " 缺失（" + ic.note + "）");
        return errs;
    }
    const auto& v = doc[key];
    if (ic.object_field != nullptr) {
        // 对象形态：会话消费的是 <key>.<object_field> 非空字符串。
        if (!v.is_object()) {
            errs.push_back(key + " 必须是对象 {\"" + ic.object_field + "\": \"<path>\"}（收到 " +
                           json_type_of(v) + "）");
        } else if (!v.contains(ic.object_field) || !v[ic.object_field].is_string()) {
            errs.push_back(key + "." + ic.object_field + " 缺失或非字符串（" +
                           (v.contains(ic.object_field) ? json_type_of(v[ic.object_field])
                                                        : std::string("缺失")) + "）");
        } else if (v[ic.object_field].get<std::string>().empty()) {
            errs.push_back(key + "." + ic.object_field + " 为空：没有输入产品");
        }
    } else {
        // 路径列表形态：必须是非空数组（字符串/数字等非数组形态 = 类型错，不猜测、不回落）。
        if (!v.is_array()) {
            errs.push_back(key + " 必须是路径数组（收到 " + json_type_of(v) + "）");
        } else if (v.empty()) {
            errs.push_back(key + " 为空：没有输入产品");
        }
    }
    return errs;
}

// ── §3.5 fail-closed 输入路径判定（存在性 / 可读性 / 类型） ──
// 设计口径（ASTROCS_DESIGN §3.5:204）：error = 文件找不到、路径问题。
// 预检必须在**磁盘**上核实，不得只看 JSON 结构（DC-310/DC-408 假绿根因）。
// 只报路径层事实，不做科学值域判定。
inline bool path_readable_file(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(p, ec) || ec) return false;
    std::ifstream f(p, std::ios::binary);
    return static_cast<bool>(f);
}

inline bool path_readable_dir(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::is_directory(p, ec) || ec) return false;
    std::filesystem::directory_iterator it(p, ec);
    return !ec;
}

// 逐路径检查：不存在 / 不可读 / 类型不符 → error 文案（含具体路径与原因）。
inline void check_input_path(std::vector<std::string>& errs, const std::string& label,
                             const std::string& raw, bool want_dir) {
    if (raw.empty()) return;   // 空 = 未提供；缺校准帧由 calibration_checks 处置
    const std::filesystem::path p = std::filesystem::u8path(raw);
    std::error_code ec;
    if (!std::filesystem::exists(p, ec) || ec) {
        errs.push_back(label + " 指向的路径不存在：" + raw +
                       "（§3.5 error：文件找不到，-y 不可越）");
        return;
    }
    const bool ok = want_dir ? path_readable_dir(p) : path_readable_file(p);
    if (!ok)
        errs.push_back(label + " 路径不可读或类型不符（应为" +
                       std::string(want_dir ? "目录" : "文件") + "）：" + raw);
}

// 全部输入路径的存在性/可读性（§3.5 error 面）：
//   normalize → input_lights[] + master_bias/master_dark/master_flat（已给定时）；
//   mosaic    → hips_paths[]（目录）；
//   export    → source.hips_dir（目录）。
// 返回空 = 全部可达。缺失 → 阻塞（rc=3 输入缺失），-y 不可越，仅 -force 越过整个预检。
inline std::vector<std::string> input_path_errors(SessionId session,
                                                  const nlohmann::json& doc) {
    std::vector<std::string> errs;
    // CLI-MULTIBLOCK（§9.68）：块级路径按平铺同款纪律逐块核磁盘，标签带 blocks[i]. 前缀
    // （一块一组 light + 一套母版；母版路径存在性 = 现有纪律：预检在盘上核实）。
    auto check_array_in = [&](const nlohmann::json& host, const char* key, bool want_dir,
                              const std::string& prefix) {
        if (!host.contains(key) || !host[key].is_array()) return;
        std::size_t i = 0;
        for (const auto& e : host[key]) {
            const std::string label = prefix + key + "[" + std::to_string(i) + "]";
            if (!e.is_string())
                errs.push_back(label + " 不是路径字符串（§3.5 error：路径问题）");
            else if (e.get<std::string>().empty())
                errs.push_back(label + " 为空：没有输入路径（§3.5 error：路径问题）");
            else
                check_input_path(errs, label, e.get<std::string>(), want_dir);
            ++i;
        }
    };
    auto check_array = [&](const char* key, bool want_dir) {
        check_array_in(doc, key, want_dir, "");
    };

    // §9.71 裁决 2（三命令同构块结构）：块内「一组输入帧」的判据按会话给 ——
    // normalize 收 light 帧文件，mosaic/export 收 HiPS 目录；键名/形态来自
    // input_contract(session)（单一声明），不在此手写第二份。
    if (config_has_blocks(doc) && doc["blocks"].is_array()) {
        const InputContract& ic = input_contract(session);
        const bool want_dir = (session != SESSION_NORMALIZE);   // HiPS 目录 vs light 帧文件
        std::size_t bi = 0;
        for (const auto& b : doc["blocks"]) {
            const std::string prefix = "blocks[" + std::to_string(bi) + "].";
            if (b.is_object()) {
                if (ic.object_field != nullptr) {
                    if (b.contains(ic.key) && b[ic.key].is_object() &&
                        b[ic.key].contains(ic.object_field) &&
                        b[ic.key][ic.object_field].is_string() &&
                        !b[ic.key][ic.object_field].get<std::string>().empty())
                        check_input_path(errs, prefix + ic.key + "." + ic.object_field,
                                         b[ic.key][ic.object_field].get<std::string>(), want_dir);
                } else {
                    check_array_in(b, ic.key, want_dir, prefix);
                }
                // 标定帧只在 normalize 会话存在（mosaic/export 不吃校准帧）。
                if (session == SESSION_NORMALIZE)
                    for (const char* k : {"master_bias", "master_dark", "master_flat"})
                        if (b.contains(k) && b[k].is_string())
                            check_input_path(errs, prefix + k, b[k].get<std::string>(),
                                             /*want_dir=*/false);
            }
            ++bi;
        }
        return errs;
    }
    switch (session) {
        case SESSION_NORMALIZE:
            check_array("input_lights", /*want_dir=*/false);
            for (const char* k : {"master_bias", "master_dark", "master_flat"})
                if (doc.contains(k) && doc[k].is_string())
                    check_input_path(errs, k, doc[k].get<std::string>(), /*want_dir=*/false);
            break;
        case SESSION_MOSAIC:
            check_array("hips_paths", /*want_dir=*/true);
            break;
        default:  // SESSION_EXPORT
            if (doc.contains("source") && doc["source"].is_object() &&
                doc["source"].contains("hips_dir") && doc["source"]["hips_dir"].is_string())
                check_input_path(errs, "source.hips_dir",
                                 doc["source"]["hips_dir"].get<std::string>(),
                                 /*want_dir=*/true);
            break;
    }
    return errs;
}

// 标定帧可见性（normalize；§3.5「缺少校准帧」= -force 可越过的 error）。
// 只陈述「该步将跳过」，不做科学判定（是否应该跳过属科学侧裁决）。
// 已提供但磁盘不可读的路径不在此报 correct（避免 fail-open 假绿）；由
// input_path_errors 报出具体原因。
inline std::vector<CheckLine> calibration_checks(SessionId session,
                                                  const nlohmann::json& doc) {
    std::vector<CheckLine> checks;
    // §9.71 裁决 2: 标定帧概念只属于 normalize 会话（mosaic/export 的输入是 HiPS 产品）。
    if (session != SESSION_NORMALIZE) return checks;
    // CLI-MULTIBLOCK（§9.68）：多块形态逐块给标定帧可见性，标签带 blocks[i]. 前缀
    // （块级归属可见；缺校准帧仍是 -force 可越过的 error，逐块独立判定）。
    auto checks_for = [&](const nlohmann::json& host, const std::string& prefix) {
        for (const char* k : {"master_bias", "master_dark", "master_flat"}) {
            const bool given = host.contains(k) && host[k].is_string() &&
                               !host[k].get<std::string>().empty();
            if (!given) {
                checks.push_back({"error", prefix + k + " 未提供：本次运行不做该标定步骤"
                                           "（如确无该标定帧，用 -force 越过）"});
            } else if (path_readable_file(std::filesystem::u8path(host[k].get<std::string>()))) {
                checks.push_back({"correct", prefix + k + " = " + host[k].get<std::string>()});
            }
            // given 但不可读 → 由 input_path_errors 报具体原因（不重复、不假绿）
        }
    };
    if (config_has_blocks(doc) && doc["blocks"].is_array()) {
        std::size_t bi = 0;
        for (const auto& b : doc["blocks"]) {
            if (b.is_object()) checks_for(b, "blocks[" + std::to_string(bi) + "].");
            ++bi;
        }
        return checks;
    }
    checks_for(doc, "");
    return checks;
}

// ── §3.5「详细预估」+ §9.74 裁决 10：磁盘预检（**唯一资源判据**） ──
//   * 预估事实（可用空间 / 预估需求下限）恒呈现（correct 行）——§3.5 要求预检页含
//     「资源与磁盘占用预估」；
//   * 余量不足 ⇒ **warn（不阻断）**：只进预检页，**不参与** has_error() 判定，
//     也不改退出码；运行期写盘失败/磁盘满才 fail-closed（disk_gate.h + commands.cpp）；
//   * 内存/CPU/线程**不设门**（本函数不产生任何此类判据）。
// 判据唯一实现 = lib/infrastructure/cli/disk_gate.h（statvfs/GetDiskFreeSpaceEx 实测 +
// 配置声明输入字节和）；本函数只组装预检行，不另造判据、不引入魔法字节数。
inline std::vector<CheckLine> disk_precheck_lines(SessionId session, const nlohmann::json& doc) {
    std::vector<CheckLine> lines;
    const InputContract& ic = input_contract(session);
    const std::string object_field = (ic.object_field == nullptr) ? std::string() : ic.object_field;
    for (const auto& sc : astrocs::disk_scopes(doc, ic.key, object_field,
                                               /*include_masters=*/session == SESSION_NORMALIZE)) {
        const astrocs::DiskSpace sp = astrocs::disk_space_of(sc.output_dir);
        const std::string warn = astrocs::disk_precheck_warning(sp, sc.est, sc.output_dir);
        if (!warn.empty()) lines.push_back({"warn", warn});
        else lines.push_back({"correct", astrocs::disk_estimate_line(sp, sc.est, sc.output_dir)});
    }
    return lines;
}

// 配置预检页（薄入口只给「可见性」，不做科学判定）：
//   error    — 结构错 / 输入路径不存在或不可读（§3.5 fail-closed）/ 缺校准帧
//   correct  — 结构可达 / 路径可达的标定帧
// 结构判据来自 config_structure_errors（唯一实现），路径判据来自 input_path_errors，
// 不再在此另造一套。
inline std::vector<CheckLine> precheck_config(SessionId session, const nlohmann::json& doc) {
    std::vector<CheckLine> checks;
    // CLI-MULTIBLOCK（§9.68）：多块形态逐块给「块归属 + output_dir + light 条目数」
    // 可见性（一块 = 一次运行 = 一个 output_dir / 一份 manifest）。
    if (config_has_blocks(doc) && doc["blocks"].is_array()) {
        const std::vector<std::string> berrs = config_structure_errors(session, doc);
        if (berrs.empty()) {
            const InputContract& ic = input_contract(session);
            std::size_t bi = 0;
            for (const auto& b : doc["blocks"]) {
                const std::string label = "blocks[" + std::to_string(bi) + "]";
                const std::string name =
                    (b.is_object() && b.contains("name") && b["name"].is_string())
                        ? (" '" + b["name"].get<std::string>() + "'") : std::string();
                const std::string inputs =
                    (ic.object_field != nullptr)
                        ? (std::string(ic.key) + "." + ic.object_field + " = " +
                           b[ic.key][ic.object_field].get<std::string>())
                        : (std::string(ic.key) + " 条目数 = " +
                           std::to_string(b[ic.key].size()));
                checks.push_back({"correct", label + name + " output_dir = " +
                                               b["output_dir"].get<std::string>() + "，" + inputs});
                ++bi;
            }
            for (const auto& e : input_path_errors(session, doc))
                checks.push_back({"error", e});
        } else {
            for (const auto& e : berrs) checks.push_back({"error", e});
        }
        for (const auto& c : calibration_checks(session, doc)) checks.push_back(c);
        for (const auto& c : disk_precheck_lines(session, doc)) checks.push_back(c);
        return checks;
    }
    const std::vector<std::string> errs = config_structure_errors(session, doc);
    if (errs.empty()) {
        checks.push_back({"correct", "output_dir = " + doc["output_dir"].get<std::string>()});
        const InputContract& ic = input_contract(session);
        const auto& v = doc[ic.key];
        if (ic.object_field != nullptr)
            checks.push_back({"correct", std::string(ic.key) + "." + ic.object_field + " = " +
                                           v[ic.object_field].get<std::string>()});
        else
            checks.push_back({"correct", std::string(ic.key) + " 条目数 = " +
                                           std::to_string(v.size())});
        // §3.5 fail-closed: 结构可达不等于文件在盘；逐个输入路径核磁盘。
        for (const auto& e : input_path_errors(session, doc))
            checks.push_back({"error", e});
    } else {
        for (const auto& e : errs) checks.push_back({"error", e});
    }
    if (session == SESSION_NORMALIZE) {
        for (const auto& c : calibration_checks(session, doc)) checks.push_back(c);
    }
    for (const auto& c : disk_precheck_lines(session, doc)) checks.push_back(c);
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
                    // 启动期取消也发恰一个 final 事件（DESIGN §7.2 机器输出
                    // 统一；空事件流无法与崩溃区分）。本次运行尚未建立 output_dir/
                    // run_context ⇒ 不写 manifest（不造假清单），final.run_manifest=null。
                    ev.emit_final(astrocs::CANCELLED, "cancelled", nullptr,
                                  "cancelled by user");
                    std::fprintf(stderr, "acsd: %s cancelled\n", name);
                    return astrocs::CANCELLED;             // 9
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        if (std::getenv("ASTROCS_TEST_CRASH")) throw std::runtime_error("cli-crash-hook");
        const std::string cfg = need_value(p, "--json");   // 缺 → ParseError → 2
        std::ifstream f(std::filesystem::u8path(cfg), std::ios::binary);
        if (!f) {
            std::fprintf(stderr, "acsd: config not found '%s'\n", cfg.c_str());
            return astrocs::INPUT;                          // 3
        }
        nlohmann::json doc;
        try {
            doc = nlohmann::json::parse(std::string(std::istreambuf_iterator<char>(f),
                                                    std::istreambuf_iterator<char>()));
        } catch (const nlohmann::json::parse_error& e) {
            std::fprintf(stderr, "acsd: config malformed JSON: %s\n", sanitize(e.what()).c_str());
            return astrocs::INPUT;                          // 3
        }
        if (!doc.is_object()) {
            std::fprintf(stderr, "acsd: config is not a JSON object\n");
            return astrocs::INPUT;                          // 3
        }
        // §3.5: -force 是「我知道我在干什么」的总开关 —— 跳过**全部**预检
        // （不显示页面、不请求确认、也不阻断结构错），直接进入运行过程。
        // 结构非法 / 路径缺失由运行期（session_dispatch → cmd_sessionN_run 的
        // validate_config_full + 节点 validate_config）自然报错并给明确理由；
        // 后果由用户承担（DC-313/DC-411 按负责人口径）。
        const bool forced = p.flags.count("-force") > 0;
        if (forced) {
            std::fprintf(stderr, "acsd: %s -force: skipping precheck and confirmation\n", name);
            return session_dispatch(static_cast<int>(session), SessionOp::Run, p, ev);
        }
        // CLI-MULTIBLOCK（§9.68 否决项）：退役的逐帧形态 {phase_name, config, inputs[]}
        // 在预检面即**明确拒绝并给迁移提示**（不落进「output_dir 缺失」一类泛化诊断；
        // 与运行期 validate_config_full 同文案、同退出码 3）。
        if (config_is_retired_perframe_form(doc)) {
            std::fputs(render_checks({{"error", retired_perframe_form_message(
                                                         session_cli_name(session))}}).c_str(),
                       stderr);
            std::fprintf(stderr, "acsd: %s blocked by config error(s); fix the config\n", name);
            return astrocs::INPUT;                          // 3: 配置形态不可用
        }
        const std::vector<CheckLine> checks = precheck_config(session, doc);
        const std::string page = render_checks(checks);
        // 阻断优先级（§3.5 + CLI_PROTOCOL §7）：
        //   ① 结构错（键缺失/类型错）→ 2（配置错）；
        //   ② 配置合同（白名单键/值域）→ 其自身退出码（未知键 3、schema_version 2…），
        //      先于③：否则「缺校准帧」这一条 finding 会掩盖未知键等更具体的诊断
        //      （与 GAP-034「一条误判掩盖其它真实错误」同族）；
        //   ③ 输入路径不存在/不可读（§3.5 error）→ 3（输入缺失），-y 不可越；
        //   ④ 可强制项（缺校准帧）→ 2（配置错），-y 不可越（仅 -force 越过整个预检）。
        const std::vector<std::string> structural = config_structure_errors(session, doc);
        if (!structural.empty()) {
            std::fputs(page.c_str(), stderr);
            std::fprintf(stderr, "acsd: %s blocked by config error(s); "
                                 "fix the config\n", name);
            return astrocs::ARGS;                           // 2: 配置错
        }
        {
            nlohmann::json validated;
            const int vrc = validate_config_full(cfg, &validated, /*session_mode=*/true, session_cli_name(session));
            if (vrc != astrocs::OK) return vrc;
        }
        if (!input_path_errors(session, doc).empty()) {
            std::fputs(page.c_str(), stderr);
            std::fprintf(stderr, "acsd: %s blocked by missing/unreadable input path(s); "
                                 "-y cannot override (use -force to bypass precheck)\n", name);
            return astrocs::INPUT;                          // 3: 输入缺失
        }
        if (has_error(checks)) {
            std::fputs(page.c_str(), stderr);
            std::fprintf(stderr, "acsd: %s blocked by precheck error(s); "
                                 "fix the config or pass -force\n", name);
            return astrocs::ARGS;                           // 2: 配置错
        }
        const bool assume_yes = p.flags.count("-y") > 0 || p.flags.count("--yes") > 0;
        // §3.5（2026-09-20 裁决）: 预检 = ① 打印有没有报错 + ② 详细预估（含资源与磁盘
        // 预估），**无论 correct / warn / error 都必须显示页面**；-y 只跳过**确认**，
        // 不跳过页面 ⇒ 无确认交互时在此打印（有确认时由 confirm_run 打印同一份 page）。
        if (assume_yes) std::fputs(page.c_str(), stderr);
        if (!assume_yes && !confirm_run(name, page)) {
            std::fputs(page.c_str(), stderr);
            std::fprintf(stderr, "acsd: %s not confirmed — aborting before any product write\n",
                         name);
            return astrocs::ARGS;                           // 2: 未确认
        }
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
                std::fprintf(stderr, "acsd: cannot write template '%s'\n", out_path.c_str());
                return astrocs::IO;                         // 7
            }
            f << text;
            if (!f.good()) return astrocs::IO;
            return astrocs::OK;
        }
        std::fputs(text.c_str(), stdout);
        return astrocs::OK;
    }

    // §1「子命令帮助与字段说明」：usage 行 + 字段表。字段表与 --template 同源
    // （session_commands.h config_fields），不手写副本（CLI-11）。
    int print_help() const {
        const auto* c = find(name);
        std::printf("%s\n", c ? help_usage(*c).c_str() : name);
        std::fputs(config_field_help(session).c_str(), stdout);
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
