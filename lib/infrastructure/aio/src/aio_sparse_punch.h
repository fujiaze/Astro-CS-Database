#ifndef AIO_SPARSE_PUNCH_H
#define AIO_SPARSE_PUNCH_H

// ============================================================================
// aio_sparse_punch.h - 裸形态体积削减「文件系统打洞」机制原语 (header-only; aio 唯一实现)
//
// 依据:
// - ASTROCS_DESIGN.md §10「I/O 与原子产品」(裸形态体积削减两种机制的分工);
// - docs/design/PRODUCT_STORAGE_FORM.md §9.1 (何时 / 对谁 / 失败怎么办 / 如何验证);
// - docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md §7 表 T1 (冻结规则 + 判据);
// - ENGINEERING_SPEC.md §11 (打洞在 fsync 之后、算哈希与原子发布之前完成)。
//
// 语义:
// - 只对**字面零字节**的、块对齐的整块区域打洞。判据是**字节比较**：本文件
//   不出现任何浮点类型、浮点常量与浮点比较。这是硬性设计约束而非风格偏好 ——
//   IEEE-754 的 -0.0 在浮点比较下等于 0.0，但其位型 0x80000000 含非零字节；
//   若用浮点等值判定可打洞，就会把 -0.0 区打成洞并**改变文件字节**。
// - signal 层边距是 IEEE NaN (0x7FC00000 等)，其位型含非零字节 ⇒ 按构造落入
//   「不可打洞」。合同要求 signal 边距必须是 NaN (docs/contracts/DATA_SEMANTICS.md)，
//   把它改写成 0.0 会把「无覆盖」变成「有效零流量」⇒ 语义破坏，禁止。
// - 文件逻辑尺寸 (st_size) 与整文件字节**逐字节不变**；只有文件分配层变化。
// - 卷/文件系统不支持 (EOPNOTSUPP 等) ⇒ 返回 PUNCH_UNSUPPORTED，**一个字节都不动**，
//   调用方按降级处理 (跳过 + 记 warn)，不 fail-closed。
// - 打洞后**读回复算**整文件 sha256 与打洞前比对；不一致 ⇒ PUNCH_VERIFY_MISMATCH
//   (硬错误：文件已被破坏，不得发布)。
//
// 平台:
// - Linux: fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, off, len)。
// - Windows: FSCTL_SET_SPARSE (置稀疏) + FSCTL_SET_ZERO_DATA (区间归零并打洞)。
//   Windows 释放粒度 64 KiB 且要求 64 KiB 对齐；非稀疏文件上 SET_ZERO_DATA
//   **不报错、只原地写零**，故必须事后用 GetCompressedFileSizeW 验证是否真释放。
//   已登记的未实测项：Windows 分支本机无运行环境，只按一手文档实现 (见
//   ACCEPTANCE_SPEC.md §3.2 的未实测登记)。
// ============================================================================

#include "aio_atomic_file.h"   // fsync_path / remove_file / make_tmp_path
#include "aio_file_io.h"       // sha256_hex
#include "aio_log.h"
#include "aio_util.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/falloc.h>      // FALLOC_FL_PUNCH_HOLE / FALLOC_FL_KEEP_SIZE
#endif
#endif

