# src/graph/factor_graph.cpp

## Role In The System

`factor_graph.cpp` is the central implementation file. It contains the
`Factor_graph::Impl` class (the PImpl body) and the thin forwarding methods of
`Factor_graph` itself.

This file is responsible for:

- owning all variable, edge, and factor storage,
- graph construction (creating variables, edges, and factors),
- the main iteration loop (factor pass, variable pass, convergence check),
- dynamic factor enable/disable logic,
- full graph reinitialization,
- iteration and reinitialize callbacks.

It deliberately does not own:

- minimizer logic (that lives in `Minimization_function` callbacks),
- the public header API shape (that lives in `factor_graph.hpp`),
- the internal data classes (those are in `edge_data.hpp`,
  `variable_data.hpp`, `factor_data.hpp`).

## State At A Glance

The `Impl` class holds all graph state. The private data members are easiest to
understand before walking through the functions:

```cpp
  double learning_rate_;
  double convergence_delta_;
  Random_engine random_;
  std::size_t iterations_ = 0;
  bool converged_ = false;
  std::vector<detail::Variable_data> variables_;
  std::vector<detail::Edge_data> edges_;
  std::vector<detail::Factor_data> factors_;
  std::vector<std::function<void()>> iteration_callbacks_;
  std::vector<std::function<void()>> reinitialize_callbacks_;
```

`learning_rate_` is the `alpha` scale factor for `u` updates.
`convergence_delta_` is the threshold below which variable belief-value changes
are considered converged.

`random_` is a `std::mt19937_64` engine passed to minimizers for tie-breaking.

`iterations_` counts how many full iterations have been performed. `converged_`
records whether the last iteration found all variable belief values stable.

`variables_`, `edges_`, and `factors_` are the three parallel storage vectors,
one entry per variable node, graph edge, and factor node respectively.

`iteration_callbacks_` holds functions called after each iteration completes.
`reinitialize_callbacks_` holds functions called after a full reinitialization.

## Code Walkthrough

### Includes

The file begins:

```cpp
#include "twalib/graph/factor_graph.hpp"

#include "graph/edge_data.hpp"
#include "graph/factor_data.hpp"
#include "graph/variable_data.hpp"
```

The first include is the class's own public header, following the convention
that a `.cpp` file includes its own header first. The next three are the
internal data classes. Because this is the implementation file, it is allowed
to see the `detail` namespace.

The standard headers are:

```cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <utility>
```

`<algorithm>` supplies `std::ranges::count_if` for counting enabled
edges/factors. `<cmath>` provides `std::abs` for belief-value differences.
`<cstddef>` provides `std::size_t`. `<cstdint>` provides `std::uint64_t` for
the random seed. `<initializer_list>` supports the brace-list overload of
`create_factor`. `<optional>` provides the return type for message-difference
diagnostics. `<stdexcept>` provides
`std::out_of_range` and `std::logic_error`. `<utility>` supplies `std::move`.

### Namespace

```cpp
namespace twalib {
```

Everything in this file lives in the `twalib` namespace. The `detail` sub-
namespace is used only by the included internal headers.

### `Impl` Class And Constructor

The PImpl body begins:

```cpp
class Factor_graph::Impl {
 public:
  Impl(double learning_rate, double convergence_delta, std::uint64_t random_seed)
      : learning_rate_(learning_rate),
        convergence_delta_(convergence_delta),
        random_(random_seed) {}
```

The constructor stores the algorithm parameters and seeds the random engine.
The vectors start empty; they grow as the user creates variables, edges, and
factors.

### Parameter Accessors

```cpp
  [[nodiscard]] auto learning_rate() const -> double {
    return learning_rate_;
  }

  auto set_learning_rate(double learning_rate) -> void {
    learning_rate_ = learning_rate;
  }

  [[nodiscard]] auto convergence_delta() const -> double {
    return convergence_delta_;
  }

  auto set_convergence_delta(double convergence_delta) -> void {
    convergence_delta_ = convergence_delta;
  }

  auto set_random_seed(std::uint64_t random_seed) -> void {
    random_.seed(random_seed);
  }
```

