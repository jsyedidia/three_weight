# src/graph/edge_data.hpp

## Role In The System

`Edge_data` is the internal storage for one graph edge. It is not part of the
public API; users hold `Graph_edge` handles, while `Factor_graph` owns the
actual `Edge_data` objects.

An edge connects one factor to one variable. It stores:

- the factor-side local value `x`,
- the variable-side consensus value `z`,
- the accumulated disagreement `u`,
- the leftward and rightward message weights,
- whether the edge is currently enabled,
- enough previous-message state to test convergence.

The public concept note `notes/concepts/message_passing.md` explains the
algorithmic picture. This file note focuses on how the C++ class implements
that picture.

## State At A Glance

The private data members are easiest to understand before walking through the
functions:

```cpp
  Variable_node variable_;
  double x_ = 0.0;
  double u_ = 0.0;
  double z_ = 0.0;
  std::optional<double> old_message_to_factor_;
  std::optional<double> message_difference_;
  Weight_pair weights_;
  bool enabled_ = true;
```

`variable_` is the variable endpoint of this edge. It is fixed when the edge is
created.

`x_` is the factor-side local value. `z_` is the variable-side consensus value.
`u_` is the accumulated disagreement that shifts the messages in both
directions.

`old_message_to_factor_` and `message_difference_` are convergence bookkeeping.
The first stores the previous leftward message when one exists; the second
stores the absolute change from that previous message.

`weights_` stores one symbolic message weight for each direction. The nested
`Weight_pair` type is defined later in the source, but conceptually it holds a
leftward variable-to-factor weight and a rightward factor-to-variable weight.

`enabled_` records whether this edge currently participates in factor
minimization, variable consensus, and convergence checks.

## Code Walkthrough

### Include Guard

The include guard:

```cpp
#ifndef TWALIB_SRC_GRAPH_EDGE_DATA_HPP
#define TWALIB_SRC_GRAPH_EDGE_DATA_HPP
```

prevents the header from being processed more than once in a single
translation unit. The name includes `SRC_GRAPH` because this is an
implementation header under `src/`, not a public header under `include/`.

### Includes

The dependencies are:

```cpp
#include "twalib/graph/variable_node.hpp"
#include "twalib/graph/weighted_value.hpp"
```

`Variable_node` identifies the variable attached to this edge.
`Weighted_value` supplies both `Weighted_value` and `Message_weight`.

The standard headers are:

```cpp
#include <cmath>
#include <optional>
```

`<cmath>` provides `std::abs` for convergence differences. `<optional>` stores
values that may not exist yet, such as the previous message before the first
iteration has completed.

### Namespace

The namespace is:

```cpp
namespace twalib::detail {
```

`detail` marks this class as internal. It is available to implementation files
and internal tests, but it is not the API users should build against.

### Class And Public Section

The class begins:

```cpp
class Edge_data {
 public:
```

It is a small value-like object owned in a `std::vector` by
`Factor_graph::Impl`.

### Constructor

The constructor is:

```cpp
  Edge_data(Variable_node variable, Weighted_value initial_value)
      : variable_(variable) {
    reset(initial_value);
  }
```

The member initializer list stores the attached variable handle. The body calls
`reset` so that construction and later reinitialization use one path for
setting `x`, `z`, `u`, weights, and convergence bookkeeping.

### `reset`

The reset function is:

```cpp
  auto reset(Weighted_value reset_value) -> void {
    enabled_ = true;
    x_ = 0.0;
    u_ = 0.0;
    z_ = reset_value.value;
    set_weight_to_left(reset_value.weight);
    set_weight_to_right(Message_weight::zero);
    old_message_to_factor_.reset();
    message_difference_.reset();
  }
```

Reset reenables the edge, clears factor-side state, clears accumulated
disagreement, and sets the variable-side consensus value from the caller's
`reset_value`.

