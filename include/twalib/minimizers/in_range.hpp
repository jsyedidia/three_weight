#ifndef TWALIB_MINIMIZERS_IN_RANGE_HPP
#define TWALIB_MINIMIZERS_IN_RANGE_HPP

#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"

namespace twalib {

[[nodiscard]] auto create_in_range_factor(
    Factor_graph& graph,
    Variable_node variable,
    double lower,
    double upper) -> Factor_node;

} // namespace twalib

#endif