These let users tune algorithm behavior at any time. Changing the learning rate
or convergence delta mid-run is safe; the next iteration will simply use the
new values.

### Status Accessors

```cpp
  [[nodiscard]] auto iterations() const -> std::size_t {
    return iterations_;
  }

  [[nodiscard]] auto converged() const -> bool {
    return converged_;
  }
```

Clients can check how many iterations have run and whether convergence was
reached.

### Size Accessors

```cpp
  [[nodiscard]] auto num_variables() const -> std::size_t {
    return variables_.size();
  }

  [[nodiscard]] auto num_edges() const -> std::size_t {
    return edges_.size();
  }

  [[nodiscard]] auto num_enabled_edges() const -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(edges_, [](const detail::Edge_data& edge) {
      return edge.is_enabled();
    }));
  }

  [[nodiscard]] auto num_factors() const -> std::size_t {
    return factors_.size();
  }

  [[nodiscard]] auto num_enabled_factors() const -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(factors_, [](const detail::Factor_data& factor) {
      return factor.is_enabled();
    }));
  }

  [[nodiscard]] auto max_message_difference() const -> std::optional<double> {
    double max_difference = 0.0;

    for (const auto& edge : edges_) {
      if (!edge.is_enabled()) {
        continue;
      }

      const auto difference = edge.message_difference();
      if (!difference.has_value()) {
        return std::nullopt;
      }
      max_difference = std::max(max_difference, *difference);
    }

    return max_difference;
  }
```

`num_variables`, `num_edges`, and `num_factors` return total counts.
`num_enabled_edges` and `num_enabled_factors` scan linearly and count only the
active subset. These are diagnostic helpers, not hot-path functions.

`max_message_difference` scans enabled edges and returns the largest available
message difference. If any enabled edge has not yet accumulated enough history,
the result is `std::nullopt`. This is a diagnostic for message/dual state; it
does not decide default convergence.

### `create_variable`

```cpp
  [[nodiscard]] auto create_variable(double initial_value, Message_weight initial_weight) -> Variable_node {
    const auto variable = Variable_node{variables_.size()};
    variables_.emplace_back(Weighted_value{initial_value, initial_weight});
    return variable;
  }
```

A new `Variable_data` is appended to the vector. The returned `Variable_node`
handle holds the new index. The initial value and weight become the variable's
starting belief and are also stored for future reinitialization.

### `create_edge`

```cpp
  [[nodiscard]] auto create_edge(Variable_node variable) -> Graph_edge {
    validate_variable(variable);

    const auto edge = Graph_edge{edges_.size()};
    edges_.emplace_back(variable, variables_[variable.index].initial_value());
    variables_[variable.index].add_edge(edge);
    return edge;
  }
```

The caller specifies which variable this edge connects to. A new `Edge_data` is
appended, initialized with the variable's initial weighted value (so `z` starts
at the variable's belief and the leftward weight matches the variable's initial
weight). The edge handle is registered with the variable's incident-edge list.

### `create_factor`

```cpp
  [[nodiscard]] auto create_factor(
      std::span<const Graph_edge> factor_edges,
      Minimization_function minimization_function) -> Factor_node {
    for (const Graph_edge edge : factor_edges) {
      validate_edge(edge);
    }

    const auto factor = Factor_node{factors_.size()};
    factors_.emplace_back(factor_edges, std::move(minimization_function));
    return factor;
  }
```

All edge handles are validated before constructing the factor. The
`Factor_data` constructor builds the exchange buffer from those edges and takes
ownership of the minimization function via move.

### `value` And `weight`

```cpp
  [[nodiscard]] auto value(Variable_node variable) const -> double {
    validate_variable(variable);
    return variables_[variable.index].value();
  }

  [[nodiscard]] auto weight(Variable_node variable) const -> Message_weight {
    validate_variable(variable);
    return variables_[variable.index].weight();
  }
```

