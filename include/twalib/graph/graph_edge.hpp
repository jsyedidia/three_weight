#ifndef TWALIB_GRAPH_GRAPH_EDGE_HPP
#define TWALIB_GRAPH_GRAPH_EDGE_HPP

#include <cstddef>
#include <limits>

namespace twalib {

struct Graph_edge {
  static constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();

  std::size_t index = invalid_index;

  [[nodiscard]] constexpr auto is_valid() const -> bool {
    return index != invalid_index;
  }
};

} // namespace twalib

#endif
