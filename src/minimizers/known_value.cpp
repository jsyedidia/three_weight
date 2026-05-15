#include "twalib/minimizers/known_value.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <span>

namespace twalib {

auto create_known_value_factor(
    Factor_graph& graph,
    Variable_node variable,
    double value) -> Factor_node {
  const Graph_edge edge = graph.create_edge(variable);

  return graph.create_factor({edge}, [value](std::span<Weighted_value_exchange> exchanges, Random_engine&) {
    exchanges[0].set(Weighted_value{value, Message_weight::infinite});
  });
}

} // namespace twalib
