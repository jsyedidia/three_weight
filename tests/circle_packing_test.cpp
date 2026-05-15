#include "twalib/graph/factor_graph.hpp"
#include "twalib/problems/circle_packing.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace {

using twalib::Circle;
using twalib::Circle_packing;
using twalib::Coordinate_range;
using twalib::Factor_graph;
using twalib::Kissing_circle;
using twalib::Radius_count;
using twalib::Variable_node;

auto nearly_equal(double left, double right, double tolerance = 1e-9) -> bool {
  return std::abs(left - right) < tolerance;
}

auto unit_range() -> Coordinate_range {
  return Coordinate_range{0.0, 1.0};
}

auto test_generate_circles_is_deterministic() -> void {
  const std::vector<Radius_count> radii{
      Radius_count{0.05, 3},
      Radius_count{0.10, 2},
  };

  const auto first = Circle_packing::generate_circles(17, radii, unit_range(), unit_range());
  const auto second = Circle_packing::generate_circles(17, radii, unit_range(), unit_range());
  const auto third = Circle_packing::generate_circles(18, radii, unit_range(), unit_range());

  assert(first.size() == 5);
  assert(first.size() == second.size());
  assert(first[0].x == second[0].x);
  assert(first[0].y == second[0].y);
  assert(first[0].radius == 0.05);
  assert(first[3].radius == 0.10);
  assert(first[0].x != third[0].x || first[0].y != third[0].y);

  for (const Circle circle : first) {
    assert(circle.x >= 0.0);
    assert(circle.x <= 1.0);
    assert(circle.y >= 0.0);
    assert(circle.y <= 1.0);
  }
}

auto test_add_to_factor_graph_counts() -> void {
  Factor_graph graph{};
  const std::vector<Circle> circles{
      Circle{0.2, 0.2, 0.05},
      Circle{0.5, 0.5, 0.05},
      Circle{0.8, 0.8, 0.05},
  };

  const auto variables = Circle_packing::add_to_factor_graph(graph, circles, unit_range(), unit_range());

  assert(variables.x.size() == 3);
  assert(variables.y.size() == 3);
  assert(variables.radii.size() == 3);
  assert(variables.intersection_factors.size() == 2);
  assert(variables.intersection_factors[0].size() == 2);
  assert(variables.intersection_factors[1].size() == 1);
  assert(graph.num_variables() == 6);
  assert(graph.num_factors() == 9);
  assert(graph.num_edges() == 18);
}

auto test_fast_builder_enables_only_nearby_intersections() -> void {
  Factor_graph graph{};
  const std::vector<Circle> circles{
      Circle{0.2, 0.2, 0.1},
      Circle{0.42, 0.2, 0.1},
      Circle{0.8, 0.8, 0.1},
  };

  const auto variables =
      Circle_packing::add_to_factor_graph_fast(graph, circles, unit_range(), unit_range());

  assert(variables.intersection_factors.size() == 2);
  assert(graph.num_variables() == 6);
  assert(graph.num_factors() == 9);
  assert(graph.num_edges() == 18);
  assert(graph.num_enabled_factors() == 7);
  assert(graph.num_enabled_edges() == 10);
  assert(graph.is_factor_enabled(variables.intersection_factors[0][0]));
  assert(!graph.is_factor_enabled(variables.intersection_factors[0][1]));
  assert(!graph.is_factor_enabled(variables.intersection_factors[1][0]));
}

auto test_fast_builder_enables_intersections_after_iteration() -> void {
  Factor_graph graph{};
  const std::vector<Circle> circles{
      Circle{0.75, 0.5, 0.1},
      Circle{1.2, 0.5, 0.1},
  };

  const auto variables =
      Circle_packing::add_to_factor_graph_fast(graph, circles, unit_range(), unit_range());

  assert(!graph.is_factor_enabled(variables.intersection_factors[0][0]));
  assert(graph.num_enabled_factors() == 4);

  graph.iterate();

  assert(graph.is_factor_enabled(variables.intersection_factors[0][0]));
  assert(graph.num_enabled_factors() == 5);
}

auto test_max_overlap_reports_boundary_and_intersection_violations() -> void {
  Factor_graph graph{};
  const std::vector<Circle> circles{
      Circle{-0.05, 0.5, 0.1},
      Circle{0.12, 0.5, 0.1},
  };

  const auto variables = Circle_packing::add_to_factor_graph(graph, circles, unit_range(), unit_range());

  assert(nearly_equal(Circle_packing::max_overlap(graph, variables, unit_range(), unit_range()), 0.15));
}

