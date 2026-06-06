#include "twalib/graph/factor_graph.hpp"
#include "twalib/problems/compact_sudoku.hpp"
#include "twalib/problems/sudoku.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using twalib::Compact_sudoku;
using twalib::Factor_graph;
using twalib::Sudoku;

constexpr const char* default_problem_path = "data/sudoku/example_4x4_medium.txt";

enum class Sudoku_builder {
  compact,
  full,
};

struct Options {
  std::string problem_path = default_problem_path;
  Sudoku_builder builder = Sudoku_builder::compact;
  double learning_rate = 1.0;
  double convergence_delta = 1e-5;
  std::size_t max_iterations = 100000;
  std::uint64_t seed = 42;
  bool print_solution = false;
  bool show_help = false;
};

struct Sudoku_problem {
  std::size_t inner_side = 0;
  std::size_t outer_side = 0;
  Sudoku::Givens givens;
};

auto print_usage(const char* program_name) -> void {
  std::print(
      R"(Usage: {} [--problem_path PATH] [options]

Solve a Sudoku text file using the Three-Weight Algorithm factor graph engine.

Options:
  --problem_path PATH  Path to a Sudoku text file using . for empty cells
                       (default: {})
  --builder NAME       Graph builder: compact or full (default: compact)
  --learning_rate X    Dual update learning rate / alpha (default: 1.0)
  --convergence_delta X
                       Variable belief convergence threshold (default: 1e-5)
  --max_iterations N   Maximum solver iterations (default: 100000)
  --seed N             RNG seed for reproducible one-hot tie-breaking (default: 42)
  --print_solution     Print the solved grid after solving
  --help               Show this help message
)",
      program_name,
      default_problem_path);
}

auto require_value(int argc, char** argv, int& index, const std::string& flag_name) -> std::string {
  if (index + 1 >= argc) {
    std::cerr << "Missing value for " << flag_name << "\n";
    std::exit(1);
  }
  return argv[++index];
}

auto parse_unsigned_argument(const std::string& value, const std::string& flag_name) -> std::uint64_t {
  try {
    return std::stoull(value);
  } catch (const std::exception&) {
    std::cerr << "Invalid unsigned integer for " << flag_name << ": " << value << "\n";
    std::exit(1);
  }
}

auto parse_size_argument(const std::string& value, const std::string& flag_name) -> std::size_t {
  return static_cast<std::size_t>(parse_unsigned_argument(value, flag_name));
}

auto parse_double_argument(const std::string& value, const std::string& flag_name) -> double {
  try {
    const double result = std::stod(value);
    if (!std::isfinite(result)) {
      throw std::invalid_argument{"not finite"};
    }
    return result;
  } catch (const std::exception&) {
    std::cerr << "Invalid floating-point value for " << flag_name << ": " << value << "\n";
    std::exit(1);
  }
}

auto parse_builder_argument(const std::string& value, const std::string& flag_name) -> Sudoku_builder {
  if (value == "compact") {
    return Sudoku_builder::compact;
  }
  if (value == "full" || value == "swift") {
    return Sudoku_builder::full;
  }

  std::cerr << "Invalid builder for " << flag_name << ": " << value << "\n";
  std::exit(1);
}

