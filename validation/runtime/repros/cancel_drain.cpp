// Does wait_external wake for a task that was cancelled while queued?
#include <acsd/contracts.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>
namespace rt = acsd::rt;
int main() {
  rt::PoolConfig cfg;
  cfg.process.ceiling.host_bytes = 1u << 24;
  cfg.process.max_work = 1000;
  cfg.cpu_threads = 1; cfg.read_slots = 1; cfg.write_slots = 1;
  cfg.scratch_root = "/tmp/acsd-cancel-repro";
  auto services = rt::make_services(cfg);
  auto ctx = rt::open_command(*services.value, "cancel-repro", cfg.process, 1);
  auto command = *ctx.value;
  std::vector<rt::Task<rt::Done>> tasks;
  for (int i = 0; i < 20; ++i) {
    rt::TaskSpec spec; spec.key = rt::Key{0,0,0,(unsigned long long)i};
    spec.running.cpu_tokens = 1; spec.work_upper = 1; spec.semantic = "repro.unit";
    tasks.push_back(rt::submit_cpu<rt::Done>(command, spec,
      [](rt::InvocationContext t) -> acsd::Result<rt::Done> {
        (void)rt::consume_work(t, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        acsd::Result<rt::Done> r; r.code = acsd::Code::ok; r.value = rt::Done{}; return r;
      }));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  rt::request_cancel(command);
  std::printf("cancel requested\n"); std::fflush(stdout);
  for (size_t i = 0; i < tasks.size(); ++i) {
    std::printf("waiting %zu state=%d\n", i, static_cast<int>(tasks[i].state())); std::fflush(stdout);
    auto r = rt::wait_external(std::move(tasks[i]));
    std::printf("  -> %d\n", static_cast<int>(r.code)); std::fflush(stdout);
  }
  return 0;
}
