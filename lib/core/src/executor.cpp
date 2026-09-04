// RT-004 唯一共享执行器实现（CPU heavy + 有界 I/O）
//
// 语义（executor.h 冻结合同 + 约束 D.1-D.4）:
//   - CPU heavy executor: worker 数 = ThreadBudget 预算上限（唯一共享池，进程内
//     只此一个；模块不得自建私有池）。每个任务执行前经 ThreadBudget::acquire
//     (1, budget, NONBLOCK) 原子预留 —— lease RAII 析构自动归还；预算耗尽 →
//     worker 阻塞在 cv 等待预算可用（不忙等/不空转，见 defect fix RT-004）。
//     worker 在队列空时阻塞 CV（不空转）；cancel() notify_all 唤醒全部
//     （取消能唤醒等待）。
//   - worker 生命周期：Impl 持有 std::vector<std::thread>，构造创建、析构
//     stop+notify+join 完整回收（detach 会造成对象析构后 UAF —— 已修，
//     RT-004 worker join 语义）。
//   - 任务执行后显式归还 lease（available+1）再 notify executor cv，唤醒
//     等待预算的 worker（消除预算耗尽 busy-loop）。
//   - 有界 I/O executor: 有界并发 + 有界队列；短 I/O 任务带 IoTaskClass 标记；
//     队列满 → enqueue 返回 false（调用方串行降级/重试）。
//   - 无私有池：本文件是全仓唯一 executor 池实现；worker 只在本文件创建，
//     scheduler 不再自建 std::thread 池（RT-004 消灭 per-run 池）。
#include "astrocs/core/executor.h"

#include <algorithm>

namespace astrocs::core {

// ───────────────────────── CPU heavy executor ─────────────────────────

struct CpuHeavyExecutor::Impl {
  explicit Impl(std::shared_ptr<ThreadBudget> b)
      : budget(std::move(b)), worker_count(budget ? budget->budget() : 1u) {}

  std::shared_ptr<ThreadBudget> budget;   // RT-003 唯一预算（lease 来源）
  uint32_t worker_count;

