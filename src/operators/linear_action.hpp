#pragma once
// Linear actions: the single implementation shared by the geometry, noise,
// sky, coadd and export modules.  Every entry point (CLI, contract driver,
// in-process API) uses this one implementation through the public
// `LinearAction` interface declared in include/acsd/contracts.hpp.
#include <memory>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"

namespace acsd::operators {

struct CsrMatrix {
  U64 rows = 0;
  U64 cols = 0;
  std::vector<U64> offsets;    // rows + 1 entries
  std::vector<U64> indices;    // ascending within a row, duplicates merged
  std::vector<double> values;
  U64 nonzeros() const { return indices.size(); }
};

// Reads a contiguous element range of an f64 array.
Result<std::vector<double>> read_f64(Context& context, ArrayRef array, U64 begin, U64 count);
Result<ArrayRef> write_f64(Context& context, const std::vector<double>& values,
                           std::span<const U64> shape, const Unit& unit, const Id& index_id);

// The concrete action object.  It is immutable after construction and owns
// its children.
class ActionImpl final : public LinearAction {
 public:
  ActionImpl() = default;
  ActionImpl(ActionRecipe recipe, DomainUnits spaces);
  ActionImpl(ActionRecipe recipe, DomainUnits spaces, std::vector<Ref<LinearAction>> children,
             std::vector<ArrayRef> arrays);

  Result<ArrayRef> apply(Context& context, ArrayRef values) const override;
  Result<ArrayRef> transpose_apply(Context& context, ArrayRef values) const override;
  Result<std::vector<Id>> dependency_blocks(BlockRequest range, bool transpose) const override;
  Result<ActionRecipe> serialize_recipe() const override;
  Result<std::vector<ExactInterval>> row_enclosure(Context& context, U64 row) const override;

  // Direct access used by the composition and solve helpers.
  const ActionRecipe& recipe() const { return recipe_; }
  const DomainUnits& spaces() const { return spaces_; }
  const std::vector<Ref<LinearAction>>& children() const { return children_; }
  const std::vector<ArrayRef>& arrays() const { return arrays_; }
  void set_recipe(ActionRecipe recipe) { recipe_ = std::move(recipe); }
  void set_spaces(DomainUnits spaces) { spaces_ = std::move(spaces); }
  // The CSR payload when the action is a csr kind (empty otherwise).
  const CsrMatrix& csr() const { return csr_; }
  void set_csr(CsrMatrix matrix) { csr_ = std::move(matrix); }
  // Diagonal payload for the diagonal kind.
  const std::vector<double>& diagonal() const { return diagonal_; }
  void set_diagonal(std::vector<double> values) { diagonal_ = std::move(values); }
  // Selection payload: indices and scale.
  const std::vector<U64>& selection_indices() const { return selection_indices_; }
  const std::vector<double>& selection_scale() const { return selection_scale_; }
  void set_selection(std::vector<U64> indices, std::vector<double> scale);

  // For a csr action the domain is [cols] and the codomain is [rows].
  U64 domain_size() const { return domain_size_; }
  U64 codomain_size() const { return codomain_size_; }
  void set_sizes(U64 domain, U64 codomain) {
    domain_size_ = domain;
    codomain_size_ = codomain;
  }

 private:
  ActionRecipe recipe_;
  DomainUnits spaces_;
  std::vector<Ref<LinearAction>> children_;
  std::vector<ArrayRef> arrays_;
  CsrMatrix csr_;
  std::vector<double> diagonal_;
  std::vector<U64> selection_indices_;
  std::vector<double> selection_scale_;
  U64 domain_size_ = 0;
  U64 codomain_size_ = 0;
};

// Builds a CSR action from raw offsets/indices/values arrays.
Result<Ref<LinearAction>> make_csr_action(Context& context, U64 rows, U64 cols,
                                           const std::vector<U64>& offsets,
                                           const std::vector<U64>& indices,
                                           const std::vector<double>& values,
                                           const Id& domain_index, const Id& codomain_index,
                                           const Unit& unit);

}  // namespace acsd::operators
