// C-CAL-WRITE / C-CAL-CLEAN fault injection for four private spill writers.
//
// Three independent faults, each in its own Services instance and scratch root:
//   1. short write          : RLIMIT_FSIZE makes the append I/O fail after the
//                             write was submitted; no sealed array may be
//                             published and every writer must be drained;
//   2. seal failure         : the private spool disappears before seal, so the
//                             canonical finalize fails; the failure is reported
//                             and all four writers are aborted;
//   3. cancel after submit  : the append I/O is in flight when the command is
//                             cancelled and abort is requested; abort must be
//                             deferred until the real write completes, then
//                             clean the private spool and return ok.
//
// Only the public runtime service/Storage API is used.  The recorded criteria
// are: a failed short write/seal never delivers an ArrayRef; all four private
// spools are removed after abort; a cancelled command still drains its writers.
// Cleanup-only secondary issues cannot be injected through this public path
// because storage treats private-spool removal as best effort; that is reported
// as a limitation instead of a pass.
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/storage.hpp>

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <sys/resource.h>
#include <vector>

namespace {

namespace rt = acsd::rt;
using acsd::ArrayRef;
using acsd::BlockRequest;
using acsd::Code;
using acsd::DType;
using acsd::Index;
using acsd::Ref;
using acsd::Result;
using acsd::U64;
using acsd::Unit;

constexpr U64 kElements = 4096;  // < canonical 1 MiB block: one append segment

Unit adu_unit() {
  Unit unit;
  unit.scale = "ADU";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 1;
  return unit;
}

Unit dq_unit() {
  Unit unit;
  unit.scale = "1";
  unit.sr_power = 0;
  unit.pixel_power = 0;
  unit.nm_power = 0;
  unit.multiplier = 1.0;
  unit.linear_power = 0;
  return unit;
}

Ref<Index> make_index(U64 size) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = Index::Kind::native;
  index->namespace_id = "detector0";
  index->identity.object_id = "acsd.Index:detector0";
  index->identity.content_hash = "sha256:" + std::string(64, 'a');
  index->identity.schema_id = "acsd.Index";
  index->identity.schema_major = 2;
  index->identity.schema_minor = 0;
  index->assessment.complete = true;
  return index;
}

template <class T>
rt::Block<T> make_block(U64 begin, U64 count, T fill, Ref<Index> index, const Unit& unit) {
  auto owner = std::make_shared<std::vector<T>>(static_cast<std::size_t>(count), fill);
  rt::Block<T> block;
  block.range = BlockRequest{begin, count};
  block.index = std::move(index);
  block.unit = unit;
  block.model_hash = "sha256:" + std::string(64, 'c');
  block.values = std::span<const T>(owner->data(), owner->size());
  block.lease.owner = std::static_pointer_cast<const void>(owner);
  return block;
}

std::size_t count_spools(const std::string& root) {
  std::error_code ec;
  std::size_t count = 0;
  for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
    if (entry.is_directory(ec)) ++count;
  }
  return count;
}

struct CaseEnv {
  std::shared_ptr<rt::Services> services;
  rt::InvocationContext context;
  rt::ArenaRef arena;
};

bool setup_case(CaseEnv& out, const std::string& scratch, const std::string& command_id) {
  rt::PoolConfig config;
  config.process.ceiling.host_bytes = 64ull << 20;
  config.process.ceiling.cpu_tokens = 1;
  config.process.max_work = 1000000;
  config.cpu_threads = 2;
  config.read_slots = 1;
  config.write_slots = 1;
  config.scratch_root = scratch;
  std::error_code ec;
  std::filesystem::remove_all(scratch, ec);
  Result<std::shared_ptr<rt::Services>> services = rt::make_services(config);
  if (services.code != Code::ok || !services.value) return false;
  out.services = *services.value;
  Result<rt::InvocationContext> command =
      rt::open_command(out.services, command_id, config.process, 1);
  if (command.code != Code::ok || !command.value) return false;
  out.context = *command.value;
  Result<rt::ArenaRef> arena = rt::reserve(out.context, rt::ResourceVector{64ull << 10});
  if (arena.code != Code::ok || !arena.value) return false;
  out.arena = *arena.value;
  out.context.persistent = out.arena;
  return true;
}

void teardown_case(CaseEnv& env) {
  if (env.services && env.context.services)
    (void)rt::wait_external(rt::close_command(env.context));
  env.context = rt::InvocationContext{};
  if (env.arena) env.arena->close();
  env.arena.reset();
  env.services.reset();
}

