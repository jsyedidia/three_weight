# include/twalib/graph/graph_edge.hpp

## Role In The System

`Graph_edge` is the public handle for an edge in a `Factor_graph`. An edge
connects exactly one factor to exactly one variable and carries messages in
both directions during the Three-Weight Algorithm.

The handle does not store those messages. It only identifies an edge owned by
the graph. The actual per-edge state lives in internal `Edge_data` objects.

Users commonly receive `Graph_edge` values from `Factor_graph::create_edge`
and pass them into `Factor_graph::create_factor` so a factor can be connected
to the variables it constrains.

## Main Type

This header defines one public type:

```cpp
struct Graph_edge
```

Like `Variable_node` and `Factor_node`, it is a typed index wrapper. Separate
handle types make it clear which API calls expect variables, factors, or
edges.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_GRAPH_GRAPH_EDGE_HPP
#define TWALIB_GRAPH_GRAPH_EDGE_HPP
```

It prevents duplicate inclusion of this header.

### Includes

The header uses:

```cpp
#include <cstddef>
#include <limits>
```

`std::size_t` comes from `<cstddef>`. `std::numeric_limits` comes from
`<limits>` and is used to define the invalid sentinel.

### Namespace

The declarations live in:

```cpp
namespace twalib {
```

`Graph_edge` is part of the public API.

### `Graph_edge`

The type begins:

```cpp
struct Graph_edge {
```

The type is a small public handle with one data member.

### `invalid_index`

The invalid index is:

```cpp
  static constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();
```

This compile-time constant is the sentinel used by invalid edge handles.

### `index`

The stored edge index is:

```cpp
  std::size_t index = invalid_index;
```

Default construction yields an invalid edge. A graph-created edge stores an
index into that graph's internal edge vector.

### `is_valid`

The validity check is:

```cpp
  [[nodiscard]] constexpr auto is_valid() const -> bool {
    return index != invalid_index;
  }
```

The function returns true when the handle is not using the invalid sentinel.
It does not check whether a particular graph owns an edge at that index.

`[[nodiscard]]` asks callers not to throw away the result. `constexpr` and
`const` reflect that this is a simple read-only value check.

### Closing The File

The struct, namespace, and include guard close with:

```cpp
};

} // namespace twalib

#endif
```

## Important Invariants

- A default-constructed `Graph_edge` is invalid.
- A valid `Graph_edge` is meaningful only relative to the graph that created
  it.
- The handle does not know its factor endpoint; that association is stored by
  `Factor_data`.
- The handle does not know its variable endpoint; that association is stored
  by `Edge_data`.

## Relationship To The Paper

The paper's edges carry messages between function cost nodes and equality
nodes. `Graph_edge` is the public token for one of those edges. The message
state itself lives in `Edge_data`.

## Extension Notes

Keep endpoint lookup on `Factor_graph`. Adding endpoint pointers to
`Graph_edge` would make the handle heavier and introduce lifetime questions
that the current index-based design avoids.
