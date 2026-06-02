# Circle Packing

## Role In The System

Circle packing is the main continuous problem domain in this repository. The
goal is to place circles inside a rectangular boundary without overlap.

The problem is useful for understanding TWA because the constraints have a
clear geometric interpretation:

- boundary constraints keep each circle inside the rectangle,
- intersection constraints push overlapping circles apart,
- optional kissing constraints keep circles tangent to a fixed circle.

Unlike Sudoku, circle packing uses zero-weight messages heavily. A satisfied
non-overlap constraint sends zero weight, meaning it has no active correction
to make.

## Variables

Each circle has two graph variables:

```text
x position
y position
```

The radius is not a variable in the current model. It is fixed input data
stored alongside the variable handles.

For `n` circles, the builder creates:

```text
2 * n variables
```

The public `Circle_packing_variables` object stores the handles:

```text
x[i]      x-position variable for circle i
y[i]      y-position variable for circle i
radii[i]  fixed radius for circle i
```

The current circle positions can be read back with
`Circle_packing::extract_circles`.

## Boundary Constraints

Each circle must stay entirely inside the rectangular packing region. If a
circle has radius `r`, then its center must satisfy:

```text
horizontal.lower + r <= x <= horizontal.upper - r
vertical.lower + r <= y <= vertical.upper - r
```

The builder creates two `in_range` factors for each circle:

- one for the `x` coordinate,
- one for the `y` coordinate.

When the center is already inside the allowed range, the `in_range` factor
sends zero weight. When the center is outside, it clamps the coordinate to the
nearest allowed endpoint with standard weight.

## Intersection Constraints

Two circles with radii `r1` and `r2` do not overlap when the distance between
their centers is at least:

```text
r1 + r2
```

The intersection factor connects to four variables:

```text
x1, y1, x2, y2
```

It reads the current center positions. If the circles are not overlapping, it
sends zero-weight messages for all four coordinates. If they overlap, it
projects the two centers apart along the line connecting them.

For an overlap of length `d`, each circle is moved by `d / 2` in opposite
directions. Moving both centers symmetrically is the local projection used by
the minimizer.

If the two centers are exactly coincident, there is no well-defined direction
between them. The implementation chooses the deterministic `+x` direction so
the projection remains stable and reproducible.

## Full Builder

`Circle_packing::add_to_factor_graph` builds the full graph:

- two variables per circle,
- two boundary factors per circle,
- one intersection factor for every unordered pair of circles,
- optional kiss factors if a kissing circle is supplied.

For `n` circles, the full builder creates:

```text
2 * n variables
2 * n + n * (n - 1) / 2 factors
4 * n + 4 * n * (n - 1) / 2 edges
```

The factor count excludes optional kiss factors. If a kissing circle is
provided, there is one additional two-edge kiss factor per circle.

Even in the full builder, intersection factors that are already satisfied send
zero-weight messages. That means distant circles do not affect the variable
consensus, although their factors are still evaluated.

## Fast Builder

`Circle_packing::add_to_factor_graph_fast` builds the same variables and
intersection factors as the full builder, but then disables most intersection
factors and uses a dynamic spatial grid to reenable only nearby pairs.

This is an efficiency layer for TWA. The mathematical role of a disabled
far-away intersection factor is the same as an enabled far-away intersection
factor that would send zero-weight messages: it should not affect consensus.

The fast builder still stores handles for all intersection factors. The
dynamic manager owns the policy for enabling and disabling them after each
iteration.

The `nearby_radius_scale` parameter controls the buffer around circles used by
the spatial search. The default is conservative. Smaller values enable fewer
pairs but can miss relevant overlaps if made too small; larger values enable
more pairs and behave more like the full builder.

The current implementation reduces active factor minimization work, but it is
not a pure `O(n)` implementation: it still keeps a flat pair-state vector and
synchronizes enabled/disabled state across all pairs.

## Kissing Constraints

A kiss factor holds a circle at an exact distance from a fixed point. The
distance is usually the sum of the moving circle's radius and the fixed
circle's radius, which makes the two circles tangent.

The kiss factor connects to one circle's `x` and `y` variables. It projects
the current point onto the circle centered at the fixed point with the given
exact distance.

Unlike an intersection factor, a kiss factor always sends standard-weight
messages. Even when the current point is already at the correct distance, the
factor must keep asserting the tangent relationship so other constraints do
not pull the circle away.

## Measuring Progress

`Circle_packing::max_overlap` reports the worst current violation:

- boundary protrusion beyond the rectangle,
- pairwise circle overlap.

A value near zero means the current beliefs form a feasible packing. The GUI
uses this as an easy status readout while the graph iterates.

## Relationship To TWA

Circle packing shows why zero-weight messages are useful:

- an active boundary violation sends a standard-weight correction,
- a satisfied boundary constraint sends zero weight,
- an active intersection sends standard-weight separation messages,
- a satisfied intersection sends zero weight.

The variable consensus step averages the active standard-weight proposals. The
accumulated disagreement `u` helps coordinate the competing geometric
constraints over iterations.

## Further Reading

- [`notes/src/problems/circle_packing.cpp.md`](../src/problems/circle_packing.cpp.md) explains the implementation.
- [`notes/include/twalib/problems/circle_packing.hpp.md`](../include/twalib/problems/circle_packing.hpp.md) explains the public
  API.
- [`notes/src/minimizers/in_range.cpp.md`](../src/minimizers/in_range.cpp.md) explains the boundary minimizer.
- [`notes/concepts/weights.md`](weights.md) explains zero, standard, and infinite weights.
