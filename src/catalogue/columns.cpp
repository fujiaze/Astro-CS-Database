// Parsing of the public catalogue columns into rows.
#include <cmath>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "catalogue/catalogue_internal.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"

namespace acsd::catalogue {
namespace {

Result<std::vector<double>> read_f64_array(Context& context, const ArrayRef& array, U64 count,
                                            const char* what, std::string& reason) {
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    reason = std::string("the ") + what + " array cannot be read";
    return model::fail<std::vector<double>>(view.code, reason, "catalogue.columns",
                                            "the columns cannot be parsed");
  }
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<std::vector<U64>> read_u64_array(Context& context, const ArrayRef& array, U64 count,
                                         const char* what, std::string& reason) {
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    reason = std::string("the ") + what + " array cannot be read";
    return model::fail<std::vector<U64>>(view.code, reason, "catalogue.columns",
                                         "the columns cannot be parsed");
  }
  return model::ok_result(model::decode_u64(val(view).bytes));
}

}  // namespace

U64 spectral_coefficient_count(const CatalogueColumns& columns) {
  if (!columns.xp_coefficients) return 0;
  if (columns.xp_coefficients->shape.size() != 2) return 0;
  return columns.xp_coefficients->shape[1];
}

Result<std::vector<StarRow>> parse_columns(Context& context, const CatalogueColumns& columns,
                                            std::string& reason) {
  if (!columns.source_ids || !columns.astrometry || !columns.brightness_mag) {
    reason = "the catalogue columns lack a mandatory array";
    return model::fail<std::vector<StarRow>>(Code::invalid_request, reason, "catalogue.columns",
                                              "the columns cannot be parsed");
  }
  if (columns.source_ids->shape.size() != 1) {
    reason = "the source identifier array must be one dimensional";
    return model::fail<std::vector<StarRow>>(Code::invalid_request, reason, "catalogue.columns",
                                              "the columns cannot be parsed");
  }
  const U64 count = columns.source_ids->shape[0];
  if (columns.astrometry->shape.size() != 2 || columns.astrometry->shape[0] != count ||
      columns.astrometry->shape[1] != 8) {
    reason = "the astrometry array must have the shape [N,8]";
    return model::fail<std::vector<StarRow>>(Code::invalid_model, reason, "catalogue.columns",
                                              "the columns cannot be parsed");
  }
  if (columns.brightness_mag->shape.size() != 1 || columns.brightness_mag->shape[0] != count) {
    reason = "the brightness array does not match the source count";
    return model::fail<std::vector<StarRow>>(Code::invalid_model, reason, "catalogue.columns",
                                              "the columns cannot be parsed");
  }

  Result<std::vector<U64>> source_ids =
      read_u64_array(context, columns.source_ids, count, "source identifier", reason);
  if (!ok(source_ids)) {
    return model::fail<std::vector<StarRow>>(source_ids.code, reason, "catalogue.columns",
                                              "the columns cannot be parsed");
  }
  Result<std::vector<double>> astrometry =
      read_f64_array(context, columns.astrometry, count * 8, "astrometry", reason);
  if (!ok(astrometry)) {
    return model::fail<std::vector<StarRow>>(astrometry.code, reason, "catalogue.columns",
                                              "the columns cannot be parsed");
  }
  Result<std::vector<double>> brightness =
      read_f64_array(context, columns.brightness_mag, count, "brightness", reason);
  if (!ok(brightness)) {
    return model::fail<std::vector<StarRow>>(brightness.code, reason, "catalogue.columns",
                                              "the columns cannot be parsed");
  }

  std::vector<StarRow> rows;
  rows.reserve(count);
  for (U64 i = 0; i < count; ++i) {
    StarRow row;
    row.source_id = val(source_ids)[i];
    const double* values = &val(astrometry)[static_cast<std::size_t>(i) * 8];
    row.ra_rad = values[0];
    row.dec_rad = values[1];
    row.epoch_year = values[2];
    row.pmra_cosdec_rad_year = values[3];
    row.pmdec_rad_year = values[4];
    row.parallax_rad = values[5];
    row.rv_m_s = values[6];
    row.rv_present = values[7] != 0.0;
    row.brightness_mag = val(brightness)[i];
    for (std::size_t column = 0; column < 8; ++column) {
      const double value = values[column];
      if (!std::isfinite(value)) {
        reason = "the catalogue contains a non finite value";
        return model::fail<std::vector<StarRow>>(Code::invalid_model, reason, "catalogue.columns",
                                                  "the columns cannot be parsed");
      }
    }
    if (!row.rv_present) row.rv_m_s = 0.0;
    if (row.dec_rad < -1.5707963267948966 || row.dec_rad > 1.5707963267948966) {
      reason = "the declination is out of range";
      return model::fail<std::vector<StarRow>>(Code::invalid_model, reason, "catalogue.columns",
                                                "the columns cannot be parsed");
    }
    row.unit = unit_from_ra_dec(row.ra_rad, row.dec_rad);
    rows.push_back(row);
  }
  return model::ok_result(std::move(rows));
}

}  // namespace acsd::catalogue
