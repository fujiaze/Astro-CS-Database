#ifndef HP_DRIZZLE_INTERNAL_H
#define HP_DRIZZLE_INTERNAL_H

/* hp_drizzle_internal.h - drizzle 生产源各 TU 之间的私有共享声明。
 *
 * 不进公共导出面 (无 C ABI 符号)。背景 (F-13 / P1-003 生产路由清理):
 * 原 hp_drizzle_run_hips 与 hp_drizzle_run 同处 hp_drizzle_api.cpp 单一 TU;
 * CLI 生产闭包只引用 hp_drizzle_run, 但静态库按 .o 取成员, 会把同一 .o 内
 * 未被引用的 run_hips 一并拉入 acsd exe。将 hp_drizzle_run_hips 原样迁至
 * hp_drizzle_hips_api.cpp, 两 TU 经本头共享 run_drizzle_internal 与错误写入
 * 辅助; 签名/语义/数值零改动。
 *
 * 符号可见性: run_drizzle_internal 在 acsd_p1_drizzle.so 内经链接
 * version-script (local: *) 降 local, 不进模块导出面。 */

#include "hp_drizzle_api.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// P1-DRZ-ASYNC-01: Phase1 写盘异步化的进程级写池（与公共头 HpDrizzleJob 配对）。
// 宽度 = 调用方经 drz_write_pool_ensure() 传入（0 = 关闭，走同步路径）。
// 线程 detach + 池对象 new 后不析构 ⇒ 进程退出不 terminate。
struct HpDrizzleJob {
    std::future<int> fut;      // 写盘任务的 rc（-13 = HiPS 直写失败）
    int has_variance = 0;
    int disk_full = 0;         // 写线程内 FailureEpoch 判定的跨线程回传
    double write_s = 0.0;      // 写线程内实测写盘时长
    double wait_s = 0.0;       // 帧线程 wait 阻塞时长
    std::string err;           // 写线程内的错误串（g_hips_error 是 thread_local）
    std::string dir;
};

class DrzWritePool {
  public:
    static DrzWritePool& inst() {
        static DrzWritePool* p = new DrzWritePool();   // 故意不析构（detach 线程）
        return *p;
    }
    // 确保写池宽度（幂等；只在宽度为 0 时建池；不得缩池/重建）。
    // 返回实际宽度。调用方保证 width ≤ 64 且内存门控已通过。
    int ensure(int width) {
        std::call_once(once_, [this, width] {
            width_ = width;
            for (int i = 0; i < width; ++i)
                std::thread([this] { worker(); }).detach();
        });
        return width_;
    }
    int width() const { return width_; }
    int active() const { return active_.load(std::memory_order_relaxed); }
    int queued() {
        std::lock_guard<std::mutex> lk(m_);
        return static_cast<int>(q_.size());
    }
    std::future<int> submit(std::function<int()> fn) {
        auto task = std::make_shared<std::packaged_task<int()>>(std::move(fn));
        std::future<int> f = task->get_future();
        {
            std::lock_guard<std::mutex> lk(m_);
            q_.push([task] { (*task)(); });
        }
        cv_.notify_one();
        return f;
    }

  private:
    void worker() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return !q_.empty(); });
                job = std::move(q_.front());
                q_.pop();
            }
            active_.fetch_add(1, std::memory_order_relaxed);
            job();
            active_.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    std::once_flag once_;
    int width_ = 0;
    std::queue<std::function<void()>> q_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::atomic<int> active_{0};
};

/* 将 std::string 错误信息拷贝到 result->error_msg (截断到 511 字节)。
 * 原为 hp_drizzle_api.cpp 内 static 辅助, 迁入本头为 static inline
 * (逐 TU 内部链接, 行为逐字节不变)。 */
static inline void setErrorMsg(HpDrizzleResult* result, const std::string& msg) {
    if (!result) return;
    size_t n = msg.copy(result->error_msg, sizeof(result->error_msg) - 1);
    result->error_msg[n] = '\0';
}

/* 共享 Drizzle 执行体 (定义在 hp_drizzle_api.cpp; 供 hp_drizzle_run 与
 * hp_drizzle_run_hips 两个 C 导出薄壳调用)。 */
/* hips_profile: HiPS 直写档位 (仅 write_hips=true 时生效):
 *   0 = 通用直写 (write_hips_direct; signal+support+snr/variance, provenance)
 *   1 = Phase1 生产末端 (write_hips_phase1; 与旧 writer 节点产物逐字节等价)
 * hips_filter_passband: profile=1 的 obs_filter 透传 (可 NULL/空串). */
int run_drizzle_internal(PipelineFrame* frame,
                         int nside, int nested, double pixfrac,
                         const char* output_path,
                         const char* hips_dir,
                         bool write_hips,
                         bool write_legacy_hiss,
                         HpDrizzleResult* result,
                         int precision_mode,
                         int hips_profile,
                         const char* hips_filter_passband,
                         HpDrizzleJob** out_job = nullptr);

#endif /* HP_DRIZZLE_INTERNAL_H */
