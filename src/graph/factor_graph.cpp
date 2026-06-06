#include "twalib/graph/factor_graph.hpp"

#include "graph/edge_data.hpp"
#include "graph/factor_data.hpp"
#include "graph/variable_data.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <utility>

namespace twalib {

class Factor_graph::Impl {
 public:
  Impl(double learning_rate, double convergence_delta, std::uint64_t random_seed)
      : learning_rate_(learning_rate),
        convergence_delta_(convergence_delta),
        random_(random_seed) {}

  [[nodiscard]] auto learning_rate() const -> double {
    return learning_rate_;
  }

  auto set_learning_rate(double learning_rate) -> void {
    learning_rate_ = learning_rate;
  }

  [[nodiscard]] auto convergence_delta() const -> double {
    return convergence_delta_;
  }

  auto set_convergence_delta(double convergence_delta) -> void {
    convergence_delta_ = convergence_delta;
  }

  auto set_random_seed(std::uint64_t random_seed) -> void {
    random_.seed(random_seed);
  }

  [[nodiscard]] auto iterations() const -> std::size_t {
    return iterations_;
  }

  [[nodiscard]] auto converged() const -> bool {
    return converged_;
  }

  [[nodiscard]] auto num_variables() const -> std::size_t {
    return variables_.size();
  }

  [[nodiscard]] auto num_edges() const -> std::size_t {
    return edges_.size();
  }

