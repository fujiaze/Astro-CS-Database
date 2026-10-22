#include "resources/cpu_resources.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "model/units.hpp"

namespace acsd::resources {
namespace {

double now_seconds() {
  using clock = std::chrono::steady_clock;
  static const clock::time_point origin = clock::now();
  return std::chrono::duration<double>(clock::now() - origin).count();
}

struct LeaseState {
  U64 bytes = 0;
  U32 threads = 0;
};

struct LeaseRelease {
  CpuResources* owner = nullptr;
  std::shared_ptr<LeaseState> state;
  void operator()(const void*) const noexcept {
    if (owner != nullptr && state) owner->release_lease(state->bytes, state->threads);
  }
};

}  // namespace

double process_uptime_seconds() { return now_seconds(); }

CpuResources::CpuResources(const Limits& limits) : limits_(limits) {
  if (limits_.threads > 0) {
    // A single shared pool; nested parallel sections are forbidden by
    // construction because the callers lease threads before entering.
    workers_.reserve(limits_.threads);
    for (U32 i = 0; i < limits_.threads; ++i) {
      workers_.emplace_back([this] {
        std::uint64_t seen_generation = 0;
        while (true) {
          std::unique_lock<std::mutex> lock(queue_mutex_);
          queue_cv_.wait(lock, [this, &seen_generation] {
            return shutdown_ || job_generation_ != seen_generation;
          });
          if (shutdown_) return;
          seen_generation = job_generation_;
          while (job_next_ < job_count_) {
            const U64 index = job_next_++;
            const auto body = job_;
            lock.unlock();
            body(index);
            lock.lock();
            if (--job_active_ == 0) done_cv_.notify_all();
          }
        }
      });
    }
  }
}

CpuResources::~CpuResources() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    shutdown_ = true;
  }
  queue_cv_.notify_all();
  for (std::thread& worker : workers_) {
    if (worker.joinable()) worker.join();
  }
}

Result<Reservation> CpuResources::reserve(U64 bytes, U32 threads) {
  if (bytes > limits_.bytes) {
    return model::fail<Reservation>(Code::resource_exhausted,
                                    "reservation exceeds the configured byte budget",
                                    "resources.reserve", "requested bytes exceed the process budget");
  }
  if (threads > limits_.threads) {
    return model::fail<Reservation>(Code::resource_exhausted,
                                    "reservation exceeds the configured thread budget",
                                    "resources.reserve", "requested threads exceed the process budget");
  }
  U64 previous_bytes = bytes_in_use_.fetch_add(bytes);
  U64 previous_threads = threads_in_use_.fetch_add(threads);
  if (previous_bytes + bytes > limits_.bytes || previous_threads + threads > limits_.threads) {
    bytes_in_use_.fetch_sub(bytes);
    threads_in_use_.fetch_sub(threads);
    return model::fail<Reservation>(Code::resource_exhausted,
                                    "the process resource budget is exhausted",
                                    "resources.reserve", "another lease holds the required capacity");
  }
  const U64 peak = bytes_in_use_.load();
  U64 observed = bytes_peak_.load();
  while (peak > observed && !bytes_peak_.compare_exchange_weak(observed, peak)) {}

  Reservation reservation;
  reservation.bytes = bytes;
  reservation.threads = threads;
  auto* self = this;
  reservation.lease.owner = std::shared_ptr<const void>(
      new std::uint8_t(0), [self, bytes, threads](const void*) {
        self->release_lease(bytes, threads);
      });
  return model::ok_result(std::move(reservation));
}

Result<U64> CpuResources::consume_work(U64 units) {
  const U64 used = work_used_.fetch_add(units);
  if (used + units > limits_.work) {
    return model::fail<U64>(Code::resource_exhausted, "the work budget is exhausted",
                            "resources.consume_work", "no further work may be consumed");
  }
  return model::ok_result(used);
}

void CpuResources::release_lease(U64 bytes, U32 threads) {
  bytes_in_use_.fetch_sub(bytes);
  threads_in_use_.fetch_sub(threads);
}

Usage CpuResources::usage() const {
  Usage usage;
  usage.work = work_used_.load();
  usage.peak_payload_bytes = bytes_peak_.load();
  usage.read_bytes = read_bytes_.load();
  usage.written_bytes = written_bytes_.load();
  usage.spill_bytes = spill_bytes_.load();
  usage.elapsed_seconds = now_seconds();
  return usage;
}

void CpuResources::parallel_for(U64 count, U32 threads, const std::function<void(U64)>& body) {
  if (count == 0) return;
  if (workers_.empty() || threads <= 1 || count == 1) {
    for (U64 i = 0; i < count; ++i) body(i);
    return;
  }
  const U32 use = static_cast<U32>(std::min<U64>(threads, workers_.size()));
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    job_ = body;
    job_count_ = count;
    job_next_ = 0;
    job_active_ = use;
    ++job_generation_;
  }
  queue_cv_.notify_all();
  // The calling thread participates as one of the workers.
  while (true) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (job_next_ >= job_count_) break;
    const U64 index = job_next_++;
    const auto job = job_;
    lock.unlock();
    job(index);
    lock.lock();
    if (--job_active_ == 0) done_cv_.notify_all();
  }
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    done_cv_.wait(lock, [this] { return job_active_ == 0; });
    job_ = nullptr;
    job_count_ = 0;
    job_next_ = 0;
  }
}

}  // namespace acsd::resources
