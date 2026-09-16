#ifndef AIO_ATOMIC_FILE_H
#define AIO_ATOMIC_FILE_H

// ============================================================================
// aio_atomic_file.h - AIO 统一"临时文件 + fsync + 原子 rename"落盘原语 (header-only)
//
// 依据:
// - ASTROCS_DESIGN.md §9: 所有产品 = 临时文件/目录 + 校验 + fsync + 原子 rename 提交;
//   失败/取消不得留下可被误认为正式产品的半成品。
// - ENGINEERING_SPEC.md §9: 输出临时文件 + 原子提交; 错误通过统一状态码+结构化诊断传播。
// - docs/plugins/infrastructure/17_aio.md §4: 写 = 分层写 + flush/close/fsync + checksum
//   + 原子 rename; §8: 长路径 / Windows+Linux 双平台。
//
// 语义:
// - 临时文件名: <final>.tmp.<pid>.<seq> —— 与目标同目录 (rename 不跨文件系统)。
// - 失败路径: 关闭并删除临时文件, 绝不留下 <final> 的半成品。
// - 原子替换: Windows = MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH);
//   POSIX = rename(2)。**禁止先删目标再 rename** (hiss_stream_writer.cpp:174 冻结禁令:
//   删除后 rename 前崩溃 → 文件丢失)。
// - 路径按 UTF-8 解释 (aio_fopen_utf8): Windows 下必须 widen 后 _wfopen/MoveFileExW,
//   不得用 ANSI 代码页 API。
//
// 本头文件只提供机制, 不含产品语义; 科学值/键集合由调用方决定。
// ============================================================================

#include "aio_util.h"

#include <cstdio>
#include <cstdint>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace aio_atomic {

// 进程内单调序号: 与 pid 联合规避同进程并发写同名目标的临时名冲突。
inline uint64_t next_seq() {
    static uint64_t seq = 0;
    return ++seq;
}

inline std::string make_tmp_path(const std::string& final_path) {
#ifdef _WIN32
    const long pid = (long)_getpid();
#else
    const long pid = (long)getpid();
#endif
    return final_path + ".tmp." + std::to_string(pid) + "." +
           std::to_string((unsigned long long)next_seq());
}

#ifdef _WIN32
inline std::wstring widen_utf8(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0) return std::wstring(s.begin(), s.end());
    std::wstring ws((size_t)len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
    ws.resize((size_t)len - 1);
    return ws;
}
#endif

// 原子替换: 返回 0=成功, 非 0=失败。不删除目标 (失败时目标保持原状)。
inline int atomic_replace(const std::string& tmp_path, const std::string& final_path) {
#ifdef _WIN32
    const std::wstring wtmp = widen_utf8(tmp_path);
    const std::wstring wfinal = widen_utf8(final_path);
    if (!MoveFileExW(wtmp.c_str(), wfinal.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return -1;
    }
    return 0;
#else
    if (std::rename(tmp_path.c_str(), final_path.c_str()) != 0) return -1;
    return 0;
#endif
}

// 把 content 原子写为 final_path。返回 0=成功; 非 0=失败 (临时文件已清理, 目标未动)。
// err 非空时写入失败原因 (结构化诊断)。
inline int write_file_atomic(const std::string& final_path,
                            const std::string& content,
                            std::string* err) {
    const std::string tmp = make_tmp_path(final_path);
    FILE* f = aio_fopen_utf8(tmp.c_str(), "wb");
    if (!f) {
        if (err) *err = "open tmp failed: " + tmp;
        return -1;
    }
    if (!content.empty()) {
        const size_t n = std::fwrite(content.data(), 1, content.size(), f);
        if (n != content.size()) {
            std::fclose(f);
            std::remove(tmp.c_str());
            if (err) *err = "short write: " + tmp;
            return -2;
        }
    }
    if (std::fflush(f) != 0) {
        std::fclose(f);
        std::remove(tmp.c_str());
        if (err) *err = "fflush failed: " + tmp;
        return -3;
    }
#ifdef _WIN32
    if (_commit(_fileno(f)) != 0) {
#else
    if (fsync(fileno(f)) != 0) {
#endif
        std::fclose(f);
        std::remove(tmp.c_str());
        if (err) *err = "fsync failed: " + tmp;
        return -4;
    }
    if (std::fclose(f) != 0) {
        std::remove(tmp.c_str());
        if (err) *err = "fclose failed: " + tmp;
        return -5;
    }
    if (atomic_replace(tmp, final_path) != 0) {
        std::remove(tmp.c_str());
        if (err) *err = "atomic rename failed: " + tmp + " -> " + final_path;
        return -6;
    }
    return 0;
}

}  // namespace aio_atomic

#endif  // AIO_ATOMIC_FILE_H