  [[nodiscard]] auto num_enabled_edges() const -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(edges_, [](const detail::Edge_data& edge) {
      return edge.is_enabled();
    }));
  }

  [[nodiscard]] auto num_factors() const -> std::size_t {
    return factors_.size();
  }

  [[nodiscard]] auto num_enabled_factors() const -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(factors_, [](const detail::Factor_data& factor) {
      return factor.is_enabled();
    }));
  }

  [[nodiscard]] auto max_message_difference() const -> std::optional<double> {
    double max_difference = 0.0;

    for (const auto& edge : edges_) {
      if (!edge.is_enabled()) {
        continue;
      }

      const auto difference = edge.message_difference();
      if (!difference.has_value()) {
        return std::nullopt;
      }
      max_difference = std::max(max_difference, *difference);
    }

    return max_difference;
  }

  [[nodiscard]] auto create_variable(double initial_value, Message_weight initial_weight) -> Variable_node {
    const auto variable = Variable_node{variables_.size()};
    variables_.emplace_back(Weighted_value{initial_value, initial_weight});
    return variable;
  }

  [[nodiscard]] auto create_edge(Variable_node variable) -> Graph_edge {
    validate_variable(variable);

    const auto edge = Graph_edge{edges_.size()};
    edges_.emplace_back(variable, variables_[variable.index].initial_value());
    variables_[variable.index].add_edge(edge);
    return edge;
  }

  [[nodiscard]] auto create_factor(
      std::span<const Graph_edge> factor_edges,
      Minimization_function minimization_function) -> Factor_node {
    for (const Graph_edge edge : factor_edges) {
      validate_edge(edge);
    }

    const auto factor = Factor_node{factors_.size()};
    factors_.emplace_back(factor_edges, std::move(minimization_function));
    return factor;
  }

  [[nodiscard]] auto value(Variable_node variable) const -> double {
    validate_variable(variable);
    return variables_[variable.index].value();
  }

  [[nodiscard]] auto weight(Variable_node variable) const -> Message_weight {
    validate_variable(variable);
    return variables_[variable.index].weight();
  }

  [[nodiscard]] auto is_factor_enabled(Factor_node factor) const -> bool {
    validate_factor(factor);
    return factors_[factor.index].is_enabled();
  }

  auto set_factor_enabled(Factor_node factor, bool enabled) -> void {
    validate_factor(factor);

    if (enabled) {
      enable_factor(factor.index);
    } else {
      disable_factor(factor.index);
    }
  }

  auto iterate() -> bool {
    return iterate_with_satisfaction([] {
      return true;
    });
  }

  auto iterate_with_satisfaction(const std::function<bool()>& is_satisfied) -> bool {
    if (converged_) {
      return true;
    }

    for (auto& factor : factors_) {
      factor.minimize(edges_, random_);
    }

    bool beliefs_converged = true;
    for (auto& variable : variables_) {
      const auto enabled_edges = variable.enabled_edges(edges_);
      const Weighted_value result = enforce_variable_equality(enabled_edges);
      const bool has_lone_standard_message = has_lone_standard_message_to_variable(enabled_edges);
      beliefs_converged = beliefs_converged && belief_converged(variable.value(), result.value);
      variable.update_result(result);

      for (const Graph_edge edge : enabled_edges) {
        const bool reset_disagreement =
            has_lone_standard_message &&
            edges_[edge.index].weighted_message_to_variable().weight == Message_weight::standard;
        edges_[edge.index].set_result_from_variable(result, learning_rate_, reset_disagreement);
      }
    }

    ++iterations_;
    converged_ = beliefs_converged;

    for (const auto& callback : iteration_callbacks_) {
      callback();
    }

    if (converged_ && !is_satisfied()) {
      converged_ = false;
    }

    return converged_;
  }

  auto iterate_until_converged(std::size_t max_iterations) -> bool {
    for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
      if (iterate()) {
        return true;
      }
    }

    return converged_;
  }

  auto iterate_until_satisfied(
      std::size_t max_iterations,
      const std::function<bool()>& is_satisfied) -> bool {
    if (converged_) {
      if (is_satisfied()) {
        return true;
      }
      converged_ = false;
    }

    for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
      if (iterate_with_satisfaction(is_satisfied)) {
        return true;
      }
    }

    return converged_;
  }

  auto reinitialize() -> void {
    for (auto& variable : variables_) {
      variable.reset();
      for (const Graph_edge edge : variable.edges()) {
        edges_[edge.index].reset(variable.initial_value());
      }
    }

    for (auto& factor : factors_) {
      factor.reset();
    }

    iterations_ = 0;
    converged_ = false;

    for (const auto& callback : reinitialize_callbacks_) {
      callback();
    }
  }

  auto add_iteration_callback(std::function<void()> callback) -> void {
    iteration_callbacks_.push_back(std::move(callback));
  }

  auto add_reinitialize_callback(std::function<void()> callback) -> void {
    reinitialize_callbacks_.push_back(std::move(callback));
  }

 private:
  auto validate_variable(Variable_node variable) const -> void {
    if (!variable.is_valid() || variable.index >= variables_.size()) {
      throw std::out_of_range("Factor_graph references an invalid variable");
    }
  }

  auto validate_edge(Graph_edge edge) const -> void {
    if (!edge.is_valid() || edge.index >= edges_.size()) {
      throw std::out_of_range("Factor_graph references an invalid edge");
    }
  }

  auto validate_factor(Factor_node factor) const -> void {
    if (!factor.is_valid() || factor.index >= factors_.size()) {
      throw std::out_of_range("Factor_graph references an invalid factor");
    }
  }

  auto enable_factor(std::size_t factor_index) -> void {
    if (!factors_[factor_index].enable()) {
      return;
    }

    for (const auto& exchange : factors_[factor_index].exchanges()) {
      const Graph_edge edge = exchange.edge();
      const Variable_node variable = edges_[edge.index].variable();
      edges_[edge.index].reset(Weighted_value{variables_[variable.index].value(), Message_weight::standard});
      variables_[variable.index].reenable_edge(edge);
    }

    converged_ = false;
  }

  auto disable_factor(std::size_t factor_index) -> void {
    if (!factors_[factor_index].disable()) {
      return;
    }

    for (const auto& exchange : factors_[factor_index].exchanges()) {
      const Graph_edge edge = exchange.edge();
      const Variable_node variable = edges_[edge.index].variable();
      edges_[edge.index].disable();
      variables_[variable.index].force_enabled_edges_update();
    }

    converged_ = false;
  }

  [[nodiscard]] auto enforce_variable_equality(std::span<const Graph_edge> enabled_edges) const -> Weighted_value {
    double all_sum = 0.0;
    double standard_sum = 0.0;
    std::size_t standard_count = 0;

    for (const Graph_edge edge : enabled_edges) {
      const Weighted_value message = edges_[edge.index].weighted_message_to_variable();
      if (message.weight == Message_weight::infinite) {
        return message;
      }
      if (message.weight == Message_weight::standard) {
        standard_sum += message.value;
        ++standard_count;
      }
      all_sum += message.value;
    }

    if (standard_count > 0) {
      return Weighted_value{
          standard_sum / static_cast<double>(standard_count),
          Message_weight::standard};
    }

    return Weighted_value{
        all_sum / static_cast<double>(enabled_edges.size()),
        Message_weight::zero};
  }

  [[nodiscard]] auto has_lone_standard_message_to_variable(std::span<const Graph_edge> enabled_edges) const -> bool {
    std::size_t standard_count = 0;
    for (const Graph_edge edge : enabled_edges) {
      const Message_weight weight = edges_[edge.index].weighted_message_to_variable().weight;
      if (weight == Message_weight::infinite) {
        return false;
      }
      if (weight == Message_weight::standard) {
        ++standard_count;
      }
    }

    return standard_count == 1;
  }

  [[nodiscard]] auto belief_converged(double old_value, double new_value) const -> bool {
    return std::abs(old_value - new_value) <= convergence_delta_;
  }

  double learning_rate_;
  double convergence_delta_;
  Random_engine random_;
  std::size_t iterations_ = 0;
  bool converged_ = false;
  std::vector<detail::Variable_data> variables_;
  std::vector<detail::Edge_data> edges_;
  std::vector<detail::Factor_data> factors_;
  std::vector<std::function<void()>> iteration_callbacks_;
  std::vector<std::function<void()>> reinitialize_callbacks_;
};

