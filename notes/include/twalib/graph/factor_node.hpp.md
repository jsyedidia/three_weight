# include/twalib/graph/factor_node.hpp

## Role In The System

`Factor_node` is the public handle for a factor node in a `Factor_graph`.
It does not store a minimization function, incident edges, or enabled state.
Those live in the graph's internal `Factor_data` storage.

Instead, `Factor_node` is a typed index. Application code receives one when it
creates a factor and can later pass it back to `Factor_graph` to inspect or
change whether that factor is enabled.

This is the same handle pattern used by `Variable_node` and `Graph_edge`.
Having separate handle types makes the API harder to misuse: a factor handle
cannot be passed accidentally where a variable handle is required.

## Main Type

This header defines one public type:

```cpp
struct Factor_node
```

The type is intentionally tiny. It is cheap to copy, has no ownership, and is
valid only relative to the `Factor_graph` that created it.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_GRAPH_FACTOR_NODE_HPP
#define TWALIB_GRAPH_FACTOR_NODE_HPP
```

It prevents duplicate processing of this header in one translation unit.

### Includes

The standard headers are:

```cpp
#include <cstddef>
#include <limits>
```

`<cstddef>` provides `std::size_t`, the integer type used for indexes.
`<limits>` provides `std::numeric_limits`, which defines the invalid sentinel.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

`Factor_node` is part of the user-facing API, so it lives directly in
`twalib`.

### `Factor_node`

The handle type begins:

```cpp
struct Factor_node {
```

Using `struct` keeps the single data member public. The handle is just an
index, not an object with hidden behavior.

### `invalid_index`

The invalid sentinel is:

```cpp
  static constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();
```

`static constexpr` makes this a compile-time constant associated with the type.
The maximum possible `std::size_t` value is reserved to mean "not a valid
factor."

### `index`

The stored index is:

```cpp
  std::size_t index = invalid_index;
```

A default-constructed `Factor_node` is invalid. A factor created by
`Factor_graph::create_factor` has an index into that graph's internal factor
storage.

### `is_valid`

The validity check is:

```cpp
  [[nodiscard]] constexpr auto is_valid() const -> bool {
    return index != invalid_index;
  }
```

`[[nodiscard]]` encourages callers to use the boolean result. `constexpr`
allows compile-time evaluation when possible. The trailing return type says the
function returns `bool`, and `const` says it does not modify the handle.

The function checks only whether the index is different from the sentinel. It
does not prove that any particular `Factor_graph` contains a factor at that
index.

### Closing The File

The type closes with:

```cpp
};
```

The namespace closes with:

```cpp
} // namespace twalib
```

The include guard closes with:

```cpp
#endif
```

## Important Invariants

- A default-constructed `Factor_node` is invalid.
- A valid `Factor_node` is a handle, not an owner.
- The `index` is meaningful only to the graph that created it.
- Strong validation belongs in `Factor_graph`, not in the handle.

## Relationship To The Paper

The paper's "function cost nodes" correspond to factor nodes in this codebase.
`Factor_node` is the public token for referring to one such node.

The actual factor behavior is implemented by the minimization function stored
inside the graph's internal `Factor_data` object.

## Extension Notes

Keep this type small and passive. If future APIs need richer factor
inspection, prefer adding methods to `Factor_graph`, where the implementation
can validate the handle and access the graph-owned storage.
