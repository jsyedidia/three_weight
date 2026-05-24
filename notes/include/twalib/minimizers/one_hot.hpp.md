# include/twalib/minimizers/one_hot.hpp

## Role In The System

This header declares the public helper for adding a one-hot factor to a
`Factor_graph`.

A one-hot factor connects to a group of variables and enforces the constraint
that exactly one variable should have value `1` while the rest should have
value `0`. Sudoku uses this kind of factor heavily: for example, each cell
must choose one digit, each row-digit pair must choose one cell, and similarly
for columns and boxes.

The implementation lives in `src/minimizers/one_hot.cpp`. This header is the
small public entry point that application code includes when it wants to add
one-hot constraints.

## Main Functions

This header declares two overloads:

```cpp
create_one_hot_factor(Factor_graph& graph, std::span<const Variable_node> variables)
create_one_hot_factor(Factor_graph& graph, std::initializer_list<Variable_node> variables)
```

Both create edges from the factor to the supplied variables, register the
factor's minimization function with the graph, and return a `Factor_node`
handle.

The two overloads differ only in how the variable list is supplied. The
`std::span` overload is convenient when the variables already live in a
container. The `std::initializer_list` overload is convenient for short
brace-enclosed calls.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_MINIMIZERS_ONE_HOT_HPP
#define TWALIB_MINIMIZERS_ONE_HOT_HPP
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
returned by the helper. `Variable_node` is the handle type supplied by the
caller.

### Standard Includes

The standard includes are:

```cpp
#include <initializer_list>
#include <span>
```

`std::initializer_list` supports calls with brace-enclosed variable lists.
`std::span` supports a non-owning view of variables stored elsewhere.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

The minimizer helper is part of the public library API.

### `create_one_hot_factor` Span Overload

The primary declaration is:

```cpp
[[nodiscard]] auto create_one_hot_factor(
    Factor_graph& graph,
    std::span<const Variable_node> variables) -> Factor_node;
```

`[[nodiscard]]` asks callers not to ignore the returned `Factor_node`. The
factor is added to the graph regardless, but the handle is useful for later
inspection or for enabling and disabling dynamic factors.

`Factor_graph& graph` is a non-owning reference to the graph that will receive
the new factor. The helper mutates the graph by creating edges and registering
the factor.

`std::span<const Variable_node> variables` is a read-only view of the variable
handles that participate in the one-hot constraint. The span does not own the
variables. It only lets the function read the handles during the call.

The trailing return type:

```cpp
-> Factor_node
```

says the helper returns the handle for the newly created factor.

### `create_one_hot_factor` Initializer-List Overload

The convenience overload is:

```cpp
[[nodiscard]] auto create_one_hot_factor(
    Factor_graph& graph,
    std::initializer_list<Variable_node> variables) -> Factor_node;
```

This lets callers write:

```cpp
create_one_hot_factor(graph, {a, b, c});
```

instead of first constructing a separate container. The implementation simply
wraps the initializer list in a span and forwards to the primary overload.

### Closing The File

The namespace and include guard close with:

```cpp
} // namespace twalib

#endif
```

## Important Invariants

- The variable list must be nonempty. The implementation checks this and throws
  `std::invalid_argument` otherwise.
- The helper creates one graph edge per supplied variable.
- The returned `Factor_node` belongs to the same graph passed into the helper.
- The one-hot minimizer uses the graph's shared random engine for tie-breaking,
  so seeded runs are reproducible.

## Relationship To The Paper

The one-hot factor implements the simplex-style choice constraint used in the
paper's Sudoku construction. In paper language, it is a function cost node
whose feasible states have exactly one incident edge set to `1`.

## Extension Notes

Keep this header as a declaration-only API surface. Algorithmic details such
as tie-breaking, certainty propagation, and infeasible-message handling belong
in `src/minimizers/one_hot.cpp` and the shared graph machinery.

For large collections of variables, prefer the `std::span` overload so callers
can pass an existing vector or array without creating a temporary list.
