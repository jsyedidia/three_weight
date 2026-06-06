# src/problems/circle_packing.cpp

## Role In The System

`circle_packing.cpp` implements the `Circle_packing` problem builder. It
constructs a factor graph that places circles inside a rectangular boundary
without overlap. This is a continuous optimization problem, unlike the discrete
Sudoku problem.

The file provides:

- `Circle_packing::generate_circles` — random initial placement,
- `Circle_packing::create_kiss_factor` — a factor that holds a circle at a
  fixed distance from a point,
- `Circle_packing::create_intersection_factor` — a factor that pushes two
  overlapping circles apart,
- `Circle_packing::add_to_factor_graph` — the full (all-pairs) graph builder,
- `Circle_packing::add_to_factor_graph_fast` — a dynamic version that only
  enables intersection factors for nearby pairs,
- `Circle_packing::extract_circles` — reads current positions from the graph,
- `Circle_packing::max_overlap` — measures the worst remaining violation,
- an anonymous-namespace `Dynamic_intersection_manager` class that powers the
  fast mode.

The public header is `include/twalib/problems/circle_packing.hpp`.

## Code Walkthrough

### Includes

```cpp
#include "twalib/problems/circle_packing.hpp"
```

The own public header comes first, following project convention.

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
#include "twalib/minimizers/in_range.hpp"
```

`graph_edge.hpp` provides the `Graph_edge` handle type needed to create edges.
`weighted_value.hpp` provides `Weighted_value` and `Message_weight` for
building messages. `weighted_value_exchange.hpp` provides
`Weighted_value_exchange`, `Minimization_function`, and `Random_engine`.
`in_range.hpp` provides `create_in_range_factor` for boundary-clamping.

```cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
```

`<algorithm>` supplies `std::max`. `<cmath>` provides `std::hypot`, `std::abs`,
and `std::floor`. `<cstddef>` provides `std::size_t`. `<cstdint>` provides
`std::uint64_t` and `std::uint32_t` for the grid hash. `<memory>` supplies
`std::make_shared` for the dynamic manager's shared lifetime. `<random>`
provides `std::mt19937_64` and `std::uniform_real_distribution` for circle
generation. `<span>` is the non-owning view type used in minimizer signatures.
`<stdexcept>` provides `std::invalid_argument` and related exception types.
`<string>` supplies `std::string` for building error messages.
`<unordered_map>` stores the spatial grid. `<vector>` is the primary dynamic
collection type.

### Namespace

```cpp
namespace twalib {
namespace {
```

The outer namespace is `twalib`. The inner anonymous namespace hides all helper
functions and the `Dynamic_intersection_manager` class from other translation
units.

### `validate_range`

```cpp
auto validate_range(Coordinate_range range, const char* name) -> void {
  if (range.upper < range.lower) {
    throw std::invalid_argument(std::string{name} + " requires lower <= upper");
  }
}
```

Checks that a coordinate range is non-inverted. The `name` parameter is
included in the error message so the caller knows which range failed.

### `validate_radius`

```cpp
auto validate_radius(double radius, const char* name) -> void {
  if (radius < 0.0) {
    throw std::invalid_argument(std::string{name} + " requires non-negative radii");
  }
}
```

Rejects negative radii. Zero-radius circles are permitted (they are points).

### `validate_fits`

```cpp
auto validate_fits(double radius, Coordinate_range range, const char* name) -> void {
  validate_radius(radius, name);
  if (range.lower + radius > range.upper - radius) {
    throw std::invalid_argument(std::string{name} + " radius does not fit in range");
  }
}
```

First checks that the radius is non-negative, then checks that the circle's
diameter does not exceed the range width. The condition
`range.lower + radius > range.upper - radius` is equivalent to
`2 * radius > range.upper - range.lower`.

### `Grid_cell`

```cpp
struct Grid_cell {
  int x = 0;
  int y = 0;

