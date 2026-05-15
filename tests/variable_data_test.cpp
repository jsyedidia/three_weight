#include "graph/variable_data.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"

#include <cassert>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using twalib::Graph_edge;
using twalib::Message_weight;
using twalib::Variable_node;
using twalib::Weighted_value;
using twalib::detail::Edge_data;
using twalib::detail::Variable_data;

auto edge_indexes(std::span<const Graph_edge> edges) -> std::vector<std::size_t> {
  std::vector<std::size_t> indexes;
  indexes.reserve(edges.size());
  for (const Graph_edge edge : edges) {
    indexes.push_back(edge.index);
  }
  return indexes;
}

auto make_edge_data(std::size_t count) -> std::vector<Edge_data> {
  std::vector<Edge_data> edges;
  edges.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    edges.emplace_back(Variable_node{0}, Weighted_value{0.0, Message_weight::standard});
  }
  return edges;
}

auto test_initial_value_and_empty_enabled_edges() -> void {
  Variable_data variable{Weighted_value{2.5, Message_weight::infinite}};

  assert(variable.initial_value().value == 2.5);
  assert(variable.initial_value().weight == Message_weight::infinite);
  assert(variable.value() == 2.5);
  assert(variable.weight() == Message_weight::infinite);
  assert(variable.edges().empty());

  bool threw = false;
  try {
    const auto edge_data = std::vector<Edge_data>{};
    (void)variable.enabled_edges(edge_data);
  } catch (const std::logic_error&) {
    threw = true;
  }
  assert(threw);
}

auto test_enabled_edges_are_filtered_lazily() -> void {
  auto edge_data = make_edge_data(3);
  Variable_data variable{Weighted_value{1.0, Message_weight::standard}};
  variable.add_edge(Graph_edge{0});
  variable.add_edge(Graph_edge{1});
  variable.add_edge(Graph_edge{2});

  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 1, 2}));

  edge_data[1].disable();
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 1, 2}));

  variable.force_enabled_edges_update();
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 2}));
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 2}));
}

auto test_reenabled_edges_do_not_duplicate() -> void {
  auto edge_data = make_edge_data(3);
  Variable_data variable{Weighted_value{1.0, Message_weight::standard}};
  variable.add_edge(Graph_edge{0});
  variable.add_edge(Graph_edge{1});
  variable.add_edge(Graph_edge{2});

  edge_data[1].disable();
  variable.force_enabled_edges_update();
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 2}));

  edge_data[1].reset(Weighted_value{4.0, Message_weight::standard});
  variable.reenable_edge(Graph_edge{1});
  variable.reenable_edge(Graph_edge{1});
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 2, 1}));

  variable.force_enabled_edges_update();
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 1, 2}));
}

auto test_reset_restores_initial_state() -> void {
  auto edge_data = make_edge_data(2);
  Variable_data variable{Weighted_value{3.0, Message_weight::zero}};
  variable.add_edge(Graph_edge{0});
  variable.add_edge(Graph_edge{1});

  edge_data[1].disable();
  variable.force_enabled_edges_update();
  variable.update_value(9.0);
  variable.update_result(Weighted_value{10.0, Message_weight::infinite});

  assert(variable.value() == 10.0);
  assert(variable.weight() == Message_weight::infinite);
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0}));

  variable.reset();
  assert(variable.value() == 3.0);
  assert(variable.weight() == Message_weight::zero);
  assert(edge_indexes(variable.enabled_edges(edge_data)) == std::vector<std::size_t>({0, 1}));
}

auto test_no_enabled_edges_is_explicit() -> void {
  auto edge_data = make_edge_data(2);
  Variable_data variable{Weighted_value{1.0, Message_weight::standard}};
  variable.add_edge(Graph_edge{0});
  variable.add_edge(Graph_edge{1});

  edge_data[0].disable();
  edge_data[1].disable();
  variable.force_enabled_edges_update();

  bool threw = false;
  try {
    (void)variable.enabled_edges(edge_data);
  } catch (const std::logic_error&) {
    threw = true;
  }
  assert(threw);
}

auto test_invalid_edge_reference_is_explicit() -> void {
  auto edge_data = make_edge_data(1);
  Variable_data variable{Weighted_value{1.0, Message_weight::standard}};
  variable.add_edge(Graph_edge{2});

  bool threw = false;
  try {
    (void)variable.enabled_edges(edge_data);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);

  threw = false;
  try {
    variable.reenable_edge(Graph_edge{3});
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);
}

} // namespace

auto main() -> int {
  test_initial_value_and_empty_enabled_edges();
  test_enabled_edges_are_filtered_lazily();
  test_reenabled_edges_do_not_duplicate();
  test_reset_restores_initial_state();
  test_no_enabled_edges_is_explicit();
  test_invalid_edge_reference_is_explicit();
  return 0;
}
