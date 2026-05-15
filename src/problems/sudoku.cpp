#include "twalib/problems/sudoku.hpp"

#include "twalib/graph/weighted_value.hpp"
#include "twalib/minimizers/known_value.hpp"
#include "twalib/minimizers/one_hot.hpp"

#include <stdexcept>
#include <vector>

namespace twalib {
namespace {

auto outer_side_for(std::size_t inner_side) -> std::size_t {
  if (inner_side == 0) {
    throw std::invalid_argument("Sudoku inner_side must be positive");
  }

  return inner_side * inner_side;
}

auto cell_index(std::size_t row, std::size_t column, std::size_t outer_side) -> std::size_t {
  return row * outer_side + column;
}

auto validate_givens(const Sudoku::Givens& givens, std::size_t outer_side) -> void {
  const std::size_t num_cells = outer_side * outer_side;
  for (const auto& [cell, value] : givens) {
    if (cell >= num_cells) {
      throw std::out_of_range("Sudoku given cell is out of range");
    }
    if (value >= outer_side) {
      throw std::out_of_range("Sudoku given value is out of range");
    }
  }
}

} // namespace

auto Sudoku::add_to_factor_graph(
    Factor_graph& graph,
    std::size_t inner_side,
    const Givens& givens) -> Variables {
  const std::size_t outer_side = outer_side_for(inner_side);
  validate_givens(givens, outer_side);

  Variables variables;
  variables.reserve(outer_side * outer_side);

  for (std::size_t cell = 0; cell < outer_side * outer_side; ++cell) {
    const auto given = givens.find(cell);
    const bool has_given = given != givens.end();
    const std::size_t given_value = has_given ? given->second : outer_side;
    const Message_weight initial_weight = has_given ? Message_weight::infinite : Message_weight::standard;

    std::vector<Variable_node> options;
    options.reserve(outer_side);
    for (std::size_t value = 0; value < outer_side; ++value) {
      options.push_back(graph.create_variable(value == given_value ? 1.0 : 0.0, initial_weight));
    }

    variables.push_back(options);
    [[maybe_unused]] const auto cell_factor = create_one_hot_factor(graph, variables.back());
    if (has_given) {
      [[maybe_unused]] const auto given_factor =
          create_known_value_factor(graph, variables.back()[given_value], 1.0);
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
        row_value_variables.push_back(variables[cell_index(index, offset, outer_side)][value]);
        column_value_variables.push_back(variables[cell_index(offset, index, outer_side)][value]);

        const std::size_t square_row = index / inner_side;
        const std::size_t square_column = index % inner_side;
        const std::size_t row_in_square = offset / inner_side;
        const std::size_t column_in_square = offset % inner_side;
        const std::size_t row = square_row * inner_side + row_in_square;
        const std::size_t column = square_column * inner_side + column_in_square;
        square_value_variables.push_back(variables[cell_index(row, column, outer_side)][value]);
      }

      [[maybe_unused]] const auto row_factor = create_one_hot_factor(graph, row_value_variables);
      [[maybe_unused]] const auto column_factor = create_one_hot_factor(graph, column_value_variables);
      [[maybe_unused]] const auto square_factor = create_one_hot_factor(graph, square_value_variables);
    }
  }

  return variables;
}

auto Sudoku::extract_state(
    const Factor_graph& graph,
    const Variables& variables) -> std::vector<int> {
  std::vector<int> state;
  state.reserve(variables.size());

  for (const auto& options : variables) {
    int selected_value = -1;
    for (std::size_t value = 0; value < options.size(); ++value) {
      if (graph.value(options[value]) > 0.99) {
        selected_value = static_cast<int>(value);
      }
    }
    state.push_back(selected_value);
  }

  return state;
}

} // namespace twalib
