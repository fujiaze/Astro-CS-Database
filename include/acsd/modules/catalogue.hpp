#pragma once
// Typed catalogue module functions and the catalogue.access process binding.
#include <acsd/modules/generated/inputs.hpp>
#include <acsd/runtime/process.hpp>

namespace acsd::modules {

// catalogue.access: build_catalogue_index, make_catalogue_provider,
// catalogue.query, catalogue.query_quads, catalogue.lookup.
Result<rt::Done> register_catalogue_access(rt::RegistryBuilder& builder);
// Plan introspection for validation/process/catalogue.access.
Result<rt::InvocationPlan> plan_build_catalogue_index_op(rt::InvocationContext, Ref<BuildCatalogueIndexInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_make_catalogue_provider_op(rt::InvocationContext, Ref<MakeCatalogueProviderInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_catalogue_query_op(rt::InvocationContext, Ref<CatalogueQueryInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_catalogue_query_quads_op(rt::InvocationContext, Ref<CatalogueQueryQuadsInput>, rt::PlanOptions);
Result<rt::InvocationPlan> plan_catalogue_lookup_op(rt::InvocationContext, Ref<CatalogueLookupInput>, rt::PlanOptions);
}  // namespace acsd::modules