These expose the current consensus value and weight for a variable. They are
the primary way application code reads solver results.

### `is_factor_enabled` And `set_factor_enabled`

```cpp
  [[nodiscard]] auto is_factor_enabled(Factor_node factor) const -> bool {
    validate_factor(factor);
    return factors_[factor.index].is_enabled();
  }

  auto set_factor_enabled(Factor_node factor, bool enabled) -> void {
    validate_factor(factor);

    if (enabled) {
      enable_factor(factor.index);
    } else {
      disable_factor(factor.index);
    }
  }
```

The public setter dispatches to the private `enable_factor` or
`disable_factor` helpers, which handle the edge and variable-cache side effects.

### `iterate`

The main iteration function is the heart of the solver:

```cpp
  auto iterate() -> bool {
    return iterate_with_satisfaction([] {
      return true;
    });
  }

  auto iterate_with_satisfaction(const std::function<bool()>& is_satisfied) -> bool {
    if (converged_) {
      return true;
    }

    for (auto& factor : factors_) {
      factor.minimize(edges_, random_);
    }

    bool beliefs_converged = true;
    for (auto& variable : variables_) {
      const auto enabled_edges = variable.enabled_edges(edges_);
      const Weighted_value result = enforce_variable_equality(enabled_edges);
      const bool has_lone_standard_message = has_lone_standard_message_to_variable(enabled_edges);
      beliefs_converged = beliefs_converged && belief_converged(variable.value(), result.value);
      variable.update_result(result);

      for (const Graph_edge edge : enabled_edges) {
        const bool reset_disagreement =
            has_lone_standard_message &&
            edges_[edge.index].weighted_message_to_variable().weight == Message_weight::standard;
        edges_[edge.index].set_result_from_variable(result, learning_rate_, reset_disagreement);
      }
    }

    ++iterations_;
    converged_ = beliefs_converged;

    for (const auto& callback : iteration_callbacks_) {
      callback();
    }

    if (converged_ && !is_satisfied()) {
      converged_ = false;
    }

    return converged_;
  }
```

If already converged, it short-circuits and returns `true`.

**Factor pass.** Every factor (enabled or not; disabled ones bail out
internally) runs its minimizer. This reads the variable-to-factor messages,
solves the local subproblem, and writes factor-to-variable messages back to the
edges.

**Variable pass.** For each variable:

1. Get the enabled incident edges (rebuilding the cache if dirty).
2. Compute the consensus value by calling `enforce_variable_equality`.
3. Detect the lone-standard-message case (used for `u` resets).
4. Store the consensus result in `Variable_data`.
5. Write the result back through each enabled edge, updating `z`, the leftward
   weight, and `u`.

The lone-standard-message check implements the paper's rule that when only one
standard-weight factor-to-variable message exists, the `u` variable on that
edge should be reset because there is no genuine disagreement.

**Post-pass bookkeeping.** The iteration counter increments. Convergence is
checked by comparing each variable's new belief value with its previous value.
Finally, iteration callbacks fire. If a domain-specific satisfaction predicate
was supplied, it can clear `converged_` after callbacks when the variable
beliefs are stable but the domain constraints are not yet satisfied.

### `iterate_until_converged`

```cpp
  auto iterate_until_converged(std::size_t max_iterations) -> bool {
    for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
      if (iterate()) {
        return true;
      }
    }

    return converged_;
  }
```

A convenience loop. It returns `true` if convergence was reached within the
budget, `false` otherwise.

### `iterate_until_satisfied`

```cpp
  auto iterate_until_satisfied(
      std::size_t max_iterations,
      const std::function<bool()>& is_satisfied) -> bool {
    if (converged_) {
      if (is_satisfied()) {
        return true;
      }
      converged_ = false;
    }

    for (std::size_t iteration = 0; iteration < max_iterations; ++iteration) {
      if (iterate_with_satisfaction(is_satisfied)) {
        return true;
      }
    }

    return converged_;
  }
```

