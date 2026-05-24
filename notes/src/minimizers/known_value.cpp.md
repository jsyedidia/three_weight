# src/minimizers/known_value.cpp

## Role In The System

`known_value.cpp` implements the known-value minimizer. A known-value factor
connects to one variable and asserts that the variable's value is known with
certainty.

This is the minimizer to use whenever a graph needs hard external evidence:
the connected variable is forced to the supplied value with infinite weight.

The minimizer always sends the same outgoing message:

```text
value = the known value
weight = infinite
```

It does not depend on the incoming message.

## Code Walkthrough

### Own Header Include

The file starts with:

```cpp
#include "twalib/minimizers/known_value.hpp"
```

Including the matching public header first helps ensure the declaration and
definition stay consistent.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
```

`Graph_edge` is the handle for the edge created by this helper.
`Weighted_value` is the value/weight pair written by the minimizer.
`Weighted_value_exchange` is the per-edge exchange object passed to the
minimization lambda. It also brings in the `Random_engine` type used by the
standard minimizer signature.

### Standard Includes

The only standard include is:

```cpp
#include <span>
```

`std::span` is the exchange-buffer view type used by every
`Minimization_function`.

### Namespace

The implementation lives in:

```cpp
namespace twalib {
```

This matches the public declaration in the header.

### `create_known_value_factor`

The function begins:

```cpp
auto create_known_value_factor(
    Factor_graph& graph,
    Variable_node variable,
    double value) -> Factor_node {
```

`graph` is the factor graph being modified. `variable` is the variable that
should be fixed. `value` is the numeric value to force.

The function returns a `Factor_node` handle for the created factor.

### Edge Creation

The helper creates one edge:

```cpp
  const Graph_edge edge = graph.create_edge(variable);
```

The new edge connects the soon-to-be-created factor to the supplied variable.
Known-value factors are unary factors, so they need only one edge.

### Factor Creation And Lambda

The function returns the newly created factor:

```cpp
  return graph.create_factor({edge}, [value](std::span<Weighted_value_exchange> exchanges, Random_engine&) {
```

The brace list `{edge}` gives the factor its single edge.

The lambda captures:

```cpp
[value]
```

which stores a copy of the known value inside the minimizer. That copy remains
available whenever the graph runs the factor pass.

The lambda parameters are:

```cpp
std::span<Weighted_value_exchange> exchanges, Random_engine&
```

`exchanges` is the non-owning view of edge exchanges for this factor. Because
this factor has one edge, it uses `exchanges[0]`.

The random engine parameter is unnamed because this minimizer never makes a
random choice. It is present because all minimization functions have the same
signature.

### Writing The Certain Value

The lambda body is one line:

```cpp
    exchanges[0].set(Weighted_value{value, Message_weight::infinite});
```

It writes the known value with infinite weight. Infinite weight means the
factor is certain; during the variable consensus step, this message dominates
standard or zero-weight messages.

The minimizer does not call `get()`, because the incoming value cannot change
what a known-value factor believes.

### Closing The Lambda, Function, And Namespace

The factor creation call and function close with:

```cpp
  });
}
```

The namespace closes with:

```cpp
} // namespace twalib
```

## Important Invariants

- The factor has exactly one edge and constrains exactly one variable.
- The outgoing value is always the captured known value.
- The outgoing weight is always `Message_weight::infinite`.
- The incoming message is intentionally ignored.

## Relationship To The Paper

In paper terms, a known-value factor is a function cost node that represents a
hard constraint. Its feasible set contains only one value, so the outgoing
message has infinite weight.

The current Sudoku builders encode givens without adding known-value factors,
but this minimizer remains the general public helper for one-variable hard
constraints.

## Extension Notes

Use this minimizer only when the value is genuinely fixed. If an application
wants a soft preference, it should use a different minimizer that sends
standard weight rather than infinite weight.
