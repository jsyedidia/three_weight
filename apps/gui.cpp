#define GL_SILENCE_DEPRECATION

#include "twalib/graph/factor_graph.hpp"
#include "twalib/problems/compact_sudoku.hpp"
#include "twalib/problems/circle_packing.hpp"
#include "twalib/problems/sudoku.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using twalib::Circle;
using twalib::Circle_packing;
using twalib::Circle_packing_variables;
using twalib::Compact_sudoku;
using twalib::Compact_sudoku_variables;
using twalib::Coordinate_range;
using twalib::Factor_graph;
using twalib::Message_weight;
using twalib::Radius_count;
using twalib::Sudoku;
using twalib::Variable_node;

constexpr std::size_t max_drawn_connection_lines = 5000;
constexpr double visual_overlap_epsilon = 1e-8;

struct Puzzle {
  std::string name;
  std::size_t inner_side = 0;
  Sudoku::Givens givens;
  std::vector<int> solution;
};

enum class Build_mode {
  compact,
  full,
};

enum class Problem_domain {
  sudoku,
  circle_packing,
};

enum class Circle_build_mode {
  fast,
  full,
};

struct Circle_problem {
  std::string name;
  Coordinate_range horizontal_range;
  Coordinate_range vertical_range;
  std::vector<Radius_count> radii;
};

auto square_root(std::size_t value) -> std::size_t {
  for (std::size_t root = 1; root * root <= value; ++root) {
    if (root * root == value) {
      return root;
    }
  }
  throw std::invalid_argument("Sudoku side length must be a perfect square");
}

auto read_puzzle(std::string name, const std::string& path, std::vector<int> solution = {}) -> Puzzle {
  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error("Could not open Sudoku fixture: " + path);
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

  const std::size_t side = rows.size();
  Puzzle puzzle;
  puzzle.name = std::move(name);
  puzzle.inner_side = square_root(side);
  puzzle.solution = std::move(solution);

  for (std::size_t row = 0; row < side; ++row) {
    if (rows[row].size() != side) {
      throw std::invalid_argument("Sudoku fixture must be square: " + path);
    }
    for (std::size_t column = 0; column < side; ++column) {
      if (rows[row][column] == ".") {
        continue;
      }
      const int value = std::stoi(rows[row][column]);
      if (value < 1 || value > static_cast<int>(side)) {
        throw std::out_of_range("Sudoku fixture value out of range: " + path);
      }
      puzzle.givens.emplace(row * side + column, static_cast<std::size_t>(value - 1));
    }
  }

  return puzzle;
}

