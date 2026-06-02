# src/graph/factor_data.hpp

## Role In The System

`Factor_data` is the internal storage for one factor node. It is not part of
the public API; users register factors through `Factor_graph`, while
`Factor_graph::Impl` owns the actual `Factor_data` objects.

A factor node is the constraint or local-objective side of the factor graph.
During each iteration, `Factor_graph` asks every enabled factor to run its
minimization function. The minimizer reads the incoming variable-to-factor
messages, computes locally optimal values for its edges, and writes the results
back as factor-to-variable messages.

`Factor_data` stores:

- the minimization function (a `std::function` callback),
- the exchange buffer through which the minimizer reads and writes messages,
- a parallel vector tracking which incoming messages had infinite weight,
- whether the factor is currently enabled.

The message-passing note [`notes/concepts/message_passing.md`](../../concepts/message_passing.md) explains the
algorithmic role of factor minimization. This file note explains the C++
storage object that supports that step.

## State At A Glance

The private data members are easiest to understand before walking through the
functions:

```cpp
  Minimization_function minimization_function_;
  std::vector<Weighted_value_exchange> exchanges_;
  std::vector<bool> incoming_infinite_weights_;
  bool enabled_ = true;
```

`minimization_function_` is the `std::function` callback that solves this
factor's local subproblem. It is set once at construction and never changes.

`exchanges_` is the per-edge buffer shared with the minimizer. Each entry
corresponds to one incident edge and carries the incoming message value and
weight as well as space for the minimizer to write its result back.

`incoming_infinite_weights_` is a parallel vector of flags recording which
incoming messages had infinite weight. It lets the factor skip those edges
during minimization.

`enabled_` records whether this factor is active in the current graph pass.
Disabled factors are skipped during the factor-minimization step.

## Code Walkthrough

### Include Guard

The include guard:

```cpp
#ifndef TWALIB_SRC_GRAPH_FACTOR_DATA_HPP
#define TWALIB_SRC_GRAPH_FACTOR_DATA_HPP
```

prevents this implementation header from being processed more than once in a
single translation unit. The `SRC_GRAPH` part mirrors the path and reminds us
that this is not a public header.

### Includes

The first include is:

```cpp
#include "graph/edge_data.hpp"
```

`Factor_data` needs direct access to `Edge_data` objects so it can read
incoming messages and write factor results back into the edge storage.