This loop requires both belief convergence and a caller-supplied satisfaction
predicate. If the graph is already marked converged but the predicate is false,
the cached convergence flag is cleared and iteration resumes.

### `reinitialize`

```cpp
  auto reinitialize() -> void {
    for (auto& variable : variables_) {
      variable.reset();
      for (const Graph_edge edge : variable.edges()) {
        edges_[edge.index].reset(variable.initial_value());
      }
    }

    for (auto& factor : factors_) {
      factor.reset();
    }

    iterations_ = 0;
    converged_ = false;

    for (const auto& callback : reinitialize_callbacks_) {
      callback();
    }
  }
```

Reinitialization returns the graph to its starting state without rebuilding the
topology. For each variable, it resets the variable's value and weight, then
resets every incident edge from the variable's original initial value. For each
factor, it reenables the factor. Finally, iteration state is cleared and
reinitialize callbacks fire.

This is useful for applications like the debug GUI where the user wants to
re-solve the same problem from scratch, possibly after changing parameters.

### Callback Registration

```cpp
  auto add_iteration_callback(std::function<void()> callback) -> void {
    iteration_callbacks_.push_back(std::move(callback));
  }

  auto add_reinitialize_callback(std::function<void()> callback) -> void {
    reinitialize_callbacks_.push_back(std::move(callback));
  }
```

Callbacks are stored by value. They fire in registration order.

### Private Section

```cpp
 private:
```

Everything below this point supports the public `Impl` methods.

### `validate_variable`, `validate_edge`, `validate_factor`

```cpp
  auto validate_variable(Variable_node variable) const -> void {
    if (!variable.is_valid() || variable.index >= variables_.size()) {
      throw std::out_of_range("Factor_graph references an invalid variable");
    }
  }

  auto validate_edge(Graph_edge edge) const -> void {
    if (!edge.is_valid() || edge.index >= edges_.size()) {
      throw std::out_of_range("Factor_graph references an invalid edge");
    }
  }

  auto validate_factor(Factor_node factor) const -> void {
    if (!factor.is_valid() || factor.index >= factors_.size()) {
      throw std::out_of_range("Factor_graph references an invalid factor");
    }
  }
```

All three follow the same pattern: check validity, check bounds, throw on
failure. They protect every public method that accepts a user-supplied handle.

### `enable_factor`

```cpp
  auto enable_factor(std::size_t factor_index) -> void {
    if (!factors_[factor_index].enable()) {
      return;
    }

    for (const auto& exchange : factors_[factor_index].exchanges()) {
      const Graph_edge edge = exchange.edge();
      const Variable_node variable = edges_[edge.index].variable();
      edges_[edge.index].reset(Weighted_value{variables_[variable.index].value(), Message_weight::standard});
      variables_[variable.index].reenable_edge(edge);
    }

    converged_ = false;
  }
```

If `Factor_data::enable()` returns `false`, the factor was already enabled and
nothing happens. Otherwise, each incident edge is reset with the variable's
*current* value (not the initial value) and standard weight. This means the
newly reenabled edge starts with a belief consistent with the variable's latest
consensus. The variable's enabled-edge cache is updated via `reenable_edge`.

Setting `converged_` to `false` forces at least one more iteration, since a
freshly reenabled factor may change messages.

### `disable_factor`

```cpp
  auto disable_factor(std::size_t factor_index) -> void {
    if (!factors_[factor_index].disable()) {
      return;
    }

    for (const auto& exchange : factors_[factor_index].exchanges()) {
      const Graph_edge edge = exchange.edge();
      const Variable_node variable = edges_[edge.index].variable();
      edges_[edge.index].disable();
      variables_[variable.index].force_enabled_edges_update();
    }

    converged_ = false;
  }
```

