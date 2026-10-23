#include "operators/linear_action.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"

namespace acsd::operators {
namespace {

U64 element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    if (total > (~0ull) / extent) return 0;
    total *= extent;
  }
  return total;
}

}  // namespace

Result<std::vector<double>> read_f64(Context& context, ArrayRef array, U64 begin, U64 count) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_request, "no array supplied",
                                            "operators.read", "the values cannot be read");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is not f64",
                                            "operators.read", "the values cannot be read");
  }
  const U64 total = element_count(array->shape);
  if (begin > count || begin + count > total) {
    return model::fail<std::vector<double>>(Code::invalid_request, "the range is out of bounds",
                                            "operators.read", "the values cannot be read");
  }
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{begin, count});
  if (!ok(view)) return model::fail<std::vector<double>>(view.code, "the array cannot be read",
                                                     "operators.read", "the values cannot be read");
  return model::ok_result(model::decode_f64(val(view).bytes));
}

Result<ArrayRef> write_f64(Context& context, const std::vector<double>& values,
                           std::span<const U64> shape, const Unit& unit, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

ActionImpl::ActionImpl(ActionRecipe recipe, DomainUnits spaces)
    : recipe_(std::move(recipe)), spaces_(std::move(spaces)) {
  // The public member of the base class carries the same information: a
  // consumer that only sees a LinearAction must still be able to read the
  // domain and the codomain of the action.
  this->LinearAction::spaces = spaces_;
}

ActionImpl::ActionImpl(ActionRecipe recipe, DomainUnits spaces,
                       std::vector<Ref<LinearAction>> children, std::vector<ArrayRef> arrays)
    : recipe_(std::move(recipe)),
      spaces_(std::move(spaces)),
      children_(std::move(children)),
      arrays_(std::move(arrays)) {
  this->LinearAction::spaces = spaces_;
}

void ActionImpl::set_selection(std::vector<U64> indices, std::vector<double> scale) {
  selection_indices_ = std::move(indices);
  selection_scale_ = std::move(scale);
}

Result<ArrayRef> ActionImpl::apply(Context& context, ArrayRef values) const {
  const ActionRecipe::Kind kind = recipe_.kind;
  const Id& domain_index = spaces_.domain ? spaces_.domain->identity.object_id : Id();
  const Id& codomain_index = spaces_.codomain ? spaces_.codomain->identity.object_id : Id();

  if (kind == ActionRecipe::Kind::identity) {
    if (values->dtype != DType::f64) {
      return model::fail<ArrayRef>(Code::invalid_model, "identity expects f64 input",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<std::vector<double>> data = read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.apply", "the action cannot be applied");
    return write_f64(context, val(data), values->shape, values->unit, codomain_index);
  }

  if (kind == ActionRecipe::Kind::diagonal) {
    if (diagonal_.empty() || arrays_.empty()) {
      return model::fail<ArrayRef>(Code::invalid_model, "the diagonal payload is missing",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<std::vector<double>> data =
        read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.apply", "the action cannot be applied");
    Result<std::vector<double>> weights =
        read_f64(context, arrays_[0], 0, element_count(arrays_[0]->shape));
    if (!ok(weights)) return model::fail<ArrayRef>(weights.code, "the diagonal cannot be read",
                                               "operators.apply", "the action cannot be applied");
    if (val(weights).size() != val(data).size()) {
      return model::fail<ArrayRef>(Code::invalid_model, "the diagonal length does not match",
                                   "operators.apply", "the action cannot be applied");
    }
    std::vector<double> out(val(data).size());
    for (std::size_t i = 0; i < val(data).size(); ++i) out[i] = (val(weights))[i] * (val(data))[i];
    std::vector<U64> shape{static_cast<U64>(out.size())};
    return write_f64(context, out, shape, spaces_.output, codomain_index);
  }

  if (kind == ActionRecipe::Kind::csr || kind == ActionRecipe::Kind::leaf_merge) {
    if (csr_.offsets.size() != csr_.rows + 1) {
      return model::fail<ArrayRef>(Code::invalid_model, "the CSR offsets are inconsistent",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<std::vector<double>> data = read_f64(context, values, 0, csr_.cols);
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.apply", "the action cannot be applied");
    std::vector<double> out(csr_.rows, 0.0);
    for (U64 row = 0; row < csr_.rows; ++row) {
      double sum = 0.0;
      for (U64 k = csr_.offsets[row]; k < csr_.offsets[row + 1]; ++k) {
        sum += csr_.values[k] * (val(data))[csr_.indices[k]];
      }
      out[row] = sum;
    }
    std::vector<U64> shape{csr_.rows};
    return write_f64(context, out, shape, spaces_.output, codomain_index);
  }

  if (kind == ActionRecipe::Kind::compose) {
    if (children_.size() != 2) {
      return model::fail<ArrayRef>(Code::invalid_model, "compose needs two children",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<ArrayRef> inner = children_[1]->apply(context, values);
    if (!ok(inner)) return inner;
    Result<ArrayRef> outer = children_[0]->apply(context, val(inner));
    if (!ok(outer)) return outer;
    return outer;
  }

  if (kind == ActionRecipe::Kind::sum) {
    if (children_.empty() || arrays_.empty()) {
      return model::fail<ArrayRef>(Code::invalid_model, "sum needs children and coefficients",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<std::vector<double>> coefficients =
        read_f64(context, arrays_[0], 0, element_count(arrays_[0]->shape));
    if (!ok(coefficients)) {
      return model::fail<ArrayRef>(coefficients.code, "the coefficients cannot be read",
                                    "operators.apply", "the action cannot be applied");
    }
    if (val(coefficients).size() != children_.size()) {
      return model::fail<ArrayRef>(Code::invalid_model, "the coefficient count does not match",
                                   "operators.apply", "the action cannot be applied");
    }
    std::vector<double> accumulator;
    for (std::size_t k = 0; k < children_.size(); ++k) {
      Result<ArrayRef> partial = children_[k]->apply(context, values);
      if (!ok(partial)) return partial;
      Result<std::vector<double>> data =
          read_f64(context, val(partial), 0, element_count(val(partial)->shape));
      if (!ok(data)) {
        return model::fail<ArrayRef>(data.code, "the partial result cannot be read",
                                     "operators.apply", "the action cannot be applied");
      }
      if (accumulator.empty()) accumulator.assign(val(data).size(), 0.0);
      if (accumulator.size() != val(data).size()) {
        return model::fail<ArrayRef>(Code::invalid_model, "the children disagree on the codomain",
                                     "operators.apply", "the action cannot be applied");
      }
      for (std::size_t i = 0; i < accumulator.size(); ++i)
        accumulator[i] += (val(coefficients))[k] * (val(data))[i];
    }
    std::vector<U64> shape{static_cast<U64>(accumulator.size())};
    return write_f64(context, accumulator, shape, spaces_.output, codomain_index);
  }

  if (kind == ActionRecipe::Kind::transpose) {
    if (children_.size() != 1) {
      return model::fail<ArrayRef>(Code::invalid_model, "transpose needs one child",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<ArrayRef> partial = children_[0]->transpose_apply(context, values);
    if (!ok(partial)) return partial;
    return partial;
  }

  if (kind == ActionRecipe::Kind::factor_covariance) {
    // C = sum_k L_k F_k L_k^T, applied as v -> sum_k L_k (L_k^T v).
    if (children_.empty()) {
      return model::fail<ArrayRef>(Code::invalid_model, "factor_covariance needs children",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<std::vector<double>> data = read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.apply", "the action cannot be applied");
    std::vector<double> accumulator(val(data).size(), 0.0);
    for (const Ref<LinearAction>& child : children_) {
      Result<ArrayRef> projected = child->transpose_apply(context, values);
      if (!ok(projected)) return projected;
      Result<std::vector<double>> latent =
          read_f64(context, val(projected), 0, element_count(val(projected)->shape));
      if (!ok(latent)) {
        return model::fail<ArrayRef>(latent.code, "the latent block cannot be read",
                                     "operators.apply", "the action cannot be applied");
      }
      Result<ArrayRef> mapped = child->apply(context, val(projected));
      if (!ok(mapped)) return mapped;
      Result<std::vector<double>> contribution =
          read_f64(context, val(mapped), 0, element_count(val(mapped)->shape));
      if (!ok(contribution)) {
        return model::fail<ArrayRef>(contribution.code, "the contribution cannot be read",
                                     "operators.apply", "the action cannot be applied");
      }
      if (val(contribution).size() != accumulator.size()) {
        return model::fail<ArrayRef>(Code::invalid_model, "the factors disagree on the codomain",
                                     "operators.apply", "the action cannot be applied");
      }
      for (std::size_t i = 0; i < accumulator.size(); ++i) accumulator[i] += (val(contribution))[i];
    }
    std::vector<U64> shape{static_cast<U64>(accumulator.size())};
    return write_f64(context, accumulator, shape, spaces_.output, codomain_index);
  }

  if (kind == ActionRecipe::Kind::solve) {
    if (children_.size() != 1 || !recipe_.solve) {
      return model::fail<ArrayRef>(Code::invalid_model, "solve needs one child and a solve spec",
                                   "operators.apply", "the action cannot be applied");
    }
    return model::fail<ArrayRef>(Code::unsupported,
                                 "the solve action is applied through the declared solver",
                                 "operators.apply", "the action cannot be applied");
  }

  if (kind == ActionRecipe::Kind::sky) {
    // P = I - E K T
    if (children_.size() != 3) {
      return model::fail<ArrayRef>(Code::invalid_model, "sky needs E, K and T",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<std::vector<double>> data = read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.apply", "the action cannot be applied");
    Result<ArrayRef> tv = children_[2]->apply(context, values);   // T v
    if (!ok(tv)) return tv;
    Result<ArrayRef> kv = children_[1]->apply(context, val(tv));      // K T v
    if (!ok(kv)) return kv;
    Result<ArrayRef> ekv = children_[0]->apply(context, val(kv));     // E K T v
    if (!ok(ekv)) return ekv;
    Result<std::vector<double>> correction =
        read_f64(context, val(ekv), 0, element_count(val(ekv)->shape));
    if (!ok(correction)) {
      return model::fail<ArrayRef>(correction.code, "the correction cannot be read",
                                    "operators.apply", "the action cannot be applied");
    }
    if (val(correction).size() != val(data).size()) {
      return model::fail<ArrayRef>(Code::invalid_model, "the sky action has inconsistent domains",
                                   "operators.apply", "the action cannot be applied");
    }
    std::vector<double> out(val(data).size());
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = (val(data))[i] - (val(correction))[i];
    std::vector<U64> shape{static_cast<U64>(out.size())};
    return write_f64(context, out, shape, spaces_.output, codomain_index);
  }

  if (kind == ActionRecipe::Kind::selection) {
    if (selection_indices_.empty()) {
      return model::fail<ArrayRef>(Code::invalid_model, "the selection payload is missing",
                                   "operators.apply", "the action cannot be applied");
    }
    Result<std::vector<double>> data = read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.apply", "the action cannot be applied");
    std::vector<double> out(selection_indices_.size(), 0.0);
    for (std::size_t i = 0; i < selection_indices_.size(); ++i) {
      const U64 index = selection_indices_[i];
      if (index >= val(data).size()) {
        return model::fail<ArrayRef>(Code::invalid_model, "a selection index is out of range",
                                     "operators.apply", "the action cannot be applied");
      }
      const double scale = selection_scale_.empty() ? 1.0 : selection_scale_[i];
      out[i] = scale * (val(data))[index];
    }
    std::vector<U64> shape{static_cast<U64>(out.size())};
    return write_f64(context, out, shape, spaces_.output, codomain_index);
  }

  return model::fail<ArrayRef>(Code::unsupported, "unknown action kind", "operators.apply",
                               "the action cannot be applied");
}

Result<ArrayRef> ActionImpl::transpose_apply(Context& context, ArrayRef values) const {
  const ActionRecipe::Kind kind = recipe_.kind;
  const Id& codomain_index = spaces_.codomain ? spaces_.codomain->identity.object_id : Id();

  if (kind == ActionRecipe::Kind::identity) {
    Result<std::vector<double>> data = read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.transpose_apply",
                                            "the action cannot be applied");
    return write_f64(context, val(data), values->shape, values->unit, codomain_index);
  }
  if (kind == ActionRecipe::Kind::diagonal) {
    return apply(context, values);
  }
  if (kind == ActionRecipe::Kind::csr || kind == ActionRecipe::Kind::leaf_merge) {
    Result<std::vector<double>> data = read_f64(context, values, 0, csr_.rows);
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.transpose_apply",
                                            "the action cannot be applied");
    std::vector<double> out(csr_.cols, 0.0);
    for (U64 row = 0; row < csr_.rows; ++row) {
      const double value = (val(data))[row];
      for (U64 k = csr_.offsets[row]; k < csr_.offsets[row + 1]; ++k) {
        out[csr_.indices[k]] += csr_.values[k] * value;
      }
    }
    std::vector<U64> shape{csr_.cols};
    return write_f64(context, out, shape, spaces_.output, codomain_index);
  }
  if (kind == ActionRecipe::Kind::compose) {
    if (children_.size() != 2) {
      return model::fail<ArrayRef>(Code::invalid_model, "compose needs two children",
                                   "operators.transpose_apply", "the action cannot be applied");
    }
    Result<ArrayRef> outer = children_[1]->transpose_apply(context, values);
    if (!ok(outer)) return outer;
    Result<ArrayRef> inner = children_[0]->transpose_apply(context, val(outer));
    if (!ok(inner)) return inner;
    return inner;
  }
  if (kind == ActionRecipe::Kind::sum) {
    if (children_.empty() || arrays_.empty()) {
      return model::fail<ArrayRef>(Code::invalid_model, "sum needs children and coefficients",
                                   "operators.transpose_apply", "the action cannot be applied");
    }
    Result<std::vector<double>> coefficients =
        read_f64(context, arrays_[0], 0, element_count(arrays_[0]->shape));
    if (!ok(coefficients)) {
      return model::fail<ArrayRef>(coefficients.code, "the coefficients cannot be read",
                                    "operators.transpose_apply", "the action cannot be applied");
    }
    std::vector<double> accumulator;
    for (std::size_t k = 0; k < children_.size(); ++k) {
      Result<ArrayRef> partial = children_[k]->transpose_apply(context, values);
      if (!ok(partial)) return partial;
      Result<std::vector<double>> data =
          read_f64(context, val(partial), 0, element_count(val(partial)->shape));
      if (!ok(data)) {
        return model::fail<ArrayRef>(data.code, "the partial result cannot be read",
                                     "operators.transpose_apply", "the action cannot be applied");
      }
      if (accumulator.empty()) accumulator.assign(val(data).size(), 0.0);
      for (std::size_t i = 0; i < accumulator.size(); ++i)
        accumulator[i] += (val(coefficients))[k] * (val(data))[i];
    }
    std::vector<U64> shape{static_cast<U64>(accumulator.size())};
    return write_f64(context, accumulator, shape, spaces_.output, codomain_index);
  }
  if (kind == ActionRecipe::Kind::transpose) {
    if (children_.size() != 1) {
      return model::fail<ArrayRef>(Code::invalid_model, "transpose needs one child",
                                   "operators.transpose_apply", "the action cannot be applied");
    }
    Result<ArrayRef> partial = children_[0]->apply(context, values);
    if (!ok(partial)) return partial;
    return partial;
  }
  if (kind == ActionRecipe::Kind::factor_covariance) {
    // The covariance is symmetric, so the transpose is the same map.
    return apply(context, values);
  }
  if (kind == ActionRecipe::Kind::sky) {
    // (I - E K T)^T = I - T^T K^T E^T
    if (children_.size() != 3) {
      return model::fail<ArrayRef>(Code::invalid_model, "sky needs E, K and T",
                                   "operators.transpose_apply", "the action cannot be applied");
    }
    Result<std::vector<double>> data = read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.transpose_apply",
                                            "the action cannot be applied");
    Result<ArrayRef> ev = children_[0]->transpose_apply(context, values);
    if (!ok(ev)) return ev;
    Result<ArrayRef> kt = children_[1]->transpose_apply(context, val(ev));
    if (!ok(kt)) return kt;
    Result<ArrayRef> tkt = children_[2]->transpose_apply(context, val(kt));
    if (!ok(tkt)) return tkt;
    Result<std::vector<double>> correction =
        read_f64(context, val(tkt), 0, element_count(val(tkt)->shape));
    if (!ok(correction)) {
      return model::fail<ArrayRef>(correction.code, "the correction cannot be read",
                                    "operators.transpose_apply", "the action cannot be applied");
    }
    if (val(correction).size() != val(data).size()) {
      return model::fail<ArrayRef>(Code::invalid_model, "the sky action has inconsistent domains",
                                   "operators.transpose_apply", "the action cannot be applied");
    }
    std::vector<double> out(val(data).size());
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = (val(data))[i] - (val(correction))[i];
    std::vector<U64> shape{static_cast<U64>(out.size())};
    return write_f64(context, out, shape, spaces_.output, codomain_index);
  }
  if (kind == ActionRecipe::Kind::selection) {
    // Scatter: y[index] += scale * x
    Result<std::vector<double>> data = read_f64(context, values, 0, element_count(values->shape));
    if (!ok(data)) return model::fail<ArrayRef>(data.code, "the input cannot be read",
                                            "operators.transpose_apply",
                                            "the action cannot be applied");
    const U64 target = domain_size_ > 0 ? domain_size_ : val(data).size();
    std::vector<double> out(target, 0.0);
    for (std::size_t i = 0; i < selection_indices_.size(); ++i) {
      const U64 index = selection_indices_[i];
      if (index >= out.size()) {
        return model::fail<ArrayRef>(Code::invalid_model, "a selection index is out of range",
                                     "operators.transpose_apply", "the action cannot be applied");
      }
      const double scale = selection_scale_.empty() ? 1.0 : selection_scale_[i];
      out[index] += scale * (val(data))[i];
    }
    std::vector<U64> shape{target};
    return write_f64(context, out, shape, spaces_.output, codomain_index);
  }
  return model::fail<ArrayRef>(Code::unsupported, "unknown action kind",
                               "operators.transpose_apply", "the action cannot be applied");
}

Result<std::vector<Id>> ActionImpl::dependency_blocks(BlockRequest range, bool transpose) const {
  std::vector<Id> ids;
  for (const ArrayRef& array : arrays_) ids.push_back(array->identity.object_id);
  for (const Ref<LinearAction>& child : children_) {
    Result<ActionRecipe> recipe = child->serialize_recipe();
    if (ok(recipe)) {
      for (const Id& id : val(recipe).arrays) ids.push_back(id);
      for (const Id& id : val(recipe).children) ids.push_back(id);
    }
  }
  (void)range;
  (void)transpose;
  return model::ok_result(ids);
}

Result<ActionRecipe> ActionImpl::serialize_recipe() const { return model::ok_result(recipe_); }

Result<std::vector<ExactInterval>> ActionImpl::row_enclosure(Context& context, U64 row) const {
  if (recipe_.kind != ActionRecipe::Kind::csr && recipe_.kind != ActionRecipe::Kind::leaf_merge) {
    return model::fail<std::vector<ExactInterval>>(Code::unsupported,
                                                   "row enclosure is only available for sparse actions",
                                                   "operators.row_enclosure",
                                                   "the row cannot be enclosed");
  }
  if (row >= csr_.rows) {
    return model::fail<std::vector<ExactInterval>>(Code::invalid_request, "row out of range",
                                                   "operators.row_enclosure",
                                                   "the row cannot be enclosed");
  }
  std::vector<ExactInterval> row_enclosure(csr_.cols, model::exact_point(0.0));
  for (U64 k = csr_.offsets[row]; k < csr_.offsets[row + 1]; ++k) {
    row_enclosure[csr_.indices[k]] = model::exact_point(csr_.values[k]);
  }
  (void)context;
  return model::ok_result(std::move(row_enclosure));
}

Result<Ref<LinearAction>> make_csr_action(Context& context, U64 rows, U64 cols,
                                           const std::vector<U64>& offsets,
                                           const std::vector<U64>& indices,
                                           const std::vector<double>& values,
                                           const Id& domain_index, const Id& codomain_index,
                                           const Unit& unit) {
  if (offsets.size() != rows + 1) {
    return model::fail<Ref<LinearAction>>(Code::invalid_model, "the CSR offsets are inconsistent",
                                          "operators.make_csr", "the action cannot be built");
  }
  if (indices.size() != values.size()) {
    return model::fail<Ref<LinearAction>>(Code::invalid_model, "the CSR values do not match",
                                          "operators.make_csr", "the action cannot be built");
  }
  for (U64 row = 0; row < rows; ++row) {
    if (offsets[row] > offsets[row + 1] || offsets[row + 1] > indices.size()) {
      return model::fail<Ref<LinearAction>>(Code::invalid_model, "the CSR offsets are inconsistent",
                                            "operators.make_csr", "the action cannot be built");
    }
    for (U64 k = offsets[row]; k + 1 < offsets[row + 1]; ++k) {
      if (indices[k] >= indices[k + 1]) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model,
                                              "the CSR column indices are not ascending",
                                              "operators.make_csr", "the action cannot be built");
      }
    }
    for (U64 k = offsets[row]; k < offsets[row + 1]; ++k) {
      if (indices[k] >= cols) {
        return model::fail<Ref<LinearAction>>(Code::invalid_model, "a CSR column index is out of range",
                                              "operators.make_csr", "the action cannot be built");
      }
    }
  }
  auto action = std::make_shared<ActionImpl>();
  CsrMatrix matrix;
  matrix.rows = rows;
  matrix.cols = cols;
  matrix.offsets = offsets;
  matrix.indices = indices;
  matrix.values = values;
  action->set_csr(std::move(matrix));
  action->set_sizes(cols, rows);
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::csr;
  recipe.algorithm = "canonical_csr_v1";
  recipe.version = 1;
  // The action always describes its own spaces: a consumer that only sees the
  // action (a covariance action, a drizzle recipe, an external implementation)
  // has to know the size of the domain and of the codomain.  An unnamed space
  // gets a synthetic identity that carries its size.
  DomainUnits spaces;
  {
    Index domain;
    domain.size = cols;
    domain.kind = Index::Kind::parameter;
    domain.identity.object_id = domain_index.empty() ? Id("csr-domain-" + std::to_string(cols))
                                                     : domain_index;
    domain.identity.content_hash = domain.identity.object_id;
    domain.identity.schema_id = "acsd.Index";
    spaces.domain = std::make_shared<const Index>(domain);
  }
  {
    Index codomain;
    codomain.size = rows;
    codomain.kind = Index::Kind::parameter;
    codomain.identity.object_id = codomain_index.empty() ? Id("csr-codomain-" + std::to_string(rows))
                                                         : codomain_index;
    codomain.identity.content_hash = codomain.identity.object_id;
    codomain.identity.schema_id = "acsd.Index";
    spaces.codomain = std::make_shared<const Index>(codomain);
  }
  spaces.input = unit;
  spaces.output = unit;
  action->set_recipe(recipe);
  action->set_spaces(spaces);
  (void)context;
  return model::ok_result(std::shared_ptr<const LinearAction>(action));
}

}  // namespace acsd::operators