  std::mutex mtx;
  std::condition_variable cv;             // 队列空/预算耗尽 → worker 阻塞（不空转）
  std::queue<ExecutorTask> tasks;         // 待执行任务（尚未被 worker 领取）
  std::vector<std::thread> workers;       // 唯一共享池 worker（join 生命周期）
  std::atomic<bool> cancelled{false};
  std::atomic<uint32_t> running{0};       // 正在执行任务数（已 acquire lease）
  std::atomic<uint32_t> pending{0};       // 队列内排队任务数
  std::atomic<uint32_t> inflight{0};      // 已被 worker 领取但未完成（含等预算）
  bool stop = false;                      // 析构后停止
};

CpuHeavyExecutor::CpuHeavyExecutor(std::shared_ptr<ThreadBudget> budget)
    : impl_(std::make_unique<Impl>(std::move(budget))) {
  worker_count_ = impl_->worker_count;
  const uint32_t n = impl_->worker_count;
  // 唯一共享 worker 池：每 worker 循环取任务；任务执行前 acquire lease。
  impl_->workers.reserve(n);
  for (uint32_t i = 0; i < n; ++i) {
    impl_->workers.emplace_back([this]() { worker_loop(); });
  }
}

void CpuHeavyExecutor::worker_loop() {
  for (;;) {
    ExecutorTask task;
    {
      std::unique_lock<std::mutex> lock(impl_->mtx);
      impl_->cv.wait(lock, [this] {
        return impl_->stop || impl_->cancelled.load() || !impl_->tasks.empty();
      });
      if (impl_->stop) return;
      if (impl_->cancelled.load()) {
        // 取消：清空队列，唤醒等待者退出（取消后尽快返回）
        while (!impl_->tasks.empty()) impl_->tasks.pop();
        impl_->pending.store(0);
        return;
      }
      task = std::move(impl_->tasks.front());
      impl_->tasks.pop();
      --impl_->pending;
      ++impl_->inflight;                  // 领取成功：计入未完成（wait_all 依据）
    }
    // ── lease 注入：任务执行前原子预留（不超卖） ──
    ThreadLease lease;
    for (;;) {
      lease = impl_->budget
                  ? impl_->budget->acquire(1u, impl_->budget->budget(),
                                           AcquirePolicy::NONBLOCK)
                  : ThreadLease();
      if (lease.acquired()) break;
      // 预算耗尽：阻塞等待预算可用/取消/停止（不忙等、不把任务放回队列空转）。
      // 任务完成路径 release 后 notify cv → 此处被唤醒重试 acquire。
      std::unique_lock<std::mutex> lock(impl_->mtx);
      impl_->cv.wait(lock, [this] {
        return impl_->stop || impl_->cancelled.load() ||
               (impl_->budget && impl_->budget->available() >= 1u);
      });
      if (impl_->stop || impl_->cancelled.load()) {
        // 取消/停止：丢弃手上任务（不空转等待）
        --impl_->inflight;
        if (impl_->cancelled.load()) {
          impl_->cv.notify_all();         // 让其他等待 worker 一并退出
        }
        return;
      }
    }
    impl_->running.fetch_add(1);
    RunContext ctx;
    // RT-003: 把唯一预算注入任务上下文（模块/任务经 ctx.acquire_lease 拿 lease）
    ctx.set_budget(impl_->budget);
    ctx.set_thread_budget(impl_->budget ? impl_->budget->budget() : 1u);
    // RT-006: 任务观测起始（真实时间点；trace 汇注入后才写）
    const auto t_start = std::chrono::steady_clock::now();
    std::shared_ptr<TraceStore> obs_store;
    {
      std::lock_guard<std::mutex> lock(obs_mu_);
      obs_store = trace_store_;
    }
    if (obs_store) {
      TraceEvent e;
      e.type = TraceEventType::WORKER_TASK;
      e.status = "STARTED";
      e.workers = static_cast<uint32_t>(lease.size());
      e.granted_workers = impl_->budget ? impl_->budget->budget() : 1u;
      e.provider = observed_provider_;
      obs_store->record(std::move(e));
    }
    try {
      task(ctx);                          // lease RAII：任务结束/异常自动归还
    } catch (...) {
      // 异常不得杀死 worker；lease 经析构回收
    }
    // RT-006: 任务观测结束（真实完成；计数+1；收集任务内 provider 置位观测）
    tasks_executed_.fetch_add(1, std::memory_order_relaxed);
    {
      std::lock_guard<std::mutex> lock(obs_mu_);
      if (!ctx.provider().empty() && ctx.provider() != observed_provider_) {
        observed_provider_ = ctx.provider();
        provider_sets_.fetch_add(1, std::memory_order_relaxed);
      }
      if (obs_store) {
        TraceEvent e;
        e.type = TraceEventType::WORKER_TASK;
        e.status = "COMPLETED";
        e.workers = static_cast<uint32_t>(lease.size());
        e.provider = observed_provider_;
        e.wall_ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t_start)
                        .count();
        obs_store->record(std::move(e));
      }
    }
    impl_->running.fetch_sub(1);
    if (lease.acquired()) lease.release();  // 显式归还 → available+1
    {
      std::lock_guard<std::mutex> lock(impl_->mtx);
      --impl_->inflight;
    }
    impl_->cv.notify_all();               // 唤醒等待预算/等待任务的 worker
  }
}

void CpuHeavyExecutor::enqueue(ExecutorTask task) {
  if (!task) return;
  std::lock_guard<std::mutex> lock(impl_->mtx);
  if (impl_->cancelled.load() || impl_->stop) return;  // 取消后拒绝新任务
  impl_->tasks.push(std::move(task));
  ++impl_->pending;
  impl_->cv.notify_one();
}

void CpuHeavyExecutor::cancel() noexcept {
  impl_->cancelled.store(true, std::memory_order_release);
  impl_->cv.notify_all();                 // 取消唤醒等待 worker（验收点）
}

void CpuHeavyExecutor::wait_all() {
  std::unique_lock<std::mutex> lock(impl_->mtx);
  impl_->cv.wait(lock, [this] {
    return impl_->cancelled.load() ||     // 取消后尽快返回
           (impl_->tasks.empty() && impl_->running.load() == 0 &&
            impl_->pending.load() == 0 && impl_->inflight.load() == 0);
  });
}

CpuHeavyExecutor::~CpuHeavyExecutor() {
  {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->stop = true;
  }
  impl_->cv.notify_all();                 // 唤醒全部 worker 退出（join 回收）
  for (auto& t : impl_->workers) {
    if (t.joinable()) t.join();           // 等 worker 完全退出 → 无 UAF
  }
}

// ───────────────────────── 有界 I/O executor ─────────────────────────

struct IoExecutor::Impl {
  Impl(uint32_t max_conc, uint32_t cap)
      : max_concurrency(std::max<uint32_t>(1, max_conc)),
        queue_capacity(std::max<uint32_t>(1, cap)) {}

