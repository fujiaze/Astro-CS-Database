// Self test of the operator base: the composition must declare its children
// and must apply in the order outer(inner(values)).
//
// The composition used to fail always: the recipe of the composed action did
// not declare the children, while make_action requires the recipe and the
// supplied dependencies to match one for one.
#include <array>
#include <cstdio>
#include <span>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "resources/cpu_resources.hpp"
#include "storage/local_storage.hpp"

namespace {

int failures = 0;

void check(const char* name, bool condition, const char* detail = "") {
  if (condition) {
    std::printf("ok     %s\n", name);
  } else {
    std::printf("FAIL   %s %s\n", name, detail);
    ++failures;
  }
}

class NeverCancelled final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

}  // namespace

int main() {
  using namespace acsd;
  NeverCancelled cancellation;
  Limits limits;
  limits.bytes = 1u << 20;
  limits.work = 1000000;
  limits.threads = 1;
  resources::CpuResources resources(limits);
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};
  Unit unit;

  const std::vector<double> diagonal_values{2.0, 3.0};
  auto diagonal_bytes = model::encode_f64(diagonal_values);
  auto copied = storage.copy(context, DType::f64, diagonal_bytes,
                              std::span<const U64>(std::array<U64, 1>{2}), unit, Id("diag-1"));
  if (!ok(copied)) {
    std::printf("FAIL   the diagonal array cannot be stored\n");
    return 1;
  }

  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::diagonal;
  recipe.algorithm = "selftest_diagonal_v1";
  recipe.version = 1;
  recipe.arrays = {"diag-1"};
  std::vector<ArrayRef> arrays{val(copied)};
  auto action = make_action(context, recipe, std::span<const Ref<LinearAction>>{}, arrays);
  if (!ok(action)) {
    std::printf("FAIL   the diagonal action cannot be built\n");
    return 1;
  }

  auto composed = compose(context, val(action), val(action));
  check("compose succeeds", ok(composed));
  if (ok(composed)) {
    auto serialised = val(composed)->serialize_recipe();
    check("compose declares its children", ok(serialised) && val(serialised).children.size() == 2);
  } else {
    return 1;
  }

  const std::vector<double> input{1.0, 1.0};
  auto input_bytes = model::encode_f64(input);
  auto stored_input = storage.copy(context, DType::f64, input_bytes,
                                   std::span<const U64>(std::array<U64, 1>{2}), unit, Id("in-1"));
  if (!ok(stored_input)) {
    std::printf("FAIL   the input array cannot be stored\n");
    return 1;
  }
  auto applied = apply(context, val(composed), val(stored_input));
  check("the composed action applies", ok(applied));
  if (!ok(applied)) return 1;
  auto view = storage.read(context, val(applied), BlockRequest{0, 2});
  if (!ok(view)) {
    std::printf("FAIL   the result cannot be read back\n");
    return 1;
  }
  const auto output = model::decode_f64(val(view).bytes);
  check("compose(diag(2,3), diag(2,3)) on (1,1) is (4,9)",
        output.size() == 2 && output[0] == 4.0 && output[1] == 9.0);

  std::printf("%s: %d failure(s)\n", failures == 0 ? "operators self test PASS" :
                                          "operators self test FAIL", failures);
  return failures == 0 ? 0 : 1;
}
