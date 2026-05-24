# Weights

This note explains how message weights work in this repository. It assumes the
basic message-passing picture from `notes/concepts/message_passing.md`: factors
send messages to variables, variables send messages back to factors, and each
edge stores the local values `x`, `z`, and `u`.

## What A Weighted Value Represents

The code packages a scalar value and a message weight together:

```cpp
struct Weighted_value {
  double value = 0.0;
  Message_weight weight = Message_weight::standard;
};
```

The value says what number the sender is proposing. The weight says how the
receiver should treat that proposal.

For example:

```cpp
Weighted_value{7.0, Message_weight::infinite}
```

means "the value should be 7, and I am certain."

This:

```cpp
Weighted_value{0.43, Message_weight::zero}
```

means "I am passing along the value 0.43, but I have no active opinion about
it."

## The Three Weights

The public weight type is:

```cpp
enum class Message_weight {
  zero,
  standard,
  infinite,
};
```

The intended meanings are:

- `zero`: no opinion,
- `standard`: ordinary finite opinion,
- `infinite`: certainty.

The helper function:

```cpp
message_weight_value(Message_weight weight)
```

maps those symbolic weights to numeric values:

- `zero` becomes `0.0`,
- `standard` becomes `1.0`,
- `infinite` becomes floating-point infinity.

Most of the graph code does not do arithmetic directly with those numeric
weights. It branches on the three symbolic cases instead. That keeps the TWA
rules explicit and avoids fragile computations involving infinity.

## Direction Names

The code uses "left" and "right" because the paper often pictures factor nodes
on the left and equality or variable nodes on the right.

The names correspond to:

```text
leftward / variable-to-factor: n = z - u
rightward / factor-to-variable: m = x + u
```

So:

- `weight_to_left()` is attached to the `n` message,
- `weight_to_right()` is attached to the `m` message.

The code does not store `n` and `m` as independent state. It computes them from
`x`, `z`, and `u`.

## Edge Storage

Each edge stores one weight for each direction:

```cpp
struct Weight_pair {
  Message_weight left = Message_weight::zero;
  Message_weight right = Message_weight::zero;
};
```

The weighted variable-to-factor message is:

```cpp
Weighted_value{message_to_factor(), weight_to_left()}
```

The weighted factor-to-variable message is:

```cpp
Weighted_value{message_to_variable(), weight_to_right()}
```

There is no runtime algorithm switch here. If a minimizer writes zero,
standard, or infinite weight, the edge stores that weight and later reports it
to the variable equality step.

## Initial And Reset Weights

When a variable is created, it has an initial `Weighted_value`.

When an edge is created, it is initialized from the associated variable's
initial value and initial weight.

The lower-level edge reset function takes a `Weighted_value`:

```cpp
z_ = reset_value.value;
set_weight_to_left(reset_value.weight);
set_weight_to_right(Message_weight::zero);
```

The parameter name is `reset_value` inside `Edge_data::reset`, because the
caller decides what value to pass. During a full graph reinitialize, the caller
passes the variable's original initial value:

```cpp
edges_[edge.index].reset(variable.initial_value());
```

When a disabled factor is enabled again, the caller instead resets that edge to
the variable's current belief with standard weight:

```cpp
edges_[edge.index].reset(Weighted_value{variables_[variable.index].value(), Message_weight::standard});
```

So an edge reset does not always mean "go back to the value from variable
creation." It means "forget the edge's local `x`/`u` history and start its
variable-to-factor side from the `Weighted_value` supplied by the caller."

In all cases, the factor-to-variable direction begins with zero weight after an
edge reset. That default makes sense because before the first factor
minimization, the factor side has not yet formed an opinion.

## Factor-Side Weights

Each factor minimizer receives incoming weighted messages from the variable
side, then writes outgoing weighted messages back to the factor's exchanges.

Examples:

```cpp
exchanges[0].set(Weighted_value{value, Message_weight::infinite});
```

