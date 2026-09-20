// lib/infrastructure/cli/disk_gate.h — 磁盘门（**唯一资源判据**）
//
// 规范依据（逐条）：
//   * ASTROCS_DESIGN.md §3.5「资源门只管磁盘：运行前磁盘余量不足 ⇒ 报 warn（不阻断）；
//     运行中写盘失败/磁盘满 ⇒ 报错（fail-closed）；内存 / CPU / 线程不设门」；
//   * ASTROCS_DESIGN.md §6.3 退出码表「10 = 磁盘写满 / 写盘失败（一般性资源超限门已取消）」；
//   * GAP_AUDIT.md（RELEASE-02）§9.74 裁决 10（负责人逐字：「不应该有资源超限（除非存储不足）。
//     那个问题直接在跑前报 warn，写入磁盘满了报错……只考虑磁盘写满这一个问题」）；
//   * GAP_AUDIT.md（RELEASE-03）§4.3 Q6（运行事件流唯一 schema = protocol.h/jsonl.h）。
//
// 纪律：
//   1) 本模块**不设**任何内存/CPU/线程门；exit 10 只由 write_failure_is_resource_exit() 判定；
//   2) 预检 warn **不阻断**：它只进预检页（subcommand.h），不参与 has_error() 判定；
//   3) 运行期 fail-closed：写盘失败/磁盘满 ⇒ error 事件 + exit 10（由调用方按本模块判定落码）；
//   4) **阈值自由**：本模块不引入经验系数/魔法字节数 —— 预估需求下限 = 配置声明输入的字节和
//      （「喂进去多少，至少要能再放得下多少」），可用空间由 statvfs/GetDiskFreeSpaceEx 实测。
//      ⇒ 无需 contracts/resource_gate_v1.json 新增数值键（该文件仍是资源**观测**阈值唯一数值源）。
#pragma once

#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif

