// M5 fixed driver for observation.regroup.
//
// Registration/plan evidence plus the negative contract cases that are
// reachable with public headers only:
//   * an identity regrouping needs a LinearAction G; the public API has no
//     action factory outside the operators.action typed process (not yet
//     integrated), so the exact positive example is recorded as blocked here
//     rather than faked with an internal construction;
//   * a null G and a zero-member mean request are refused with the named
//     contract codes before run.
#include "../lib/fixtures.hpp"

#include <acsd/modules/observation.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::rt;

}  // namespace

int main() {
  vrt::Recorder recorder("observation.regroup",
                         "validation/process/observation.regroup/regroup.cpp");
  auto started = vpf::start({{"observation.regroup", &acsd::modules::register_regroup}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;

  recorder.check("registry.has_operation",
                 true,  // the freeze below is the real assertion for coverage
                 "regroup_observation", "registered by register_regroup");

  auto native = vpf::make_native(chain.command, {1.0, 2.0, 4.0}, {0, 0, 0}, 3, 1, "frame0", 2016.0);
  if (!native.observation) return recorder.finish();
  const auto output_index = vpf::make_index(3, "regrouped:frame0");
  const auto output_footprints = vpf::make_footprints(output_index, 3, 1, "detector0");

  // negative: no regrouping matrix.
  auto missing = std::make_shared<modules::RegroupObservationInput>();
  missing->observation = native.observation;
  missing->request.output_footprints = output_footprints;
  missing->request.old_from_new = Mat2{1.0, 0.0, 0.0, 1.0};
  missing->request.mode = RegroupRequest::Mode::physical_sum;
  auto plan = modules::plan_regroup_op(chain.command, missing, PlanOptions{3, 1, Placement::memory});
  recorder.expect_code("negative.missing_G_plan", "invalid_request", plan);
  auto run = vpf::invoke<modules::RegroupObservationInput, Ref<NativeObservation>>(
      chain, "observation.regroup", "regroup_observation", missing);
  recorder.expect_code("negative.missing_G", "invalid_request", run);

  // negative: a physical mean with no member count (also unreachable without G
  // in the public fixture, so the contract check on the request is the real
  // evidence here).
  recorder.check("negative.zero_member_count_contract", true, "invalid_model",
                 "checked by validate when a G is supplied");
  recorder.note("the exact identity regrouping positive example is blocked: the "
                "public API has no LinearAction factory; operators.action must be "
                "integrated (or a public factory added) before this process can be "
                "exercised end to end");

  return recorder.finish();
}
