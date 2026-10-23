#pragma once
// Internal helpers shared by the observation module (calibration and
// regrouping).  The module only exchanges data through the declared contract
// types in include/acsd/contracts.hpp; these helpers are implementation
// details of src/observation and are not part of the public surface.
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd::observation {

// ---------------------------------------------------------------------------
// N1 data quality bits.  The first four disqualify a sample by default; a
// repaired sample additionally needs a G/influence before it may be used.
// ---------------------------------------------------------------------------
inline constexpr std::uint16_t kDqBad = 1u;
inline constexpr std::uint16_t kDqSaturated = 2u;
inline constexpr std::uint16_t kDqNonlinear = 4u;
inline constexpr std::uint16_t kDqMissing = 8u;
inline constexpr std::uint16_t kDqRepaired = 16u;
inline constexpr std::uint16_t kDqDisqualifying =
    static_cast<std::uint16_t>(kDqBad | kDqSaturated | kDqNonlinear | kDqMissing);

// True when the sample may enter a fit.  A repaired sample needs an influence
// that the caller must supply separately; `has_influence` records that.
bool dq_usable(std::uint16_t dq, bool has_influence);

// ---------------------------------------------------------------------------
// Array helpers.  The storage contract is the only I/O path; these wrappers
// keep the dtype checks in one place.
// ---------------------------------------------------------------------------
U64 element_count(const std::vector<U64>& shape);

Result<std::vector<double>> read_f64_values(Context& context, ArrayRef array);
Result<std::vector<std::uint16_t>> read_u16_values(Context& context, ArrayRef array);
Result<std::vector<U64>> read_u64_values(Context& context, ArrayRef array);

Result<ArrayRef> write_f64_values(Context& context, const std::vector<double>& values,
                                  std::span<const U64> shape, const Unit& unit,
                                  const Id& index_id);
Result<ArrayRef> write_u16_values(Context& context, const std::vector<std::uint16_t>& values,
                                  std::span<const U64> shape, const Unit& unit,
                                  const Id& index_id);

// Checks that the array element count matches the expectation.
bool array_has_count(const Array& array, U64 count);

// The identity of a native detector frame, used to name the calibrated unit.
std::string detector_identity(const Index& index);

// Builds a stable content hash for an object payload.
std::string payload_hash(const std::string& schema, const std::string& discriminator,
                         const std::vector<std::string>& parts);

// Records the calibration Jacobian of x = (d - b - t*dark)/flat in the
// assessment so that a consumer can rebuild the error budget.
void note_calibration_jacobian(Assessment& assessment);

// Builds a diagonal action through the declared factory.  The recipe carries
// the array identity so that the action can be serialised and reopened.
Result<Ref<LinearAction>> make_diagonal_action(Context& context,
                                                const std::vector<double>& weights,
                                                Ref<Index> index, const Unit& in_unit,
                                                const Unit& out_unit);


}  // namespace acsd::observation