The leftward, variable-to-factor weight also comes from `reset_value`. The
rightward, factor-to-variable weight starts as zero because the factor has not
yet made a new proposal. The optional convergence fields are cleared because a
fresh edge has no previous message to compare with.

`Factor_graph::reinitialize()` calls this with the variable's original initial
value. Reenabling a dynamic factor calls this with the variable's current value
and standard weight.

### `disable`

The disable function is intentionally small:

```cpp
  auto disable() -> void {
    enabled_ = false;
  }
```

Disabling an edge means the graph should ignore it in factor minimization,
variable consensus, and convergence checks. The old numeric state remains in
the object, but it is not active until the edge is reset and reenabled.

### Basic Accessors

The variable accessor is:

```cpp
  [[nodiscard]] auto variable() const -> Variable_node {
    return variable_;
  }
```

It returns the variable handle this edge connects to. `[[nodiscard]]` asks the
compiler to warn if a caller ignores the result.

The enabled accessor is:

```cpp
  [[nodiscard]] auto is_enabled() const -> bool {
    return enabled_;
  }
```

It is used by graph passes and tests to filter active edges.

The raw state accessors are:

```cpp
  [[nodiscard]] auto x() const -> double {
    return x_;
  }

  [[nodiscard]] auto z() const -> double {
    return z_;
  }

  [[nodiscard]] auto u() const -> double {
    return u_;
  }
```

They expose the stored factor-side value, variable-side value, and disagreement
term. These are mostly for internal tests and debugging.

### Message Values

The variable-to-factor message is:

```cpp
  [[nodiscard]] auto message_to_factor() const -> double {
    return z_ - u_;
  }
```

This is the paper's leftward message, often written as `n`.

The factor-to-variable message is:

```cpp
  [[nodiscard]] auto message_to_variable() const -> double {
    return x_ + u_;
  }
```

This is the paper's rightward message, often written as `m`.

### Weighted Messages

The weighted leftward message is:

```cpp
  [[nodiscard]] auto weighted_message_to_factor() const -> Weighted_value {
    return Weighted_value{message_to_factor(), weight_to_left()};
  }
```

It combines the computed scalar message with the stored leftward weight.
Factors read this value before running their minimizers.

The weighted rightward message is:

```cpp
  [[nodiscard]] auto weighted_message_to_variable() const -> Weighted_value {
    return Weighted_value{message_to_variable(), weight_to_right()};
  }
```

Variables read this value during the equality or consensus step.

### `message_difference`

The convergence accessor is:

```cpp
  [[nodiscard]] auto message_difference() const -> std::optional<double> {
    return message_difference_;
  }
```

There is no difference before the edge has enough history, so the return type
is `std::optional<double>`.

### `set_result_from_factor`

The factor update is:

```cpp
  auto set_result_from_factor(Weighted_value result) -> void {
    x_ = result.value;
    set_weight_to_right(result.weight);

    const double current_message_to_factor = message_to_factor();
    if (old_message_to_factor_.has_value()) {
      message_difference_ = std::abs(current_message_to_factor - *old_message_to_factor_);
    }
    old_message_to_factor_ = current_message_to_factor;

    if (weight_to_right() == Message_weight::infinite) {
      u_ = 0.0;
    }
  }
```

A factor minimizer has produced a new factor-side local value and outgoing
weight. The edge stores those as `x_` and the rightward weight.

The middle block updates convergence bookkeeping. The graph tracks changes in
the variable-to-factor message. On the first factor update there is no
previous value, so no difference is available yet. After that, the absolute
difference is stored for convergence checks.

If the factor sends certainty, accumulated disagreement should not keep
offsetting the message. The paper resets `u` to zero when either direction has
infinite weight.

### `set_result_from_variable`

The variable update is:

```cpp
  auto set_result_from_variable(Weighted_value result, double alpha, bool reset_disagreement = false) -> void {
    z_ = result.value;
    set_weight_to_left(result.weight);

    if (should_reset_disagreement(reset_disagreement)) {
      u_ = 0.0;
    } else {
      u_ += alpha * (x_ - z_);
    }
  }
```