The mirror of `enable_factor`. Each incident edge is disabled and the variable
cache is dirtied. Disabled edges remain in storage but are ignored by the
variable pass and convergence check.

### `enforce_variable_equality`

```cpp
  [[nodiscard]] auto enforce_variable_equality(std::span<const Graph_edge> enabled_edges) const -> Weighted_value {
    double all_sum = 0.0;
    double standard_sum = 0.0;
    std::size_t standard_count = 0;

    for (const Graph_edge edge : enabled_edges) {
      const Weighted_value message = edges_[edge.index].weighted_message_to_variable();
      if (message.weight == Message_weight::infinite) {
        return message;
      }
      if (message.weight == Message_weight::standard) {
        standard_sum += message.value;
        ++standard_count;
      }
      all_sum += message.value;
    }

    if (standard_count > 0) {
      return Weighted_value{
          standard_sum / static_cast<double>(standard_count),
          Message_weight::standard};
    }

    return Weighted_value{
        all_sum / static_cast<double>(enabled_edges.size()),
        Message_weight::zero};
  }
```

This implements the paper's weighted average at equality nodes:

1. If any message is infinite, it wins immediately. The paper says there cannot
   be contradictory infinite messages if the minimizer logic is correct.
2. If there are standard-weight messages, average only those. Zero-weight
   messages do not participate in the consensus.
3. If all messages are zero-weight, average everything and mark the result as
   zero weight — the variable has no confident belief.

### `has_lone_standard_message_to_variable`

```cpp
  [[nodiscard]] auto has_lone_standard_message_to_variable(std::span<const Graph_edge> enabled_edges) const -> bool {
    std::size_t standard_count = 0;
    for (const Graph_edge edge : enabled_edges) {
      const Message_weight weight = edges_[edge.index].weighted_message_to_variable().weight;
      if (weight == Message_weight::infinite) {
        return false;
      }
      if (weight == Message_weight::standard) {
        ++standard_count;
      }
    }

    return standard_count == 1;
  }
```

This detects the special case described in the paper: if exactly one edge
carries a standard-weight message while all others are zero-weight, that lone
edge's `u` should be reset. The reasoning is that with no competing standard
opinions, there is no genuine disagreement to track.

An infinite message returns `false` immediately because in the presence of
certainty the lone-standard rule does not apply.

### `belief_converged`

```cpp
  [[nodiscard]] auto belief_converged(double old_value, double new_value) const -> bool {
    return std::abs(old_value - new_value) <= convergence_delta_;
  }
```

The default convergence check compares variable belief values before and after
the variable pass. It intentionally ignores message differences: message state
can keep cycling even when the beliefs and domain constraints are stable.

### Data Members

```cpp
  double learning_rate_;
  double convergence_delta_;
  Random_engine random_;
  std::size_t iterations_ = 0;
  bool converged_ = false;
  std::vector<detail::Variable_data> variables_;
  std::vector<detail::Edge_data> edges_;
  std::vector<detail::Factor_data> factors_;
  std::vector<std::function<void()>> iteration_callbacks_;
  std::vector<std::function<void()>> reinitialize_callbacks_;
```

The three entity vectors are the graph's core storage. Handles index into these
vectors. The callbacks are optional hooks for applications that need
per-iteration or per-reinitialize side effects (e.g. the debug GUI updates its
display after each iteration).

### `Factor_graph` Forwarding Methods

The remainder of the file defines the public `Factor_graph` class's methods.
Each one delegates directly to `impl_`:

```cpp
Factor_graph::Factor_graph(
    double learning_rate,
    double convergence_delta,
    std::uint64_t random_seed)
    : impl_(std::make_unique<Impl>(learning_rate, convergence_delta, random_seed)) {}

Factor_graph::~Factor_graph() = default;

Factor_graph::Factor_graph(Factor_graph&&) noexcept = default;

auto Factor_graph::operator=(Factor_graph&&) noexcept -> Factor_graph& = default;
```

