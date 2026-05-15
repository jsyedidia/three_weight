#ifndef TWALIB_GRAPH_VARIABLE_NODE_HPP
#define TWALIB_GRAPH_VARIABLE_NODE_HPP

#include <cstddef>
#include <limits>

namespace twalib {

struct Variable_node {
  static constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

  std::size_t index = invalid_index;

  [[nodiscard]] constexpr auto is_valid() const -> bool {
    return index != invalid_index;
  }
};

} // namespace twalib

#endif
