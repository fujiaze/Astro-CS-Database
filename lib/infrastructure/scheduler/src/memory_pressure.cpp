// ACSD Core — 内存压力治理实现。
// 层次合同、规范依据与全部语义定义见 lib/include/astrocs/core/memory_pressure.h。
#include "astrocs/core/memory_pressure.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <cstdio>
#include <memory>
#include <sstream>

// 阈值的唯一数值源（CMake 从 eng/packaging/config/runtime_resources.json 生成）。
#include "runtime_resources_generated.h"
// aio 唯一追加写原语（台账落盘；本 TU 不自持第二条追加写通道）。
#include "aio_atomic_file.h"

namespace astrocs::core {

double memory_pressure_default_clock() noexcept {
  using clock = std::chrono::steady_clock;
  static const clock::time_point t0 = clock::now();
  return std::chrono::duration<double>(clock::now() - t0).count();
}

const char* pressure_level_name(PressureLevel l) noexcept {
  return (l == PressureLevel::HIGH) ? "high" : "normal";
}

const char* pressure_action_name(PressureAction a) noexcept {
  switch (a) {
    case PressureAction::NONE: return "none";
    case PressureAction::STOP_DISPATCH: return "stop_dispatch";
    case PressureAction::RESUME_DISPATCH: return "resume_dispatch";
    case PressureAction::EVICT_LOWEST: return "evict_lowest";
    case PressureAction::DISABLED: return "disabled";
    case PressureAction::UNAVAILABLE: return "unavailable";
  }
  return "?";
}

PressurePolicy pressure_policy_from_config() noexcept {
  PressurePolicy p;
  p.enabled = astrocs::runtime_resources::kMemoryPressureEnabled != 0;
  p.high_percent = astrocs::runtime_resources::kMemoryPressureHighPercent;
  p.low_percent = astrocs::runtime_resources::kMemoryPressureLowPercent;
  p.high_dwell_samples = astrocs::runtime_resources::kMemoryPressureHighDwellSamples;
  p.low_dwell_samples = astrocs::runtime_resources::kMemoryPressureLowDwellSamples;
  p.evict_after_samples = astrocs::runtime_resources::kMemoryPressureEvictAfterSamples;
  return p;
}

namespace {

// floor(100 × rss / budget)，整数、无浮点、溢出安全。
// 饱和口径（显式，不静默）：rss 远超预算时返回 kPressurePercentSaturated，
// 调用方只与 high_percent 比较 ⇒ 饱和不改变「高压力」判定。
std::uint32_t pressure_percent_of(std::uint64_t rss, std::uint64_t budget) {
  if (budget == 0) return 0;
  const std::uint64_t whole = rss / budget;      // ≥1 ⇒ 已越预算
  const std::uint64_t rem = rss % budget;        // < budget
  std::uint64_t frac = 0;
  if (rem != 0) {
    // rem < budget 且 budget ≤ 物理内存（≪ UINT64_MAX/100）⇒ 不溢出；
    // 防御性回退：极端 budget 下先除后乘（语义仍为 floor 的保守下界）。
    frac = (rem <= (UINT64_MAX / 100ull)) ? (rem * 100ull) / budget
                                          : ((rem / 100ull) * 100ull) / budget;
  }
  const std::uint64_t pct = whole * 100ull + frac;
  const std::uint64_t cap = 1000000ull;          // 饱和上界（10000×）
  return static_cast<std::uint32_t>(std::min(pct, cap));
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
    else if (c == '\n') out += "\\n";
    else out.push_back(c);
  }
  return out;
}

}  // namespace

struct MemoryPressureGovernor::Impl {
  MemoryBudget budget;
  PressurePolicy policy;
  RssProbe probe = nullptr;
  ClockFn clock = memory_pressure_default_clock;

  mutable std::mutex mu;
  PressureLevel level = PressureLevel::NORMAL;
  std::uint32_t high_streak = 0;   // 连续 ≥ high 的采样数
  std::uint32_t low_streak = 0;    // 连续 ≤ low 的采样数
  std::uint32_t high_total = 0;    // HIGH 状态下的累计采样数（进入丢弃态的门槛）
  std::uint64_t high_ticks = 0;    // 高压累计采样 tick（离开 HIGH 清零；丢弃节流用它）
  // 逐帧身份已丢弃次数（键 = frame_id，**跨"重跑后重新认领"保持**）：牺牲帧选取的
  // 主序 ⇒ 每个可丢弃帧各被丢弃一次之后才轮到第二次，不会反复丢弃同一帧造成颠簸。
  std::map<std::string, std::uint32_t> frame_evictions;
  PressureSample last;
  bool last_valid = false;
  bool announced_unavailable = false;


