// ACSD Core — ARCH-502 normalize 异步工作流调度器
//
// 依据：ASTROCS_DESIGN.md §8.1（三命令独立进程、独立调度器）、§8.3（normalize =
//       异步工作流编排）、§9（探针驱动优化）；ENGINEERING_SPEC.md §4.1（管线纪律）；
//       CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md（统一 entrypoint、DAG 声明、
//       资源声明、探针事件 schema、取消与原子性）。
//
// 边界（本任务不做的事）：不做科学计算；不改三阶段装配（ARCH-505）；不做性能调优
//   ——窗口/并发度的**最终取值**由 PERF-501 基于本调度器产出的探针数据确定，本任务
//   只保证机制正确与探针齐全。
//
// 三条硬约束（合同条款，测试逐条锁）：
//   ① 线程/内存/队列深度**只从配置读**，调度器是阶段内线程池的**唯一**所有者，
//      模块不得私建线程池（合同 §1）；
//   ② 确定性：固定 worker 数下归约顺序冻结；1/N worker **逐位一致**——异步只影响
//      顺序不影响结果，归约一律按帧索引升序（合同 §2）；
//   ③ 取消：cancel() 置位后唤醒全部等待，在途块全部销毁、无泄漏（合同 §5）。
#pragma once

#include "astrocs/core/block_frame.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace astrocs::core {

// ── 探针事件（CONTRACT-501 SCHEDULER_CONTRACT §4，字段与 schema 一一对应）──────
enum class ProbeKind : std::uint8_t {
  NODE_WALL = 0,    // 节点墙钟
  QUEUE_WAIT = 1,   // 排队等待
  BLOCK_BIRTH = 2,  // 块创建
  BLOCK_DEATH = 3,  // 块销毁
  RSS = 4,          // 常驻内存
  IO = 5,           // I/O 字节
  WORKER_BUSY = 6,  // worker 忙
  CACHE_HIT = 7,    // 缓存命中/未命中
};

const char* probe_kind_name(ProbeKind k);

// 预取键 → 帧内块名（唯一口径：调用方据此在 DAG 中声明消费，保证帧内无残留块）
std::string prefetch_block_name(const std::string& key);

struct ProbeEvent {
  double ts = 0.0;                 // 单调时钟秒
  std::string stage = "normalize";
  ProbeKind kind = ProbeKind::NODE_WALL;
  std::string node;                // 可空
  std::string block;               // 可空
  std::int64_t bytes = 0;
  double value = 0.0;
  std::string unit = "1";          // s / B / 1
  std::uint64_t frame_id = 0;
  std::uint64_t window_id = 0;
  int worker = -1;
  int prefetch_phase = 0;   // 0=非预取事件；1=空闲期预取；2=帧执行前兜底预取
};

// 探针落点：JSONL（每事件一行）。线程安全；path 为空则只留在内存。
class ProbeSink {
 public:
  explicit ProbeSink(std::string path = std::string());
  ~ProbeSink();
  ProbeSink(const ProbeSink&) = delete;
  ProbeSink& operator=(const ProbeSink&) = delete;

  void emit(const ProbeEvent& ev);
  const std::vector<ProbeEvent>& events() const { return events_; }
  std::size_t count(ProbeKind k) const;
  // 按预取阶段计数：1=空闲期预取，2=帧执行前兜底预取，0=其它事件
  std::size_t count_prefetch_phase(int phase) const;
  double sum_value(ProbeKind k) const;
  bool flush();

 private:
  std::string path_;
  std::vector<ProbeEvent> events_;
  mutable std::mutex mu_;
};

// ── 资源声明（合同 §3：全部来自配置，禁止硬编码线程数）──────────────────────
struct NormalizeWorkflowConfig {
  int workers = 1;                  // 帧并发度（由配置/资源门注入）
  std::uint64_t memory_limit_bytes = 0;   // 0 = 不限制
  int queue_depth = 0;              // 0 = 不限
  std::string probe_path;           // JSONL 落点；空 = 不落盘
  // 空闲期预取开关（PERF-501 调优面）。关闭时帧仍必须同步加载自己的预取键——
  // 这正是"无预取 vs 预取"墙钟对比的语义：关 = I/O 落在关键路径上。
  int prefetch_threads = 1;         // 专用预取线程数（0 = 只做帧内兜底加载）
  bool prefetch_enabled = true;
};

// ── 两级预取缓存（合同 §2：星表同组查询合并 + 两级缓存复用）──────────────────
// L1 = 进程内已解析结果；L2 = 由 loader 提供的持久面（可为空 ⇒ 只有 L1）。
// 同 key 并发请求合并为一次 loader 调用（single-flight）。
class PrefetchCache {
 public:
  using Loader = std::function<std::string(const std::string& key)>;

  explicit PrefetchCache(Loader l2 = nullptr);

  // 命中返回 true 并回填 value；未命中返回 false（不触发加载）。
  bool try_get(const std::string& key, std::string* value);
  // 取或加载：L1 → L2 → loader（single-flight，同一 key 只加载一次）。
  std::string get_or_load(const std::string& key);
  void put(const std::string& key, std::string value);
  // 注入/替换 L2 加载器（不可整体赋值：本类含 atomic，拷贝赋值被删除）
  void set_loader(Loader l2);

