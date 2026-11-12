#pragma once
// The model packaging shared by build_drizzle and the streaming drizzle_grid.
#include "acsd/contracts.hpp"
#include "geometry/drizzle_core.hpp"

namespace acsd {

struct DrizzleGeometrySource {
  Id geometry_id;
  Id primitives_id;
  Ref<Overlaps> overlaps;  // null for the streaming pixel grid
  bool recipe_identity_proven = false;
  bool overlaps_certified = false;
  bool geometry_termination_met = true;
};

Result<DrizzleResult> package_drizzle(Context& context, const geometry::DrizzleLeafOutput& leaf_output,
                                      Ref<NativeObservation> observation,
                                      const DrizzleRequest& request, U64 source_count,
                                      const DrizzleGeometrySource& source);

}  // namespace acsd
