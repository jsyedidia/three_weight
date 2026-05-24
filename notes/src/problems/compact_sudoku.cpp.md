# src/problems/compact_sudoku.cpp

## Role In The System

`compact_sudoku.cpp` implements the `Compact_sudoku` problem builder. It
constructs a factor graph representing a Sudoku puzzle, but unlike the simpler
`Sudoku` builder, it exploits the given (pre-filled) values to eliminate
variables that cannot participate in any valid solution. This results in a
smaller, faster-converging graph.

The file provides:

- `Compact_sudoku::add_to_factor_graph`, which creates the variables and
  one-hot factors for a puzzle with given clues,
- `Compact_sudoku::extract_state`, which reads the solved values back out,
- several anonymous-namespace helpers for coordinate arithmetic and
  constraint pruning.

The public header is `include/twalib/problems/compact_sudoku.hpp`. It declares
the `Compact_sudoku` class and the `Compact_sudoku_variables` struct that holds
the graph's variable handles.

## How Compact Sudoku Differs From Sudoku

The plain `Sudoku` builder creates one variable for every (cell, value) pair
in the grid, regardless of givens. `Compact_sudoku` skips variables that are
ruled out by a given in the same row, column, or box. It also skips entire
cells that are already filled. This can dramatically reduce graph size for
well-clued puzzles.

## Code Walkthrough

### Includes

```cpp
#include "twalib/problems/compact_sudoku.hpp"
```

The own public header comes first, following project convention.

```cpp
#include "twalib/graph/weighted_value.hpp"
#include "twalib/minimizers/one_hot.hpp"
```

`weighted_value.hpp` provides `Message_weight` for variable creation.
`one_hot.hpp` provides `create_one_hot_factor` for building the constraint
factors.

```cpp
#include <stdexcept>
#include <vector>
```

`<stdexcept>` supplies `std::invalid_argument` and `std::out_of_range` for
validation. `<vector>` stores candidate lists.

### Namespace

```cpp
namespace twalib {
namespace {
```

The outer namespace is `twalib`. The inner anonymous namespace hides the
helper functions from other translation units.

### `outer_side_for`

```cpp
auto outer_side_for(std::size_t inner_side) -> std::size_t {
  if (inner_side == 0) {
    throw std::invalid_argument("Compact_sudoku inner_side must be positive");
  }

  return inner_side * inner_side;
}
```

Converts the box dimension (`inner_side`) into the grid dimension
(`outer_side`). A standard 9×9 Sudoku has `inner_side = 3` and
`outer_side = 9`. The guard rejects zero.

### `cell_index`

```cpp
auto cell_index(std::size_t row, std::size_t column, std::size_t outer_side) -> std::size_t {
  return row * outer_side + column;
}
```

Flattens a (row, column) pair into a linear cell index. The grid is stored in
row-major order.

### `square_index`

```cpp
auto square_index(
    std::size_t row,
    std::size_t column,
    std::size_t inner_side) -> std::size_t {
  return column / inner_side + inner_side * (row / inner_side);
}
```

Returns which box (square) a cell belongs to. Boxes are numbered left-to-right,
top-to-bottom. `row / inner_side` gives the box row; multiplying by
`inner_side` converts that to a box index offset. `column / inner_side` gives
the box column within that row of boxes.

### `square_base_row`

```cpp
auto square_base_row(std::size_t square, std::size_t inner_side) -> std::size_t {
  return (square / inner_side) * inner_side;
}
```

Given a box index, returns the top-left corner's row in the full grid.
`square / inner_side` gives which row of boxes this is; multiplying by
`inner_side` converts to a grid row.

### `square_base_column`

```cpp
auto square_base_column(std::size_t square, std::size_t inner_side) -> std::size_t {
  return (square % inner_side) * inner_side;
}
```

Given a box index, returns the top-left corner's column in the full grid.
`square % inner_side` gives which column of boxes this is.

### `validate_givens`