auto make_puzzles() -> std::vector<Puzzle> {
  return {
      read_puzzle(
          "2x2",
          "data/sudoku/example_2x2.txt",
          {
              0, 1, 2, 3,
              3, 2, 1, 0,
              1, 0, 3, 2,
              2, 3, 0, 1,
          }),
      read_puzzle(
          "3x3",
          "data/sudoku/example_3x3.txt",
          {
              4, 2, 3, 5, 6, 7, 8, 0, 1,
              5, 6, 1, 0, 8, 4, 2, 3, 7,
              0, 8, 7, 2, 3, 1, 4, 5, 6,
              7, 4, 8, 6, 5, 0, 3, 1, 2,
              3, 1, 5, 7, 4, 2, 6, 8, 0,
              6, 0, 2, 8, 1, 3, 7, 4, 5,
              8, 5, 0, 4, 2, 6, 1, 7, 3,
              1, 7, 6, 3, 0, 8, 5, 2, 4,
              2, 3, 4, 1, 7, 5, 0, 6, 8,
          }),
      read_puzzle(
          "4x4 medium",
          "data/sudoku/example_4x4_medium.txt",
          {
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
              9, 8, 11, 4, 5, 7, 3, 14, 12, 15, 0, 2, 6, 10, 13, 1,
          }),
      read_puzzle(
          "4x4 hard",
          "data/sudoku/example_4x4_hard.txt",
          {
              5, 0, 4, 6, 3, 2, 9, 15, 8, 7, 1, 11, 10, 12, 13, 14,
              3, 7, 2, 11, 6, 4, 13, 12, 15, 14, 9, 10, 1, 5, 0, 8,
              14, 9, 1, 13, 11, 8, 10, 0, 3, 2, 12, 5, 6, 15, 4, 7,
              8, 10, 12, 15, 5, 7, 1, 14, 0, 13, 6, 4, 2, 11, 9, 3,
              12, 14, 9, 3, 7, 10, 15, 6, 4, 11, 0, 8, 5, 13, 1, 2,
              15, 5, 13, 8, 9, 0, 2, 1, 6, 12, 10, 7, 11, 14, 3, 4,
              7, 4, 0, 10, 13, 11, 12, 5, 14, 1, 3, 2, 9, 6, 8, 15,
              1, 11, 6, 2, 4, 3, 14, 8, 13, 9, 5, 15, 7, 10, 12, 0,
              11, 12, 7, 14, 8, 15, 0, 9, 10, 3, 4, 1, 13, 2, 6, 5,
              0, 1, 15, 9, 10, 13, 7, 4, 5, 6, 2, 14, 8, 3, 11, 12,
              2, 8, 3, 5, 12, 1, 6, 11, 7, 0, 15, 13, 4, 9, 14, 10,
              13, 6, 10, 4, 2, 14, 5, 3, 12, 8, 11, 9, 0, 7, 15, 1,
              10, 15, 11, 7, 14, 9, 3, 13, 1, 5, 8, 0, 12, 4, 2, 6,
              9, 3, 5, 1, 0, 6, 11, 10, 2, 4, 14, 12, 15, 8, 7, 13,
              4, 2, 14, 12, 1, 5, 8, 7, 11, 15, 13, 6, 3, 0, 10, 9,
              6, 13, 8, 0, 15, 12, 4, 2, 9, 10, 7, 3, 14, 1, 5, 11,
          }),
      read_puzzle(
          "5x5",
          "data/sudoku/example_5x5.txt",
          {
              21, 5, 15, 23, 19, 18, 11, 14, 9, 13, 20, 1, 12, 8, 2, 10, 17, 22, 4, 24, 0, 6, 7, 3, 16,
              17, 9, 13, 0, 24, 20, 7, 21, 15, 12, 19, 16, 10, 22, 6, 2, 1, 11, 14, 3, 5, 23, 4, 18, 8,
              20, 4, 2, 8, 18, 1, 19, 0, 6, 5, 3, 14, 11, 15, 17, 16, 23, 21, 13, 7, 24, 22, 12, 9, 10,
              1, 3, 14, 11, 22, 23, 8, 4, 16, 10, 7, 5, 24, 21, 9, 0, 15, 6, 12, 18, 17, 20, 2, 19, 13,
              12, 6, 16, 7, 10, 22, 3, 2, 17, 24, 0, 4, 23, 18, 13, 5, 20, 9, 19, 8, 1, 11, 14, 15, 21,
              6, 14, 1, 2, 16, 11, 15, 20, 24, 7, 5, 3, 9, 23, 0, 4, 19, 18, 22, 17, 13, 8, 21, 10, 12,
              9, 21, 17, 18, 23, 8, 2, 12, 0, 16, 24, 6, 15, 4, 22, 13, 5, 10, 11, 20, 3, 14, 19, 1, 7,
              0, 7, 4, 12, 15, 14, 13, 22, 3, 23, 1, 11, 19, 10, 18, 6, 8, 16, 21, 2, 20, 24, 9, 17, 5,
              24, 10, 19, 20, 11, 5, 6, 17, 21, 4, 8, 13, 2, 14, 12, 7, 3, 0, 9, 1, 18, 15, 22, 16, 23,
              13, 8, 22, 5, 3, 19, 9, 1, 10, 18, 21, 7, 17, 20, 16, 23, 24, 12, 15, 14, 2, 4, 0, 6, 11,
              11, 1, 10, 15, 17, 0, 22, 18, 7, 20, 4, 23, 13, 3, 24, 19, 14, 8, 16, 12, 6, 9, 5, 21, 2,
              3, 19, 18, 22, 4, 12, 23, 8, 13, 21, 2, 10, 0, 17, 20, 24, 9, 1, 5, 6, 7, 16, 11, 14, 15,
              8, 12, 20, 24, 9, 16, 1, 6, 4, 17, 15, 21, 14, 19, 5, 11, 2, 3, 7, 22, 23, 10, 13, 0, 18,
              14, 23, 0, 6, 2, 3, 24, 5, 11, 15, 9, 12, 22, 16, 7, 21, 18, 4, 10, 13, 19, 17, 1, 8, 20,
              7, 16, 5, 21, 13, 2, 14, 10, 19, 9, 6, 18, 8, 1, 11, 20, 0, 17, 23, 15, 22, 12, 24, 4, 3,
              15, 24, 8, 16, 5, 9, 12, 19, 18, 11, 23, 2, 3, 7, 1, 14, 10, 13, 17, 0, 4, 21, 6, 20, 22,
              10, 2, 11, 13, 0, 15, 21, 16, 20, 1, 14, 17, 4, 6, 19, 8, 22, 7, 18, 9, 12, 5, 3, 23, 24,
              19, 18, 9, 3, 1, 10, 0, 7, 23, 6, 13, 22, 16, 5, 21, 15, 12, 24, 20, 4, 8, 2, 17, 11, 14,
              22, 20, 12, 4, 21, 13, 17, 24, 14, 2, 11, 0, 18, 9, 8, 3, 16, 5, 6, 23, 10, 19, 15, 7, 1,
              23, 17, 7, 14, 6, 4, 5, 3, 8, 22, 10, 24, 20, 12, 15, 1, 11, 19, 2, 21, 9, 18, 16, 13, 0,
              5, 22, 6, 17, 7, 24, 18, 15, 1, 19, 16, 20, 21, 2, 3, 9, 13, 23, 8, 11, 14, 0, 10, 12, 4,
              2, 11, 21, 19, 20, 7, 16, 9, 12, 3, 17, 8, 1, 24, 23, 22, 4, 14, 0, 10, 15, 13, 18, 5, 6,
              4, 0, 3, 1, 14, 17, 20, 11, 22, 8, 18, 9, 5, 13, 10, 12, 6, 15, 24, 16, 21, 7, 23, 2, 19,
              18, 15, 23, 10, 12, 21, 4, 13, 2, 0, 22, 19, 6, 11, 14, 17, 7, 20, 3, 5, 16, 1, 8, 24, 9,
              16, 13, 24, 9, 8, 6, 10, 23, 5, 14, 12, 15, 7, 0, 4, 18, 21, 2, 1, 19, 11, 3, 20, 22, 17,
          }),
  };
}

auto unit_range() -> Coordinate_range {
  return Coordinate_range{0.0, 1.0};
}

