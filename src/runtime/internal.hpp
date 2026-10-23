#pragma once
// Internal implementation contract shared by the runtime translation units.
//
// Nothing here is public API: modules see only include/acsd/runtime/*.hpp.
// This header is the single place where the ledger, the executor, the command
// queues, the block store and the probe sink are declared, so that the runtime
// has exactly one pool, one ledger and one cancellation state.
#include <acsd/runtime/diagnostics.hpp>
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/pipe.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/storage.hpp>
#include <acsd/runtime/task.hpp>
#include <acsd/runtime/types.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace acsd::rt {

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

struct CancelState {
  std::atomic<bool> requested{false};
  std::mutex mutex;
  std::condition_variable cv;
  // Bounded wake hooks (pipe waiters, parked tasks).  They run outside the
  // lock and must only wake already-accounted nodes; they never start science.
  std::vector<std::function<void()>> watchers;

  bool is_requested() const noexcept { return requested.load(); }
  void request() noexcept {
    requested.store(true);
    std::vector<std::function<void()>> copy;
    {
      std::lock_guard<std::mutex> lock(mutex);
      copy = watchers;
      cv.notify_all();
    }
    for (auto& watcher : copy) {
      try {
        watcher();
      } catch (...) {
      }
    }
  }
  void add_watcher(std::function<void()> watcher) {
    if (!watcher) return;
    std::lock_guard<std::mutex> lock(mutex);
    watchers.push_back(std::move(watcher));
  }
};

namespace detail {

struct ArenaImpl;
struct ClaimImpl;
class TaskNode;

}  // namespace detail

// ---------------------------------------------------------------------------
// Resource ledger
// ---------------------------------------------------------------------------

// Scope states: accepting -> stopping -> draining -> closed.
enum class ScopeStatus { accepting, stopping, draining, closed };

struct ScopeState {
  Id local_id;
  ScopeRef parent;
  ScopeLimits limits;
  std::shared_ptr<CancelState> cancel;
  std::chrono::steady_clock::time_point start;
  std::weak_ptr<Services> owner;

  mutable std::mutex mutex;
  ResourceVector held, claimed, allocated, peak_allocated;
  // work/work_reserved are subtree aggregates updated on every ancestor, so a
  // scope snapshot never re-adds its children.  own_work/work_upper belong to
  // the execution segment of one task scope.
  U64 work = 0;
  U64 work_reserved = 0;
  U64 own_work = 0;
  U64 work_upper = 0;
  // Set on the task sub-scope of an admitted execution segment: work_upper is
  // then a hard cap, including the explicit zero.
  bool work_upper_enforced = false;
  U64 read_bytes = 0, written_bytes = 0, spill_written_bytes = 0;
  U64 cleanup_work = 0;
  U64 metadata_bytes = 0;
  ScopeStatus status = ScopeStatus::accepting;
  std::vector<std::weak_ptr<ScopeState>> children;

  void add_child(const ScopeRef& child);
};