namespace aio_sparse {

// ── 打洞粒度 ────────────────────────────────────────────────────────────────
// 合同 §7 T1 冻结「4 KiB 对齐的整块全零区域」。Windows 的 FSCTL_SET_ZERO_DATA
// 释放粒度是 64 KiB 且必须 64 KiB 对齐 ⇒ 该平台按 64 KiB 取块（更粗，仍然只覆盖
// 全零区，字节不变性不受影响）。
#ifdef _WIN32
constexpr std::uint64_t kPunchBlockBytes = 65536ULL;
#else
constexpr std::uint64_t kPunchBlockBytes = 4096ULL;
#endif

// 扫描缓冲 1 MiB（必为块粒度的整数倍：Linux 256 块 / Windows 16 块）。
constexpr std::size_t kScanBufBytes = static_cast<std::size_t>(1) << 20;
static_assert(kScanBufBytes % static_cast<std::size_t>(kPunchBlockBytes) == 0,
              "scan buffer must be an integer number of punch blocks");

// ── 返回码 ──────────────────────────────────────────────────────────────────
enum PunchRc {
    PUNCH_OK = 0,               // 完成（punched_bytes == 0 也是合法结果：无全零块）
    PUNCH_UNSUPPORTED = 1,      // 卷/文件系统不支持打洞（或策略关闭）⇒ 降级，不 fail-closed
    PUNCH_VERIFY_MISMATCH = 2,  // 打洞后读回与打洞前不一致 ⇒ 硬错误（文件已破坏）
    PUNCH_IO_ERROR = 3,         // 打开/读取/stat 失败
    PUNCH_NO_RELEASE = 4,       // 洞提交了但分配字节未下降 ⇒ 判红（不得当成功）
};

// ── 打洞结果（provenance / 验收证据的机器可读载荷）─────────────────────────
struct PunchResult {
    int rc = PUNCH_OK;
    int sys_errno = 0;              // 原始系统错误码（诊断）
    std::uint64_t size_bytes = 0;   // st_size（打洞前后必须相同）
    std::uint64_t alloc_before = 0; // 打洞前已分配字节（POSIX st_blocks*512）
    std::uint64_t alloc_after = 0;  // 打洞后已分配字节
    std::uint64_t zero_blocks = 0;  // 扫出的可打洞块数
    std::uint64_t punched_bytes = 0;// 提交打洞的字节数（块粒度整数倍）
    std::uint32_t holes = 0;        // 打洞区间段数
    std::string sha256_before;      // 打洞前整文件 sha256
    std::string sha256_after;       // 打洞后**读回**整文件 sha256
    bool verified = false;          // 读回逐字节一致
    std::string reason;             // 降级/失败原因（trim=skipped(<reason>) 的载荷）

    bool punched() const { return punched_bytes != 0; }
    bool degraded() const { return rc == PUNCH_UNSUPPORTED; }
    // 释放字节数（打洞成功时 > 0）。
    std::uint64_t released_bytes() const {
        return alloc_before > alloc_after ? alloc_before - alloc_after : 0;
    }
};

// ── 全零块判定（红线本体：字节级，无浮点解释）──────────────────────────────
// 任一非零字节 ⇒ 不可打洞。NaN 与 -0.0 的位型都含非零字节 ⇒ 按构造被排除。
inline bool block_is_literal_zero(const unsigned char* p, std::size_t n) {
    if (!p) return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (p[i] != 0) return false;
    }
    return true;
}

// 判据辅助（**非打洞路径**）：块内是否含 IEEE-754 binary32 的 NaN 位型。
// 用途：把「NaN 区不得打洞」做成可执行断言 —— 对任意含 NaN 的块，
// block_is_literal_zero 必须为 false。按大端与小端两种解释各扫一遍，
// 避免把 FITS 的大端存储误判成非 NaN。
inline bool block_contains_binary32_nan(const unsigned char* p, std::size_t n) {
    if (!p) return false;
    for (std::size_t i = 0; i + 4 <= n; i += 4) {
        const std::uint32_t be = (static_cast<std::uint32_t>(p[i]) << 24) |
                                 (static_cast<std::uint32_t>(p[i + 1]) << 16) |
                                 (static_cast<std::uint32_t>(p[i + 2]) << 8) |
                                 static_cast<std::uint32_t>(p[i + 3]);
        const std::uint32_t le = static_cast<std::uint32_t>(p[i]) |
                                 (static_cast<std::uint32_t>(p[i + 1]) << 8) |
                                 (static_cast<std::uint32_t>(p[i + 2]) << 16) |
                                 (static_cast<std::uint32_t>(p[i + 3]) << 24);
        const std::uint32_t exp = 0x7F800000u;
        const std::uint32_t man = 0x007FFFFFu;
        if (((be & exp) == exp && (be & man) != 0) ||
            ((le & exp) == exp && (le & man) != 0)) {
            return true;
        }
    }
    return false;
}