auto make_circle_problems() -> std::vector<Circle_problem> {
  return {
      Circle_problem{
          "equal radii",
          unit_range(),
          unit_range(),
          {
              Radius_count{0.040, 200},
          }},
      Circle_problem{
          "mixed radii",
          unit_range(),
          unit_range(),
          {
              Radius_count{0.035, 118},
              Radius_count{0.060, 60},
              Radius_count{0.085, 22},
          }},
  };
}

auto outer_side(const Puzzle& puzzle) -> std::size_t {
  return puzzle.inner_side * puzzle.inner_side;
}

auto value_label(int value) -> std::string {
  if (value < 0) {
    return ".";
  }
  return std::to_string(value + 1);
}

auto span_of(Coordinate_range range) -> double {
  return range.upper - range.lower;
}

auto base_circle_count(const Circle_problem& problem) -> std::size_t {
  std::size_t count = 0;
  for (const Radius_count radius_count : problem.radii) {
    count += radius_count.count;
  }
  return count;
}

struct Solver_app {
  std::vector<Puzzle> puzzles = make_puzzles();
  std::vector<Circle_problem> circle_problems = make_circle_problems();
  int domain_index = 1;
  int puzzle_index = 2;
  int build_index = 0;
  int circle_problem_index = 1;
  int circle_build_index = 0;
  std::uint64_t random_seed = 42;
  double sudoku_learning_rate = 1.0;
  double circle_learning_rate = 0.3;
  double convergence_delta = 1e-7;
  int circle_count = 200;
  double circle_density_target = 0.82;
  double nearby_radius_scale = 1.4;
  int max_iterations = 100000;
  int iterations_per_frame = 1;
  int selected_cell = 0;
  int selected_circle = 0;
  bool running = false;
  bool settings_dirty = false;
  double last_step_ms = 0.0;

  std::unique_ptr<Factor_graph> graph;
  Sudoku::Variables full_variables;
  Compact_sudoku_variables compact_variables;
  std::vector<Circle> initial_circles;
  Circle_packing_variables circle_variables;

  Solver_app() {
    rebuild();
  }

  [[nodiscard]] auto puzzle() const -> const Puzzle& {
    return puzzles[static_cast<std::size_t>(puzzle_index)];
  }

  [[nodiscard]] auto circle_problem() const -> const Circle_problem& {
    return circle_problems[static_cast<std::size_t>(circle_problem_index)];
  }

  [[nodiscard]] auto scaled_radii() const -> std::vector<Radius_count> {
    const Circle_problem& problem = circle_problem();
    std::vector<Radius_count> radii = problem.radii;
    for (Radius_count& radius_count : radii) {
      radius_count.count = 0;
    }

    const std::size_t base_count = base_circle_count(problem);
    if (base_count == 0 || radii.empty()) {
      return radii;
    }

    const std::size_t target_count = static_cast<std::size_t>(std::clamp(circle_count, 1, 1000));
    std::vector<double> remainders(radii.size(), 0.0);
    std::size_t assigned_count = 0;
    for (std::size_t index = 0; index < radii.size(); ++index) {
      const double exact_count =
          static_cast<double>(target_count) * static_cast<double>(problem.radii[index].count) /
          static_cast<double>(base_count);
      radii[index].count = static_cast<std::size_t>(std::floor(exact_count));
      remainders[index] = exact_count - static_cast<double>(radii[index].count);
      assigned_count += radii[index].count;
    }

    while (assigned_count < target_count) {
      const auto best = std::max_element(remainders.begin(), remainders.end());
      const std::size_t index = static_cast<std::size_t>(std::distance(remainders.begin(), best));
      ++radii[index].count;
      *best = 0.0;
      ++assigned_count;
    }

    const double area = span_of(problem.horizontal_range) * span_of(problem.vertical_range);
    if (area <= 0.0) {
      return radii;
    }

    double unscaled_density = 0.0;
    for (const Radius_count radius_count : radii) {
      unscaled_density +=
          static_cast<double>(radius_count.count) * std::numbers::pi * radius_count.radius * radius_count.radius;
    }
    unscaled_density /= area;
    if (unscaled_density <= 0.0) {
      return radii;
    }

    const double radius_scale = std::sqrt(std::max(0.0, circle_density_target) / unscaled_density);
    for (Radius_count& radius_count : radii) {
      radius_count.radius *= radius_scale;
    }
    return radii;
  }

  [[nodiscard]] auto circle_density() const -> double {
    const double area =
        span_of(circle_problem().horizontal_range) * span_of(circle_problem().vertical_range);
    if (area <= 0.0) {
      return 0.0;
    }

    double circle_area = 0.0;
    for (const Radius_count radius_count : scaled_radii()) {
      circle_area +=
          static_cast<double>(radius_count.count) * std::numbers::pi * radius_count.radius * radius_count.radius;
    }
    return circle_area / area;
  }

  [[nodiscard]] auto domain() const -> Problem_domain {
    return domain_index == 0 ? Problem_domain::sudoku : Problem_domain::circle_packing;
  }

  [[nodiscard]] auto build_mode() const -> Build_mode {
    return build_index == 0 ? Build_mode::compact : Build_mode::full;
  }

  [[nodiscard]] auto circle_build_mode() const -> Circle_build_mode {
    return circle_build_index == 0 ? Circle_build_mode::fast : Circle_build_mode::full;
  }

  [[nodiscard]] auto active_learning_rate() const -> double {
    return domain() == Problem_domain::sudoku ? sudoku_learning_rate : circle_learning_rate;
  }

  auto active_learning_rate() -> double& {
    return domain() == Problem_domain::sudoku ? sudoku_learning_rate : circle_learning_rate;
  }

