# include/twalib/minimizers/spy.hpp

## Role In The System

This header declares the public API for adding a spy factor to a
`Factor_graph`.

A spy factor is a one-variable factor whose behavior is supplied by caller
code. The caller receives the incoming `Weighted_value` and returns either:

- a `Weighted_value`, meaning the spy should emit that message, or
- `std::nullopt`, meaning the spy should observe without influencing the
  graph.

The implementation lives in `src/minimizers/spy.cpp`.

## Main Types And Functions

This header defines:

```cpp
using Spy_function = std::function<std::optional<Weighted_value>(Weighted_value)>;
```

and declares:

```cpp
create_spy_factor(Factor_graph& graph, Variable_node variable, Spy_function value_function)
```

The type alias names the callback shape expected by the spy. The helper creates
one edge attached to the supplied variable, registers the spy minimizer, and
returns the new factor's handle.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_MINIMIZERS_SPY_HPP
#define TWALIB_MINIMIZERS_SPY_HPP
```

It prevents duplicate processing of this public minimizer header.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"
```

`Factor_graph` is the graph being modified. `Factor_node` is the handle
returned by the helper. `Variable_node` identifies the variable being observed.
`Weighted_value` is the message type passed into and possibly returned from the
spy callback.

### Standard Includes

The standard includes are:

```cpp
#include <functional>
#include <optional>
```

`std::function` stores any callable with the required spy signature.
`std::optional` represents the fact that the spy may or may not emit a value.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

The spy API is part of the public library namespace.

### `Spy_function`

The callback alias is:

```cpp
using Spy_function = std::function<std::optional<Weighted_value>(Weighted_value)>;
```

`using` creates a type alias. This line says that `Spy_function` is a
`std::function` holding any callable with the signature:

```cpp
std::optional<Weighted_value>(Weighted_value)
```

The input is the incoming weighted value from the variable side. The optional
return value controls the spy's outgoing message:

- returning a `Weighted_value` makes the spy emit it,
- returning `std::nullopt` makes the spy emit zero weight and stay inactive.

### `create_spy_factor`

The declaration is:

```cpp
[[nodiscard]] auto create_spy_factor(
    Factor_graph& graph,
    Variable_node variable,
    Spy_function value_function) -> Factor_node;
```

`[[nodiscard]]` asks callers not to ignore the returned `Factor_node`. The
factor is still added to the graph if the handle is ignored, but the handle is
useful for inspection or dynamic enable/disable operations.

`Factor_graph& graph` is a non-owning reference to the graph that will receive
the factor.

`Variable_node variable` is the variable being observed or influenced.

`Spy_function value_function` is the callback to run during the factor pass.
The implementation moves this callback into the factor's minimization lambda.

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
- The callback receives the incoming weighted value.
- Returning `std::nullopt` means the spy should send zero weight.
- The returned `Factor_node` belongs to the graph passed into the helper.

## Relationship To The Paper

The spy factor is not a mathematical primitive required by the paper. It is a
convenience factor for observing or externally influencing the message-passing
process while still using the same factor API as ordinary constraints.

## Extension Notes

Keep spy callbacks simple. They run inside the solver's factor pass, so they
should not perform expensive UI work or mutate unrelated graph state in ways
that make iteration order hard to reason about.