// ── 上报钩子（默认走 aio_log；探针/单元测试可替换以免依赖日志后端）──────────
using SinkFn = void (*)(int level, const char* msg);
inline SinkFn& sink_hook() {
    static SinkFn f = nullptr;
    return f;
}
inline void set_sink(SinkFn f) { sink_hook() = f; }
inline void report(int level, const std::string& msg) {
    if (sink_hook()) {
        sink_hook()(level, msg.c_str());
        return;
    }
    aio_log(level, "aio_sparse", "%s", msg.c_str());
}

// ── 运行级策略（默认启用；调用方按形态显式关闭）────────────────────────────
// 归档形态不实施打洞（docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md §7「生效面」）。
inline bool& punch_enabled_ref() {
    static bool v = true;
    return v;
}
inline void set_punch_enabled(bool on) { punch_enabled_ref() = on; }
inline bool punch_enabled() { return punch_enabled_ref(); }

// 注入面（测试专用；未设置时零行为差异）：ASTROCS_SPARSE_PUNCH_FAULT=unsupported
// 等价于「卷不支持打洞」，用于证明降级路径可执行且不 fail-closed。
inline bool fault_unsupported() {
    const char* v = std::getenv("ASTROCS_SPARSE_PUNCH_FAULT");
    return v != nullptr && std::strcmp(v, "unsupported") == 0;
}

inline const char* errno_name(int e) {
    switch (e) {
        case 0: return "OK";
#ifdef EOPNOTSUPP
        case EOPNOTSUPP: return "EOPNOTSUPP";
#endif
#ifdef ENOTSUP
#if !defined(EOPNOTSUPP) || (ENOTSUP != EOPNOTSUPP)
        case ENOTSUP: return "ENOTSUP";
#endif
#endif
#ifdef EINVAL
        case EINVAL: return "EINVAL";
#endif
#ifdef ENOSYS
        case ENOSYS: return "ENOSYS";
#endif
#ifdef EPERM
        case EPERM: return "EPERM";
#endif
#ifdef ENODEV
        case ENODEV: return "ENODEV";
#endif
#ifdef EXDEV
        case EXDEV: return "EXDEV";
#endif
#ifdef EIO
        case EIO: return "EIO";
#endif
        default: return "ERRNO";
    }
}

// 「不支持打洞」的错误码族 ⇒ 降级；其余错误码 ⇒ I/O 失败。
// Windows 侧无一手 GetLastError 清单（协议层是 STATUS_INVALID_DEVICE_REQUEST），
// 故该平台一律按「不支持」处理，并由事后 GetCompressedFileSizeW 验证是否真释放。
inline bool errno_is_unsupported(int e) {
#ifdef _WIN32
    (void)e;
    return true;
#else
    if (e == EOPNOTSUPP) return true;
#ifdef ENOTSUP
    if (e == ENOTSUP) return true;
#endif
    if (e == EINVAL) return true;
    if (e == ENOSYS) return true;
    if (e == EPERM) return true;
    if (e == ENODEV) return true;
    if (e == EXDEV) return true;
    return false;
#endif
}

// ── 平台机制 ────────────────────────────────────────────────────────────────

// 文件尺寸与已分配字节。返回 true = 已回填。
inline bool file_metrics(const std::string& path, std::uint64_t* size,
                         std::uint64_t* alloc) {
#ifdef _WIN32
    struct _stat64 st;
    if (_stat64(path.c_str(), &st) != 0) return false;
    if (size) *size = static_cast<std::uint64_t>(st.st_size);
    if (alloc) {
        DWORD high = 0;
        const DWORD low = GetCompressedFileSizeW(
            aio_atomic::widen_utf8(path).c_str(), &high);
        if (low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) return false;
        *alloc = (static_cast<std::uint64_t>(high) << 32) | low;
    }
    return true;
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    if (size) *size = static_cast<std::uint64_t>(st.st_size);
    if (alloc) *alloc = static_cast<std::uint64_t>(st.st_blocks) * 512ULL;
    return true;
#endif
}