  [[nodiscard]] auto sudoku_state() const -> std::vector<int> {
    if (graph == nullptr) {
      return {};
    }
    if (build_mode() == Build_mode::compact) {
      return Compact_sudoku::extract_state(*graph, compact_variables);
    }
    return Sudoku::extract_state(*graph, full_variables);
  }

  [[nodiscard]] auto packed_circles() const -> std::vector<Circle> {
    if (graph == nullptr) {
      return {};
    }
    return Circle_packing::extract_circles(*graph, circle_variables);
  }

  [[nodiscard]] auto circle_max_overlap() const -> double {
    if (graph == nullptr) {
      return 0.0;
    }
    const Circle_problem& problem = circle_problem();
    return Circle_packing::max_overlap(*graph, circle_variables, problem.horizontal_range, problem.vertical_range);
  }

  [[nodiscard]] auto enabled_intersection_factors() const -> std::size_t {
    if (graph == nullptr) {
      return 0;
    }

    std::size_t count = 0;
    for (const auto& row : circle_variables.intersection_factors) {
      for (const auto factor : row) {
        if (graph->is_factor_enabled(factor)) {
          ++count;
        }
      }
    }
    return count;
  }

  [[nodiscard]] auto total_intersection_factors() const -> std::size_t {
    std::size_t count = 0;
    for (const auto& row : circle_variables.intersection_factors) {
      count += row.size();
    }
    return count;
  }

  [[nodiscard]] auto matches_solution(const std::vector<int>& current_state) const -> bool {
    return !puzzle().solution.empty() && current_state == puzzle().solution;
  }

  auto rebuild() -> void {
    graph = std::make_unique<Factor_graph>(
        active_learning_rate(),
        convergence_delta,
        random_seed);
    full_variables.clear();
    compact_variables = {};
    initial_circles.clear();
    circle_variables = {};

    if (domain() == Problem_domain::sudoku) {
      const Puzzle& current_puzzle = puzzle();
      if (build_mode() == Build_mode::compact) {
        compact_variables =
            Compact_sudoku::add_to_factor_graph(*graph, current_puzzle.inner_side, current_puzzle.givens);
      } else {
        full_variables = Sudoku::add_to_factor_graph(*graph, current_puzzle.inner_side, current_puzzle.givens);
      }
      const std::size_t num_cells = outer_side(current_puzzle) * outer_side(current_puzzle);
      selected_cell = std::clamp(selected_cell, 0, static_cast<int>(num_cells - 1));
    } else {
      const Circle_problem& current_problem = circle_problem();
      const std::vector<Radius_count> radii = scaled_radii();
      initial_circles = Circle_packing::generate_circles(
          random_seed,
          radii,
          current_problem.horizontal_range,
          current_problem.vertical_range);
      if (circle_build_mode() == Circle_build_mode::fast) {
        circle_variables = Circle_packing::add_to_factor_graph_fast(
            *graph,
            initial_circles,
            current_problem.horizontal_range,
            current_problem.vertical_range,
            nearby_radius_scale);
      } else {
        circle_variables = Circle_packing::add_to_factor_graph(
            *graph,
            initial_circles,
            current_problem.horizontal_range,
            current_problem.vertical_range);
      }
      if (!initial_circles.empty()) {
        selected_circle = std::clamp(selected_circle, 0, static_cast<int>(initial_circles.size() - 1));
      }
    }
    running = false;
    settings_dirty = false;
    last_step_ms = 0.0;
  }

  auto step_many(int count) -> void {
    if (graph == nullptr || graph->converged()) {
      running = false;
      return;
    }

    const int clamped_count = std::max(1, count);
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < clamped_count && !graph->converged(); ++i) {
      graph->iterate();
    }
    const auto end = std::chrono::steady_clock::now();
    last_step_ms = std::chrono::duration<double, std::milli>{end - start}.count();
    if (graph->converged()) {
      running = false;
    }
  }

  auto run_to_convergence() -> void {
    if (graph == nullptr || graph->converged()) {
      running = false;
      return;
    }

    const std::size_t max_count = static_cast<std::size_t>(std::max(1, max_iterations));
    const auto start = std::chrono::steady_clock::now();
    graph->iterate_until_converged(max_count);
    const auto end = std::chrono::steady_clock::now();
    last_step_ms = std::chrono::duration<double, std::milli>{end - start}.count();
    running = false;
  }
};