The variable consensus pass has produced a new shared value and leftward
weight. The edge stores those as `z_` and the leftward weight.

`alpha` is the graph learning rate. If no reset rule applies, `u` accumulates
the factor/variable disagreement `x - z`. Otherwise, the disagreement is
cleared.

### Private Section

The private section begins:

```cpp
 private:
```

Everything below this point supports the public edge operations.

### `Weight_pair`

The directional weights are stored as:

```cpp
  struct Weight_pair {
    Message_weight left = Message_weight::zero;
    Message_weight right = Message_weight::zero;
  };
```

`left` means variable-to-factor. `right` means factor-to-variable. Both default
to zero weight, matching an edge with no active opinion yet.

### Weight Accessors

The weight getters are direct:

```cpp
  [[nodiscard]] auto weight_to_left() const -> Message_weight {
    return weights_.left;
  }

  [[nodiscard]] auto weight_to_right() const -> Message_weight {
    return weights_.right;
  }
```

The edge reports the weights it stores. There is no alternate interpretation or
runtime algorithm branch.

The setters are also direct:

```cpp
  auto set_weight_to_left(Message_weight weight) -> void {
    weights_.left = weight;
  }

  auto set_weight_to_right(Message_weight weight) -> void {
    weights_.right = weight;
  }
```

Keeping the setters as small helpers makes the update functions read in the
same vocabulary as the paper: set the leftward weight, set the rightward
weight.

### `should_reset_disagreement`

The reset predicate is:

```cpp
  [[nodiscard]] auto should_reset_disagreement(bool reset_disagreement) const -> bool {
    return reset_disagreement || weight_to_left() == Message_weight::infinite ||
           weight_to_right() == Message_weight::infinite || weight_to_right() == Message_weight::zero;
  }
```

`reset_disagreement` is supplied by `Factor_graph` for the lone-standard-message
case. The other cases are local edge facts:

- certainty in either direction resets `u`,
- a zero rightward message resets `u` because that factor did not participate
  in the consensus.

The line break keeps the long boolean expression readable without changing the
logic.

### Data Members

The data members are:

```cpp
  Variable_node variable_;
  double x_ = 0.0;
  double u_ = 0.0;
  double z_ = 0.0;
  std::optional<double> old_message_to_factor_;
  std::optional<double> message_difference_;
  Weight_pair weights_;
  bool enabled_ = true;
```

`variable_` is stable for the life of the edge. The three doubles are the
message-passing state. The two optionals support convergence checking.
`weights_` stores the directional TWA weights. `enabled_` supports dynamic
factor activation, especially in fast circle packing.

### Closing The File

The file closes the namespace and include guard:

```cpp
} // namespace twalib::detail

#endif
```

## Important Invariants

- `message_to_factor()` is always `z - u`.
- `message_to_variable()` is always `x + u`.
- Reset clears `x`, clears `u`, clears convergence history, and starts the
  rightward weight at zero.
- Disabled edges keep their stored values but are ignored until reset and
  reenabled.
- The edge reports its actual stored weights; there is no alternate weight
  interpretation.
- Infinite weights and inactive rightward messages reset `u`.

## Relationship To The Paper

The names line up with the paper's TWA notation:

- `x_`: factor-side local value,
- `z_`: equality-node or variable consensus value,
- `u_`: accumulated disagreement,
- `message_to_factor()`: leftward `n = z - u`,
- `message_to_variable()`: rightward `m = x + u`,
- `Message_weight`: zero, standard, or infinite weight.

## Extension Notes

If new minimizers are added, they should express "no active opinion" with
`Message_weight::zero`, ordinary finite pushes with `Message_weight::standard`,
and certainty with `Message_weight::infinite`.

If new dynamic-factor behavior is added, reenabled edges should usually be
reset from an explicit caller-supplied `Weighted_value`, just as the current
fast circle-packing path does.
