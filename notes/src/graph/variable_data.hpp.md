# src/graph/variable_data.hpp

## Role In The System

`Variable_data` is the internal storage for one variable node. It is not part
of the public API; users hold `Variable_node` handles, while `Factor_graph`
owns the actual `Variable_data` objects.

A variable node is the equality or consensus side of the factor graph. During
each iteration, `Factor_graph` gathers the enabled edges attached to a
variable, combines their factor-to-variable messages into one shared belief,
and writes that result back through those same edges.

Recall that for efficiency reasons, we will sometimes disable factors and their
edges if those factors are sending zero-weight messages. Circle packing uses
this for pairs of circles that are not currently close to each other. We need
to be able to reenable those edges when the situation changes. `Variable_data`
supports that by maintaining both a complete list of incident edges and a
lazily maintained cache of the currently enabled subset.

`Variable_data` stores:

- the variable's initial weighted value,
- the current consensus value,
- the current consensus weight,
- all incident edge handles,
- a lazily maintained cache of enabled incident edge handles.

The message-passing note [`notes/concepts/message_passing.md`](../../concepts/message_passing.md) explains the
algorithmic role of variable equality. This file note explains the C++ storage
object that supports that step.

## State At A Glance

The private data members are easiest to understand before walking through the
functions:

```cpp
  Weighted_value initial_value_;
  double value_ = 0.0;
  Message_weight weight_ = Message_weight::standard;
  std::vector<Graph_edge> edges_;
  std::vector<Graph_edge> enabled_edges_;
  bool enabled_edges_need_update_ = false;
```

`initial_value_` is the weighted value used when the whole graph is
reinitialized.

`value_` and `weight_` are the variable's current consensus belief. They are
what `Factor_graph::value` and `Factor_graph::weight` report to callers.

`edges_` is the complete set of incident edge handles for this variable. It is
the durable ownership-side list: disabled edges stay in `edges_`.

`enabled_edges_` is a cache of the incident edges that currently participate in
the variable equality step.

`enabled_edges_need_update_` says whether the cache must be rebuilt from the
authoritative `Edge_data` storage before it can be trusted.

## Code Walkthrough

### Include Guard

The include guard:

```cpp
#ifndef TWALIB_SRC_GRAPH_VARIABLE_DATA_HPP
#define TWALIB_SRC_GRAPH_VARIABLE_DATA_HPP
```

prevents this implementation header from being processed more than once in a
single translation unit. The `SRC_GRAPH` part mirrors the path and reminds us
that this is not a public header.

### Includes

The first include is:

```cpp
#include "graph/edge_data.hpp"
```

`Variable_data` needs to inspect whether an incident `Edge_data` object is
enabled when rebuilding its enabled-edge cache.

