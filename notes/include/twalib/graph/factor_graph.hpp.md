# include/twalib/graph/factor_graph.hpp

## Role In The System

`Factor_graph` is the main public API for building and running a
Three-Weight Algorithm factor graph.

Application code uses this class to:

- create variables,
- create edges from factors to variables,
- create factors with minimization functions,
- run iterations,
- read values and weights,
- enable or disable dynamic factors,
- reset a graph for another run.

The header presents a compact public facade. The implementation details live
behind a private `Impl` class in `src/graph/factor_graph.cpp`.

## Main Type

This header defines one public class:

```cpp
class Factor_graph
```

The class owns the graph. Handles such as `Variable_node`, `Graph_edge`, and
`Factor_node` are only indexes into this owned storage.

## State At A Glance

The public class stores only:

```cpp
  std::unique_ptr<Impl> impl_;
```

The real state is inside `Impl`: vectors of variables, edges, and factors;
solver settings; callbacks; iteration count; convergence state; and the random
engine. This is the PImpl pattern. It keeps implementation headers out of the
public header and lets the internal layout change without changing the public
class definition.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_GRAPH_FACTOR_GRAPH_HPP
#define TWALIB_GRAPH_FACTOR_GRAPH_HPP
```

It prevents duplicate processing of the public graph header.

### Graph Includes

The public graph headers are:

```cpp
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
```

`Factor_graph` exposes all of these types in its public methods. A user who
includes this header gets the handles, message weights, weighted values, and
minimizer function type needed to build a graph.

### Standard Includes

The standard includes are:

```cpp
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
```

`<cstddef>` provides `std::size_t` for counts and iteration limits.
`<cstdint>` provides `std::uint64_t` for random seeds. `<functional>` provides
callback storage. `<initializer_list>` supports convenient factor creation from
brace lists. `<memory>` provides `std::unique_ptr` for the PImpl member.
`<span>` supports factor creation from a non-owning view of edge handles.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

`Factor_graph` is the central user-facing type in this namespace.

### Class And Public Section

The class begins:

```cpp
class Factor_graph {
 public:
```

The public section contains construction, graph-building, iteration, querying,
and callback APIs.

### Constructor And Destructor

Construction is declared as:

```cpp
  explicit Factor_graph(
      double learning_rate = 1.0,
      double convergence_delta = 1e-5,
      std::uint64_t random_seed = 0);
  ~Factor_graph();
```

`explicit` prevents accidental construction from a single `double`.

`learning_rate` is the algorithm's update parameter for accumulated
disagreement. `convergence_delta` is the per-edge message-change threshold used
by convergence checks. `random_seed` seeds the graph's deterministic random
engine.

The destructor is declared in the header but defined in the implementation
file. That matters because `impl_` points to an incomplete type here; the
implementation file knows the full definition of `Impl`.

### Copy And Move

The copy operations are disabled:

```cpp
  Factor_graph(const Factor_graph&) = delete;
  auto operator=(const Factor_graph&) -> Factor_graph& = delete;
```

A graph owns substantial internal state through `std::unique_ptr<Impl>`.
Copying it would require defining what it means to duplicate all variables,
edges, factors, callbacks, and random-engine state. The API avoids that
ambiguity by deleting copy construction and copy assignment.

Move operations are allowed:

```cpp
  Factor_graph(Factor_graph&&) noexcept;
  auto operator=(Factor_graph&&) noexcept -> Factor_graph&;
```

Moving transfers ownership of the implementation object. `noexcept` says the
move operation is not expected to throw, which helps standard containers and
callers reason about moving graphs.

### Solver Settings

The learning-rate API is:

```cpp
  [[nodiscard]] auto learning_rate() const -> double;
  auto set_learning_rate(double learning_rate) -> void;
```

The getter reports the current update parameter. The setter changes it for
future iterations.

The convergence-threshold API is:

```cpp
  [[nodiscard]] auto convergence_delta() const -> double;
  auto set_convergence_delta(double convergence_delta) -> void;
```

The getter reports the threshold used by convergence checks. The setter changes
that threshold.

The random-seed API is:

```cpp
  auto set_random_seed(std::uint64_t random_seed) -> void;
```

Setting the seed resets the graph's random engine sequence. This affects
minimizers that use random tie-breaking.

### Iteration State

The iteration-state queries are:

```cpp
  [[nodiscard]] auto iterations() const -> std::size_t;
  [[nodiscard]] auto converged() const -> bool;
```

`iterations()` reports how many iterations have run since construction or
reinitialization. `converged()` reports the graph's current convergence flag.

Both are marked `[[nodiscard]]` because callers usually ask these questions in
order to use the answer.

### Graph Counts

The graph-size queries are:

```cpp
  [[nodiscard]] auto num_variables() const -> std::size_t;
  [[nodiscard]] auto num_edges() const -> std::size_t;
  [[nodiscard]] auto num_enabled_edges() const -> std::size_t;
  [[nodiscard]] auto num_factors() const -> std::size_t;
  [[nodiscard]] auto num_enabled_factors() const -> std::size_t;
```

They report total variable, edge, and factor counts, plus enabled counts for
edges and factors. Enabled counts matter for dynamic problems such as fast
circle packing, where some factors can be temporarily inactive.

### Graph Construction

Creating a variable uses:

```cpp
  [[nodiscard]] auto create_variable(
      double initial_value,
      Message_weight initial_weight = Message_weight::standard) -> Variable_node;
```

The graph stores a new variable with the given initial value and weight, then
returns a `Variable_node` handle. The default initial weight is standard.

Creating an edge uses:

```cpp
  [[nodiscard]] auto create_edge(Variable_node variable) -> Graph_edge;
```

The new edge is attached to the given variable. Later, that edge can be grouped
with other edges into a factor.

The span-based factor creation overload is:

```cpp
  [[nodiscard]] auto create_factor(
      std::span<const Graph_edge> edges,
      Minimization_function minimization_function) -> Factor_node;
```

It creates a factor over a non-owning view of edge handles and stores the
minimization function that will run during factor passes.

The initializer-list overload is:

```cpp
  [[nodiscard]] auto create_factor(
      std::initializer_list<Graph_edge> edges,
      Minimization_function minimization_function) -> Factor_node;
```

It supports convenient calls such as:

```cpp
graph.create_factor({edge_a, edge_b}, minimizer);
```

Both overloads return a `Factor_node` handle for later inspection or dynamic
enable/disable operations.

### Reading Values And Weights

The variable inspection API is:

```cpp
  [[nodiscard]] auto value(Variable_node variable) const -> double;
  [[nodiscard]] auto weight(Variable_node variable) const -> Message_weight;
```

`value()` returns the current consensus value for a variable. `weight()`
returns the current consensus weight, which can be zero, standard, or
infinite.

### Dynamic Factors

The factor-enabled API is:

```cpp
  [[nodiscard]] auto is_factor_enabled(Factor_node factor) const -> bool;
  auto set_factor_enabled(Factor_node factor, bool enabled) -> void;
```

Dynamic problems can disable factors whose outgoing messages would be
zero-weight and reenable them when they become relevant. Disabling a factor
also disables its edges; reenabling resets those edges from the current
variable beliefs.

### Iteration And Reinitialization

One iteration is run with:

```cpp
  auto iterate() -> bool;
```

It performs one factor pass, one variable pass, dynamic-factor updates, and a
convergence check. The return value is true when the graph has converged.

Repeated iteration is run with:

```cpp
  auto iterate_until_converged(std::size_t max_iterations) -> bool;
```

It runs up to `max_iterations` iterations and returns whether convergence was
reached.

The graph can be reset with:

```cpp
  auto reinitialize() -> void;
```

Reinitialization keeps the graph structure but restores variables, edges, and
factors to their starting state.

### Callbacks

Iteration callbacks are registered with:

```cpp
  auto add_iteration_callback(std::function<void()> callback) -> void;
```

The graph calls these after iterations. They are useful for instrumentation,
dynamic factor updates, and GUI refresh logic.

Reinitialization callbacks are registered with:

```cpp
  auto add_reinitialize_callback(std::function<void()> callback) -> void;
```

The graph calls these after reinitialization so external helper objects can
reset any cached state they maintain alongside the graph.

### Private Section

The private section begins:

```cpp
 private:
```

Only the public methods should access implementation ownership.

### `Impl`

The implementation class is forward-declared:

```cpp
  class Impl;
```

This says that a nested class named `Factor_graph::Impl` exists, without
showing its definition in the public header.

### Implementation Pointer

The only data member is:

```cpp
  std::unique_ptr<Impl> impl_;
```

`std::unique_ptr` gives `Factor_graph` sole ownership of its implementation
object. This is the PImpl idiom: public code sees a stable, small class
definition, while implementation details stay in the `.cpp` file.

### Closing The File

The class closes with:

```cpp
};
```

The namespace and include guard close with:

```cpp
} // namespace twalib

