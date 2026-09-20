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

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

#ifdef _WIN32
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
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

// 把 writer(FILE*) 生成的内容原子写为 final_path (与 write_file_atomic 同语义,
// 供内容由多次 fprintf/fwrite 生成的场景使用)。返回 0=成功; 非 0=失败
// (临时文件已清理, 目标未动)。writer 返回 false 视为内容生成失败。
// §9: 产品落盘统一走 临时文件 → fflush → fsync → 原子 rename。
inline int write_file_atomic_stream(const std::string& final_path,
                                    const std::function<bool(FILE*)>& writer,
                                    std::string* err) {
    if (!writer) {
        if (err) *err = "null writer";
        return -1;
    }
    const std::string tmp = make_tmp_path(final_path);
    FILE* f = aio_fopen_utf8(tmp.c_str(), "wb");
    if (!f) {
        if (err) *err = "open tmp failed: " + tmp;
        return -1;
    }
    if (!writer(f)) {
        std::fclose(f);
        std::remove(tmp.c_str());
        if (err) *err = "writer failed: " + tmp;
        return -2;
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

// ============================================================================
// 文件系统机制原语 (aio 唯一实现)
// ----------------------------------------------------------------------------
// 依据: ASTROCS_DESIGN.md §9「aio 是文件级唯一 I/O 边界」+ §9.73 裁决 U5
//       (负责人逐字:「全部走 aio……没有其他需要读写的地方了」)+ 机器判据
//       「全仓文件打开/流式读写/文件系统写操作, 除 aio 内部外应为 0」。
// 纪律: 调用方 (算法/基建) **禁止**再自行 mkdir/stat/opendir/unlink/rmdir/
//       rename/fsync/open —— 一律经本组原语。返回原始系统错误码 (POSIX errno
//       或 CRT errno); **策略**(状态码映射)留给调用方, 机制只在本头。
// 路径: 按 UTF-8 解释 (Windows 走 A 系 API 的既有语义, 与 aio_publish v1
//       原实现逐位一致; UTF-8→UTF-16 收口登记为遗留项, 不在本头改语义)。
// ============================================================================

// 路径是否存在 (0=不存在; 1=存在)。is_dir 非空时回填是否为目录。
inline int path_exists(const std::string& path, int* is_dir) {
    if (is_dir) *is_dir = 0;
#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(path.c_str(), &st) != 0) return 0;
    if (is_dir) *is_dir = ((st.st_mode & _S_IFDIR) != 0) ? 1 : 0;
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    if (is_dir) *is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
#endif
    return 1;
}

// 常规文件大小 (字节)。返回 1 = 已回填 (size, is_dir); 0 = 路径不存在。
inline int path_size(const std::string& path, uint64_t* size, int* is_dir) {
    if (size) *size = 0;
    if (is_dir) *is_dir = 0;
#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(path.c_str(), &st) != 0) return 0;
    if (is_dir) *is_dir = ((st.st_mode & _S_IFDIR) != 0) ? 1 : 0;
    if (size) *size = static_cast<uint64_t>(st.st_size);
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    if (is_dir) *is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    if (size) *size = static_cast<uint64_t>(st.st_size);
#endif
    return 1;
}

// 逐段 mkdir -p (EEXIST 忽略)。返回 0 或 errno。
inline int make_dirs(const std::string& path) {
    if (path.empty()) return EINVAL;
    std::string cur;
    cur.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        const char c = path[i];
        cur.push_back(c);
        if (c != '/' && c != '\\') continue;
        if (cur.size() <= 1) continue;              // 根 "/"
#ifdef _WIN32
        if (cur.size() == 3 && cur[1] == ':') continue;   // "C:\"
        if (_mkdir(cur.c_str()) != 0 && errno != EEXIST) return errno;
#else
        if (mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) return errno;
#endif
    }
#ifdef _WIN32
    if (_mkdir(path.c_str()) != 0 && errno != EEXIST) return errno;
#else
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) return errno;
#endif
    return 0;
}