The public graph-type includes are:

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
```

`Graph_edge` is the typed handle stored by the variable. `Weighted_value`
provides both the variable's weighted initial value and its current
`Message_weight`.

The standard headers are:

```cpp
#include <algorithm>
#include <span>
#include <stdexcept>
#include <vector>
```

`<algorithm>` supplies `std::ranges::find_if`. `<span>` lets the class receive
a non-owning view of the graph's edge storage. `<stdexcept>` provides the
exception types used for invalid graph state. `<vector>` stores the edge handle
lists.

### Namespace

The namespace is:

```cpp
namespace twalib::detail {
```

`detail` marks this as internal implementation machinery, not the surface API
that application code should use.

### Class And Public Section

The class begins:

```cpp
class Variable_data {
 public:
```

It is a small object stored by value in `Factor_graph::Impl`.

### Constructor

The constructor is:

```cpp
  explicit Variable_data(Weighted_value initial_value)
      : initial_value_(initial_value), value_(initial_value.value), weight_(initial_value.weight) {}
```

`explicit` prevents accidental conversion from `Weighted_value` to
`Variable_data`. The initializer list records the initial weighted value and
also uses it to set the current value and weight. The body is empty because all
construction work is handled by the initializer list.

### `reset`

The reset function is:

```cpp
  auto reset() -> void {
    enabled_edges_ = edges_;
    enabled_edges_need_update_ = false;
    value_ = initial_value_.value;
    weight_ = initial_value_.weight;
  }
```

In normal library use, this is invoked by `Factor_graph::reinitialize()`, not
by application code directly. Reinitialization is the operation that returns an
already-built graph to its starting state without reconstructing all variables,
edges, and factors. The graph calls `reset()` once for every `Variable_data`,
then resets each incident `Edge_data` from that variable's initial value, then
resets every `Factor_data`.

The `reset()` function restores the variable to its original belief. It also
treats every incident edge as enabled again by copying `edges_` into
`enabled_edges_`. Because the cache has just been made accurate,
`enabled_edges_need_update_` becomes false.

### `add_edge`

Adding a new edge uses:

```cpp
  auto add_edge(Graph_edge edge) -> void {
    edges_.push_back(edge);
    enabled_edges_.push_back(edge);
    enabled_edges_need_update_ = true;
  }
```

The edge is appended to both the complete incident-edge list and the enabled
cache. The cache is then marked dirty.

That last line is easy to underestimate. `add_edge()` receives only a
`Graph_edge` handle; it does not receive the graph's `Edge_data` storage, so it
cannot validate that the handle is in range or check whether the underlying
edge is actually enabled. Marking the cache dirty means the next call to
`enabled_edges(edge_data)` must rebuild the cache from the authoritative
`Edge_data` objects. That rebuild performs the bounds check and filters by
`Edge_data::is_enabled()`.

So the line is not only about preserving a previously stale cache. It also
prevents `enabled_edges_` from becoming a trusted cache before it has ever been
validated against the actual edge storage.

### `reenable_edge`

Reenabling an existing edge uses:

```cpp
  auto reenable_edge(Graph_edge edge) -> void {
    const auto known_edge = std::ranges::find_if(edges_, [edge](Graph_edge variable_edge) {
      return variable_edge.index == edge.index;
    });
    if (known_edge == edges_.end()) {
      throw std::out_of_range("Variable_data cannot reenable an edge it does not own");
    }

    if (enabled_edges_need_update_) {
      return;
    }

    const auto already_enabled = std::ranges::find_if(enabled_edges_, [edge](Graph_edge enabled_edge) {
      return enabled_edge.index == edge.index;
    });
    if (already_enabled == enabled_edges_.end()) {
      enabled_edges_.push_back(edge);
    }
  }
```

This is called by `Factor_graph` when a previously disabled factor becomes
enabled again. The name is intentionally `reenable_edge`, not `add_edge`,
because the edge must already belong to this variable.

The first `find_if` verifies ownership. The lambda captures the requested
`edge` by value and compares edge indexes, because `Graph_edge` is a
lightweight handle.

If the enabled-edge cache is already dirty, there is no reason to patch it. The
next call to `enabled_edges` will rebuild the cache from the complete edge list
and the actual edge enabled flags.

If the cache is clean, the second `find_if` avoids adding a duplicate. This
lets repeated reenable calls be harmless.

### `force_enabled_edges_update`

The explicit dirty-marking function is:

```cpp
  auto force_enabled_edges_update() -> void {
    enabled_edges_need_update_ = true;
  }
```

`Factor_graph` calls this when an edge is disabled. Disabling happens on the
edge object, so the variable cache needs a separate signal that its list may no
longer match reality.

### Value Updates

The simple value update is:

```cpp
  auto update_value(double new_value) -> void {
    value_ = new_value;
  }
```

This updates only the scalar value. It is useful for tests and for cases where
the weight should remain unchanged.

The weighted result update is:

```cpp
  auto update_result(Weighted_value result) -> void {
    value_ = result.value;
    weight_ = result.weight;
  }
```

This is what the graph's variable pass uses after computing the weighted
consensus for the variable.

### Accessors

The initial value accessor is:

```cpp
  [[nodiscard]] auto initial_value() const -> Weighted_value {
    return initial_value_;
  }
```

`[[nodiscard]]` asks the compiler to warn if the caller ignores the returned
value. `Factor_graph` uses this during full reinitialization.

The current value accessor is:

```cpp
  [[nodiscard]] auto value() const -> double {
    return value_;
  }
```

This is the value users see through `Factor_graph::value`.

The current weight accessor is:

```cpp
  [[nodiscard]] auto weight() const -> Message_weight {
    return weight_;
  }
```

This is the consensus weight users see through `Factor_graph::weight`.

The full edge list accessor is:

```cpp
  [[nodiscard]] auto edges() const -> std::span<const Graph_edge> {
    return edges_;
  }
```

The return type is `std::span<const Graph_edge>`, a non-owning view into the
stored vector. The caller can read the handles but cannot mutate them through
this span.

### `enabled_edges`

The enabled-edge accessor is:

```cpp
  auto enabled_edges(std::span<const Edge_data> edge_data) -> std::span<const Graph_edge> {
    if (enabled_edges_need_update_) {
      rebuild_enabled_edges(edge_data);
    }

    if (enabled_edges_.empty()) {
      throw std::logic_error("Variable_data has no enabled edges");
    }

    return enabled_edges_;
  }
```

Unlike `edges()`, this may need current edge storage so it can rebuild the
enabled cache if the cache is dirty.

The lazy rebuild check defers cache rebuilding until the graph actually needs
the enabled-edge list. That keeps repeated factor enable/disable operations
cheap.

A variable with no enabled edges cannot participate in the equality step.
Throwing makes that invalid graph state explicit instead of silently dividing
by zero later.

Finally, the accessor returns the cache. This converts the vector to a
read-only span. The vector remains owned by `Variable_data`.

### Private Section

The private section begins:

```cpp
 private:
```

Everything below this point is implementation detail used by the public member
functions above.

### `rebuild_enabled_edges`

The private rebuild function is:

```cpp
  auto rebuild_enabled_edges(std::span<const Edge_data> edge_data) -> void {
    enabled_edges_.clear();

    for (const Graph_edge edge : edges_) {
      if (!edge.is_valid() || edge.index >= edge_data.size()) {
        throw std::out_of_range("Variable_data references an invalid edge");
      }
      if (edge_data[edge.index].is_enabled()) {
        enabled_edges_.push_back(edge);
      }
    }

    enabled_edges_need_update_ = false;
  }
```

Rebuilding starts from an empty cache. Then every known incident edge is
checked against the authoritative `edge_data` span.

The bounds check catches corrupted graph state or a mismatched edge span. If
the edge is valid and currently enabled, its handle is copied into the cache.
Disabled edges remain in the complete `edges_` list but are omitted from
`enabled_edges_`.

At the end, `enabled_edges_need_update_` becomes false because the cache is
accurate again.

### Data Members

The data members are:

```cpp
  Weighted_value initial_value_;
  double value_ = 0.0;
  Message_weight weight_ = Message_weight::standard;
  std::vector<Graph_edge> edges_;
  std::vector<Graph_edge> enabled_edges_;
  bool enabled_edges_need_update_ = false;
```

`initial_value_` is stable and used for resets. `value_` and `weight_` are the
current consensus belief. `edges_` is the complete incident-edge list.
`enabled_edges_` is a cache of the currently active subset.
`enabled_edges_need_update_` records whether that cache must be rebuilt from
the actual `Edge_data` enabled flags.

### Closing The File

The file closes the namespace and include guard:

```cpp
} // namespace twalib::detail

#endif
```

## Important Invariants

- `edges_` is the complete set of incident edges for the variable.
- `enabled_edges_` is only a cache; when dirty, `rebuild_enabled_edges` is the
  source of truth.
- A variable with no enabled edges is invalid for the graph's equality step.
- The `reset()` method restores the original weighted value and treats all
  incident edges as enabled.
- `reenable_edge` may only be called with an edge already owned by the
  variable.

## Relationship To The Paper

`Variable_data` represents the equality node or consensus side of the paper's
factor graph. Its `value_` is the current `z` value for the variable, while
`weight_` records whether that consensus is zero, standard, or infinite weight.

The actual weighted averaging rule is implemented in `factor_graph.cpp`, not
here. This class provides the incident enabled-edge list and stores the result.

## Extension Notes

If new dynamic-factor behavior is added, preserve the distinction between the
complete edge list and the enabled-edge cache. It is usually better to mark the
cache dirty than to try to make many local edits perfectly precise.

If a future feature allows variables to be temporarily isolated, the graph
iteration logic should be changed deliberately. Today, `Variable_data` treats
"no enabled edges" as an error because the consensus step has no meaningful
input.
