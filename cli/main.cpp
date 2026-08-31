// astrocs CLI — 单一用户入口 (V5, CLI-002)
// 统一 parser + JSON/JSONL writer + 退出码映射 + 协作取消 + crash boundary。
// 命令树/协议/退出码唯一权威: 控制包 04 + docs/api/CLI_PROTOCOL_V1.md。
// Windows Unicode: wmain → UTF-16 argv 转 UTF-8, 文件经 std::filesystem::u8path 打开。
//
// RT-008: 本文件仅保留入口壳(crash boundary + 平台入口)与薄 include 面;
// parser/命令实现已拆至 cli/parser.cpp 与 cli/commands.cpp(共享头 cli/cli_common.h)。
// 本文件不 include 任何 session/CFITSIO/AIO/Drizzle 科学内部头(CHK-001 验收)。
#include <cstdio>
#include <string>
#include <vector>

#include "cli_common.h"

#include "cancel_token.h"
#include "exit_codes.h"
#include "jsonl.h"

#ifdef _WIN32
#include <windows.h>
#endif

// ─────────────── crash boundary + 平台入口 ───────────────

int real_main(int argc, char** argv_utf8) {
    astrocs::install_cancel_handlers();
    std::string joined_for_report;
    try {
        Parsed p = parse_args(argc, argv_utf8);
        joined_for_report = p.join();
        if (joined_for_report.empty() || joined_for_report == "--help" || joined_for_report == "-h") {
            std::fputs(kHelp, joined_for_report.empty() ? stderr : stdout);
            return joined_for_report.empty() ? astrocs::ARGS : astrocs::OK;
        }
        return dispatch(p);
    } catch (const ParseError& e) {
        std::fprintf(stderr, "astrocs: %s\n", e.what());
        std::fputs(kHelp, stderr);
        return astrocs::ARGS;   // 04: CLI 参数错 → 2
    } catch (const std::exception& e) {
        // 04 §5: 未捕获异常 → 70 + run_id + 阶段 + 最小脱敏 crash report, 不泄露凭据
        std::fprintf(stderr,
                     "astrocs: CRASH run_id=%s command='%s' detail='%s' (sanitized; no credentials)\n",
                     astrocs::make_run_id().c_str(),
                     sanitize(joined_for_report).c_str(), sanitize(e.what()).c_str());
        return astrocs::INTERNAL;
    } catch (...) {
        std::fprintf(stderr, "astrocs: CRASH run_id=%s command='%s' detail='unknown exception'\n",
                     astrocs::make_run_id().c_str(), sanitize(joined_for_report).c_str());
        return astrocs::INTERNAL;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> u8;
    u8.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
        if (n > 1) WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, s.data(), n, nullptr, nullptr);
        u8.push_back(std::move(s));
    }
    std::vector<char*> ptrs;
    for (auto& s : u8) ptrs.push_back(s.data());
    ptrs.push_back(nullptr);
    return real_main(static_cast<int>(u8.size()), ptrs.data());
}
#else
int main(int argc, char** argv) { return real_main(argc, argv); }
#endif
