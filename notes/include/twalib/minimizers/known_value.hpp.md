# include/twalib/minimizers/known_value.hpp

## Role In The System

This header declares the public helper for adding a known-value factor to a
`Factor_graph`.

A known-value factor fixes one variable to a specified value with infinite
weight. It is the API for hard evidence: the graph should treat the value as
certain rather than as a soft preference.

The implementation lives in `src/minimizers/known_value.cpp`.

## Main Function

This header declares:

```cpp
create_known_value_factor(Factor_graph& graph, Variable_node variable, double value)
```

The helper creates one edge attached to `variable`, registers a minimizer that
always emits `value` with infinite weight, and returns the new factor's
handle.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_MINIMIZERS_KNOWN_VALUE_HPP
#define TWALIB_MINIMIZERS_KNOWN_VALUE_HPP
```

It prevents duplicate processing of this public minimizer header.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"
```

`Factor_graph` is the graph being modified. `Factor_node` is the handle
returned by the helper. `Variable_node` identifies the variable being fixed.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

The helper is part of the public library API.

### `create_known_value_factor`

The declaration is:

```cpp
[[nodiscard]] auto create_known_value_factor(
    Factor_graph& graph,
    Variable_node variable,
    double value) -> Factor_node;
```

`[[nodiscard]]` asks callers not to ignore the returned `Factor_node`. The
factor is added to the graph even if the handle is ignored, but the handle can
be useful for inspection or dynamic enable/disable operations.

`Factor_graph& graph` is a non-owning reference to the graph that will receive
the factor.

`Variable_node variable` is the handle for the variable to fix.

`double value` is the value the factor will force with infinite weight.

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
- The known value is a hard constraint, represented by infinite weight.
- The returned `Factor_node` belongs to the graph passed into the helper.

## Relationship To The Paper

The known-value factor is a one-variable function cost node with a single
feasible value. It is the general helper for hard external evidence.

## Extension Notes

Do not use this helper for uncertain observations. A probabilistic or soft
observation should use a separate minimizer that expresses a finite preference
instead of an infinite-weight certainty.
