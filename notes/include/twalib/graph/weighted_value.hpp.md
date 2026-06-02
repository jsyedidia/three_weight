# include/twalib/graph/weighted_value.hpp

## Role In The System

This header defines the small value types used to pair a numeric belief with
one of the Three-Weight Algorithm's symbolic message weights.

The core idea is that a message is not only a number. It also says how strongly
that number should matter:

- zero weight means "ignore this value",
- standard weight means "ordinary finite opinion",
- infinite weight means "certain value."

The concept note [`notes/concepts/weights.md`](../../../concepts/weights.md) explains the algorithmic meaning
of these weights in more detail.

## Main Types

This header defines:

```cpp
enum class Message_weight
struct Weighted_value
```

`Message_weight` is the symbolic weight. `Weighted_value` bundles a `double`
with one of those weights.

It also defines:

```cpp
message_weight_value
```

which converts a symbolic weight to the corresponding numeric weight when the
algorithm needs arithmetic.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_GRAPH_WEIGHTED_VALUE_HPP
#define TWALIB_GRAPH_WEIGHTED_VALUE_HPP
```

It prevents duplicate processing of this public header.

### Includes

The one standard include is:

```cpp
#include <limits>
```

`message_weight_value` uses `std::numeric_limits<double>::infinity()` for the
numeric representation of infinite weight.

### Namespace

The declarations live in:

```cpp
namespace twalib {
```

These types are part of the public graph API.

### `Message_weight`

The weight enum is:

```cpp
enum class Message_weight {
  zero,
  standard,
  infinite,
};
```

`enum class` creates a scoped enumeration. Callers write
`Message_weight::zero`, not just `zero`, which avoids name collisions and makes
the code more explicit.

The three cases are exactly the three weights used by the algorithm. They are
symbolic rather than arbitrary floating-point weights because TWA's behavior
depends on these categories.

### `message_weight_value`

The conversion function begins:

```cpp
[[nodiscard]] constexpr auto message_weight_value(Message_weight weight) -> double {
```

`[[nodiscard]]` says the numeric result should be used. `constexpr` allows the
conversion to happen at compile time when the input is known. The trailing
return type says the function returns `double`.

The switch handles each symbolic case:

```cpp
  switch (weight) {
  case Message_weight::zero:
    return 0.0;
  case Message_weight::standard:
    return 1.0;
  case Message_weight::infinite:
    return std::numeric_limits<double>::infinity();
  }
```

Zero weight maps to `0.0`, standard weight maps to `1.0`, and infinite weight
maps to floating-point infinity.

The final line is:

```cpp
  return 0.0;
```

All current enum cases return from inside the switch. This final return keeps
the function well-formed for compilers that require every non-void function to
end with a return statement.

The function closes with:

```cpp
}
```

### `Weighted_value`

The bundle type is:

```cpp
struct Weighted_value {
  double value = 0.0;
  Message_weight weight = Message_weight::standard;
};
```

`value` is the numeric belief. `weight` is the symbolic strength of that
belief.

Default construction creates the ordinary value `0.0` with standard weight.
Many graph-building APIs accept or produce this pair because TWA messages
always need both pieces of information.

### Closing The File

The namespace and include guard close with:

```cpp
} // namespace twalib

#endif
```

## Important Invariants

- TWA uses only the three symbolic categories in `Message_weight`.
- Standard weight has numeric value `1.0`.
- Infinite weight means certainty, not merely a very large finite preference.
- A `Weighted_value` is just data; it does not validate whether the value makes
  sense for a particular problem domain.

## Relationship To The Paper

The paper describes messages with values and weights. In this codebase,
`Weighted_value` is the public representation of that pair, and
`Message_weight` represents the paper's zero, standard, and infinite weights.

## Extension Notes

If the algorithm is extended to support arbitrary finite weights, this header
would need a deeper redesign. The current enum deliberately encodes the
three-weight algorithm, not general weighted least squares.
