#include "twalib/minimizers/one_hot.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace twalib {
namespace {

auto set_all(std::span<Weighted_value_exchange> exchanges, Weighted_value weighted_value) -> void {
  for (auto& exchange : exchanges) {
    exchange.set(weighted_value);
  }
}

auto choose_uniform(std::span<const std::size_t> candidates, Random_engine& random) -> std::size_t {
  if (candidates.empty()) {
    throw std::logic_error("one_hot tie set is empty");
  }

  std::uniform_int_distribution<std::size_t> distribution{0, candidates.size() - 1};
  return candidates[distribution(random)];
}

auto create_one_hot_minimizer(std::size_t num_variables) -> Minimization_function {
  return [num_variables](std::span<Weighted_value_exchange> exchanges, Random_engine& random) {
    std::vector<std::size_t> infinite_one_indices;
    std::optional<std::size_t> only_non_certain_zero_index;
    std::size_t num_infinite_zero = 0;
    std::vector<std::size_t> biggest_standard_indices;
    double biggest_standard_value = 0.0;
    std::vector<std::size_t> biggest_zero_indices;
    double biggest_zero_value = 0.0;

    for (std::size_t index = 0; index < exchanges.size(); ++index) {
      const Weighted_value incoming = exchanges[index].get();

      if (incoming.weight == Message_weight::infinite) {
        if (incoming.value == 0.0) {
          ++num_infinite_zero;
        } else {
          infinite_one_indices.push_back(index);
        }
      } else {
        only_non_certain_zero_index = index;

        if (incoming.weight == Message_weight::zero) {
          if (biggest_zero_indices.empty() || incoming.value > biggest_zero_value) {
            biggest_zero_value = incoming.value;
            biggest_zero_indices.clear();
            biggest_zero_indices.push_back(index);
          } else if (incoming.value == biggest_zero_value) {
            biggest_zero_indices.push_back(index);
          }
        } else if (biggest_standard_indices.empty() || incoming.value > biggest_standard_value) {
          biggest_standard_value = incoming.value;
          biggest_standard_indices.clear();
          biggest_standard_indices.push_back(index);
        } else if (incoming.value == biggest_standard_value) {
          biggest_standard_indices.push_back(index);
        }
      }
    }

    Message_weight outgoing_weight = Message_weight::standard;
    std::optional<std::size_t> one_index;

    if (!infinite_one_indices.empty()) {
      outgoing_weight = Message_weight::infinite;
      one_index = choose_uniform(infinite_one_indices, random);
    } else if (num_infinite_zero == num_variables - 1) {
      outgoing_weight = Message_weight::infinite;
      one_index = only_non_certain_zero_index;
    } else if (!biggest_standard_indices.empty()) {
      one_index = choose_uniform(biggest_standard_indices, random);
    } else if (!biggest_zero_indices.empty()) {
      one_index = choose_uniform(biggest_zero_indices, random);
    }

    if (!one_index.has_value()) {
      throw std::logic_error("one_hot constraint has no feasible non-certain-zero variable");
    }

    set_all(exchanges, Weighted_value{0.0, outgoing_weight});
    exchanges[*one_index].set(Weighted_value{1.0, outgoing_weight});
  };
}

} // namespace

auto create_one_hot_factor(
    Factor_graph& graph,
    std::span<const Variable_node> variables) -> Factor_node {
  if (variables.empty()) {
    throw std::invalid_argument("create_one_hot_factor requires at least one variable");
  }

  std::vector<Graph_edge> edges;
  edges.reserve(variables.size());
  for (const Variable_node variable : variables) {
    edges.push_back(graph.create_edge(variable));
  }

  return graph.create_factor(edges, create_one_hot_minimizer(variables.size()));
}

auto create_one_hot_factor(
    Factor_graph& graph,
    std::initializer_list<Variable_node> variables) -> Factor_node {
  return create_one_hot_factor(
      graph,
      std::span<const Variable_node>{variables.begin(), variables.size()});
}

} // namespace twalib