```cpp
auto validate_givens(const Compact_sudoku::Givens& givens, std::size_t outer_side) -> void {
  const std::size_t num_cells = outer_side * outer_side;
  for (const auto& [cell, value] : givens) {
    if (cell >= num_cells) {
      throw std::out_of_range("Compact_sudoku given cell is out of range");
    }
    if (value >= outer_side) {
      throw std::out_of_range("Compact_sudoku given value is out of range");
    }
  }
}
```

Checks that every given references a valid cell and a valid digit. The
structured binding `[cell, value]` destructures each map entry. Cell indices
must be less than `outer_side * outer_side` and digit values must be less than
`outer_side`.

### `has_given_value`

```cpp
auto has_given_value(
    const Compact_sudoku::Givens& givens,
    std::size_t cell,
    std::size_t value) -> bool {
  const auto given = givens.find(cell);
  return given != givens.end() && given->second == value;
}
```

Returns whether a specific cell has been given a specific value. It looks up
the cell in the givens map; if the cell is present and its value matches, the
predicate is true. Used by `value_found` to check peers.

### `value_found`

```cpp
auto value_found(
    const Compact_sudoku::Givens& givens,
    std::size_t inner_side,
    std::size_t outer_side,
    std::size_t row,
    std::size_t column,
    std::size_t value) -> bool {
```

This is the core elimination predicate. It returns `true` if the given `value`
already appears as a given in the same row, column, or box as the cell at
(row, column). If it returns `true`, the (cell, value) pair does not need a
variable in the graph because no valid solution can assign that value to that
cell.

```cpp
  for (std::size_t candidate_column = 0; candidate_column < outer_side; ++candidate_column) {
    if (has_given_value(givens, cell_index(row, candidate_column, outer_side), value)) {
      return true;
    }
  }
```

The row check iterates over every column in the same row. If any cell in that
row has been given the target value, the candidate is eliminated.

```cpp
  for (std::size_t candidate_row = 0; candidate_row < outer_side; ++candidate_row) {
    if (has_given_value(givens, cell_index(candidate_row, column, outer_side), value)) {
      return true;
    }
  }
```

The column check iterates over every row in the same column.

```cpp
  const std::size_t square = square_index(row, column, inner_side);
  const std::size_t base_row = square_base_row(square, inner_side);
  const std::size_t base_column = square_base_column(square, inner_side);
  for (std::size_t row_offset = 0; row_offset < inner_side; ++row_offset) {
    for (std::size_t column_offset = 0; column_offset < inner_side; ++column_offset) {
      if (has_given_value(
              givens,
              cell_index(base_row + row_offset, base_column + column_offset, outer_side),
              value)) {
        return true;
      }
    }
  }
```

The box check first determines which box the cell belongs to, then finds that
box's top-left corner. It iterates over the `inner_side × inner_side` cells
within that box, checking each for the target value.

```cpp
  return false;
}
```

If none of the three checks found the value, it is not eliminated.

### `valid_candidate`

```cpp
auto valid_candidate(Variable_node variable) -> bool {
  return variable.is_valid();
}
```

A thin wrapper that tests whether a `Variable_node` handle refers to an actual
variable. Invalid handles (default-constructed) mark eliminated candidates in
the `candidates` grid.

### `add_one_hot_if_nonempty`

```cpp
auto add_one_hot_if_nonempty(
    Factor_graph& graph,
    const std::vector<Variable_node>& candidates) -> void {
  if (!candidates.empty()) {
    [[maybe_unused]] const auto factor = create_one_hot_factor(graph, candidates);
  }
}
```

Creates a one-hot factor only if the candidate list is non-empty. After
elimination, some row/column/box constraints may have no surviving candidates
(because every cell in that unit already has the value given). The
`[[maybe_unused]]` suppresses the warning from discarding the returned
`Factor_node` handle — the factor is owned by the graph and does not need
external tracking.

### Closing The Anonymous Namespace

```cpp
} // namespace
```

### `Compact_sudoku::add_to_factor_graph`

```cpp
auto Compact_sudoku::add_to_factor_graph(
    Factor_graph& graph,
    std::size_t inner_side,
    const Givens& givens) -> Variables {
```