// 单区间打洞。off/len 必须块对齐。返回 0 或系统错误码。
inline int punch_range_at(int fd, std::uint64_t off, std::uint64_t len,
                          bool* need_sparse) {
    if (len == 0) return 0;
#ifdef _WIN32
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (h == INVALID_HANDLE_VALUE) return EIO;
    DWORD ret = 0;
    if (need_sparse && *need_sparse) {
        if (!DeviceIoControl(h, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &ret,
                             nullptr)) {
            return EIO;
        }
        *need_sparse = false;
    }
    FILE_ZERO_DATA_INFORMATION z;
    z.FileOffset.QuadPart = static_cast<LONGLONG>(off);
    z.BeyondFinalZero.QuadPart = static_cast<LONGLONG>(off + len);
    if (!DeviceIoControl(h, FSCTL_SET_ZERO_DATA, &z, sizeof(z), nullptr, 0, &ret,
                         nullptr)) {
        return EIO;
    }
    return 0;
#else
    (void)need_sparse;
#if defined(__linux__) && defined(FALLOC_FL_PUNCH_HOLE)
    if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                  static_cast<off_t>(off), static_cast<off_t>(len)) != 0) {
        return errno ? errno : EIO;
    }
    return 0;
#else
    (void)fd; (void)off; (void)len;
    return EOPNOTSUPP;   // 非 Linux 平台无本机制 ⇒ 降级
#endif
#endif
}

// 位置读（不改文件游标；打洞扫描用）。返回 true = 恰读 count 字节。
inline bool read_at(int fd, std::uint64_t off, unsigned char* buf,
                    std::size_t count) {
    std::size_t done = 0;
    while (done < count) {
#ifdef _WIN32
        const int n = _read(fd, buf + done, static_cast<unsigned>(count - done));
#else
        const ssize_t n = pread(fd, buf + done, count - done,
                                static_cast<off_t>(off + done));
#endif
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}

// 丢弃页缓存（打洞后**从设备重读**，使读回复算真的能检出字节破坏）。
inline void drop_page_cache(int fd) {
#ifdef _WIN32
    HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (h != INVALID_HANDLE_VALUE) FlushFileBuffers(h);
#else
#if defined(POSIX_FADV_DONTNEED)
    (void)posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);
#else
    (void)fd;
#endif
#endif
}

// 打开（读写；打洞需要可写句柄）。返回 fd 或 -1。
inline int open_rw(const std::string& path) {
#ifdef _WIN32
    return _open(path.c_str(), _O_RDWR | _O_BINARY);
#else
    return open(path.c_str(), O_RDWR);
#endif
}

inline void close_fd(int fd) {
    if (fd < 0) return;
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
}

// ── 卷能力探测（按父目录缓存；探测文件与目标同目录 ⇒ 同卷同文件系统）────────
inline std::map<std::string, std::pair<int, std::string>>& probe_cache() {
    static std::map<std::string, std::pair<int, std::string>> c;
    return c;
}
inline std::mutex& probe_mutex() {
    static std::mutex m;
    return m;
}

inline std::string parent_dir_of(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return std::string(".");
    if (slash == 0) return std::string("/");
    return path.substr(0, slash);
}

