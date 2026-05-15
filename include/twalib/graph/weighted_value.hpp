#ifndef TWALIB_GRAPH_WEIGHTED_VALUE_HPP
#define TWALIB_GRAPH_WEIGHTED_VALUE_HPP

#include <limits>

namespace twalib {

enum class Message_weight {
  zero,
  standard,
  infinite,
};

[[nodiscard]] constexpr auto message_weight_value(Message_weight weight) -> double {
  switch (weight) {
  case Message_weight::zero:
    return 0.0;
  case Message_weight::standard:
    return 1.0;
  case Message_weight::infinite:
    return std::numeric_limits<double>::infinity();
  }

  return 0.0;
}

struct Weighted_value {
  double value = 0.0;
  Message_weight weight = Message_weight::standard;
};

} // namespace twalib

#endif
