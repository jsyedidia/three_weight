#ifndef TWALIB_PROBLEMS_CIRCLE_PACKING_HPP
#define TWALIB_PROBLEMS_CIRCLE_PACKING_HPP

#include "twalib/graph/factor_graph.hpp"
#include "twalib/graph/factor_node.hpp"
#include "twalib/graph/variable_node.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace twalib {

struct Coordinate_range {
  double lower = 0.0;
  double upper = 0.0;
};

struct Circle {
  double x = 0.0;
  double y = 0.0;
  double radius = 0.0;
};

struct Radius_count {
  double radius = 0.0;
  std::size_t count = 0;
};

struct Kissing_circle {
  double x = 0.0;
  double y = 0.0;
  double radius = 0.0;
};

struct Circle_packing_variables {
  std::vector<Variable_node> x;
  std::vector<Variable_node> y;
  std::vector<double> radii;
  std::vector<std::vector<Factor_node>> intersection_factors;
};

class Circle_packing {
 public:
  [[nodiscard]] static auto generate_circles(
      std::uint64_t random_seed,
      std::span<const Radius_count> radii,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range) -> std::vector<Circle>;

  [[nodiscard]] static auto create_kiss_factor(
      Factor_graph& graph,
      Variable_node x,
      Variable_node y,
      double center_x,
      double center_y,
      double exact_distance) -> Factor_node;

  [[nodiscard]] static auto create_intersection_factor(
      Factor_graph& graph,
      Variable_node x1,
      Variable_node y1,
      Variable_node x2,
      Variable_node y2,
      double sum_radius) -> Factor_node;

  [[nodiscard]] static auto add_to_factor_graph(
      Factor_graph& graph,
      std::span<const Circle> circles,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range,
      std::optional<Kissing_circle> kissing = std::nullopt) -> Circle_packing_variables;

  [[nodiscard]] static auto add_to_factor_graph_fast(
      Factor_graph& graph,
      std::span<const Circle> circles,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range,
      double nearby_radius_scale = 1.4) -> Circle_packing_variables;

  [[nodiscard]] static auto max_overlap(
      const Factor_graph& graph,
      const Circle_packing_variables& variables,
      Coordinate_range horizontal_range,
      Coordinate_range vertical_range) -> double;

  [[nodiscard]] static auto extract_circles(
      const Factor_graph& graph,
      const Circle_packing_variables& variables) -> std::vector<Circle>;
};

} // namespace twalib

#endif