  auto operator==(const Grid_cell&) const -> bool = default;
};
```

An integer coordinate in the spatial hash grid. The defaulted `operator==`
provides equality comparison needed by `std::unordered_map`. The coordinates
are `int` rather than `std::size_t` because circles near the left or bottom
boundary can map to negative grid cells.

### `Grid_cell_hash`

```cpp
struct Grid_cell_hash {
  [[nodiscard]] auto operator()(Grid_cell cell) const -> std::size_t {
    const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.x));
    const auto y = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.y));
    return static_cast<std::size_t>((x << 32U) ^ y);
  }
};
```

A hash functor for `Grid_cell`. It casts each signed 32-bit coordinate to an
unsigned 32-bit value (preserving the bit pattern), widens to 64 bits, then
packs both into a single `std::size_t` using shift-and-XOR. This gives a
fast, collision-resistant hash for the spatial grid.

### `num_pairs`

```cpp
[[nodiscard]] auto num_pairs(std::size_t count) -> std::size_t {
  return count < 2 ? 0 : count * (count - 1) / 2;
}
```

Returns the number of unordered pairs (the size of the upper triangle of the
pair matrix). For fewer than two circles, there are no pairs.

### `pair_index`

```cpp
[[nodiscard]] auto pair_index(std::size_t first, std::size_t second, std::size_t count) -> std::size_t {
  return first * (2 * count - first - 1) / 2 + (second - first - 1);
}
```

Maps an ordered pair `(first, second)` with `first < second` to a flat index
into the upper triangle. This is the standard combinatorial number system
formula for encoding pair indices.

### `factor_for_pair`

```cpp
[[nodiscard]] auto factor_for_pair(
    const Circle_packing_variables& variables,
    std::size_t first,
    std::size_t second) -> Factor_node {
  return variables.intersection_factors[first][second - first - 1];
}
```

Looks up the intersection factor handle for a given pair. Row `first` of
`intersection_factors` stores factors for pairs `(first, first+1),
(first, first+2), ...`, so the column index is `second - first - 1`.

### `Dynamic_intersection_manager` — Class Declaration

```cpp
class Dynamic_intersection_manager {
 public:
```

This class implements the spatial-acceleration strategy for the fast builder.
Rather than keeping all $O(n^2)$ intersection factors active at once, it enables
only the factors for circle pairs that are currently near each other.

### `Dynamic_intersection_manager` — Constructor

```cpp
  Dynamic_intersection_manager(
      Factor_graph& graph,
      const Circle_packing_variables& variables,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range,
      double nearby_radius_scale)
      : graph_(graph),
        variables_(variables),
        horizontal_range_(horizontal_range),
        vertical_range_(vertical_range),
        nearby_radius_scale_(nearby_radius_scale),
        active_pairs_(num_pairs(variables.radii.size()), false) {
    if (nearby_radius_scale < 0.0) {
      throw std::invalid_argument("add_to_factor_graph_fast requires non-negative nearby_radius_scale");
    }

    max_radius_ = 0.0;
    for (const double radius : variables_.radii) {
      max_radius_ = std::max(max_radius_, radius);
    }
    search_buffer_ = nearby_radius_scale_ * max_radius_;
    cell_size_ = std::max(max_radius_ + search_buffer_, 1e-12);
  }
```

The member initializer list stores the graph reference, copies the variable
handles, saves the coordinate ranges and scale parameter, and initializes
`active_pairs_` to all-false with one entry per unordered pair.

The body validates that `nearby_radius_scale` is non-negative. It then finds
the largest radius among all circles and uses it to compute:

- `search_buffer_` = `nearby_radius_scale_ * max_radius_`. This is the extra
  distance beyond touching at which a pair is considered "nearby."
- `cell_size_` = `max(max_radius_ + search_buffer_, 1e-12)`. The grid cell
  edge length. The `1e-12` floor prevents division by zero when all radii are
  zero. The cell size ensures that any two circles whose edges could be within
  `search_buffer_` of each other always land in the same or adjacent grid
  cells.

### `reinitialize`

```cpp
  auto reinitialize() -> void {
    disable_all();
    update();
  }
```

Called when the graph is reinitialized. Disables all intersection factors first
(since positions have been reset), then runs a fresh proximity check to
re-enable the ones that are needed.

### `update`

```cpp
  auto update() -> void {
    std::vector<char> should_enable(active_pairs_.size(), false);
    const std::vector<Circle> circles = Circle_packing::extract_circles(graph_, variables_);
    const auto grid = build_grid(circles);
```

Called after every iteration. It allocates a fresh boolean vector for the
desired enable state, extracts the current circle positions from the graph, and
builds a spatial hash grid from those positions.

```cpp
    for (const auto& [cell, circle_indexes] : grid) {
      for (int y_offset = -1; y_offset <= 1; ++y_offset) {
        for (int x_offset = -1; x_offset <= 1; ++x_offset) {
          const auto neighbor = Grid_cell{cell.x + x_offset, cell.y + y_offset};
          const auto found = grid.find(neighbor);
          if (found == grid.end()) {
            continue;
          }

          for (const std::size_t first : circle_indexes) {
            for (const std::size_t second : found->second) {
              consider_pair(circles, first, second, should_enable);
            }
          }
        }
      }
    }
```

For every occupied grid cell, the code looks at the 3×3 neighborhood (the cell
itself and its 8 neighbors). For each neighbor cell that exists in the grid, it
considers every pair of circles — one from the current cell and one from the
neighbor cell. The `consider_pair` function decides whether each pair should be
enabled. The structured binding `[cell, circle_indexes]` destructures each map
entry into the grid coordinate and the vector of circle indices in that cell.

```cpp
    apply_enabled_pairs(should_enable);
  }
```

After the scan completes, `apply_enabled_pairs` synchronizes the graph's factor
enable/disable state with the desired state.

### Private Section

```cpp
 private:
  using Grid = std::unordered_map<Grid_cell, std::vector<std::size_t>, Grid_cell_hash>;
```

A type alias for the spatial hash grid: a map from grid cells to vectors of
circle indices that currently occupy that cell.

### `cell_for`

```cpp
  [[nodiscard]] auto cell_for(Circle circle) const -> Grid_cell {
    return Grid_cell{
        static_cast<int>(std::floor((circle.x - horizontal_range_.lower) / cell_size_)),
        static_cast<int>(std::floor((circle.y - vertical_range_.lower) / cell_size_))};
  }
```

Converts a circle's continuous position to a discrete grid cell. The position
is offset by the range's lower bound so that the grid origin aligns with the
boundary corner, then divided by `cell_size_` and floored to get an integer
cell coordinate.

### `build_grid`

```cpp
  [[nodiscard]] auto build_grid(const std::vector<Circle>& circles) const -> Grid {
    Grid grid;
    for (std::size_t index = 0; index < circles.size(); ++index) {
      grid[cell_for(circles[index])].push_back(index);
    }
    return grid;
  }
```

Assigns each circle to a grid cell based on its current position. Multiple
circles can share a cell. The resulting grid maps each occupied cell to the list
of circle indices it contains.

### `consider_pair`

```cpp
  auto consider_pair(
      const std::vector<Circle>& circles,
      std::size_t first,
      std::size_t second,
      std::vector<char>& should_enable) const -> void {
    if (first >= second) {
      return;
    }

    const double dx = std::abs(circles[first].x - circles[second].x);
    const double dy = std::abs(circles[first].y - circles[second].y);
    const double search_distance =
        std::max(circles[first].radius, circles[second].radius) + search_buffer_;
    if (dx <= search_distance && dy <= search_distance) {
      should_enable[pair_index(first, second, circles.size())] = true;
    }
  }
```

Checks whether a single pair of circles should have its intersection factor
enabled. The `first >= second` guard ensures each unordered pair is considered
only once (the nested loops in `update` generate both orderings).

The check uses an axis-aligned bounding-box approximation rather than a full
Euclidean distance: if both `dx` and `dy` are within `search_distance`, the
pair is flagged for enabling. The `search_distance` is the larger of the two
radii plus the `search_buffer_`.

With the default `nearby_radius_scale` of 1.4, this is intentionally
conservative: it may enable some pairs that are not actually overlapping so
that the fast builder does not miss pairs that are close enough to matter.
If the scale is made too small, fast mode becomes more aggressive and can miss
overlaps that the full graph would evaluate.

### `apply_enabled_pairs`

```cpp
  auto apply_enabled_pairs(const std::vector<char>& should_enable) -> void {
    const std::size_t count = variables_.radii.size();
    for (std::size_t first = 0; first + 1 < count; ++first) {
      for (std::size_t second = first + 1; second < count; ++second) {
        const std::size_t index = pair_index(first, second, count);
        const bool enabled = should_enable[index] != 0;
        if (enabled != active_pairs_[index]) {
          graph_.set_factor_enabled(factor_for_pair(variables_, first, second), enabled);
          active_pairs_[index] = enabled;
        }
      }
    }
  }
```

Iterates over all unordered pairs. For each pair whose desired state differs
from the current tracked state in `active_pairs_`, it calls
`graph_.set_factor_enabled` to toggle the factor and updates the tracking
vector. The `if` guard avoids redundant enable/disable calls for pairs whose
state has not changed.

### `disable_all`

```cpp
  auto disable_all() -> void {
    const std::size_t count = variables_.radii.size();
    for (std::size_t first = 0; first + 1 < count; ++first) {
      for (std::size_t second = first + 1; second < count; ++second) {
        const std::size_t index = pair_index(first, second, count);
        graph_.set_factor_enabled(factor_for_pair(variables_, first, second), false);
        active_pairs_[index] = false;
      }
    }
  }
```

Unconditionally disables every intersection factor and marks all entries in
`active_pairs_` as false. Used during reinitialization before a fresh proximity
scan.

### Data Members

```cpp
  Factor_graph& graph_;
  Circle_packing_variables variables_;
  Coordinate_range horizontal_range_;
  Coordinate_range vertical_range_;
  double nearby_radius_scale_ = 1.4;
  double search_buffer_ = 0.0;
  double max_radius_ = 0.0;
  double cell_size_ = 1.0;
  std::vector<char> active_pairs_;
};
```

`graph_` is a reference to the live factor graph. `variables_` is a copy of the
circle variable handles and radii. `horizontal_range_` and `vertical_range_`
define the packing boundary. `nearby_radius_scale_` is the user-provided
scaling factor. `search_buffer_` is the computed proximity threshold.
`max_radius_` is the largest radius among all circles. `cell_size_` is the
grid cell edge length. `active_pairs_` is a flat boolean vector tracking the
current enabled state of each pair's intersection factor. `std::vector<char>`
is used instead of `std::vector<bool>` because `vector<bool>` is a
space-optimized specialization that does not provide reference semantics.

### Closing The Anonymous Namespace

```cpp
} // namespace
```

### `Circle_packing::generate_circles`

```cpp
auto Circle_packing::generate_circles(
    std::uint64_t random_seed,
    std::span<const Radius_count> radii,
    Coordinate_range horizontal_range,
    Coordinate_range vertical_range) -> std::vector<Circle> {
  validate_range(horizontal_range, "generate_circles horizontal_range");
  validate_range(vertical_range, "generate_circles vertical_range");
```

Validates both coordinate ranges.

```cpp
  std::size_t total_count = 0;
  for (const Radius_count radius_count : radii) {
    validate_radius(radius_count.radius, "generate_circles");
    total_count += radius_count.count;
  }
```

Iterates over the radius specifications to validate each radius and compute the
total number of circles to generate.

```cpp
  std::mt19937_64 random{random_seed};
  std::uniform_real_distribution<double> x_distribution{horizontal_range.lower, horizontal_range.upper};
  std::uniform_real_distribution<double> y_distribution{vertical_range.lower, vertical_range.upper};
```

Creates a deterministic random engine and two uniform distributions — one for
each coordinate axis. The seed ensures reproducibility.

```cpp
  std::vector<Circle> circles;
  circles.reserve(total_count);
  for (const Radius_count radius_count : radii) {
    for (std::size_t index = 0; index < radius_count.count; ++index) {
      circles.push_back(Circle{
          x_distribution(random),
          y_distribution(random),
          radius_count.radius});
    }
  }

  return circles;
}
```

Generates circles by iterating over each radius specification and creating
`count` circles of that radius, each at a random position within the rectangle.
Circles of the same radius are grouped together in the output. The positions
are not constrained to avoid overlap or stay within boundary offsets — those
constraints are enforced later by the factor graph.

### `Circle_packing::create_kiss_factor`

```cpp
auto Circle_packing::create_kiss_factor(
    Factor_graph& graph,
    Variable_node x,
    Variable_node y,
    double center_x,
    double center_y,
    double exact_distance) -> Factor_node {
  if (exact_distance < 0.0) {
    throw std::invalid_argument("create_kiss_factor requires non-negative exact_distance");
  }
```

Validates that the target distance is non-negative.

```cpp
  const Graph_edge x_edge = graph.create_edge(x);
  const Graph_edge y_edge = graph.create_edge(y);
```

Creates one edge for the circle's x variable and one for its y variable. These
connect the factor to the circle's position.

```cpp
  return graph.create_factor({x_edge, y_edge}, [center_x, center_y, exact_distance](
                                                  std::span<Weighted_value_exchange> exchanges,
                                                  Random_engine&) {
```

Creates a two-edge factor with a minimizer lambda that captures the fixed center
point and the target distance. The `Random_engine&` parameter is unused because
this minimizer has no tie-breaking.

```cpp
    const double x_value = exchanges[0].get().value;
    const double y_value = exchanges[1].get().value;
```

Reads the incoming messages: the current x and y positions proposed by the
variable consensus.

```cpp
    const double dx = x_value - center_x;
    const double dy = y_value - center_y;
    const double distance = std::hypot(dx, dy);
```

Computes the displacement vector from the fixed center to the circle's current
position, and the Euclidean distance.

```cpp
    const double unit_x = distance == 0.0 ? 1.0 : dx / distance;
    const double unit_y = distance == 0.0 ? 0.0 : dy / distance;
```

Computes the unit vector from the center toward the circle. If the circle is
exactly at the center (distance zero), uses the arbitrary direction `(1, 0)` to
break the degeneracy.

```cpp
    exchanges[0].set(Weighted_value{center_x + exact_distance * unit_x, Message_weight::standard});
    exchanges[1].set(Weighted_value{center_y + exact_distance * unit_y, Message_weight::standard});
  });
}
```

Projects the circle onto the circle of radius `exact_distance` centered at the
fixed point, along the direction from center to current position. Always sends
standard-weight messages — even when the constraint is already satisfied. This
is intentionally different from the intersection factor, which sends zero-weight
when satisfied. The kiss factor must keep asserting the correct position so that
other factors do not pull the circle away from the kissing constraint.

### `Circle_packing::create_intersection_factor`

```cpp
auto Circle_packing::create_intersection_factor(
    Factor_graph& graph,
    Variable_node x1,
    Variable_node y1,
    Variable_node x2,
    Variable_node y2,
    double sum_radius) -> Factor_node {
  if (sum_radius < 0.0) {
    throw std::invalid_argument("create_intersection_factor requires non-negative sum_radius");
  }
```

Validates that the sum of radii is non-negative.

```cpp
  const Graph_edge x1_edge = graph.create_edge(x1);
  const Graph_edge y1_edge = graph.create_edge(y1);
  const Graph_edge x2_edge = graph.create_edge(x2);
  const Graph_edge y2_edge = graph.create_edge(y2);
```

Creates four edges — two for each circle's (x, y) position.

```cpp
  return graph.create_factor({x1_edge, y1_edge, x2_edge, y2_edge}, [sum_radius](
                                                                  std::span<Weighted_value_exchange> exchanges,
                                                                  Random_engine&) {
```

Creates a four-edge factor with a minimizer that captures `sum_radius`.

```cpp
    const double x1_value = exchanges[0].get().value;
    const double y1_value = exchanges[1].get().value;
    const double x2_value = exchanges[2].get().value;
    const double y2_value = exchanges[3].get().value;
```

Reads the current positions of both circles from the incoming messages.

```cpp
    const double dx = x2_value - x1_value;
    const double dy = y2_value - y1_value;
    const double distance = std::hypot(dx, dy);
    const double overlap = sum_radius - distance;
```

Computes the displacement from circle 1 to circle 2, the Euclidean distance
between centers, and the overlap amount. Positive overlap means the circles
intersect.

```cpp
    if (overlap < 0.0) {
      exchanges[0].set(Weighted_value{x1_value, Message_weight::zero});
      exchanges[1].set(Weighted_value{y1_value, Message_weight::zero});
      exchanges[2].set(Weighted_value{x2_value, Message_weight::zero});
      exchanges[3].set(Weighted_value{y2_value, Message_weight::zero});
      return;
    }
```

If overlap is negative, the circles are already far enough apart. The factor
sends zero-weight messages — it is satisfied and has no opinion about where the
circles should be. All four exchanges are set individually.

```cpp
    const double unit_x = distance == 0.0 ? 1.0 : dx / distance;
    const double unit_y = distance == 0.0 ? 0.0 : dy / distance;
    const double half_overlap = overlap / 2.0;
    const double move_x = half_overlap * unit_x;
    const double move_y = half_overlap * unit_y;
```

Computes the unit vector from circle 1 toward circle 2 (with arbitrary
direction `(1, 0)` for the coincident case). Then computes the displacement
each circle should move: half the overlap distance along that line, so each
moves equally.

```cpp
    exchanges[0].set(Weighted_value{x1_value - move_x, Message_weight::standard});
    exchanges[1].set(Weighted_value{y1_value - move_y, Message_weight::standard});
    exchanges[2].set(Weighted_value{x2_value + move_x, Message_weight::standard});
    exchanges[3].set(Weighted_value{y2_value + move_y, Message_weight::standard});
  });
}
```

Proposes moving circle 1 backward (subtracting the displacement) and circle 2
forward (adding it), which separates their centers by the overlap amount. Sends
standard-weight messages for all four coordinates.

### `Circle_packing::add_to_factor_graph`

```cpp
auto Circle_packing::add_to_factor_graph(
    Factor_graph& graph,
    std::span<const Circle> circles,
    Coordinate_range horizontal_range,
    Coordinate_range vertical_range,
    std::optional<Kissing_circle> kissing) -> Circle_packing_variables {
  validate_range(horizontal_range, "add_to_factor_graph horizontal_range");
  validate_range(vertical_range, "add_to_factor_graph vertical_range");
  if (kissing.has_value()) {
    validate_radius(kissing->radius, "add_to_factor_graph kissing");
  }
```

Validates the coordinate ranges and, if a kissing circle is provided, its
radius.

```cpp
  Circle_packing_variables variables;
  variables.x.reserve(circles.size());
  variables.y.reserve(circles.size());
  variables.radii.reserve(circles.size());
```

Pre-allocates the output variable vectors.

```cpp
  for (const Circle circle : circles) {
    validate_fits(circle.radius, horizontal_range, "add_to_factor_graph horizontal");
    validate_fits(circle.radius, vertical_range, "add_to_factor_graph vertical");

    const Variable_node x = graph.create_variable(circle.x, Message_weight::standard);
    const Variable_node y = graph.create_variable(circle.y, Message_weight::standard);
    variables.x.push_back(x);
    variables.y.push_back(y);
    variables.radii.push_back(circle.radius);

    [[maybe_unused]] const Factor_node x_bounds =
        create_in_range_factor(graph, x, horizontal_range.lower + circle.radius, horizontal_range.upper - circle.radius);
    [[maybe_unused]] const Factor_node y_bounds =
        create_in_range_factor(graph, y, vertical_range.lower + circle.radius, vertical_range.upper - circle.radius);
  }
```

For each circle: validates that it fits in both ranges, creates two variables
(x, y) initialized to the circle's starting position with standard weight,
records them in `variables`, and creates two `in_range` boundary factors. The
boundary range is offset inward by the circle's radius so the center stays far
enough from the edge to keep the entire circle inside. The `[[maybe_unused]]`
annotations suppress warnings — the factor handles are owned by the graph and
do not need to be tracked externally.

```cpp
  if (!circles.empty()) {
    variables.intersection_factors.reserve(circles.size() - 1);
  }
  for (std::size_t i1 = 0; i1 + 1 < circles.size(); ++i1) {
    std::vector<Factor_node> row;
    row.reserve(circles.size() - i1 - 1);
    for (std::size_t i2 = i1 + 1; i2 < circles.size(); ++i2) {
      row.push_back(create_intersection_factor(
          graph,
          variables.x[i1],
          variables.y[i1],
          variables.x[i2],
          variables.y[i2],
          variables.radii[i1] + variables.radii[i2]));
    }
    variables.intersection_factors.push_back(row);
  }
```

Creates an intersection factor for every unordered pair of circles. The outer
loop iterates over `i1` from 0 to n-2. For each `i1`, a row vector collects
the factors for pairs `(i1, i1+1), (i1, i1+2), ..., (i1, n-1)`. Each factor
uses `sum_radius = radii[i1] + radii[i2]` as the minimum allowed distance
between centers. The reserve calls avoid reallocations.

```cpp
  if (kissing.has_value()) {
    for (std::size_t index = 0; index < circles.size(); ++index) {
      [[maybe_unused]] const Factor_node kiss_factor = create_kiss_factor(
          graph,
          variables.x[index],
          variables.y[index],
          kissing->x,
          kissing->y,
          kissing->radius + variables.radii[index]);
    }
  }
```

If a kissing circle is provided, creates a kiss factor for every circle. The
`exact_distance` is the sum of the kissing circle's radius and the current
circle's radius — this holds them tangent. The kiss factors are not stored in
`variables` because they do not need dynamic enable/disable management.

```cpp
  return variables;
}
```

Returns the complete `Circle_packing_variables` struct containing all variable
handles, radii, and intersection factor handles.

### `Circle_packing::add_to_factor_graph_fast`

```cpp
auto Circle_packing::add_to_factor_graph_fast(
    Factor_graph& graph,
    std::span<const Circle> circles,
    Coordinate_range horizontal_range,
    Coordinate_range vertical_range,
    double nearby_radius_scale) -> Circle_packing_variables {
  Circle_packing_variables variables =
      add_to_factor_graph(graph, circles, horizontal_range, vertical_range);
```

First calls the full builder (without a kissing circle) to create all
variables, boundary factors, and intersection factors.

```cpp
  auto manager = std::make_shared<Dynamic_intersection_manager>(
      graph,
      variables,
      horizontal_range,
      vertical_range,
      nearby_radius_scale);
```

Creates a `Dynamic_intersection_manager` on the heap via `std::make_shared`.
The shared pointer will be captured by the callbacks below, giving the manager
a lifetime tied to the graph's callback storage.

```cpp
  manager->reinitialize();
```

Immediately disables all intersection factors and then enables only those for
pairs that are currently nearby.

```cpp
  graph.add_reinitialize_callback([manager]() {
    manager->reinitialize();
  });
  graph.add_iteration_callback([manager]() {
    manager->update();
  });
```

Registers two callbacks with the graph. The reinitialize callback ensures that
if the graph is reset, all factors start disabled and a fresh proximity scan
runs. The iteration callback runs the spatial grid check after every iteration
so that factors are enabled and disabled as circles move. Both lambdas capture
the `shared_ptr` by value, keeping the manager alive.

```cpp
  return variables;
}
```

Returns the same `variables` struct as the full builder. The caller does not
need to know that dynamic management is happening behind the scenes.

### `Circle_packing::extract_circles`

```cpp
auto Circle_packing::extract_circles(
    const Factor_graph& graph,
    const Circle_packing_variables& variables) -> std::vector<Circle> {
  if (variables.x.size() != variables.y.size() || variables.x.size() != variables.radii.size()) {
    throw std::invalid_argument("extract_circles requires matching variable and radius counts");
  }
```

Validates that the x, y, and radii vectors have the same length. A mismatch
would indicate a corrupted `variables` struct.

```cpp
  std::vector<Circle> circles;
  circles.reserve(variables.radii.size());
  for (std::size_t index = 0; index < variables.radii.size(); ++index) {
    circles.push_back(Circle{
        graph.value(variables.x[index]),
        graph.value(variables.y[index]),
        variables.radii[index]});
  }
  return circles;
}
```

Reads the current consensus value for each circle's x and y variables from the
graph, pairs them with the stored radius, and returns a vector of `Circle`
structs.

### `Circle_packing::max_overlap`

```cpp
auto Circle_packing::max_overlap(
    const Factor_graph& graph,
    const Circle_packing_variables& variables,
    Coordinate_range horizontal_range,
    Coordinate_range vertical_range) -> double {
  validate_range(horizontal_range, "max_overlap horizontal_range");
  validate_range(vertical_range, "max_overlap vertical_range");

  const std::vector<Circle> circles = extract_circles(graph, variables);
  double max_overlap = 0.0;
```

Validates ranges, extracts current positions, and initializes the running
maximum to zero (meaning no violation yet).

```cpp
  for (const Circle circle : circles) {
    max_overlap = std::max(max_overlap, horizontal_range.lower - (circle.x - circle.radius));
    max_overlap = std::max(max_overlap, (circle.x + circle.radius) - horizontal_range.upper);
    max_overlap = std::max(max_overlap, vertical_range.lower - (circle.y - circle.radius));
    max_overlap = std::max(max_overlap, (circle.y + circle.radius) - vertical_range.upper);
  }
```

Checks boundary violations for each circle. The four expressions measure how
far the circle protrudes beyond the left, right, bottom, and top edges
respectively. A positive value means the circle exceeds the boundary.

```cpp
  for (std::size_t i1 = 0; i1 + 1 < circles.size(); ++i1) {
    for (std::size_t i2 = i1 + 1; i2 < circles.size(); ++i2) {
      const double dx = circles[i1].x - circles[i2].x;
      const double dy = circles[i1].y - circles[i2].y;
      const double distance = std::hypot(dx, dy);
      const double overlap = circles[i1].radius + circles[i2].radius - distance;
      max_overlap = std::max(max_overlap, overlap);
    }
  }
```

Checks pairwise overlap for all unordered pairs. The overlap is
`sum_radius - distance`: positive when the circles intersect. The running
maximum tracks the worst violation.

```cpp
  return max_overlap;
}
```

Returns the worst violation found. A return value of zero or below means the
packing is feasible (all circles are within bounds and non-overlapping).
Callers can combine this with `Factor_graph::iterate_until_satisfied` when
they want convergence to mean both stable variable beliefs and satisfied
packing constraints.

### Closing The Namespace

```cpp
} // namespace twalib
```

## Important Invariants

- `intersection_factors[i]` has exactly `circles.size() - i - 1` entries,
  matching the pairs `(i, i+1) ... (i, n-1)`.
- The dynamic manager's `active_pairs_` vector has exactly `num_pairs(n)`
  entries and stays in sync with the graph's factor-enabled state.
- Intersection factors send zero-weight messages when satisfied. This means
  non-overlapping pairs do not influence the consensus even when enabled.
- With the default `nearby_radius_scale`, the grid search is conservative
  enough to catch overlapping pairs. Smaller user-provided scales trade away
  that safety for fewer enabled factors.
- The `Variables` struct must remain paired with the `Factor_graph` that
  created it.

## Relationship To The Paper

Section 5 of the paper describes the circle-packing formulation. Each pair of
circles has an intersection constraint, and each circle has boundary
constraints. The minimizer for intersection is a geometric projection: if two
circles overlap, move each center outward by half the overlap along the line
connecting them. The zero-weight message for non-overlapping pairs corresponds
to the paper's notion of a satisfied constraint that sends no correction.

The fast mode's dynamic factor enable/disable is an implementation
optimization not discussed in the paper. It reduces active factor minimization
work by only activating factors for nearby pairs, exploiting the spatial
locality of overlap violations. The current implementation still stores and
synchronizes an all-pairs enabled-state vector, so it should be understood as a
practical efficiency layer rather than a fully sub-quadratic spatial solver.

## Extension Notes

The fast builder's correctness depends on the nearby search buffer being large
enough for the intended use. The default `nearby_radius_scale` is conservative,
but callers can request smaller values. If the fast builder is later exposed as
a strict "same result as full builder" mode, consider enforcing a minimum scale
or switching the proximity test to a criterion with an explicit no-missed-
overlap guarantee.

The dynamic manager currently loops over all unordered pairs when applying
enabled-state changes. If larger circle counts become important, this is the
next place to optimize: track the candidate pairs discovered by the grid and
only disable pairs that were active but are no longer candidates.
