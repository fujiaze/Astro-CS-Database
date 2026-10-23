// Does then() complete when the continuation returns an already-ready Task?
#include <acsd/contracts.hpp>
#include <cstdio>
#include <memory>
namespace rt = acsd::rt;
int main() {
  rt::PoolConfig cfg;
  cfg.process.ceiling.host_bytes = 1u << 24;
  cfg.process.max_work = 100;
  cfg.cpu_threads = 1;
  cfg.read_slots = 1;
  cfg.write_slots = 1;
  cfg.scratch_root = "/tmp/acsd-then-repro";
  auto services = rt::make_services(cfg);
  if (services.code != acsd::Code::ok) { std::printf("services failed\n"); return 2; }
  auto ctx = rt::open_command(*services.value, "repro", cfg.process, 1);
  if (ctx.code != acsd::Code::ok) { std::printf("command failed\n"); return 2; }
  acsd::Result<int> one; one.code = acsd::Code::ok; one.value = 1;
  auto first = rt::ready<int>(*ctx.value, std::move(one));
  auto chained = rt::then(*ctx.value, std::move(first), [c = *ctx.value](int v) -> rt::Task<int> {
    acsd::Result<int> out; out.code = acsd::Code::ok; out.value = v + 1;
    return rt::ready<int>(c, std::move(out));
  });
  std::printf("waiting...\n"); std::fflush(stdout);
  auto result = rt::wait_external(std::move(chained));
  std::printf("result code=%s value=%d\n", acsd::rt::Done{} == acsd::rt::Done{} ? "" : "", 0);
  std::printf("done code=%d\n", static_cast<int>(result.code));
  return result.code == acsd::Code::ok ? 0 : 1;
}
