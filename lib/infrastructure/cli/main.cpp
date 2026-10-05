// acsd CLI — 单一用户入口 (V5, CLI-002)
// 统一 parser + JSON/JSONL writer + 退出码映射 + 协作取消 + crash boundary。
// 命令树唯一权威: docs/ACSD_DESIGN §7.1 + docs/engineering/contracts/CLI_PROTOCOL.md「命令树」一节
// （落在 lib/infrastructure/cli/command_tree.h）；
// 协议/退出码唯一权威: docs/engineering/contracts/CLI_PROTOCOL.md + docs/engineering/standards/ERROR_MODEL.md §7
// （码值唯一源 = lib/infrastructure/cli/exit_codes.h）。
// Windows Unicode: wmain → UTF-16 argv 转 UTF-8, 文件经 std::filesystem::u8path 打开。
//
// RT-008: 本文件仅保留入口壳(crash boundary + 平台入口)与薄 include 面;
// parser/命令实现已拆至 lib/infrastructure/cli/parser.cpp 与 lib/infrastructure/cli/commands.cpp(共享头 lib/infrastructure/cli/cli_common.h)。
// 本文件不 include 任何 session/CFITSIO/AIO/Drizzle 科学内部头(CHK-001 验收)。
#include <csignal>
#include <cstdio>
#include <string>
#include <vector>

#if defined(__GLIBC__)
#include <malloc.h>   // PERF-MEM-FIX-01 (F3): mallopt 分配器调优
#endif

#include "cli_common.h"

#include "cancel_token.h"
#include "exit_codes.h"
#include "jsonl.h"

#ifdef _WIN32
#include <windows.h>
#endif

// ─────────────── crash boundary + 平台入口 ───────────────

// ─────────────── PERF-MEM-FIX-01 (F3): glibc 分配器调优 ───────────────
// P1 drizzle 节点的叶数组单块 2.67-8.39 MB (nside=65536/depth=9 时
// n_leaf_per_tile = 262144 叶 × 32 B = 8 MiB), 落在 glibc **动态** mmap 阈值
// 区间内 (默认 128 KiB, 每 free 一个大块就自动上调, 上限 32 MB) ⇒ 释放后整块
// 滞留在线程 arena 不还内核, 峰值 RSS 远高于真实工作集 (实测 3 帧串行单核:
// live heap 峰值 4.06 GB / RSS 4.09 GB, 而收尾 malloc_trim 后残留仅 2.3 MB)。
// 两处进程级设置 (不改任何科学公式/归约顺序/线程预算):
//   M_MMAP_THRESHOLD = 1 MiB —— 显式设定即关闭动态调整, 使 ≥1 MiB 的叶数组一律
//     走 mmap, free 时 munmap 立即归还内核, RSS 峰值跟随真实工作集;
//   M_ARENA_MAX = 2 —— 限制"每线程一个 64 MiB arena"的虚拟地址预留与跨 arena
//     碎片 (worker 数仍由 Runtime profile/lease 决定, 不在此硬编码)。
// 仅 __GLIBC__ 平台生效 (Windows/musl 无此接口)。
static void tune_allocator_for_large_tiles() {
#if defined(__GLIBC__)
    mallopt(M_MMAP_THRESHOLD, 1 << 20);
    mallopt(M_ARENA_MAX, 2);
#endif
}

int real_main(int argc, char** argv_utf8) {
    tune_allocator_for_large_tiles();
    acsd::install_cancel_handlers();
    std::string joined_for_report;
    try {
        Parsed p = parse_args(argc, argv_utf8);
        joined_for_report = p.join();
        if (joined_for_report.empty()) {
            std::fputs(kHelp, stderr);
            return acsd::ARGS;   // 04: 无命令 → 2（help 只打印到 stderr）
        }
        return dispatch(p);
    } catch (const ParseError& e) {
        std::fprintf(stderr, "acsd: %s\n", e.what());
        std::fputs(kHelp, stderr);
        return acsd::ARGS;   // 04: CLI 参数错 → 2
    } catch (const std::exception& e) {
        // 04 §5: 未捕获异常 → 70 + run_id + 阶段 + 最小脱敏 crash report, 不泄露凭据
        std::fprintf(stderr,
                     "acsd: CRASH run_id=%s command='%s' detail='%s' (sanitized; no credentials)\n",
                     acsd::make_run_id().c_str(),
                     sanitize(joined_for_report).c_str(), sanitize(e.what()).c_str());
        return acsd::INTERNAL;
    } catch (...) {
        std::fprintf(stderr, "acsd: CRASH run_id=%s command='%s' detail='unknown exception'\n",
                     acsd::make_run_id().c_str(), sanitize(joined_for_report).c_str());
        return acsd::INTERNAL;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    std::vector<std::string> u8;
    u8.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        // B13-R13-4: 修复 1 字节越界写 — 历史代码分配 n-1 却传 cbMultiByte=n。
        // 正确顺序 (缓冲计算见 cli_common.h utf8_from_wide_*): 分配 n → 转 n → 去 NUL。
        const int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (!acsd::utf8_from_wide_should_convert(n)) {
            u8.emplace_back();
            continue;
        }
        std::string s(acsd::utf8_from_wide_alloc_bytes(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, s.data(), n, nullptr, nullptr);
        s.resize(acsd::utf8_from_wide_final_len(n));
        u8.push_back(std::move(s));
    }
    std::vector<char*> ptrs;
    for (auto& s : u8) ptrs.push_back(s.data());
    ptrs.push_back(nullptr);
    return real_main(static_cast<int>(u8.size()), ptrs.data());
}
#else
int main(int argc, char** argv) {
    // P-163 / docs/detail/infrastructure/21_observability.md §7：日志/事件写入失败必须
    // "记 stderr 脱敏摘要 + 以非 0 退出码结束（IO=7；磁盘满=10）"。POSIX 对 SIGPIPE 的默认
    // 处置是**直接终止进程**，会让写入路径的 EPIPE 分支永不可达（实测 rc=-13 被信号杀死，
    // 合同要求的 IO(7) 映射不可达）⇒ 忽略 SIGPIPE，使写调用返回 -1/EPIPE，
    // 由事件写入层（jsonl.h）登记为写失败并按 errno 映射（ENOSPC→10、否则→7）。
    std::signal(SIGPIPE, SIG_IGN);
    return real_main(argc, argv);
}
#endif
