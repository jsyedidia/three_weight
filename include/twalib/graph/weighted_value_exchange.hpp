#ifndef TWALIB_GRAPH_WEIGHTED_VALUE_EXCHANGE_HPP
#define TWALIB_GRAPH_WEIGHTED_VALUE_EXCHANGE_HPP

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"

#include <functional>
#include <random>
#include <span>

namespace twalib {

class Weighted_value_exchange {
 public:
  explicit constexpr Weighted_value_exchange(Graph_edge edge) : edge_(edge) {}

  [[nodiscard]] constexpr auto edge() const -> Graph_edge {
    return edge_;
  }

  [[nodiscard]] constexpr auto get() const -> Weighted_value {
    return weighted_value_;
  }

  constexpr auto set(Weighted_value weighted_value) -> void {
    weighted_value_ = weighted_value;
  }

 private:
  Graph_edge edge_;
  Weighted_value weighted_value_;
};

using Random_engine = std::mt19937_64;
using Minimization_function = std::function<void(std::span<Weighted_value_exchange>, Random_engine&)>;

} // namespace twalib

#endif
