# src/minimizers/in_range.cpp

## Role In The System

`in_range.cpp` implements the range constraint minimizer. An in-range factor
connects to one variable and enforces that the variable should stay between a
lower and upper bound.

The minimizer is intentionally simple:

- if the incoming value is below the interval, it sends back the lower bound
  with standard weight,
- if the incoming value is above the interval, it sends back the upper bound
  with standard weight,
- if the incoming value is already inside the interval, it sends back the same
  value with zero weight.

That last case is important. A zero-weight message means the factor has no
active opinion because the constraint is already satisfied, and nearby values
would also satisfy the constraint.

Circle packing uses `in_range` factors to keep circle centers inside the
allowed rectangle, adjusted by each circle's radius.

## Code Walkthrough

### Own Header Include

The file starts with its own public header:

```cpp
#include "twalib/minimizers/in_range.hpp"
```

Including the matching header first helps catch mismatches between the public
declaration and the implementation.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
```

`Graph_edge` is the handle returned by `graph.create_edge`. `Weighted_value`
is the value/weight pair read from and written to the exchange.
`Weighted_value_exchange` is the per-edge buffer passed to the factor
minimizer. This header also provides the `Random_engine` type used in the
lambda signature.

### Standard Includes

The standard includes are:

```cpp
#include <span>
#include <stdexcept>
```

`<span>` provides `std::span`, the view type used by minimization functions.
`<stdexcept>` provides `std::invalid_argument`, used when the caller supplies
an invalid interval.

### Namespace

The implementation lives in:

```cpp
namespace twalib {
```

This matches the public declaration in the header.

### `create_in_range_factor`

The function begins:

```cpp
auto create_in_range_factor(
    Factor_graph& graph,
    Variable_node variable,
    double lower,
    double upper) -> Factor_node {
```

`graph` is the factor graph being modified. `variable` is the single variable
that should be constrained to the interval. `lower` and `upper` are the
inclusive interval bounds.

The function returns a `Factor_node` handle for the newly created factor.

### Bound Validation

The first check is:

```cpp
  if (upper < lower) {
    throw std::invalid_argument("create_in_range_factor requires lower <= upper");
  }
```

The interval is valid only when `lower <= upper`. If the bounds are inverted,
the helper throws before modifying the graph.

### Edge Creation

The factor needs one edge:

```cpp
  const Graph_edge edge = graph.create_edge(variable);
```

This creates an edge attached to `variable`. The edge will become the only
edge incident to the new in-range factor.

### Factor Creation And Lambda

The function returns the result of creating the factor:

```cpp
  return graph.create_factor({edge}, [lower, upper](std::span<Weighted_value_exchange> exchanges, Random_engine&) {
```

The brace list `{edge}` says the factor has exactly one edge.

The second argument is a lambda, which becomes the factor's minimization
function. The capture list:

```cpp
[lower, upper]
```

stores copies of the interval bounds inside the lambda so they remain
available whenever the factor runs.

The lambda parameters are:

```cpp
std::span<Weighted_value_exchange> exchanges, Random_engine&
```

`exchanges` is the non-owning view of the factor's exchange buffers. This
factor has one edge, so the lambda uses `exchanges[0]`.

The random engine parameter is unnamed because this minimizer does not need
randomness. It is still present because every `Minimization_function` has the
same signature.

### Reading The Incoming Message

The incoming value is read with:

```cpp
    const Weighted_value incoming = exchanges[0].get();
```

The range constraint cares about `incoming.value`. It does not branch on
`incoming.weight` directly. The graph layer still applies the general TWA rule
that incoming infinite certainty on an edge is preserved in the outgoing
weight for that same edge.

### Clamping Below The Range

The lower-bound case is:

```cpp
    if (incoming.value < lower) {
      exchanges[0].set(Weighted_value{lower, Message_weight::standard});
```

If the incoming value is below the interval, the closest feasible point is the
lower bound. The minimizer sends `lower` with standard weight, meaning the
factor has an active ordinary opinion.

### Clamping Above The Range

The upper-bound case is:

```cpp
    } else if (incoming.value > upper) {
      exchanges[0].set(Weighted_value{upper, Message_weight::standard});
```

If the incoming value is above the interval, the closest feasible point is the
upper bound. Again, the factor sends standard weight because it is actively
pulling the variable back into the allowed range.

### No Opinion Inside The Range

The satisfied case is:

```cpp
    } else {
      exchanges[0].set(Weighted_value{incoming.value, Message_weight::zero});
    }
```

When the incoming value is already feasible, the factor sends the same value
with zero weight. Zero weight says this constraint is currently inactive: it
does not need to affect the variable consensus.

### Closing The Lambda, Function, And Namespace

The lambda and `create_factor` call close with:

```cpp
  });
}
```

The namespace closes with:

```cpp
} // namespace twalib
```

## Important Invariants

- The interval must satisfy `lower <= upper`.
- The factor has exactly one edge and constrains exactly one variable.
- Values outside the interval are projected to the nearest endpoint.
- Values inside the interval produce zero-weight messages.
- Infinite-weight preservation is handled by the shared factor machinery, not
  by special logic in this minimizer.

## Relationship To The Paper

In paper terms, an in-range factor is a function cost node for the constraint
`lower <= x <= upper`. Its minimization step is the projection of the incoming
value onto that interval. If the value is already feasible, the zero-weight
message expresses that this factor is currently indifferent.

## Extension Notes

This minimizer is deliberately one-dimensional. A multi-variable box
constraint could be built by adding one in-range factor per variable. A coupled
constraint over several variables should use a separate minimizer so its
projection logic is explicit.
