// P1-7 bounded ready window and P2 control-lane command ceiling.
//
// 1) With P=1 and one gated blocker holding the only CPU token, a burst of
//    100000 submissions can keep at most 2P ready + 8P deferred tasks: the
//    rest complete with a named resource_exhausted.  This is the SCHEDULER
//    bound, not an unbounded future set.
// 2) With P=2 and a command CPU ceiling of 1, a completion callback (control
//    lane) and a scientific task of the same command never run concurrently:
//    control work is scheduled, but it spends one command token.
//
// Only public runtime headers are used.  Evidence is the JSON line printed by
// this driver (see run_window_ceiling.sh).
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/task.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;
using acsd::U64;

using namespace std::chrono_literals;

Result<int> ok_int(int value) {
  Result<int> result;
  result.code = Code::ok;
  result.value = value;
  return result;
}

rt::TaskSpec unit_spec(U64 item) {
  rt::TaskSpec spec;
  spec.key = rt::Key{0, 0, 0, item};
  spec.running.cpu_tokens = 1;
  spec.semantic = "validation.window.unit";
  return spec;
}

}  // namespace

int main() {
  int failures = 0;
  auto check = [&](bool condition, const char* name) {
    std::printf("%s: %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
  };

  // 1) Deterministic burst: the blocker is released only after every
  //    submission, so the window cannot slide during the burst.
  U64 submitted = 0, accepted = 0, refused = 0, other = 0;
  {
    rt::PoolConfig config;
    config.process.ceiling.host_bytes = 1ull << 26;
    config.process.ceiling.cpu_tokens = 1;
    config.process.max_work = 1000000;
    config.cpu_threads = 1;
    config.read_slots = 1;
    config.write_slots = 1;
    config.scratch_root = "/tmp/acsd-window-ceiling";
    Result<std::shared_ptr<rt::Services>> services = rt::make_services(config);
    check(services.code == Code::ok && services.value, "make_services");
    if (!services.value) return 1;
    rt::ScopeLimits limits;
    limits.ceiling.host_bytes = 1ull << 26;
    limits.ceiling.cpu_tokens = 1;
    limits.max_work = 1000000;
    Result<rt::InvocationContext> command =
        rt::open_command(*services.value, "validation.window", limits, 1);
    check(command.code == Code::ok && command.value.has_value(), "open_command");
    if (!command.value) return 1;
    rt::InvocationContext context = *command.value;

    std::atomic<bool> gate{false};
    rt::TaskSpec blocker_spec = unit_spec(1);
    blocker_spec.work_upper = 1;
    auto blocker = rt::submit_cpu<int>(
        context, blocker_spec, [&gate](rt::InvocationContext task) -> Result<int> {
          (void)rt::consume_work(task, 1);
          while (!gate.load(std::memory_order_acquire))
            std::this_thread::sleep_for(1ms);
          return ok_int(0);
        });
    std::this_thread::sleep_for(20ms);
    std::vector<rt::Task<int>> tasks;
    constexpr U64 kSubmit = 100000;
    tasks.reserve(kSubmit);
    for (U64 i = 0; i < kSubmit; ++i)
      tasks.push_back(rt::submit_cpu<int>(
          context, unit_spec(i + 2),
          [](rt::InvocationContext) -> Result<int> { return ok_int(1); }));
    gate.store(true, std::memory_order_release);
    for (auto& task : tasks) {
      const Result<int> result = rt::wait_external(std::move(task));
      ++submitted;
      if (result.code == Code::ok)
        ++accepted;
      else if (result.code == Code::resource_exhausted)
        ++refused;
      else
        ++other;
    }
    check(rt::wait_external(std::move(blocker)).code == Code::ok,
          "gated blocker completes");
    check(submitted == kSubmit, "every submission reached a terminal state");
    // 2P ready + 8P deferred = 10 for P=1.
    check(accepted <= 10, "at most 2P ready + 8P deferred tasks were accepted");
    check(refused + 1 >= kSubmit - 10, "the burst beyond the window was refused");
    check(other == 0, "refusals carry the named resource_exhausted code");
    check(rt::wait_external(rt::close_command(context)).code == Code::ok,
          "close_command after the burst");
  }

  // 2) Control lane spends a command CPU token: P=2, ceiling=1.
  int max_active = 0;
  {
    rt::PoolConfig config;
    config.process.ceiling.host_bytes = 1ull << 26;
    config.process.ceiling.cpu_tokens = 2;
    config.process.max_work = 1000000;
    config.cpu_threads = 2;
    config.read_slots = 1;
    config.write_slots = 1;
    config.scratch_root = "/tmp/acsd-window-ceiling";
    Result<std::shared_ptr<rt::Services>> services = rt::make_services(config);
    check(services.code == Code::ok && services.value, "control: make_services");
    if (!services.value) return 1;
    rt::ScopeLimits limits;
    limits.ceiling.host_bytes = 1ull << 26;
    limits.ceiling.cpu_tokens = 1;
    limits.max_work = 1000000;
    Result<rt::InvocationContext> command =
        rt::open_command(*services.value, "validation.ceiling", limits, 1);
    check(command.code == Code::ok && command.value.has_value(),
          "control: open_command");
    if (!command.value) return 1;
    rt::InvocationContext context = *command.value;

    std::atomic<int> active{0};
    auto enter = [&] {
      const int now = active.fetch_add(1) + 1;
      int previous = max_active;
      while (now > previous &&
             !__atomic_compare_exchange_n(&max_active, &previous, now, false,
                                          __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        ;
    };
    auto leave = [&] { active.fetch_sub(1); };
    auto first = rt::submit_cpu<int>(
        context, unit_spec(1),
        [](rt::InvocationContext) -> Result<int> { return ok_int(1); });
    auto continuation = rt::then(
        context, std::move(first),
        [context, enter, leave](int value) -> rt::Task<int> {
          enter();
          std::this_thread::sleep_for(120ms);
          leave();
          return rt::ready<int>(context, ok_int(value + 1));
        });
    auto science = rt::submit_cpu<int>(
        context, unit_spec(2),
        [enter, leave](rt::InvocationContext) -> Result<int> {
          enter();
          std::this_thread::sleep_for(300ms);
          leave();
          return ok_int(7);
        });
    const Code continuation_code = rt::wait_external(std::move(continuation)).code;
    const Code science_code = rt::wait_external(std::move(science)).code;
    check(continuation_code == Code::ok, "control continuation completes");
    check(science_code == Code::ok, "scientific task completes");
    check(max_active <= 1,
          "control lane and science never exceed the command CPU ceiling");
    check(rt::wait_external(rt::close_command(context)).code == Code::ok,
          "control: close_command");
  }

  std::printf(
      "{\"case\":\"m2.window_ceiling\",\"submitted\":%llu,\"accepted\":%llu,"
      "\"refused\":%llu,\"other\":%llu,\"max_active\":%d,\"failures\":%d}\n",
      static_cast<unsigned long long>(submitted),
      static_cast<unsigned long long>(accepted),
      static_cast<unsigned long long>(refused),
      static_cast<unsigned long long>(other), max_active, failures);
  return failures == 0 ? 0 : 1;
}
