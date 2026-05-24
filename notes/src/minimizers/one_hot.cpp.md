# src/minimizers/one_hot.cpp

## Role In The System

`one_hot.cpp` implements the one-hot constraint minimizer. A one-hot factor
connects to a set of variables and enforces that exactly one of them takes the
value 1 while all others take the value 0. This is the fundamental building
block of Sudoku: each row-digit, column-digit, box-digit, and cell constraint
is a one-hot factor.

The file provides:

- `create_one_hot_minimizer`, an anonymous-namespace helper that builds the
  `Minimization_function` lambda,
- `create_one_hot_factor`, the public function that wires up edges and
  registers the factor with the graph.

## Code Walkthrough

### Includes

```cpp
#include "twalib/minimizers/one_hot.hpp"
```

The own public header comes first, declaring the `create_one_hot_factor`
overloads.

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
```

`Graph_edge` is the typed edge handle. `Weighted_value` carries a
value/weight pair for incoming messages. `Weighted_value_exchange` is the
per-edge buffer the minimizer reads from and writes to, and is also the source
of the `Minimization_function` and `Random_engine` type aliases.

```cpp
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>
```

`<cstddef>` provides `std::size_t` for index arithmetic. `<initializer_list>`
supports the brace-list overload. `<optional>` lets the minimizer track
indices that may not exist yet. `<random>` supplies `std::uniform_int_distribution`
for tie-breaking. `<span>` is the exchange-buffer view type.
`<stdexcept>` provides `std::logic_error` and `std::invalid_argument`.
`<vector>` stores candidate index lists.

### Namespace

```cpp
namespace twalib {
namespace {
```

The outer namespace is `twalib`. The inner anonymous namespace hides the two
helper functions from other translation units. They are implementation details
that only `create_one_hot_factor` needs.

### `set_all`

```cpp
auto set_all(std::span<Weighted_value_exchange> exchanges, Weighted_value weighted_value) -> void {
  for (auto& exchange : exchanges) {
    exchange.set(weighted_value);
  }
}
```

A small utility that writes the same weighted value into every exchange slot.
The minimizer uses it to set all edges to zero before overwriting the chosen
one with 1.

### `choose_uniform`

```cpp
auto choose_uniform(std::span<const std::size_t> candidates, Random_engine& random) -> std::size_t {
  if (candidates.empty()) {
    throw std::logic_error("one_hot tie set is empty");
  }

  std::uniform_int_distribution<std::size_t> distribution{0, candidates.size() - 1};
  return candidates[distribution(random)];
}
```

When multiple edges tie for the best candidate, the minimizer picks one
uniformly at random using the graph's shared random engine. The throw guards
against a logic error in the caller; an empty candidate set should never
happen in a valid factor graph.

### `create_one_hot_minimizer`

```cpp
auto create_one_hot_minimizer(std::size_t num_variables) -> Minimization_function {
  return [num_variables](std::span<Weighted_value_exchange> exchanges, Random_engine& random) {
```

This function returns a `Minimization_function` lambda that captures
`num_variables`. That capture is needed to detect the case where all but one
edge sends infinite-weight zero, which implies the remaining edge must be 1.

#### Lambda State Variables

Inside the lambda, several local vectors and values track the incoming
messages:

```cpp
    std::vector<std::size_t> infinite_one_indices;
    std::optional<std::size_t> only_non_certain_zero_index;
    std::size_t num_infinite_zero = 0;
    std::vector<std::size_t> biggest_standard_indices;
    double biggest_standard_value = 0.0;
    std::vector<std::size_t> biggest_zero_indices;
    double biggest_zero_value = 0.0;
```

`infinite_one_indices` collects edges that claim with certainty to be 1.
`only_non_certain_zero_index` tracks the sole remaining non-certain-zero
edge when the process-of-elimination rule is about to fire.
`num_infinite_zero` counts edges that claim with certainty to be 0.
`biggest_standard_indices` and `biggest_standard_value` track the
standard-weight edges with the largest incoming value (the strongest
candidates). `biggest_zero_indices` and `biggest_zero_value` do the same for
zero-weight edges, used as a fallback when no standard-weight messages exist.

#### Classification Loop

```cpp
    for (std::size_t index = 0; index < exchanges.size(); ++index) {
      const Weighted_value incoming = exchanges[index].get();
```

The loop reads each incoming message and classifies it by weight.

```cpp
      if (incoming.weight == Message_weight::infinite) {
        if (incoming.value == 0.0) {
          ++num_infinite_zero;
        } else {
          infinite_one_indices.push_back(index);
        }
```

If the incoming weight is infinite, the message represents certainty. A value
of 0 means another factor is certain this edge is 0, so `num_infinite_zero`
increments. Any nonzero value means another factor is certain this edge is 1,
so the index goes into `infinite_one_indices`.

```cpp
      } else {
        only_non_certain_zero_index = index;
```

For non-infinite-weight messages, the index is unconditionally recorded in
`only_non_certain_zero_index`. This overwrites on each iteration, so after the
loop it holds the last such index — which is meaningful only when exactly one
non-certain-zero edge exists.

```cpp
        if (incoming.weight == Message_weight::zero) {
          if (biggest_zero_indices.empty() || incoming.value > biggest_zero_value) {
            biggest_zero_value = incoming.value;
            biggest_zero_indices.clear();
            biggest_zero_indices.push_back(index);
          } else if (incoming.value == biggest_zero_value) {
            biggest_zero_indices.push_back(index);
          }
```

Zero-weight edges (messages where the sender has no opinion) are tracked
separately. If this value exceeds the current best, the list is reset to just
this index. If it ties with the current best, the index is appended.

```cpp
        } else if (biggest_standard_indices.empty() || incoming.value > biggest_standard_value) {
          biggest_standard_value = incoming.value;
          biggest_standard_indices.clear();
          biggest_standard_indices.push_back(index);
        } else if (incoming.value == biggest_standard_value) {
          biggest_standard_indices.push_back(index);
        }
      }
    }
```

Standard-weight edges use the same tie-collection pattern: clear-and-push when
a new maximum is found, push when the value equals the current maximum. This
builds a list of all edges sharing the best value so that `choose_uniform` can
break the tie fairly. The closing braces end the else block and the for loop.

#### Decision Logic

After classification, the minimizer decides which edge should be 1:

```cpp
    Message_weight outgoing_weight = Message_weight::standard;
    std::optional<std::size_t> one_index;
```

The default outgoing weight is standard. `one_index` will hold the chosen
edge; it remains empty if no valid choice exists.

```cpp
    if (!infinite_one_indices.empty()) {
      outgoing_weight = Message_weight::infinite;
      one_index = choose_uniform(infinite_one_indices, random);
```

If any edge is certain to be 1, the factor picks one uniformly and sends
infinite weight — certainty propagates outward.

```cpp
    } else if (num_infinite_zero == num_variables - 1) {
      outgoing_weight = Message_weight::infinite;
      one_index = only_non_certain_zero_index;
```

If all edges except one are certain-zero, the remaining edge must be 1 by
elimination. The factor sends infinite weight because this deduction is
certain. `num_variables` (captured at creation time) is the total edge count.

```cpp
    } else if (!biggest_standard_indices.empty()) {
      one_index = choose_uniform(biggest_standard_indices, random);
```

Otherwise, the factor picks the standard-weight edge with the largest value.
The outgoing weight stays standard because this is not a certain deduction.

```cpp
    } else if (!biggest_zero_indices.empty()) {
      one_index = choose_uniform(biggest_zero_indices, random);
    }
```

As a final fallback, if only zero-weight messages exist, the factor picks the
largest of those. It still sends standard weight — the factor has an opinion
even when all incoming messages are zero-weight.

```cpp
    if (!one_index.has_value()) {
      throw std::logic_error("one_hot constraint has no feasible non-certain-zero variable");
    }
```

If none of the four cases fired, the factor has no feasible choice (all edges
are certain-zero with no remaining free edge). This should never happen in a
valid graph.

#### Writing Results

```cpp
    set_all(exchanges, Weighted_value{0.0, outgoing_weight});
    exchanges[*one_index].set(Weighted_value{1.0, outgoing_weight});
  };
}
```

First, every edge is set to 0 with the chosen outgoing weight. Then the
selected edge is overwritten with 1. This is the one-hot constraint: exactly
one edge gets 1, all others get 0. The closing `};` ends the lambda, and `}`
ends `create_one_hot_minimizer`.

Notice that `outgoing_weight` is one shared weight for the whole one-hot
decision. The graph's `Factor_data` layer performs one additional
algorithm-wide rule after the minimizer runs: if a specific incoming edge had
infinite weight, the outgoing message on that same edge is kept infinite. That
edge-by-edge certainty preservation is not special to one-hot factors; it is a
general TWA rule applied to all factor minimizers.

### Closing The Anonymous Namespace

```cpp
} // namespace
```

### `create_one_hot_factor` (span overload)

```cpp
auto create_one_hot_factor(
    Factor_graph& graph,
    std::span<const Variable_node> variables) -> Factor_node {
  if (variables.empty()) {
    throw std::invalid_argument("create_one_hot_factor requires at least one variable");
  }

  std::vector<Graph_edge> edges;
  edges.reserve(variables.size());
  for (const Variable_node variable : variables) {
    edges.push_back(graph.create_edge(variable));
  }

  return graph.create_factor(edges, create_one_hot_minimizer(variables.size()));
}
```

This is the public entry point. It validates that the variable list is
non-empty, then creates one edge per variable. The edges are collected into a
local vector so they can be passed to `graph.create_factor` along with the
minimization function returned by `create_one_hot_minimizer`.

`edges.reserve(variables.size())` avoids reallocations during the loop.

### `create_one_hot_factor` (initializer_list overload)

```cpp
auto create_one_hot_factor(
    Factor_graph& graph,
    std::initializer_list<Variable_node> variables) -> Factor_node {
  return create_one_hot_factor(
      graph,
      std::span<const Variable_node>{variables.begin(), variables.size()});
}
```

This overload lets callers write brace-enclosed variable lists directly:

```cpp
create_one_hot_factor(graph, {v1, v2, v3});
```

It simply wraps the initializer list in a `std::span` and forwards to the
primary overload.

### Closing The Namespace

```cpp
} // namespace twalib
```

## Important Invariants

- A one-hot factor must connect to at least one variable. The guard at the top
  of `create_one_hot_factor` enforces this.
- The minimizer always writes exactly one 1 and the rest 0. There is no
  partial-assignment path.
- Tie-breaking is uniform random using the graph's shared engine, ensuring
  reproducibility for a given seed.
- When the minimizer sends infinite weight, it is because certainty was
  already present in the incoming messages. The factor never spontaneously
  invents certainty.
- Edge-by-edge preservation of incoming infinite weights is handled by
  `Factor_data`, so all minimizers obey that TWA rule consistently.

## Relationship To The Paper

The one-hot constraint corresponds to the simplex projection described in
Section 4 of the paper (the Sudoku section). In the three-weight formulation,
the minimizer's job is to pick which edge should be 1. The weight it sends
depends on whether it can deduce the answer with certainty from incoming
infinite-weight messages (propagation of certainty) or must rely on the largest
incoming standard-weight value (iterative convergence).