std::vector<rt::WriterRef> begin_writers(CaseEnv& env) {
  std::vector<rt::WriterRef> writers;
  const Ref<Index> index = make_index(kElements);
  const Unit adu = adu_unit();
  const Unit dq = dq_unit();
  auto add = [&](DType dtype, const Unit& unit, std::size_t bytes) {
    rt::BuildSpec spec;
    spec.layout.dtype = dtype;
    spec.layout.shape = {kElements};
    spec.layout.unit = unit;
    spec.layout.index = index;
    spec.placement = rt::Placement::spill;
    spec.buffer_bytes = kElements * bytes;
    spec.metadata_bytes = 512;
    Result<rt::WriterRef> writer = rt::wait_external(rt::begin_async(env.context, env.arena, spec));
    if (writer.code == Code::ok && writer.value) writers.push_back(*writer.value);
  };
  add(DType::f64, adu, sizeof(double));
  add(DType::f64, adu, sizeof(double));
  add(DType::f64, adu, sizeof(double));
  add(DType::u16, dq, sizeof(std::uint16_t));
  return writers;
}

rt::Task<rt::Done> append_frame(CaseEnv& env, const std::vector<rt::WriterRef>& writers,
                                U64 begin) {
  const Ref<Index> index = make_index(kElements);
  std::vector<rt::Task<U64>> appends;
  appends.push_back(rt::append_async(env.context, writers[0],
                                     make_block<double>(begin, kElements, 4.0, index, adu_unit())));
  appends.push_back(rt::append_async(env.context, writers[1],
                                     make_block<double>(begin, kElements, 3.5, index, adu_unit())));
  appends.push_back(rt::append_async(env.context, writers[2],
                                     make_block<double>(begin, kElements, 4.5, index, adu_unit())));
  appends.push_back(rt::append_async(env.context, writers[3],
                                     make_block<std::uint16_t>(begin, kElements, 0, index, dq_unit())));
  return rt::then(env.context, rt::when_all(env.context, std::move(appends)),
                  [context = env.context](std::vector<U64> counts) -> rt::Task<rt::Done> {
                    for (U64 count : counts) {
                      if (count != kElements) {
                        Result<rt::Done> failure;
                        failure.code = Code::internal;
                        return rt::ready<rt::Done>(context, std::move(failure));
                      }
                    }
                    Result<rt::Done> ok;
                    ok.code = Code::ok;
                    ok.value = rt::Done{};
                    return rt::ready<rt::Done>(context, std::move(ok));
                  });
}

rt::Task<std::vector<Result<ArrayRef>>> seal_writers(CaseEnv& env,
                                                     const std::vector<rt::WriterRef>& writers) {
  std::vector<rt::Task<ArrayRef>> seals;
  for (const rt::WriterRef& writer : writers) seals.push_back(rt::seal_async(env.context, writer));
  return rt::settle_all(env.context, std::move(seals));
}

rt::Task<std::vector<Result<rt::Done>>> abort_writers(CaseEnv& env,
                                                      const std::vector<rt::WriterRef>& writers) {
  std::vector<rt::Task<rt::Done>> aborts;
  for (const rt::WriterRef& writer : writers) aborts.push_back(rt::abort_async(env.context, writer));
  return rt::settle_all(env.context, std::move(aborts));
}

struct CaseResult {
  Code append_code = Code::ok;
  Code seal_code = Code::ok;
  Code abort_code = Code::ok;
  U64 spools_after_begin = 0;
  U64 spools_before_abort = 0;
  U64 spools_after_abort = 0;
  bool product = false;
  bool deferred_abort = false;
  std::string note;
};

template <class T>
bool all_ok(const std::vector<Result<T>>& results, Code& first_failure) {
  bool ok = true;
  for (const auto& result : results) {
    if (result.code != Code::ok) {
      if (ok) first_failure = result.code;
      ok = false;
    }
  }
  return ok;
}

