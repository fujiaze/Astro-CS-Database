// ACSD Core — 内存压力治理（压力可观测 + 滞回派发门 + 「可丢弃重跑」处置）
//
// 规范依据（逐条；口径唯一，不复述上级原文）
//   * **ASTROCS_DESIGN.md §8.3 编排策略**（最高设计，:609-615）规定了调度器的五条策略，
//     其中三条由本单元落实：
//       :611「静态预算」   —— 预算值本身（不变量：预算 = 可用内存 × 比例，见下）
//       :613「异步并行」   —— 「预算充裕时异步启动多个独立工作流」：**充裕**需要可观测判定
//       :614「可中断排队」 —— 已由 Scheduler 预约式回压实现（scheduler.cpp 内存回压）
//       :615「可丢弃重跑」 —— 「内存仍不足时，丢弃进度最低的工作流并释放其占用；
//                              该工作流随后重新开始」：**本条此前零生产实现，本单元补上**
//       :612「探针校正」   —— 压力与决策「随事件流落盘」（本单元的台账即该落盘面）
//       :680               —— 并行度按「内存闸门 × Runtime lease」联合确定 ⇒ 内存闸门
//                              必须是一个**运行期可读的量**，而不是一次性快照
//   * **docs/contracts/SCHEDULER_CONTRACT.md:14/:38**：「线程数、内存上限、队列深度一律
//     从配置/资源门读取」「线程预算、内存上限（峰值工作集）、队列深度……」
//   * **ASTROCS_DESIGN.md §3.5 + ENGINEERING_SPEC.md §12 + eng/contracts/resource_gate_v1.json
//     #disk_gate.no_gate**：资源门只管磁盘，内存/CPU/线程不设门 ⇒ 本单元是**调度准入
//     与编排处置**，不产生退出码、不阻断运行、不让 run 失败。
//   * **ASTROCS_DESIGN.md §8.3:616 不变量**：数值结果与并发度无关 ⇒ 丢弃并重跑一帧
//     不得改变任何帧的科学结果（丢弃只改变「何时算」，不改变「算什么」）。
//
// 预算口径（**唯一来源，不另发明**）
//   预算 = astrocs::core::resolve_memory_budget(可用内存, 比例)（lib/include/astrocs/core/
//   memory_budget.h；比例默认 95 的唯一数值源 = eng/packaging/config/runtime_resources.json）。
//   本单元**不重算预算**，只持有与消费该结果（禁止第二份预算实现）。
//   「可用内存」的定义沿用该头所链的 aio 探针口径（MemAvailable（含可回收 page cache，
//   不是 MemFree）∩ cgroup 内存余量）；本单元不重定义它。
//
// 压力口径（本单元新增的观测量；分子分母都必须是指定物理量）
//   压力(%) = floor(100 × 进程树 RSS / 预算)
//     · 分子 = 进程树 RSS（生产由 aio_process_tree_rss_bytes 提供，与外部看门狗
//       mem_guard.py 的进程树口径一致 ⇒ 两条阈值可在同一曲线上比较）；
//     · 分母 = 上述预算（不是物理总量，也不是可用内存）—— 问的是「离我自己声明的
//       上限还有多远」，这与 §8.3:611「静态预算」的语义一致。
//
// 滞回（避免阈值附近抖动；本单元的核心语义）
//   置 HIGH 需连续 high_dwell_samples 次采样压力 ≥ high_percent；
//   回 NORMAL 需连续 low_dwell_samples 次采样压力 ≤ low_percent；
//   处于 [low, high) 死区时**维持当前状态**（这就是滞回带，不抖动）。
//
// 「进度最低」的定义（§8.3:615 的「进度最低的工作流」在本项目的实例）
//   normalize 的工作流实例 = **一帧**（最高设计 :603 逐字「多帧彼此独立」「帧内节点按
//   DAG 流水，多帧并行」）。一帧的进度由两个可观测量共同确定：
//     · 阶段（做到哪个阶段）= 当前 op/DAG 节点名与序号（stage/stage_index/stage_total）
//     · 已完成量          = 自认领起的**在飞时长**（claim 序 + claimed_at）
//   同一 op 内全部在飞帧执行**相同的逐帧工序**，故在飞时长是「已完成工作量」的单调代理，
//   且认领序使比较**确定**（同输入必同选择，可测）。⇒ 进度最低 = 认领序最大（最年轻）、
//   在飞时长最小的那一帧。
//   为什么这样选：① 丢弃它销毁的已完成工作量最少（不浪费已算出的东西）；
//   ② 它尚未到达任何产物提交点，丢弃天然无半截产物；
//   ③ 与「丢弃最大内存占用者」相比，逐帧占用近似相等（同一 op、同一帧几何），
//      而后者的丢弃代价更大 ⇒ 按「最小销毁」选。
//
// 语义边界
//   * 本单元只做**决策与留痕**：派发门由调度器/帧轴查询，丢弃由持帧者执行并回报。
//   * enabled=false ⇒ 恒 NORMAL、恒允许派发、零台账、零计数（与未启用逐字节等价）。
//   * 预算/RSS 不可判定 ⇒ 不启用（fail-open 到「无治理」）并**显式落盘**登记原因，
//     不冒充「有治理」也不静默。
#pragma once

