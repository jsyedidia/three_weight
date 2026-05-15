#ifndef TWALIB_SRC_GRAPH_FACTOR_DATA_HPP
#define TWALIB_SRC_GRAPH_FACTOR_DATA_HPP

#include "graph/edge_data.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace twalib::detail {

class Factor_data {
 public:
  Factor_data(std::span<const Graph_edge> edges, Minimization_function minimization_function)
      : minimization_function_(std::move(minimization_function)) {
    exchanges_.reserve(edges.size());
    incoming_infinite_weights_.reserve(edges.size());
    for (const Graph_edge edge : edges) {
      exchanges_.emplace_back(edge);
      incoming_infinite_weights_.push_back(false);
    }
  }

  auto reset() -> void {
    enabled_ = true;
  }

  auto enable() -> bool {
    return switch_enabled(true);
  }

  auto disable() -> bool {
    return switch_enabled(false);
  }

  [[nodiscard]] auto is_enabled() const -> bool {
    return enabled_;
  }

  [[nodiscard]] auto exchanges() const -> std::span<const Weighted_value_exchange> {
    return exchanges_;
  }

  auto minimize(std::span<Edge_data> edge_data, Random_engine& random) -> void {
    if (!enabled_) {
      return;
    }

    for (std::size_t index = 0; index < exchanges_.size(); ++index) {
      Weighted_value_exchange& exchange = exchanges_[index];
      const Weighted_value incoming = edge_for(exchange, edge_data).weighted_message_to_factor();
      incoming_infinite_weights_[index] = incoming.weight == Message_weight::infinite;
      exchange.set(incoming);
    }

    minimization_function_(exchanges_, random);

    for (std::size_t index = 0; index < exchanges_.size(); ++index) {
      const Weighted_value_exchange exchange = exchanges_[index];
      Weighted_value result = exchange.get();
      if (incoming_infinite_weights_[index]) {
        result.weight = Message_weight::infinite;
      }
      edge_for(exchange, edge_data).set_result_from_factor(result);
    }
  }

 private:
  static auto edge_for(Weighted_value_exchange exchange, std::span<Edge_data> edge_data) -> Edge_data& {
    const Graph_edge edge = exchange.edge();
    if (!edge.is_valid() || edge.index >= edge_data.size()) {
      throw std::out_of_range("Factor_data references an invalid edge");
    }
    return edge_data[edge.index];
  }

  auto switch_enabled(bool enabled) -> bool {
    if (enabled_ == enabled) {
      return false;
    }

    enabled_ = enabled;
    return true;
  }

  Minimization_function minimization_function_;
  std::vector<Weighted_value_exchange> exchanges_;
  std::vector<bool> incoming_infinite_weights_;
  bool enabled_ = true;
};

} // namespace twalib::detail

#endif