// 1) Short write: an append I/O fails because RLIMIT_FSIZE caps the file size.
CaseResult case_short_write(const std::string& scratch) {
  CaseResult result;
  CaseEnv env;
  if (!setup_case(env, scratch, "validation.cal.fault.write")) {
    result.note = "setup failed";
    result.append_code = Code::internal;
    return result;
  }
  {
    std::vector<rt::WriterRef> writers = begin_writers(env);
    result.spools_after_begin = count_spools(scratch);
    if (writers.size() != 4) {
      result.note = "begin failed";
      result.append_code = Code::internal;
    } else {
      struct sigaction previous {};
      struct sigaction ignore {};
      ignore.sa_handler = SIG_IGN;
      sigaction(SIGXFSZ, &ignore, &previous);
      struct rlimit saved {};
      getrlimit(RLIMIT_FSIZE, &saved);
      struct rlimit capped = saved;
      capped.rlim_cur = 1024;  // the 32 KiB value segment cannot be written
      if (capped.rlim_max != RLIM_INFINITY && capped.rlim_max < capped.rlim_cur)
        capped.rlim_cur = capped.rlim_max;
      setrlimit(RLIMIT_FSIZE, &capped);
      Result<rt::Done> appended = rt::wait_external(append_frame(env, writers, 0));
      result.append_code = appended.code;
      result.note = "code=" + std::to_string(static_cast<int>(appended.code));
      for (const acsd::Issue& issue : appended.assessment.issues)
        result.note += " | " + issue.reason;
      setrlimit(RLIMIT_FSIZE, &saved);
      sigaction(SIGXFSZ, &previous, nullptr);

      result.spools_before_abort = count_spools(scratch);
      Code failure = Code::ok;
      Result<std::vector<Result<rt::Done>>> aborted =
          rt::wait_external(abort_writers(env, writers));
      if (aborted.code != Code::ok || !aborted.value) {
        result.abort_code = aborted.code == Code::ok ? Code::internal : aborted.code;
      } else {
        result.abort_code = all_ok(*aborted.value, failure) ? Code::ok : failure;
      }
      result.spools_after_abort = count_spools(scratch);
      result.note = result.append_code == Code::io_error
                        ? "short append I/O failed with io_error; all four writers drained"
                        : "append failed before the injected short write";
    }
    writers.clear();
  }
  teardown_case(env);
  return result;
}

// 2) Seal failure: the private spool disappears between append and seal.
CaseResult case_seal_failure(const std::string& scratch) {
  CaseResult result;
  CaseEnv env;
  if (!setup_case(env, scratch, "validation.cal.fault.seal")) {
    result.note = "setup failed";
    result.seal_code = Code::internal;
    return result;
  }
  {
    std::vector<rt::WriterRef> writers = begin_writers(env);
    result.spools_after_begin = count_spools(scratch);
    if (writers.size() != 4) {
      result.note = "begin failed";
      result.seal_code = Code::internal;
    } else {
      result.append_code = rt::wait_external(append_frame(env, writers, 0)).code;
      if (result.append_code != Code::ok) {
        result.note = "append failed before the seal injection";
      } else {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(scratch, ec))
          std::filesystem::remove_all(entry.path(), ec);
        Result<std::vector<Result<ArrayRef>>> sealed =
            rt::wait_external(seal_writers(env, writers));
        if (sealed.code != Code::ok || !sealed.value) {
          result.seal_code = sealed.code == Code::ok ? Code::internal : sealed.code;
        } else {
          Code failure = Code::ok;
          result.seal_code = all_ok(*sealed.value, failure) ? Code::ok : failure;
          for (const Result<ArrayRef>& array : *sealed.value)
            if (array.code == Code::ok && array.value) result.product = true;
        }
        result.spools_before_abort = count_spools(scratch);
        Code cleanup_failure = Code::ok;
        Result<std::vector<Result<rt::Done>>> aborted =
            rt::wait_external(abort_writers(env, writers));
        if (aborted.code != Code::ok || !aborted.value) {
          result.abort_code = aborted.code == Code::ok ? Code::internal : aborted.code;
        } else {
          result.abort_code =
              all_ok(*aborted.value, cleanup_failure) ? Code::ok : cleanup_failure;
        }
        result.spools_after_abort = count_spools(scratch);
        result.note = "canonical seal finalize failed after the spool disappeared";
      }
    }
    writers.clear();
  }
  teardown_case(env);
  return result;
}

