// Independent probe of the covariance semantics (phot module).
//
// It checks the contract of the declaration: the noise root carries a factor
// F with C_root = F F^T, and the influence of the root maps the root's
// coordinate space onto the space of the observation.  With a diagonal root
// F = diag(sigma) the covariance of a unit vector must be sigma^2.
//
// It uses only the public declaration contract (include/acsd/contracts.hpp)
// and its own storage, resource and cancellation providers.
//
// Build and run:
//   g++ -std=c++20 -O2 -I include validation/phot/covariance_probe.cpp \
//       build/dev/libuncertainty.a build/dev/liboperators.a \
//       build/dev/libnumerics.a build/dev/libmodel.a -o /tmp/covariance_probe
//   /tmp/covariance_probe
// Expected output:
//   make_action code 0
//   make_covariance code 0
//   covariance_apply code 0
//   the applied covariance of the unit vector: 1.000000e-10 0.000000e+00
//   (the variance should be 1e-10)
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#include "acsd/contracts.hpp"
using namespace acsd;
class NoCancel final : public Cancellation { public: bool requested() const noexcept override { return false; } };
class Res final : public Resources {
 public:
  Result<Reservation> reserve(U64 bytes, U32 threads) override { Reservation r; r.bytes=bytes; r.threads=threads; Result<Reservation> o; o.code=Code::ok; o.value=r; return o; }
  Result<U64> consume_work(U64 units) override { Result<U64> o; o.code=Code::ok; o.value=units; return o; }
  Usage usage() const override { return Usage{}; }
};
class Sto final : public Storage {
 public:
  Result<ArrayRef> copy(Context&, DType dtype, std::span<const std::byte> bytes, std::span<const U64> shape, const Unit& unit, Id index) override {
    auto arr = std::make_shared<Array>();
    arr->dtype = dtype; arr->shape.assign(shape.begin(), shape.end()); arr->unit = unit; arr->index_id = index;
    arr->identity.object_id = "a" + std::to_string(counter_++);
    auto buf = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    { std::lock_guard<std::mutex> l(m_); buffers_[arr->identity.object_id] = buf; }
    Result<ArrayRef> o; o.code = Code::ok; o.value = std::shared_ptr<const Array>(arr); return o;
  }
  Result<ArrayView> read(Context&, ArrayRef array, BlockRequest range) const override {
    std::shared_ptr<std::vector<std::byte>> buf;
    { std::lock_guard<std::mutex> l(m_); auto it = buffers_.find(array->identity.object_id); if (it != buffers_.end()) buf = it->second; }
    const std::size_t item = 8;
    ArrayView v; v.dtype = array->dtype; v.shape = array->shape;
    v.bytes = std::span<const std::byte>(buf->data() + range.begin * item, range.count * item);
    Result<ArrayView> o; o.code = Code::ok; o.value = v; return o;
  }
  Result<ArrayRef> open(Context&, std::string, const Array&, std::span<const BlockDescriptor>) override { Result<ArrayRef> o; o.code=Code::unsupported; return o; }
 private:
  mutable std::mutex m_; std::map<std::string, std::shared_ptr<std::vector<std::byte>>> buffers_; U64 counter_=1;
};
int main() {
  Sto storage; Res resources; NoCancel cancel; Context ctx{resources, cancel, storage};
  // A diagonal covariance action with the variance 1e-10 over two coordinates.
  // The factor F of a root carries the standard deviation (C = F F^T), so the
  // diagonal action holds sqrt(variance).
  std::vector<double> values{1e-5, 1e-5};
  std::vector<std::byte> bytes(values.size() * 8);
  for (std::size_t i = 0; i < values.size(); ++i) { double v = values[i]; std::memcpy(bytes.data()+i*8, &v, 8); }
  std::vector<U64> shape{2};
  auto arr = *storage.copy(ctx, DType::f64, bytes, shape, Unit{"v",0,0,0,1.0,2}, "idx").value;
  // The diagonal action through make_action
  ActionRecipe recipe; recipe.kind = ActionRecipe::Kind::diagonal; recipe.algorithm = "x"; recipe.version = 1;
  recipe.arrays = {"values"};
  std::vector<ArrayRef> arrays;
  arrays.push_back(std::shared_ptr<const Array>(arr));
  std::vector<Ref<LinearAction>> no_children;
  auto action = make_action(ctx, recipe, no_children, arrays);
  std::printf("make_action code %d\n", (int)action.code);
  auto root = std::make_shared<NoiseRoot>();
  root->shared_id = SharedId{"v","e","1","c"}; root->unit = Unit{"v",0,0,0,1.0,2};
  Index idx; idx.size = 2; idx.identity.object_id = "idx"; root->coordinates = std::make_shared<const Index>(idx);
  root->factor = *action.value;
  auto model = std::make_shared<NoiseModel>();
  model->variance_unit = Unit{"v",0,0,0,1.0,2}; model->condition = Condition{"v","d"};
  model->shared_factors.push_back(root);
  // The identity influence.
  ActionRecipe irec; irec.kind = ActionRecipe::Kind::identity; irec.algorithm = "x"; irec.version = 1;
  auto identity = make_action(ctx, irec, no_children, std::vector<ArrayRef>{});
  model->influence.push_back(*identity.value);
  auto cov = make_covariance(ctx, model);
  std::printf("make_covariance code %d\n", (int)cov.code);
  if (!cov.value) return 1;
  // Apply the covariance to the unit vector (1,0).
  std::vector<double> unit{1.0, 0.0};
  // The input is the unit vector (1,0) in the root's coordinate space.
  std::vector<double> unit_vector{1.0, 0.0};
  std::vector<std::byte> unit_bytes(unit_vector.size() * 8);
  for (std::size_t i = 0; i < unit_vector.size(); ++i) { double v = unit_vector[i]; std::memcpy(unit_bytes.data()+i*8, &v, 8); }
  auto input = *storage.copy(ctx, DType::f64, unit_bytes, shape, Unit{"1",0,0,0,1.0,0}, "unit").value;
  auto applied = covariance_apply(ctx, *cov.value, input);
  std::printf("covariance_apply code %d\n", (int)applied.code);
  if (applied.value) {
    std::vector<U64> total{2};
    auto view = storage.read(ctx, *applied.value, BlockRequest{0, 2});
    double out[2] = {0.0, 0.0};
    if (view.value) { std::memcpy(out, view.value->bytes.data(), 16); }
    std::printf("the applied covariance of the unit vector: %.6e %.6e (the variance should be 1e-10)\n",
                out[0], out[1]);
  }
  return 0;
}
