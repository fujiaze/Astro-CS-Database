#pragma once
// The single process wide resource service: one byte budget, one thread
// budget, one work counter and one CPU thread pool.  Every module borrows
// from this service through the public `Resources` interface.
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::resources {

class CpuResources final : public Resources {
 public:
  explicit CpuResources(const Limits& limits);
  ~CpuResources() override;

  Result<Reservation> reserve(U64 bytes, U32 threads) override;
  Result<U64> consume_work(U64 units) override;
  Usage usage() const override;

  // The total number of threads this service may run at once.
  U32 thread_capacity() const { return limits_.threads; }
  const Limits& limits() const { return limits_; }

  // Runs `body(index)` for index in [0, count) using at most `threads` workers.
  // The caller must have reserved the thread budget.  The work is executed in
  // a deterministic reduction order by the caller; this helper only provides
  // the parallel execution.
  void parallel_for(U64 count, U32 threads, const std::function<void(U64)>& body);

  // Scratch directory for spills, empty when the budget forbids spilling.
  const std::string& scratch_directory() const { return limits_.scratch_directory; }

  // Returns a reservation to the pool.  Called by the lease deleter.
  void release_lease(U64 bytes, U32 threads);

 private:

  Limits limits_;
  std::atomic<U64> bytes_in_use_{0};
  std::atomic<U64> bytes_peak_{0};
  std::atomic<U64> work_used_{0};
  std::atomic<U64> threads_in_use_{0};
  std::atomic<U64> read_bytes_{0};
  std::atomic<U64> written_bytes_{0};
  std::atomic<U64> spill_bytes_{0};
  mutable std::mutex mutex_;
  std::atomic<double> elapsed_{0.0};
  std::vector<std::thread> workers_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::condition_variable done_cv_;
  std::function<void(U64)> job_;
  U64 job_count_ = 0;
  U64 job_next_ = 0;
  U64 job_active_ = 0;
  std::uint64_t job_generation_ = 0;
  bool shutdown_ = false;
};

// Monotonic elapsed seconds since the process started.
double process_uptime_seconds();

}  // namespace acsd::resources