  std::uint64_t l1_hits() const { return l1_hits_; }
  std::uint64_t l2_hits() const { return l2_hits_; }
  std::uint64_t misses() const { return misses_; }
  std::uint64_t loads() const { return loads_; }
  std::size_t size() const;

 private:
  Loader l2_;
  std::map<std::string, std::string> l1_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::map<std::string, bool> in_flight_;
  std::atomic<std::uint64_t> l1_hits_{0}, l2_hits_{0}, misses_{0}, loads_{0};
};

// ── 帧任务：一个帧的 DAG（节点按声明顺序执行；每节点拿到自己的 BlockFrame）────
struct NormalizeNode {
  std::string id;
  // 节点体：读入参块、产出新块、消费旧块。返回值 = 是否成功。
  std::function<bool(BlockFrame&)> run;
  // 该节点消费的块名（执行后由调度器统一 consume，保证生命周期由调度器掌管）
  std::vector<std::string> consumes;
};

struct NormalizeFrame {
  std::uint64_t frame_id = 0;
  std::vector<NormalizeNode> nodes;
  // 预取键（空闲 worker 会提前 get_or_load 这些键，结果进帧内块 "prefetch:<key>"）
  std::vector<std::string> prefetch_keys;
};

// ── 结果：按帧索引升序冻结（归约顺序确定性的载体）──────────────────────────
struct FrameOutcome {
  std::uint64_t frame_id = 0;
  bool ok = false;
  std::string error;
  std::uint64_t checksum = 0;      // 帧内所有块内容的确定性校验和
  std::size_t peak_bytes = 0;      // 该帧在途块峰值字节
  // 帧内 DAG 跑完后仍存活的字节数（= 调度器强制回收量）。良构帧必须为 0；非 0 表示
  // 存在无人消费的块（如声明了预取键却没有任何节点声明消费它）。
  std::size_t bytes_leaked = 0;
  double wall_seconds = 0.0;
};

class NormalizeWorkflowScheduler {
 public:
  explicit NormalizeWorkflowScheduler(NormalizeWorkflowConfig cfg);
  ~NormalizeWorkflowScheduler();
  NormalizeWorkflowScheduler(const NormalizeWorkflowScheduler&) = delete;
  NormalizeWorkflowScheduler& operator=(const NormalizeWorkflowScheduler&) = delete;

  // 登记一帧（必须在 run() 之前）
  void add_frame(NormalizeFrame frame);
  // 登记预取键 → 内容来源（星表/PSF 初值等）。同 key 只加载一次。
  void set_prefetch_loader(PrefetchCache::Loader loader);

  // 执行全部帧。返回按 frame_id 升序的 outcome（与 worker 数无关）。
  std::vector<FrameOutcome> run();

  // 取消：置位后唤醒全部等待；在途帧走销毁路径。
  void cancel();
  bool cancelled() const { return cancel_.load(); }

  ProbeSink& probes() { return probes_; }
  PrefetchCache& cache() { return cache_; }
  // 峰值在途字节（全体帧的最大值）
  std::size_t peak_inflight_bytes() const { return peak_inflight_.load(); }

 private:
  void worker_loop(int worker_index);
  void prefetch_loop(int thread_index);   // 专用预取线程：按帧序提前加载预取键
  void emit_prefetch_probe(std::size_t idx, int phase, int worker);
  FrameOutcome run_frame(const NormalizeFrame& f, int worker_index);

  NormalizeWorkflowConfig cfg_;
  ProbeSink probes_;
  PrefetchCache cache_;

  std::vector<NormalizeFrame> frames_;
  std::vector<FrameOutcome> outcomes_;

  // 线程池形态（RT-004 架构约束，不得回退）：本调度器**不持有**线程池成员。
  // 每次 run() 在实现文件内建立 run 作用域的**有界**池（帧 worker 数 = cfg_.workers、
  // 预取线程数 = cfg_.prefetch_threads，均由配置注入），run 返回前全部 join 回收；
  // 无 detach、无常驻线程。原 started_ 标志（仅 run 内置位、无人读取）随池一并移除。
  std::mutex mu_;
  std::condition_variable cv_;
  std::condition_variable cv_done_;
  std::deque<std::size_t> pending_;
  std::size_t completed_ = 0;
  std::atomic<bool> cancel_{false};
  std::atomic<std::size_t> peak_inflight_{0};
  std::atomic<std::uint64_t> busy_workers_{0};
  std::atomic<std::size_t> inflight_bytes_{0};
  // 空闲期预取游标：worker 取不到帧时，按此游标推进"为尚未开始的帧预取"（真重叠）
  std::atomic<std::size_t> prefetch_cursor_{0};
  std::atomic<std::size_t> prefetch_done_{0};
  // 每帧"探针归属"原子位：预取线程与 worker 竞争，谁先谁发**唯一**一条 cache_hit，
  // 保证每帧恰好一条探针（与并发时序无关，可复现）。
  std::vector<std::unique_ptr<std::atomic<int>>> probe_claimed_;
};

}  // namespace astrocs::core