auto draw_control_panel(Solver_app& app) -> void {
  ImGui::Begin("Controls");

  std::array<const char*, 2> domain_names{"Sudoku", "Circle Packing"};
  if (ImGui::Combo("Domain", &app.domain_index, domain_names.data(), static_cast<int>(domain_names.size()))) {
    app.rebuild();
  }

  if (app.domain() == Problem_domain::sudoku) {
    std::vector<const char*> puzzle_names;
    puzzle_names.reserve(app.puzzles.size());
    for (const Puzzle& puzzle : app.puzzles) {
      puzzle_names.push_back(puzzle.name.data());
    }

    if (ImGui::Combo("Puzzle", &app.puzzle_index, puzzle_names.data(), static_cast<int>(puzzle_names.size()))) {
      app.selected_cell = 0;
      app.rebuild();
    }
    std::array<const char*, 2> build_names{"Compact", "Full"};
    if (ImGui::Combo("Build", &app.build_index, build_names.data(), static_cast<int>(build_names.size()))) {
      app.rebuild();
    }
  } else {
    std::vector<const char*> problem_names;
    problem_names.reserve(app.circle_problems.size());
    for (const Circle_problem& problem : app.circle_problems) {
      problem_names.push_back(problem.name.data());
    }
    if (ImGui::Combo("Problem", &app.circle_problem_index, problem_names.data(), static_cast<int>(problem_names.size()))) {
      app.selected_circle = 0;
      app.circle_count = static_cast<int>(base_circle_count(app.circle_problem()));
      app.rebuild();
    }
    std::array<const char*, 2> build_names{"Fast", "Full"};
    if (ImGui::Combo("Build", &app.circle_build_index, build_names.data(), static_cast<int>(build_names.size()))) {
      app.rebuild();
    }

    if (ImGui::InputInt("Circle count", &app.circle_count, 10, 50)) {
      app.circle_count = std::clamp(app.circle_count, 1, 1000);
      app.settings_dirty = true;
    }
    if (ImGui::InputDouble("Density", &app.circle_density_target, 0.001, 0.01, "%.3f")) {
      app.circle_density_target = std::max(0.0, app.circle_density_target);
      app.settings_dirty = true;
    }
    if (app.circle_build_mode() == Circle_build_mode::fast &&
        ImGui::InputDouble("Nearby radius scale", &app.nearby_radius_scale, 0.1, 0.5, "%.3f")) {
      app.nearby_radius_scale = std::max(0.0, app.nearby_radius_scale);
      app.settings_dirty = true;
    }
  }

  auto seed_input = static_cast<std::int64_t>(std::min<std::uint64_t>(
      app.random_seed,
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())));
  const std::int64_t seed_step = 1;
  const std::int64_t seed_fast_step = 10;
  if (ImGui::InputScalar("Random seed", ImGuiDataType_S64, &seed_input, &seed_step, &seed_fast_step)) {
    seed_input = std::max<std::int64_t>(0, seed_input);
    app.random_seed = static_cast<std::uint64_t>(seed_input);
    app.settings_dirty = true;
  }
  double& learning_rate = app.active_learning_rate();
  if (ImGui::InputDouble("Learning rate", &learning_rate, 0.05, 0.25, "%.6f")) {
    learning_rate = std::max(0.0, learning_rate);
    app.settings_dirty = true;
  }
  if (ImGui::InputDouble("Convergence delta", &app.convergence_delta, 1e-7, 1e-7, "%.8f")) {
    app.convergence_delta = std::max(0.0, app.convergence_delta);
    app.settings_dirty = true;
  }
  ImGui::InputInt("Max iterations", &app.max_iterations);
  app.max_iterations = std::max(1, app.max_iterations);
  ImGui::InputInt("Iterations/frame", &app.iterations_per_frame);
  app.iterations_per_frame = std::clamp(app.iterations_per_frame, 1, 200);

  if (app.settings_dirty) {
    ImGui::TextColored(ImVec4{1.0F, 0.76F, 0.22F, 1.0F}, "Settings changed. Reset to apply.");
  }

  if (ImGui::Button("Reset")) {
    app.rebuild();
  }
  ImGui::SameLine();
  if (ImGui::Button("Step")) {
    if (app.settings_dirty) {
      app.rebuild();
    }
    app.step_many(1);
  }
  ImGui::SameLine();
  if (ImGui::Button(app.running ? "Pause" : "Run")) {
    if (app.settings_dirty) {
      app.rebuild();
    }
    app.running = !app.running;
  }

  if (ImGui::Button("Run to convergence")) {
    if (app.settings_dirty) {
      app.rebuild();
    }
    app.run_to_convergence();
  }

  ImGui::Separator();

  if (app.graph != nullptr) {
    ImGui::Text("Solver: TWA");
    ImGui::Text("Iterations: %zu", app.graph->iterations());
    ImGui::Text("Converged: %s", app.graph->converged() ? "yes" : "no");
    if (app.domain() == Problem_domain::sudoku) {
      const auto current_state = app.sudoku_state();
      ImGui::Text("Solved: %s", app.matches_solution(current_state) ? "yes" : "no");
    } else {
      ImGui::Text("Max overlap: %.8f", app.circle_max_overlap());
      ImGui::Text("Circles: %zu", app.circle_variables.radii.size());
      ImGui::Text("Density: %.3f", app.circle_density());
      ImGui::Text(
          "Intersections: %zu enabled / %zu total",
          app.enabled_intersection_factors(),
          app.total_intersection_factors());
    }
    ImGui::Text("Variables: %zu", app.graph->num_variables());
    ImGui::Text("Factors: %zu enabled / %zu total", app.graph->num_enabled_factors(), app.graph->num_factors());
    ImGui::Text("Edges: %zu enabled / %zu total", app.graph->num_enabled_edges(), app.graph->num_edges());
    ImGui::Text("Last step: %.3f ms", app.last_step_ms);
  }

  ImGui::End();
}

auto variable_is_certain(const Solver_app& app, twalib::Variable_node variable) -> bool {
  return app.graph != nullptr && variable.is_valid() && app.graph->weight(variable) == Message_weight::infinite;
}

auto candidate_variable(const Solver_app& app, std::size_t cell, std::size_t value) -> Variable_node {
  if (app.build_mode() == Build_mode::compact) {
    if (cell >= app.compact_variables.candidates.size() ||
        value >= app.compact_variables.candidates[cell].size()) {
      return {};
    }
    return app.compact_variables.candidates[cell][value];
  }

  if (cell >= app.full_variables.size() || value >= app.full_variables[cell].size()) {
    return {};
  }
  return app.full_variables[cell][value];
}

