// MEMGOV-01 单元判据：内存压力治理（ASTROCS_DESIGN.md §8.3:609-615 编排策略）
//
// 本文件只判「机制本身」的四条**负例**（负例 = 机制不按设计工作时必须判红），
// 与生产接线判据（帧轴/调度器集成）分开：
//   ① 预算极小 ⇒ 必须在高水位停止派发、持续高压必须丢弃进度最低的在飞帧，**并落盘留痕**
//      （红条件：丢弃发生了但台账无对应事件 = 静默丢弃）；
//   ② 预算充裕 ⇒ 不得触发（红条件：无压力却出现 stop/evict 事件或堵住派发）；
//   ③ 压力回落 ⇒ 必须恢复派发，且死区（低水位..高水位）不得抖动
//      （红条件：死区内反复 stop/resume，或回落后仍不恢复）；
//   ④ 机制关闭 ⇒ 与未启用**逐字节等价**（恒允许派发、零计数、零台账）。
//   ⑤ 预算/RSS 不可判定 ⇒ fail-open（放行）且**显式留痕**（红条件：静默变哑）。
//   ⑥ 调度器集成：压力高 ⇒ 并发节点退化为依次执行且仍跑完（不挂死）；未注入治理器
//      ⇒ 恢复并发（红绿配对，证明行为差异确实来自本机制）。
#include "astrocs/core/memory_pressure.h"
#include "astrocs/core/memory_budget.h"
#include "astrocs/core/scheduler.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace astrocs::core;

static int failures = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__,   \
                   #cond);                                                   \
      ++failures;                                                            \
    }                                                                        \
  } while (0)
#define CHECK_MSG(cond, msg)                                                 \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK failed %s:%d: %s | %s\n", __FILE__,        \
                   __LINE__, #cond, (msg));                                  \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

