#include "twalib/graph/factor_graph.hpp"
#include "twalib/problems/sudoku.hpp"

#include <cassert>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace {

using twalib::Factor_graph;
using twalib::Sudoku;

auto expect_graph_size(
    const Factor_graph& graph,
    std::size_t outer_side) -> void {
  assert(graph.num_variables() == outer_side * outer_side * outer_side);
  assert(graph.num_factors() == 4 * outer_side * outer_side);
  assert(graph.num_edges() == 4 * outer_side * outer_side * outer_side);
  assert(graph.num_enabled_factors() == graph.num_factors());
  assert(graph.num_enabled_edges() == graph.num_edges());
}

auto test_size_checks_for_4x4() -> void {
  Factor_graph graph{};
  const Sudoku::Givens givens{
      {0, 0},
      {5, 1},
      {10, 2},
      {15, 3}};

  const auto variables = Sudoku::add_to_factor_graph(graph, 2, givens);

  assert(variables.size() == 16);
  for (const auto& options : variables) {
    assert(options.size() == 4);
  }
  expect_graph_size(graph, 4);
}

auto test_size_checks_for_9x9() -> void {
  Factor_graph graph{};
  const Sudoku::Givens givens{
      {0, 4},
      {10, 2},
      {80, 8}};

  const auto variables = Sudoku::add_to_factor_graph(graph, 3, givens);

  assert(variables.size() == 81);
  for (const auto& options : variables) {
    assert(options.size() == 9);
  }
  expect_graph_size(graph, 9);
}

auto test_extract_state_reports_thresholded_values() -> void {
  Factor_graph graph{};
  const Sudoku::Givens givens{
      {0, 2},
      {3, 1}};

  const auto variables = Sudoku::add_to_factor_graph(graph, 2, givens);
  const auto initial_state = Sudoku::extract_state(graph, variables);

  assert(initial_state.size() == 16);
  assert(initial_state[0] == 2);
  assert(initial_state[3] == 1);
  assert(initial_state[1] == -1);
}

