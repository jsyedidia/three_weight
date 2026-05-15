#include "twalib/problems/compact_sudoku.hpp"

#include "twalib/graph/weighted_value.hpp"
#include "twalib/minimizers/one_hot.hpp"

#include <stdexcept>
#include <vector>

namespace twalib {
namespace {

auto outer_side_for(std::size_t inner_side) -> std::size_t {
  if (inner_side == 0) {
    throw std::invalid_argument("Compact_sudoku inner_side must be positive");
  }

  return inner_side * inner_side;
}

auto cell_index(std::size_t row, std::size_t column, std::size_t outer_side) -> std::size_t {
  return row * outer_side + column;
}

auto square_index(
    std::size_t row,
    std::size_t column,
    std::size_t inner_side) -> std::size_t {
  return column / inner_side + inner_side * (row / inner_side);
}

auto square_base_row(std::size_t square, std::size_t inner_side) -> std::size_t {
  return (square / inner_side) * inner_side;
}

auto square_base_column(std::size_t square, std::size_t inner_side) -> std::size_t {
  return (square % inner_side) * inner_side;
}

auto validate_givens(const Compact_sudoku::Givens& givens, std::size_t outer_side) -> void {
  const std::size_t num_cells = outer_side * outer_side;
  for (const auto& [cell, value] : givens) {
    if (cell >= num_cells) {
      throw std::out_of_range("Compact_sudoku given cell is out of range");
    }
    if (value >= outer_side) {
      throw std::out_of_range("Compact_sudoku given value is out of range");
    }
  }
}

auto has_given_value(
    const Compact_sudoku::Givens& givens,
    std::size_t cell,
    std::size_t value) -> bool {
  const auto given = givens.find(cell);
  return given != givens.end() && given->second == value;
}

auto value_found(
    const Compact_sudoku::Givens& givens,
    std::size_t inner_side,
    std::size_t outer_side,
    std::size_t row,
    std::size_t column,
    std::size_t value) -> bool {
  for (std::size_t candidate_column = 0; candidate_column < outer_side; ++candidate_column) {
    if (has_given_value(givens, cell_index(row, candidate_column, outer_side), value)) {
      return true;
    }
  }

  for (std::size_t candidate_row = 0; candidate_row < outer_side; ++candidate_row) {
    if (has_given_value(givens, cell_index(candidate_row, column, outer_side), value)) {
      return true;
    }
  }

  const std::size_t square = square_index(row, column, inner_side);
  const std::size_t base_row = square_base_row(square, inner_side);
  const std::size_t base_column = square_base_column(square, inner_side);
  for (std::size_t row_offset = 0; row_offset < inner_side; ++row_offset) {
    for (std::size_t column_offset = 0; column_offset < inner_side; ++column_offset) {
      if (has_given_value(
              givens,
              cell_index(base_row + row_offset, base_column + column_offset, outer_side),
              value)) {
        return true;
      }
    }
  }

  return false;
}

auto valid_candidate(Variable_node variable) -> bool {
  return variable.is_valid();
}

auto add_one_hot_if_nonempty(
    Factor_graph& graph,
    const std::vector<Variable_node>& candidates) -> void {
  if (!candidates.empty()) {
    [[maybe_unused]] const auto factor = create_one_hot_factor(graph, candidates);
  }
}

} // namespace

auto Compact_sudoku::add_to_factor_graph(
    Factor_graph& graph,
    std::size_t inner_side,
    const Givens& givens) -> Variables {
  const std::size_t outer_side = outer_side_for(inner_side);
  validate_givens(givens, outer_side);

  Variables variables;
  variables.inner_side = inner_side;
  variables.outer_side = outer_side;
  variables.givens = givens;
  variables.candidates.resize(
      outer_side * outer_side,
      std::vector<Variable_node>(outer_side, Variable_node{}));

  for (std::size_t row = 0; row < outer_side; ++row) {
    for (std::size_t column = 0; column < outer_side; ++column) {
      const std::size_t cell = cell_index(row, column, outer_side);
      if (givens.contains(cell)) {
        continue;
      }

      std::vector<Variable_node> cell_candidates;
      cell_candidates.reserve(outer_side);
      for (std::size_t value = 0; value < outer_side; ++value) {
        if (value_found(givens, inner_side, outer_side, row, column, value)) {
          continue;
        }

        const Variable_node variable = graph.create_variable(0.0, Message_weight::standard);
        variables.candidates[cell][value] = variable;
        cell_candidates.push_back(variable);
      }

      if (cell_candidates.empty()) {
        throw std::invalid_argument("Compact_sudoku cell has no surviving candidates");
      }
      [[maybe_unused]] const auto cell_factor = create_one_hot_factor(graph, cell_candidates);
    }
  }

  for (std::size_t index = 0; index < outer_side; ++index) {
    for (std::size_t value = 0; value < outer_side; ++value) {
      std::vector<Variable_node> row_value_variables;
      std::vector<Variable_node> column_value_variables;
      std::vector<Variable_node> square_value_variables;
      row_value_variables.reserve(outer_side);
      column_value_variables.reserve(outer_side);
      square_value_variables.reserve(outer_side);

      for (std::size_t offset = 0; offset < outer_side; ++offset) {
        const Variable_node row_variable =
            variables.candidates[cell_index(index, offset, outer_side)][value];
        if (valid_candidate(row_variable)) {
          row_value_variables.push_back(row_variable);
        }

        const Variable_node column_variable =
            variables.candidates[cell_index(offset, index, outer_side)][value];
        if (valid_candidate(column_variable)) {
          column_value_variables.push_back(column_variable);
        }

        const std::size_t square_row = index / inner_side;
        const std::size_t square_column = index % inner_side;
        const std::size_t row_in_square = offset / inner_side;
        const std::size_t column_in_square = offset % inner_side;
        const std::size_t row = square_row * inner_side + row_in_square;
        const std::size_t column = square_column * inner_side + column_in_square;
        const Variable_node square_variable =
            variables.candidates[cell_index(row, column, outer_side)][value];
        if (valid_candidate(square_variable)) {
          square_value_variables.push_back(square_variable);
        }
      }

      add_one_hot_if_nonempty(graph, row_value_variables);
      add_one_hot_if_nonempty(graph, column_value_variables);
      add_one_hot_if_nonempty(graph, square_value_variables);
    }
  }

  return variables;
}

auto Compact_sudoku::extract_state(
    const Factor_graph& graph,
    const Variables& variables) -> std::vector<int> {
  std::vector<int> state;
  state.reserve(variables.candidates.size());

  for (std::size_t cell = 0; cell < variables.candidates.size(); ++cell) {
    const auto given = variables.givens.find(cell);
    if (given != variables.givens.end()) {
      state.push_back(static_cast<int>(given->second));
      continue;
    }

    int selected_value = -1;
    for (std::size_t value = 0; value < variables.candidates[cell].size(); ++value) {
      const Variable_node variable = variables.candidates[cell][value];
      if (valid_candidate(variable) && graph.value(variable) > 0.99) {
        selected_value = static_cast<int>(value);
      }
    }
    state.push_back(selected_value);
  }

  return state;
}

} // namespace twalib