auto cell_value_is_certain(
    const Solver_app& app,
    const std::vector<int>& current_state,
    std::size_t cell) -> bool {
  if (app.graph == nullptr || cell >= current_state.size()) {
    return false;
  }

  const int value = current_state[cell];
  if (value < 0) {
    return false;
  }

  return variable_is_certain(app, candidate_variable(app, cell, static_cast<std::size_t>(value)));
}

auto cell_fill_color(
    const Puzzle& puzzle,
    std::size_t cell,
    bool selected,
    bool solved_correctly,
    bool certain) -> ImU32 {
  if (selected) {
    return IM_COL32(72, 110, 160, 255);
  }
  if (puzzle.givens.contains(cell)) {
    return certain ? IM_COL32(62, 66, 72, 255) : IM_COL32(44, 54, 66, 255);
  }
  if (certain) {
    return IM_COL32(58, 70, 42, 255);
  }
  if (solved_correctly) {
    return IM_COL32(36, 74, 54, 255);
  }
  return IM_COL32(28, 30, 34, 255);
}

auto draw_sudoku_grid(Solver_app& app, const std::vector<int>& current_state) -> void {
  const Puzzle& puzzle = app.puzzle();
  const std::size_t side = outer_side(puzzle);
  const float available_width = ImGui::GetContentRegionAvail().x;
  const float cell_size = std::clamp(available_width / static_cast<float>(side), 24.0F, 42.0F);
  const float board_size = cell_size * static_cast<float>(side);
  const ImVec2 top_left = ImGui::GetCursorScreenPos();

  ImGui::InvisibleButton("sudoku_board", ImVec2{board_size, board_size});
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    const ImVec2 mouse = ImGui::GetMousePos();
    const int column = static_cast<int>((mouse.x - top_left.x) / cell_size);
    const int row = static_cast<int>((mouse.y - top_left.y) / cell_size);
    if (row >= 0 && column >= 0 && row < static_cast<int>(side) && column < static_cast<int>(side)) {
      app.selected_cell = row * static_cast<int>(side) + column;
    }
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const bool solved = app.matches_solution(current_state);
  for (std::size_t row = 0; row < side; ++row) {
    for (std::size_t column = 0; column < side; ++column) {
      const std::size_t cell = row * side + column;
      const float x = top_left.x + static_cast<float>(column) * cell_size;
      const float y = top_left.y + static_cast<float>(row) * cell_size;
      const bool selected = app.selected_cell == static_cast<int>(cell);
      const bool certain = cell_value_is_certain(app, current_state, cell);
      const bool given = puzzle.givens.contains(cell);
      const ImU32 fill = cell_fill_color(puzzle, cell, selected, solved, certain);
      draw_list->AddRectFilled(ImVec2{x, y}, ImVec2{x + cell_size, y + cell_size}, fill);
      draw_list->AddRect(ImVec2{x, y}, ImVec2{x + cell_size, y + cell_size}, IM_COL32(80, 86, 96, 255));
      if (certain) {
        const ImU32 certain_color = given ? IM_COL32(198, 204, 214, 255) : IM_COL32(226, 196, 94, 255);
        draw_list->AddRect(
            ImVec2{x + 2.0F, y + 2.0F},
            ImVec2{x + cell_size - 2.0F, y + cell_size - 2.0F},
            certain_color,
            0.0F,
            0,
            2.0F);
      }

      if (cell < current_state.size() && current_state[cell] >= 0) {
        const std::string label = value_label(current_state[cell]);
        const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
        const ImVec2 text_position{
            x + (cell_size - text_size.x) * 0.5F,
            y + (cell_size - text_size.y) * 0.5F,
        };
        draw_list->AddText(text_position, IM_COL32(236, 239, 244, 255), label.c_str());
      }
    }
  }

  for (std::size_t i = 0; i <= side; ++i) {
    const bool major = i % puzzle.inner_side == 0;
    const float thickness = major ? 2.0F : 1.0F;
    const ImU32 color = major ? IM_COL32(210, 216, 226, 255) : IM_COL32(92, 98, 108, 255);
    const float offset = static_cast<float>(i) * cell_size;
    draw_list->AddLine(ImVec2{top_left.x + offset, top_left.y}, ImVec2{top_left.x + offset, top_left.y + board_size}, color, thickness);
    draw_list->AddLine(ImVec2{top_left.x, top_left.y + offset}, ImVec2{top_left.x + board_size, top_left.y + offset}, color, thickness);
  }
}

