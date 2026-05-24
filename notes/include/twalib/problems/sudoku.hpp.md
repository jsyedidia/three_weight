# include/twalib/problems/sudoku.hpp

## Role In The System

This header declares the public direct Sudoku problem builder.

`Sudoku` is a namespace-like class with static functions. It does not own a
puzzle or a graph. Instead, callers pass in a `Factor_graph`, an `inner_side`,
and a map of givens, and the builder adds the corresponding variables and
factors to the graph.

The implementation lives in `src/problems/sudoku.cpp`.

## Main Types And Functions

The class defines:

```cpp
using Givens = std::unordered_map<std::size_t, std::size_t>;
using Variables = std::vector<std::vector<Variable_node>>;
```

`Givens` maps flat cell indexes to zero-based values. `Variables` stores the
candidate variable handles returned by the builder.

The public functions are:

```cpp
Sudoku::add_to_factor_graph
Sudoku::extract_state
```

The first builds the graph. The second reads a flat solved-state vector back
from the graph.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_PROBLEMS_SUDOKU_HPP
#define TWALIB_PROBLEMS_SUDOKU_HPP
```

It prevents duplicate processing of this public problem-builder header.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/variable_node.hpp"
```

`Factor_graph` is the graph being modified and queried. `Variable_node` is the
handle type stored in the returned candidate grid.

### Standard Includes

The standard includes are:

```cpp
#include <cstddef>
#include <unordered_map>
#include <vector>
```

`<cstddef>` provides `std::size_t`. `<unordered_map>` stores givens by flat
cell index. `<vector>` stores the candidate handle grid and extracted state.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

The builder is part of the public library API.

### `Sudoku`

The class begins:

```cpp
class Sudoku {
 public:
```

All members are public static API. The class is used as a grouping type rather
than as something callers instantiate.

### `Givens`

The givens alias is:

```cpp
  using Givens = std::unordered_map<std::size_t, std::size_t>;
```

The key is the flat row-major cell index. The value is the zero-based Sudoku
value for that cell.

### `Variables`

The variable-grid alias is:

```cpp
  using Variables = std::vector<std::vector<Variable_node>>;
```

The outer vector has one entry per cell. The inner vector has one
`Variable_node` per possible value. In the direct builder, every cell has
`outer_side` candidates.

### `add_to_factor_graph`

The builder declaration is:

```cpp
  [[nodiscard]] static auto add_to_factor_graph(
      Factor_graph& graph,
      std::size_t inner_side,
      const Givens& givens) -> Variables;
```

`[[nodiscard]]` asks callers to keep the returned `Variables` object, because
it is needed to extract the solved state.

`static` means callers invoke this as:

```cpp
Sudoku::add_to_factor_graph(graph, inner_side, givens)
```

without constructing a `Sudoku` object.

`graph` is the factor graph to mutate. `inner_side` is the box side length.
`givens` contains the puzzle clues.

The function returns the candidate handle grid.

### `extract_state`

The extraction declaration is:

```cpp
  [[nodiscard]] static auto extract_state(
      const Factor_graph& graph,
      const Variables& variables) -> std::vector<int>;
```

`graph` is read-only because extraction should not modify the solver state.
`variables` is the handle grid returned by `add_to_factor_graph`.

The returned vector has one integer per cell, in row-major order. Values are
zero-based. A cell is `-1` if no candidate has crossed the selection
threshold.

### Closing The File

The class closes with:

```cpp
};
```

The namespace and include guard close with:

```cpp
} // namespace twalib

#endif
```

## Important Invariants

- `inner_side` is the box side length; the grid side is `inner_side *
  inner_side`.
- Givens use flat cell indexes and zero-based values.
- `Variables` handles belong to the graph passed to `add_to_factor_graph`.
- `extract_state` expects the matching `Variables` object for the graph it is
  reading.

## Relationship To The Paper

This public builder exposes the Sudoku factor-graph construction described in
the paper: candidate variables plus one-hot constraints for cells, rows,
columns, and boxes.

## Extension Notes

Keep this direct builder simple. It is useful as a clear reference
construction. Optimizations that remove candidates or reduce graph size belong
in `Compact_sudoku`.
