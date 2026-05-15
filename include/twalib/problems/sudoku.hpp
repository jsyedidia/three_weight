#ifndef TWALIB_PROBLEMS_SUDOKU_HPP
#define TWALIB_PROBLEMS_SUDOKU_HPP

#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/variable_node.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace twalib {

class Sudoku {
 public:
  using Givens = std::unordered_map<std::size_t, std::size_t>;
  using Variables = std::vector<std::vector<Variable_node>>;

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
