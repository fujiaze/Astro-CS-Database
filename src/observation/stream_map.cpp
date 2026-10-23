// stream_map_ordered: the generic ordered block map over one stream region.
//
// One cohort (a bounded flattened index range) is loaded, transformed and
// appended through a sink; the sink always receives the lowest required key.
// At most `window` cohorts are expanded at once (the plan window, default 1),
// so a slow cohort does not stall the whole region and no unbounded vector of
// tasks/futures is built.  The first required failure stops launching new
// cohorts, drains the already expanded tasks and returns the original failure
// without a fabricated completion value.
//
// The template definition lives here; the calibration instantiation at the end
// of this file is the one the observation.calibrate adapter links.  A new
// process that wants the same combiner adds one explicit instantiation in its
// own translation unit.
#include <acsd/modules/observation.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd::modules {
namespace {

template <class In, class Out>
struct StreamState {
  using Load = std::function<rt::Task<In>(BlockContext)>;
  using Transform = std::function<rt::Task<Out>(BlockContext, In)>;
  using Sink = std::function<rt::Task<rt::Done>(BlockContext, Out)>;

  rt::InvocationContext context;
  rt::ArenaRef arena;
  U64 elements = 0;
  U64 block_elements = 0;
  U64 cohorts = 0;
  U64 next = 0;
  std::size_t window = 1;
  Load load;
  Transform transform;
  Sink sink;
  std::deque<std::pair<BlockContext, rt::Task<Out>>> pending;
  std::function<rt::Task<rt::Done>(rt::InvocationContext)> step;
};

template <class T>
Result<rt::Done> propagate_failure(const Result<T>& from) {
  Result<rt::Done> out;
  out.code = from.code;
  out.assessment = from.assessment;
  out.usage = from.usage;
  out.dependencies = from.dependencies;
  return out;
}

Result<rt::Done> invalid_request_done(const char* reason) {
  Result<rt::Done> out;
  out.code = Code::invalid_request;
  out.assessment.complete = false;
  out.assessment.validity = Validity::invalid;
  out.assessment.execution = ExecutionState::failed;
  Issue issue;
  issue.code = Code::invalid_request;
  issue.origin = "stream_map_ordered";
  issue.reason = reason;
  out.assessment.issues.push_back(std::move(issue));
  return out;
}

template <class In, class Out>
rt::Task<rt::Done> drain_and_fail(const std::shared_ptr<StreamState<In, Out>>& state,
                                  const Result<Out>& failure) {
  std::vector<rt::Task<Out>> remaining;
  remaining.reserve(state->pending.size());
  for (auto& entry : state->pending) remaining.push_back(std::move(entry.second));
  state->pending.clear();
  if (remaining.empty()) {
    return rt::ready<rt::Done>(state->context, propagate_failure(failure));
  }
  // The already expanded cohorts keep running and must really drain before the
  // outer task fails; their values/errors are secondary to the first one.
  rt::InvocationContext context = state->context;
  return rt::then(
      context, rt::settle_all(context, std::move(remaining)),
      [context, failure](std::vector<Result<Out>>) -> rt::Task<rt::Done> {
        return rt::ready<rt::Done>(context, propagate_failure(failure));
      });
}

}  // namespace

template <class In, class Out>
rt::Task<rt::Done> stream_map_ordered(
    rt::InvocationContext context, Ref<rt::InvocationPlan> plan, U32 region_id,
    std::function<rt::Task<In>(BlockContext)> load,
    std::function<rt::Task<Out>(BlockContext, In)> transform,
    std::function<rt::Task<rt::Done>(BlockContext, Out)> sink) {
  if (!plan || !load || !transform || !sink) {
    return rt::ready<rt::Done>(context, invalid_request_done("the combiner has no plan or hook"));
  }
  if (context.region && *context.region != region_id) {
    return rt::ready<rt::Done>(context,
                               invalid_request_done("the region context does not match the plan"));
  }
  if (plan->elements == 0 || plan->block_elements == 0) {
    return rt::ready<rt::Done>(context,
                               invalid_request_done("the plan has no bounded cohort"));
  }
  rt::ArenaRef arena = context.progress ? context.progress : context.persistent;
  if (!arena) {
    return rt::ready<rt::Done>(context,
                               invalid_request_done("the region has no bound arena"));
  }

  using State = StreamState<In, Out>;
  auto state = std::make_shared<State>();
  state->context = context;
  state->arena = std::move(arena);
  state->elements = plan->elements;
  state->block_elements = plan->block_elements;
  state->cohorts = (state->elements + state->block_elements - 1) / state->block_elements;
  state->window = plan->options.window == 0 ? 1 : plan->options.window;
  if (state->window > state->cohorts) state->window = static_cast<std::size_t>(
      state->cohorts == 0 ? 1 : state->cohorts);
  state->load = std::move(load);
  state->transform = std::move(transform);
  state->sink = std::move(sink);

  state->step = [state](rt::InvocationContext step_context) -> rt::Task<rt::Done> {
    // Expand up to the planned window.  Every expanded cohort already owns its
    // TaskSpec/reservation through the Storage and kernel submissions.
    while (state->next < state->cohorts && state->pending.size() < state->window) {
      const U64 cohort = state->next++;
      const U64 begin = cohort * state->block_elements;
      const U64 available = state->elements - begin;
      const U64 count = available < state->block_elements ? available : state->block_elements;
      BlockContext block;
      block.invocation = state->context;
      block.arena = state->arena;
      block.key = rt::Key{0, cohort, 1, 0};
      block.range = BlockRequest{begin, count};
      rt::Task<In> loaded = state->load(block);
      // The key of the block and the strong state keep the block context alive
      // until the sink consumed it.
      rt::Task<Out> mapped =
          rt::then(state->context, std::move(loaded),
                   [state, block](In value) -> rt::Task<Out> {
                     return state->transform(block, std::move(value));
                   });
      state->pending.emplace_back(block, std::move(mapped));
    }
    if (state->pending.empty()) {
      return rt::ready<rt::Done>(state->context, model::ok_result(rt::Done{}));
    }
    std::pair<BlockContext, rt::Task<Out>> first = std::move(state->pending.front());
    state->pending.pop_front();
    return rt::settle(
        state->context, std::move(first.second),
        [state, block = std::move(first.first)](Result<Out> mapped) -> rt::Task<rt::Done> {
          if (mapped.code != Code::ok || !mapped.value) {
            Result<Out> effective = mapped;
            if (effective.code == Code::ok) {
              effective.code = Code::internal;
              effective.assessment.complete = false;
              effective.assessment.validity = Validity::invalid;
              Issue issue;
              issue.code = Code::internal;
              issue.origin = "stream_map_ordered";
              issue.reason = "the transform completed without a value";
              effective.assessment.issues.push_back(std::move(issue));
            }
            return drain_and_fail(state, effective);
          }
          return rt::then(
              state->context, state->sink(block, std::move(*mapped.value)),
              [state](rt::Done) -> rt::Task<rt::Done> {
                return state->step(state->context);
              });
        });
  };

  return state->step(context);
}

// The observation.calibrate instantiation.  The adapter calls exactly this
// pair; another process adds its own instantiation next to its adapter.
template rt::Task<rt::Done> stream_map_ordered<CalibrationBlock, SignalBlock>(
    rt::InvocationContext, Ref<rt::InvocationPlan>, U32,
    std::function<rt::Task<CalibrationBlock>(BlockContext)>,
    std::function<rt::Task<SignalBlock>(BlockContext, CalibrationBlock)>,
    std::function<rt::Task<rt::Done>(BlockContext, SignalBlock)>);

}  // namespace acsd::modules
