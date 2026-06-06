# Factor Graphs

This note explains what a factor graph is and how this repository uses one to
represent optimization problems.

## What Is A Factor Graph?

A factor graph is a bipartite graph with two kinds of nodes:

- **Variable nodes**, representing unknown quantities to be solved for.
- **Factor nodes**, representing constraints or local objective terms that
  involve one or more variables.

Edges connect factors to variables. An edge always links exactly one factor to
exactly one variable. A factor with three incident edges touches three
variables; those three edges carry messages back and forth between that factor
and its three variables.

The bipartite structure matters: there are never direct edges between two
variables or between two factors. All communication happens through the
factor–variable edges.

## Why Factor Graphs?

Many optimization and constraint-satisfaction problems decompose naturally into
local pieces. Sudoku, for example, has row constraints, column constraints, and
box constraints, each involving a subset of the cells. Circle packing has
pairwise non-overlap constraints between nearby circles plus boundary
constraints for each circle.

A factor graph makes this decomposition explicit:

- Each local constraint or cost becomes a factor node.
- Each unknown becomes a variable node.
- The edges show which unknowns participate in which constraints.

The Three-Weight Algorithm (TWA) exploits this structure by running
message-passing on the factor graph until the variables reach consensus.

## Factor Graphs In This Repository

The public class is `Factor_graph`, declared in
`include/twalib/graph/factor_graph.hpp`. Users build a graph by creating
variables, edges, and factors, then iterate until convergence.

### Handles

The three node and edge types exposed to users are lightweight handles:

- `Variable_node` — identifies a variable by index.
- `Graph_edge` — identifies an edge by index.
- `Factor_node` — identifies a factor by index.

Each handle is a tiny struct holding a `std::size_t` index and an `is_valid()`
check. They are cheap to copy and compare.

### Building A Graph

A typical construction sequence is:

```cpp
Factor_graph graph;

// Create variables.
Variable_node a = graph.create_variable(0.0);
Variable_node b = graph.create_variable(0.0);

// Create edges for a factor that touches both variables.
Graph_edge ea = graph.create_edge(a);
Graph_edge eb = graph.create_edge(b);

// Create the factor with a minimization function.
graph.create_factor({ea, eb}, my_minimizer);
```

Each `create_edge` call links the new edge to one variable. Each
`create_factor` call groups edges and attaches a minimization function. The
minimizer does not need to know about graph internals; it only reads and writes
`Weighted_value_exchange` objects.

### Running The Solver

After construction, the user iterates:

```cpp
graph.iterate_until_converged(1000);
```

or, for more control:

```cpp
while (!graph.iterate()) {
  // inspect intermediate state, update dynamic factors, etc.
}
```

Each call to `iterate()` runs one factor pass (all factors minimize) and one
variable pass (all variables enforce equality). It returns `true` when
converged. Default convergence is belief-based: every variable's new value must
be within `convergence_delta` of its previous value.

For domains with an additional satisfaction criterion, use
`iterate_until_satisfied()`. Circle packing uses this to require both stable
beliefs and a sufficiently small `max_overlap`.

### Reading Results

After convergence:

```cpp
double result_a = graph.value(a);
double result_b = graph.value(b);
```

The weight can also be queried:

```cpp
Message_weight w = graph.weight(a);
```

If `w` is `Message_weight::infinite`, the solver is certain of the value.

## Internal Storage

Internally, `Factor_graph` uses the PImpl idiom: the public class holds a
`std::unique_ptr<Impl>`, and `Impl` owns the actual data. The internal storage
types are:

- `Variable_data` — per-variable state (current value, weight, incident
  edges). See [`notes/src/graph/variable_data.hpp.md`](../src/graph/variable_data.hpp.md).
- `Edge_data` — per-edge message state (`x`, `z`, `u`, weights, and
  message-difference diagnostics). See [`notes/src/graph/edge_data.hpp.md`](../src/graph/edge_data.hpp.md).
- `Factor_data` — per-factor state (minimization function, exchange buffer,
  enabled flag). See [`notes/src/graph/factor_data.hpp.md`](../src/graph/factor_data.hpp.md).

These are stored in three parallel vectors inside `Impl`. The handle indexes
(e.g. `Variable_node::index`) are positions into these vectors.

## Dynamic Factors

Factors can be enabled and disabled at runtime:

```cpp
graph.set_factor_enabled(some_factor, false);  // disable
graph.set_factor_enabled(some_factor, true);   // reenable
```

Disabling a factor disables all its incident edges. The variable-side
enabled-edge cache is dirtied so it will be rebuilt on the next access.
Reenabling resets the edges with the variable's current value and standard
weight.

This mechanism supports problems like fast circle packing, where most pairwise
non-overlap factors are irrelevant at any given time because their circles are
far apart. Disabling those factors avoids unnecessary minimization work without
changing the algorithm's correctness for active constraints.

## Relationship To The Paper

The paper describes the algorithm in terms of:

- "function cost nodes" → factor nodes in this code,
- "equality nodes" → variable nodes in this code,
- edges connecting them, carrying messages `m` (rightward) and `n` (leftward).

The paper's iteration description maps directly to `Factor_graph::Impl::iterate`:

1. Compute `x` values (factor pass / minimize).
2. Compute `z` values (variable pass / enforce equality).
3. Update `u` values (part of the variable pass, inside
   `Edge_data::set_result_from_variable`).
4. Check convergence using variable belief values.

## Further Reading

- [`notes/concepts/message_passing.md`](message_passing.md) — the iteration mechanics in detail.
- [`notes/concepts/weights.md`](weights.md) — how the three weights control message strength.
- [`notes/src/graph/factor_graph.cpp.md`](../src/graph/factor_graph.cpp.md) — line-by-line walkthrough of the
  implementation.
