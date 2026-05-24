# include/twalib/problems/compact_sudoku.hpp

## Role In The System

This header declares the public compact Sudoku problem builder.

`Compact_sudoku` builds the same kind of Sudoku factor graph as `Sudoku`, but
it uses the givens to avoid creating variables for candidates that are already
impossible. The resulting graph is usually much smaller.

The implementation lives in `src/problems/compact_sudoku.cpp`.

## Main Types And Functions

This header defines:

```cpp
struct Compact_sudoku_variables
class Compact_sudoku
```

`Compact_sudoku_variables` is the returned handle bundle. It stores puzzle
metadata, the givens, and the surviving candidate variables.

`Compact_sudoku` is a namespace-like class with static functions for building
and extracting Sudoku state.

## State At A Glance

`Compact_sudoku_variables` stores:

```cpp
  std::size_t inner_side = 0;
  std::size_t outer_side = 0;
  Sudoku::Givens givens;
  std::vector<std::vector<Variable_node>> candidates;
```

The side lengths describe the puzzle shape. `givens` is copied into the result
so `extract_state` can report clue cells even though the compact builder does
not create candidate variables for them. `candidates[cell][value]` is valid
only when that candidate survived pruning.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_PROBLEMS_COMPACT_SUDOKU_HPP
#define TWALIB_PROBLEMS_COMPACT_SUDOKU_HPP
```

It prevents duplicate processing of this public problem-builder header.

### Graph And Problem Includes

The library includes are:

```cpp
#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/variable_node.hpp"
#include "twalib/problems/sudoku.hpp"
```

`Factor_graph` is the graph being modified and queried. `Variable_node` is the
handle type stored for surviving candidates. `sudoku.hpp` provides
`Sudoku::Givens`, which the compact builder reuses so both Sudoku builders
accept the same clue format.

### Standard Includes

The standard includes are:

```cpp
#include <cstddef>
#include <vector>
```

`<cstddef>` provides `std::size_t`. `<vector>` stores the compact candidate
grid and extracted state.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

The compact builder is part of the public library API.

### `Compact_sudoku_variables`

The returned variable bundle begins:

```cpp
struct Compact_sudoku_variables {
```

It is a `struct` because its fields are plain data needed by callers and by
`Compact_sudoku::extract_state`.

The side lengths are:

```cpp
  std::size_t inner_side = 0;
  std::size_t outer_side = 0;
```

Default values make a default-constructed bundle clearly empty.

The givens copy is:

```cpp
  Sudoku::Givens givens;
```

The compact builder skips variables for given cells, so extraction needs this
map to report those cells.

The candidate grid is:

```cpp
  std::vector<std::vector<Variable_node>> candidates;
```

The outer vector has one entry per cell. The inner vector has one slot per
possible value. Invalid `Variable_node` handles mark candidates that were
eliminated or cells that were givens.

The struct closes with:

```cpp
};
```

### `Compact_sudoku`

The class begins:

```cpp
class Compact_sudoku {
 public:
```

Like `Sudoku`, this is a grouping type for static functions rather than a
stateful object that callers instantiate.

### Aliases

The public aliases are:

```cpp
  using Givens = Sudoku::Givens;
  using Variables = Compact_sudoku_variables;
```

`Givens` keeps the clue format identical to the direct builder. `Variables`
names the returned compact handle bundle.

### `add_to_factor_graph`

The compact builder declaration is:

```cpp
  [[nodiscard]] static auto add_to_factor_graph(
      Factor_graph& graph,
      std::size_t inner_side,
      const Givens& givens) -> Variables;
```

`[[nodiscard]]` asks callers to keep the returned `Variables` object, because
it is needed to extract the solution.

`static` means callers invoke the builder without constructing a
`Compact_sudoku` object.

The function mutates `graph`, uses `inner_side` to determine the grid shape,
and uses `givens` both to encode clues and to prune impossible candidates.

### `extract_state`

The extraction declaration is:

```cpp
  [[nodiscard]] static auto extract_state(
      const Factor_graph& graph,
      const Variables& variables) -> std::vector<int>;
```

`graph` is read-only during extraction. `variables` is the compact handle
bundle returned by `add_to_factor_graph`.

The returned vector has one integer per cell, in row-major order. Givens are
reported directly from `variables.givens`. Non-given cells are read from
surviving candidate variables. Values are zero-based, and `-1` means no
candidate has crossed the selection threshold.

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

- `Compact_sudoku_variables` must be used with the graph that created it.
- `candidates[cell][value]` is valid only for surviving non-given candidates.
- Given cells are represented in `givens`, not by candidate variables.
- The compact builder accepts the same clue format as the direct `Sudoku`
  builder.

## Relationship To The Paper

The paper describes the Sudoku factor graph in terms of candidate variables
and one-hot constraints. `Compact_sudoku` preserves that model but performs
simple clue-based pruning before constructing the graph.

## Extension Notes

Keep the returned metadata sufficient for extraction and inspection. Any new
pruning rule that removes a candidate should leave an invalid handle in the
corresponding `candidates[cell][value]` slot so downstream code can distinguish
removed candidates from live variables.