  std::uint64_t stop_events = 0, resume_events = 0, blocked = 0, evictions = 0;

  std::uint64_t next_ticket = 1;
  std::map<std::uint64_t, InFlightFrame> frames;

  std::string ledger_path;
  // ledger = 待落盘缓冲（落盘成功后清空）；history = 本次 run 的**全部**事件
  // （只增，供 status_json / 单元判据读取）。两者分开：落盘不得抹掉可观测性。
  std::vector<std::string> ledger;
  std::vector<std::string> history;
  std::uint64_t ledger_seq = 0;

  double sample_interval = 0.0;
  double last_sample_ts = 0.0;

  bool available() const {
    return policy.enabled && budget.limit_bytes > 0 && probe != nullptr;
  }

  // §8.3:615：丢弃**进度最低**的在飞帧并释放其占用（调用方必须持有 mu）。
  // 排序键（确定、可判、可复现）：
  //   ① 本帧身份已丢弃次数（升序）—— 防颠簸：轮完一圈才轮到第二次；
  //   ② stage_index 升序 —— 进度最低（最早的阶段 = 已完成的工作最少）；
  //   ③ ticket 降序 —— 同阶段取最年轻者（耗时最短 ⇒ 销毁的工作最少）。
  // 候选 = 未越过安全点、未 ended、未被选过。无可丢弃候选 ⇒ 空操作（不制造半截产物）。
  bool evict_one_locked() {
    const InFlightFrame* best = nullptr;
    std::uint32_t best_ev = 0;
    for (const auto& kv : frames) {
      const InFlightFrame& f = kv.second;
      if (f.evicted || f.committed || f.ended) continue;
      auto it = frame_evictions.find(f.frame_id);
      const std::uint32_t ev = (it == frame_evictions.end()) ? 0u : it->second;
      if (best == nullptr) { best = &f; best_ev = ev; continue; }
      if (ev != best_ev) { if (ev < best_ev) { best = &f; best_ev = ev; } continue; }
      if (f.stage_index != best->stage_index) {
        if (f.stage_index < best->stage_index) { best = &f; best_ev = ev; }
        continue;
      }
      if (f.ticket > best->ticket) { best = &f; best_ev = ev; }
    }
    if (best == nullptr) return false;
    InFlightFrame& victim = frames[best->ticket];
    victim.evicted = true;
    evictions++;
    frame_evictions[victim.frame_id] = best_ev + 1u;
    event("evict_lowest_progress", PressureAction::EVICT_LOWEST, last.ts, &victim,
          "sustained high pressure: discard the lowest-progress in-flight frame and "
          "release its occupancy; the frame will be restarted");
    return true;
  }

  // 把缓冲行追加落盘（**调用方必须持有 mu**）。返回 false 且 *err 非空 = 写失败，
  // 失败时缓冲**原样保留**（不丢证据）。路径为空 ⇒ 无落盘目标，缓冲保留，返回 true。
  bool flush_locked(std::string* err) {
    if (err) err->clear();
    if (ledger_path.empty() || ledger.empty()) return true;
    std::string blob;
    for (const auto& l : ledger) { blob += l; blob.push_back('\n'); }
    std::string aerr;
    aio_atomic::AppendSink* s = aio_atomic::append_open(ledger_path, &aerr);
    if (!s) {
      if (err) *err = aerr.empty() ? ("append open failed: " + ledger_path) : aerr;
      return false;
    }
    const int wrc = aio_atomic::append_write_str(s, blob);
    const int crc = aio_atomic::append_close(s);
    if (wrc != 0 || crc != 0) {
      if (err) *err = (wrc != 0) ? "append write failed" : "append close failed";
      return false;
    }
    ledger.clear();
    return true;
  }

  void push_line(const std::string& obj) {
    ledger.push_back(obj);
    history.push_back(obj);
  }

