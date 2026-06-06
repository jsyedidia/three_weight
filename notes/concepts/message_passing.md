# Message Passing

This note explains the message-passing model implemented by this repository.
It connects the mathematical picture from the paper to the code-level objects:
variables, factors, edges, weighted values, minimizers, and graph iterations.

## The Big Picture

The solver represents an optimization problem as a factor graph.

There are two kinds of nodes:

- variable nodes, which hold the current shared belief for a quantity,
- factor nodes, which represent local constraints or local objective terms.

An edge connects one factor to one variable. The same quantity, but possibly
with temporarily different estimates for its value, is therefore represented in
two local places:

- the factor's local copy, called `x` in the code,
- the variable's shared consensus value, called `z` in the code.

The algorithm repeatedly asks:

1. Given the current messages from the variables, what does each factor want?
2. Given the current messages from the factors, what consensus should each
   variable enforce?
3. Did the variable beliefs stop changing?

When every variable belief value changes by at most the convergence threshold,
the graph is considered converged. Callers can also add domain-specific
satisfaction checks for problems where stable beliefs are necessary but not
quite enough.

## Factor Graphs In This Repository

The public graph type is `Factor_graph`.

Users create:

- variables with `create_variable`,
- edges with `create_edge`,
- factors with `create_factor`.

Problem builders such as Sudoku and circle packing do this construction for
larger examples. Minimizer helpers such as `known_value`, `in_range`,
`one_hot`, and `spy` create common factor behaviors.

Internally, the graph stores:

- `Variable_data` for variable-side state,
- `Factor_data` for factor-side state,
- `Edge_data` for per-edge message state.

The internal classes live under `src/graph` because they are implementation
details. Users normally interact with the public handles `Variable_node`,
`Factor_node`, and `Graph_edge`.

## What An Edge Stores

Each edge stores the state needed to pass messages in both directions.

The core scalar values are:

```text
x: the factor-side local value
z: the variable-side consensus value
u: the accumulated disagreement
```

The code keeps these in `Edge_data` as:

```cpp
double x_ = 0.0;
double u_ = 0.0;
double z_ = 0.0;
```

The edge does not store independent `m` and `n` message values. It computes
them from `x`, `z`, and `u`.

## The Two Message Directions

The message sent from a variable to a factor is:

```text
n = z - u
```

In code:

```cpp
auto message_to_factor() const -> double {
  return z_ - u_;
}
```

The message sent from a factor to a variable is:

```text
m = x + u
```

In code:

```cpp
auto message_to_variable() const -> double {
  return x_ + u_;
}
```

This means `u` shifts the raw local values before they are presented as
messages. Intuitively, `u` remembers accumulated disagreement between the
factor's local copy and the variable's consensus copy.

## Weighted Values

A message is not just a number. It is a number plus a symbolic weight:

```cpp
struct Weighted_value {
  double value = 0.0;
  Message_weight weight = Message_weight::standard;
};
```

The value says what location the sender prefers. The weight says how strongly
that preference should be treated.

TWA has three message weights:

```cpp
enum class Message_weight {
  zero,
  standard,
  infinite,
};
```

Their meanings are:

- `zero`: no opinion,
- `standard`: ordinary finite opinion,
- `infinite`: certainty.

## One Iteration

The main iteration happens in `Factor_graph::Impl::iterate`.

Conceptually, each iteration has two passes:

1. factor pass,
2. variable pass.

The factor pass is:

```cpp
for (auto& factor : factors_) {
  factor.minimize(edges_, random_);
}
```

Each factor reads incoming variable-to-factor messages from its edges, runs its
minimizer, and writes outgoing factor-to-variable messages.

The variable pass is:

```cpp
for (auto& variable : variables_) {
  const auto enabled_edges = variable.enabled_edges(edges_);
  const Weighted_value result = enforce_variable_equality(enabled_edges);
  const bool has_lone_standard_message = has_lone_standard_message_to_variable(enabled_edges);
  variable.update_result(result);

  for (const Graph_edge edge : enabled_edges) {
    const bool reset_disagreement =
        has_lone_standard_message &&
        edges_[edge.index].weighted_message_to_variable().weight == Message_weight::standard;
    edges_[edge.index].set_result_from_variable(result, learning_rate_, reset_disagreement);
  }
}
```

Each variable reads the messages from all enabled incident factors, computes a
consensus value, stores that as the variable's current belief, and sends the
result back through each edge. The `reset_disagreement` boolean variable determines
whether disagreement `u_` variables should be reset to zero as described below.