// 在 dir 所在卷上真打一次洞（探针文件，写完即删），据此判定支持性。
// supported 回填 1/0；reason 回填降级原因（不支持时）。
inline void volume_supports_punch(const std::string& dir, int* supported,
                                  std::string* reason) {
    if (supported) *supported = 0;
    if (reason) reason->clear();
    if (!punch_enabled()) {
        if (reason) *reason = "disabled";
        return;
    }
    if (fault_unsupported()) {
        if (reason) *reason = std::string("unsupported(") + errno_name(EOPNOTSUPP) +
                              ", injected)";
        return;
    }
    const std::string key = dir.empty() ? std::string(".") : dir;
    {
        std::lock_guard<std::mutex> lk(probe_mutex());
        std::map<std::string, std::pair<int, std::string>>& c = probe_cache();
        std::map<std::string, std::pair<int, std::string>>::const_iterator it =
            c.find(key);
        if (it != c.end()) {
            if (supported) *supported = it->second.first;
            if (reason) *reason = it->second.second;
            return;
        }
    }
    // 探测：写一个恰好一块的全零文件 → fsync → 打洞 → 判 rc → 删除。
    int sup = 0;
    std::string why = "probe_failed";
    const std::string probe = aio_atomic::make_tmp_path(
        (key == "/" ? std::string("/") : key + "/") + ".astrocs_sparse_probe");
    FILE* f = aio_fopen_utf8(probe.c_str(), "wb");
    if (f) {
        const std::size_t blk = static_cast<std::size_t>(kPunchBlockBytes);
        std::string zeros(blk, '\0');
        const bool wrote = (std::fwrite(zeros.data(), 1, blk, f) == blk);
        const bool flushed = (std::fflush(f) == 0);
        if (std::fclose(f) != 0 || !wrote || !flushed) {
            why = "probe_write_failed";
        } else if (aio_atomic::fsync_path(probe, 0) != 0) {
            why = "probe_fsync_failed";
        } else {
            const int fd = open_rw(probe);
            if (fd < 0) {
                why = "probe_open_failed";
            } else {
                bool need_sparse = true;
                const int e = punch_range_at(fd, 0, kPunchBlockBytes, &need_sparse);
                close_fd(fd);
                if (e == 0) {
                    sup = 1;
                    why.clear();
                } else {
                    why = std::string("unsupported(") + errno_name(e) + ")";
                }
            }
        }
        aio_atomic::remove_file(probe);
    } else {
        why = "probe_create_failed";
    }
    {
        std::lock_guard<std::mutex> lk(probe_mutex());
        probe_cache()[key] = std::make_pair(sup, why);
    }
    if (supported) *supported = sup;
    if (reason) *reason = why;
}

// 清空能力探测缓存（测试/换卷时用）。
inline void reset_probe_cache() {
    std::lock_guard<std::mutex> lk(probe_mutex());
    probe_cache().clear();
}

// ── 主入口 ──────────────────────────────────────────────────────────────────