  void event(const char* name, PressureAction action, double ts,
             const InFlightFrame* f, const std::string& note) {
    std::ostringstream o;
    o << "{\"seq\":" << (++ledger_seq)
      << ",\"ts\":" << ts
      << ",\"event\":\"" << name << "\""
      << ",\"action\":\"" << pressure_action_name(action) << "\""
      << ",\"level\":\"" << pressure_level_name(level) << "\""
      << ",\"rss_bytes\":" << last.rss_bytes
      << ",\"budget_bytes\":" << last.budget_bytes
      << ",\"pressure_percent\":" << last.pressure_percent
      << ",\"high_streak\":" << high_streak
      << ",\"in_flight\":" << frames.size();
    if (f) {
      o << ",\"ticket\":" << f->ticket
        << ",\"frame_id\":\"" << json_escape(f->frame_id) << "\""
        << ",\"stage\":\"" << json_escape(f->stage) << "\""
        << ",\"stage_index\":" << f->stage_index
        << ",\"stage_total\":" << f->stage_total
        << ",\"frame_elapsed_s\":" << (ts - f->claimed_at);
    }
    if (!note.empty()) o << ",\"note\":\"" << json_escape(note) << "\"";
    o << "}";
    push_line(o.str());
    // 状态转移/丢弃**即时落盘**（§8.3:612）：进程被外部看门狗杀死时证据仍在。
    // 持有 mu 调用是刻意的（本函数只在锁内被调），落盘失败不回滚、不抛、不阻断调度
    // —— 台账是证据面，不是判据（§3.5 内存不设门）。
    std::string ferr;
    (void)flush_locked(&ferr);
  }
};

MemoryPressureGovernor::MemoryPressureGovernor(MemoryBudget budget,
                                               PressurePolicy policy,
                                               RssProbe probe, ClockFn clock)
    : impl_(std::make_unique<Impl>()) {
  impl_->budget = budget;
  impl_->policy = policy;
  impl_->probe = probe;
  impl_->clock = clock ? clock : memory_pressure_default_clock;
  impl_->last.budget_bytes = budget.limit_bytes;
  impl_->last.ts = impl_->clock();
  // 采样间隔的唯一数值源（生成头）；不在实现侧写字面量。
  impl_->sample_interval =
      static_cast<double>(astrocs::runtime_resources::kMemoryPressureSampleIntervalMs) /
      1000.0;
}

MemoryPressureGovernor::~MemoryPressureGovernor() = default;

bool MemoryPressureGovernor::active() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->available();
}

bool MemoryPressureGovernor::enabled() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->policy.enabled;
}

MemoryBudgetSource MemoryPressureGovernor::budget_source() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->budget.source;
}

std::uint64_t MemoryPressureGovernor::budget_bytes() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->budget.limit_bytes;
}

PressurePolicy MemoryPressureGovernor::policy() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->policy;
}

void MemoryPressureGovernor::set_policy(const PressurePolicy& p) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  impl_->policy = p;

}

void MemoryPressureGovernor::set_sample_interval(double seconds) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  impl_->sample_interval = std::max(0.0, seconds);
}

PressureSample MemoryPressureGovernor::sample() {
  double now = 0.0;
  {
    std::lock_guard<std::mutex> lk(impl_->mu);
    now = impl_->clock();
  }
  return sample_at(now);
}

