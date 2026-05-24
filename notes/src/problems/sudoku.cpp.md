# src/problems/sudoku.cpp

## Role In The System

`sudoku.cpp` implements the direct `Sudoku` problem builder. It converts a
Sudoku puzzle into a `Factor_graph` by creating one variable for every
`(cell, value)` candidate and then adding one-hot factors for the Sudoku
rules.

This builder is direct rather than compact: it creates every candidate
variable, even when a given clue already rules that candidate out. The compact
builder in `src/problems/compact_sudoku.cpp` performs that pruning at graph
construction time.

The file provides:

- `Sudoku::add_to_factor_graph`, which adds variables and factors to a graph,
- `Sudoku::extract_state`, which reads a flat solved-state vector back out,
- small anonymous-namespace helpers for size, indexing, and input validation.

## Code Walkthrough

### Own Header Include

The file starts with:

```cpp
#include "twalib/problems/sudoku.hpp"
```

Including the matching public header first helps catch mismatches between the
declarations and definitions.

### Library Includes

The library includes are:

```cpp
#include "twalib/graph/weighted_value.hpp"
#include "twalib/minimizers/one_hot.hpp"
```

`weighted_value.hpp` provides `Message_weight`, used when creating variables
for given cells. `one_hot.hpp` provides the one-hot constraints that encode the
Sudoku rules.

### Standard Includes

The standard includes are:

```cpp
#include <stdexcept>
#include <vector>
```

`<stdexcept>` provides validation exceptions. `<vector>` stores candidate
handle lists and the extracted state.

### Namespace

The file opens:

```cpp
namespace twalib {
namespace {
```

The outer namespace matches the public API. The anonymous namespace hides
helper functions from other translation units.

### `outer_side_for`

The grid-size helper is:

```cpp
auto outer_side_for(std::size_t inner_side) -> std::size_t {
  if (inner_side == 0) {
    throw std::invalid_argument("Sudoku inner_side must be positive");
  }

  return inner_side * inner_side;
}
```

`inner_side` is the side length of one box. `outer_side` is the side length of
the full grid. For example, `inner_side = 3` gives `outer_side = 9`.

The guard rejects zero because a Sudoku with zero-sized boxes is not valid.

### `cell_index`

The indexing helper is:

```cpp
auto cell_index(std::size_t row, std::size_t column, std::size_t outer_side) -> std::size_t {
  return row * outer_side + column;
}
```

It converts a `(row, column)` pair into a flat row-major cell index.

### `validate_givens`

The givens validation helper is:

```cpp
auto validate_givens(const Sudoku::Givens& givens, std::size_t outer_side) -> void {
  const std::size_t num_cells = outer_side * outer_side;
  for (const auto& [cell, value] : givens) {
    if (cell >= num_cells) {
      throw std::out_of_range("Sudoku given cell is out of range");
    }
    if (value >= outer_side) {
      throw std::out_of_range("Sudoku given value is out of range");
    }
  }
}
```

Each given maps a flat cell index to a zero-based value. The cell must be less
than the total number of cells, and the value must be less than the grid side
length.

The loop uses structured binding:

```cpp
const auto& [cell, value]
```

to name the key and value from each map entry.

### Closing The Anonymous Namespace

The helper namespace closes with:

```cpp
} // namespace
```

### `Sudoku::add_to_factor_graph`

The main builder begins:

```cpp
auto Sudoku::add_to_factor_graph(
    Factor_graph& graph,
    std::size_t inner_side,
    const Givens& givens) -> Variables {
```

`graph` is the graph being modified. `inner_side` describes the Sudoku box
size. `givens` contains the puzzle clues.

The function returns `Variables`, the grid of candidate handles needed later
for extracting a solved state.

### Setup

The first lines compute the grid size and validate the clues:

```cpp
  const std::size_t outer_side = outer_side_for(inner_side);
  validate_givens(givens, outer_side);
```

Then the candidate grid is created:

```cpp
  Variables variables;
  variables.reserve(outer_side * outer_side);
```

`Variables` is a vector with one entry per cell. Each cell entry is another
vector containing one `Variable_node` per possible value.

### Creating Cell Candidates And Cell Factors

The first main loop iterates over cells:

```cpp
  for (std::size_t cell = 0; cell < outer_side * outer_side; ++cell) {
    const auto given = givens.find(cell);
    const bool has_given = given != givens.end();
    const std::size_t given_value = has_given ? given->second : outer_side;
    const Message_weight initial_weight = has_given ? Message_weight::infinite : Message_weight::standard;
```

For each cell, the code checks whether the cell has a given. If it does,
`given_value` stores the zero-based value from the puzzle and
`initial_weight` becomes infinite. If not, `given_value` is set to
`outer_side`, which is outside the valid value range and therefore will not
match any candidate.

The candidate variables for the cell are created next:

