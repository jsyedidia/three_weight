# include/twalib/graph/variable_node.hpp

## Role In The System

`Variable_node` is the public handle for a variable node in a `Factor_graph`.
It does not store the variable's current value, weight, or incident edges.
Those live in the graph's internal `Variable_data` storage.

Instead, `Variable_node` is a small index wrapper. Application code can copy it
around freely and pass it back to `Factor_graph` when it wants to inspect a
variable or create an edge connected to that variable.

The same handle pattern is also used by `Factor_node` and `Graph_edge`.
Together, these types keep the public API typed: a function that expects a
variable handle cannot accidentally receive a factor handle just because both
are represented internally by indexes.

## Main Type

This header defines one public type:

```cpp
struct Variable_node
```

It is intentionally a `struct` with a public `index` member. There is no
ownership, no allocation, and no hidden lifetime rule. A `Variable_node` is
valid only relative to the `Factor_graph` that created it, because its `index`
refers to a position in that graph's internal variable vector.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_GRAPH_VARIABLE_NODE_HPP
#define TWALIB_GRAPH_VARIABLE_NODE_HPP
```

It prevents the declarations in this header from being processed more than
once in the same translation unit. The guard name mirrors the public include
path.

### Includes

The header includes:

```cpp
#include <cstddef>
#include <limits>
```

`<cstddef>` provides `std::size_t`, the unsigned integer type used for indexes
into vectors and other container-like storage.

`<limits>` provides `std::numeric_limits`, which the handle uses to define a
sentinel invalid index.

### Namespace

The public namespace begins with:

```cpp
namespace twalib {
```

Everything in this header is part of the library's public API, so it lives in
`twalib`, not in the internal `twalib::detail` namespace used by implementation
storage types.

### `Variable_node`

The handle type begins:

```cpp
struct Variable_node {
```

This is a simple aggregate-like type. Using `struct` makes the member public by
default, which is appropriate here because the index is the whole content of
the handle.

### `invalid_index`

The invalid sentinel is:

```cpp
  static constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();
```

`static` means the value belongs to the type itself rather than to any one
`Variable_node` object. Code refers to it as `Variable_node::invalid_index`.

`constexpr` means the value is available at compile time.

`auto` asks the compiler to infer the type from the expression on the right.
Here the type is `std::size_t`.

The value chosen is the largest representable `std::size_t`. Real variable
indexes are assigned from vector positions starting at zero, so this maximum
value is reserved as a sentinel meaning "not a valid variable."

### `index`

The stored handle value is:

```cpp
  std::size_t index = invalid_index;
```

A default-constructed `Variable_node` starts invalid. This is useful because a
caller can create a handle variable before assigning it the result of
`Factor_graph::create_variable`.

When `Factor_graph` creates a variable, it returns a `Variable_node` whose
`index` is the position of that variable's internal `Variable_data` object.
That index is meaningful only to the graph that created it.

### `is_valid`

The validity check is:

```cpp
  [[nodiscard]] constexpr auto is_valid() const -> bool {
    return index != invalid_index;
  }
```

`[[nodiscard]]` tells the compiler that callers should not ignore the returned
boolean. The purpose of calling `is_valid()` is to ask a question, so silently
discarding the answer is probably a mistake.

`constexpr` allows the function to be evaluated at compile time when the input
object is known at compile time. It also signals that the function is tiny and
purely value-based.

The trailing return type:

```cpp
auto is_valid() const -> bool
```

means the function returns `bool`. This repository often uses trailing return
types for consistency.

The `const` after the parameter list means the function does not modify the
`Variable_node`.

The implementation checks whether the stored index differs from the invalid
sentinel. It does not check whether the index is in range for a particular
`Factor_graph`; only the graph can perform that stronger check because only the
graph owns the variable storage.

### Closing The Type, Namespace, And Include Guard

The struct closes with:

```cpp
};
```

The namespace closes with:

```cpp
} // namespace twalib
```

The comment makes it clear which namespace is being closed.

Finally, the include guard closes:

```cpp
#endif
```

## Important Invariants

- A default-constructed `Variable_node` is invalid.
- A valid `Variable_node` is a handle, not an owner.
- The `index` is meaningful only with the `Factor_graph` that created it.
- `is_valid()` only checks for the sentinel value; it does not prove that a
  graph still contains a variable at that index.

## Relationship To The Paper

The paper describes equality nodes that collect messages from neighboring
function cost nodes and form a consensus value. In this codebase,
`Variable_node` is the public handle for one of those equality nodes.

The actual equality-node state and update logic are implemented elsewhere:

- `src/graph/variable_data.hpp` stores per-variable state.
- `src/graph/factor_graph.cpp` owns all variables and runs the iteration.

`Variable_node` exists so user code can refer to an equality node without
needing access to those internal details.

## Extension Notes

Keep this type small and cheap to copy. It is used as an API token throughout
the graph-building code, so adding ownership or graph pointers here would make
the handle heavier and complicate lifetime rules.

If stronger validation is needed, add it to `Factor_graph`, where the graph can
check both `is_valid()` and whether the index is in range for its own internal
storage.
