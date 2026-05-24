# src/minimizers/spy.cpp

## Role In The System

`spy.cpp` implements the spy minimizer. A spy factor connects to one variable,
shows the incoming `Weighted_value` to user-provided code, and optionally lets
that user code emit a replacement outgoing message.

This makes the spy factor useful for instrumentation and interaction:

- it can observe what value the solver is currently sending toward a factor,
- it can stay invisible by returning no outgoing value,
- it can inject a value when an application wants to intervene.

For example, an interactive circle-packing tool could use a spy-like factor to
observe a selected circle, or to fix a dragged circle by emitting an
infinite-weight value while the user is holding it.

## Code Walkthrough

### Own Header Include

The file starts with:

```cpp
#include "twalib/minimizers/spy.hpp"
```

Including the matching public header first helps catch disagreement between
the declaration and the implementation.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
```

`Graph_edge` is the handle for the edge created by this helper.
`Weighted_value_exchange` is the per-edge exchange object used by the
minimization lambda. Through that header, this file also gets the
`Random_engine` type needed by the minimizer signature.

`Weighted_value` and `Message_weight` are already available through the public
spy header, because `Spy_function` uses `Weighted_value`.

### Standard Includes

The standard includes are:

```cpp
#include <span>
#include <utility>
```

`<span>` provides `std::span`, the exchange-buffer view type used by
minimization functions. `<utility>` provides `std::move`, used to move the
caller-provided callback into the minimizer lambda.

### Namespace

The implementation lives in:

```cpp
namespace twalib {
```

This matches the public declaration in the header.

### `create_spy_factor`

The function begins:

```cpp
auto create_spy_factor(
    Factor_graph& graph,
    Variable_node variable,
    Spy_function value_function) -> Factor_node {
```

`graph` is the factor graph being modified. `variable` is the single variable
the spy observes. `value_function` is caller-provided code that receives the
incoming `Weighted_value` and may return an outgoing `Weighted_value`.

The function returns a `Factor_node` handle for the created factor.

### Edge Creation

The helper creates one edge:

```cpp
  const Graph_edge edge = graph.create_edge(variable);
```

The spy is a unary factor, so it needs exactly one edge attached to the
supplied variable.

### Factor Creation And Lambda

The function returns the new factor:

```cpp
  return graph.create_factor({edge}, [value_function = std::move(value_function)](
                                         std::span<Weighted_value_exchange> exchanges,
                                         Random_engine&) {
```

The brace list `{edge}` gives the factor its single edge.

The lambda capture:

```cpp
[value_function = std::move(value_function)]
```

moves the caller's `Spy_function` into the lambda and stores it there.
`Spy_function` is a `std::function`, so moving avoids an unnecessary copy of
the type-erased callable when possible.

The lambda parameters are:

```cpp
std::span<Weighted_value_exchange> exchanges,
Random_engine&
```

`exchanges` is the non-owning view of exchange buffers for this factor. The
spy has one edge, so it uses `exchanges[0]`.

The random engine parameter is unnamed because the spy minimizer does not use
randomness. It is present because all factor minimizers share the same
`Minimization_function` signature.

### Reading The Incoming Message

The incoming message is read with:

```cpp
    const Weighted_value incoming = exchanges[0].get();
```

This is the message the variable side is currently sending toward the spy
factor. The spy passes it to user code in the next step.

### Calling The Spy Function

The optional result is handled by:

```cpp
    if (const auto result = value_function(incoming); result.has_value()) {
      exchanges[0].set(*result);
```

This uses an `if` statement with an initializer. The expression:

```cpp
const auto result = value_function(incoming)
```

calls the user-provided spy function and stores its return value. The return
type is `std::optional<Weighted_value>`, so the result may or may not contain
an outgoing message.

If `result.has_value()` is true, the spy emits that value by writing it into
the exchange. The `*result` expression extracts the contained `Weighted_value`.

### Staying Invisible

The no-result case is:

```cpp
    } else {
      exchanges[0].set(Weighted_value{incoming.value, Message_weight::zero});
    }
```

If the user function returns `std::nullopt`, the spy sends the incoming numeric
value back with zero weight. Zero weight means the spy has no active opinion;
it observes without affecting the variable consensus.

### Closing The Lambda, Function, And Namespace

The lambda and function close with:

```cpp
  });
}
```

The namespace closes with:

```cpp
} // namespace twalib
```

## Important Invariants

- The spy factor has exactly one edge and observes exactly one variable.
- The user-provided `Spy_function` receives the incoming weighted value.
- Returning a `Weighted_value` makes the spy emit that value.
- Returning `std::nullopt` makes the spy emit zero weight, which keeps it from
  influencing consensus.
- The spy uses the common minimizer signature but does not use randomness.

## Relationship To The Paper

The paper does not require a special spy factor. It is an implementation
convenience: a function cost node whose behavior is delegated to user code.
When it returns `std::nullopt`, it behaves like an inactive zero-weight factor.
When it returns a value, it behaves like whatever one-variable factor the user
has chosen to express.

## Extension Notes

The spy function runs during the factor pass, so it should be quick and should
avoid surprising side effects. It is a good hook for observation and controlled
application interaction, but long-running UI work should happen outside the
minimizer itself.