This is the public entry point. It builds the full compact Sudoku graph and
returns a `Variables` struct that the caller can use to read back the solution.

#### Setup

```cpp
  const std::size_t outer_side = outer_side_for(inner_side);
  validate_givens(givens, outer_side);
```

Computes the grid dimension and validates the givens map.

```cpp
  Variables variables;
  variables.inner_side = inner_side;
  variables.outer_side = outer_side;
  variables.givens = givens;
  variables.candidates.resize(
      outer_side * outer_side,
      std::vector<Variable_node>(outer_side, Variable_node{}));
```

Initializes the output struct. The `candidates` grid is a 2D structure: one row
per cell, one column per possible digit value. Every entry starts as an invalid
`Variable_node` handle. Valid handles will be written only for candidates that
survive elimination.

#### Cell Loop — Creating Variables And Cell Factors

```cpp
  for (std::size_t row = 0; row < outer_side; ++row) {
    for (std::size_t column = 0; column < outer_side; ++column) {
      const std::size_t cell = cell_index(row, column, outer_side);
      if (givens.contains(cell)) {
        continue;
      }
```

Iterates over every cell in the grid. Given cells are skipped entirely — they
contribute no variables or factors.

```cpp
      std::vector<Variable_node> cell_candidates;
      cell_candidates.reserve(outer_side);
      for (std::size_t value = 0; value < outer_side; ++value) {
        if (value_found(givens, inner_side, outer_side, row, column, value)) {
          continue;
        }

        const Variable_node variable = graph.create_variable(0.0, Message_weight::standard);
        variables.candidates[cell][value] = variable;
        cell_candidates.push_back(variable);
      }
```

For each non-given cell, the inner loop creates a variable for each digit value
that is not eliminated by `value_found`. Each surviving candidate gets a fresh
variable with initial value 0 and standard weight. The handle is recorded in
the `candidates` grid for later readback, and added to the local
`cell_candidates` list. The `reserve` avoids reallocations during the loop.

```cpp
      if (cell_candidates.empty()) {
        throw std::invalid_argument("Compact_sudoku cell has no surviving candidates");
      }
      [[maybe_unused]] const auto cell_factor = create_one_hot_factor(graph, cell_candidates);
    }
  }
```

After the value loop, a cell one-hot factor enforces that exactly one candidate
in this cell is selected. The exception guards against contradictory givens that
eliminate all digits from a cell.

#### Unit Loop — Creating Row, Column, And Box Factors

```cpp
  for (std::size_t index = 0; index < outer_side; ++index) {
    for (std::size_t value = 0; value < outer_side; ++value) {
      std::vector<Variable_node> row_value_variables;
      std::vector<Variable_node> column_value_variables;
      std::vector<Variable_node> square_value_variables;
      row_value_variables.reserve(outer_side);
      column_value_variables.reserve(outer_side);
      square_value_variables.reserve(outer_side);
```

For each (unit index, value) pair, three candidate lists are allocated to
collect the surviving candidates for that value in a row, column, and box
respectively. The reserves avoid reallocations during collection.

```cpp
      for (std::size_t offset = 0; offset < outer_side; ++offset) {
        const Variable_node row_variable =
            variables.candidates[cell_index(index, offset, outer_side)][value];
        if (valid_candidate(row_variable)) {
          row_value_variables.push_back(row_variable);
        }
```

For the row: `index` is the row number and `offset` iterates over columns.
Each valid candidate handle for this value in row `index` is added to the list.

```cpp
        const Variable_node column_variable =
            variables.candidates[cell_index(offset, index, outer_side)][value];
        if (valid_candidate(column_variable)) {
          column_value_variables.push_back(column_variable);
        }
```

For the column: `index` is the column number and `offset` iterates over rows.