namespace detail {

struct CommandState;

// Locks root..scope in that order.  No user code, allocator or I/O may run
// while the chain is held.
std::vector<std::shared_ptr<ScopeState>> scope_chain(const ScopeRef& scope);

// Ledger operations shared by scope/arena/claim/allocate.  They always update
// every ancestor atomically and leave no side effect on failure.
Result<Done> ledger_reserve(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_claim_committed(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_commit(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_release_claimed(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_release_committed(const ScopeRef& scope,
                              const ResourceVector& bytes);
void ledger_return_held(const ScopeRef& scope, const ResourceVector& bytes);
Result<Done> ledger_admit_work(const ScopeRef& scope, U64 units);
void ledger_consume_work(const ScopeRef& scope, U64 units);
void ledger_release_reserved_work(const ScopeRef& scope, U64 units);
void ledger_account_bytes(const ScopeRef& scope, U64 read, U64 written,
                          U64 spill);
// Wakes tasks parked in waiting_resources after capacity was returned.  The
// executor owner is resolved from the scope, so the ledger never holds the
// Services object alive.
void services_notify_capacity(const ScopeRef& scope);

struct ArenaImpl {
  ScopeRef scope;
  mutable std::mutex mutex;
  bool open = true;
  ResourceVector available;
};

struct ClaimImpl {
  std::shared_ptr<ArenaImpl> arena;
  ResourceVector bytes;
  bool committed = false;
  bool released = false;
  std::mutex mutex;
  // Releases the capacity exactly once, when the last observation of the
  // ticket dies.  For a committed ticket the physical bytes are already gone.
  ~ClaimImpl();
};

// Allocation owner: the Lease of a Buffer/Block owns this object, therefore
// the committed capacity is returned only when the last lease dies.
struct Allocation {
  void* memory = nullptr;
  std::size_t bytes = 0;
  std::size_t alignment = 0;
  std::shared_ptr<ClaimImpl> claim;
  ~Allocation();
  static void destroy(void* memory, std::size_t bytes,
                      std::size_t alignment) noexcept;
};

// Execution-段 capacity ticket.  It claims only the capacity components
// (host/pinned/device/spill); cpu_tokens/io_slots are execution credits and are
// never kept in the long-lived R/F reservation (SCHEDULER.md).
Result<std::shared_ptr<ClaimImpl>> claim_capacity(const ArenaRef& arena,
                                                  const ResourceVector& bytes);

// Creates an arena bound to an existing scope.  Used by reserve() and by the
// executor; callers must have reserved the capacity on the scope first.
Result<ArenaRef> make_arena(const ScopeRef& scope, ResourceVector available);
// Raw byte buffers for codec/spool staging.  Same claim -> commit -> release
// accounting and Lease ownership as typed buffers.
Result<Buffer<std::byte>> allocate_bytes(InvocationContext, ArenaRef, U64 bytes);
Result<Block<std::byte>> freeze_bytes(Buffer<std::byte>&&);
// Creates a child scope without requiring a full InvocationContext (used by
// the executor when it creates a command scope).
Result<ScopeRef> make_child_scope(const ScopeRef& parent, Id local_id,
                                  ScopeLimits limits);
void set_scope_status(const ScopeRef& scope, ScopeStatus status);
ScopeStatus scope_status(const ScopeRef& scope);
// Idempotent: returns any still-unclaimed arena capacity; a scope that still
// owns live outputs stays accounted until the last Lease dies.
void close_scope(const ScopeRef& scope);

// ---------------------------------------------------------------------------
// Task node
// ---------------------------------------------------------------------------

class TaskNode {
 public:
  TaskNode(InvocationContext context, TaskSpec specification);
  ~TaskNode();

  TaskId id = 0;
  InvocationContext context;
  TaskSpec spec;
  std::atomic<TaskState> state{TaskState::waiting_dependencies};
  std::atomic<bool> terminal{false};

  // The body of one execution segment.  It is installed before the node is
  // enqueued and destroyed as soon as the segment ends (FOUNDATION_API: a
  // Task is not a suspended function stack).
  std::function<void(InvocationContext)> body;

  std::mutex mutex;
  std::condition_variable cv;
  Code code = Code::internal;
  Assessment assessment;
  Usage usage;
  std::vector<Id> dependencies;
  std::shared_ptr<void> value;
  std::function<void()> continuation;
  bool continuation_attached = false;

  // Execution segment bookkeeping (only touched by the owning worker).
  ScopeRef task_scope;
  U64 work_upper = 0;
  U64 work_consumed = 0;
  std::shared_ptr<ClaimImpl> running_claim;
  std::shared_ptr<CommandState> command;

  void set_state(TaskState next) noexcept { state.store(next); }
};

// ---------------------------------------------------------------------------
// Commands and executor services
// ---------------------------------------------------------------------------

struct CommandState {
  Id id;
  U32 weight = 1;
  ScopeRef scope;
  std::shared_ptr<CancelState> cancel;
  std::mutex mutex;
  std::deque<std::shared_ptr<TaskNode>> ready;
  std::deque<std::shared_ptr<TaskNode>> waiting;  // waiting_resources retry
  U32 credit = 1;      // dispatch credit in the current round (kept for wire)
  std::int32_t deficit = 0;  // deficit round-robin credit (weighted share)
  U32 cpu_running = 0; // CPU tokens held by running nodes of this command
  U32 in_flight = 0;   // admitted nodes not yet terminal
  bool draining = false;
  bool accept_new = true;
  std::condition_variable drained_cv;
};

struct IoRequest {
  bool write = false;
  std::function<void()> body;
};

class BlockStore {
 public:
  void register_array(const Id& id,
                      std::shared_ptr<const std::vector<std::byte>> bytes,
                      DType dtype, std::vector<U64> shape, Lease lease = {});
  void register_blocks(const Id& array_id,
                       const std::vector<BlockDescriptor>& blocks);
  // Returns nullptr when the block is unknown; the caller reports
  // missing_dependency instead of reading zero bytes.
  std::shared_ptr<const std::vector<std::byte>> find(const Id& id) const;
  std::optional<BlockDescriptor> find_block(const Id& block_id) const;
  // Associates the capacity Lease of an in-memory array with its entry so the
  // charge lives as long as the stored bytes do.
  void retain(const Id& id, Lease lease);
  Lease lease_of(const Id& id) const;
  // Lifetime of a stored array.  register_array starts at one reference; a
  // caller that hands out an additional Array handle adds one and releases it
  // from the Array's deleter.  release_ref erases the entry and returns its
  // Lease exactly when the last reference dies, so an array is never pinned by
  // the cache after every ArrayRef is gone.
  void add_ref(const Id& id);
  bool release_ref(const Id& id);

 private:
  mutable std::mutex mutex_;
  struct Entry {
    std::shared_ptr<const std::vector<std::byte>> bytes;
    DType dtype = DType::f64;
    std::vector<U64> shape;
    std::vector<BlockDescriptor> blocks;
    Lease lease;
    U32 refs = 0;
  };
  mutable std::unordered_map<Id, Entry> entries_;
  mutable std::unordered_map<Id, BlockDescriptor> block_index_;
};

// Probe sink.  Defined in diagnostics.cpp; when ACSD_DEV_MODE is 0 the
// pointer stays null and no environment, directory or writer exists.
struct ProbeSink {
  virtual ~ProbeSink() = default;
  virtual bool enabled() const noexcept = 0;
  virtual bool selected(const InvocationContext&, std::string_view,
                        const ProbeTags&) noexcept = 0;
  virtual void record(const InvocationContext&, const PerfRecord&) noexcept = 0;
  virtual void snapshot(const InvocationContext&, std::string_view,
                        const ProbeTags&, const SnapshotPayload&) noexcept = 0;
};
std::shared_ptr<ProbeSink> make_probe_sink(
    const PoolConfig&, std::function<void(IoRequest)> submit_io,
    std::string& error);

// ---------------------------------------------------------------------------
struct BuilderCore;
struct HostCore;

// ---------------------------------------------------------------------------
// Science Context bridge
// ---------------------------------------------------------------------------
//
// The existing scientific free functions take acsd::Context (Resources,
// Cancellation, Storage).  The bridge serves that interface from the runtime:
// arrays live in the unique BlockStore with their capacity Lease, the ledger
// is the current scope, and cancellation is the scope CancelState.  It is
// synchronous by contract and only runs inside an already-admitted CPU
// execution segment; it never creates a second pool or ledger.
class ScienceContext {
 public:
  ScienceContext();
  ~ScienceContext();
  ScienceContext(const ScienceContext&) = delete;
  ScienceContext& operator=(const ScienceContext&) = delete;
  acsd::Context& context();

 private:
  struct Impl;
  explicit ScienceContext(std::shared_ptr<Impl> impl);
  std::shared_ptr<Impl> impl_;
  friend Result<std::shared_ptr<ScienceContext>> make_science_context(
      InvocationContext, ArenaRef);
};

Result<std::shared_ptr<ScienceContext>> make_science_context(InvocationContext,
                                                             ArenaRef);

}  // namespace detail

// ---------------------------------------------------------------------------
// The single runtime service: one ledger, one CPU pool, one I/O lane set.
// ---------------------------------------------------------------------------

class Services {
 public:
  static std::shared_ptr<Services> create(const PoolConfig& config,
                                          std::string& error);
  ~Services();

  ScopeRef root;
  std::shared_ptr<detail::BlockStore> blocks;
  std::shared_ptr<detail::ProbeSink> probes;

  U32 cpu_threads() const noexcept { return cpu_threads_; }
  U32 read_slots() const noexcept { return read_slots_; }
  U32 write_slots() const noexcept { return write_slots_; }
  const PoolConfig& config() const noexcept { return config_; }

  std::shared_ptr<detail::CommandState> add_command(Id id, ScopeLimits limits,
                                            U32 weight, std::string& error);
  void remove_command(const std::shared_ptr<detail::CommandState>& command);

  // Admits and enqueues a task node.  The node already carries its spec and
  // context; the body runs on the unique CPU pool.
  void submit(const std::shared_ptr<detail::TaskNode>& node,
              std::function<void(InvocationContext)> body);
  // A continuation which must not be attributed to a command (completion
  // callbacks, cleanup).  It still runs on the CPU pool.
  void schedule_control(const std::shared_ptr<detail::TaskNode>& node,
                        std::function<void(InvocationContext)> body);
  void submit_io(detail::IoRequest request);
  // Called by the ledger after capacity is returned; retries tasks parked in
  // waiting_resources.  Never called with a ledger lock held.
  void notify_capacity();
  void shutdown();
  bool shutting_down() const noexcept { return shutdown_.load(); }

  // Work accounting helpers used by the executor.
  Result<Done> admit_work(const ScopeRef& scope, U64 units);
  void release_work(const ScopeRef& scope, U64 units);

  // Probes (defined in diagnostics.cpp).
  bool probe_enabled() const noexcept;
  void record_probe(const InvocationContext&, const PerfRecord&) noexcept;
  void snapshot_probe(const InvocationContext&, std::string_view,
                      const ProbeTags&, const SnapshotPayload&) noexcept;
  bool probe_selected(const InvocationContext&, std::string_view,
                      const ProbeTags&) noexcept;
  U64 next_span_id() noexcept;

 private:
  Services() = default;
  void worker_loop();
  void io_loop(bool write);
  bool take_runnable(std::shared_ptr<detail::TaskNode>& node);
  void run_node(std::shared_ptr<detail::TaskNode> node);

  PoolConfig config_;
  U32 cpu_threads_ = 1, read_slots_ = 1, write_slots_ = 1;

  mutable std::mutex commands_mutex_;
  std::vector<std::shared_ptr<detail::CommandState>> commands_;
  std::size_t rr_index_ = 0;
  std::condition_variable ready_cv_;

  mutable std::mutex control_mutex_;
  std::deque<std::shared_ptr<detail::TaskNode>> control_;
  std::condition_variable control_cv_;

  mutable std::mutex io_mutex_;
  std::deque<detail::IoRequest> reads_, writes_;
  std::condition_variable read_cv_, write_cv_;

  std::vector<std::thread> workers_;
  std::atomic<bool> shutdown_{false};
  std::atomic<U32> cpu_in_use_{0};
  std::atomic<U64> span_counter_{1};

  friend class detail::TaskNode;
};


namespace detail {

std::shared_ptr<Services> services_of(const InvocationContext&);
bool cancellation_requested(const std::shared_ptr<CancelState>&) noexcept;

// Frozen registry storage shared by registry.cpp/host.cpp/wire.cpp
struct RegistryCore {
  struct Entry {
    ProcessDescriptor descriptor;
    ErasedOperation erased;
  };
  std::map<std::pair<Id, Id>, Entry> operations;      // (process, operation)
  std::map<std::pair<Id, U32>, ErasedCodec> codecs;   // (schema, revision)
  std::string manifest_json;
};

}  // namespace detail
}  // namespace acsd::rt
