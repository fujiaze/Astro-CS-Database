#ifndef ASTROCS_AIO_DISK_FULL_H
#define ASTROCS_AIO_DISK_FULL_H

// ---------------------------------------------------------------------------
// (ASTROCS_DESIGN.md §10「I/O 与原子产品」+ §7.2 退出码表
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
// 归因粒度 = **帧级**(合同唯一口径, 不是运行级):
//   docs/contracts/LOG_AND_ERROR_CONTRACT.md §5「`IO` | 7（IO）| I/O 失败;
//   **失败节点 manifest** 的 `error_kind==disk_full` 时改判 10」——判定挂在
//   **失败的那一帧 / 那个节点**上, CLI 只做运行级聚合
//   (lib/infrastructure/cli/runtime_client.cpp::pipeline_exit_code_from_error:
//   扫失败节点 manifest 的 error_kind, 任一为 disk_full ⇒ exit 10)。
//   ⇒ 分类状态必须由**发起这次写的那条执行流**独占。旧实现是进程级共享单比特 +
//     product_begin 里的 reset() + exchange 语义的 consume(), 多帧并发时会互相
//     覆盖: 另一帧的 product_begin 抹掉本帧已置位的判定, 或另一帧的收尾抢走本帧
//     的判定 ⇒ 失败帧的 manifest 丢失 error_kind ⇒ 退化成 exit 7(IO) 的 fail-open
//     (FLAKE-01 剂量-反应实证: 单帧 12/12 正确, 双帧 58 次里 5 次 rc=7 且无
//     disk_full 事件; run/FLAKE-01/EVIDENCE.md §A4)。
//
// 实现 = **执行流局部(thread_local)单调计数 + 窗口快照**:
//   · 生产路径上「写产品 → 失败分类 → 归因收尾」是同一线程内的**同步调用栈**:
//     Phase1 每帧跑在一个 p1_parallel_for worker 线程里
//     (lib/infrastructure/scheduler/src/module_adapters.cpp p1_parallel_for),
//     帧内写盘在 write_hips_phase1 内串行、无嵌套线程; Phase2 每节点一个执行
//     线程, 写盘循环同样串行。同规范的既有先例: drizzle 的 per-frame 错误槽与
//     计数即 thread_local (g_hips_error / g_tl_n_quick, 见 module_adapters.cpp
//     帧级并行的线程安全前提注释) ⇒ 帧级状态用 thread_local 是本仓既有口径。
//   · 计数**只增不减、无 reset**: 帧/节点开始写产品**之前**构造 FailureEpoch
//     取快照, 失败收尾问 failed() ⇒ 判定 = 「本执行流在本次窗口内发生过磁盘满
//     分类」。并发帧各自持有自己的窗口 ⇒ 每一帧的判定互不覆盖、互不抢占。
//   · 线程复用安全: 快照是栈上局部量; 线程被复用来跑下一帧时基线随之前移,
//     上一帧的失败计数不会被误算进下一帧。
//
// 传播: 节点适配器在失败收尾时按自己的 FailureEpoch 把 error_kind="disk_full"
// 写进失败节点 manifest; CLI 据此给 exit 10。事后探针保留为兜底(老路径), 不再是
// 唯一判据。
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>

#include <cerrno>

#ifndef _WIN32
#include <sys/statvfs.h>
#endif

namespace aio_disk {

// 执行流局部的失败计数 (header-only inline: 静态库链接进同一可执行体时唯一实例)。
// thread_local ⇒ 并发帧/节点各持一份, 不会互相抹掉或抢占。
inline uint64_t& tl_fail_seq() {
    thread_local uint64_t seq = 0;
    return seq;
}

// 帧/节点级归因窗口。用法:
//     aio_disk::FailureEpoch dsk;          // 开始写产品之前
//     rc = write_product(...);             // 失败分类发生在写路径内
//     if (rc != 0 && dsk.failed()) man["error_kind"] = "disk_full";
class FailureEpoch {
  public:
    FailureEpoch() : base_(tl_fail_seq()) {}
    bool failed() const { return tl_fail_seq() != base_; }

  private:
    uint64_t base_;
};

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

// 失败瞬间记录: 命中则给**本执行流**的归因窗口记一次并返回 true。
inline bool note_failure(const std::string& path, int err) {
    if (space_exhausted(path, err)) {
        ++tl_fail_seq();
        return true;
    }
    return false;
}

// 注入面/已知磁盘满: 直接给本执行流的归因窗口记一次。
inline void note_full() { ++tl_fail_seq(); }

}  // namespace aio_disk

#endif  // ASTROCS_AIO_DISK_FULL_H
