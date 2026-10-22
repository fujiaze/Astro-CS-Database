// Shared helpers of the independent observation validation.
//
// The harness only uses the public contract in include/acsd/contracts.hpp and
// brings its own in-memory Storage, Resources and Cancellation so that no
// implementation detail of src/ is reused on the oracle side.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd_validation {

using acsd::ArrayRef;
using acsd::Code;
using acsd::Context;
using acsd::DType;
using acsd::Id;
using acsd::Index;
using acsd::Result;
using acsd::U32;
using acsd::U64;
using acsd::Unit;

extern int g_failures;
extern int g_checks;

void check(bool condition, const std::string& what);
void check_close(double value, double expected, double tolerance, const std::string& what);

class MemoryStorage final : public acsd::Storage {
 public:
  Result<ArrayRef> copy(acsd::Context& context, DType dtype, std::span<const std::byte> bytes,
                         std::span<const U64> shape, const Unit& unit, Id index) override;
  Result<acsd::ArrayView> read(acsd::Context& context, ArrayRef array,
                               acsd::BlockRequest range) const override;
  Result<ArrayRef> open(acsd::Context& context, std::string root, const acsd::Array& description,
                         std::span<const acsd::BlockDescriptor> blocks) override;

 private:
  U64 counter_ = 0;

  struct StoredBytes {
    DType dtype = DType::f64;
    std::vector<U64> shape;
    Unit unit;
    std::shared_ptr<std::vector<std::byte>> bytes;
  };
  mutable std::map<Id, std::shared_ptr<StoredBytes>> entries_;
};

class CountingResources final : public acsd::Resources {
 public:
  explicit CountingResources(U64 work_budget) : work_budget_(work_budget) {}
  Result<acsd::Reservation> reserve(U64 bytes, U32 threads) override;
  Result<U64> consume_work(U64 units) override;
  acsd::Usage usage() const override;

 private:
  U64 work_budget_;
  U64 work_used_ = 0;
};

class NeverCancelled final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

std::vector<std::byte> to_bytes_f64(const std::vector<double>& values);
std::vector<std::byte> to_bytes_u16(const std::vector<std::uint16_t>& values);
std::vector<std::byte> to_bytes_u64(const std::vector<U64>& values);
std::vector<double> decode_f64(const acsd::ArrayView& view);
std::vector<std::uint16_t> decode_u16(const acsd::ArrayView& view);
std::vector<U64> decode_u64(const acsd::ArrayView& view);

Unit adu_unit(const std::string& identity);
Unit dimensionless();
std::shared_ptr<const Index> make_index(U64 size, const std::string& namespace_id);
std::shared_ptr<acsd::FootprintSet> make_footprints(std::shared_ptr<const Index> index, U64 width,
                                                 U64 height);
ArrayRef make_f64_array(Context& context, const std::vector<double>& values, const Unit& unit,
                        const Id& index_id);
ArrayRef make_u16_array(Context& context, const std::vector<std::uint16_t>& values,
                        const Id& index_id);
ArrayRef make_u64_array(Context& context, const std::vector<U64>& values, const Id& index_id);
Result<acsd::Ref<acsd::LinearAction>> make_csr(
    Context& context, U64 rows, U64 cols, const std::vector<U64>& offsets,
    const std::vector<U64>& indices, const std::vector<double>& values,
    std::shared_ptr<const Index> domain, std::shared_ptr<const Index> codomain, const Unit& unit);

int run_calibration_cases();
int run_regroup_cases();
int run_psf_cases();
int run_measurement_cases();

}  // namespace acsd_validation
