#ifndef TWALIB_MINIMIZERS_KNOWN_VALUE_HPP
#define TWALIB_MINIMIZERS_KNOWN_VALUE_HPP

#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"

namespace twalib {

[[nodiscard]] auto create_known_value_factor(
    Factor_graph& graph,
    Variable_node variable,
    double value) -> Factor_node;

} // namespace twalib

#endif
