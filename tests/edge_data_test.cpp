#include "graph/edge_data.hpp"

#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"

#include <cassert>
#include <cmath>

namespace {

using twalib::Message_weight;
using twalib::Variable_node;
using twalib::Weighted_value;
using twalib::detail::Edge_data;

auto nearly_equal(double left, double right) -> bool {
  return std::abs(left - right) < 1e-12;
}

auto test_reset_and_messages() -> void {
  Edge_data edge{Variable_node{4}, Weighted_value{2.0, Message_weight::infinite}};

  assert(edge.variable().index == 4);
  assert(edge.is_enabled());
  assert(edge.x() == 0.0);
  assert(edge.u() == 0.0);
  assert(edge.z() == 2.0);
  assert(edge.message_to_factor() == 2.0);
  assert(edge.message_to_variable() == 0.0);
  assert(edge.weighted_message_to_factor().weight == Message_weight::infinite);
  assert(edge.weighted_message_to_variable().weight == Message_weight::zero);
  assert(!edge.message_difference().has_value());
}

auto test_factor_and_variable_updates() -> void {
  Edge_data edge{Variable_node{1}, Weighted_value{2.0, Message_weight::standard}};

  edge.set_result_from_factor(Weighted_value{5.0, Message_weight::infinite});
  assert(edge.x() == 5.0);
  assert(edge.u() == 0.0);
  assert(edge.weighted_message_to_variable().value == 5.0);
  assert(edge.weighted_message_to_variable().weight == Message_weight::infinite);
  assert(!edge.message_difference().has_value());

  edge.set_result_from_variable(Weighted_value{3.0, Message_weight::standard}, 0.25);
  assert(edge.z() == 3.0);
  assert(edge.u() == 0.0);
  assert(edge.message_to_factor() == 3.0);
  assert(edge.weighted_message_to_factor().weight == Message_weight::standard);

  edge.set_result_from_factor(Weighted_value{6.0, Message_weight::standard});
  assert(edge.x() == 6.0);
  assert(edge.weighted_message_to_variable().weight == Message_weight::standard);
  assert(edge.message_difference().has_value());
  assert(nearly_equal(*edge.message_difference(), 1.0));

  edge.set_result_from_variable(Weighted_value{4.0, Message_weight::standard}, 0.25);
  assert(edge.z() == 4.0);
  assert(nearly_equal(edge.u(), 0.5));
  assert(nearly_equal(edge.message_to_factor(), 3.5));

  edge.set_result_from_variable(Weighted_value{4.0, Message_weight::infinite}, 0.25);
  assert(edge.z() == 4.0);
  assert(edge.u() == 0.0);
  assert(edge.weighted_message_to_factor().weight == Message_weight::infinite);
  assert(edge.message_to_factor() == 4.0);
}

auto test_zero_weight_to_variable_resets_disagreement() -> void {
  Edge_data edge{Variable_node{1}, Weighted_value{2.0, Message_weight::standard}};

  edge.set_result_from_factor(Weighted_value{7.0, Message_weight::standard});
  edge.set_result_from_variable(Weighted_value{3.0, Message_weight::standard}, 0.5);
  assert(nearly_equal(edge.u(), 2.0));

  edge.set_result_from_factor(Weighted_value{5.0, Message_weight::zero});
  edge.set_result_from_variable(Weighted_value{4.0, Message_weight::standard}, 0.5);
  assert(edge.u() == 0.0);
  assert(edge.message_to_factor() == 4.0);
  assert(edge.weighted_message_to_variable().weight == Message_weight::zero);
}

auto test_lone_standard_message_can_reset_disagreement() -> void {
  Edge_data edge{Variable_node{1}, Weighted_value{2.0, Message_weight::standard}};

  edge.set_result_from_factor(Weighted_value{7.0, Message_weight::standard});
  edge.set_result_from_variable(Weighted_value{3.0, Message_weight::standard}, 0.5);
  assert(nearly_equal(edge.u(), 2.0));

  edge.set_result_from_factor(Weighted_value{8.0, Message_weight::standard});
  edge.set_result_from_variable(Weighted_value{10.0, Message_weight::standard}, 0.5, true);
  assert(edge.u() == 0.0);
  assert(edge.message_to_factor() == 10.0);
  assert(edge.weighted_message_to_variable().weight == Message_weight::standard);
}

auto test_disable_and_reset() -> void {
  Edge_data edge{Variable_node{3}, Weighted_value{1.0, Message_weight::standard}};

  edge.disable();
  assert(!edge.is_enabled());

  edge.set_result_from_factor(Weighted_value{7.0, Message_weight::standard});
  edge.set_result_from_variable(Weighted_value{2.0, Message_weight::standard}, 0.5);
  assert(edge.u() != 0.0);

  edge.reset(Weighted_value{9.0, Message_weight::zero});
  assert(edge.is_enabled());
  assert(edge.x() == 0.0);
  assert(edge.u() == 0.0);
  assert(edge.z() == 9.0);
  assert(edge.weighted_message_to_factor().weight == Message_weight::zero);
  assert(edge.weighted_message_to_variable().weight == Message_weight::zero);
  assert(!edge.message_difference().has_value());
}

} // namespace

auto main() -> int {
  test_reset_and_messages();
  test_factor_and_variable_updates();
  test_zero_weight_to_variable_resets_disagreement();
  test_lone_standard_message_can_reset_disagreement();
  test_disable_and_reset();
  return 0;
}