PressureSample MemoryPressureGovernor::step_locked(Impl& s, double now) {
  // 幂等：同一 tick 重复调用不重复推状态机、不重复落盘。
  if (s.last_valid && now <= s.last.ts) return s.last;
  s.last.ts = now;
  s.last_valid = true;

  // 关闭 ⇒ 恒 NORMAL、**零副作用**：不设门、不采样、不写台账、不落盘
  // （与未启用逐字节等价；"关闭"这一事实经 status_json 的 enabled=false 表达，
  //  故不存在"静默"问题 —— 静默指的是**有动作却不留痕**，此处没有任何动作）。
  if (!s.policy.enabled) {
    s.level = PressureLevel::NORMAL;
    s.last.level = s.level;
    s.last.pressure_percent = 0;
    s.last.observable = false;
    s.last.rss_bytes = 0;
    return s.last;
  }
  // 预算或探针不可用 ⇒ 治理不启用（fail-open 到无治理），显式登记一次。
  if (!s.available()) {
    s.level = PressureLevel::NORMAL;
    s.last.level = s.level;
    s.last.pressure_percent = 0;
    s.last.observable = false;
    if (!s.announced_unavailable) {
      s.announced_unavailable = true;
      s.last.rss_bytes = 0;
      s.event("governance_unavailable", PressureAction::UNAVAILABLE, now, nullptr,
              s.budget.limit_bytes == 0
                  ? "budget undecidable (available memory probe returned 0 or "
                    "percent invalid): governance not engaged, dispatch not gated"
                  : "rss probe missing: governance not engaged, dispatch not gated");
    }
    return s.last;
  }

  const std::uint64_t rss = s.probe();
  s.last.rss_bytes = rss;
  s.last.budget_bytes = s.budget.limit_bytes;
  if (rss == 0) {
    // RSS 不可判定：保持当前状态、**不阻塞派发**（fail-open，避免治理本身把 run 卡死），
    // 如实落盘一次（不静默）。
    s.last.observable = false;
    s.last.level = s.level;
    s.event("rss_unavailable", PressureAction::NONE, now, nullptr,
            "process tree rss probe returned 0: state held, dispatch not gated");
    return s.last;
  }
  s.last.observable = true;
  s.last.pressure_percent = pressure_percent_of(rss, s.budget.limit_bytes);
  const std::uint32_t pct = s.last.pressure_percent;

  const PressureLevel before = s.level;
  PressureAction action = PressureAction::NONE;
  if (pct >= s.policy.high_percent) {
    s.high_streak++;
    s.low_streak = 0;
    if (s.level == PressureLevel::HIGH) {
      s.high_total++;
    } else if (s.high_streak >= std::max<std::uint32_t>(1, s.policy.high_dwell_samples)) {
      s.level = PressureLevel::HIGH;
      s.high_total = 1;
      action = PressureAction::STOP_DISPATCH;
    }
  } else if (pct <= s.policy.low_percent) {
    s.low_streak++;
    s.high_streak = 0;
    if (s.level == PressureLevel::HIGH &&
        s.low_streak >= std::max<std::uint32_t>(1, s.policy.low_dwell_samples)) {
      s.level = PressureLevel::NORMAL;
      s.high_total = 0;
      s.high_ticks = 0;
      action = PressureAction::RESUME_DISPATCH;
    } else if (s.level == PressureLevel::HIGH) {
      // 死区内的回落不足以解除（滞回带）：维持 HIGH，不抖动。
      s.high_total++;
    }
  } else {
    // 死区 [low, high)：维持当前状态（滞回本体）。
    s.high_streak = 0;
    s.low_streak = 0;
    if (s.level == PressureLevel::HIGH) s.high_total++;
  }
  s.last.level = s.level;

  if (action == PressureAction::STOP_DISPATCH) {
    s.stop_events++;
    s.event("pressure_high", action, now, nullptr,
            "pressure crossed high water mark: stop dispatching new async work units");
  } else if (action == PressureAction::RESUME_DISPATCH) {
    s.resume_events++;
    s.event("pressure_low", action, now, nullptr,
            "pressure fell to low water mark: resume dispatching");
  } else if (before != s.level) {
    s.last.level = s.level;
  }

  // ── §8.3:615「可丢弃重跑」的自主触发（不依赖调用方显式调 choose_victim）──────
  // 语义：先停派发（:614）；高压**持续**到门槛后，每再持续同样多采样 tick 丢弃一帧
  // 进度最低的可丢弃帧（节流 ⇒ 是"仍不足时"的升级动作，不是每秒一次的抖动）。
  // 无候选（全部已越过安全点）⇒ 什么都不做：此时"放弃不安全"，宁可让 run 慢，
  // 也不制造半截产物、不污染下一帧的科学结果。
  if (s.level == PressureLevel::HIGH) {
    s.high_ticks++;
    const std::uint64_t th = std::max<std::uint32_t>(1, s.policy.evict_after_samples);
    if (s.high_ticks >= th && ((s.high_ticks - th) % th) == 0) {
      if (!s.evict_one_locked()) {
        // 无可丢弃候选（在飞帧全部已越过安全点）⇒ 显式登记"本轮不丢"的理由。
        // 这是"不得静默"的一部分：**没丢**也是一个决策，必须能解释。
        s.event("evict_no_candidate", PressureAction::EVICT_LOWEST, now, nullptr,
                "eviction round due but no discardable in-flight frame "
                "(all have passed their safe point): stop-dispatch alone holds the "
                "pressure; nothing is discarded rather than risking a half-product");
      }
    }
  } else {
    s.high_ticks = 0;
  }
  return s.last;
}

PressureSample MemoryPressureGovernor::sample_at(double now) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return step_locked(*impl_, now);
}