namespace {

// ── 测试替身：可控 RSS 探针 + 可控时钟（避免依赖真实内存与墙钟）─────────────
std::atomic<uint64_t> g_fake_rss{0};
double g_fake_now = 0.0;

uint64_t fake_probe() { return g_fake_rss.load(); }
double fake_clock() { return g_fake_now; }

const char* kLedgerPath = "memgov_unit_ledger.jsonl";

PressurePolicy test_policy(bool enabled) {
  PressurePolicy p;
  p.enabled = enabled;
  p.high_percent = 90;
  p.low_percent = 75;
  p.high_dwell_samples = 2;
  p.low_dwell_samples = 2;
  p.evict_after_samples = 3;
  return p;
}

MemoryBudget test_budget(uint64_t limit) {
  MemoryBudget b;
  b.limit_bytes = limit;
  b.source = MemoryBudgetSource::PROBE;
  b.available_bytes = limit * 100 / 95;
  b.percent = 95;
  return b;
}

// 台账里是否出现某事件名（按子串；事件名是 JSON 的 "event":"<name>" 字段）。
bool ledger_has(const std::vector<std::string>& lines, const char* event) {
  const std::string needle = std::string("\"event\":\"") + event + "\"";
  for (const auto& l : lines) {
    if (l.find(needle) != std::string::npos) return true;
  }
  return false;
}

std::string ledger_dump(const std::vector<std::string>& lines) {
  std::string all;
  for (const auto& l : lines) { all += l; all += " | "; }
  return all;
}

// ── ① 预算极小：必须停派发 + 必须丢弃进度最低的帧 + 必须留痕 ─────────────────
void test_tiny_budget_triggers_eviction_and_records_it() {
  std::remove(kLedgerPath);
  g_fake_now = 100.0;
  g_fake_rss.store(950);            // 95% of 1000 ⇒ 高于高水位 90%
  MemoryPressureGovernor gov(test_budget(1000), test_policy(true), &fake_probe,
                             &fake_clock);
  gov.set_sample_interval(0.0);
  gov.set_ledger_path(kLedgerPath);
  CHECK(gov.active());

  // 三个在飞帧：stage 序号不同（1 = 最早阶段 / 3 = 较晚阶段）
  const uint64_t t1 = gov.begin_frame("f1", "cal", 1, 8);
  const uint64_t t2 = gov.begin_frame("f2", "cal", 1, 8);
  const uint64_t t3 = gov.begin_frame("f3", "phot", 5, 8);
  CHECK(t1 < t2 && t2 < t3);

  // 采样 1 次：未达高水位去抖门槛（2）⇒ 仍允许派发
  g_fake_now += 1.0;
  gov.sample_at(g_fake_now);
  CHECK(gov.level() == PressureLevel::NORMAL);
  CHECK(gov.may_dispatch());

  // 采样 2 次：进入高压力态 ⇒ 必须停止派发（§8.3:614）
  g_fake_now += 1.0;
  gov.sample_at(g_fake_now);
  CHECK(gov.level() == PressureLevel::HIGH);
  CHECK(!gov.may_dispatch());
  CHECK(gov.stop_events() == 1);

  // 高压第 2 个 tick（evict_after_samples=3 未到）⇒ 仍不得丢弃
  g_fake_now += 1.0; gov.sample_at(g_fake_now);
  CHECK(gov.eviction_count() == 0);
  // 高压第 3 个 tick = 门槛 ⇒ **自主**丢弃一帧，无需调用方显式请求
  g_fake_now += 1.0; gov.sample_at(g_fake_now);
  CHECK(gov.eviction_count() == 1);
  // 进度最低 = stage_index 最小；同阶段取票号最大（最年轻、耗时最短 ⇒ 销毁的工作最少）
  CHECK_MSG(gov.frame_evicted(t2), "f2 (lowest progress, youngest) must be the victim");
  CHECK(!gov.frame_evicted(t1));
  CHECK(!gov.frame_evicted(t3));

  // 节流：门槛之后每再持续 3 个 tick 才丢弃一帧 ⇒ 紧接着 2 个 tick 不得再丢（不是抖动）
  g_fake_now += 1.0; gov.sample_at(g_fake_now);
  g_fake_now += 1.0; gov.sample_at(g_fake_now);
  CHECK(gov.eviction_count() == 1);
  // 第 6 个高压 tick ⇒ 再丢一帧；防颠簸主序 = 本帧身份已丢弃次数升序 ⇒ 选从未被丢过的 f1
  //（f1 stage=1 低于 f3 stage=5，同为零次 ⇒ 取 f1；若只按进度最低则会再次选中 f2）
  g_fake_now += 1.0; gov.sample_at(g_fake_now);
  CHECK(gov.eviction_count() == 2);
  CHECK_MSG(gov.frame_evicted(t1),
            "thrash protection: a never-evicted frame must be chosen before re-evicting");
  CHECK(!gov.frame_evicted(t3));

  // 被丢弃帧必须在安全点被持帧者放弃（此处模拟帧体的安全点检查）
  gov.mark_frame_committed(t3);
  CHECK(!gov.frame_discardable(t3));   // 越过安全点 ⇒ 退出候选集
  // 已无可丢弃候选 ⇒ 再持续高压也不得丢弃（不得误伤已越过安全点的帧）
  for (int i = 0; i < 4; ++i) { g_fake_now += 1.0; gov.sample_at(g_fake_now); }
  CHECK(gov.eviction_count() == 2);
  // 显式选择面（供需要主动请求的调用方）：此处已无候选 ⇒ 必须返回 false
  uint64_t t4 = 0;
  InFlightFrame v4;
  CHECK(!gov.choose_victim(&t4, &v4));

  // 留痕（红条件：丢弃发生但台账没有对应事件 = 静默丢弃）
  const auto lines = gov.ledger_lines();
  CHECK_MSG(ledger_has(lines, "pressure_high"), ledger_dump(lines).c_str());
  CHECK_MSG(ledger_has(lines, "evict_lowest_progress"), ledger_dump(lines).c_str());
  // 落盘（§8.3:612）：路径已设 ⇒ 状态转移即时落盘
  std::string err;
  CHECK(gov.flush_ledger(&err) || err.empty());
  std::FILE* f = std::fopen(kLedgerPath, "rb");
  CHECK_MSG(f != nullptr, "ledger file must exist after transitions");
  if (f) {
    char buf[4096];
    const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    std::fclose(f);
    CHECK_MSG(std::strstr(buf, "evict_lowest_progress") != nullptr,
              "eviction must be persisted to disk (on-disk trace)");
  }
  gov.end_frame(t1);
  gov.end_frame(t2);
  gov.end_frame(t3);
  CHECK(gov.in_flight_count() == 0);
}

// ── ② 预算充裕：不得触发 ────────────────────────────────────────────────────
void test_sufficient_budget_does_not_trigger() {
  std::remove(kLedgerPath);
  g_fake_now = 200.0;
  g_fake_rss.store(1000);           // 10% of 10000 ⇒ 远低于低水位
  MemoryPressureGovernor gov(test_budget(10000), test_policy(true), &fake_probe,
                             &fake_clock);
  gov.set_sample_interval(0.0);
  gov.set_ledger_path(kLedgerPath);
  CHECK(gov.active());

  for (int i = 0; i < 10; ++i) {
    g_fake_now += 1.0;
    const PressureSample s = gov.sample_at(g_fake_now);
    CHECK(s.level == PressureLevel::NORMAL);
    CHECK(s.pressure_percent == 10);
    CHECK(s.observable);
    CHECK(gov.may_dispatch());
  }
  uint64_t t = 0;
  InFlightFrame v;
  const uint64_t ticket = gov.begin_frame("f1", "cal", 1, 8);
  CHECK(!gov.choose_victim(&t, &v));
  gov.end_frame(ticket);
  CHECK(gov.level() == PressureLevel::NORMAL);
  CHECK(gov.stop_events() == 0);
  CHECK(gov.resume_events() == 0);
  CHECK(gov.eviction_count() == 0);
  CHECK(gov.blocked_dispatches() == 0);
  CHECK(gov.ledger_line_count() == 0);   // 无压力 ⇒ 无事件（不与"静默"混淆：无决定可记）
}

// ── ③ 回落恢复 + 死区不抖动 ────────────────────────────────────────────────
void test_hysteresis_resume_and_dead_band() {
  g_fake_now = 300.0;
  g_fake_rss.store(950);            // 95% ⇒ HIGH
  MemoryPressureGovernor gov(test_budget(1000), test_policy(true), &fake_probe,
                             &fake_clock);
  gov.set_sample_interval(0.0);
  for (int i = 0; i < 2; ++i) { g_fake_now += 1.0; gov.sample_at(g_fake_now); }
  CHECK(gov.level() == PressureLevel::HIGH);
  CHECK(gov.stop_events() == 1);

  // 死区：80%（低水位 75 < 80 < 高水位 90）⇒ 必须保持 HIGH，不得抖动
  g_fake_rss.store(800);
  for (int i = 0; i < 6; ++i) { g_fake_now += 1.0; gov.sample_at(g_fake_now); }
  CHECK_MSG(gov.level() == PressureLevel::HIGH, "dead band must hold state");
  CHECK(gov.resume_events() == 0);

  // 回落到 70%（< 低水位）⇒ 去抖 2 次后恢复派发
  g_fake_rss.store(700);
  g_fake_now += 1.0;
  gov.sample_at(g_fake_now);
  CHECK(gov.level() == PressureLevel::HIGH);   // 第一次不够（low_dwell=2）
  g_fake_now += 1.0;
  gov.sample_at(g_fake_now);
  CHECK(gov.level() == PressureLevel::NORMAL);
  CHECK(gov.resume_events() == 1);
  CHECK(gov.may_dispatch());
  CHECK(ledger_has(gov.ledger_lines(), "pressure_low"));
}

// ── ④ 机制关闭 ⇒ 与未启用逐字节等价 ────────────────────────────────────────
void test_disabled_is_identical_to_before() {
  std::remove(kLedgerPath);
  g_fake_now = 400.0;
  g_fake_rss.store(100000);          // 10000% of 10 ⇒ 即便这样也不得有任何动作
  MemoryPressureGovernor gov(test_budget(10), test_policy(false), &fake_probe,
                             &fake_clock);
  gov.set_sample_interval(0.0);
  gov.set_ledger_path(kLedgerPath);
  CHECK(!gov.active());
  CHECK(!gov.enabled());
  for (int i = 0; i < 20; ++i) {
    g_fake_now += 1.0;
    const PressureSample s = gov.sample_at(g_fake_now);
    CHECK(s.level == PressureLevel::NORMAL);
    CHECK(gov.may_dispatch());
  }
  uint64_t t = 0;
  InFlightFrame v;
  CHECK(!gov.choose_victim(&t, &v));
  CHECK(gov.stop_events() == 0);
  CHECK(gov.resume_events() == 0);
  CHECK(gov.eviction_count() == 0);
  CHECK(gov.ledger_line_count() == 0);
  std::FILE* f = std::fopen(kLedgerPath, "rb");
  CHECK_MSG(f == nullptr, "disabled governance must not write any ledger file");
  if (f) std::fclose(f);
}

// ── ⑤ 不可判定 ⇒ fail-open 且显式留痕 ──────────────────────────────────────
void test_unobservable_fails_open_and_is_logged() {
  // ⑤a：RSS 探针缺失（nullptr）
  {
    g_fake_now = 500.0;
    MemoryPressureGovernor gov(test_budget(1000), test_policy(true), nullptr,
                               &fake_clock);
    gov.set_sample_interval(0.0);
    CHECK(!gov.active());
    CHECK(gov.may_dispatch());
    CHECK(gov.ledger_line_count() >= 1);
    CHECK_MSG(ledger_has(gov.ledger_lines(), "governance_unavailable"),
              ledger_dump(gov.ledger_lines()).c_str());
  }
  // ⑤b：预算不可判定（limit=0）
  {
    g_fake_now = 600.0;
    g_fake_rss.store(12345);
    MemoryPressureGovernor gov(test_budget(0), test_policy(true), &fake_probe,
                               &fake_clock);
    gov.set_sample_interval(0.0);
    CHECK(!gov.active());
    CHECK(gov.may_dispatch());
    CHECK_MSG(ledger_has(gov.ledger_lines(), "governance_unavailable"),
              ledger_dump(gov.ledger_lines()).c_str());
  }
  // ⑤c：探针存在但返回 0（不可判定）⇒ 必须显式登记 rss_unavailable，不得静默
  {
    g_fake_now = 700.0;
    g_fake_rss.store(0);
    MemoryPressureGovernor gov(test_budget(1000), test_policy(true), &fake_probe,
                               &fake_clock);
    gov.set_sample_interval(0.0);
    CHECK(gov.active());
    g_fake_now += 1.0;
    const PressureSample s = gov.sample_at(g_fake_now);
    CHECK(!s.observable);
    CHECK(s.level == PressureLevel::NORMAL);
    CHECK(gov.may_dispatch());
    CHECK_MSG(ledger_has(gov.ledger_lines(), "rss_unavailable"),
              ledger_dump(gov.ledger_lines()).c_str());
  }
}

// ── ⑥ 调度器集成：压力高 ⇒ 不派发新并发；未注入 ⇒ 恢复并发 ─────────────────
struct DispatchProbe {
  std::atomic<int> concurrent{0};
  std::atomic<int> max_concurrent{0};
  std::atomic<int> ran{0};
  void note_enter() {
    const int c = ++concurrent;
    ++ran;
    int cur = max_concurrent.load();
    while (cur < c && !max_concurrent.compare_exchange_weak(cur, c)) {}
  }
  void note_exit() { --concurrent; }
};

void test_scheduler_dispatch_gate(bool governed, int* max_concurrent_out,
                                  uint64_t* stops_out, bool* run_ok_out) {
  g_fake_rss.store(governed ? 950 : 0);   // governed ⇒ 95%（HIGH）
  Scheduler sched(4, 4);
  MemoryPressureGovernor* govp = nullptr;
  std::unique_ptr<MemoryPressureGovernor> gov;
  if (governed) {
    // 用**真实时钟**：本用例要验的是"门挡不挡"，去抖参数必须与它解耦 ——
    // 取 high_dwell_samples=1 使压力一越线即挡（否则去抖窗口内会额外放进一个节点，
    // 那条语义由用例 ③ 单独判）。采样节流置 0 ⇒ 每次派发请求都重新判定。
    PressurePolicy p = test_policy(true);
    p.high_dwell_samples = 1;
    gov = std::make_unique<MemoryPressureGovernor>(test_budget(1000), p,
                                                   &fake_probe);
    gov->set_sample_interval(0.0);
    govp = gov.get();
  }
  sched.set_memory_governor(govp);
  DispatchProbe probe;
  for (int i = 0; i < 4; ++i) {
    sched.add_node({std::string("n") + std::to_string(i), {},
                    [&probe](const std::string&, RunContext&) {
                      probe.note_enter();
                      std::this_thread::sleep_for(std::chrono::milliseconds(120));
                      probe.note_exit();
                      return Result<void>::success();
                    },
                    "cpu_heavy"});
  }
  RunContext ctx;
  ctx.set_thread_budget(4);
  const auto r = sched.run(ctx);
  *run_ok_out = r.ok();
  *max_concurrent_out = probe.max_concurrent.load();
  if (stops_out) *stops_out = govp ? govp->stop_events() : 0;
  CHECK(probe.ran.load() == 4);   // 治理不得丢节点（4 个节点必须全部跑到）
}

void test_scheduler_gate_red_green_pair() {
  int max_governed = 0, max_free = 0;
  uint64_t stops = 0;
  bool ok_governed = false, ok_free = false;
  test_scheduler_dispatch_gate(true, &max_governed, &stops, &ok_governed);
  test_scheduler_dispatch_gate(false, &max_free, nullptr, &ok_free);
  CHECK_MSG(ok_governed, "governed run must still complete (no deadlock)");
  CHECK_MSG(ok_free, "ungoverned run must complete");
  CHECK_MSG(stops >= 1, "high pressure must have stopped dispatch at least once");
  CHECK_MSG(max_governed == 1,
            "under sustained high pressure the scheduler must not dispatch new "
            "concurrent nodes (only the progress escape keeps it moving)");
  CHECK_MSG(max_governed < max_free,
            "governed concurrency must be strictly lower than the ungoverned one");
  CHECK_MSG(max_free >= 2,
            "without governance the same DAG must still run concurrently "
            "(proves the difference comes from this mechanism)");
}

}  // namespace

int main() {
  // 逐例打点（stderr，行缓冲）：ctest 只在失败时展示，便于定位挂死在哪一条。
  auto mark = [](const char* name) {
    std::fprintf(stderr, "[memgov] case %s\n", name);
    std::fflush(stderr);
  };
  mark("1-tiny-budget-evict");
  test_tiny_budget_triggers_eviction_and_records_it();
  mark("2-sufficient-budget");
  test_sufficient_budget_does_not_trigger();
  mark("3-hysteresis");
  test_hysteresis_resume_and_dead_band();
  mark("4-disabled");
  test_disabled_is_identical_to_before();
  mark("5-unobservable");
  test_unobservable_fails_open_and_is_logged();
  mark("6-scheduler-gate");
  test_scheduler_gate_red_green_pair();
  mark("done");
  if (failures == 0) {
    std::printf("memory_pressure_test: all checks passed\n");
    return 0;
  }
  std::fprintf(stderr, "memory_pressure_test: %d check(s) failed\n", failures);
  return 1;
}
