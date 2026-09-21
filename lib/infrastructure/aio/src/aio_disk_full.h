#ifndef ASTROCS_AIO_DISK_FULL_H
#define ASTROCS_AIO_DISK_FULL_H

// ---------------------------------------------------------------------------
// FIX-401 (ASTROCS_DESIGN.md §10「I/O 与原子产品」+ §7.2 退出码表
//          「10 = 磁盘写满 / 写盘失败」): 磁盘满/配额失败的**失败瞬间**分类。
//
// 为什么必须在失败瞬间判定, 而不是事后探针:
//   §10 要求「失败/取消路径清理临时产物」。清理会把磁盘满释放掉, 任何**事后**
//   探针 (lib/infrastructure/cli/disk_gate.h::probe_writable: 写 4 KiB 探针文件
//   看 errno) 随后必然成功 ⇒ fail-open, 磁盘满被误报成 7(IO)。实测: 失败瞬间
//   free=0KB、清理后 free=884KB(= 被删的部分 tile 字节数) ⇒ rc=7;
//   同一场景若不做清理则 rc=10。故分类必须在失败发生处、清理之前取。
//
// 判据 (确定性, 双条件):
//   1) errno ∈ {ENOSPC, EDQUOT} ⇒ 磁盘满 / 配额耗尽;
//   2) 同路径 statvfs 的 f_bavail == 0 ⇒ 已无非特权可用块。
// 反例保证 (不得把所有 I/O 失败都算磁盘满): 文件系统仍有空间时的其它 I/O 失败
// (EACCES / EXDEV / EIO / 注入的合成写失败) 两条都不成立 ⇒ 不置位 ⇒ 仍走 exit 7。
//
// 传播: 节点适配器在失败收尾时 consume() 并把 error_kind="disk_full" 写进失败
// 节点 manifest; CLI 的 pipeline_exit_code_from_error 据此给 exit 10。事后探针
// 保留为兜底 (老路径), 不再是唯一判据。
// ---------------------------------------------------------------------------

#include <atomic>
#include <cerrno>
#include <string>

#ifndef _WIN32
#include <sys/statvfs.h>
#endif

namespace aio_disk {

// 进程内粘滞标志 (header-only inline static: 静态库链接进同一可执行体时唯一实例)。
inline std::atomic<bool>& full_flag() {
    static std::atomic<bool> f{false};
    return f;
}

// 产品/节点开始写入前复位: 标志只归因**当前**这次失败。
inline void reset() { full_flag().store(false, std::memory_order_relaxed); }
inline void note_full() { full_flag().store(true, std::memory_order_relaxed); }
inline bool is_full() { return full_flag().load(std::memory_order_relaxed); }
// 读取并清除 (节点失败收尾路径: 只消费一次, 不污染后续节点)。
inline bool consume() { return full_flag().exchange(false, std::memory_order_relaxed); }

inline bool errno_is_disk_full(int err) {
    if (err == ENOSPC) return true;
#ifdef EDQUOT
    if (err == EDQUOT) return true;
#endif
    return false;
}

// path: 失败发生处的路径 (statvfs 用; 可为文件或目录)。err: 失败处 errno (0=未知)。
inline bool space_exhausted(const std::string& path, int err) {
    if (errno_is_disk_full(err)) return true;
#ifndef _WIN32
    struct statvfs vfs;
    if (!path.empty() && ::statvfs(path.c_str(), &vfs) == 0 && vfs.f_bavail == 0)
        return true;
#endif
    return false;
}

// 失败瞬间记录: 命中则置粘滞标志并返回 true。
inline bool note_failure(const std::string& path, int err) {
    if (space_exhausted(path, err)) {
        note_full();
        return true;
    }
    return false;
}

}  // namespace aio_disk

#endif  // ASTROCS_AIO_DISK_FULL_H