`known_value` uses infinite weight because it represents certainty.

```cpp
exchanges[0].set(Weighted_value{incoming.value, Message_weight::zero});
```

`in_range` uses zero weight when the incoming value is already inside the
allowed interval. The constraint is satisfied, so the factor has no need to
push.

```cpp
exchanges[0].set(Weighted_value{lower, Message_weight::standard});
```

`in_range` uses standard weight when it clamps a value to the interval
boundary. The factor is actively correcting the value, but it is not claiming
certainty.

## Certainty Preservation

The graph preserves incoming certainty on the same edge.

During the factor pass, `Factor_data` records whether each incoming
variable-to-factor message was infinite:

```cpp
incoming_infinite_weights_[index] = incoming.weight == Message_weight::infinite;
```

After the minimizer runs, it restores infinite weight on that edge if
necessary:

```cpp
if (incoming_infinite_weights_[index]) {
  result.weight = Message_weight::infinite;
}
```

This is important because a minimizer might compute a sensible outgoing value
without remembering that the input was certain. The graph-level rule prevents
certainty from being accidentally weakened.

## Variable Equality

The variable equality step combines incoming factor-to-variable messages.

If any incoming message is infinite, that message wins:

```cpp
if (message.weight == Message_weight::infinite) {
  return message;
}
```

If there are standard messages, the variable averages only those:

```cpp
if (standard_count > 0) {
  return Weighted_value{
      standard_sum / static_cast<double>(standard_count),
      Message_weight::standard};
}
```

If there are no standard or infinite messages, every message has zero weight.
The variable averages all incoming values but marks the result zero weight:

```cpp
return Weighted_value{
    all_sum / static_cast<double>(enabled_edges.size()),
    Message_weight::zero};
```

This rule is why zero-weight messages are genuinely inactive in the consensus
calculation when any standard message is present.

## Updating `u`

The ordinary disagreement update is:

```cpp
u_ += alpha * (x_ - z_);
```

The code resets `u` to zero instead when:

- either direction has infinite weight,
- the factor-to-variable direction has zero weight,
- the graph identifies this edge as the lone standard-weight message into its
  variable.

In code:

```cpp
return reset_disagreement || weight_to_left() == Message_weight::infinite ||
       weight_to_right() == Message_weight::infinite || weight_to_right() == Message_weight::zero;
```

These reset cases prevent stale disagreement from continuing to push an edge
when the edge is certain, inactive, or not actually disagreeing with a
competing standard-weight message.

## Examples By Problem

In Sudoku, `one_hot` factors use weights to express logical strength:

- a known clue becomes infinite weight,
- a chosen ordinary candidate is standard weight,
- a rejected or inactive candidate can be zero weight,
- if only one candidate remains possible, the winning message becomes
  infinite.

In circle packing, an intersection factor emits zero-weight messages when two
circles are already separated. It emits standard-weight messages when they
overlap and need to be moved apart.

This is why fast circle packing can disable far-away intersection factors as an
efficiency layer: those factors are expected to be inactive and would otherwise
send zero-weight messages.

## Invariants

- Minimiziers should use `Message_weight::zero` for "no active opinion," not
  for "the numeric value zero."
- `Message_weight::infinite` means certainty and should be used sparingly.
- A message's weight belongs to a direction. The same edge can have different
  weights leftward and rightward.
- Disabled factors are an optimization for factors that do not need to
  participate right now; reenabled edges reset their local history.
- Certainty preservation is a graph-level rule in `Factor_data`, not a rule
  that every minimizer should duplicate.

## Where To Read Next

- `notes/concepts/message_passing.md`: the full iteration cycle.
- `notes/src/graph/edge_data.hpp.md`: how a single edge stores values,
  weights, and `u`.
- `notes/src/graph/factor_data.hpp.md`: how minimizer results are exchanged
  with edges.
- `notes/src/graph/factor_graph.cpp.md`: how variable equality is implemented.
