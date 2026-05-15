#ifndef TWALIB_PROBLEMS_COMPACT_SUDOKU_HPP
#define TWALIB_PROBLEMS_COMPACT_SUDOKU_HPP

#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/problems/sudoku.hpp"

#include <cstddef>
#include <vector>

namespace twalib {

struct Compact_sudoku_variables {
  std::size_t inner_side = 0;
  std::size_t outer_side = 0;
  Sudoku::Givens givens;
  std::vector<std::vector<Variable_node>> candidates;
};

class Compact_sudoku {
 public:
  using Givens = Sudoku::Givens;
  using Variables = Compact_sudoku_variables;

  [[nodiscard]] static auto add_to_factor_graph(
      Factor_graph& graph,
      std::size_t inner_side,
      const Givens& givens) -> Variables;

  [[nodiscard]] static auto extract_state(
      const Factor_graph& graph,
      const Variables& variables) -> std::vector<int>;
};

} // namespace twalib

#endif
