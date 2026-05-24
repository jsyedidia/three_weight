# include/twalib/minimizers/in_range.hpp

## Role In The System

This header declares the public helper for adding an in-range factor to a
`Factor_graph`.

An in-range factor constrains one variable to stay inside an inclusive
interval. If the variable is already inside the interval, the factor sends a
zero-weight message. If the variable is outside, the factor clamps it to the
nearest endpoint with standard weight.

The implementation lives in `src/minimizers/in_range.cpp`.

## Main Function

This header declares:

```cpp
create_in_range_factor(Factor_graph& graph, Variable_node variable, double lower, double upper)
```

The helper creates one edge attached to `variable`, registers the range
minimizer as a factor, and returns the new factor's handle.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_MINIMIZERS_IN_RANGE_HPP
#define TWALIB_MINIMIZERS_IN_RANGE_HPP
```

It prevents duplicate processing of this public minimizer header.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"
```

`Factor_graph` is the graph that will receive the factor. `Factor_node` is the
handle returned to the caller. `Variable_node` identifies the variable being
constrained.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

The helper is part of the public library API.

### `create_in_range_factor`

The declaration is:

```cpp
[[nodiscard]] auto create_in_range_factor(
    Factor_graph& graph,
    Variable_node variable,
    double lower,
    double upper) -> Factor_node;
```

`[[nodiscard]]` asks callers not to ignore the returned `Factor_node`. The
factor is still added to the graph if the handle is ignored, but the handle is
useful for inspection or dynamic enable/disable operations.

`Factor_graph& graph` is a non-owning reference to the graph being modified.

`Variable_node variable` is the handle for the variable to constrain.

`double lower` and `double upper` are the inclusive interval bounds. The
implementation requires `lower <= upper` and throws `std::invalid_argument` if
the bounds are inverted.

The trailing return type:

```cpp
-> Factor_node
```

says the helper returns the handle for the newly created factor.

### Closing The File

The namespace and include guard close with:

```cpp
} // namespace twalib

#endif
```

## Important Invariants

- The helper creates one factor edge attached to the supplied variable.
- The interval is inclusive.
- `lower` must be less than or equal to `upper`.
- The returned `Factor_node` belongs to the graph passed into the helper.

## Relationship To The Paper

The in-range factor is a simple one-variable function cost node. It projects
the incoming value onto the feasible interval and uses the three-weight system
to say whether the constraint is active.

## Extension Notes

Use this helper for independent scalar bounds, such as circle-packing boundary
constraints. For constraints that couple multiple variables, create a separate
minimizer rather than hiding multi-variable behavior behind this one-variable
API.
