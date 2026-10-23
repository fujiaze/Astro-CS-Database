// M5 fixed driver for response.integrate.
//
// Positive: a two-function piecewise-constant basis over three segments and a
// flat unit throughput; both output columns must be finite and have the
// coefficient_count length.  Negative: a coefficient payload inconsistent with
// coefficient_count x segments x (degree+1) is refused with invalid_model.
#include "../lib/fixtures.hpp"

#include <acsd/modules/response.hpp>

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::rt;

Ref<Response> make_response(const rt::InvocationContext& ctx, const Ref<Index>& index,
                            const Unit& unit, std::vector<ArrayRef>* keep) {
  auto response = std::make_shared<Response>();
  auto wavelength = vpf::make_array<double>(ctx, DType::f64, {450, 475, 525, 575, 650},
                                            {5}, index, unit);
  auto throughput = vpf::make_array<double>(ctx, DType::f64, {1, 1, 1, 1, 1}, {5}, index,
                                            vpf::dimensionless());
  if (wavelength.code != Code::ok || throughput.code != Code::ok) return {};
  keep->push_back(wavelength.value->array);
  keep->push_back(throughput.value->array);
  response->wavelength_nm = wavelength.value->array;
  response->throughput = throughput.value->array;
  response->convention = Response::Convention::photon_throughput;
  response->support_nm = Interval{400.0, 700.0};
  response->identity = vpf::identity_of("response:flat", "acsd.Response");
  response->assessment.complete = true;
  return response;
}

Ref<SpectralBasis> make_basis(const rt::InvocationContext& ctx, const Ref<Index>& index,
                              const Unit& unit, std::vector<ArrayRef>* keep,
                              std::vector<double> coefficients = {1, 1, 1, 1, 1, 1}) {
  auto basis = std::make_shared<SpectralBasis>();
  auto knots = vpf::make_array<double>(ctx, DType::f64, {400, 500, 600, 700}, {4}, index, unit);
  auto values = vpf::make_array<double>(ctx, DType::f64, coefficients,
                                        {static_cast<U64>(coefficients.size())}, index,
                                        vpf::dimensionless());
  if (knots.code != Code::ok || values.code != Code::ok) return {};
  keep->push_back(knots.value->array);
  keep->push_back(values.value->array);
  basis->knots_nm = knots.value->array;
  basis->basis_coefficients = values.value->array;
  basis->degree = 0;
  basis->coefficient_count = 2;
  basis->support_nm = Interval{400.0, 700.0};
  basis->identity = vpf::identity_of("basis:two", "acsd.SpectralBasis");
  basis->assessment.complete = true;
  return basis;
}

}  // namespace

int main() {
  vrt::Recorder recorder("response.integrate",
                         "validation/process/response.integrate/integrate.cpp");
  auto started = vpf::start({{"response.integrate", &acsd::modules::register_integrate_response}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;

  const auto index = vpf::make_index(8, "spectrum");
  const Unit unit = vpf::unit_of("nm");
  std::vector<ArrayRef> keep;
  Ref<Response> response = make_response(chain.command, index, unit, &keep);
  Ref<SpectralBasis> basis = make_basis(chain.command, index, unit, &keep);
  recorder.check("fixture.response", response != nullptr, "true",
                 response ? "true" : "false");
  recorder.check("fixture.basis", basis != nullptr, "true", basis ? "true" : "false");
  if (!response || !basis) return recorder.finish();

  auto input = std::make_shared<modules::IntegrateResponseInput>();
  input->response = response;
  input->basis = basis;
  input->request = vpf::budget(1e-12, 1e-12, 64);
  auto plan = modules::plan_integrate_response_op(chain.command, input,
                                                  PlanOptions{3, 1, Placement::memory});
  recorder.expect_code("integrate.plan", "ok", plan);
  if (plan.code == Code::ok) {
    recorder.value("integrate.plan.elements", plan.value->elements);
    recorder.value("integrate.plan.persistent_host_bytes", plan.value->persistent.host_bytes);
    recorder.value("integrate.plan.progress_floor_host_bytes",
                   plan.value->progress_floor.host_bytes);
    recorder.value("integrate.plan.work_upper", plan.value->regions.front().stages.front().work_upper);
  }
  auto prediction = vpf::invoke<modules::IntegrateResponseInput, Ref<SpectralPrediction>>(
      chain, "response.integrate", "integrate_response", input);
  recorder.expect_code("integrate.run", "ok", prediction);
  if (prediction.code == Code::ok && prediction.value) {
    const SpectralPrediction& result = **prediction.value;
    recorder.check("integrate.basis_integrals", result.basis_integrals != nullptr, "true",
                   result.basis_integrals ? "true" : "false");
    const std::vector<double> integrals =
        vpf::read_f64(chain.command, result.basis_integrals, 2);
    const std::vector<double> errors =
        vpf::read_f64(chain.command, result.absolute_errors, 2);
    bool finite = integrals.size() == 2 && errors.size() == 2;
    for (double value : integrals) finite = finite && std::isfinite(value);
    for (double value : errors) finite = finite && std::isfinite(value);
    recorder.check("integrate.values_finite", finite, "2 finite columns",
                   std::to_string(integrals.size()) + "/" + std::to_string(errors.size()));
    if (integrals.size() == 2) {
      recorder.value("integrate.integral_0", integrals[0]);
      recorder.value("integrate.integral_1", integrals[1]);
    }
  }

  // negative: coefficient payload inconsistent with coefficient_count.
  std::vector<ArrayRef> bad_keep;
  Ref<SpectralBasis> bad_basis =
      make_basis(chain.command, index, unit, &bad_keep, {1, 2, 3});
  if (bad_basis) {
    auto bad_input = std::make_shared<modules::IntegrateResponseInput>();
    bad_input->response = response;
    bad_input->basis = bad_basis;
    bad_input->request = vpf::budget();
    auto bad = vpf::invoke<modules::IntegrateResponseInput, Ref<SpectralPrediction>>(
        chain, "response.integrate", "integrate_response", bad_input);
    recorder.expect_code("negative.coefficient_payload", "invalid_model", bad);
  }

  return recorder.finish();
}
