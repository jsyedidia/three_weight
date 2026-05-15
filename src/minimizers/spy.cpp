#include "twalib/minimizers/spy.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <span>
#include <utility>

namespace twalib {

auto create_spy_factor(
    Factor_graph& graph,
    Variable_node variable,
    Spy_function value_function) -> Factor_node {
  const Graph_edge edge = graph.create_edge(variable);

  return graph.create_factor({edge}, [value_function = std::move(value_function)](
                                         std::span<Weighted_value_exchange> exchanges,
                                         Random_engine&) {
    const Weighted_value incoming = exchanges[0].get();

    if (const auto result = value_function(incoming); result.has_value()) {
      exchanges[0].set(*result);
    } else {
      exchanges[0].set(Weighted_value{incoming.value, Message_weight::zero});
    }
  });
}

} // namespace twalib
