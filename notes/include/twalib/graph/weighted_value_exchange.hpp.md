# include/twalib/graph/weighted_value_exchange.hpp

## Role In The System

`Weighted_value_exchange` is the object a factor minimizer uses to read the
incoming message for one edge and write the outgoing message for that same
edge.

During the factor pass, the graph builds a span of exchanges for a factor's
enabled edges. The minimization function receives that span, reads each
current `Weighted_value`, and writes back the factor's preferred
`Weighted_value` for each edge.

This keeps minimizers independent from graph internals. A minimizer does not
need to know about `Edge_data`, `Factor_data`, or vector indexes beyond the
edge handle carried by each exchange.

## Main Types

This header defines:

```cpp
class Weighted_value_exchange
using Random_engine
using Minimization_function
```

The class is the per-edge exchange buffer. `Random_engine` is the pseudo-random
engine type used by minimizers that need randomized tie-breaking.
`Minimization_function` is the callable type stored by factors.

## State At A Glance

The private data members are:

```cpp
  Graph_edge edge_;
  Weighted_value weighted_value_;
```

`edge_` identifies which graph edge this exchange represents.
`weighted_value_` is the current message value and weight. It is read and then
overwritten during factor minimization.

## Code Walkthrough

### Include Guard

The include guard is:

```cpp
#ifndef TWALIB_GRAPH_WEIGHTED_VALUE_EXCHANGE_HPP
#define TWALIB_GRAPH_WEIGHTED_VALUE_EXCHANGE_HPP
```

It prevents duplicate processing of this header.

### Library Includes

The graph includes are:

```cpp
#include "twalib/graph/graph_edge.hpp"
#include "twalib/graph/weighted_value.hpp"
```

`Graph_edge` identifies the edge being exchanged. `Weighted_value` is the
message payload.

### Standard Includes

The standard includes are:

```cpp
#include <functional>
#include <random>
#include <span>
```

`<functional>` provides `std::function`, the type-erased callable wrapper used
for minimizers. `<random>` provides `std::mt19937_64`. `<span>` provides
`std::span`, a non-owning view of a contiguous sequence of exchanges.

### Namespace

The public namespace begins:

```cpp
namespace twalib {
```

Minimizers are part of the public API, so the exchange type must also be
public.

### `Weighted_value_exchange`

The class begins:

```cpp
class Weighted_value_exchange {
 public:
```

This is a small class rather than a raw struct because the edge handle is
fixed after construction while the weighted value is deliberately read and
written through named functions.

### Constructor

The constructor is:

```cpp
  explicit constexpr Weighted_value_exchange(Graph_edge edge) : edge_(edge) {}
```

`explicit` prevents accidental conversion from `Graph_edge` to
`Weighted_value_exchange`. `constexpr` allows compile-time construction when
the input is known. The initializer list stores the edge handle. The
`weighted_value_` member is default-constructed.

### `edge`

The edge accessor is:

```cpp
  [[nodiscard]] constexpr auto edge() const -> Graph_edge {
    return edge_;
  }
```

It returns the graph edge represented by this exchange. The return value is a
handle, so returning by value is cheap.

`[[nodiscard]]` says callers should use the result. `const` says the function
does not modify the exchange.

### `get`

Reading the weighted value uses:

```cpp
  [[nodiscard]] constexpr auto get() const -> Weighted_value {
    return weighted_value_;
  }
```

The minimizer calls this to inspect the incoming message for the edge. The
message is returned by value because `Weighted_value` is tiny.

### `set`

Writing the weighted value uses:

```cpp
  constexpr auto set(Weighted_value weighted_value) -> void {
    weighted_value_ = weighted_value;
  }
```

The minimizer calls this to record the outgoing message it wants the factor to
send on this edge.

The function returns `void` because its effect is the mutation of the exchange
object.

### Private Section

The private section begins:

```cpp
 private:
```

Only the class methods should directly mutate the stored state.

### Data Members

The data members are:

```cpp
  Graph_edge edge_;
  Weighted_value weighted_value_;
```

`edge_` is set by the constructor and not changed afterward. `weighted_value_`
starts with the default `Weighted_value` and is then set by graph internals and
minimizers during each factor pass.

The class closes with:

```cpp
};
```

### `Random_engine`

The random-engine alias is:

```cpp
using Random_engine = std::mt19937_64;
```

This gives the library one named random engine type. `std::mt19937_64` is a
deterministic 64-bit Mersenne Twister engine. The graph owns an engine of this
type and passes it to minimizers so seeded runs are reproducible.

### `Minimization_function`

The minimizer type alias is:

```cpp
using Minimization_function = std::function<void(std::span<Weighted_value_exchange>, Random_engine&)>;
```

`using` creates a type alias. This line says that a `Minimization_function` is
a `std::function` holding any callable with this signature:

```cpp
void(std::span<Weighted_value_exchange>, Random_engine&)
```

The first argument is the non-owning view of exchanges for the factor's enabled
edges. The second argument is the graph's random engine, passed by reference so
the minimizer can draw random numbers and advance the shared deterministic
sequence.

The callable returns `void` because it communicates its result by calling
`set()` on the exchanges.

### Closing The File

The namespace and include guard close with:

```cpp
} // namespace twalib

#endif
```

## Important Invariants

- One exchange corresponds to one edge during one factor minimization.
- The edge handle in an exchange is fixed for that exchange.
- Minimizers should use `get()` to read incoming values and `set()` to write
  outgoing values.
- Random choices should use the provided `Random_engine&`, not a separate
  unseeded generator, so runs remain reproducible.

## Relationship To The Paper

In the paper's notation, a factor minimization reads incoming leftward messages
and computes outgoing rightward messages. `Weighted_value_exchange` is the
temporary object that carries those values through a minimizer call.

## Extension Notes

Keep this type graph-agnostic. If a minimizer needs more context, prefer
capturing parameters in the minimizer lambda or function object rather than
giving `Weighted_value_exchange` access to graph internals.
