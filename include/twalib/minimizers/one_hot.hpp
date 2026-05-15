#ifndef TWALIB_MINIMIZERS_ONE_HOT_HPP
#define TWALIB_MINIMIZERS_ONE_HOT_HPP

#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"

#include <initializer_list>
#include <span>

namespace twalib {

[[nodiscard]] auto create_one_hot_factor(
    Factor_graph& graph,
    std::span<const Variable_node> variables) -> Factor_node;

[[nodiscard]] auto create_one_hot_factor(
    Factor_graph& graph,
    std::initializer_list<Variable_node> variables) -> Factor_node;

} // namespace twalib

#endif
