#ifndef TWALIB_GRAPH_FACTOR_GRAPH_HPP
#define TWALIB_GRAPH_FACTOR_GRAPH_HPP

#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>

namespace twalib {

class Factor_graph {
 public:
  explicit Factor_graph(
      double learning_rate = 1.0,
      double convergence_delta = 1e-5,
      std::uint64_t random_seed = 0);
  ~Factor_graph();

  Factor_graph(const Factor_graph&) = delete;
  auto operator=(const Factor_graph&) -> Factor_graph& = delete;
  Factor_graph(Factor_graph&&) noexcept;
  auto operator=(Factor_graph&&) noexcept -> Factor_graph&;

  [[nodiscard]] auto learning_rate() const -> double;
  auto set_learning_rate(double learning_rate) -> void;
  [[nodiscard]] auto convergence_delta() const -> double;
  auto set_convergence_delta(double convergence_delta) -> void;
  auto set_random_seed(std::uint64_t random_seed) -> void;

  [[nodiscard]] auto iterations() const -> std::size_t;
  [[nodiscard]] auto converged() const -> bool;

  [[nodiscard]] auto num_variables() const -> std::size_t;
  [[nodiscard]] auto num_edges() const -> std::size_t;
  [[nodiscard]] auto num_enabled_edges() const -> std::size_t;
  [[nodiscard]] auto num_factors() const -> std::size_t;
  [[nodiscard]] auto num_enabled_factors() const -> std::size_t;

  [[nodiscard]] auto create_variable(
      double initial_value,
      Message_weight initial_weight = Message_weight::standard) -> Variable_node;
  [[nodiscard]] auto create_edge(Variable_node variable) -> Graph_edge;
  [[nodiscard]] auto create_factor(
      std::span<const Graph_edge> edges,
      Minimization_function minimization_function) -> Factor_node;
  [[nodiscard]] auto create_factor(
      std::initializer_list<Graph_edge> edges,
      Minimization_function minimization_function) -> Factor_node;

  [[nodiscard]] auto value(Variable_node variable) const -> double;
  [[nodiscard]] auto weight(Variable_node variable) const -> Message_weight;
  [[nodiscard]] auto is_factor_enabled(Factor_node factor) const -> bool;
  auto set_factor_enabled(Factor_node factor, bool enabled) -> void;

  auto iterate() -> bool;
  auto iterate_until_converged(std::size_t max_iterations) -> bool;
  auto reinitialize() -> void;

  auto add_iteration_callback(std::function<void()> callback) -> void;
  auto add_reinitialize_callback(std::function<void()> callback) -> void;

 private:
  class Impl;

  std::unique_ptr<Impl> impl_;
};

} // namespace twalib

#endif
