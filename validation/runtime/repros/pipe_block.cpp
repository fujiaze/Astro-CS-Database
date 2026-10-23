#include <acsd/contracts.hpp>
#include <cstdio>
#include <memory>
namespace rt = acsd::rt;
using Block = rt::Block<std::uint8_t>;
int main() {
  rt::PoolConfig cfg; cfg.process.ceiling.host_bytes = 1u<<24; cfg.process.max_work=100;
  cfg.cpu_threads=1; cfg.read_slots=1; cfg.write_slots=1; cfg.scratch_root="/tmp/acsd-pipe-repro";
  auto services = rt::make_services(cfg);
  auto opened = rt::open_command(*services.value, "repro", cfg.process, 1);
  auto ctx = *opened.value;
  rt::InvocationPlan plan; plan.input_hash=plan.request_hash=plan.algorithm_hash="sha256:"+std::string(64,'0');
  plan.regions.resize(1); plan.regions[0].id=0; plan.regions[0].kind=rt::RegionKind::stream;
  plan.regions[0].cohorts=1; plan.regions[0].progress_floor.host_bytes=64;
  rt::EdgeSpec edge; edge.id=10; edge.from=0; edge.to=0; edge.max_block_bytes=8;
  edge.window.max_items=1; edge.window.max_queued_bytes=8; edge.window.order=rt::PipeOrder::ordered;
  plan.regions[0].edges.push_back(edge); rt::EdgeSpec e2=edge; e2.id=11; plan.regions[0].edges.push_back(e2);
  auto plan_ref = std::make_shared<rt::InvocationPlan>(plan);
  auto pa = rt::make_pipe<Block>(ctx, plan_ref, 0, 10);
  auto pb = rt::make_pipe<Block>(ctx, plan_ref, 0, 11);
  auto arena = rt::reserve(ctx, acsd::rt::ResourceVector{64,0,0,0,0,0});
  auto idx = std::make_shared<acsd::Index>(); idx->size=8;
  acsd::Unit unit; unit.scale="dimensionless"; unit.multiplier=1;
  auto buf = rt::allocate<std::uint8_t>(ctx, *arena.value, acsd::BlockRequest{0,8}, idx, unit, "sha256:"+std::string(64,'0'));
  std::printf("alloc=%d\n", (int)buf.code); std::fflush(stdout);
  auto blk = rt::freeze(std::move(*buf.value));
  auto ra = (*pa.value)->receive(ctx); auto rb = (*pb.value)->receive(ctx);
  std::printf("receives posted\n"); std::fflush(stdout);
  auto bc = rt::broadcast<Block>(ctx, rt::Key{0,0,0,0}, std::move(*blk.value), {*pa.value, *pb.value});
  std::printf("broadcast posted state=%d\n", (int)bc.state()); std::fflush(stdout);
  auto r1 = rt::wait_external(std::move(ra)); std::printf("r1=%d\n", (int)r1.code); std::fflush(stdout);
  auto r2 = rt::wait_external(std::move(rb)); std::printf("r2=%d\n", (int)r2.code); std::fflush(stdout);
  auto rbc = rt::wait_external(std::move(bc)); std::printf("bc=%d\n", (int)rbc.code); std::fflush(stdout);
  return 0;
}
