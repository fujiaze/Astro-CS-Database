// A01: child-domain attribution of work and physical peak; a Lease keeps its
// original domain across calls; pinned is a subset of host.
//
// Fixed configuration (VERIFICATION.md A01 row): the process history peak is
// 100 MiB, child A really peaks at 50 MiB and consumes 10 work, child B
// consumes 100 work concurrently.  A must report 50 MiB / 10 work; the parent
// must not bill the child peak twice (history stays 100 MiB); a surviving
// output Lease keeps the bytes of A's domain charged after A's handle is
// dropped; a claim with pinned > host is rejected.
#include <acsd/contracts.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "harness.hpp"

namespace {

namespace rt = acsd::rt;
using acsd::Code;
using acsd::Result;

constexpr acsd::U64 kMiB = 1ull << 20;

Result<rt::Done> consume(const rt::InvocationContext& ctx, acsd::U64 units) {
  return rt::consume_work(ctx, units);
}

}  // namespace

int main() {
  vrt::Recorder recorder("a01", "validation/runtime/a01/a01.cpp");

  auto services = vrt::start_services(768 * kMiB, 1000, 2, 2, 2,
                                      vrt::scratch_root("a01"), 16 * kMiB);
  if (services.code != Code::ok || !services.value) {
    recorder.check("a01.services", false, "ok", vrt::code_of(services));
    return recorder.finish();
  }
  auto command = rt::open_command(*services.value, "validation.a01.command",
                                  vrt::limits(512 * kMiB, 1000, 2 * kMiB, 2, 2, 16 * kMiB));
  recorder.expect_code("a01.open_command_ok", "ok", command);
  if (command.code != Code::ok) return recorder.finish();
  const rt::InvocationContext ctx = *command.value;
  const rt::ScopeSnapshot baseline = rt::snapshot(ctx.scope);

  const acsd::Unit unit = vrt::dimensionless_unit();
  auto index100 = vrt::make_index(100 * kMiB, "validation.a01.index100");
  auto index50 = vrt::make_index(50 * kMiB, "validation.a01.index50");

  // --- child B: physical peak 100 MiB, then released. -----------------------
  auto child_b = rt::child_scope(ctx, "validation.a01.B",
                                 vrt::limits(256 * kMiB, 1000, 4096));
  recorder.expect_code("a01.child_b_scope_ok", "ok", child_b);
  rt::InvocationContext ctx_b = ctx;
  if (child_b.code == Code::ok && child_b.value) {
    ctx_b.scope = *child_b.value;
    auto arena_b = rt::reserve(ctx_b, vrt::bytes(100 * kMiB));
    recorder.expect_code("a01.child_b_reserve_100mib_ok", "ok", arena_b);
    if (arena_b.code == Code::ok && arena_b.value) {
      auto block_b = rt::allocate<std::uint8_t>(ctx_b, *arena_b.value,
                                                acsd::BlockRequest{0, 100 * kMiB},
                                                index100, unit, vrt::zero_hash());
      recorder.expect_code("a01.child_b_allocate_100mib_ok", "ok", block_b);
      if (block_b.code == Code::ok && block_b.value) {
        auto frozen_b = rt::freeze(std::move(*block_b.value));
        const rt::ScopeSnapshot snap_b = rt::snapshot(*child_b.value);
        recorder.value("a01.child_b_peak", static_cast<long long>(snap_b.peak_allocated.host_bytes));
        recorder.check("a01.child_b_peak_100mib",
                       snap_b.peak_allocated.host_bytes == 100 * kMiB, "100 MiB",
                       std::to_string(snap_b.peak_allocated.host_bytes));
        if (frozen_b.value) frozen_b.value.reset();
      }
      (*arena_b.value)->close();
    }
  }

  // --- child A: physical peak 50 MiB, work 10, keeps the Block alive. -------
  auto child_a = rt::child_scope(ctx, "validation.a01.A",
                                 vrt::limits(256 * kMiB, 1000, 4096));
  recorder.expect_code("a01.child_a_scope_ok", "ok", child_a);
  rt::InvocationContext ctx_a = ctx;
  Result<rt::Block<std::uint8_t>> block_a;
  if (child_a.code == Code::ok && child_a.value) {
    ctx_a.scope = *child_a.value;
    auto arena_a = rt::reserve(ctx_a, vrt::bytes(50 * kMiB));
    recorder.expect_code("a01.child_a_reserve_50mib_ok", "ok", arena_a);
    if (arena_a.code == Code::ok && arena_a.value) {
      auto buffer_a = rt::allocate<std::uint8_t>(ctx_a, *arena_a.value,
                                                 acsd::BlockRequest{0, 50 * kMiB}, index50,
                                                 unit, vrt::zero_hash());
      recorder.expect_code("a01.child_a_allocate_50mib_ok", "ok", buffer_a);
      if (buffer_a.code == Code::ok && buffer_a.value)
        block_a = rt::freeze(std::move(*buffer_a.value));
      recorder.expect_code("a01.child_a_freeze_ok", "ok", block_a);
    }
    auto work = consume(ctx_a, 10);
    recorder.expect_code("a01.child_a_consume_10_ok", "ok", work);
  }

  if (child_a.code == Code::ok && child_a.value) {
    const rt::ScopeSnapshot snap_a = rt::snapshot(*child_a.value);
    recorder.value("a01.child_a_peak", static_cast<long long>(snap_a.peak_allocated.host_bytes));
    recorder.value("a01.child_a_work", static_cast<long long>(snap_a.work));
    recorder.check("a01.child_a_peak_50mib", snap_a.peak_allocated.host_bytes == 50 * kMiB,
                   "50 MiB", std::to_string(snap_a.peak_allocated.host_bytes),
                   "the child reports its own peak, not the global 100 MiB history");
    recorder.check("a01.child_a_work_10", snap_a.work == 10, "10",
                   std::to_string(snap_a.work));
  }

  // Parent aggregates the child work but must not double bill the history peak.
  if (child_b.code == Code::ok && child_b.value) {
    auto work_b = consume(ctx_b, 100);
    recorder.expect_code("a01.child_b_consume_100_ok", "ok", work_b);
  }
  const rt::ScopeSnapshot parent_concurrent = rt::snapshot(ctx.scope);
  const acsd::U64 parent_peak_delta =
      parent_concurrent.peak_allocated.host_bytes - baseline.peak_allocated.host_bytes;
  const acsd::U64 parent_work_delta = parent_concurrent.work - baseline.work;
  recorder.value("a01.parent_peak_delta", static_cast<long long>(parent_peak_delta));
  recorder.value("a01.parent_work_delta", static_cast<long long>(parent_work_delta));
  recorder.check("a01.parent_peak_not_double_counted",
                 parent_peak_delta >= 100 * kMiB && parent_peak_delta <= 116 * kMiB,
                 "100 MiB <= parent peak delta <= 116 MiB (not 150 MiB)",
                 std::to_string(parent_peak_delta));
  recorder.check("a01.parent_work_110", parent_work_delta == 110, "110",
                 std::to_string(parent_work_delta),
                 "10 from A + 100 from B, billed once");

  // --- Lease keeps A's domain after A's handle is dropped. -----------------
  if (block_a.code == Code::ok && block_a.value && child_a.value) {
    child_a.value.reset();  // the only explicit ScopeRef handle to A is gone
    const rt::ScopeSnapshot parent_without_handle = rt::snapshot(ctx.scope);
    const acsd::U64 allocated_delta =
        parent_without_handle.allocated.host_bytes - baseline.allocated.host_bytes;
    recorder.value("a01.parent_allocated_without_a_handle",
                   static_cast<long long>(allocated_delta));
    recorder.check("a01.lease_keeps_original_domain", allocated_delta == 50 * kMiB,
                   "50 MiB still allocated in A's domain",
                   std::to_string(allocated_delta),
                   "the surviving output Lease, not the ScopeRef, owns the lifetime");
    block_a.value.reset();
    const rt::ScopeSnapshot parent_after_release = rt::snapshot(ctx.scope);
    recorder.check("a01.lease_release_returns_to_domain",
                   parent_after_release.allocated.host_bytes ==
                       baseline.allocated.host_bytes,
                   "allocated returns to baseline",
                   std::to_string(parent_after_release.allocated.host_bytes -
                                  baseline.allocated.host_bytes));
  }

  // --- pinned is a host subset, and an over-pinned claim is rejected. ------
  auto child_p = rt::child_scope(ctx, "validation.a01.P",
                                 vrt::limits(128 * kMiB, 1000, 4096, 0, 0, 16 * kMiB));
  recorder.expect_code("a01.child_p_scope_ok", "ok", child_p);
  if (child_p.code == Code::ok && child_p.value) {
    rt::InvocationContext ctx_p = ctx;
    ctx_p.scope = *child_p.value;
    rt::ResourceVector mixed;
    mixed.host_bytes = 64 * kMiB;
    mixed.pinned_bytes = 16 * kMiB;
    auto arena_p = rt::reserve(ctx_p, mixed);
    recorder.expect_code("a01.pinned_subset_reserve_ok", "ok", arena_p);
    if (arena_p.code == Code::ok && arena_p.value) {
      rt::ResourceVector valid_claim;
      valid_claim.host_bytes = 32 * kMiB;
      valid_claim.pinned_bytes = 8 * kMiB;
      auto claim_ok = (*arena_p.value)->claim(valid_claim);
      recorder.expect_code("a01.pinned_claim_within_host_ok", "ok", claim_ok);
      rt::ResourceVector invalid_claim;
      invalid_claim.host_bytes = 8 * kMiB;
      invalid_claim.pinned_bytes = 16 * kMiB;  // pinned > host
      auto claim_bad = (*arena_p.value)->claim(invalid_claim);
      recorder.check("a01.pinned_gt_host_rejected", claim_bad.code != Code::ok,
                     "non-ok (pinned bytes are a subset of host bytes)",
                     vrt::code_of(claim_bad));
      (*arena_p.value)->close();
    }
    rt::ResourceVector invalid_reserve;
    invalid_reserve.host_bytes = 1 * kMiB;
    invalid_reserve.pinned_bytes = 2 * kMiB;
    auto reserve_bad = rt::reserve(ctx_p, invalid_reserve);
    recorder.check("a01.pinned_reserve_gt_host_rejected", reserve_bad.code != Code::ok,
                   "non-ok", vrt::code_of(reserve_bad));
  }

  auto closed = rt::wait_external(rt::close_command(ctx));
  recorder.expect_code("a01.close_command_ok", "ok", closed);
  return recorder.finish();
}
