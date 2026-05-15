#include "twalib/graph/factor_graph.hpp"
#include "twalib/problems/compact_sudoku.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using twalib::Compact_sudoku;
using twalib::Compact_sudoku_variables;
using twalib::Factor_graph;

struct Puzzle {
  std::size_t inner_side = 0;
  std::size_t outer_side = 0;
  Compact_sudoku::Givens givens;
};

auto square_root(std::size_t value) -> std::size_t {
  const auto root = static_cast<std::size_t>(std::sqrt(static_cast<double>(value)));
  if (root * root == value) {
    return root;
  }
  if ((root + 1) * (root + 1) == value) {
    return root + 1;
  }
  throw std::invalid_argument("side length must be a perfect square");
}

auto read_puzzle(const std::string& relative_path) -> Puzzle {
  const std::string path = std::string{TWALIB_SOURCE_DIR} + "/" + relative_path;
  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error("Could not open puzzle fixture: " + path);
  }

  std::vector<std::vector<std::string>> rows;
  std::string line;
  while (std::getline(input, line)) {
    std::istringstream line_input{line};
    std::vector<std::string> row;
    std::string token;
    while (line_input >> token) {
      row.push_back(token);
    }
    if (!row.empty()) {
      rows.push_back(row);
    }
  }

  Puzzle puzzle;
  puzzle.outer_side = rows.size();
  puzzle.inner_side = square_root(puzzle.outer_side);

  for (std::size_t row = 0; row < puzzle.outer_side; ++row) {
    assert(rows[row].size() == puzzle.outer_side);
    for (std::size_t column = 0; column < puzzle.outer_side; ++column) {
      if (rows[row][column] == ".") {
        continue;
      }
      const int value = std::stoi(rows[row][column]);
      assert(value >= 1);
      assert(value <= static_cast<int>(puzzle.outer_side));
      puzzle.givens.emplace(row * puzzle.outer_side + column, static_cast<std::size_t>(value - 1));
    }
  }

  return puzzle;
}

auto count_candidates(const Compact_sudoku_variables& variables) -> std::size_t {
  std::size_t count = 0;
  for (const auto& cell_candidates : variables.candidates) {
    for (const auto variable : cell_candidates) {
      if (variable.is_valid()) {
        ++count;
      }
    }
  }
  return count;
}

auto expect_compact_graph_size(
    const std::string& path,
    std::size_t expected_factors,
    std::size_t expected_variables,
    std::size_t expected_edges) -> void {
  const Puzzle puzzle = read_puzzle(path);
  Factor_graph graph{};
  const auto variables = Compact_sudoku::add_to_factor_graph(graph, puzzle.inner_side, puzzle.givens);

  assert(variables.inner_side == puzzle.inner_side);
  assert(variables.outer_side == puzzle.outer_side);
  assert(variables.candidates.size() == puzzle.outer_side * puzzle.outer_side);
  assert(count_candidates(variables) == expected_variables);
  assert(graph.num_factors() == expected_factors);
  assert(graph.num_variables() == expected_variables);
  assert(graph.num_edges() == expected_edges);
  assert(graph.num_enabled_factors() == expected_factors);
  assert(graph.num_enabled_edges() == expected_edges);
}

auto test_compact_graph_counts_match_reference_construction() -> void {
  expect_compact_graph_size("data/sudoku/example_2x2.txt", 32, 8, 32);
  expect_compact_graph_size("data/sudoku/example_3x3.txt", 204, 153, 612);
  expect_compact_graph_size("data/sudoku/example_4x4_medium.txt", 632, 784, 3136);
  expect_compact_graph_size("data/sudoku/example_4x4_hard.txt", 800, 1657, 6628);
  expect_compact_graph_size("data/sudoku/example_5x5.txt", 1356, 1860, 7440);
}

auto test_extract_state_includes_givens() -> void {
  Factor_graph graph{};
  const Compact_sudoku::Givens givens{
      {0, 0},
      {5, 1},
  };
  const auto variables = Compact_sudoku::add_to_factor_graph(graph, 2, givens);
  const auto state = Compact_sudoku::extract_state(graph, variables);

  assert(state.size() == 16);
  assert(state[0] == 0);
  assert(state[5] == 1);
}

auto test_invalid_inputs_are_explicit() -> void {
  {
    Factor_graph graph{};
    bool threw = false;
    try {
      [[maybe_unused]] const auto variables = Compact_sudoku::add_to_factor_graph(graph, 0, {});
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  {
    Factor_graph graph{};
    bool threw = false;
    try {
      [[maybe_unused]] const auto variables = Compact_sudoku::add_to_factor_graph(graph, 2, {{16, 0}});
    } catch (const std::out_of_range&) {
      threw = true;
    }
    assert(threw);
  }

  {
    Factor_graph graph{};
    bool threw = false;
    try {
      [[maybe_unused]] const auto variables = Compact_sudoku::add_to_factor_graph(graph, 2, {{0, 4}});
    } catch (const std::out_of_range&) {
      threw = true;
    }
    assert(threw);
  }
}

} // namespace

auto main() -> int {
  test_compact_graph_counts_match_reference_construction();
  test_extract_state_includes_givens();
  test_invalid_inputs_are_explicit();
  return 0;
}