Incident factors may be disabled for efficiency when we know that they would
send zero-weight messages that would not participate in consensus. Fast circle
packing uses this idea for distant circle pairs.

## The Factor Pass

A factor owns a minimization function. In code, this is:

```cpp
using Minimization_function =
    std::function<void(std::span<Weighted_value_exchange>, Random_engine&)>;
```

The graph does not know the details of each constraint. It only knows that a
factor can be called with:

- a span of `Weighted_value_exchange` objects,
- a random engine for randomized tie-breaking.

Before calling the minimizer, `Factor_data` fills each exchange with the
incoming weighted message from the variable side:

```cpp
const Weighted_value incoming = edge_for(exchange, edge_data).weighted_message_to_factor();
exchange.set(incoming);
```

The minimizer then changes the exchanges. For example, an `in_range` minimizer
may clamp a value to the nearest boundary, while a `one_hot` minimizer chooses
which option should be on.

After the minimizer runs, `Factor_data` writes the exchange values back to the
edges:

```cpp
edge_for(exchange, edge_data).set_result_from_factor(result);
```

That updates the edge's `x` value and the right-going message weight.

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

This implements the paper's rule that incoming certainty for an edge is
maintained in the outgoing weight. It also keeps individual minimizers simpler:
they can compute their local best values without each one reimplementing this
graph-level rule.

## The Variable Pass

The variable pass is the equality or consensus step. Every enabled edge
attached to the same variable is supposed to agree on one shared value.

TWA equality uses the message weights:

1. If any incoming factor-to-variable message has infinite weight, that message
   wins.
2. Otherwise, if there are standard-weight messages, average only those.
3. Otherwise, all incoming messages have zero weight, so average all values but
   mark the result as zero weight.

In code this is `enforce_variable_equality`:

```cpp
if (message.weight == Message_weight::infinite) {
  return message;
}
if (message.weight == Message_weight::standard) {
  standard_sum += message.value;
  ++standard_count;
}
all_sum += message.value;
```

Then:

```cpp
if (standard_count > 0) {
  return Weighted_value{
      standard_sum / static_cast<double>(standard_count),
      Message_weight::standard};
}
```

And finally:

```cpp
return Weighted_value{
    all_sum / static_cast<double>(enabled_edges.size()),
    Message_weight::zero};
```

This is the "concur" step: the variable node forms a consensus belief from
the factor-to-variable messages.

## Updating `u`

After the variable consensus `z` is known, each edge updates its accumulated
disagreement `u`.

The ordinary update is:

```text
u = u + alpha * (x - z)
```

In code:

```cpp
u_ += alpha * (x_ - z_);
```

Some cases reset `u` to zero instead:

- the left-going message is infinite,
- the right-going message is infinite,
- the right-going message has zero weight,
- the graph found that this edge is the lone standard-weight message into its
  variable.

Those cases match the reasoning in the paper: if an edge is certain, inactive,
or not actually in disagreement with any competing standard message, the stored
disagreement should not keep pushing future messages.

## Convergence

After each variable pass, the graph compares every variable's previous belief
value with its newly computed consensus value:

```cpp
beliefs_converged = beliefs_converged && belief_converged(variable.value(), result.value);
```

The helper applies the graph's configured threshold:

```cpp
return std::abs(old_value - new_value) <= convergence_delta_;
```

For ordinary `iterate()` and `iterate_until_converged()`, belief stability is
the stopping rule. Some domains need an additional problem-level check. Circle
packing can ask for both stable beliefs and `max_overlap <= tolerance` by using
`Factor_graph::iterate_until_satisfied()`.

`Edge_data` still stores the previous variable-to-factor message value after
each factor update:

```cpp
message_difference_ = std::abs(current_message_to_factor - *old_message_to_factor_);
```

`Factor_graph::max_message_difference()` exposes the largest enabled-edge
message change as a diagnostic. That quantity tracks dual-state motion; it is
not the default stopping criterion because the messages can have a stable
limit cycle after the variable beliefs have settled.

## Dynamic Factors

Factors can be enabled and disabled. This is used most visibly in fast circle
packing.

The full circle-packing builder creates all pairwise intersection factors. The
fast builder creates the same factors, disables the dynamic pair factors, and
reenables only nearby pairs as the layout evolves.

This is an efficiency layer. Distant circle-pair factors would emit zero-weight
messages in TWA, so leaving them disabled avoids work without changing the
intended consensus calculation for active constraints.
