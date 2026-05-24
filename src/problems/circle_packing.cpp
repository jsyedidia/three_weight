#include "twalib/problems/circle_packing.hpp"

#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
#include "twalib/graph/weighted_value_exchange.hpp"
#include "twalib/minimizers/in_range.hpp"

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

namespace twalib {
namespace {

auto validate_range(Coordinate_range range, const char* name) -> void {
  if (range.upper < range.lower) {
    throw std::invalid_argument(std::string{name} + " requires lower <= upper");
  }
}

auto validate_radius(double radius, const char* name) -> void {
  if (radius < 0.0) {
    throw std::invalid_argument(std::string{name} + " requires non-negative radii");
  }
}

auto validate_fits(double radius, Coordinate_range range, const char* name) -> void {
  validate_radius(radius, name);
  if (range.lower + radius > range.upper - radius) {
    throw std::invalid_argument(std::string{name} + " radius does not fit in range");
  }
}

struct Grid_cell {
  int x = 0;
  int y = 0;

  auto operator==(const Grid_cell&) const -> bool = default;
};

struct Grid_cell_hash {
  [[nodiscard]] auto operator()(Grid_cell cell) const -> std::size_t {
    const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.x));
    const auto y = static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.y));
    return static_cast<std::size_t>((x << 32U) ^ y);
  }
};

[[nodiscard]] auto num_pairs(std::size_t count) -> std::size_t {
  return count < 2 ? 0 : count * (count - 1) / 2;
}

[[nodiscard]] auto pair_index(std::size_t first, std::size_t second, std::size_t count) -> std::size_t {
  return first * (2 * count - first - 1) / 2 + (second - first - 1);
}

[[nodiscard]] auto factor_for_pair(
    const Circle_packing_variables& variables,
    std::size_t first,
    std::size_t second) -> Factor_node {
  return variables.intersection_factors[first][second - first - 1];
}

class Dynamic_intersection_manager {
 public:
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

  auto reinitialize() -> void {
    disable_all();
    update();
  }

  auto update() -> void {
    std::vector<char> should_enable(active_pairs_.size(), false);
    const std::vector<Circle> circles = Circle_packing::extract_circles(graph_, variables_);
    const auto grid = build_grid(circles);

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

    apply_enabled_pairs(should_enable);
  }

 private:
  using Grid = std::unordered_map<Grid_cell, std::vector<std::size_t>, Grid_cell_hash>;

  [[nodiscard]] auto cell_for(Circle circle) const -> Grid_cell {
    return Grid_cell{
        static_cast<int>(std::floor((circle.x - horizontal_range_.lower) / cell_size_)),
        static_cast<int>(std::floor((circle.y - vertical_range_.lower) / cell_size_))};
  }

  [[nodiscard]] auto build_grid(const std::vector<Circle>& circles) const -> Grid {
    Grid grid;
    for (std::size_t index = 0; index < circles.size(); ++index) {
      grid[cell_for(circles[index])].push_back(index);
    }
    return grid;
  }

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

} // namespace