namespace astrocs {

// ── 磁盘余量探测（唯一实现；output_dir 可尚未创建 → 向上找最近已存在目录）──
struct DiskSpace {
    bool probed = false;        // statvfs/GetDiskFreeSpaceEx 成功
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;    // 调用方可用空间（POSIX: f_bavail × f_frsize）
    int err = 0;                // 探测失败原因（errno / GetLastError）
};

inline DiskSpace disk_space_of(const std::string& raw_path) {
    DiskSpace sp;
    if (raw_path.empty()) { sp.err = ENOENT; return sp; }
    std::filesystem::path p = std::filesystem::u8path(raw_path);
    std::error_code ec;
    for (int i = 0; i < 64 && !p.empty(); ++i) {
        std::error_code e2;
        if (std::filesystem::exists(p, e2)) {
            if (!std::filesystem::is_directory(p, e2)) p = p.parent_path();
            break;
        }
        p = p.parent_path();
    }
    if (p.empty()) p = std::filesystem::current_path(ec);
#if defined(_WIN32)
    ULARGE_INTEGER avail{}, total{}, free_bytes{};
    if (GetDiskFreeSpaceExW(p.wstring().c_str(), &avail, &total, &free_bytes)) {
        sp.probed = true;
        sp.total_bytes = static_cast<uint64_t>(total.QuadPart);
        sp.free_bytes = static_cast<uint64_t>(avail.QuadPart);
    } else {
        sp.err = static_cast<int>(GetLastError());
    }
#else
    struct statvfs vfs {};
    const std::string ps = p.string();
    if (::statvfs(ps.c_str(), &vfs) == 0) {
        sp.probed = true;
        sp.total_bytes = static_cast<uint64_t>(vfs.f_blocks) * vfs.f_frsize;
        sp.free_bytes = static_cast<uint64_t>(vfs.f_bavail) * vfs.f_frsize;
    } else {
        sp.err = errno;
    }
#endif
    return sp;
}

// ── 输入规模预估（预估需求**下限**；目录扫描有界，防预检期大 I/O）──
inline constexpr std::size_t kDiskEstimateMaxEntries = 20000;

struct DiskEstimate {
    uint64_t input_bytes = 0;   // 配置声明输入的字节和（常规文件；目录有界递归）
    std::size_t files = 0;
    std::size_t dirs = 0;
    bool truncated = false;     // 目录扫描达到 kDiskEstimateMaxEntries（下界，非全量）
};

inline uint64_t path_bytes_bounded(const std::string& raw, DiskEstimate* est) {
    if (raw.empty()) return 0;
    std::error_code ec;
    const std::filesystem::path p = std::filesystem::u8path(raw);
    if (std::filesystem::is_regular_file(p, ec)) {
        const auto sz = std::filesystem::file_size(p, ec);
        if (ec) return 0;
        if (est != nullptr) ++est->files;
        return static_cast<uint64_t>(sz);
    }
    if (std::filesystem::is_directory(p, ec)) {
        if (est != nullptr) ++est->dirs;
        uint64_t sum = 0;
        std::size_t n = 0;
        std::error_code it_ec;
        for (std::filesystem::recursive_directory_iterator it(
                 p, std::filesystem::directory_options::skip_permission_denied, it_ec);
             !it_ec && it != std::filesystem::recursive_directory_iterator(); it.increment(it_ec)) {
            if (++n > kDiskEstimateMaxEntries) {
                if (est != nullptr) est->truncated = true;
                break;
            }
            std::error_code e2;
            if (it->is_regular_file(e2)) {
                const auto sz = it->file_size(e2);
                if (!e2) {
                    sum += static_cast<uint64_t>(sz);
                    if (est != nullptr) ++est->files;
                }
            }
        }
        return sum;
    }
    return 0;   // 不存在/其它类型：预检面另由 input_path_errors 报 error（本模块不重复报）
}

// 一次运行的磁盘预估作用域（一块 = 一次运行 = 一个 output_dir；§9.68/§9.71 裁决 2）
struct DiskScope {
    std::string output_dir;
    DiskEstimate est;
};

// 从配置收集作用域（平铺单块简写 → 1 个；blocks[] → 每块 1 个）。
// input_key/object_field 来自 session_commands.h 的 input_contract（单一声明，不手写第二份）。
inline std::vector<DiskScope> disk_scopes(const nlohmann::json& doc,
                                          const std::string& input_key,
                                          const std::string& object_field,
                                          bool include_masters) {
    std::vector<DiskScope> out;
    if (!doc.is_object()) return out;
    auto add_inputs = [&](const nlohmann::json& host, DiskScope* sc) {
        if (host.contains(input_key)) {
            const auto& v = host[input_key];
            if (v.is_array()) {
                for (const auto& e : v)
                    if (e.is_string()) sc->est.input_bytes += path_bytes_bounded(e.get<std::string>(), &sc->est);
            } else if (v.is_object() && !object_field.empty() && v.contains(object_field) &&
                       v[object_field].is_string()) {
                sc->est.input_bytes += path_bytes_bounded(v[object_field].get<std::string>(), &sc->est);
            }
        }
        if (include_masters) {
            for (const char* k : {"master_bias", "master_dark", "master_flat"})
                if (host.contains(k) && host[k].is_string())
                    sc->est.input_bytes += path_bytes_bounded(host[k].get<std::string>(), &sc->est);
        }
    };
    if (doc.contains("blocks") && doc["blocks"].is_array()) {
        for (const auto& b : doc["blocks"]) {
            if (!b.is_object()) continue;
            DiskScope sc;
            if (b.contains("output_dir") && b["output_dir"].is_string())
                sc.output_dir = b["output_dir"].get<std::string>();
            add_inputs(b, &sc);
            out.push_back(std::move(sc));
        }
        return out;
    }
    DiskScope sc;
    if (doc.contains("output_dir") && doc["output_dir"].is_string())
        sc.output_dir = doc["output_dir"].get<std::string>();
    add_inputs(doc, &sc);
    out.push_back(std::move(sc));
    return out;
}

// ── 写失败归类（运行期 fail-closed 判定的唯一实现）──
//   DiskFull   —— ENOSPC / EDQUOT（磁盘写满）          → exit 10
//   WriteFailed—— EFBIG（文件大小上限等写盘失败）      → exit 10
//   IoFailure  —— 其它（EIO/权限/…）                   → 维持既有 exit 7（I/O 失败）
enum class WriteFailureKind { None, DiskFull, WriteFailed, IoFailure };

inline WriteFailureKind classify_write_failure(int err) {
    switch (err) {
#if defined(ENOSPC)
    case ENOSPC: return WriteFailureKind::DiskFull;
#endif
#if defined(EDQUOT)
    case EDQUOT: return WriteFailureKind::DiskFull;
#endif
#if defined(EFBIG)
    case EFBIG: return WriteFailureKind::WriteFailed;
#endif
    case 0: return WriteFailureKind::None;
    default: return WriteFailureKind::IoFailure;
    }
}

inline const char* write_failure_kind_name(WriteFailureKind k) {
    switch (k) {
    case WriteFailureKind::None:        return "none";
    case WriteFailureKind::DiskFull:    return "disk_full";
    case WriteFailureKind::WriteFailed: return "write_failed";
    default:                            return "io_failure";
    }
}

// 「磁盘写满 / 写盘失败」的唯一退出码（ASTROCS_DESIGN §6.3 exit 10；数值源 = exit_codes.h）。
inline bool write_failure_is_resource_exit(WriteFailureKind k) {
    return k == WriteFailureKind::DiskFull || k == WriteFailureKind::WriteFailed;
}

// ── 运行中写失败探测（真实写一个 4 KiB 探针文件后立即删除）──
// 仅在**已经发生失败**的收尾路径调用（不污染成功 run 的 output_dir 目录树哈希）。
struct WriteProbe {
    WriteFailureKind kind = WriteFailureKind::None;
    int err = 0;
};

inline WriteProbe probe_writable(const std::string& dir) {
    WriteProbe pr;
    if (dir.empty()) { pr.kind = WriteFailureKind::IoFailure; pr.err = ENOENT; return pr; }
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::u8path(dir), ec);   // best effort
    const std::string probe = dir + "/.astrocs_write_probe";
    std::FILE* f = std::fopen(probe.c_str(), "wb");
    if (f == nullptr) { pr.err = errno; pr.kind = classify_write_failure(pr.err); return pr; }
    char buf[4096] = {0};
    const std::size_t n = std::fwrite(buf, 1, sizeof(buf), f);
    if (n != sizeof(buf)) pr.err = errno;
    std::fclose(f);
    std::remove(probe.c_str());
    pr.kind = classify_write_failure(pr.err);
    return pr;
}