// 3) Cancel after the write was submitted: abort must wait for the real I/O,
//    then reclaim the private spool even though the command is cancelled.
CaseResult case_cancel_after_submit(const std::string& scratch) {
  CaseResult result;
  CaseEnv env;
  if (!setup_case(env, scratch, "validation.cal.fault.cancel")) {
    result.note = "setup failed";
    result.abort_code = Code::internal;
    return result;
  }
  {
    std::vector<rt::WriterRef> writers = begin_writers(env);
    result.spools_after_begin = count_spools(scratch);
    if (writers.size() != 4) {
      result.note = "begin failed";
      result.abort_code = Code::internal;
    } else {
      rt::Task<rt::Done> append_task = append_frame(env, writers, 0);
      rt::request_cancel(env.context);
      rt::Task<std::vector<Result<rt::Done>>> abort_task = abort_writers(env, writers);
      result.spools_before_abort = count_spools(scratch);
      result.deferred_abort = result.spools_before_abort > 0;
      result.append_code = rt::wait_external(std::move(append_task)).code;
      Code failure = Code::ok;
      Result<std::vector<Result<rt::Done>>> aborted = rt::wait_external(std::move(abort_task));
      if (aborted.code != Code::ok || !aborted.value) {
        result.abort_code = aborted.code == Code::ok ? Code::internal : aborted.code;
      } else {
        result.abort_code = all_ok(*aborted.value, failure) ? Code::ok : failure;
      }
      result.spools_after_abort = count_spools(scratch);
      result.note = "cancelled command; abort waited for the real write and cleaned";
    }
    writers.clear();
  }
  teardown_case(env);
  return result;
}

std::string case_json(const std::string& name, const CaseResult& result) {
  std::string out = "{\"case\":\"" + name + "\"";
  out += ",\"append_code\":" + std::to_string(static_cast<int>(result.append_code));
  out += ",\"seal_code\":" + std::to_string(static_cast<int>(result.seal_code));
  out += ",\"abort_code\":" + std::to_string(static_cast<int>(result.abort_code));
  out += ",\"spools_after_begin\":" + std::to_string(result.spools_after_begin);
  out += ",\"spools_before_abort\":" + std::to_string(result.spools_before_abort);
  out += ",\"spools_after_abort\":" + std::to_string(result.spools_after_abort);
  out += ",\"product\":" + std::string(result.product ? "true" : "false");
  out += ",\"deferred_abort\":" + std::string(result.deferred_abort ? "true" : "false");
  out += ",\"note\":\"" + result.note + "\"}";
  return out;
}

}  // namespace

int main() {
  int failures = 0;
  auto check = [&](bool condition, const std::string& name) {
    std::printf("%s: %s\n", condition ? "PASS" : "FAIL", name.c_str());
    if (!condition) ++failures;
  };

  const CaseResult short_write = case_short_write("/tmp/acsd-cal-fault-write");
  check(short_write.spools_after_begin == 4, "short write: four private writers created");
  check(short_write.append_code != Code::ok, "short write: append reports a failure");
  check(short_write.append_code == Code::io_error, "short write: the failure is io_error");
  check(short_write.abort_code == Code::ok, "short write: abort drains all writers");
  check(short_write.spools_after_abort == 0, "short write: private spools removed");

  const CaseResult seal_failure = case_seal_failure("/tmp/acsd-cal-fault-seal");
  check(seal_failure.spools_after_begin == 4, "seal failure: four private writers created");
  check(seal_failure.append_code == Code::ok, "seal failure: append succeeded before injection");
  check(seal_failure.seal_code != Code::ok, "seal failure: seal reports a failure");
  check(!seal_failure.product, "seal failure: no ArrayRef product is delivered");
  check(seal_failure.abort_code == Code::ok, "seal failure: abort drains all writers");
  check(seal_failure.spools_after_abort == 0, "seal failure: private spools removed");

  const CaseResult cancel = case_cancel_after_submit("/tmp/acsd-cal-fault-cancel");
  check(cancel.spools_after_begin == 4, "cancel: four private writers created");
  check(cancel.deferred_abort, "cancel: the spool still existed when abort was requested");
  check(cancel.abort_code == Code::ok, "cancel: abort returns ok after the cancelled write");
  check(cancel.spools_after_abort == 0, "cancel: private spools removed after the real write");
  check(!cancel.product, "cancel: no product is delivered");

  std::printf("{\"case\":\"cal.faults\",\"failures\":%d,\"cases\":[%s,%s,%s]}\n",
              failures, case_json("short_write", short_write).c_str(),
              case_json("seal_failure", seal_failure).c_str(),
              case_json("cancel_after_submit", cancel).c_str());
  return failures == 0 ? 0 : 1;
}
