// M5 fixed driver for catalogue.access.
//
// Positive: a five-star shard is indexed offline, the provider is built through
// the typed process, a cone query and an identifier lookup are executed.  The
// query is checked against the frozen star list by source id.  Negative: a
// page size of zero is refused by validate/invalid_request.
#include "../lib/fixtures.hpp"

#include <acsd/modules/catalogue.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace acsd;
using namespace acsd::rt;

SkyICRS direction(double ra, double dec) {
  SkyICRS sky;
  sky.nx = std::cos(dec) * std::cos(ra);
  sky.ny = std::cos(dec) * std::sin(ra);
  sky.nz = std::sin(dec);
  return sky;
}

struct Columns {
  Ref<CatalogueColumns> columns;
  std::vector<U64> ids;
  double epoch = 2016.0;
};

Columns make_columns(const rt::InvocationContext& ctx, const Ref<Index>& index) {
  Columns out;
  const std::vector<U64> ids{101, 102, 103, 104, 105};
  std::vector<double> astrometry;
  std::vector<double> brightness;
  const double ra0 = 0.10;
  const double dec0 = 0.05;
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const double ra = ra0 + 0.0003 * static_cast<double>(i);
    const double dec = dec0 + 0.0002 * static_cast<double>(i);
    astrometry.insert(astrometry.end(),
                      {ra, dec, out.epoch, 0.0, 0.0, 0.0, 0.0, 0.0});
    brightness.push_back(12.0 + 0.5 * static_cast<double>(i));
  }
  auto id_array = vpf::make_array<U64>(ctx, DType::u64, ids,
                                       {static_cast<U64>(ids.size())}, index,
                                       vpf::dimensionless());
  auto astrometry_array = vpf::make_array<double>(
      ctx, DType::f64, astrometry, {static_cast<U64>(ids.size()), 8}, index,
      vpf::unit_of("rad", 1));
  auto brightness_array = vpf::make_array<double>(
      ctx, DType::f64, brightness, {static_cast<U64>(ids.size())}, index,
      vpf::dimensionless());
  if (id_array.code != Code::ok || astrometry_array.code != Code::ok ||
      brightness_array.code != Code::ok)
    return out;
  auto columns = std::make_shared<CatalogueColumns>();
  columns->source_ids = id_array.value->array;
  columns->astrometry = astrometry_array.value->array;
  columns->brightness_mag = brightness_array.value->array;
  columns->identity = vpf::identity_of("columns:shard", "acsd.CatalogueColumns");
  columns->assessment.complete = true;
  out.columns = columns;
  out.ids = ids;
  return out;
}

}  // namespace