// 单层独占建目录 (不忽略 EEXIST)。返回 0 或 errno (EEXIST = 已被占用)。
inline int make_dir(const std::string& path) {
    if (path.empty()) return EINVAL;
#ifdef _WIN32
    if (_mkdir(path.c_str()) != 0) return errno ? errno : EIO;
#else
    if (mkdir(path.c_str(), 0755) != 0) return errno ? errno : EIO;
#endif
    return 0;
}

// 目录遍历 (机制): 对 path 下每个非 "."/".." 项调用 fn(子项完整路径, kind)。
// kind: 0 = 常规文件; 1 = 目录; 2 = 其他 (符号链接/fifo/设备等, 不跟随)。
// 不跟随符号链接 (POSIX 用 lstat 判类型; Windows 用 dwFileAttributes)。
// 返回 0 = 遍历正常结束; 非 0 = 无法打开目录 **或 lstat 失败** (errno)。
// fn 返回非 0 ⇒ 立即中止, 本函数返回 0 且 abort_rc 记该值 (若 abort_rc 非空)。
inline int for_each_child(const std::string& path,
                          const std::function<int(const std::string&, int)>& fn,
                          int* abort_rc) {
    if (abort_rc) *abort_rc = 0;
    if (!fn) return EINVAL;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    const std::string pat = path + "\\*";
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return errno ? errno : EIO;
    do {
        if (std::strcmp(fd.cFileName, ".") == 0 ||
            std::strcmp(fd.cFileName, "..") == 0) continue;
        const int is_dir =
            (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        const int rc = fn(path + "\\" + fd.cFileName, is_dir);
        if (rc != 0) { FindClose(h); if (abort_rc) *abort_rc = rc; return 0; }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return 0;
#else
    DIR* d = opendir(path.c_str());
    if (!d) return errno ? errno : EIO;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (std::strcmp(e->d_name, ".") == 0 ||
            std::strcmp(e->d_name, "..") == 0) continue;
        const std::string child = path + "/" + e->d_name;
        struct stat st;
        if (lstat(child.c_str(), &st) != 0) {           // 病态项: 上报 (fail-closed)
            const int le = errno ? errno : EIO;
            closedir(d);
            return le;
        }
        const int kind = S_ISDIR(st.st_mode) ? 1 : (S_ISREG(st.st_mode) ? 0 : 2);
        const int rc = fn(child, kind);
        if (rc != 0) { closedir(d); if (abort_rc) *abort_rc = rc; return 0; }
    }
    closedir(d);
    return 0;
#endif
}

// 目录是否非空 (0=空/不存在; 1=非空)。ok 回填"枚举成功"。
inline int dir_is_nonempty(const std::string& path, int* ok) {
    if (ok) *ok = 1;
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    const std::string pat = path + "\\*";
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) { if (ok) *ok = 0; return 0; }
    int n = 0;
    do {
        if (std::strcmp(fd.cFileName, ".") == 0 ||
            std::strcmp(fd.cFileName, "..") == 0) continue;
        ++n;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return n != 0 ? 1 : 0;
#else
    DIR* d = opendir(path.c_str());
    if (!d) { if (ok) *ok = 0; return 0; }
    struct dirent* e;
    int n = 0;
    while ((e = readdir(d)) != NULL) {
        if (std::strcmp(e->d_name, ".") == 0 ||
            std::strcmp(e->d_name, "..") == 0) continue;
        ++n;
    }
    closedir(d);
    return n != 0 ? 1 : 0;
#endif
}

// 文件或目录 fsync (落盘屏障)。返回 0 或 errno。
// Windows: _commit 要求可写句柄 (p3_output R18 34201181796 诊断 errno=9 实证),
// 故以 _O_RDWR 打开 (不改内容); 目录句柄在 Windows 无 fsync 语义 ⇒ 尽力模式
// (返回 0, 目录元数据随 promote 的 rename 收敛 — 与 publish.h 登记一致)。
inline int fsync_path(const std::string& path, int is_dir) {
#ifdef _WIN32
    if (is_dir) return 0;                       // Windows 目录 fsync: 尽力模式
    const int fd = _open(path.c_str(), _O_RDWR | _O_BINARY);
    if (fd < 0) return errno ? errno : EIO;
    int rc = (_commit(fd) != 0) ? (errno ? errno : EIO) : 0;
    if (_close(fd) != 0 && rc == 0) rc = errno ? errno : EIO;
    return rc;
#else
    const int fd = is_dir ? open(path.c_str(), O_RDONLY | O_DIRECTORY)
                          : open(path.c_str(), O_RDONLY);
    if (fd < 0) return errno ? errno : EIO;
    int rc = (fsync(fd) != 0) ? (errno ? errno : EIO) : 0;
    if (close(fd) != 0 && rc == 0) rc = errno ? errno : EIO;
    return rc;
#endif
}

// 删除单个文件 (不存在 = 成功, 幂等)。返回 0 或 errno。
inline int remove_file(const std::string& path) {
#ifdef _WIN32
    if (_unlink(path.c_str()) != 0 && errno != ENOENT) return errno;
#else
    if (unlink(path.c_str()) != 0 && errno != ENOENT) return errno;
#endif
    return 0;
}

// 递归删除文件或目录 (不跟随符号链接; 深度上限 64 防御病态输入)。
// 返回 0 或 errno; 目标不存在 → 0 (幂等)。
inline int remove_tree(const std::string& path, int depth) {
    if (depth > 64) return ELOOP;
    int is_dir = 0;
    if (!path_exists(path, &is_dir)) return 0;
    if (!is_dir) return remove_file(path);
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    const std::string pat = path + "\\*";
    HANDLE h = FindFirstFileA(pat.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (std::strcmp(fd.cFileName, ".") == 0 ||
                std::strcmp(fd.cFileName, "..") == 0) continue;
            const int rc = remove_tree(path + "\\" + fd.cFileName, depth + 1);
            if (rc != 0) { FindClose(h); return rc; }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    if (_rmdir(path.c_str()) != 0 && errno != ENOENT) return errno;
#else
    DIR* d = opendir(path.c_str());
    if (!d) return errno ? errno : EIO;
    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        if (std::strcmp(e->d_name, ".") == 0 ||
            std::strcmp(e->d_name, "..") == 0) continue;
        const int rc = remove_tree(path + "/" + e->d_name, depth + 1);
        if (rc != 0) { closedir(d); return rc; }
    }
    closedir(d);
    if (rmdir(path.c_str()) != 0 && errno != ENOENT) return errno;
#endif
    return 0;
}

// 整树原子改名 (stage → final): 同一文件系统内内核原子。
// POSIX = rename(2); Windows = MoveFileExA(REPLACE_EXISTING)。
enum AioPromoteResult {
    PROMOTE_OK = 0,
    PROMOTE_CROSS_DEVICE = 1,   /* EXDEV / ERROR_NOT_SAME_DEVICE */
    PROMOTE_STATE = 2,          /* 目标被占用 / 不存在等状态类 */
    PROMOTE_IO = 3,             /* 其他 I/O 失败 */
};

inline int promote_dir(const std::string& stage_path, const std::string& final_path) {
#ifdef _WIN32
    if (!MoveFileExA(stage_path.c_str(), final_path.c_str(),
                     MOVEFILE_REPLACE_EXISTING)) {
        return (GetLastError() == ERROR_NOT_SAME_DEVICE) ? PROMOTE_CROSS_DEVICE
                                                         : PROMOTE_STATE;
    }
    return PROMOTE_OK;
#else
    if (rename(stage_path.c_str(), final_path.c_str()) != 0) {
        if (errno == EXDEV) return PROMOTE_CROSS_DEVICE;
        if (errno == ENOTEMPTY || errno == EEXIST || errno == ENOENT)
            return PROMOTE_STATE;
        return PROMOTE_IO;
    }
    return PROMOTE_OK;
#endif
}

// 父目录 fsync (rename 元数据落盘; 尽力 — 失败不使已完成的 rename 回滚)。
inline void fsync_parent_dir(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos || slash == 0) return;
    (void)fsync_path(path.substr(0, slash), 1);
}

}  // namespace aio_atomic

#endif  // AIO_ATOMIC_FILE_H
