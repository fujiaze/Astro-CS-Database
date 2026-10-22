// The public linear action entry points declared in include/acsd/contracts.hpp.
#include <algorithm>
#include <cmath>
#include <numeric>

#include "acsd/contracts.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using operators::ActionImpl;
using operators::CsrMatrix;

std::vector<U64> flatten_shape(std::span<const U64> shape) {
  return std::vector<U64>(shape.begin(), shape.end());
}

// The identity of a child action: the domain it maps onto carries the
// identity of the modelled domain, otherwise the identity of the action
// itself, otherwise a stable role name.
Id child_identity(const Ref<LinearAction>& child, const char* role) {
  if (!child) return Id(role);
  if (child->spaces.domain && !child->spaces.domain->identity.object_id.empty()) {
    return child->spaces.domain->identity.object_id;
  }
  if (!child->identity.object_id.empty()) return child->identity.object_id;
  return Id(role);
}

}  // namespace

Result<Ref<LinearAction>> make_action(Context& context, const ActionRecipe& recipe,
                                      std::span<const Ref<LinearAction>> children,
                                      std::span<const ArrayRef> arrays) {
  if (recipe.children.size() != children.size() || recipe.arrays.size() != arrays.size()) {
    return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                          "the recipe does not match the supplied dependencies",
                                          "make_action", "the action cannot be built");
  }
  if (recipe.kind != ActionRecipe::Kind::solve && recipe.solve) {
    return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                          "only a solve recipe may carry a solve specification",
                                          "make_action", "the action cannot be built");
  }
  if (recipe.kind == ActionRecipe::Kind::solve && !recipe.solve) {
    return model::fail<Ref<LinearAction>>(Code::invalid_model, "a solve recipe needs a solve spec",
                                          "make_action", "the action cannot be built");
  }

  auto action = std::make_shared<ActionImpl>(recipe, recipe.spaces,
                                             std::vector<Ref<LinearAction>>(children.begin(), children.end()),
                                             std::vector<ArrayRef>(arrays.begin(), arrays.end()));

  switch (recipe.kind) {
    case ActionRecipe::Kind::identity:
      if (!children.empty() || !arrays.empty()) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model, "identity takes no dependency",
                                              "make_action", "the action cannot be built");
      }
      break;
    case ActionRecipe::Kind::diagonal: {
      if (children.size() != 0 || arrays.size() != 1) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                              "diagonal takes one array and no children",
                                              "make_action", "the action cannot be built");
      }
      Result<std::vector<double>> values =
          operators::read_f64(context, arrays[0], 0, arrays[0]->shape.empty()
                                                            ? 0
                                                            : arrays[0]->shape[0]);
      if (!ok(values)) {
        return model::fail<Ref<LinearAction>>(values.code, "the diagonal cannot be read",
                                              "make_action", "the action cannot be built");
      }
      action->set_diagonal(val(values));
      action->set_sizes(static_cast<U64>(val(values).size()), static_cast<U64>(val(values).size()));
      break;
    }
    case ActionRecipe::Kind::csr:
    case ActionRecipe::Kind::leaf_merge: {
      if (children.size() != 0 || arrays.size() != 3) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                              "a sparse action takes offsets, indices and values",
                                              "make_action", "the action cannot be built");
      }
      // Offsets and indices are u64 arrays; read them through the raw byte view.
      auto read_u64_array = [&](const ArrayRef& array, U64 count) -> Result<std::vector<U64>> {
        Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
        if (!ok(view)) {
          return model::fail<std::vector<U64>>(view.code, "the array cannot be read",
                                               "make_action", "the action cannot be built");
        }
        return model::ok_result(model::decode_u64(val(view).bytes));
      };
      const U64 offset_count = arrays[0]->shape.empty() ? 0 : arrays[0]->shape[0];
      const U64 index_count = arrays[1]->shape.empty() ? 0 : arrays[1]->shape[0];
      Result<std::vector<U64>> offsets_u64 = read_u64_array(arrays[0], offset_count);
      if (!ok(offsets_u64)) return model::fail<Ref<LinearAction>>(offsets_u64.code, "the offsets cannot be read", "make_action", "the action cannot be built");
      Result<std::vector<U64>> indices_u64 = read_u64_array(arrays[1], index_count);
      if (!ok(indices_u64)) return model::fail<Ref<LinearAction>>(indices_u64.code, "the indices cannot be read", "make_action", "the action cannot be built");
      Result<std::vector<double>> values = operators::read_f64(context, arrays[2], 0, index_count);
      if (!ok(values)) return model::fail<Ref<LinearAction>>(values.code, "the values cannot be read", "make_action", "the action cannot be built");
      const U64 rows = offset_count > 0 ? offset_count - 1 : 0;
      U64 cols = 0;
      for (U64 index : val(indices_u64)) cols = std::max(cols, index + 1);
      operators::CsrMatrix matrix;
      matrix.rows = rows;
      matrix.cols = cols;
      matrix.offsets = val(offsets_u64);
      matrix.indices = val(indices_u64);
      matrix.values = val(values);
      action->set_csr(std::move(matrix));
      action->set_sizes(cols, rows);
      break;
    }
    case ActionRecipe::Kind::compose:
    case ActionRecipe::Kind::transpose:
      if (children.size() != (recipe.kind == ActionRecipe::Kind::compose ? 2u : 1u) ||
          !arrays.empty()) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                              "the recipe does not match the composition contract",
                                              "make_action", "the action cannot be built");
      }
      break;
    case ActionRecipe::Kind::sum: {
      if (children.empty() || arrays.size() != 1) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                              "sum takes children and one coefficient array",
                                              "make_action", "the action cannot be built");
      }
      Result<std::vector<double>> coefficients =
          operators::read_f64(context, arrays[0], 0, arrays[0]->shape.empty() ? 0 : arrays[0]->shape[0]);
      if (!ok(coefficients)) {
        return model::fail<Ref<LinearAction>>(coefficients.code, "the coefficients cannot be read",
                                              "make_action", "the action cannot be built");
      }
      if (val(coefficients).size() != children.size()) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                              "the coefficient count does not match the children",
                                              "make_action", "the action cannot be built");
      }
      break;
    }
    case ActionRecipe::Kind::factor_covariance:
      if (children.empty()) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model, "factor_covariance needs factors",
                                              "make_action", "the action cannot be built");
      }
      break;
    case ActionRecipe::Kind::solve:
      if (children.size() != 1) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model, "solve needs exactly one child",
                                              "make_action", "the action cannot be built");
      }
      break;
    case ActionRecipe::Kind::sky:
      if (children.size() != 3) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model, "sky needs E, K and T",
                                              "make_action", "the action cannot be built");
      }
      break;
    case ActionRecipe::Kind::selection: {
      if (children.size() != 0 || arrays.size() != 2) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                              "selection takes an index and a scale array",
                                              "make_action", "the action cannot be built");
      }
      const U64 count = arrays[0]->shape.empty() ? 0 : arrays[0]->shape[0];
      Result<ArrayView> index_view = context.storage.read(context, arrays[0], BlockRequest{0, count});
      if (!ok(index_view)) {
        return model::fail<Ref<LinearAction>>(index_view.code, "the selection indices cannot be read",
                                              "make_action", "the action cannot be built");
      }
      const std::vector<U64> indices = model::decode_u64(val(index_view).bytes);
      Result<std::vector<double>> scales = operators::read_f64(context, arrays[1], 0, count);
      if (!ok(scales)) {
        return model::fail<Ref<LinearAction>>(scales.code, "the selection scales cannot be read",
                                              "make_action", "the action cannot be built");
      }
      action->set_selection(indices, val(scales));
      action->set_sizes(count, count);
      break;
    }
  }

  return model::ok_result(std::shared_ptr<const LinearAction>(action));
}

