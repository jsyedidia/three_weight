#ifndef TWALIB_SRC_GRAPH_EDGE_DATA_HPP
#define TWALIB_SRC_GRAPH_EDGE_DATA_HPP

#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"

#include <cmath>
#include <optional>

namespace twalib::detail {

class Edge_data {
 public:
  Edge_data(Variable_node variable, Weighted_value initial_value)
      : variable_(variable) {
    reset(initial_value);
  }

  auto reset(Weighted_value reset_value) -> void {
    enabled_ = true;
    x_ = 0.0;
    u_ = 0.0;
    z_ = reset_value.value;
    set_weight_to_left(reset_value.weight);
    set_weight_to_right(Message_weight::zero);
    old_message_to_factor_.reset();
    message_difference_.reset();
  }

  auto disable() -> void {
    enabled_ = false;
  }

  [[nodiscard]] auto variable() const -> Variable_node {
    return variable_;
  }

  [[nodiscard]] auto is_enabled() const -> bool {
    return enabled_;
  }

  [[nodiscard]] auto x() const -> double {
    return x_;
  }

  [[nodiscard]] auto z() const -> double {
    return z_;
  }

  [[nodiscard]] auto u() const -> double {
    return u_;
  }

  [[nodiscard]] auto message_to_factor() const -> double {
    return z_ - u_;
  }

  [[nodiscard]] auto message_to_variable() const -> double {
    return x_ + u_;
  }

  [[nodiscard]] auto weighted_message_to_factor() const -> Weighted_value {
    return Weighted_value{message_to_factor(), weight_to_left()};
  }

  [[nodiscard]] auto weighted_message_to_variable() const -> Weighted_value {
    return Weighted_value{message_to_variable(), weight_to_right()};
  }

  [[nodiscard]] auto message_difference() const -> std::optional<double> {
    return message_difference_;
  }

  auto set_result_from_factor(Weighted_value result) -> void {
    x_ = result.value;
    set_weight_to_right(result.weight);

    const double current_message_to_factor = message_to_factor();
    if (old_message_to_factor_.has_value()) {
      message_difference_ = std::abs(current_message_to_factor - *old_message_to_factor_);
    }
    old_message_to_factor_ = current_message_to_factor;

    if (weight_to_right() == Message_weight::infinite) {
      u_ = 0.0;
    }
  }

  auto set_result_from_variable(Weighted_value result, double alpha, bool reset_disagreement = false) -> void {
    z_ = result.value;
    set_weight_to_left(result.weight);

    if (should_reset_disagreement(reset_disagreement)) {
      u_ = 0.0;
    } else {
      u_ += alpha * (x_ - z_);
    }
  }

 private:
  struct Weight_pair {
    Message_weight left = Message_weight::zero;
    Message_weight right = Message_weight::zero;
  };

  [[nodiscard]] auto weight_to_left() const -> Message_weight {
    return weights_.left;
  }

  [[nodiscard]] auto weight_to_right() const -> Message_weight {
    return weights_.right;
  }

  auto set_weight_to_left(Message_weight weight) -> void {
    weights_.left = weight;
  }

  auto set_weight_to_right(Message_weight weight) -> void {
    weights_.right = weight;
  }

  [[nodiscard]] auto should_reset_disagreement(bool reset_disagreement) const -> bool {
    return reset_disagreement || weight_to_left() == Message_weight::infinite ||
           weight_to_right() == Message_weight::infinite || weight_to_right() == Message_weight::zero;
  }

  Variable_node variable_;
  double x_ = 0.0;
  double u_ = 0.0;
  double z_ = 0.0;
  std::optional<double> old_message_to_factor_;
  std::optional<double> message_difference_;
  Weight_pair weights_;
  bool enabled_ = true;
};

} // namespace twalib::detail

#endif