The constructor creates the `Impl` with `std::make_unique`. The destructor,
move constructor, and move assignment are defaulted — the compiler-generated
versions do the right thing because `unique_ptr` is move-only.

The copy constructor and copy assignment are deleted in the header:

```cpp
Factor_graph(const Factor_graph&) = delete;
auto operator=(const Factor_graph&) -> Factor_graph& = delete;
```

A factor graph is not copyable because its minimization functions may capture
mutable external state.

The remaining forwarding methods are mechanical one-liners:

```cpp
auto Factor_graph::learning_rate() const -> double {
  return impl_->learning_rate();
}
```

Each public method validates handles (inside `Impl`) and delegates. The
`create_factor` overload accepting `std::initializer_list<Graph_edge>` converts
the initializer list to a `std::span` before forwarding:

```cpp
auto Factor_graph::create_factor(
    std::initializer_list<Graph_edge> edges,
    Minimization_function minimization_function) -> Factor_node {
  return impl_->create_factor(
      std::span<const Graph_edge>{edges.begin(), edges.size()},
      std::move(minimization_function));
}
```

This overload exists so users can write `graph.create_factor({e1, e2}, fn)`
with brace syntax.

The message-difference diagnostic forwards directly:

```cpp
auto Factor_graph::max_message_difference() const -> std::optional<double> {
  return impl_->max_message_difference();
}
```

The domain-specific iteration wrapper adapts the public predicate, which sees
`const Factor_graph&`, to the implementation predicate, which needs no
arguments:

```cpp
auto Factor_graph::iterate_until_satisfied(
    std::size_t max_iterations,
    std::function<bool(const Factor_graph&)> is_satisfied) -> bool {
  return impl_->iterate_until_satisfied(
      max_iterations,
      [this, is_satisfied = std::move(is_satisfied)] {
        return is_satisfied(*this);
      });
}
```

### Closing The File

```cpp
} // namespace twalib
```

## Important Invariants

- Handle indexes are stable for the life of the graph. Variables, edges, and
  factors are never removed or reordered.
- `iterate()` is idempotent once converged: it returns `true` without modifying
  state.
- `reinitialize()` restores all numeric state to the post-construction
  condition but does not remove or add entities.
- Enabling or disabling a factor always sets `converged_ = false` if the
  state actually changed.
- `enforce_variable_equality` never divides by zero because `Variable_data`
  throws if the enabled-edge list is empty.
- Infinite incoming weight on an edge is always preserved through the factor
  output, regardless of the minimizer's opinion.

## Relationship To The Paper

The paper describes one iteration as:

1. Update `x` via local minimization (factor pass).
2. Compute weighted average `z` at equality nodes (variable pass).
3. Update `u` with `u = u + alpha*(x - z)`, subject to reset rules.
4. Check convergence.

`iterate()` follows this sequence exactly. The paper's learning rate `alpha /
rho_0` corresponds to `learning_rate_` (since the code normalizes `rho_0 = 1`
for hard-constraint problems). This implementation reports default convergence
when variable belief values stop changing within `convergence_delta_`.
`max_message_difference()` still exposes edge message changes for diagnostics,
which is useful when the dual variables continue a stable cycle after the
beliefs have settled.

The paper's weight logic for equality nodes:

- Infinite incoming → infinite outgoing.
- At least one standard → average standards, standard outgoing.
- All zero → average all, zero outgoing.

maps directly to the three return paths in `enforce_variable_equality`.

## Extension Notes

If new iteration strategies are needed (e.g. damping, asynchronous schedules,
or variable-ordering heuristics), they would be added to or wrap `iterate()`.
The current design intentionally keeps iteration simple and serial.

If the graph needs to support entity removal, handle stability would need to
change — currently indexes are positions in dense vectors.

Callbacks fire synchronously inside `iterate()` and `reinitialize()`. Long
callbacks will block the solver. If asynchronous notification is needed, the
callback should enqueue work rather than performing it inline.