Result<ArrayRef> apply(Context& context, Ref<LinearAction> action, ArrayRef values) {
  if (!action) {
    return model::fail<ArrayRef>(Code::invalid_request, "no action supplied", "apply",
                                  "the action cannot be applied");
  }
  return action->apply(context, values);
}

Result<ArrayRef> transpose_apply(Context& context, Ref<LinearAction> action, ArrayRef values) {
  if (!action) {
    return model::fail<ArrayRef>(Code::invalid_request, "no action supplied", "transpose_apply",
                                  "the action cannot be applied");
  }
  return action->transpose_apply(context, values);
}

Result<Ref<LinearAction>> compose(Context& context, Ref<LinearAction> outer, Ref<LinearAction> inner) {
  if (!outer || !inner) {
    return model::fail<Ref<LinearAction>>(Code::invalid_request, "compose needs two actions",
                                          "compose", "the composition cannot be built");
  }
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::compose;
  recipe.algorithm = "canonical_compose_v1";
  recipe.version = 1;
  Result<ActionRecipe> outer_recipe = outer->serialize_recipe();
  if (!ok(outer_recipe)) {
    return model::fail<Ref<LinearAction>>(outer_recipe.code, "the outer action cannot be described",
                                          "compose", "the composition cannot be built");
  }
  recipe.spaces = val(outer_recipe).spaces;
  // The recipe must declare its dependencies: make_action requires the child
  // list of the recipe to match the supplied children one for one.
  recipe.children = {child_identity(outer, "outer"), child_identity(inner, "inner")};
  const std::vector<Ref<LinearAction>> children{outer, inner};
  return make_action(context, recipe, children, std::span<const ArrayRef>{});
}

}  // namespace acsd
