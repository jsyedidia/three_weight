# GUI Help

The GUI app is the ImGui-based inspection tool for this repository. Run it with:

```sh
pixi run gui
```

It is meant for watching the Three-Weight Algorithm work, comparing graph
builders, and developing intuition about messages, weights, certainty, and
dynamic factors.

You will see three panels, that you can resize and rearrange as you like.

## Controls Panel

The `Controls` panel is the main place to choose a problem, rebuild the graph,
and run iterations.

### Domain

`Domain` chooses the kind of problem to inspect:

- `Sudoku`
- `Circle Packing`

Changing the domain rebuilds the graph immediately.

### Sudoku Settings

When `Domain` is `Sudoku`, the controls include `Puzzle` and `Build`.

`Puzzle` chooses one of the built-in Sudoku fixtures:

- `2x2`: a 4-by-4 Sudoku with 2-by-2 boxes.
- `3x3`: a 9-by-9 Sudoku with 3-by-3 boxes.
- `4x4 medium`: a 16-by-16 Sudoku with 4-by-4 boxes.
- `4x4 hard`: a harder 16-by-16 Sudoku.
- `5x5`: a 25-by-25 Sudoku with 5-by-5 boxes.

Changing the puzzle rebuilds the graph immediately and selects the first cell.

`Build` chooses the Sudoku graph construction:

- `Compact`: creates variables only for candidate values that survive pruning
  from the original clues. This is the default and is usually much smaller.
- `Full`: creates the full cell/value graph. This is useful for comparing the
  compact builder with the more direct construction.

Changing the Sudoku build rebuilds the graph immediately.

### Circle Packing Settings

When `Domain` is `Circle Packing`, the controls include `Problem`, `Build`,
`Circle count`, `Density`, and sometimes `Nearby radius scale`.

`Problem` chooses the radius distribution:

- `equal radii`: all circles start with the same radius before density scaling.
- `mixed radii`: circles are divided among small, medium, and larger radii
  before density scaling.

Changing the circle-packing problem rebuilds the graph immediately and resets
the circle count to the base count for that problem.

`Build` chooses the circle-packing graph construction:

- `Fast`: builds the pairwise intersection factors but enables only nearby
  dynamic intersection factors. This is the scalable default.
- `Full`: keeps all pairwise intersection factors enabled. This algorithm will
still work reasonably well because zero-weight messages will be sent, but it is
somewhat slower.

Changing the circle-packing build rebuilds the graph immediately.

`Circle count` sets the target number of circles. It is clamped to the range
`1` through `1000`. Changing it marks the settings as dirty; press `Reset`,
`Step`, `Run`, or `Run to convergence` to rebuild with the new value.

`Density` sets the target circle area divided by the available rectangle area.
The GUI computes a radius scale from this target density and applies it to all
radii. Changing it marks the settings as dirty.

`Nearby radius scale` appears only for the `Fast` build. It controls the search
buffer used to decide which pairwise intersection factors should be enabled.
Larger values enable more nearby factors; smaller values enable fewer. Changing
it marks the settings as dirty.

### Shared Solver Settings

`Random seed` controls deterministic random choices, including initial circle
positions and one-hot tie-breaking. It cannot go below zero. Changing it marks
the settings as dirty.

`Learning rate` is the TWA update parameter used when updating the accumulated
disagreement `u`. Sudoku and circle packing keep separate learning-rate values
so switching domains preserves the last value used for each domain. Changing
the learning rate marks the settings as dirty.

`Convergence delta` is the per-edge message-change threshold used to decide
whether the graph has converged. Smaller values require tighter convergence.
Changing it marks the settings as dirty.

`Max iterations` controls how many iterations `Run to convergence` may perform
before stopping.

`Iterations/frame` controls how many iterations are run on each GUI frame while
the solver is in `Run` mode. It is clamped to the range `1` through `200`.

When settings are dirty, the panel shows:

```text
Settings changed. Reset to apply.
```

