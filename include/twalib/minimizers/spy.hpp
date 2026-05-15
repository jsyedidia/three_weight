#ifndef TWALIB_MINIMIZERS_SPY_HPP
#define TWALIB_MINIMIZERS_SPY_HPP

#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"

#include <functional>
#include <optional>

namespace twalib {

using Spy_function = std::function<std::optional<Weighted_value>(Weighted_value)>;

[[nodiscard]] auto create_spy_factor(
    Factor_graph& graph,
    Variable_node variable,
    Spy_function value_function) -> Factor_node;

} // namespace twalib

#endif