auto draw_candidate_panel(Solver_app& app, const std::vector<int>& current_state) -> void {
  if (app.graph == nullptr) {
    return;
  }

  const Puzzle& puzzle = app.puzzle();
  const std::size_t side = outer_side(puzzle);
  const std::size_t num_cells = side * side;
  const int selected_cell = std::clamp(app.selected_cell, 0, static_cast<int>(num_cells - 1));
  const int row = selected_cell / static_cast<int>(side);
  const int column = selected_cell % static_cast<int>(side);

  ImGui::Begin("Selected Cell");
  ImGui::Text("Row %d, column %d", row + 1, column + 1);
  if (selected_cell < static_cast<int>(current_state.size())) {
    ImGui::Text("Extracted value: %s", value_label(current_state[static_cast<std::size_t>(selected_cell)]).c_str());
  }

  const auto given = puzzle.givens.find(static_cast<std::size_t>(selected_cell));
  if (given != puzzle.givens.end()) {
    ImGui::Text("Given: %s", value_label(static_cast<int>(given->second)).c_str());
  }
  if (!puzzle.solution.empty()) {
    ImGui::Text("Solution: %s", value_label(puzzle.solution[static_cast<std::size_t>(selected_cell)]).c_str());
  }

  ImGui::Separator();
  const std::size_t selected = static_cast<std::size_t>(selected_cell);
  bool has_candidates = false;
  for (std::size_t value = 0; value < side; ++value) {
    const Variable_node variable = candidate_variable(app, selected, value);
    if (!variable.is_valid()) {
      continue;
    }

    has_candidates = true;
    const double raw_confidence = app.graph->value(variable);
    const float confidence = static_cast<float>(std::clamp(raw_confidence, 0.0, 1.0));
    const bool certain = variable_is_certain(app, variable);
    const auto given = puzzle.givens.find(static_cast<std::size_t>(selected_cell));
    const bool given_certain = certain && given != puzzle.givens.end() && given->second == value;
    char label[64];
    std::snprintf(
        label,
        sizeof(label),
        "%s  %.3f%s",
        value_label(static_cast<int>(value)).c_str(),
        raw_confidence,
        certain ? (given_certain ? "  clue" : "  certain") : "");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{0.58F, 0.16F, 0.20F, 1.0F});
    ImGui::ProgressBar(confidence, ImVec2{-1.0F, 0.0F}, label);
    ImGui::PopStyleColor();
  }
  if (!has_candidates) {
    ImGui::TextDisabled("No candidate variables in compact build.");
  }
  ImGui::End();
}

auto draw_sudoku_panel(Solver_app& app) -> void {
  ImGui::Begin("Sudoku");
  const auto current_state = app.sudoku_state();
  draw_sudoku_grid(app, current_state);
  ImGui::End();

  draw_candidate_panel(app, current_state);
}

auto intersection_factor(
    const Circle_packing_variables& variables,
    std::size_t first,
    std::size_t second) -> twalib::Factor_node {
  if (first > second) {
    std::swap(first, second);
  }
  return variables.intersection_factors[first][second - first - 1];
}

auto circle_pair_enabled(const Solver_app& app, std::size_t first, std::size_t second) -> bool {
  return app.graph != nullptr &&
         app.graph->is_factor_enabled(intersection_factor(app.circle_variables, first, second));
}

auto boundary_overlap(const Circle& circle, const Circle_problem& problem) -> double {
  double overlap = 0.0;
  overlap = std::max(overlap, problem.horizontal_range.lower - (circle.x - circle.radius));
  overlap = std::max(overlap, (circle.x + circle.radius) - problem.horizontal_range.upper);
  overlap = std::max(overlap, problem.vertical_range.lower - (circle.y - circle.radius));
  overlap = std::max(overlap, (circle.y + circle.radius) - problem.vertical_range.upper);
  return overlap;
}

auto pair_overlap(const Circle& first, const Circle& second) -> double {
  const double dx = first.x - second.x;
  const double dy = first.y - second.y;
  return first.radius + second.radius - std::hypot(dx, dy);
}

auto visually_overlaps(double overlap) -> bool {
  return overlap > visual_overlap_epsilon;
}

auto world_to_screen(
    const Circle_problem& problem,
    const Circle& circle,
    ImVec2 top_left,
    float scale,
    float height) -> ImVec2 {
  return ImVec2{
      top_left.x + static_cast<float>(circle.x - problem.horizontal_range.lower) * scale,
      top_left.y + height - static_cast<float>(circle.y - problem.vertical_range.lower) * scale};
}

auto draw_circle_packing_canvas(Solver_app& app, const std::vector<Circle>& circles) -> void {
  const Circle_problem& problem = app.circle_problem();
  const double horizontal_span = span_of(problem.horizontal_range);
  const double vertical_span = span_of(problem.vertical_range);
  if (horizontal_span <= 0.0 || vertical_span <= 0.0) {
    return;
  }

  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float max_width = std::max(260.0F, std::min(available.x, 760.0F));
  const float width = max_width;
  const float height = width * static_cast<float>(vertical_span / horizontal_span);
  const float scale = width / static_cast<float>(horizontal_span);
  const ImVec2 top_left = ImGui::GetCursorScreenPos();
  const ImVec2 bottom_right{top_left.x + width, top_left.y + height};

  ImGui::InvisibleButton("circle_packing_canvas", ImVec2{width, height});
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !circles.empty()) {
    const ImVec2 mouse = ImGui::GetMousePos();
    int nearest = 0;
    float nearest_distance = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < circles.size(); ++index) {
      const ImVec2 center = world_to_screen(problem, circles[index], top_left, scale, height);
      const float dx = mouse.x - center.x;
      const float dy = mouse.y - center.y;
      const float distance = dx * dx + dy * dy;
      if (distance < nearest_distance) {
        nearest = static_cast<int>(index);
        nearest_distance = distance;
      }
    }
    app.selected_circle = nearest;
  }

  std::vector<bool> has_overlap(circles.size(), false);
  for (std::size_t index = 0; index < circles.size(); ++index) {
    has_overlap[index] = visually_overlaps(boundary_overlap(circles[index], problem));
  }
  for (std::size_t first = 0; first + 1 < circles.size(); ++first) {
    for (std::size_t second = first + 1; second < circles.size(); ++second) {
      if (visually_overlaps(pair_overlap(circles[first], circles[second]))) {
        has_overlap[first] = true;
        has_overlap[second] = true;
      }
    }
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(top_left, bottom_right, IM_COL32(18, 20, 24, 255));
  draw_list->AddRect(top_left, bottom_right, IM_COL32(210, 216, 226, 255), 0.0F, 0, 2.0F);

  if (app.circle_build_mode() == Circle_build_mode::fast) {
    std::size_t connection_lines_drawn = 0;
    for (std::size_t first = 0; first + 1 < circles.size() &&
                                connection_lines_drawn < max_drawn_connection_lines;
         ++first) {
      for (std::size_t second = first + 1;
           second < circles.size() && connection_lines_drawn < max_drawn_connection_lines;
           ++second) {
        if (!circle_pair_enabled(app, first, second)) {
          continue;
        }
        const ImVec2 first_center = world_to_screen(problem, circles[first], top_left, scale, height);
        const ImVec2 second_center = world_to_screen(problem, circles[second], top_left, scale, height);
        draw_list->AddLine(first_center, second_center, IM_COL32(226, 196, 94, 180), 1.25F);
        ++connection_lines_drawn;
      }
    }
  }

  for (std::size_t index = 0; index < circles.size(); ++index) {
    const Circle& circle = circles[index];
    const ImVec2 center = world_to_screen(problem, circle, top_left, scale, height);
    const float radius = static_cast<float>(circle.radius) * scale;
    const bool selected = app.selected_circle == static_cast<int>(index);
    const ImU32 fill = selected ? IM_COL32(62, 102, 142, 220) : IM_COL32(48, 92, 118, 210);
    const ImU32 outline = has_overlap[index] ? IM_COL32(210, 72, 76, 255) : IM_COL32(166, 204, 214, 255);
    draw_list->AddCircleFilled(center, radius, fill, 40);
    draw_list->AddCircle(center, radius, outline, 40, has_overlap[index] ? 3.0F : 1.5F);
    if (selected) {
      draw_list->AddCircle(center, radius + 4.0F, IM_COL32(226, 196, 94, 255), 40, 2.0F);
    }
  }
}