auto parse_options(int argc, char** argv) -> Options {
  Options options;

  for (int index = 1; index < argc; ++index) {
    const auto argument = std::string{argv[index]};
    if (argument == "--help" || argument == "-h") {
      options.show_help = true;
      continue;
    }
    if (argument == "--problem_path") {
      options.problem_path = require_value(argc, argv, index, argument);
      continue;
    }
    if (argument == "--builder") {
      options.builder = parse_builder_argument(require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--learning_rate") {
      options.learning_rate = parse_double_argument(require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--convergence_delta") {
      options.convergence_delta = parse_double_argument(require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--max_iterations") {
      options.max_iterations = parse_size_argument(require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--seed") {
      options.seed = parse_unsigned_argument(require_value(argc, argv, index, argument), argument);
      continue;
    }
    if (argument == "--print_solution") {
      options.print_solution = true;
      continue;
    }

    std::cerr << "Unknown option: " << argument << "\n";
    std::exit(1);
  }

  return options;
}

auto builder_name(Sudoku_builder builder) -> const char* {
  switch (builder) {
  case Sudoku_builder::compact:
    return "compact";
  case Sudoku_builder::full:
    return "full";
  }

  return "unknown";
}

auto is_perfect_square(std::size_t value) -> bool {
  const auto root = static_cast<std::size_t>(std::sqrt(static_cast<double>(value)));
  return root * root == value || (root + 1) * (root + 1) == value;
}

auto square_root(std::size_t value) -> std::size_t {
  const auto root = static_cast<std::size_t>(std::sqrt(static_cast<double>(value)));
  if (root * root == value) {
    return root;
  }
  if ((root + 1) * (root + 1) == value) {
    return root + 1;
  }
  throw std::invalid_argument("Sudoku side length must be a perfect square");
}

auto parse_token(const std::string& token, std::size_t outer_side, std::size_t row, std::size_t column) -> int {
  if (token == ".") {
    return -1;
  }

  int value = 0;
  try {
    value = std::stoi(token);
  } catch (const std::exception&) {
    throw std::invalid_argument("Invalid Sudoku token at row " + std::to_string(row + 1) +
                                ", column " + std::to_string(column + 1) + ": " + token);
  }

  if (value < 1 || value > static_cast<int>(outer_side)) {
    throw std::out_of_range("Sudoku clue out of range at row " + std::to_string(row + 1) +
                            ", column " + std::to_string(column + 1) + ": " + token);
  }
  return value - 1;
}

auto read_problem(const std::string& path) -> Sudoku_problem {
  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error("Could not open Sudoku problem: " + path);
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

  if (rows.empty()) {
    throw std::invalid_argument("Sudoku problem is empty");
  }

  const std::size_t outer_side = rows.size();
  if (!is_perfect_square(outer_side)) {
    throw std::invalid_argument("Sudoku row count must be a perfect square");
  }
  for (std::size_t row = 0; row < rows.size(); ++row) {
    if (rows[row].size() != outer_side) {
      throw std::invalid_argument("Sudoku problem must be square; row " + std::to_string(row + 1) +
                                  " has " + std::to_string(rows[row].size()) + " columns");
    }
  }

  Sudoku_problem problem;
  problem.inner_side = square_root(outer_side);
  problem.outer_side = outer_side;

  for (std::size_t row = 0; row < outer_side; ++row) {
    for (std::size_t column = 0; column < outer_side; ++column) {
      const int value = parse_token(rows[row][column], outer_side, row, column);
      if (value >= 0) {
        problem.givens.emplace(row * outer_side + column, static_cast<std::size_t>(value));
      }
    }
  }

  return problem;
}

auto print_solution(const std::vector<int>& solution, std::size_t outer_side) -> void {
  std::print("\n== Solution ==\n\n");
  for (std::size_t row = 0; row < outer_side; ++row) {
    for (std::size_t column = 0; column < outer_side; ++column) {
      if (column > 0) {
        std::print("\t");
      }

      const int value = solution[row * outer_side + column];
      if (value < 0) {
        std::print(".");
      } else {
        std::print("{}", value + 1);
      }
    }
    std::print("\n");
  }
}

auto run(const Options& options) -> int {
  std::print("Solving:\n");
  std::print(" generating problem...");
  const auto generation_start = std::chrono::steady_clock::now();
  const Sudoku_problem problem = read_problem(options.problem_path);
  Factor_graph graph{
      options.learning_rate,
      options.convergence_delta,
      options.seed};

  Sudoku::Variables full_variables;
  Compact_sudoku::Variables compact_variables;
  if (options.builder == Sudoku_builder::compact) {
    compact_variables = Compact_sudoku::add_to_factor_graph(graph, problem.inner_side, problem.givens);
  } else {
    full_variables = Sudoku::add_to_factor_graph(graph, problem.inner_side, problem.givens);
  }

  const auto generation_end = std::chrono::steady_clock::now();
  const double generation_milliseconds =
      std::chrono::duration<double, std::milli>{generation_end - generation_start}.count();
  std::print(
      " done ({} factors, {} variables, {} edges, {} msecs)\n",
      graph.num_factors(),
      graph.num_variables(),
      graph.num_edges(),
      generation_milliseconds);
  std::print(
      " configuration: builder={}, learning_rate={}, convergence_delta={}, max_iterations={}, seed={}\n",
      builder_name(options.builder),
      options.learning_rate,
      options.convergence_delta,
      options.max_iterations,
      options.seed);

  std::print(" solving problem...");
  const auto start = std::chrono::steady_clock::now();
  const bool converged = graph.iterate_until_converged(options.max_iterations);
  const auto end = std::chrono::steady_clock::now();
  const double milliseconds = std::chrono::duration<double, std::milli>{end - start}.count();
  const double milliseconds_per_iteration =
      graph.iterations() == 0 ? 0.0 : milliseconds / static_cast<double>(graph.iterations());

  std::print(
      " done ({} in {} iterations, {} msecs, {} msec/iteration)\n",
      converged ? "did converge" : "did not converge",
      graph.iterations(),
      milliseconds,
      milliseconds_per_iteration);

  if (options.print_solution) {
    if (options.builder == Sudoku_builder::compact) {
      print_solution(Compact_sudoku::extract_state(graph, compact_variables), problem.outer_side);
    } else {
      print_solution(Sudoku::extract_state(graph, full_variables), problem.outer_side);
    }
  }
  return converged ? 0 : 2;
}

} // namespace

auto main(int argc, char** argv) -> int {
  const Options options = parse_options(argc, argv);
  if (options.show_help) {
    print_usage(argv[0]);
    return 0;
  }

  try {
    return run(options);
  } catch (const std::exception& exception) {
    std::cerr << "sudoku failed: " << exception.what() << "\n";
    return 1;
  }
}