```cpp
        const std::size_t square_row = index / inner_side;
        const std::size_t square_column = index % inner_side;
        const std::size_t row_in_square = offset / inner_side;
        const std::size_t column_in_square = offset % inner_side;
        const std::size_t row = square_row * inner_side + row_in_square;
        const std::size_t column = square_column * inner_side + column_in_square;
        const Variable_node square_variable =
            variables.candidates[cell_index(row, column, outer_side)][value];
        if (valid_candidate(square_variable)) {
          square_value_variables.push_back(square_variable);
        }
      }
```

For the box: `index` identifies which box (0 through `outer_side - 1`), and
`offset` iterates over the cells within that box. `square_row` and
`square_column` locate the box in the grid of boxes. `row_in_square` and
`column_in_square` locate the cell within the box. Combining them gives the
cell's absolute row and column in the full grid.

```cpp
      add_one_hot_if_nonempty(graph, row_value_variables);
      add_one_hot_if_nonempty(graph, column_value_variables);
      add_one_hot_if_nonempty(graph, square_value_variables);
    }
  }
```

After collecting candidates for all three units, one-hot factors are created for
each non-empty list. These factors enforce that each value appears exactly once
in each row, column, and box — the standard Sudoku constraints.

#### Return

```cpp
  return variables;
}
```

The `Variables` struct is returned so the caller can later pass it to
`extract_state`.

### `Compact_sudoku::extract_state`

```cpp
auto Compact_sudoku::extract_state(
    const Factor_graph& graph,
    const Variables& variables) -> std::vector<int> {
  std::vector<int> state;
  state.reserve(variables.candidates.size());
```

Allocates the output vector with one entry per cell. The reserve matches the
total number of cells in the grid.

```cpp
  for (std::size_t cell = 0; cell < variables.candidates.size(); ++cell) {
    const auto given = variables.givens.find(cell);
    if (given != variables.givens.end()) {
      state.push_back(static_cast<int>(given->second));
      continue;
    }
```

For given cells, the value comes directly from the `givens` map. The
`static_cast<int>` converts the `std::size_t` digit to `int` for the output
vector.

```cpp
    int selected_value = -1;
    for (std::size_t value = 0; value < variables.candidates[cell].size(); ++value) {
      const Variable_node variable = variables.candidates[cell][value];
      if (valid_candidate(variable) && graph.value(variable) > 0.99) {
        selected_value = static_cast<int>(value);
      }
    }
    state.push_back(selected_value);
  }
```

For non-given cells, the function iterates over all candidate slots. Only valid
handles (non-eliminated candidates) are checked. If the graph's consensus value
for that variable exceeds 0.99, the variable has effectively converged to 1 and
is the selected digit. If no candidate has converged, `selected_value` remains
-1, indicating an unsolved cell.

```cpp
  return state;
}
```

Returns the flat vector of solved digits in row-major cell order.

### Closing The Namespace

```cpp
} // namespace twalib
```

## Important Invariants

- `candidates[cell][value]` is a valid `Variable_node` if and only if that
  (cell, value) pair survived elimination. All other entries are
  default-constructed invalid handles.
- Every non-given cell must have at least one surviving candidate. The
  constructor throws if elimination removes all candidates from a cell.
- The one-hot factors assume their variable lists are non-empty. The
  `add_one_hot_if_nonempty` helper protects against creating empty factors.
- The `Variables` struct must be kept paired with the `Factor_graph` that
  created it. The handles are indices into that specific graph's storage.

## Relationship To The Paper

Section 4 of the paper describes the Sudoku formulation. Each cell, row-value,
column-value, and box-value constraint is a one-hot (simplex projection)
factor. `Compact_sudoku` implements the same constraint structure but with
fewer variables: given values are propagated at build time rather than during
iteration. This precomputation is an optimization that does not change the
algorithm's semantics — it simply removes variables that would immediately
converge to certainty.

## Extension Notes

The compact builder's main invariant is that invalid `Variable_node` handles
mean "this candidate is not represented in the graph." Code that inspects
`variables.candidates` must keep checking `is_valid()` before using a handle.

If future pruning rules are added, they should preserve that representation:
the candidate grid should remain rectangular, with invalid handles marking
removed candidates. That keeps GUI inspection, tests, and `extract_state`
simple.