auto draw_selected_circle_panel(Solver_app& app, const std::vector<Circle>& circles) -> void {
  if (app.graph == nullptr || circles.empty()) {
    return;
  }

  app.selected_circle = std::clamp(app.selected_circle, 0, static_cast<int>(circles.size() - 1));
  const std::size_t selected = static_cast<std::size_t>(app.selected_circle);
  const Circle& circle = circles[selected];
  const Circle_problem& problem = app.circle_problem();

  std::size_t enabled_neighbors = 0;
  double max_pair_overlap = 0.0;
  for (std::size_t other = 0; other < circles.size(); ++other) {
    if (other == selected) {
      continue;
    }
    if (circle_pair_enabled(app, selected, other)) {
      ++enabled_neighbors;
    }
    max_pair_overlap = std::max(max_pair_overlap, pair_overlap(circle, circles[other]));
  }

  ImGui::Begin("Selected Circle");
  ImGui::Text("Circle %zu of %zu", selected + 1, circles.size());
  ImGui::Text("x: %.6f", circle.x);
  ImGui::Text("y: %.6f", circle.y);
  ImGui::Text("radius: %.6f", circle.radius);
  ImGui::Separator();
  ImGui::Text("Boundary overlap: %.8f", std::max(0.0, boundary_overlap(circle, problem)));
  ImGui::Text("Pair overlap: %.8f", std::max(0.0, max_pair_overlap));
  ImGui::Text("Enabled neighbors: %zu", enabled_neighbors);
  ImGui::End();
}

auto draw_circle_packing_panel(Solver_app& app) -> void {
  ImGui::Begin("Circle Packing");
  const auto circles = app.packed_circles();
  ImGui::Text("Circles: %zu", circles.size());
  ImGui::Text("Density: %.3f", app.circle_density());
  ImGui::Text("Max overlap: %.8f", app.circle_max_overlap());
  const std::size_t enabled_intersections = app.enabled_intersection_factors();
  ImGui::Text(
      "Dynamic intersections: %zu enabled / %zu total",
      enabled_intersections,
      app.total_intersection_factors());
  if (app.circle_build_mode() == Circle_build_mode::fast &&
      enabled_intersections > max_drawn_connection_lines) {
    ImGui::TextDisabled("Connection lines capped at %zu.", max_drawn_connection_lines);
  }
  ImGui::Separator();
  draw_circle_packing_canvas(app, circles);
  ImGui::End();

  draw_selected_circle_panel(app, circles);
}

auto apply_theme() -> void {
  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0F;
  style.FrameRounding = 3.0F;
  style.GrabRounding = 3.0F;
  style.TabRounding = 3.0F;
}

auto create_window() -> GLFWwindow* {
  if (glfwInit() != GLFW_TRUE) {
    return nullptr;
  }

#if defined(__APPLE__)
  const char* glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

  GLFWwindow* window = glfwCreateWindow(1600, 860, "Three Weight Algorithm GUI", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return nullptr;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  apply_theme();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  return window;
}

auto destroy_window(GLFWwindow* window) -> void {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
}

auto run() -> int {
  GLFWwindow* window = create_window();
  if (window == nullptr) {
    return 1;
  }

  Solver_app app;

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();
    if (app.running) {
      app.step_many(app.iterations_per_frame);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    draw_control_panel(app);
    if (app.domain() == Problem_domain::sudoku) {
      draw_sudoku_panel(app);
    } else {
      draw_circle_packing_panel(app);
    }

    ImGui::Render();
    int display_width = 0;
    int display_height = 0;
    glfwGetFramebufferSize(window, &display_width, &display_height);
    glViewport(0, 0, display_width, display_height);
    glClearColor(0.09F, 0.10F, 0.12F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  destroy_window(window);
  return 0;
}

} // namespace

auto main() -> int {
  try {
    return run();
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "gui failed: %s\n", exception.what());
    return 1;
  }
}