// 对 path 内**全部块对齐的、字面全零的**区域打洞。
// verify=true 时做「读回复算」：打洞前记整文件 sha256，打洞后丢页缓存重读再比对。
// 返回 PunchRc；明细写入 out（非空时）。
inline int punch_all_zero_blocks(const std::string& path, PunchResult* out,
                                 bool verify) {
    PunchResult r;
    if (out) *out = r;
    const auto rc_final = [&]() -> int {
        if (out) *out = r;
        return r.rc;
    };

    if (path.empty()) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "empty_path";
        return rc_final();
    }

    std::uint64_t size = 0;
    if (!file_metrics(path, &size, &r.alloc_before)) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "stat_failed";
        return rc_final();
    }
    r.size_bytes = size;
    if (size == 0) {                       // 空文件：无事可做，也不是失败
        r.rc = PUNCH_OK;
        r.alloc_after = r.alloc_before;
        r.verified = verify;
        return rc_final();
    }

    // 打洞前整文件 sha256（读回复算的参考；读失败 ⇒ I/O 错误，不得当成功）。
    if (verify && !aio_file::sha256_hex(path.c_str(), &r.sha256_before)) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "pre_hash_failed";
        return rc_final();
    }

    // 卷能力：不支持 ⇒ **纯降级**（一个字节都不动）。
    int supported = 0;
    std::string preason;
    volume_supports_punch(parent_dir_of(path), &supported, &preason);
    if (!supported) {
        r.rc = PUNCH_UNSUPPORTED;
        r.reason = preason.empty() ? "unsupported" : preason;
        return rc_final();
    }

    const int fd = open_rw(path);
    if (fd < 0) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "open_failed";
        r.sys_errno = errno;
        return rc_final();
    }

    // 扫描：块对齐、逐块判「字面全零」。EOF 之外的补零字节按隐式零处理
    // （把末块补到块边界可释放尾部块；st_size 不变，读回语义不变）。
    const std::uint64_t total_blocks =
        (size + kPunchBlockBytes - 1) / kPunchBlockBytes;
    const std::uint64_t buf_blocks =
        static_cast<std::uint64_t>(kScanBufBytes) / kPunchBlockBytes;
    std::string buf(kScanBufBytes, '\0');
    bool need_sparse = true;
    bool in_run = false;
    bool punch_failed = false;
    std::uint64_t run_start = 0;

    const auto flush_run = [&](std::uint64_t end_block) -> bool {
        const std::uint64_t off = run_start * kPunchBlockBytes;
        const std::uint64_t len = (end_block - run_start) * kPunchBlockBytes;
        const int e = punch_range_at(fd, off, len, &need_sparse);
        if (e != 0) {
            r.sys_errno = e;
            r.reason = std::string("punch_failed(") + errno_name(e) + ")";
            r.rc = errno_is_unsupported(e) ? PUNCH_UNSUPPORTED : PUNCH_IO_ERROR;
            return false;
        }
        ++r.holes;
        r.punched_bytes += len;
        return true;
    };

    std::uint64_t b = 0;
    while (b < total_blocks && !punch_failed) {
        const std::uint64_t chunk_blocks =
            std::min<std::uint64_t>(buf_blocks, total_blocks - b);
        const std::uint64_t chunk_off = b * kPunchBlockBytes;
        const std::uint64_t chunk_len = chunk_blocks * kPunchBlockBytes;
        const std::uint64_t in_file =
            std::min<std::uint64_t>(chunk_len, size - chunk_off);
        if (in_file != 0 &&
            !read_at(fd, chunk_off, reinterpret_cast<unsigned char*>(&buf[0]),
                     static_cast<std::size_t>(in_file))) {
            r.rc = PUNCH_IO_ERROR;
            r.reason = "scan_read_failed";
            close_fd(fd);
            return rc_final();
        }
        if (in_file < chunk_len) {   // EOF 之外：隐式零
            std::memset(&buf[static_cast<std::size_t>(in_file)], 0,
                        static_cast<std::size_t>(chunk_len - in_file));
        }
        for (std::uint64_t i = 0; i < chunk_blocks; ++i) {
            const unsigned char* p =
                reinterpret_cast<const unsigned char*>(&buf[0]) +
                static_cast<std::size_t>(i * kPunchBlockBytes);
            const bool zero =
                block_is_literal_zero(p, static_cast<std::size_t>(kPunchBlockBytes));
            if (zero) {
                if (!in_run) { run_start = b + i; in_run = true; }
                ++r.zero_blocks;
            } else if (in_run) {
                if (!flush_run(b + i)) { punch_failed = true; break; }
                in_run = false;
            }
        }
        b += chunk_blocks;
    }
    if (in_run && !punch_failed) {
        if (!flush_run(total_blocks)) punch_failed = true;
    }
    if (r.rc == PUNCH_OK && r.punched_bytes != 0) {
        (void)aio_atomic::fsync_path(path, 0);   // 洞的分配元数据落盘（尽力）
    }
    close_fd(fd);

    // 读回复算：丢页缓存 → 从设备重读 → 与打洞前逐字节比较（sha256）。
    if (verify) {
        const int vfd = open_rw(path);
        if (vfd >= 0) {
            drop_page_cache(vfd);
            close_fd(vfd);
        }
        if (!aio_file::sha256_hex(path.c_str(), &r.sha256_after)) {
            r.rc = PUNCH_IO_ERROR;
            r.reason = "post_hash_failed";
        } else if (r.sha256_after != r.sha256_before) {
            r.rc = PUNCH_VERIFY_MISMATCH;
            r.reason = "readback_mismatch";
        } else {
            r.verified = true;
        }
    }

    std::uint64_t size2 = 0;
    if (!file_metrics(path, &size2, &r.alloc_after)) {
        if (r.rc == PUNCH_OK) {
            r.rc = PUNCH_IO_ERROR;
            r.reason = "post_stat_failed";
        }
        return rc_final();
    }
    if (size2 != r.size_bytes && r.rc == PUNCH_OK) {
        r.rc = PUNCH_VERIFY_MISMATCH;
        r.reason = "size_changed";
        return rc_final();
    }
    // 判据（合同 §7 T1 ⑤）：洞提交了但分配字节没下降 ⇒ 判红，不得当成功。
    if (r.rc == PUNCH_OK && r.punched_bytes != 0 &&
        r.alloc_after >= r.alloc_before) {
        r.rc = PUNCH_NO_RELEASE;
        r.reason = "no_release";
    }
    return rc_final();
}