#ifndef ASTROCS_CORE_MEMORY_PRESSURE_H
#define ASTROCS_CORE_MEMORY_PRESSURE_H

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "astrocs/core/memory_budget.h"

namespace astrocs::core {

// 压力分子来源（依赖注入：core 不链接 aio，调用方注入 aio_process_tree_rss_bytes）。
// 返回 0 = 不可判定。必须 reentrant（可被多线程并发调用）。
using RssProbe = std::uint64_t (*)();

// 单调时间源（秒）。默认 = steady_clock；测试注入以驱动滞回而无需真等待。
using ClockFn = double (*)();

double memory_pressure_default_clock() noexcept;

enum class PressureLevel : std::uint8_t {
  NORMAL = 0,
  HIGH = 1,
};

const char* pressure_level_name(PressureLevel l) noexcept;

// 治理动作（台账/观测面；稳定字符串）
enum class PressureAction : std::uint8_t {
  NONE = 0,             // 本 tick 无决策
  STOP_DISPATCH = 1,    // 跨高水位 ⇒ 停止派发新异步并发（在途跑完）
  RESUME_DISPATCH = 2,  // 回落到低水位 ⇒ 恢复派发
  EVICT_LOWEST = 3,     // 高水位持续 ⇒ 丢弃进度最低的在飞帧
  DISABLED = 4,         // 配置关闭 ⇒ 恒 NORMAL（零副作用）
  UNAVAILABLE = 5,      // 预算或 RSS 不可判定 ⇒ 治理不启用（显式登记）
};

const char* pressure_action_name(PressureAction a) noexcept;

// 策略（数值全部来自 eng/packaging/config/runtime_resources.json，经生成头传入配置层；
// 本头不写任何阈值字面量）。
struct PressurePolicy {
  bool enabled = true;
  std::uint32_t high_percent = 0;        // 0 = 未配置 ⇒ 由配置层填默认
  std::uint32_t low_percent = 0;
  std::uint32_t high_dwell_samples = 0;
  std::uint32_t low_dwell_samples = 0;
  // 高压持续达该采样 tick 数后允许丢弃；此后**每再持续同样多 tick 丢弃一帧**
  // （节流：丢弃是"仍不足时"的升级动作，不是每秒一次的抖动）。
  std::uint32_t evict_after_samples = 0;
  // 配置层解析结果（唯一数值源经生成头）；见 pressure_policy_from_config()。
};

// 从生成头（runtime_resources_generated.h）装配默认策略。唯一数值源 = 该 JSON。
PressurePolicy pressure_policy_from_config() noexcept;

struct PressureSample {
  double ts = 0.0;
  std::uint64_t rss_bytes = 0;
  std::uint64_t budget_bytes = 0;
  std::uint32_t pressure_percent = 0;
  PressureLevel level = PressureLevel::NORMAL;
  bool observable = false;   // false = RSS 不可判定（0）
};

// 在飞工作流（normalize 的实例 = 一帧）
struct InFlightFrame {
  std::uint64_t ticket = 0;      // 认领票号（单调；内部比较用）
  std::string frame_id;
  std::string stage;             // 当前 op / DAG 节点名（做到哪个阶段）
  std::uint32_t stage_index = 0; // 该阶段序号（1..stage_total）
  std::uint32_t stage_total = 0;
  double claimed_at = 0.0;
  bool evicted = false;          // 已被选为牺牲帧（持帧者必须在安全点放弃）
  bool committed = false;        // 已越过本 op 的**丢弃安全点** ⇒ 不再作为牺牲帧候选
  bool ended = false;
};

class MemoryPressureGovernor {
 public:
  MemoryPressureGovernor(MemoryBudget budget, PressurePolicy policy,
                         RssProbe probe, ClockFn clock = memory_pressure_default_clock);
  // 析构在 .cpp 内定义（pimpl：Impl 不完整类型不得在调用方 TU 内联析构）。
  ~MemoryPressureGovernor();
  MemoryPressureGovernor(const MemoryPressureGovernor&) = delete;
  MemoryPressureGovernor& operator=(const MemoryPressureGovernor&) = delete;

  // 治理是否生效：enabled && 预算可判定 && RSS 探针存在。
  // 不生效时 may_dispatch() 恒 true、level() 恒 NORMAL、零计数（与未启用等价）。
  bool active() const;
  bool enabled() const;
  MemoryBudgetSource budget_source() const;
  std::uint64_t budget_bytes() const;
  PressurePolicy policy() const;
  void set_policy(const PressurePolicy& p);

  // 采样 + 滞回状态机。now 由时间源提供；同一 tick 内重复调用幂等
  // （ts 相同 ⇒ 返回上次结果，不重复推状态机、不重复落盘）。
  PressureSample sample();
  PressureSample sample_at(double now);

