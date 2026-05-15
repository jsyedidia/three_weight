#include "twalib/graph/factor_graph.hpp"
#include "twalib/minimizers/in_range.hpp"
#include "twalib/minimizers/known_value.hpp"
#include "twalib/minimizers/one_hot.hpp"
#include "twalib/minimizers/spy.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {

using twalib::Factor_graph;
using twalib::Message_weight;
using twalib::Variable_node;

auto nearly_equal(double left, double right) -> bool {
  return std::abs(left - right) < 1e-10;
}

auto test_known_value_converges_with_infinite_weight() -> void {
  Factor_graph graph;
  const Variable_node variable = graph.create_variable(0.0);

  [[maybe_unused]] const auto factor = twalib::create_known_value_factor(graph, variable, 3.5);

  assert(graph.num_edges() == 1);
  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(variable), 3.5));
}

auto test_in_range_clamps_below_and_above() -> void {
  {
    Factor_graph graph;
    const Variable_node variable = graph.create_variable(-10.0);

    [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 0.0, 1.0);

    assert(graph.iterate_until_converged(100));
    assert(nearly_equal(graph.value(variable), 0.0));
  }

  {
    Factor_graph graph;
    const Variable_node variable = graph.create_variable(10.0);

    [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 0.0, 1.0);

    assert(graph.iterate_until_converged(100));
    assert(nearly_equal(graph.value(variable), 1.0));
  }
}

auto test_in_range_has_no_opinion_inside_range() -> void {
  Factor_graph graph;
  const Variable_node variable = graph.create_variable(0.25);

  [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 0.0, 1.0);

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(variable), 0.25));
}

auto test_in_range_preserves_certain_initial_values_inside_range() -> void {
  Factor_graph graph;
  const Variable_node variable = graph.create_variable(0.75, Message_weight::infinite);

  [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 0.0, 1.0);

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(variable), 0.75));
}

auto test_in_range_rejects_inverted_bounds() -> void {
  Factor_graph graph;
  const Variable_node variable = graph.create_variable(0.0);

  bool threw = false;
  try {
    [[maybe_unused]] const auto factor = twalib::create_in_range_factor(graph, variable, 1.0, 0.0);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

auto test_spy_hides_with_zero_weight() -> void {
  Factor_graph graph;
  const Variable_node variable = graph.create_variable(2.0);
  bool saw_incoming_value = false;

  [[maybe_unused]] const auto factor = twalib::create_spy_factor(
      graph,
      variable,
      [&saw_incoming_value](twalib::Weighted_value incoming) -> std::optional<twalib::Weighted_value> {
        saw_incoming_value = nearly_equal(incoming.value, 2.0);
        return std::nullopt;
      });

  assert(graph.iterate_until_converged(100));
  assert(saw_incoming_value);
  assert(nearly_equal(graph.value(variable), 2.0));
}

auto test_spy_emits_user_value() -> void {
  Factor_graph graph;
  const Variable_node variable = graph.create_variable(2.0);

  [[maybe_unused]] const auto factor = twalib::create_spy_factor(
      graph,
      variable,
      [](twalib::Weighted_value) -> std::optional<twalib::Weighted_value> {
        return twalib::Weighted_value{5.0, Message_weight::infinite};
      });

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(variable), 5.0));
}

auto values(const Factor_graph& graph, const std::vector<Variable_node>& variables) -> std::vector<double> {
  std::vector<double> result;
  result.reserve(variables.size());
  for (const Variable_node variable : variables) {
    result.push_back(graph.value(variable));
  }
  return result;
}

auto tied_one_hot_choice(std::uint64_t seed) -> int {
  Factor_graph graph{1.0, 1e-5, seed};
  const auto variables = std::vector{
      graph.create_variable(0.8, Message_weight::zero),
      graph.create_variable(0.8, Message_weight::zero)};

  [[maybe_unused]] const auto factor = twalib::create_one_hot_factor(graph, variables);

  assert(graph.iterate_until_converged(100));
  const auto result = values(graph, variables);
  if (result == std::vector<double>({1.0, 0.0})) {
    return 0;
  }
  if (result == std::vector<double>({0.0, 1.0})) {
    return 1;
  }
  assert(false);
  return -1;
}