  uint32_t max_concurrency;
  uint32_t queue_capacity;

  std::mutex mtx;
  std::condition_variable cv;
  std::queue<std::pair<IoTaskClass, ExecutorTask>> tasks;
  std::vector<std::thread> workers;       // I/O worker（join 生命周期）
  std::atomic<bool> cancelled{false};
  uint32_t active = 0;                    // 正在执行任务数（mtx 保护）
  bool stop = false;
};

IoExecutor::IoExecutor(uint32_t max_concurrency, uint32_t queue_capacity)
    : impl_(std::make_unique<Impl>(max_concurrency, queue_capacity)) {
  max_concurrency_ = impl_->max_concurrency;
  queue_capacity_ = impl_->queue_capacity;
  impl_->workers.reserve(max_concurrency_);
  for (uint32_t i = 0; i < max_concurrency_; ++i) {
    impl_->workers.emplace_back([this]() { io_worker_loop(); });
  }
}

void IoExecutor::io_worker_loop() {
  for (;;) {
    ExecutorTask task;
    {
      std::unique_lock<std::mutex> lock(impl_->mtx);
      impl_->cv.wait(lock, [this] {
        return impl_->stop || impl_->cancelled.load() || !impl_->tasks.empty();
      });
      if (impl_->stop) return;
      if (impl_->cancelled.load()) {
        while (!impl_->tasks.empty()) impl_->tasks.pop();
        return;
      }
      task = std::move(impl_->tasks.front().second);
      impl_->tasks.pop();
      ++impl_->active;
    }
    RunContext ctx;                       // I/O 任务上下文（无科学预算语义）
    ctx.set_thread_budget(1u);
    try {
      task(ctx);
    } catch (...) {
      // I/O 任务异常不杀死 worker
    }
    {
      std::lock_guard<std::mutex> lock(impl_->mtx);
      --impl_->active;
    }
    impl_->cv.notify_all();
  }
}

bool IoExecutor::enqueue(IoTaskClass cls, ExecutorTask task) {
  if (!task) return false;
  std::lock_guard<std::mutex> lock(impl_->mtx);
  if (impl_->cancelled.load() || impl_->stop) return false;
  if (impl_->tasks.size() >= impl_->queue_capacity) return false;  // 队列满 → 拒绝
  impl_->tasks.emplace(cls, std::move(task));
  impl_->cv.notify_one();
  return true;
}

void IoExecutor::cancel() noexcept {
  impl_->cancelled.store(true, std::memory_order_release);
  impl_->cv.notify_all();
}

void IoExecutor::wait_all() {
  std::unique_lock<std::mutex> lock(impl_->mtx);
  impl_->cv.wait(lock, [this] {
    return impl_->cancelled.load() ||     // 取消后尽快返回
           (impl_->tasks.empty() && impl_->active == 0);
  });
}

uint32_t IoExecutor::queued() const noexcept {
  std::lock_guard<std::mutex> lock(impl_->mtx);
  return static_cast<uint32_t>(impl_->tasks.size());
}

IoExecutor::~IoExecutor() {
  {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->stop = true;
  }
  impl_->cv.notify_all();
  for (auto& t : impl_->workers) {
    if (t.joinable()) t.join();           // 等 worker 完全退出 → 无 UAF
  }
}

// ───────────────────────── 工厂 ─────────────────────────

Result<std::unique_ptr<CpuHeavyExecutor>> create_cpu_heavy_executor(
    std::shared_ptr<ThreadBudget> budget) noexcept {
  if (!budget || budget->budget() == 0) {
    return Result<std::unique_ptr<CpuHeavyExecutor>>::fail(
        Error(ErrorDomain::RESOURCE, "create_cpu_heavy_executor: budget required (>0)"));
  }
  return Result<std::unique_ptr<CpuHeavyExecutor>>::ok(
      std::make_unique<CpuHeavyExecutor>(std::move(budget)));
}

Result<std::unique_ptr<IoExecutor>> create_io_executor(
    uint32_t max_concurrency, uint32_t queue_capacity) noexcept {
  if (max_concurrency == 0 || queue_capacity == 0) {
    return Result<std::unique_ptr<IoExecutor>>::fail(
        Error(ErrorDomain::RESOURCE,
              "create_io_executor: max_concurrency and queue_capacity must be > 0"));
  }
  return Result<std::unique_ptr<IoExecutor>>::ok(
      std::make_unique<IoExecutor>(max_concurrency, queue_capacity));
}

}  // namespace astrocs::core