auto test_invalid_inputs_are_explicit() -> void {
  {
    Factor_graph graph{};
    bool threw = false;
    try {
      [[maybe_unused]] const auto variables = Sudoku::add_to_factor_graph(graph, 0, {});
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  {
    Factor_graph graph{};
    bool threw = false;
    try {
      [[maybe_unused]] const auto variables = Sudoku::add_to_factor_graph(graph, 2, {{16, 0}});
    } catch (const std::out_of_range&) {
      threw = true;
    }
    assert(threw);
  }

  {
    Factor_graph graph{};
    bool threw = false;
    try {
      [[maybe_unused]] const auto variables = Sudoku::add_to_factor_graph(graph, 2, {{0, 4}});
    } catch (const std::out_of_range&) {
      threw = true;
    }
    assert(threw);
  }
}

auto test_4x4_puzzle_solves() -> void {
  Factor_graph graph{1.0, 1e-5, 0};
  const Sudoku::Givens givens{
      {0, 0},  {2, 2},
      {5, 2},  {7, 0},
      {8, 1},  {10, 3},
      {13, 3}, {15, 1}};
  const std::vector<int> solution{
      0, 1, 2, 3,
      3, 2, 1, 0,
      1, 0, 3, 2,
      2, 3, 0, 1};

  const auto variables = Sudoku::add_to_factor_graph(graph, 2, givens);

  assert(graph.iterate_until_converged(200));
  assert(Sudoku::extract_state(graph, variables) == solution);
}

auto swift_16x16_givens() -> Sudoku::Givens {
  return {
      {62, 5},   {130, 6}, {91, 8},   {2, 4},    {114, 0}, {222, 2}, {139, 7},  {39, 5},
      {102, 2},  {60, 0},  {188, 10}, {148, 2},  {42, 15}, {224, 13}, {28, 1},  {119, 6},
      {85, 14},  {135, 4}, {170, 8},  {158, 7},  {146, 5}, {58, 7},  {181, 0},  {36, 7},
      {180, 15}, {33, 10}, {107, 9},  {253, 10}, {125, 2}, {141, 8}, {252, 6}, {178, 9},
      {97, 4},   {216, 6}, {74, 12},  {128, 0},  {213, 8}, {152, 9}, {75, 15},  {183, 8},
      {199, 2},  {77, 14}, {240, 9},  {116, 4},  {113, 1}, {194, 1}, {173, 0}, {31, 13},
      {248, 12}, {112, 12}, {164, 12}, {101, 15}, {67, 5},  {88, 0},  {43, 1},  {246, 3},
      {238, 0},  {7, 3},   {232, 11}, {196, 6},  {103, 13}, {138, 10}, {195, 14}, {153, 0},
      {6, 12},   {72, 3},  {1, 5},    {212, 0},  {117, 3}, {166, 11}, {250, 0}, {82, 10},
      {193, 0},  {120, 14}, {197, 9}, {254, 13}, {227, 7}, {109, 11}, {3, 9},   {23, 0},
      {15, 14},  {154, 14}, {61, 15}, {17, 11},  {59, 14}, {219, 3}, {136, 13}, {5, 11},
      {142, 15}, {89, 6},  {127, 8},  {9, 13},   {56, 2},  {44, 3},  {143, 9}, {249, 15},
      {167, 9},  {211, 10}};
}

auto swift_16x16_solution() -> std::vector<int> {
  return {
      15, 5, 4, 9, 1, 11, 12, 3, 10, 13, 6, 0, 2, 7, 8, 14,
      8, 11, 7, 2, 14, 6, 15, 0, 5, 3, 4, 12, 1, 9, 10, 13,
      14, 10, 13, 0, 7, 2, 9, 5, 8, 11, 15, 1, 3, 6, 4, 12,
      1, 3, 12, 6, 13, 4, 8, 10, 2, 9, 7, 14, 0, 15, 5, 11,
      2, 9, 8, 5, 11, 1, 0, 7, 3, 4, 12, 15, 13, 14, 6, 10,
      7, 13, 10, 11, 9, 14, 5, 12, 0, 6, 2, 8, 15, 1, 3, 4,
      6, 4, 14, 3, 8, 15, 2, 13, 1, 10, 5, 9, 7, 11, 12, 0,
      12, 1, 0, 15, 4, 3, 10, 6, 14, 7, 11, 13, 5, 2, 9, 8,
      0, 2, 6, 12, 3, 5, 14, 4, 13, 1, 10, 7, 11, 8, 15, 9,
      4, 15, 5, 8, 2, 10, 6, 1, 9, 0, 14, 11, 12, 13, 7, 3,
      10, 7, 3, 1, 12, 13, 11, 9, 15, 2, 8, 5, 4, 0, 14, 6,
      11, 14, 9, 13, 15, 0, 7, 8, 4, 12, 3, 6, 10, 5, 1, 2,
      3, 0, 1, 14, 6, 9, 4, 2, 7, 5, 13, 10, 8, 12, 11, 15,
      5, 12, 15, 10, 0, 8, 13, 11, 6, 14, 1, 3, 9, 4, 2, 7,
      13, 6, 2, 7, 10, 12, 1, 15, 11, 8, 9, 4, 14, 3, 0, 5,
      9, 8, 11, 4, 5, 7, 3, 14, 12, 15, 0, 2, 6, 10, 13, 1};
}

auto test_swift_16x16_fixture_size_and_givens() -> void {
  Factor_graph graph{};
  const Sudoku::Givens givens = swift_16x16_givens();
  const auto variables = Sudoku::add_to_factor_graph(graph, 4, givens);
  const auto initial_state = Sudoku::extract_state(graph, variables);
  const auto solution = swift_16x16_solution();

  assert(solution.size() == 256);
  assert(variables.size() == 256);
  expect_graph_size(graph, 16);
  for (const auto& [cell, value] : givens) {
    assert(initial_state[cell] == static_cast<int>(value));
    assert(solution[cell] == static_cast<int>(value));
  }
}

auto solve_swift_16x16() -> std::size_t {
  Factor_graph graph{1.0, 1e-5, 0};
  const auto variables = Sudoku::add_to_factor_graph(graph, 4, swift_16x16_givens());

  assert(graph.iterate_until_converged(2000));
  assert(Sudoku::extract_state(graph, variables) == swift_16x16_solution());
  return graph.iterations();
}

auto test_swift_16x16_solves() -> void {
  assert(solve_swift_16x16() > 0);
}

} // namespace

auto main() -> int {
  test_size_checks_for_4x4();
  test_size_checks_for_9x9();
  test_extract_state_reports_thresholded_values();
  test_invalid_inputs_are_explicit();
  test_4x4_puzzle_solves();
  test_swift_16x16_fixture_size_and_givens();
  test_swift_16x16_solves();
  return 0;
}