#endif
```

## Important Invariants

- `Factor_graph` owns the graph; node and edge handles do not.
- Handles returned by one graph should not be used with another graph.
- Copying a graph is intentionally disabled.
- Moving a graph transfers ownership of the implementation.
- Dynamic factor enablement must keep factor, edge, and variable enabled-edge
  state consistent.
- Iteration callbacks should not assume they own graph state; they observe or
  update auxiliary state around the graph.

## Relationship To The Paper

`Factor_graph` is the public object that runs the paper's message-passing
iteration. Its variables correspond to equality nodes, its factors correspond
to function cost nodes, and its edges carry the leftward and rightward messages
between them.

The detailed mapping from the paper's notation to implementation state is in:

- [`notes/concepts/message_passing.md`](../../../concepts/message_passing.md)
- [`notes/concepts/weights.md`](../../../concepts/weights.md)
- [`notes/src/graph/factor_graph.cpp.md`](../../../src/graph/factor_graph.cpp.md)

## Extension Notes

Prefer adding public operations here only when they preserve the handle-based
ownership model. If a feature needs to inspect or mutate internal data, expose
it as a validated `Factor_graph` method rather than by making internal storage
types public.

Because this header uses PImpl, many implementation changes can happen in
`src/graph/factor_graph.cpp` without changing this public header.