Factor_graph::Factor_graph(
    double learning_rate,
    double convergence_delta,
    std::uint64_t random_seed)
    : impl_(std::make_unique<Impl>(learning_rate, convergence_delta, random_seed)) {}

Factor_graph::~Factor_graph() = default;

Factor_graph::Factor_graph(Factor_graph&&) noexcept = default;

auto Factor_graph::operator=(Factor_graph&&) noexcept -> Factor_graph& = default;

auto Factor_graph::learning_rate() const -> double {
  return impl_->learning_rate();
}

auto Factor_graph::set_learning_rate(double learning_rate) -> void {
  impl_->set_learning_rate(learning_rate);
}

auto Factor_graph::convergence_delta() const -> double {
  return impl_->convergence_delta();
}

auto Factor_graph::set_convergence_delta(double convergence_delta) -> void {
  impl_->set_convergence_delta(convergence_delta);
}

auto Factor_graph::set_random_seed(std::uint64_t random_seed) -> void {
  impl_->set_random_seed(random_seed);
}

auto Factor_graph::iterations() const -> std::size_t {
  return impl_->iterations();
}

auto Factor_graph::converged() const -> bool {
  return impl_->converged();
}

auto Factor_graph::num_variables() const -> std::size_t {
  return impl_->num_variables();
}

auto Factor_graph::num_edges() const -> std::size_t {
  return impl_->num_edges();
}

auto Factor_graph::num_enabled_edges() const -> std::size_t {
  return impl_->num_enabled_edges();
}

auto Factor_graph::num_factors() const -> std::size_t {
  return impl_->num_factors();
}

auto Factor_graph::num_enabled_factors() const -> std::size_t {
  return impl_->num_enabled_factors();
}

auto Factor_graph::max_message_difference() const -> std::optional<double> {
  return impl_->max_message_difference();
}

auto Factor_graph::create_variable(double initial_value, Message_weight initial_weight) -> Variable_node {
  return impl_->create_variable(initial_value, initial_weight);
}

auto Factor_graph::create_edge(Variable_node variable) -> Graph_edge {
  return impl_->create_edge(variable);
}

auto Factor_graph::create_factor(
    std::span<const Graph_edge> edges,
    Minimization_function minimization_function) -> Factor_node {
  return impl_->create_factor(edges, std::move(minimization_function));
}

auto Factor_graph::create_factor(
    std::initializer_list<Graph_edge> edges,
    Minimization_function minimization_function) -> Factor_node {
  return impl_->create_factor(
      std::span<const Graph_edge>{edges.begin(), edges.size()},
      std::move(minimization_function));
}

auto Factor_graph::value(Variable_node variable) const -> double {
  return impl_->value(variable);
}

auto Factor_graph::weight(Variable_node variable) const -> Message_weight {
  return impl_->weight(variable);
}

auto Factor_graph::is_factor_enabled(Factor_node factor) const -> bool {
  return impl_->is_factor_enabled(factor);
}

auto Factor_graph::set_factor_enabled(Factor_node factor, bool enabled) -> void {
  impl_->set_factor_enabled(factor, enabled);
}

auto Factor_graph::iterate() -> bool {
  return impl_->iterate();
}

auto Factor_graph::iterate_until_converged(std::size_t max_iterations) -> bool {
  return impl_->iterate_until_converged(max_iterations);
}

auto Factor_graph::iterate_until_satisfied(
    std::size_t max_iterations,
    std::function<bool(const Factor_graph&)> is_satisfied) -> bool {
  return impl_->iterate_until_satisfied(
      max_iterations,
      [this, is_satisfied = std::move(is_satisfied)] {
        return is_satisfied(*this);
      });
}

auto Factor_graph::reinitialize() -> void {
  impl_->reinitialize();
}

auto Factor_graph::add_iteration_callback(std::function<void()> callback) -> void {
  impl_->add_iteration_callback(std::move(callback));
}

auto Factor_graph::add_reinitialize_callback(std::function<void()> callback) -> void {
  impl_->add_reinitialize_callback(std::move(callback));
}

} // namespace twalib
