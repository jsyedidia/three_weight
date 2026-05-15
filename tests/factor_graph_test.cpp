#include "twalib/graph/factor_graph.hpp"
#include "twalib/minimizers/in_range.hpp"
#include "twalib/minimizers/known_value.hpp"

#include <cassert>
#include <cmath>
#include <span>
#include <stdexcept>

namespace {

using twalib::Factor_graph;
using twalib::Graph_edge;
using twalib::Message_weight;
using twalib::Variable_node;

auto nearly_equal(double left, double right) -> bool {
  return std::abs(left - right) < 1e-10;
}

auto test_known_value_converges() -> void {
  Factor_graph graph{};
  const Variable_node variable = graph.create_variable(0.0);
  [[maybe_unused]] const auto factor = twalib::create_known_value_factor(graph, variable, 7.0);

  assert(graph.iterate_until_converged(100));

  assert(graph.converged());
  assert(nearly_equal(graph.value(variable), 7.0));
  assert(graph.weight(variable) == Message_weight::infinite);
  assert(graph.iterations() >= 2);
}

auto test_range_clamps_below_above_and_inside() -> void {
  {
    Factor_graph graph{};
    const Variable_node variable = graph.create_variable(-2.0);
    [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 0.0, 1.0);

    assert(graph.iterate_until_converged(100));
    assert(nearly_equal(graph.value(variable), 0.0));
  }

  {
    Factor_graph graph{};
    const Variable_node variable = graph.create_variable(5.0);
    [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 0.0, 1.0);

    assert(graph.iterate_until_converged(100));
    assert(nearly_equal(graph.value(variable), 1.0));
  }

  {
    Factor_graph graph{};
    const Variable_node variable = graph.create_variable(0.4);
    [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 0.0, 1.0);

    assert(graph.iterate_until_converged(100));
    assert(nearly_equal(graph.value(variable), 0.4));
  }
}

auto test_reinitialize_resets_graph_state() -> void {
  Factor_graph graph{};
  const Variable_node variable = graph.create_variable(2.0);
  [[maybe_unused]] const auto factor = twalib::create_known_value_factor(graph, variable, 4.0);

  int iteration_callbacks = 0;
  int reinitialize_callbacks = 0;
  graph.add_iteration_callback([&iteration_callbacks] {
    ++iteration_callbacks;
  });
  graph.add_reinitialize_callback([&reinitialize_callbacks] {
    ++reinitialize_callbacks;
  });

  assert(graph.iterate_until_converged(100));
  assert(graph.converged());
  assert(graph.iterations() > 0);
  assert(iteration_callbacks == static_cast<int>(graph.iterations()));
  assert(nearly_equal(graph.value(variable), 4.0));

  graph.reinitialize();
  assert(!graph.converged());
  assert(graph.iterations() == 0);
  assert(reinitialize_callbacks == 1);
  assert(nearly_equal(graph.value(variable), 2.0));

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(variable), 4.0));
}

auto test_factor_enable_disable_counts_and_behavior() -> void {
  Factor_graph graph{};
  const Variable_node variable = graph.create_variable(1.0);

  [[maybe_unused]] const auto active_factor =
      twalib::create_in_range_factor(graph, variable, 0.0, 20.0);
  const auto optional_factor = twalib::create_known_value_factor(graph, variable, 10.0);

  assert(graph.num_variables() == 1);
  assert(graph.num_edges() == 2);
  assert(graph.num_enabled_edges() == 2);
  assert(graph.num_factors() == 2);
  assert(graph.num_enabled_factors() == 2);

  graph.set_factor_enabled(optional_factor, false);
  assert(!graph.is_factor_enabled(optional_factor));
  assert(graph.num_enabled_factors() == 1);
  assert(graph.num_enabled_edges() == 1);

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(variable), 1.0));

  graph.set_factor_enabled(optional_factor, true);
  assert(graph.is_factor_enabled(optional_factor));
  assert(graph.num_enabled_factors() == 2);
  assert(graph.num_enabled_edges() == 2);

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(variable), 10.0));
}

auto test_lone_standard_message_resets_disagreement() -> void {
  Factor_graph graph{0.5};
  const Variable_node variable = graph.create_variable(0.0);
  const Graph_edge first_edge = graph.create_edge(variable);
  const Graph_edge second_edge = graph.create_edge(variable);

  [[maybe_unused]] const auto first_factor =
      graph.create_factor({first_edge}, [](std::span<twalib::Weighted_value_exchange> exchanges, auto&) {
        exchanges[0].set({10.0, Message_weight::standard});
      });

  int second_factor_calls = 0;
  [[maybe_unused]] const auto second_factor = graph.create_factor(
      {second_edge},
      [&second_factor_calls](std::span<twalib::Weighted_value_exchange> exchanges, auto&) {
        if (second_factor_calls == 0) {
          exchanges[0].set({0.0, Message_weight::standard});
        } else {
          exchanges[0].set({0.0, Message_weight::zero});
        }
        ++second_factor_calls;
      });

  assert(!graph.iterate());
  assert(nearly_equal(graph.value(variable), 5.0));

  assert(!graph.iterate());
  assert(nearly_equal(graph.value(variable), 12.5));

  assert(!graph.iterate());
  assert(nearly_equal(graph.value(variable), 10.0));
}

auto test_invalid_handles_are_explicit() -> void {
  Factor_graph graph{};

  bool threw = false;
  try {
    (void)graph.value(Variable_node{0});
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);

  const Variable_node variable = graph.create_variable(0.0);
  threw = false;
  try {
    [[maybe_unused]] const auto factor = graph.create_factor({Graph_edge{3}}, [](auto, auto&) {});
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    (void)graph.create_edge(Variable_node{variable.index + 1});
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);
}

} // namespace

auto main() -> int {
  test_known_value_converges();
  test_range_clamps_below_above_and_inside();
  test_reinitialize_resets_graph_state();
  test_factor_enable_disable_counts_and_behavior();
  test_lone_standard_message_resets_disagreement();
  test_invalid_handles_are_explicit();
  return 0;
}