bool MemoryPressureGovernor::may_dispatch() {
  std::lock_guard<std::mutex> lk(impl_->mu);
  Impl& s = *impl_;
  // 关闭 ⇒ 恒 true、零副作用（不采样、不落盘，与改动前逐字节等价）。
  if (!s.policy.enabled) return true;
  // 预算/探针不可判定 ⇒ fail-open 放行，但**必须显式登记一次**（不静默）：
  // 经 step_locked 走一次状态机，它会落 governance_unavailable 事件（幂等）。
  if (!s.available()) {
    (void)step_locked(s, s.clock());
    return true;
  }
  const double now = s.clock();
  if (!(s.sample_interval > 0.0) || !s.last_valid ||
      (now - s.last.ts) >= s.sample_interval) {
    const PressureSample sm = step_locked(s, now);
    if (sm.level == PressureLevel::HIGH) {
      s.blocked++;
      return false;
    }
    return true;
  }
  // 节流窗口内沿用上次判定（不重复采样）。
  if (s.level == PressureLevel::HIGH) {
    s.blocked++;
    return false;
  }
  return true;
}

void MemoryPressureGovernor::observe() {
  std::lock_guard<std::mutex> lk(impl_->mu);
  Impl& s = *impl_;
  if (!s.policy.enabled) return;              // 关闭 ⇒ 零副作用
  const double now = s.clock();
  if (!(s.sample_interval > 0.0) || !s.last_valid ||
      (now - s.last.ts) >= s.sample_interval) {
    (void)step_locked(s, now);
  }
}

PressureLevel MemoryPressureGovernor::level() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->level;
}

PressureSample MemoryPressureGovernor::last_sample() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->last;
}

std::uint64_t MemoryPressureGovernor::stop_events() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->stop_events;
}
std::uint64_t MemoryPressureGovernor::resume_events() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->resume_events;
}
std::uint64_t MemoryPressureGovernor::blocked_dispatches() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->blocked;
}
std::uint64_t MemoryPressureGovernor::eviction_count() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->evictions;
}

std::uint64_t MemoryPressureGovernor::begin_frame(const std::string& frame_id,
                                                  const std::string& stage,
                                                  std::uint32_t stage_index,
                                                  std::uint32_t stage_total) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  Impl& s = *impl_;
  const std::uint64_t ticket = s.next_ticket++;
  InFlightFrame f;
  f.ticket = ticket;
  f.frame_id = frame_id;
  f.stage = stage;
  f.stage_index = stage_index;
  f.stage_total = stage_total;
  f.claimed_at = s.last.ts;
  s.frames[ticket] = f;
  return ticket;
}

void MemoryPressureGovernor::end_frame(std::uint64_t ticket) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  auto it = impl_->frames.find(ticket);
  if (it != impl_->frames.end()) impl_->frames.erase(it);
}

std::vector<InFlightFrame> MemoryPressureGovernor::in_flight() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  std::vector<InFlightFrame> out;
  out.reserve(impl_->frames.size());
  for (const auto& kv : impl_->frames) out.push_back(kv.second);
  return out;
}

std::size_t MemoryPressureGovernor::in_flight_count() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->frames.size();
}

bool MemoryPressureGovernor::choose_victim(std::uint64_t* ticket_out,
                                           InFlightFrame* victim_out) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  Impl& s = *impl_;
  if (ticket_out) *ticket_out = 0;
  if (!s.policy.enabled || !s.available()) return false;
  if (s.level != PressureLevel::HIGH) return false;
  const std::uint32_t need = std::max<std::uint32_t>(1, s.policy.evict_after_samples);
  if (s.high_total < need) return false;         // HIGH 尚未持续足够久
  if (s.frames.empty()) return false;

  // 「进度最低」= 阶段序最小，其次在飞时长最小（= 认领序最大，最年轻）。
  // 比较确定（票号唯一）⇒ 同输入必同选择。
  const InFlightFrame* best = nullptr;
  for (const auto& kv : s.frames) {
    const InFlightFrame& f = kv.second;
    if (f.evicted) continue;                     // 已选过的不重复选
    if (f.committed) continue;                   // 已越过安全点 ⇒ 放弃不安全，不选
    if (!best) { best = &f; continue; }
    if (f.stage_index < best->stage_index) { best = &f; continue; }
    if (f.stage_index == best->stage_index && f.ticket > best->ticket) best = &f;
  }
  if (!best) return false;

  InFlightFrame victim = *best;
  victim.evicted = true;
  s.frames[best->ticket].evicted = true;
  s.evictions++;
  s.event("evict_lowest_progress", PressureAction::EVICT_LOWEST, s.last.ts, &victim,
          "discard the least-advanced in-flight frame (ASTROCS_DESIGN §8.3 "
          "\"drop the lowest-progress workflow and release its occupancy\"); "
          "it will be restarted after pressure subsides");
  if (ticket_out) *ticket_out = victim.ticket;
  if (victim_out) *victim_out = victim;
  return true;
}