```cpp
    std::vector<Variable_node> options;
    options.reserve(outer_side);
    for (std::size_t value = 0; value < outer_side; ++value) {
      options.push_back(graph.create_variable(value == given_value ? 1.0 : 0.0, initial_weight));
    }
```

Every possible value gets one variable. If this is a given cell, the given
candidate starts at `1.0` and the other candidates start at `0.0`; all of them
start with infinite weight. If this is not a given cell, all candidates start
at `0.0` with standard weight.

After the candidate list is complete, the cell one-hot factor is added:

```cpp
    variables.push_back(options);
    [[maybe_unused]] const auto cell_factor = create_one_hot_factor(graph, variables.back());
  }
```

The candidate list is stored in `variables`, and a one-hot factor enforces
that this cell selects exactly one value. For given cells, the clue has already
been encoded by the infinite-weight initial values on the cell's candidates,
so no extra known-value factor is needed.

### Creating Row, Column, And Box Factors

The second main loop creates the unit constraints:

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

For each unit index and each value, the code collects three lists: candidates
for that value in one row, one column, and one box.

The inner loop fills those lists:

```cpp
      for (std::size_t offset = 0; offset < outer_side; ++offset) {
        row_value_variables.push_back(variables[cell_index(index, offset, outer_side)][value]);
        column_value_variables.push_back(variables[cell_index(offset, index, outer_side)][value]);
```

For the row list, `index` is the row and `offset` is the column. For the
column list, `index` is the column and `offset` is the row.

The box coordinates are computed next:

```cpp
        const std::size_t square_row = index / inner_side;
        const std::size_t square_column = index % inner_side;
        const std::size_t row_in_square = offset / inner_side;
        const std::size_t column_in_square = offset % inner_side;
        const std::size_t row = square_row * inner_side + row_in_square;
        const std::size_t column = square_column * inner_side + column_in_square;
        square_value_variables.push_back(variables[cell_index(row, column, outer_side)][value]);
      }
```

`index` selects which box is being processed. `offset` selects a cell inside
that box. The calculation turns those two flat indexes into the absolute row
and column of the cell.

Once the lists are filled, the one-hot factors are created:

```cpp
      [[maybe_unused]] const auto row_factor = create_one_hot_factor(graph, row_value_variables);
      [[maybe_unused]] const auto column_factor = create_one_hot_factor(graph, column_value_variables);
      [[maybe_unused]] const auto square_factor = create_one_hot_factor(graph, square_value_variables);
    }
  }
```

These factors enforce that each value appears exactly once in each row, column,
and box.

### Returning The Variable Grid

The builder returns:

```cpp
  return variables;
}
```

Callers keep this handle grid so they can later extract the solved Sudoku
state.

### `Sudoku::extract_state`

State extraction begins:

```cpp
auto Sudoku::extract_state(
    const Factor_graph& graph,
    const Variables& variables) -> std::vector<int> {
  std::vector<int> state;
  state.reserve(variables.size());
```

`graph` is the solved or partially solved factor graph. `variables` is the
handle grid returned by `add_to_factor_graph`. The output is a flat row-major
vector with one integer per cell.

The outer loop visits each cell's candidate list:

```cpp
  for (const auto& options : variables) {
    int selected_value = -1;
```

`selected_value` starts at `-1`, meaning no selected candidate has been found.

The inner loop checks each candidate:

```cpp
    for (std::size_t value = 0; value < options.size(); ++value) {
      if (graph.value(options[value]) > 0.99) {
        selected_value = static_cast<int>(value);
      }
    }
    state.push_back(selected_value);
  }
```

If a candidate's consensus value is greater than `0.99`, it is treated as
selected. The value index is converted to `int` and stored. If no candidate
passes the threshold, the cell remains `-1`.

The function returns the full state:

```cpp
  return state;
}
```

### Closing The Namespace

The namespace closes with:

```cpp
} // namespace twalib
```

## Important Invariants

- `inner_side` must be positive.
- Given cell indexes must be in range.
- Given values are zero-based and must be less than `outer_side`.
- The direct builder creates one variable for every `(cell, value)` pair.
- Every cell has a one-hot factor.
- Every row-value, column-value, and box-value group has a one-hot factor.
- `Variables` must be used with the same `Factor_graph` that created it.

## Relationship To The Paper

The Sudoku paper section describes one-hot constraints for cells, rows,
columns, and boxes. This file implements that construction directly. Givens
are encoded as infinite-weight initial candidate values, and the `one_hot`
factors perform the simplex-style choices described in the paper.

## Extension Notes

This builder is intentionally straightforward. Prefer keeping it easy to read
and using `Compact_sudoku` for production-sized puzzles where graph size
matters.

If puzzle input validation becomes stricter, keep a distinction between
out-of-range givens, which this file already checks, and contradictory givens,
which require checking row, column, and box consistency.