// ── 预检面文案（§3.5「详细预估」；warn 不阻断）──
// 返回空串 = 余量充足（调用方按 correct 行呈现预估事实）。
inline std::string disk_precheck_warning(const DiskSpace& sp, const DiskEstimate& est,
                                         const std::string& out_dir) {
    const uint64_t need = est.input_bytes;
    if (!sp.probed) {
        return "磁盘余量无法探测（output_dir = " + out_dir + "，errno=" +
               std::to_string(sp.err) +
               "）⇒ 继续运行（warn 不阻断；写盘失败将 fail-closed 报 error）";
    }
    if (need > sp.free_bytes) {
        return "磁盘余量不足：output_dir = " + out_dir + " 可用 " +
               std::to_string(sp.free_bytes / (1024 * 1024)) + " MiB < 预估需求下限 " +
               std::to_string(need / (1024 * 1024)) + " MiB（= 配置声明输入字节和，" +
               std::to_string(est.files) + " 文件" + (est.truncated ? "，目录扫描已达上限" : "") +
               "）⇒ 继续运行（warn 不阻断；写盘失败将 fail-closed 报 error）";
    }
    return {};
}

// 预估事实行（无论余量是否充足都呈现；§3.5「详细预估」）。
inline std::string disk_estimate_line(const DiskSpace& sp, const DiskEstimate& est,
                                      const std::string& out_dir) {
    const std::string avail = sp.probed
        ? (std::to_string(sp.free_bytes / (1024 * 1024)) + " MiB")
        : std::string("不可探测(errno=") + std::to_string(sp.err) + ")";
    return "磁盘预估：output_dir = " + out_dir + " 可用 " + avail + "；预估需求下限 " +
           std::to_string(est.input_bytes / (1024 * 1024)) + " MiB（输入 " +
           std::to_string(est.files) + " 文件/" + std::to_string(est.dirs) + " 目录" +
           (est.truncated ? "，目录扫描已达上限" : "") + "）";
}

}  // namespace astrocs