The public graph-type includes are:

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
```

`Graph_edge` is the lightweight handle identifying an edge by index.
`Weighted_value_exchange` is both the exchange buffer type and the source of
`Minimization_function` and `Random_engine` type aliases.

The standard headers are:

```cpp
#include <cstddef>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
```

`<cstddef>` provides `std::size_t` for index arithmetic. `<span>` lets the
factor receive a non-owning view of the graph's edge storage. `<stdexcept>`
provides the exception type for invalid edge handles. `<utility>` supplies
`std::move` for transferring ownership of the minimization function.
`<vector>` stores the exchange buffer and the infinite-weight flags.

### Namespace

The namespace is:

```cpp
namespace twalib::detail {
```

`detail` marks this class as internal implementation machinery, not the surface
API that application code should use.

### Class And Public Section

The class begins:

```cpp
class Factor_data {
 public:
```

It is stored by value in a `std::vector` inside `Factor_graph::Impl`.

### Constructor

The constructor is:

```cpp
  Factor_data(std::span<const Graph_edge> edges, Minimization_function minimization_function)
      : minimization_function_(std::move(minimization_function)) {
    exchanges_.reserve(edges.size());
    incoming_infinite_weights_.reserve(edges.size());
    for (const Graph_edge edge : edges) {
      exchanges_.emplace_back(edge);
      incoming_infinite_weights_.push_back(false);
    }
  }
```

The initializer list moves the minimization function into the member. Moving
avoids copying the potentially heavy `std::function` object.

The body builds two parallel vectors from the incoming edge handles. Each edge
gets one `Weighted_value_exchange` in the exchange buffer, constructed with
that edge's handle so the exchange always knows which edge it belongs to. The
`incoming_infinite_weights_` vector starts all-false and will be set per
iteration during `minimize`.

`reserve` is called before the loop so both vectors allocate once rather than
growing incrementally.

### `reset`

The reset function is:

```cpp
  auto reset() -> void {
    enabled_ = true;
  }
```

Unlike `Edge_data::reset` or `Variable_data::reset`, the factor reset does not
clear numeric state. Factor-side local values live in the edges, not in the
factor itself. The only per-factor state that matters across iterations is
whether the factor is enabled, so reset simply reenables it.

### `enable`

The enable function is:

```cpp
  auto enable() -> bool {
    return switch_enabled(true);
  }
```

It returns `true` if the factor was previously disabled and is now enabled,
`false` if it was already enabled. The caller (`Factor_graph`) uses the return
value to decide whether incident edges need to be reset and variables need
their enabled-edge caches dirtied.

### `disable`

The disable function is:

```cpp
  auto disable() -> bool {
    return switch_enabled(false);
  }
```

It returns `true` if the factor was previously enabled and is now disabled,
`false` if it was already disabled. The caller uses the return value to decide
whether incident edges should be disabled and variable caches dirtied.

### `is_enabled`

The enabled accessor is:

```cpp
  [[nodiscard]] auto is_enabled() const -> bool {
    return enabled_;
  }
```

`[[nodiscard]]` asks the compiler to warn if a caller ignores the result. The
graph uses this during convergence checks and iteration passes.

### `exchanges`

The exchange accessor is:

```cpp
  [[nodiscard]] auto exchanges() const -> std::span<const Weighted_value_exchange> {
    return exchanges_;
  }
```

It returns a read-only span over the exchange buffer. This lets `Factor_graph`
iterate over a factor's edges without granting mutation access.

### `minimize`

The minimize function is the heart of the factor's per-iteration work:

```cpp
  auto minimize(std::span<Edge_data> edge_data, Random_engine& random) -> void {
    if (!enabled_) {
      return;
    }

    for (std::size_t index = 0; index < exchanges_.size(); ++index) {
      Weighted_value_exchange& exchange = exchanges_[index];
      const Weighted_value incoming = edge_for(exchange, edge_data).weighted_message_to_factor();
      incoming_infinite_weights_[index] = incoming.weight == Message_weight::infinite;
      exchange.set(incoming);
    }

    minimization_function_(exchanges_, random);

    for (std::size_t index = 0; index < exchanges_.size(); ++index) {
      const Weighted_value_exchange exchange = exchanges_[index];
      Weighted_value result = exchange.get();
      if (incoming_infinite_weights_[index]) {
        result.weight = Message_weight::infinite;
      }
      edge_for(exchange, edge_data).set_result_from_factor(result);
    }
  }
```

The early return skips disabled factors entirely. Disabled factors send no
messages, so the graph treats them as absent.

**Input phase.** The first loop reads each edge's leftward weighted message
(`z - u` paired with the leftward weight) and loads it into the exchange
buffer. It also records whether the incoming weight was infinite, because that
information must survive the minimizer call.

**Minimizer call.** The minimization function receives the exchange buffer and
a random engine. The minimizer reads the incoming values via `exchange.get()`,
solves its local subproblem, and writes the results back via `exchange.set()`.
The minimizer is free to use any local algorithm; the factor does not know or
care what strategy it uses.

**Output phase.** The second loop reads the minimizer's results out of the
exchange buffer and writes them into the edge storage via
`set_result_from_factor`. If the incoming message had infinite weight, the
outgoing weight is forced to infinite regardless of what the minimizer wrote.
This preserves the paper's rule that certainty propagates through factors:
if a variable is already known with certainty, the factor must not downgrade
that certainty.

### Private Section

The private section begins:

```cpp
 private:
```

Everything below this point supports the public factor operations.

### `edge_for`

The private helper that resolves an exchange to its edge storage is:

```cpp
  static auto edge_for(Weighted_value_exchange exchange, std::span<Edge_data> edge_data) -> Edge_data& {
    const Graph_edge edge = exchange.edge();
    if (!edge.is_valid() || edge.index >= edge_data.size()) {
      throw std::out_of_range("Factor_data references an invalid edge");
    }
    return edge_data[edge.index];
  }
```

It extracts the `Graph_edge` handle from the exchange, bounds-checks it
against the edge storage span, and returns a mutable reference to the actual
`Edge_data`. The bounds check catches corrupted graph state early rather than
allowing silent out-of-bounds access.

The function is `static` because it does not use any member state; it operates
only on its arguments.

### `switch_enabled`

The private toggle helper is:

```cpp
  auto switch_enabled(bool enabled) -> bool {
    if (enabled_ == enabled) {
      return false;
    }

    enabled_ = enabled;
    return true;
  }
```

Both `enable()` and `disable()` delegate to this. The pattern avoids
duplicating the "already in the target state" check. The return value tells the
caller whether the state actually changed.

### Data Members

The data members are:

```cpp
  Minimization_function minimization_function_;
  std::vector<Weighted_value_exchange> exchanges_;
  std::vector<bool> incoming_infinite_weights_;
  bool enabled_ = true;
```

`minimization_function_` is stable for the life of the factor. It is a
`std::function<void(std::span<Weighted_value_exchange>, Random_engine&)>` that
encapsulates the factor's constraint logic.

`exchanges_` is the communication buffer between the graph and the minimizer.
Each entry corresponds to one incident edge. The exchange stores both the
`Graph_edge` handle and the current `Weighted_value` being passed through it.

`incoming_infinite_weights_` is a parallel vector that records, for each edge,
whether the most recent incoming message had infinite weight. This is needed
because the minimizer may overwrite the weight in the exchange buffer, but
infinite incoming weight must be preserved in the output.

`enabled_` tracks whether this factor is active. Disabled factors are skipped
during minimization and do not contribute messages.

### Closing The File

The file closes the namespace and include guard:

```cpp
} // namespace twalib::detail

#endif
```

## Important Invariants

- The exchange buffer and the infinite-weight vector have the same length,
  equal to the number of incident edges. They are parallel: index `i` in
  both refers to the same edge.
- A disabled factor never runs its minimizer and never writes to edge storage.
- Infinite incoming weight is always preserved in the outgoing result,
  regardless of what the minimizer produces.
- `enable()` and `disable()` return `true` only when the state actually
  changes, so the caller can avoid redundant cache invalidation.
- `reset()` only reenables the factor; it does not alter the exchange buffer
  or the minimization function.

## Relationship To The Paper

In the paper's TWA algorithm, a factor node runs a local minimization subject
to its constraint. The factor reads the incoming messages `n_i = z_i - u_i`
(one per incident edge), solves its subproblem, and produces outgoing values
`x_i` with associated weights.

In this implementation:

- The incoming message is `Edge_data::weighted_message_to_factor()`.
- The minimizer receives and returns values through `Weighted_value_exchange`.
- The outgoing result is stored via `Edge_data::set_result_from_factor()`.
- The three-weight system (zero, standard, infinite) controls how strongly
  messages influence their recipients.
- The infinite-weight preservation rule implements the paper's certainty
  propagation.

## Extension Notes

When adding a new minimizer, implement it as a function matching
`Minimization_function`: it receives `std::span<Weighted_value_exchange>` and
a `Random_engine&`. Read incoming values with `exchange.get()`, compute the
local optimum, and write results with `exchange.set()`. The factor
infrastructure handles loading messages from edges and writing results back.

If a new minimizer needs to express "no opinion" on an edge, it should set the
outgoing weight to `Message_weight::zero`. If it can determine an exact value,
it should use `Message_weight::infinite`.