Pressing `Reset`, `Step`, `Run`, or `Run to convergence` will rebuild first
when settings are dirty.

### Run Buttons

`Reset` rebuilds the current graph from the current controls.

`Step` runs one iteration. If settings are dirty, it rebuilds first and then
steps.

`Run` toggles continuous iteration. While running, the button changes to
`Pause`.

`Run to convergence` runs until the graph converges or reaches `Max
iterations`.

### Status Readout

The lower part of the `Controls` panel reports:

- `Solver`: currently always `TWA`.
- `Iterations`: number of iterations completed since the last rebuild.
- `Converged`: whether the graph has met the convergence threshold.
- `Solved`: for Sudoku, whether the extracted grid matches the stored solution.
- `Max overlap`: for circle packing, the worst current overlap.
- `Circles`: for circle packing, the number of circles.
- `Density`: for circle packing, the current density after radius scaling.
- `Intersections`: for circle packing, enabled dynamic intersection factors
  compared with total dynamic intersection factors.
- `Variables`, `Factors`, and `Edges`: graph sizes, with enabled counts where
  applicable.
- `Last step`: wall-clock time for the most recent step or run command.

## Sudoku View

The `Sudoku` panel shows the current extracted Sudoku grid. Click a cell to
inspect it in the `Selected Cell` panel.

Cell coloring:

- Selected cells are blue.
- Original clues are dark gray.
- Solved non-clue cells are green when the extracted grid matches the stored
  solution.
- Cells with an infinite-weight selected value get an outline:
  - silver for original clues,
  - gold for cells that became certain through solving.

The board displays values using normal decimal labels starting at `1`.

## Selected Cell Panel

The `Selected Cell` panel reports:

- row and column,
- extracted value,
- original clue, if the cell is a clue,
- known solution value, when a solution is stored for the fixture.

Below that, it shows one progress bar for each candidate variable that exists
in the current build.

Each candidate bar label contains:

- the candidate value,
- the raw graph value for that candidate,
- `clue` if it is certain because of an original clue,
- `certain` if it has infinite weight for another reason.

The bar fill is clamped visually to the range `0` through `1`, but the numeric
label shows the raw value. In compact builds, clue cells and pruned candidates
may have no candidate variable; in that case the panel reports that there are
no candidate variables.

## Circle Packing View

The `Circle Packing` panel shows:

- number of circles,
- density,
- maximum overlap,
- dynamic intersection-factor counts,
- a canvas drawing the current circle beliefs.

On the canvas:

- The rectangle is the allowed packing region.
- Circles are drawn at their current belief positions.
- The selected circle gets a gold outer ring.
- Circles with visible overlap get a red outline.
- In `Fast` build mode, yellow lines show enabled dynamic intersection factors.
- In `Full` build mode, pair lines are not drawn because every pair has an
  enabled intersection factor.

If the fast build has more than `5000` enabled intersection factors, connection
lines are capped and the panel displays a note.

Click a circle to select it. The current GUI selects the nearest circle to the
click position; dragging is planned for richer future tooling but is not
currently implemented.

## Selected Circle Panel

The `Selected Circle` panel reports:

- selected circle index,
- current `x` and `y` belief positions,
- radius,
- boundary overlap,
- maximum pair overlap involving the selected circle,
- number of enabled neighboring dynamic intersection factors.

These values are useful for understanding whether a circle is stuck because of
the boundary, nearby circles, or the fast dynamic-factor neighborhood.

## Practical Tips

- Use `Compact` Sudoku for large puzzles unless you specifically want to
  inspect the full cell/value graph.
- Use `Fast` circle packing for larger circle counts. Use `Full` only for
  comparison purposes.
- If a setting says it changed, press `Reset` to rebuild before interpreting
  graph counts.
- For circle packing, a higher density makes the problem harder. Near dense
  packings, small changes in seed, learning rate, or nearby radius scale can
  change convergence behavior.
- For Sudoku, certain non-clue cells are especially interesting: they show
  where the message-passing process has produced an infinite-weight conclusion.