auto test_boundary_constraints_move_circle_inside() -> void {
  Factor_graph graph{};
  const std::vector<Circle> circles{
      Circle{0.0, 1.0, 0.1},
  };

  const auto variables = Circle_packing::add_to_factor_graph(graph, circles, unit_range(), unit_range());

  assert(graph.iterate_until_converged(100));
  const auto packed = Circle_packing::extract_circles(graph, variables);
  assert(nearly_equal(packed[0].x, 0.1));
  assert(nearly_equal(packed[0].y, 0.9));
  assert(nearly_equal(Circle_packing::max_overlap(graph, variables, unit_range(), unit_range()), 0.0));
}

auto test_intersection_factor_separates_overlapping_circles() -> void {
  Factor_graph graph{};
  const Variable_node x1 = graph.create_variable(0.45);
  const Variable_node y1 = graph.create_variable(0.5);
  const Variable_node x2 = graph.create_variable(0.55);
  const Variable_node y2 = graph.create_variable(0.5);

  [[maybe_unused]] const auto factor =
      Circle_packing::create_intersection_factor(graph, x1, y1, x2, y2, 0.2);

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(x1), 0.4));
  assert(nearly_equal(graph.value(y1), 0.5));
  assert(nearly_equal(graph.value(x2), 0.6));
  assert(nearly_equal(graph.value(y2), 0.5));
}

auto test_intersection_factor_handles_coincident_centers() -> void {
  Factor_graph graph{};
  const Variable_node x1 = graph.create_variable(0.5);
  const Variable_node y1 = graph.create_variable(0.5);
  const Variable_node x2 = graph.create_variable(0.5);
  const Variable_node y2 = graph.create_variable(0.5);

  [[maybe_unused]] const auto factor =
      Circle_packing::create_intersection_factor(graph, x1, y1, x2, y2, 0.2);

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(x1), 0.4));
  assert(nearly_equal(graph.value(y1), 0.5));
  assert(nearly_equal(graph.value(x2), 0.6));
  assert(nearly_equal(graph.value(y2), 0.5));
}

auto test_kiss_factor_handles_coincident_center() -> void {
  Factor_graph graph{};
  const Variable_node x = graph.create_variable(0.0);
  const Variable_node y = graph.create_variable(0.0);

  [[maybe_unused]] const auto factor =
      Circle_packing::create_kiss_factor(graph, x, y, 0.0, 0.0, 1.0);

  assert(graph.iterate_until_converged(100));
  assert(nearly_equal(graph.value(x), 1.0));
  assert(nearly_equal(graph.value(y), 0.0));
}

auto test_non_fast_generated_packing_converges() -> void {
  constexpr double convergence_delta = 1e-5;
  Factor_graph graph{0.07, convergence_delta, 0};
  const std::vector<Radius_count> radii{
      Radius_count{0.055, 12},
  };
  const auto circles = Circle_packing::generate_circles(777, radii, unit_range(), unit_range());
  const auto variables = Circle_packing::add_to_factor_graph(graph, circles, unit_range(), unit_range());

  assert(graph.iterate_until_converged(2000));
  assert(Circle_packing::max_overlap(graph, variables, unit_range(), unit_range()) < 100.0 * convergence_delta);
}

auto test_invalid_inputs_are_explicit() -> void {
  {
    bool threw = false;
    try {
      const std::vector<Radius_count> radii{Radius_count{-1.0, 1}};
      [[maybe_unused]] const auto circles = Circle_packing::generate_circles(0, radii, unit_range(), unit_range());
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  {
    Factor_graph graph{};
    bool threw = false;
    try {
      const std::vector<Circle> circles{Circle{0.5, 0.5, 0.6}};
      [[maybe_unused]] const auto variables = Circle_packing::add_to_factor_graph(graph, circles, unit_range(), unit_range());
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

  {
    Factor_graph graph{};
    bool threw = false;
    try {
      const std::vector<Circle> circles{Circle{0.5, 0.5, 0.1}};
      [[maybe_unused]] const auto variables = Circle_packing::add_to_factor_graph(
          graph,
          circles,
          unit_range(),
          unit_range(),
          Kissing_circle{0.5, 0.5, -0.1});
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    assert(threw);
  }

}

} // namespace

auto main() -> int {
  test_generate_circles_is_deterministic();
  test_add_to_factor_graph_counts();
  test_fast_builder_enables_only_nearby_intersections();
  test_fast_builder_enables_intersections_after_iteration();
  test_max_overlap_reports_boundary_and_intersection_violations();
  test_boundary_constraints_move_circle_inside();
  test_intersection_factor_separates_overlapping_circles();
  test_intersection_factor_handles_coincident_centers();
  test_kiss_factor_handles_coincident_center();
  test_non_fast_generated_packing_converges();
  test_invalid_inputs_are_explicit();
  return 0;
}
