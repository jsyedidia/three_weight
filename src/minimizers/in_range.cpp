#include "twalib/minimizers/in_range.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <span>
#include <stdexcept>

namespace twalib {

auto create_in_range_factor(
    Factor_graph& graph,
    Variable_node variable,
    double lower,
    double upper) -> Factor_node {
  if (upper < lower) {
    throw std::invalid_argument("create_in_range_factor requires lower <= upper");
  }

  const Graph_edge edge = graph.create_edge(variable);

  return graph.create_factor({edge}, [lower, upper](std::span<Weighted_value_exchange> exchanges, Random_engine&) {
    const Weighted_value incoming = exchanges[0].get();

    if (incoming.value < lower) {
      exchanges[0].set(Weighted_value{lower, Message_weight::standard});
    } else if (incoming.value > upper) {
      exchanges[0].set(Weighted_value{upper, Message_weight::standard});
    } else {
      exchanges[0].set(Weighted_value{incoming.value, Message_weight::zero});
    }
  });
}

} // namespace twalib