  // 派发门：true = 允许派发新的异步并发；false = 压力高，只让在途跑完。
  // 内部按 min_sample_interval 节流采样（interval 由调用方经 set_sample_interval 设）。
  bool may_dispatch();

  PressureLevel level() const;
  PressureSample last_sample() const;
  std::uint64_t stop_events() const;     // 进入 HIGH 的次数
  std::uint64_t resume_events() const;   // 回到 NORMAL 的次数
  std::uint64_t blocked_dispatches() const;  // 被门挡下的派发请求次数
  std::uint64_t eviction_count() const;

  // ── 在飞帧登记（§8.3:615 的「工作流」实例）──
  std::uint64_t begin_frame(const std::string& frame_id, const std::string& stage,
                            std::uint32_t stage_index, std::uint32_t stage_total);
  void end_frame(std::uint64_t ticket);
  std::vector<InFlightFrame> in_flight() const;
  std::size_t in_flight_count() const;

  // 选择并标记「进度最低」的牺牲帧（§8.3:615）。
  // 返回 true ⇒ *ticket_out 对应的在飞帧已被标记 evicted，持帧者必须在**安全点**放弃它
  // （不落任何产物）、随后重新开始；调用方完成放弃后必须 end_frame(ticket)。
  // 仅在 level==HIGH 且 HIGH 已连续持续 evict_after_samples 次采样时才会返回 true。
  bool choose_victim(std::uint64_t* ticket_out, InFlightFrame* victim_out);

  // 标记该在飞帧已越过丢弃安全点（此后放弃会留下半截产物或污染跨帧共享状态）
  // ⇒ 立即退出牺牲帧候选集（§8.3:615 的丢弃只在「放弃是安全的」时才允许）。
  // 未登记的票号是空操作。
  void mark_frame_committed(std::uint64_t ticket);
  // 该票号是否已被选为牺牲帧（持帧者在安全点据此放弃）。
  bool frame_evicted(std::uint64_t ticket) const;
  // 该票号是否仍在牺牲帧候选集内（未 committed、未 evicted）。
  bool frame_discardable(std::uint64_t ticket) const;
  // 用真实身份改写某在飞帧的 frame_id（认领时只有占位名；帧体在安全点给出真名，
  // 使台账能指名「丢弃了哪一帧」）。未登记的票号是空操作。
  void set_frame_label(std::uint64_t ticket, const std::string& label);

  // 记一条具名台账事件（调用方自定义 event 名与 note）。用于把**调用侧**的决策
  // （如帧轴的「无在飞帧时放行」「逐 op 汇总」）也落盘 —— 不得静默。
  void record_note(const char* event, const std::string& note);

  // ── 台账（§8.3:612「随事件流落盘」）──
  // 路径为空 ⇒ 只进内存缓冲（不静默：status_json/ledger_lines 仍可读）。
  // 注意：机制**关闭**时零事件、零落盘（与改动前逐字节等价）；"不静默"约束的是
  // "有动作必须留痕"，关闭态没有任何动作，其事实由 status_json.enabled=false 表达。
  void set_ledger_path(const std::string& path);
  std::string ledger_path() const;
  // 把缓冲行追加落盘（aio 唯一追加写原语）。返回 false 且 *err 非空 = 写失败。
  bool flush_ledger(std::string* err);
  // 本次 run 的**全部**事件行（落盘不清空它；供观测与判据读取）。
  std::vector<std::string> ledger_lines() const;
  std::size_t ledger_line_count() const;

  // 观测快照（compact JSON，一行）。
  std::string status_json() const;

  // 采样节流间隔（秒）。0 = 每次都采样（测试用）。
  void set_sample_interval(double seconds);

 private:
  struct Impl;
  // 滞回状态机本体（**调用方必须持有 impl_->mu**）。public 面各自加锁后调用它；
  // 抽出的原因：may_dispatch() 需要在持锁状态下推一次采样，若直接调加锁的
  // sample_at() 会自死锁（同一非递归互斥量二次加锁）。
  static PressureSample step_locked(Impl& s, double now);
  std::unique_ptr<Impl> impl_;
};

// ── 线程本地当前治理器（派发点的取用面）──────────────────────────────────────
// 与 trace_current_node()/set_dispatch_budget_hint() 同款模式：调度器在执行一个节点前
// 绑定本节点所属的治理器，模块侧（帧轴派发点）就近取用，不做 JSON 透传、不改模块签名。
MemoryPressureGovernor* current_governor() noexcept;
void set_current_governor(MemoryPressureGovernor* g) noexcept;

// RAII 绑定（异常路径亦复位）。
class GovernorScope {
 public:
  explicit GovernorScope(MemoryPressureGovernor* g) noexcept;
  ~GovernorScope();
  GovernorScope(const GovernorScope&) = delete;
  GovernorScope& operator=(const GovernorScope&) = delete;

 private:
  MemoryPressureGovernor* prev_;
};

}  // namespace astrocs::core

#endif  // ASTROCS_CORE_MEMORY_PRESSURE_H