int main() {
  vrt::Recorder recorder("catalogue.access",
                         "validation/process/catalogue.access/catalogue.cpp");
  auto started = vpf::start({{"catalogue.access", &acsd::modules::register_catalogue_access}});
  for (const auto& [name, code] : started.steps)
    recorder.check("start." + name, code == Code::ok, "ok", vrt::code_name(code));
  if (started.chain.code != Code::ok || !started.chain.value) return recorder.finish();
  const vpf::Chain& chain = *started.chain.value;

  const auto index = vpf::make_index(5, "catalogue:shard");
  const Columns columns = make_columns(chain.command, index);
  recorder.check("fixture.columns", columns.columns != nullptr, "true",
                 columns.columns ? "true" : "false");
  if (!columns.columns) return recorder.finish();

  auto build_input = std::make_shared<modules::BuildCatalogueIndexInput>();
  build_input->request.columns = columns.columns;
  build_input->request.sky_order = 3;
  build_input->request.max_stars_per_cell = 8;
  build_input->request.neighbors_per_anchor = 3;
  build_input->request.quads_per_anchor = 4;
  build_input->request.quad_baseline_rad = Interval{1e-4, 1e-2};
  build_input->request.release = "validation-release";
  build_input->request.calibration = "validation-calibration";
  auto build_plan = modules::plan_build_catalogue_index_op(
      chain.command, build_input, PlanOptions{5, 1, Placement::memory});
  recorder.expect_code("build.plan", "ok", build_plan);
  if (build_plan.code == Code::ok) {
    recorder.value("build.plan.elements", build_plan.value->elements);
    recorder.value("build.plan.persistent_host_bytes", build_plan.value->persistent.host_bytes);
    recorder.value("build.plan.progress_floor_host_bytes",
                   build_plan.value->progress_floor.host_bytes);
  }
  auto built = vpf::invoke<modules::BuildCatalogueIndexInput, Ref<CatalogueIndex>>(
      chain, "catalogue.access", "build_catalogue_index", build_input);
  recorder.expect_code("build.run", "ok", built);
  if (built.code != Code::ok || !built.value) return recorder.finish();
  recorder.value("build.star_count", (*built.value)->metadata.star_count);

  auto provider_input = std::make_shared<modules::MakeCatalogueProviderInput>();
  provider_input->index = *built.value;
  auto provider = vpf::invoke<modules::MakeCatalogueProviderInput, Ref<CatalogueProvider>>(
      chain, "catalogue.access", "make_catalogue_provider", provider_input);
  recorder.expect_code("provider.run", "ok", provider);
  if (provider.code != Code::ok || !provider.value) return recorder.finish();

  auto query_input = std::make_shared<modules::CatalogueQueryInput>();
  query_input->provider = *provider.value;
  query_input->request.region.center = direction(0.10, 0.05);
  query_input->request.region.radius_rad = 0.01;
  query_input->request.epoch_julian_year = columns.epoch;
  query_input->request.page_size = 8;
  auto query_plan = modules::plan_catalogue_query_op(
      chain.command, query_input, PlanOptions{8, 1, Placement::memory});
  recorder.expect_code("query.plan", "ok", query_plan);
  if (query_plan.code == Code::ok) {
    recorder.value("query.plan.elements", query_plan.value->elements);
    recorder.value("query.plan.persistent_host_bytes", query_plan.value->persistent.host_bytes);
    recorder.value("query.plan.progress_floor_host_bytes",
                   query_plan.value->progress_floor.host_bytes);
  }
  auto page = vpf::invoke<modules::CatalogueQueryInput, CataloguePage>(
      chain, "catalogue.access", "catalogue.query", query_input);
  recorder.expect_code("query.run", "ok", page);
  if (page.code == Code::ok) {
    recorder.value("query.returned", static_cast<U64>(page.value->stars.size()));
    bool known_ids = true;
    for (const CatalogueStar& star : page.value->stars)
      known_ids = known_ids &&
                  std::find(columns.ids.begin(), columns.ids.end(), star.source_id) !=
                      columns.ids.end();
    recorder.check("query.known_ids", known_ids, "subset of the shard",
                   std::to_string(page.value->stars.size()) + " returned");
  }

  auto lookup_input = std::make_shared<modules::CatalogueLookupInput>();
  lookup_input->provider = *provider.value;
  lookup_input->source_ids = std::make_shared<const std::vector<U64>>(
      std::vector<U64>{101, 103, 105});
  auto found = vpf::invoke<modules::CatalogueLookupInput, std::vector<CatalogueStar>>(
      chain, "catalogue.access", "catalogue.lookup", lookup_input);
  recorder.expect_code("lookup.run", "ok", found);
  if (found.code == Code::ok) {
    bool ordered = found.value->size() == 3 && (*found.value)[0].source_id == 101 &&
                   (*found.value)[1].source_id == 103 && (*found.value)[2].source_id == 105;
    recorder.check("lookup.order", ordered, "[101,103,105]",
                   found.value->size() == 3
                       ? std::to_string((*found.value)[0].source_id) + "," +
                             std::to_string((*found.value)[1].source_id) + "," +
                             std::to_string((*found.value)[2].source_id)
                       : std::to_string(found.value->size()));
  }

  auto quad_input = std::make_shared<modules::CatalogueQueryQuadsInput>();
  quad_input->provider = *provider.value;
  quad_input->request.descriptor = {Interval{-0.1, 0.1}, Interval{-0.1, 0.1},
                                    Interval{-0.1, 0.1}, Interval{-0.1, 0.1}};
  quad_input->request.baseline_rad = Interval{0.0, 0.01};
  quad_input->request.page_size = 8;
  auto quads = vpf::invoke<modules::CatalogueQueryQuadsInput, QuadPage>(
      chain, "catalogue.access", "catalogue.query_quads", quad_input);
  recorder.expect_code("quads.run", "ok", quads);

  // negative: page size zero is not a valid catalogue page.
  auto bad_query = std::make_shared<modules::CatalogueQueryInput>();
  bad_query->provider = *provider.value;
  bad_query->request.region.center = direction(0.10, 0.05);
  bad_query->request.region.radius_rad = 0.01;
  bad_query->request.epoch_julian_year = columns.epoch;
  bad_query->request.page_size = 0;
  auto bad = vpf::invoke<modules::CatalogueQueryInput, CataloguePage>(
      chain, "catalogue.access", "catalogue.query", bad_query);
  recorder.expect_code("negative.page_size_zero", "invalid_request", bad);

  return recorder.finish();
}