void MemoryPressureGovernor::mark_frame_committed(std::uint64_t ticket) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  auto it = impl_->frames.find(ticket);
  if (it != impl_->frames.end()) it->second.committed = true;
}

bool MemoryPressureGovernor::frame_evicted(std::uint64_t ticket) const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  auto it = impl_->frames.find(ticket);
  return it != impl_->frames.end() && it->second.evicted;
}

bool MemoryPressureGovernor::frame_discardable(std::uint64_t ticket) const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  auto it = impl_->frames.find(ticket);
  return it != impl_->frames.end() && !it->second.committed && !it->second.evicted;
}

void MemoryPressureGovernor::set_frame_label(std::uint64_t ticket,
                                             const std::string& label) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  auto it = impl_->frames.find(ticket);
  if (it != impl_->frames.end() && !label.empty()) it->second.frame_id = label;
}

void MemoryPressureGovernor::record_note(const char* event,
                                         const std::string& note) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  Impl& s = *impl_;
  // 先推一次采样（按节流间隔）：使台账行携带**记这条事件那一刻**的 RSS/压力，
  // 而不是上一次派发判定的旧值（帧轴汇总行因此反映"本 op 跑完时"的压力）。
  (void)step_locked(s, s.clock());
  impl_->event(event ? event : "note", PressureAction::NONE, s.last.ts, nullptr,
               note);
}

void MemoryPressureGovernor::set_ledger_path(const std::string& path) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  impl_->ledger_path = path;
}

std::string MemoryPressureGovernor::ledger_path() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->ledger_path;
}

bool MemoryPressureGovernor::flush_ledger(std::string* err) {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->flush_locked(err);
}

std::vector<std::string> MemoryPressureGovernor::ledger_lines() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->history;   // 全部事件（落盘不抹掉可观测性）
}

std::size_t MemoryPressureGovernor::ledger_line_count() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->history.size();
}

std::string MemoryPressureGovernor::status_json() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  const Impl& s = *impl_;
  std::ostringstream o;
  o << "{\"enabled\":" << (s.policy.enabled ? "true" : "false")
    << ",\"active\":" << (s.available() ? "true" : "false")
    << ",\"level\":\"" << pressure_level_name(s.level) << "\""
    << ",\"budget_bytes\":" << s.budget.limit_bytes
    << ",\"budget_source\":\"" << memory_budget_source_name(s.budget.source) << "\""
    << ",\"budget_available_bytes\":" << s.budget.available_bytes
    << ",\"budget_percent\":" << s.budget.percent
    << ",\"rss_bytes\":" << s.last.rss_bytes
    << ",\"pressure_percent\":" << s.last.pressure_percent
    << ",\"observable\":" << (s.last.observable ? "true" : "false")
    << ",\"high_percent\":" << s.policy.high_percent
    << ",\"low_percent\":" << s.policy.low_percent
    << ",\"high_dwell_samples\":" << s.policy.high_dwell_samples
    << ",\"low_dwell_samples\":" << s.policy.low_dwell_samples
    << ",\"evict_after_samples\":" << s.policy.evict_after_samples
    << ",\"stop_events\":" << s.stop_events
    << ",\"resume_events\":" << s.resume_events
    << ",\"blocked_dispatches\":" << s.blocked
    << ",\"evictions\":" << s.evictions
    << ",\"in_flight\":" << s.frames.size()
    << ",\"ledger_path\":\"" << json_escape(s.ledger_path) << "\""
    << ",\"ledger_lines\":" << s.history.size()
    << ",\"ledger_pending\":" << s.ledger.size() << "}";
  return o.str();
}

// ── 线程本地当前治理器 ──────────────────────────────────────────────────────
namespace {
thread_local MemoryPressureGovernor* g_current_governor = nullptr;
}  // namespace

MemoryPressureGovernor* current_governor() noexcept { return g_current_governor; }

void set_current_governor(MemoryPressureGovernor* g) noexcept {
  g_current_governor = g;
}

GovernorScope::GovernorScope(MemoryPressureGovernor* g) noexcept
    : prev_(g_current_governor) {
  g_current_governor = g;
}

GovernorScope::~GovernorScope() { g_current_governor = prev_; }

}  // namespace astrocs::core
