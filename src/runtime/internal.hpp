#pragma once
// Internal implementation contract shared by the runtime translation units.
//
// Nothing here is public API: modules see only include/acsd/runtime/*.hpp.
// This header is the single place where the ledger, the executor, the command
// queues, the block store and the probe sink are declared, so that the runtime
// has exactly one pool, one ledger and one cancellation state.
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/pipe.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/storage.hpp>
#include <acsd/runtime/task.hpp>
#include <acsd/runtime/diagnostics.hpp>
#include <acsd/runtime/types.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace acsd::rt::detail {

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

struct CancelState {
  std::atomic<bool> requested{false};
  std::mutex mutex;
  std::condition_variable cv;

  bool is_requested() const noexcept { return requested.load(); }
  void request() noexcept {
    requested.store(true);
    std::lock_guard<std::mutex> lock(mutex);
    cv.notify_all();
  }
};

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

  mutable std::mutex mutex;
  ResourceVector held, claimed, allocated, peak_allocated;
  U64 work = 0;               // work consumed directly in this scope
  U64 work_reserved = 0;      // work admitted for running tasks of this scope
  U64 read_bytes = 0, written_bytes = 0, spill_written_bytes = 0;
  U64 cleanup_work = 0;
  U64 metadata_bytes = 0;
  ScopeStatus status = ScopeStatus::accepting;
  std::vector<std::weak_ptr<ScopeState>> children;

  // Child registration is done under the parent mutex; the child keeps its
  // parent alive.  A closed scope still keeps its live outputs accounted.
  void add_child(const ScopeRef& child);
};

// Locks root..scope in that order.  No user code, allocator or I/O may run
// while the chain is held.
std::vector<std::shared_ptr<ScopeState>> scope_chain(const ScopeRef& scope);

// Ledger operations shared by scope/arena/claim/allocate.  They always update
// every ancestor atomically and leave no side effect on failure.
Result<Done> ledger_reserve(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_claim_committed(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_commit(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_release_claimed(const ScopeRef& scope, const ResourceVector& bytes);
void ledger_release_committed(const ScopeRef& scope, const ResourceVector& bytes);
// Returns unused capacity to the arena; caller holds no lock.
void ledger_return_held(const ScopeRef& scope, const ResourceVector& bytes);
Result<Done> ledger_admit_work(const ScopeRef& scope, U64 units);
void ledger_consume_work(const ScopeRef& scope, U64 units);
void ledger_release_reserved_work(const ScopeRef& scope, U64 units);

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
};

// Allocation owner: the Lease of a Buffer/Block owns this object, therefore
// the committed capacity is returned only when the last lease dies.
struct Allocation {
  void* memory = nullptr;
  std::size_t bytes = 0;
  std::size_t alignment = 0;
  std::shared_ptr<ClaimImpl> claim;
  ~Allocation();
  static void destroy(void* memory, std::size_t bytes, std::size_t alignment) noexcept;
};

// ---------------------------------------------------------------------------
// Task node
// ---------------------------------------------------------------------------

struct CommandState;

class TaskNode {
 public:
  TaskNode(InvocationContext context, TaskSpec specification);
  ~TaskNode();

  TaskId id = 0;
  InvocationContext context;
  TaskSpec spec;
  std::atomic<TaskState> state{TaskState::waiting_dependencies};
  std::atomic<bool> terminal{false};

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
  U32 credit = 1;               // dispatch credit in the current round
  bool accept_new = true;
};

struct IoRequest {
  bool write = false;
  std::function<void()> body;
};

class BlockStore {
 public:
  void register_array(const Id& id, std::shared_ptr<const std::vector<std::byte>> bytes,
                      DType dtype, std::vector<U64> shape);
  // Returns nullptr when the block is unknown; the caller reports
  // missing_dependency instead of reading zero bytes.
  std::shared_ptr<const std::vector<std::byte>> find(const Id& id) const;

 private:
  mutable std::mutex mutex_;
  struct Entry {
    std::shared_ptr<const std::vector<std::byte>> bytes;
    DType dtype;
    std::vector<U64> shape;
  };
  std::unordered_map<Id, Entry> entries_;
};

struct ProbeSink;  // defined in diagnostics.cpp

class Services {
 public:
  static std::shared_ptr<Services> create(const PoolConfig& config,
                                          std::string& error);
  ~Services();

  ScopeRef root;
  std::shared_ptr<BlockStore> blocks;
  std::shared_ptr<ProbeSink> probes;

  U32 cpu_threads() const noexcept { return cpu_threads_; }
  U32 read_slots() const noexcept { return read_slots_; }
  U32 write_slots() const noexcept { return write_slots_; }
  const PoolConfig& config() const noexcept { return config_; }

  std::shared_ptr<CommandState> add_command(Id id, ScopeLimits limits,
                                            U32 weight, std::string& error);
  void remove_command(const std::shared_ptr<CommandState>& command);

  // Admits and enqueues a task node.  The node already carries its spec and
  // context; the body runs on the unique CPU pool.
  void submit(const std::shared_ptr<TaskNode>& node,
              std::function<void(InvocationContext)> body);
  // A continuation which must not be attributed to a command (completion
  // callbacks, cleanup).  It still runs on the CPU pool.
  void schedule_control(const std::shared_ptr<TaskNode>& node,
                        std::function<void(InvocationContext)> body);
  // Marks a node ready without CPU work (used by then/settle mirroring and by
  // the I/O completion path).  The node completes in the caller.
  void enqueue_ready(const std::shared_ptr<TaskNode>& node);

  void submit_io(IoRequest request);
  void shutdown();
  bool shutting_down() const noexcept { return shutdown_.load(); }

  // Work accounting helpers used by the executor.
  Result<Done> admit_work(const ScopeRef& scope, U64 units);
  void release_work(const ScopeRef& scope, U64 units);

  // Probes.
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
  bool take_runnable(std::shared_ptr<TaskNode>& node);
  void run_node(std::shared_ptr<TaskNode> node,
                std::function<void(InvocationContext)> body);
  void complete_node_state(const std::shared_ptr<TaskNode>& node);

  PoolConfig config_;
  U32 cpu_threads_ = 1, read_slots_ = 1, write_slots_ = 1;
  ScopeRef root_scope_;

  mutable std::mutex commands_mutex_;
  std::vector<std::shared_ptr<CommandState>> commands_;
  std::size_t rr_index_ = 0;
  std::condition_variable ready_cv_;

  mutable std::mutex control_mutex_;
  std::deque<std::pair<std::shared_ptr<TaskNode>,
                       std::function<void(InvocationContext)>>>
      control_;
  std::condition_variable control_cv_;

  mutable std::mutex io_mutex_;
  std::deque<IoRequest> reads_, writes_;
  std::condition_variable read_cv_, write_cv_;

  std::vector<std::thread> workers_;
  std::atomic<bool> shutdown_{false};
  std::atomic<U32> cpu_in_use_{0};
  std::atomic<U64> span_counter_{1};
};

// Returns the Services of a context; a null pointer means the context was not
// opened by make_services and every runtime call fails as invalid_request.
std::shared_ptr<Services> services_of(const InvocationContext&);

// Scheduler-side cancellation check used by task/pipe/storage.
bool cancellation_requested(const std::shared_ptr<CancelState>&) noexcept;

// Registry internals (definition in registry.cpp).
struct ManifestOperation {
  Id process, operation;
  TypeId input, output;
  std::string document;
  Id module;
  U32 semantic_revision = 1;
};
struct ManifestProcess {
  ProcessDescriptor descriptor;
  bool has_codec = false;
};

}  // namespace acsd::rt::detail