auto Circle_packing::generate_circles(
    std::uint64_t random_seed,
    std::span<const Radius_count> radii,
    Coordinate_range horizontal_range,
    Coordinate_range vertical_range) -> std::vector<Circle> {
  validate_range(horizontal_range, "generate_circles horizontal_range");
  validate_range(vertical_range, "generate_circles vertical_range");

  std::size_t total_count = 0;
  for (const Radius_count radius_count : radii) {
    validate_radius(radius_count.radius, "generate_circles");
    total_count += radius_count.count;
  }

  std::mt19937_64 random{random_seed};
  std::uniform_real_distribution<double> x_distribution{horizontal_range.lower, horizontal_range.upper};
  std::uniform_real_distribution<double> y_distribution{vertical_range.lower, vertical_range.upper};

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

  const Graph_edge x_edge = graph.create_edge(x);
  const Graph_edge y_edge = graph.create_edge(y);

  return graph.create_factor({x_edge, y_edge}, [center_x, center_y, exact_distance](
                                                  std::span<Weighted_value_exchange> exchanges,
                                                  Random_engine&) {
    const double x_value = exchanges[0].get().value;
    const double y_value = exchanges[1].get().value;

    const double dx = x_value - center_x;
    const double dy = y_value - center_y;
    const double distance = std::hypot(dx, dy);

    const double unit_x = distance == 0.0 ? 1.0 : dx / distance;
    const double unit_y = distance == 0.0 ? 0.0 : dy / distance;

    exchanges[0].set(Weighted_value{center_x + exact_distance * unit_x, Message_weight::standard});
    exchanges[1].set(Weighted_value{center_y + exact_distance * unit_y, Message_weight::standard});
  });
}

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

  const Graph_edge x1_edge = graph.create_edge(x1);
  const Graph_edge y1_edge = graph.create_edge(y1);
  const Graph_edge x2_edge = graph.create_edge(x2);
  const Graph_edge y2_edge = graph.create_edge(y2);

  return graph.create_factor({x1_edge, y1_edge, x2_edge, y2_edge}, [sum_radius](
                                                                  std::span<Weighted_value_exchange> exchanges,
                                                                  Random_engine&) {
    const double x1_value = exchanges[0].get().value;
    const double y1_value = exchanges[1].get().value;
    const double x2_value = exchanges[2].get().value;
    const double y2_value = exchanges[3].get().value;

    const double dx = x2_value - x1_value;
    const double dy = y2_value - y1_value;
    const double distance = std::hypot(dx, dy);
    const double overlap = sum_radius - distance;

    if (overlap < 0.0) {
      exchanges[0].set(Weighted_value{x1_value, Message_weight::zero});
      exchanges[1].set(Weighted_value{y1_value, Message_weight::zero});
      exchanges[2].set(Weighted_value{x2_value, Message_weight::zero});
      exchanges[3].set(Weighted_value{y2_value, Message_weight::zero});
      return;
    }

    const double unit_x = distance == 0.0 ? 1.0 : dx / distance;
    const double unit_y = distance == 0.0 ? 0.0 : dy / distance;
    const double half_overlap = overlap / 2.0;
    const double move_x = half_overlap * unit_x;
    const double move_y = half_overlap * unit_y;

    exchanges[0].set(Weighted_value{x1_value - move_x, Message_weight::standard});
    exchanges[1].set(Weighted_value{y1_value - move_y, Message_weight::standard});
    exchanges[2].set(Weighted_value{x2_value + move_x, Message_weight::standard});
    exchanges[3].set(Weighted_value{y2_value + move_y, Message_weight::standard});
  });
}

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

  Circle_packing_variables variables;
  variables.x.reserve(circles.size());
  variables.y.reserve(circles.size());
  variables.radii.reserve(circles.size());

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

  return variables;
}

auto Circle_packing::add_to_factor_graph_fast(
    Factor_graph& graph,
    std::span<const Circle> circles,
    Coordinate_range horizontal_range,
    Coordinate_range vertical_range,
    double nearby_radius_scale) -> Circle_packing_variables {
  Circle_packing_variables variables =
      add_to_factor_graph(graph, circles, horizontal_range, vertical_range);

  auto manager = std::make_shared<Dynamic_intersection_manager>(
      graph,
      variables,
      horizontal_range,
      vertical_range,
      nearby_radius_scale);

  manager->reinitialize();
  graph.add_reinitialize_callback([manager]() {
    manager->reinitialize();
  });
  graph.add_iteration_callback([manager]() {
    manager->update();
  });

  return variables;
}

auto Circle_packing::extract_circles(
    const Factor_graph& graph,
    const Circle_packing_variables& variables) -> std::vector<Circle> {
  if (variables.x.size() != variables.y.size() || variables.x.size() != variables.radii.size()) {
    throw std::invalid_argument("extract_circles requires matching variable and radius counts");
  }

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

auto Circle_packing::max_overlap(
    const Factor_graph& graph,
    const Circle_packing_variables& variables,
    Coordinate_range horizontal_range,
    Coordinate_range vertical_range) -> double {
  validate_range(horizontal_range, "max_overlap horizontal_range");
  validate_range(vertical_range, "max_overlap vertical_range");

  const std::vector<Circle> circles = extract_circles(graph, variables);
  double max_overlap = 0.0;

  for (const Circle circle : circles) {
    max_overlap = std::max(max_overlap, horizontal_range.lower - (circle.x - circle.radius));
    max_overlap = std::max(max_overlap, (circle.x + circle.radius) - horizontal_range.upper);
    max_overlap = std::max(max_overlap, vertical_range.lower - (circle.y - circle.radius));
    max_overlap = std::max(max_overlap, (circle.y + circle.radius) - vertical_range.upper);
  }

  for (std::size_t i1 = 0; i1 + 1 < circles.size(); ++i1) {
    for (std::size_t i2 = i1 + 1; i2 < circles.size(); ++i2) {
      const double dx = circles[i1].x - circles[i2].x;
      const double dy = circles[i1].y - circles[i2].y;
      const double distance = std::hypot(dx, dy);
      const double overlap = circles[i1].radius + circles[i2].radius - distance;
      max_overlap = std::max(max_overlap, overlap);
    }
  }

  return max_overlap;
}

} // namespace twalib
