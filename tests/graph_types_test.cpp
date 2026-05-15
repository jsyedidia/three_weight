#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <cassert>
#include <cmath>
#include <concepts>

namespace {

static_assert(!std::same_as<twalib::Variable_node, twalib::Factor_node>);
static_assert(!std::same_as<twalib::Variable_node, twalib::Graph_edge>);
static_assert(!std::same_as<twalib::Factor_node, twalib::Graph_edge>);

auto test_handles() -> void {
  const auto default_variable = twalib::Variable_node{};
  const auto default_factor = twalib::Factor_node{};
  const auto default_edge = twalib::Graph_edge{};

  assert(!default_variable.is_valid());
  assert(!default_factor.is_valid());
  assert(!default_edge.is_valid());

  const auto variable = twalib::Variable_node{3};
  const auto factor = twalib::Factor_node{5};
  const auto edge = twalib::Graph_edge{7};

  assert(variable.is_valid());
  assert(factor.is_valid());
  assert(edge.is_valid());
  assert(variable.index == 3);
  assert(factor.index == 5);
  assert(edge.index == 7);
}

auto test_weighted_values() -> void {
  const auto default_value = twalib::Weighted_value{};
  assert(default_value.value == 0.0);
  assert(default_value.weight == twalib::Message_weight::standard);

  const auto known_zero = twalib::Weighted_value{2.0, twalib::Message_weight::zero};
  assert(known_zero.value == 2.0);
  assert(known_zero.weight == twalib::Message_weight::zero);

  assert(twalib::message_weight_value(twalib::Message_weight::zero) == 0.0);
  assert(twalib::message_weight_value(twalib::Message_weight::standard) == 1.0);
  assert(std::isinf(twalib::message_weight_value(twalib::Message_weight::infinite)));
}

auto test_weighted_value_exchange() -> void {
  auto exchange = twalib::Weighted_value_exchange{twalib::Graph_edge{11}};
  assert(exchange.edge().index == 11);
  assert(exchange.get().value == 0.0);
  assert(exchange.get().weight == twalib::Message_weight::standard);

  exchange.set(twalib::Weighted_value{4.0, twalib::Message_weight::infinite});
  assert(exchange.get().value == 4.0);
  assert(exchange.get().weight == twalib::Message_weight::infinite);
}

} // namespace

auto main() -> int {
  test_handles();
  test_weighted_values();
  test_weighted_value_exchange();
  return 0;
}
