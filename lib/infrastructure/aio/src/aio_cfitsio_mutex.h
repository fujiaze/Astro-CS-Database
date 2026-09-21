// aio_cfitsio_mutex.h — cfitsio 访问的线程模型 + 锁等待**实测**计数。
//
// ── 线程模型（本仓库 vendored cfitsio 4.6.4，CMakeLists.txt:407 显式
//    _REENTRANT 构建；fits_is_reentrant() 必须返回 1）─────────────────────
//   * 全局 FptrTable（fits_already_open 用）的插入/删除/查找由 cfitsio 自带的
//     Fitsio_Lock 保护：cfileio.c:174 / :740 / :803 / :1469 / :1488 / :4124 /
//     :4289 / :4404（FFLOCK = FFLOCK1(Fitsio_Lock)，fitsio2.h:27）。
//   * 错误消息栈 ffxmsg 同样由 Fitsio_Lock 保护（fitscore.c:766）。
//   * READONLY 打开时 fits_already_open 立即返回（cfileio.c:1544:
//     "if (mode == 0) return(*status);"）⇒ cfitsio 不会把同一个 FITSfile*
//     交给两个线程，每次 open 都得到独立句柄。
//   ⇒ **每个线程用自己的 fitsfile*、只用 READONLY** 时，并发访问是安全的；
//     进程级串行化互斥量对读路径不是必要条件（RT-008 遗留的粗粒度串行化）。
//     唯一残留的跨线程写是 Fitsio_Pthread_Status（pthread_mutex_* 返回码，
//     纯诊断，不参与数据流；TSan 抑制见 eng/tools/quality/tsan/cfitsio.supp）。
//
// ── 锁等待实测计数 ─────────────────────────────────────────────────────────
//   cfitsio_io_mutex() 仍保留给确实需要跨编译单元串行化的调用点（aio_fits
//   通用读、p3_output 写）。取锁点改用 CfitsioLockGuard，把**真实**的阻塞
//   等待纳秒累加到进程级原子计数，供 resource_timeseries.csv 的 lock_wait_ns
//   列与性能证据使用——实测，不是估计。
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace aio {

// 进程级 cfitsio 串行化锁（单一定义，跨 aio_fits / aio_hips_reader / p3_output 共用）。
inline std::mutex& cfitsio_io_mutex() {
    static std::mutex m;
    return m;
}

// ── 锁等待计数（进程级；inline 函数的局部 static 在全程序唯一）──────────────
inline std::atomic<std::uint64_t>& cfitsio_lock_acquisitions() {
    static std::atomic<std::uint64_t> v{0};
    return v;
}
inline std::atomic<std::uint64_t>& cfitsio_lock_contended() {
    static std::atomic<std::uint64_t> v{0};
    return v;
}
inline std::atomic<std::uint64_t>& cfitsio_lock_wait_ns() {
    static std::atomic<std::uint64_t> v{0};
    return v;
}
inline std::atomic<std::uint64_t>& cfitsio_lock_hold_ns() {
    static std::atomic<std::uint64_t> v{0};
    return v;
}

struct CfitsioLockStats {
    std::uint64_t acquisitions = 0;  // 取锁次数
    std::uint64_t contended = 0;     // 需要阻塞等待的次数
    std::uint64_t wait_ns = 0;       // 阻塞等待累计纳秒（实测）
    // 持锁累计纳秒 = 被强制串行化的工作量（wall 占比的分母是运行墙钟）。
    // 单看 wait_ns 会低估串行化代价：临界区本身在最后持有者身上是"有用时间"，
    // 但它把其它线程挡在门外；hold_ns 才是"锁造成的串行段总长"。
    std::uint64_t hold_ns = 0;
};

inline CfitsioLockStats cfitsio_lock_stats() {
    CfitsioLockStats s;
    s.acquisitions = cfitsio_lock_acquisitions().load(std::memory_order_relaxed);
    s.contended = cfitsio_lock_contended().load(std::memory_order_relaxed);
    s.wait_ns = cfitsio_lock_wait_ns().load(std::memory_order_relaxed);
    s.hold_ns = cfitsio_lock_hold_ns().load(std::memory_order_relaxed);
    return s;
}

inline void cfitsio_lock_stats_reset() {
    cfitsio_lock_acquisitions().store(0, std::memory_order_relaxed);
    cfitsio_lock_contended().store(0, std::memory_order_relaxed);
    cfitsio_lock_wait_ns().store(0, std::memory_order_relaxed);
    cfitsio_lock_hold_ns().store(0, std::memory_order_relaxed);
}

// RAII：取锁并累计真实阻塞等待（try_lock 成功即无竞争，不读第二次时钟）。
class CfitsioLockGuard {
public:
    CfitsioLockGuard() {
        std::mutex& m = cfitsio_io_mutex();
        if (!m.try_lock()) {
            const auto t0 = std::chrono::steady_clock::now();
            m.lock();
            const auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now() - t0)
                                .count();
            cfitsio_lock_wait_ns().fetch_add(
                static_cast<std::uint64_t>(dt < 0 ? 0 : dt), std::memory_order_relaxed);
            cfitsio_lock_contended().fetch_add(1, std::memory_order_relaxed);
        }
        cfitsio_lock_acquisitions().fetch_add(1, std::memory_order_relaxed);
        t_hold_ = std::chrono::steady_clock::now();
    }
    ~CfitsioLockGuard() {
        const auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - t_hold_)
                            .count();
        cfitsio_lock_hold_ns().fetch_add(
            static_cast<std::uint64_t>(dt < 0 ? 0 : dt), std::memory_order_relaxed);
        cfitsio_io_mutex().unlock();
    }
    CfitsioLockGuard(const CfitsioLockGuard&) = delete;
    CfitsioLockGuard& operator=(const CfitsioLockGuard&) = delete;

private:
    std::chrono::steady_clock::time_point t_hold_{};
};

}  // namespace aio
