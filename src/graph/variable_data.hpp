#ifndef TWALIB_SRC_GRAPH_VARIABLE_DATA_HPP
#define TWALIB_SRC_GRAPH_VARIABLE_DATA_HPP

#include "graph/edge_data.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <vector>

namespace twalib::detail {

class Variable_data {
 public:
  explicit Variable_data(Weighted_value initial_value)
      : initial_value_(initial_value), value_(initial_value.value), weight_(initial_value.weight) {}

  auto reset() -> void {
    enabled_edges_ = edges_;
    enabled_edges_need_update_ = false;
    value_ = initial_value_.value;
    weight_ = initial_value_.weight;
  }

  auto add_edge(Graph_edge edge) -> void {
    edges_.push_back(edge);
    enabled_edges_.push_back(edge);
    enabled_edges_need_update_ = true;
  }

  auto reenable_edge(Graph_edge edge) -> void {
    const auto known_edge = std::ranges::find_if(edges_, [edge](Graph_edge variable_edge) {
      return variable_edge.index == edge.index;
    });
    if (known_edge == edges_.end()) {
      throw std::out_of_range("Variable_data cannot reenable an edge it does not own");
    }

    if (enabled_edges_need_update_) {
      return;
    }

    const auto already_enabled = std::ranges::find_if(enabled_edges_, [edge](Graph_edge enabled_edge) {
      return enabled_edge.index == edge.index;
    });
    if (already_enabled == enabled_edges_.end()) {
      enabled_edges_.push_back(edge);
    }
  }

  auto force_enabled_edges_update() -> void {
    enabled_edges_need_update_ = true;
  }

  auto update_value(double new_value) -> void {
    value_ = new_value;
  }

  auto update_result(Weighted_value result) -> void {
    value_ = result.value;
    weight_ = result.weight;
  }

  [[nodiscard]] auto initial_value() const -> Weighted_value {
    return initial_value_;
  }

  [[nodiscard]] auto value() const -> double {
    return value_;
  }

  [[nodiscard]] auto weight() const -> Message_weight {
    return weight_;
  }

  [[nodiscard]] auto edges() const -> std::span<const Graph_edge> {
    return edges_;
  }

  auto enabled_edges(std::span<const Edge_data> edge_data) -> std::span<const Graph_edge> {
    if (enabled_edges_need_update_) {
      rebuild_enabled_edges(edge_data);
    }

    if (enabled_edges_.empty()) {
      throw std::logic_error("Variable_data has no enabled edges");
    }

    return enabled_edges_;
  }

 private:
  auto rebuild_enabled_edges(std::span<const Edge_data> edge_data) -> void {
    enabled_edges_.clear();

    for (const Graph_edge edge : edges_) {
      if (!edge.is_valid() || edge.index >= edge_data.size()) {
        throw std::out_of_range("Variable_data references an invalid edge");
      }
      if (edge_data[edge.index].is_enabled()) {
        enabled_edges_.push_back(edge);
      }
    }

    enabled_edges_need_update_ = false;
  }

  Weighted_value initial_value_;
  double value_ = 0.0;
  Message_weight weight_ = Message_weight::standard;
  std::vector<Graph_edge> edges_;
  std::vector<Graph_edge> enabled_edges_;
  bool enabled_edges_need_update_ = false;
};

} // namespace twalib::detail

#endif
