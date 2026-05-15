#include "graph/factor_data.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <cassert>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using twalib::Graph_edge;
using twalib::Message_weight;
using twalib::Minimization_function;
using twalib::Random_engine;
using twalib::Variable_node;
using twalib::Weighted_value;
using twalib::Weighted_value_exchange;
using twalib::detail::Edge_data;
using twalib::detail::Factor_data;

auto make_edges() -> std::vector<Edge_data> {
  std::vector<Edge_data> edges;
  edges.emplace_back(Variable_node{0}, Weighted_value{1.0, Message_weight::standard});
  edges.emplace_back(Variable_node{1}, Weighted_value{2.0, Message_weight::zero});
  return edges;
}

auto test_weighted_value_exchange() -> void {
  Weighted_value_exchange exchange{Graph_edge{3}};

  assert(exchange.edge().index == 3);
  assert(exchange.get().value == 0.0);
  assert(exchange.get().weight == Message_weight::standard);

  exchange.set(Weighted_value{4.5, Message_weight::infinite});
  assert(exchange.get().value == 4.5);
  assert(exchange.get().weight == Message_weight::infinite);
}

auto test_enable_disable_state() -> void {
  const auto factor_edges = std::vector{Graph_edge{0}};
  Factor_data factor{factor_edges, [](std::span<Weighted_value_exchange>, Random_engine&) {}};

  assert(factor.is_enabled());
  assert(!factor.enable());
  assert(factor.disable());
  assert(!factor.is_enabled());
  assert(!factor.disable());
  assert(factor.enable());
  assert(factor.is_enabled());
}

auto test_minimization_uses_ordered_exchanges() -> void {
  auto edge_data = make_edges();
  const auto factor_edges = std::vector{Graph_edge{1}, Graph_edge{0}};
  bool minimizer_ran = false;

  Minimization_function minimizer = [&minimizer_ran](
                                        std::span<Weighted_value_exchange> exchanges,
                                        Random_engine&) {
    minimizer_ran = true;

    assert(exchanges.size() == 2);
    assert(exchanges[0].edge().index == 1);
    assert(exchanges[1].edge().index == 0);
    assert(exchanges[0].get().value == 2.0);
    assert(exchanges[0].get().weight == Message_weight::zero);
    assert(exchanges[1].get().value == 1.0);
    assert(exchanges[1].get().weight == Message_weight::standard);

    exchanges[0].set(Weighted_value{20.0, Message_weight::infinite});
    exchanges[1].set(Weighted_value{10.0, Message_weight::standard});
  };

  Factor_data factor{factor_edges, minimizer};
  Random_engine random{0};
  factor.minimize(edge_data, random);

  assert(minimizer_ran);
  assert(edge_data[1].x() == 20.0);
  assert(edge_data[1].weighted_message_to_variable().weight == Message_weight::infinite);
  assert(edge_data[0].x() == 10.0);
  assert(edge_data[0].weighted_message_to_variable().weight == Message_weight::standard);
}

auto test_incoming_infinite_weight_is_preserved() -> void {
  std::vector<Edge_data> edge_data;
  edge_data.emplace_back(Variable_node{0}, Weighted_value{3.0, Message_weight::infinite});
  edge_data.emplace_back(Variable_node{1}, Weighted_value{4.0, Message_weight::standard});

  const auto factor_edges = std::vector{Graph_edge{0}, Graph_edge{1}};
  Factor_data factor{factor_edges, [](std::span<Weighted_value_exchange> exchanges, Random_engine&) {
                       assert(exchanges[0].get().weight == Message_weight::infinite);
                       assert(exchanges[1].get().weight == Message_weight::standard);

                       exchanges[0].set(Weighted_value{30.0, Message_weight::zero});
                       exchanges[1].set(Weighted_value{40.0, Message_weight::zero});
                     }};

  Random_engine random{0};
  factor.minimize(edge_data, random);

  assert(edge_data[0].weighted_message_to_variable().value == 30.0);
  assert(edge_data[0].weighted_message_to_variable().weight == Message_weight::infinite);
  assert(edge_data[1].weighted_message_to_variable().value == 40.0);
  assert(edge_data[1].weighted_message_to_variable().weight == Message_weight::zero);
}

auto test_disabled_factor_does_not_minimize() -> void {
  auto edge_data = make_edges();
  const auto factor_edges = std::vector{Graph_edge{0}};

  Factor_data factor{factor_edges, [](std::span<Weighted_value_exchange> exchanges, Random_engine&) {
                       exchanges[0].set(Weighted_value{9.0, Message_weight::infinite});
                     }};
  factor.disable();
  Random_engine random{0};
  factor.minimize(edge_data, random);

  assert(edge_data[0].x() == 0.0);
  assert(edge_data[0].weighted_message_to_variable().weight == Message_weight::zero);
}

auto test_reset_enables_factor() -> void {
  const auto factor_edges = std::vector{Graph_edge{0}};
  Factor_data factor{factor_edges, [](std::span<Weighted_value_exchange>, Random_engine&) {}};

  factor.disable();
  factor.reset();

  assert(factor.is_enabled());
}

auto test_invalid_edge_reference_is_explicit() -> void {
  auto edge_data = make_edges();
  const auto factor_edges = std::vector{Graph_edge{2}};
  Factor_data factor{factor_edges, [](std::span<Weighted_value_exchange>, Random_engine&) {}};

  bool threw = false;
  try {
    Random_engine random{0};
    factor.minimize(edge_data, random);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);
}

} // namespace

auto main() -> int {
  test_weighted_value_exchange();
  test_enable_disable_state();
  test_minimization_uses_ordered_exchanges();
  test_incoming_infinite_weight_is_preserved();
  test_disabled_factor_does_not_minimize();
  test_reset_enables_factor();
  test_invalid_edge_reference_is_explicit();
  return 0;
}
