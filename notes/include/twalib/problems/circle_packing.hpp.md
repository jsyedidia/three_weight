# include/twalib/problems/circle_packing.hpp

## Role In The System

This header declares the public circle-packing problem builder and its support
types.

Circle packing places fixed-radius circles inside a rectangular boundary while
trying to remove overlaps. The builder creates a factor graph with one `x`
variable and one `y` variable per circle, boundary factors for each coordinate,
and intersection factors for circle pairs.

The implementation lives in `src/problems/circle_packing.cpp`.

## Main Types And Functions

The support structs are:

```cpp
Coordinate_range
Circle
Radius_count
Kissing_circle
Circle_packing_variables
```

The public API class is:

```cpp
class Circle_packing
```

It provides static helper functions for generating random starting circles,
creating individual geometric factors, building full or fast factor graphs,
measuring overlap, and extracting current circle beliefs.

## State At A Glance

The returned variable bundle is:

```cpp
struct Circle_packing_variables {
  std::vector<Variable_node> x;
  std::vector<Variable_node> y;
  std::vector<double> radii;
  std::vector<std::vector<Factor_node>> intersection_factors;
};
```

`x[i]`, `y[i]`, and `radii[i]` describe circle `i`. The
`intersection_factors` member stores handles for pairwise intersection factors
in upper-triangular form.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_PROBLEMS_CIRCLE_PACKING_HPP
#define TWALIB_PROBLEMS_CIRCLE_PACKING_HPP
```

It prevents duplicate processing of this public problem-builder header.

### Graph Includes

The graph includes are:

```cpp
#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"
```

`Factor_graph` is the graph being built and queried. `Factor_node` is stored
for pairwise intersection factors. `Variable_node` is stored for circle
coordinate variables.

### Standard Includes

The standard includes are:

```cpp
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
```

`<cstddef>` provides `std::size_t`. `<cstdint>` provides `std::uint64_t` for
random seeds. `<optional>` represents an optional kissing circle. `<span>`
lets callers pass non-owning views of circles and radius specifications.
`<vector>` stores circles, variable handles, radii, and factor handles.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

The circle-packing API is part of the public library namespace.

### `Coordinate_range`

The coordinate range type is:

```cpp
struct Coordinate_range {
  double lower = 0.0;
  double upper = 0.0;
};
```

It represents an inclusive one-dimensional interval. Circle packing uses one
range for horizontal coordinates and one for vertical coordinates.

### `Circle`

The circle value type is:

```cpp
struct Circle {
  double x = 0.0;
  double y = 0.0;
  double radius = 0.0;
};
```

It stores a circle center and fixed radius. The same type is used for input
circles and extracted output circles.

### `Radius_count`

The radius-count type is:

```cpp
struct Radius_count {
  double radius = 0.0;
  std::size_t count = 0;
};
```

`generate_circles` uses this to say how many circles of each radius should be
created.

### `Kissing_circle`

The kissing-circle type is:

```cpp
struct Kissing_circle {
  double x = 0.0;
  double y = 0.0;
  double radius = 0.0;
};
```

It describes a fixed circle that generated circles may be constrained to kiss.
The fixed circle itself is not represented by graph variables.

### `Circle_packing_variables`

The returned handle bundle is:

```cpp
struct Circle_packing_variables {
  std::vector<Variable_node> x;
  std::vector<Variable_node> y;
  std::vector<double> radii;
  std::vector<std::vector<Factor_node>> intersection_factors;
};
```

The first three vectors have matching indexes: entry `i` describes circle `i`.
The `intersection_factors` vector stores pairwise factor handles. Row `i`
contains factors for pairs `(i, i + 1)`, `(i, i + 2)`, and so on.

The struct closes with:

```cpp
};
```

### `Circle_packing`

The public API class begins:

```cpp
class Circle_packing {
 public:
```

Like the Sudoku builders, this is a grouping type for static functions. It is
not meant to be instantiated.

### `generate_circles`

The random generation declaration is:

```cpp
  [[nodiscard]] static auto generate_circles(
      std::uint64_t random_seed,
      std::span<const Radius_count> radii,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range) -> std::vector<Circle>;
```

It creates reproducible random starting positions. `random_seed` controls the
random engine. `radii` says how many circles of each radius to generate. The
two coordinate ranges define the rectangle from which initial centers are
sampled.

The function returns a vector of generated circles.

### `create_kiss_factor`

The kiss-factor declaration is:

```cpp
  [[nodiscard]] static auto create_kiss_factor(
      Factor_graph& graph,
      Variable_node x,
      Variable_node y,
      double center_x,
      double center_y,
      double exact_distance) -> Factor_node;
```

This creates a factor for one circle center. The factor projects `(x, y)` onto
the circle centered at `(center_x, center_y)` with radius `exact_distance`.

The returned `Factor_node` belongs to `graph`.

### `create_intersection_factor`

The intersection-factor declaration is:

```cpp
  [[nodiscard]] static auto create_intersection_factor(
      Factor_graph& graph,
      Variable_node x1,
      Variable_node y1,
      Variable_node x2,
      Variable_node y2,
      double sum_radius) -> Factor_node;
```

This creates a factor between two circle centers. The factor keeps their
distance at least `sum_radius`, which is normally the sum of the two radii.

The factor sends zero-weight messages when the circles are already separated
and standard-weight messages when it needs to push them apart.

### `add_to_factor_graph`

The full builder declaration is:

```cpp
  [[nodiscard]] static auto add_to_factor_graph(
      Factor_graph& graph,
      std::span<const Circle> circles,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range,
      std::optional<Kissing_circle> kissing = std::nullopt) -> Circle_packing_variables;
```

It adds all variables, boundary factors, and pairwise intersection factors to
`graph`. The optional `kissing` argument adds one kiss factor per circle.

The returned `Circle_packing_variables` object is needed for extraction,
overlap measurement, GUI inspection, and dynamic factor management.

### `add_to_factor_graph_fast`

The fast builder declaration is:

```cpp
  [[nodiscard]] static auto add_to_factor_graph_fast(
      Factor_graph& graph,
      std::span<const Circle> circles,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range,
      double nearby_radius_scale = 1.4) -> Circle_packing_variables;
```

It builds the same pairwise intersection factors as the full builder, then
uses callbacks to enable only nearby intersection factors during iteration.

`nearby_radius_scale` controls the search buffer used by the dynamic spatial
grid. The default is `1.4`.

### `max_overlap`

The overlap-measurement declaration is:

```cpp
  [[nodiscard]] static auto max_overlap(
      const Factor_graph& graph,
      const Circle_packing_variables& variables,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range) -> double;
```

It reads the current circle beliefs and returns the worst boundary or pairwise
overlap violation. A value near zero indicates a feasible packing.

### `extract_circles`

The extraction declaration is:

```cpp
  [[nodiscard]] static auto extract_circles(
      const Factor_graph& graph,
      const Circle_packing_variables& variables) -> std::vector<Circle>;
```

It reads the current graph values for each circle's `x` and `y` variables and
combines them with the stored radii.

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

- `Circle_packing_variables::x`, `y`, and `radii` must have matching sizes.
- `intersection_factors[i][j]` represents the pair `(i, i + j + 1)`.
- `Circle_packing_variables` must be used with the graph that created it.
- Radii and exact distances must be non-negative.
- A circle radius must fit within both coordinate ranges before the graph is
  built.

## Relationship To The Paper

The paper's circle-packing problem uses boundary and pairwise non-overlap
constraints. This header exposes the public API for building those constraints
as TWA factors.

## Extension Notes

Keep this header focused on problem construction and inspection. Lower-level
geometric projection details belong in `src/problems/circle_packing.cpp`, and
interactive behavior should usually be layered on top of the public handles
returned in `Circle_packing_variables`.