auto tied_one_hot_choice_after_reseed(std::uint64_t seed) -> int {
  Factor_graph graph;
  graph.set_random_seed(seed);

  const auto variables = std::vector{
      graph.create_variable(0.8, Message_weight::zero),
      graph.create_variable(0.8, Message_weight::zero)};

  [[maybe_unused]] const auto factor = twalib::create_one_hot_factor(graph, variables);

  assert(graph.iterate_until_converged(100));
  const auto result = values(graph, variables);
  if (result == std::vector<double>({1.0, 0.0})) {
    return 0;
  }
  if (result == std::vector<double>({0.0, 1.0})) {
    return 1;
  }
  assert(false);
  return -1;
}

auto test_one_hot_seeded_ties_are_reproducible() -> void {
  assert(tied_one_hot_choice(1234) == tied_one_hot_choice(1234));
  assert(tied_one_hot_choice_after_reseed(5678) == tied_one_hot_choice_after_reseed(5678));
}

auto test_one_hot_prefers_standard_over_zero_weight() -> void {
  Factor_graph graph;
  const auto variables = std::vector{
      graph.create_variable(0.9, Message_weight::zero),
      graph.create_variable(0.1, Message_weight::standard),
      graph.create_variable(0.2, Message_weight::zero)};

  [[maybe_unused]] const auto factor = twalib::create_one_hot_factor(graph, variables);

  assert(graph.iterate_until_converged(100));
  assert(values(graph, variables) == std::vector<double>({0.0, 1.0, 0.0}));
}

auto test_one_hot_infinite_one_wins() -> void {
  Factor_graph graph;
  const auto variables = std::vector{
      graph.create_variable(0.9),
      graph.create_variable(1.0, Message_weight::infinite),
      graph.create_variable(0.8)};

  [[maybe_unused]] const auto factor = twalib::create_one_hot_factor(graph, variables);

  assert(graph.iterate_until_converged(100));
  assert(values(graph, variables) == std::vector<double>({0.0, 1.0, 0.0}));
}

auto test_one_hot_all_but_one_infinite_zero_implies_one() -> void {
  Factor_graph graph;
  const auto variables = std::vector{
      graph.create_variable(0.0, Message_weight::infinite),
      graph.create_variable(0.2, Message_weight::standard),
      graph.create_variable(0.0, Message_weight::infinite)};

  [[maybe_unused]] const auto factor = twalib::create_one_hot_factor(graph, variables);

  assert(graph.iterate_until_converged(100));
  assert(values(graph, variables) == std::vector<double>({0.0, 1.0, 0.0}));
}

auto test_one_hot_preserves_incoming_infinite_zero_weight() -> void {
  Factor_graph graph;
  const auto variables = std::vector{
      graph.create_variable(0.0, Message_weight::infinite),
      graph.create_variable(0.2, Message_weight::standard),
      graph.create_variable(0.8, Message_weight::standard)};

  [[maybe_unused]] const auto factor = twalib::create_one_hot_factor(graph, variables);

  graph.iterate();
  assert(values(graph, variables) == std::vector<double>({0.0, 0.0, 1.0}));
  assert(graph.weight(variables[0]) == Message_weight::infinite);
  assert(graph.weight(variables[1]) == Message_weight::standard);
  assert(graph.weight(variables[2]) == Message_weight::standard);
}

auto test_one_hot_standard_ties_are_randomized() -> void {
  bool saw_first = false;
  bool saw_second = false;
  for (std::uint64_t seed = 0; seed < 50; ++seed) {
    const int choice = tied_one_hot_choice(seed);
    saw_first = saw_first || choice == 0;
    saw_second = saw_second || choice == 1;
  }

  assert(saw_first);
  assert(saw_second);
}

auto test_one_hot_rejects_empty_variable_set() -> void {
  Factor_graph graph;

  bool threw = false;
  try {
    [[maybe_unused]] const auto factor = twalib::create_one_hot_factor(graph, std::vector<Variable_node>{});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  assert(threw);
}

} // namespace

auto main() -> int {
  test_known_value_converges_with_infinite_weight();
  test_in_range_clamps_below_and_above();
  test_in_range_has_no_opinion_inside_range();
  test_in_range_preserves_certain_initial_values_inside_range();
  test_in_range_rejects_inverted_bounds();
  test_spy_hides_with_zero_weight();
  test_spy_emits_user_value();
  test_one_hot_seeded_ties_are_reproducible();
  test_one_hot_prefers_standard_over_zero_weight();
  test_one_hot_infinite_one_wins();
  test_one_hot_all_but_one_infinite_zero_implies_one();
  test_one_hot_preserves_incoming_infinite_zero_weight();
  test_one_hot_standard_ties_are_randomized();
  test_one_hot_rejects_empty_variable_set();
  return 0;
}