// ── 负例注入面（**仅供判据/探针**；生产路径只用 punch_all_zero_blocks）──────
// 对指定区间强制打洞，**不做**全零校验。对含非零字节的区间调用必然使读回复算
// 判红（PUNCH_VERIFY_MISMATCH）——这正是「对 NaN 区打洞 ⇒ 判红」的注入通道。
// 前置条件：off 与 len 均为 kPunchBlockBytes 的整数倍。
inline int punch_range_forced(const std::string& path, std::uint64_t off,
                              std::uint64_t len, PunchResult* out, bool verify) {
    PunchResult r;
    if (out) *out = r;
    const auto rc_final = [&]() -> int {
        if (out) *out = r;
        return r.rc;
    };
    if (path.empty() || len == 0 || (off % kPunchBlockBytes) != 0 ||
        (len % kPunchBlockBytes) != 0) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "bad_range";
        return rc_final();
    }
    std::uint64_t size = 0;
    if (!file_metrics(path, &size, &r.alloc_before)) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "stat_failed";
        return rc_final();
    }
    r.size_bytes = size;
    if (verify && !aio_file::sha256_hex(path.c_str(), &r.sha256_before)) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "pre_hash_failed";
        return rc_final();
    }
    const int fd = open_rw(path);
    if (fd < 0) {
        r.rc = PUNCH_IO_ERROR;
        r.reason = "open_failed";
        return rc_final();
    }
    bool need_sparse = true;
    const int e = punch_range_at(fd, off, len, &need_sparse);
    close_fd(fd);
    if (e != 0) {
        r.rc = errno_is_unsupported(e) ? PUNCH_UNSUPPORTED : PUNCH_IO_ERROR;
        r.reason = std::string("punch_failed(") + errno_name(e) + ")";
        r.sys_errno = e;
        return rc_final();
    }
    r.holes = 1;
    r.punched_bytes = len;
    if (verify) {
        const int vfd = open_rw(path);
        if (vfd >= 0) {
            drop_page_cache(vfd);
            close_fd(vfd);
        }
        if (!aio_file::sha256_hex(path.c_str(), &r.sha256_after)) {
            r.rc = PUNCH_IO_ERROR;
            r.reason = "post_hash_failed";
        } else if (r.sha256_after != r.sha256_before) {
            r.rc = PUNCH_VERIFY_MISMATCH;
            r.reason = "readback_mismatch";
        } else {
            r.verified = true;
        }
    }
    (void)file_metrics(path, &size, &r.alloc_after);
    return rc_final();
}

}  // namespace aio_sparse

#endif  // AIO_SPARSE_PUNCH_H
