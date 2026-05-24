# Sudoku

## Role In The System

Sudoku is the first complete discrete problem domain in this repository. It is
useful because the rules are familiar, the constraints are cleanly local, and
the graph structure is built almost entirely out of one minimizer:
`one_hot`.

The repository has two Sudoku builders:

- `Sudoku`, the direct builder, creates one variable for every `(cell, value)`
  pair.
- `Compact_sudoku`, the compact builder, uses the givens to avoid creating
  variables for candidates that are already ruled out.

Both builders represent the same kind of solution state: each cell chooses one
value, and each row, column, and box contains each value exactly once.

## Grid Size

The builders use two side lengths:

```text
inner_side
outer_side = inner_side * inner_side
```

`inner_side` is the side length of one box. `outer_side` is the side length of
the whole Sudoku grid.

Examples:

- `inner_side = 2` gives a 4-by-4 Sudoku with 2-by-2 boxes.
- `inner_side = 3` gives a 9-by-9 Sudoku with 3-by-3 boxes.
- `inner_side = 4` gives a 16-by-16 Sudoku with 4-by-4 boxes.

Cells are stored in row-major order. The cell at `(row, column)` has flat
index:

```text
row * outer_side + column
```

Values are stored zero-based inside the library. A 9-by-9 puzzle uses values
`0` through `8` internally, even if a text file displays those values as `1`
through `9`.

## Candidate Variables

The core modeling choice is to represent a possible cell value as a variable.
For a cell `c` and a value `v`, the corresponding variable asks:

```text
Does cell c contain value v?
```

The variable is intended to converge to either:

```text
1.0  yes, this candidate is selected
0.0  no, this candidate is not selected
```

This means a direct 9-by-9 Sudoku has:

```text
81 cells * 9 values = 729 candidate variables
```

The direct builder creates all of them. The compact builder creates only the
candidates that survive pruning from the original givens.

## One-Hot Constraints

Sudoku rules are expressed with one-hot factors. A one-hot factor connects to a
set of candidate variables and says exactly one of them should be `1`.

There are four families of one-hot constraints.

### Cell Constraints

Each cell must contain exactly one value.

For one cell in a 9-by-9 Sudoku, the factor connects to nine variables:

```text
(cell, 0), (cell, 1), ..., (cell, 8)
```

Exactly one should be selected.

### Row-Value Constraints

Each row must contain each value exactly once.

For row `r` and value `v`, the factor connects to every cell in that row with
that value:

```text
(row r, column 0, value v)
(row r, column 1, value v)
...
(row r, column outer_side - 1, value v)
```

Exactly one cell in the row should contain `v`.

### Column-Value Constraints

Each column must contain each value exactly once.

For column `c` and value `v`, the factor connects to every cell in that column
with that value.

### Box-Value Constraints

Each box must contain each value exactly once.

For box `b` and value `v`, the factor connects to every cell in that box with
that value.

## Givens

Givens are the pre-filled clues in a Sudoku puzzle. In the API, they are stored
as:

```cpp
std::unordered_map<std::size_t, std::size_t>
```

The key is the flat cell index. The value is the zero-based Sudoku value.

For the direct `Sudoku` builder, a given affects the graph through initial
variable values and weights:

All candidate variables in that cell are initialized with infinite weight. The
given candidate starts at `1.0`; the others start at `0.0`. Those
infinite-weight initial messages are enough to encode the clue, so the direct
builder does not add separate `known_value` factors for givens.

The one-hot and certainty-propagation rules then push that information through
the rest of the graph.

The compact builder goes further. It avoids creating variables for given cells
and for candidates ruled out by givens in the same row, column, or box.

## Direct Builder vs. Compact Builder

The direct builder is conceptually simplest. For an `outer_side` by
`outer_side` Sudoku, it creates:

```text
outer_side^3 variables
4 * outer_side^2 factors
4 * outer_side^3 edges
```

The four factor families are cell, row-value, column-value, and box-value
constraints.

The compact builder is usually better for actual solving. It keeps the same
constraint idea, but it removes candidates that are already impossible because
of the givens. That produces fewer variables, fewer edges, and fewer factor
inputs to minimize.

The direct builder remains valuable because it is easier to understand and
useful for validating the compact builder.

## Extracting A State

Both builders return a `Variables` object that records which graph variables
correspond to which Sudoku candidates. After iteration, callers pass that
object back to `extract_state`.

The returned state is a flat vector of integers in row-major order:

```text
state[cell] = selected value, or -1 if no value is selected
```

Selection is thresholded: a candidate with graph value greater than `0.99` is
treated as selected.

## Relationship To TWA

Sudoku is a clean example of TWA as constraint satisfaction:

- candidate variables are equality nodes,
- one-hot constraints are function cost nodes,
- givens are hard constraints expressed with infinite weight,
- solving means reaching consensus about which candidates are `1`.

For Sudoku, infinite-weight messages encode certainty
from givens and deductions, while standard-weight messages drive ordinary iterative
choice. In the Sudoku builders, zero-weight messages are not used.

## Further Reading

- `notes/src/problems/sudoku.cpp.md` explains the direct builder.
- `notes/src/problems/compact_sudoku.cpp.md` explains the compact builder.
- `notes/src/minimizers/one_hot.cpp.md` explains the main minimizer used by
  Sudoku.
- `notes/src/minimizers/known_value.cpp.md` explains the general hard-value
  minimizer, which is useful outside the current Sudoku builders.
