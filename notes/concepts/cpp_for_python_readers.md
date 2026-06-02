# C++ For Python Readers

This note explains C++ ideas that appear throughout this repository. It is written
for readers who may be comfortable with Python and with the mathematics of the
three-weight algorithm, but less comfortable reading modern C++.

The goal is not to teach all of C++. The goal is to make this repository's code
readable.

## C++ Is Compiled And Typed Earlier

Python usually checks names and types while the program is running. C++ checks
many more things while compiling the program.

When C++ sees:

```cpp
auto value(Variable_node variable) const -> double;
```

the compiler knows before the program runs that:

- the function is named `value`,
- it takes a `Variable_node`,
- it does not modify the object because of `const`,
- it returns a `double`.

This makes C++ more verbose than Python, but it also lets the compiler catch
many mistakes before the solver runs.

## Namespaces

Most public library code lives in the `twalib` namespace:

```cpp
namespace twalib {

class Factor_graph;

}  // namespace twalib
```

A namespace is similar to a Python module prefix. It keeps names organized and
prevents accidental collisions with other libraries.

Code outside the namespace refers to the class as:

```cpp
twalib::Factor_graph graph;
```

The `::` operator means "look inside this namespace or type."

## Header Files And Source Files

C++ commonly separates declarations from implementations.

Header files, usually `.hpp`, say what exists:

```cpp
class Factor_graph {
public:
  auto iterate() -> bool;
};
```

Source files, usually `.cpp`, define how those things work:

```cpp
auto Factor_graph::iterate() -> bool {
  return impl_->iterate();
}
```

The `#include` directive copies declarations from a header into the current
file during compilation:

```cpp
#include "twalib/graph/factor_graph.hpp"
```

This is different from Python `import`. A C++ include is mostly textual: it
makes names visible to the compiler for this translation unit.

## Values, References, And Pointers

C++ makes a sharp distinction between storing an object, referring to an object,
and pointing at an object.

### Values

This creates an actual object:

```cpp
Factor_graph graph;
```

The object has a lifetime. It is constructed when execution reaches this line
and destroyed automatically when it leaves scope.

### References

This accepts an existing object without copying it:

```cpp
auto add_to_factor_graph(Factor_graph& graph) -> void;
```

`Factor_graph& graph` means "`graph` is another name for an existing
`Factor_graph`." In Python terms, function arguments already behave somewhat
like references, but C++ makes this explicit.

In this repo, the style is:

```cpp
T& x
T* x
```

The `&` or `*` is visually attached to the type.

### Const References

This accepts an existing object and promises not to modify it:

```cpp
auto extract_state(const Factor_graph& graph) -> std::vector<int>;
```

`const Factor_graph& graph` is common for large objects that should not be
copied and should not be changed.

### Pointers

A pointer stores an address:

```cpp
Impl* impl;
```

Raw pointers are uncommon in the public API. When this repo needs
owning pointer behavior, it usually uses `std::unique_ptr`, described below.

## Scope And Lifetime

C++ destroys local objects automatically when they leave scope:

```cpp
{
  Factor_graph graph;
  graph.iterate();
}
```

After the closing brace, `graph` is destroyed.

This pattern is called RAII: Resource Acquisition Is Initialization. It means
that objects own resources, and cleanup happens automatically through object
lifetime. For example, a `std::vector` releases its memory when the vector is
destroyed.

Python also cleans up objects, but the timing is usually less central to how
code is written. In C++, lifetime is one of the main design tools.

## `const`

`const` means "this thing should not be modified through this name."

For a local variable:

```cpp
const double radius = circle.radius;
```

`radius` cannot be assigned a new value.

For a member function:

```cpp
auto iterations() const -> std::size_t;
```

the function promises not to modify the object it is called on.

For a reference:

```cpp
const Factor_graph& graph
```

the function receives an existing graph but cannot mutate it through this
reference.

This is one of the main ways C++ code communicates intent.

## `auto` And Trailing Return Types

This repo often writes functions like this:

```cpp
auto iterate() -> bool;
```

That means the same thing as:

```cpp
bool iterate();
```

The return type appears after `->`. This style keeps function names aligned and
works nicely with more complicated return types.

Inside a function, `auto` asks the compiler to infer the type:

```cpp
const auto edge = Graph_edge{edges_.size()};
```

The compiler knows that `Graph_edge{edges_.size()}` creates a `Graph_edge`, so
`edge` has type `Graph_edge`.

`auto` does not make C++ dynamically typed. The type is still fixed at compile
time.

## Braced Initialization

C++ has several ways to initialize objects. This repo often uses braces:

```cpp
Graph_edge edge{0};
Weighted_value value{1.0, Message_weight::standard};
```

Braces construct an object from the values inside. This is similar in spirit to
calling a Python constructor:

```python
edge = GraphEdge(0)
```

For simple structs, braces can initialize fields directly.

## Classes, Structs, And Access

C++ has both `class` and `struct`.

```cpp
struct Circle {
  double x = 0.0;
  double y = 0.0;
  double radius = 1.0;
};
```

Struct members are public by default.

```cpp
class Factor_graph {
public:
  auto iterate() -> bool;

private:
  std::unique_ptr<Impl> impl_;
};
```

Class members are private by default. `public:` marks the API callers can use.
`private:` marks implementation details.

Member variables in this repo often end with `_`, as in `impl_`, to distinguish
them from local variables and parameters.

## Constructors

A constructor initializes an object:

```cpp
explicit constexpr Weighted_value_exchange(Graph_edge edge) : edge_(edge) {}
```

The part after `:` is the member initializer list. It initializes fields before
the constructor body runs.

`explicit` prevents accidental implicit conversions. If a constructor is
explicit, C++ will not silently turn a `Graph_edge` into a
`Weighted_value_exchange` in places where that would be surprising.

## Handles

The graph API uses small handle types such as:

```cpp
Variable_node
Factor_node
Graph_edge
```

These are lightweight identifiers. They are not the full internal object. They
are closer to an index wrapped in a named type.

This is safer than passing plain integers everywhere. A function that expects a
`Variable_node` cannot accidentally receive a `Factor_node`.

## `enum class`

An enum class defines a closed set of named values:

```cpp
enum class Message_weight {
  zero,
  standard,
  infinite,
};
```

Using it looks like:

```cpp
Message_weight::standard
```

This is more explicit than a Python string such as `"standard"`, and the
compiler can catch misspellings.

## Standard Library Containers

The C++ standard library provides reusable containers. The most common one in
this repo is `std::vector`.

```cpp
std::vector<Graph_edge> edges_;
```

A vector is similar to a Python list in that it stores a sequence of values. It
is different in important ways:

- all elements have the same type,
- indexing does not check bounds with `operator[]`,
- memory layout is contiguous,
- pushing more elements may move the stored objects to new memory.

`std::unordered_map` is similar to a Python dictionary:

```cpp
std::unordered_map<std::size_t, std::size_t> givens;
```

It maps keys to values.

## `std::span`

`std::span<T>` is a non-owning view of a contiguous sequence.

```cpp
auto minimize(std::span<Weighted_value_exchange> exchanges) -> void;
```

A span does not store the elements. It stores a pointer to the first element and
a length. It can view elements owned by a `std::vector`, an array, or another
contiguous container.

This is useful when a function should work with a sequence but should not own
or copy that sequence.

Python analogy: a span is somewhat like passing a view into a list or NumPy
array. It is not the owner.

The lifetime rule matters: the original data must outlive the span.

## `std::optional`

`std::optional<T>` means "there may or may not be a `T` here."

```cpp
std::optional<Weighted_value> value;
```

This is similar to using either a value or `None` in Python, but the possible
absence is written into the type.

You can check it like this:

```cpp
if (value.has_value()) {
  const Weighted_value actual = *value;
}
```

This repo also uses a compact form:

```cpp
if (const auto result = value_function(incoming); result.has_value()) {
  exchanges[0].set(*result);
}
```

That creates `result`, checks whether it contains a value, and keeps `result`
available inside the `if` body.

## `std::function`

`std::function` stores something callable.

```cpp
using Minimization_function =
    std::function<void(std::span<Weighted_value_exchange>, Random_engine&)>;
```

This says a `Minimization_function` can hold any callable object that:

- takes a mutable span of `Weighted_value_exchange`,
- takes a random engine by reference,
- returns `void`.

The callable might be a normal function, a lambda, or another function-like
object.

This is how factors store their minimizers. The graph does not need to know the
exact C++ type of every minimizer; it only needs to know how to call it.

## Lambdas

A lambda is an inline function object.

```cpp
[value](std::span<Weighted_value_exchange> exchanges, Random_engine&) {
  exchanges[0].set(Weighted_value{value, Message_weight::infinite});
}
```

The syntax has three main parts:

```cpp
[capture](parameters) {
  body
}
```

The capture list says which outside variables the lambda keeps.

```cpp
[value]
```

means the lambda stores its own copy of the outside variable named `value`.

```cpp
[&random]
```

would capture `random` by reference, meaning the lambda refers to the original
object.

```cpp
[]
```

means the lambda captures nothing.

Lambdas are common in this repo because minimizers are small pieces of behavior
attached to factors.

## `using`

`using` creates an alias:

```cpp
using Random_engine = std::mt19937_64;
```

After this, `Random_engine` means `std::mt19937_64`.

Aliases make code easier to read when the underlying type is long or when the
project wants a domain name for a standard-library type.

## `std::unique_ptr`

`std::unique_ptr<T>` owns a dynamically allocated `T`.

```cpp
std::unique_ptr<Impl> impl_;
```

There is exactly one owner. When the `unique_ptr` is destroyed, the object it
owns is destroyed too.

This repo uses `std::unique_ptr<Impl>` in `Factor_graph` to hide implementation
details from the public header. The public class owns its private implementation,
but callers do not need to see the vectors and helper functions inside it.

Python analogy: this is not like normal Python assignment, where many names can
refer to the same object freely. A `unique_ptr` makes ownership explicit and
move-only.

## Copying, Moving, And `= delete`

C++ objects can often be copied:

```cpp
auto b = a;
```

But sometimes copying should not be allowed. `Factor_graph` owns a unique
implementation object, so it disables copying:

```cpp
Factor_graph(const Factor_graph&) = delete;
auto operator=(const Factor_graph&) -> Factor_graph& = delete;
```

`= delete` means "this operation is intentionally unavailable."

The graph can still be moved:

```cpp
Factor_graph(Factor_graph&&) noexcept;
auto operator=(Factor_graph&&) noexcept -> Factor_graph&;
```

Moving transfers resources from one object to another. After a move, the source
object remains destructible but should not be treated as if it still has its old
contents.

## `noexcept`

`noexcept` says a function is not expected to throw exceptions:

```cpp
Factor_graph(Factor_graph&&) noexcept;
```

This matters for performance and correctness in some standard-library
containers and algorithms. For example, vectors can move elements more
confidently when moving cannot throw.

## `constexpr`

`constexpr` means a value or function can be evaluated at compile time when its
inputs are known:

```cpp
static constexpr auto invalid_index = std::numeric_limits<std::size_t>::max();
```

The value still exists in ordinary runtime code too. The important point is
that the compiler is allowed to compute it early.

## `[[nodiscard]]`

`[[nodiscard]]` asks the compiler to warn if a caller ignores a return value:

```cpp
[[nodiscard]] auto converged() const -> bool;
```

This is useful for functions whose result is the point of calling them.

Ignoring `graph.converged()` is probably a mistake, so the annotation gives the
compiler a chance to help.

## Ranges And Algorithms

C++ has standard algorithms such as `std::ranges::find_if` and
`std::ranges::count_if`.

```cpp
const auto found = std::ranges::find_if(edges, [edge](Graph_edge candidate) {
  return candidate == edge;
});
```

This is similar to using a Python generator expression with `next`, but the C++
version is typed and works with iterators.

The lambda supplies the test.

## Range-Based `for`

C++ can loop over containers directly:

```cpp
for (const Graph_edge edge : edges_) {
  // use edge
}
```

This is similar to:

```python
for edge in edges:
    ...
```

The `const Graph_edge edge` form copies each edge into a local variable and
prevents modifying that local copy. For larger objects, code often uses
`const auto& item` to avoid copying.

## Structured Bindings

Structured bindings unpack tuple-like values:

```cpp
for (const auto& [cell, value] : givens) {
  // use cell and value
}
```

This is similar to Python:

```python
for cell, value in givens.items():
    ...
```

The `&` means the loop is referring to the map entry instead of copying it.

## Exceptions

This repo uses exceptions for invalid inputs:

```cpp
throw std::invalid_argument{"inner_side must be positive"};
```

An exception interrupts normal control flow until it is caught:

```cpp
try {
  run(options);
} catch (const std::exception& exception) {
  std::cerr << exception.what() << '\n';
}
```

This is similar to Python `raise` and `try` / `except`.

## Casts

C++ does not freely convert all numeric types. When a conversion should be
explicit, this repo uses casts such as:

```cpp
const auto root = static_cast<std::size_t>(std::sqrt(static_cast<double>(value)));
```

`static_cast<T>(x)` says "convert `x` to type `T` using a normal checked-at-
compile-time conversion."

Explicit casts make narrowing, signedness, and floating-point conversions
visible to the reader.

## C++ Standard Library Names

Names beginning with `std::` come from the C++ standard library:

```cpp
std::vector
std::span
std::optional
std::function
std::size_t
```

`std::size_t` is an unsigned integer type used for sizes and indexes.

## Template Syntax

Angle brackets provide type parameters:

```cpp
std::vector<Variable_node>
std::optional<Weighted_value>
std::span<const Graph_edge>
```

This is similar to type annotations in Python such as:

```python
list[VariableNode]
Optional[WeightedValue]
```

but in C++ the compiler uses the type parameters to generate and check real
code.

## `const` Inside Template Arguments

This type:

```cpp
std::span<const Graph_edge>
```

means "a span of `Graph_edge` values that cannot be modified through this
span."

The span object itself can be reassigned, but the elements it views are
read-only through this handle.

## Member Access: `.` And `->`

C++ uses `.` for direct objects:

```cpp
circle.radius
```

and `->` for pointers or pointer-like objects:

```cpp
impl_->iterate()
```

`impl_->iterate()` is shorthand for:

```cpp
(*impl_).iterate()
```

That means "follow the pointer-like object, then call `iterate` on the object
it owns or points to."

## `this`

Inside a member function, `this` points to the object whose function is running.
Most code in this repo does not need to write `this` explicitly.

For example:

```cpp
auto Factor_graph::iterations() const -> std::size_t {
  return impl_->iterations();
}
```

The function is called on a `Factor_graph`, and `impl_` means the `impl_` member
of that particular object.

## Private Implementation Pattern

Some public classes hide their implementation behind a private nested type:

```cpp
class Factor_graph {
private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
```

The public header says that an `Impl` class exists, but does not show its
fields. The `.cpp` file defines it:

```cpp
class Factor_graph::Impl {
  // internal storage and helper functions
};
```

This does not mean `Factor_graph` inherits from `Impl`. It means `Impl` is a
class nested inside the scope of `Factor_graph`, and each `Factor_graph` owns
one `Impl`.

The benefit is that internal storage can change without exposing those details
to every file that includes the public header.

## Internal Detail Namespace

Some implementation types live in `twalib::detail`.

```cpp
namespace twalib::detail {
class Edge_data;
}
```

`detail` is a convention meaning "not part of the main public API." Tests may
inspect these types, and implementation files may use them, but ordinary users
of the library should prefer the public API.

## Algorithms As Objects

A key design idea in this repo is that minimizers are values that can be stored
and called later.

For example, a factor stores a `Minimization_function`. During an iteration,
the graph gives that function a span of exchanges and a random engine. The
function reads incoming weighted values and writes outgoing weighted values.

This is a common C++ pattern: behavior can be represented by objects, not only
by named functions.

## Reading A Declaration

Consider:

```cpp
[[nodiscard]] auto create_factor(
    std::span<const Graph_edge> edges,
    Minimization_function minimization_function) -> Factor_node;
```

Read it in pieces:

- `[[nodiscard]]`: callers should use the returned factor handle,
- `auto ... -> Factor_node`: the function returns a `Factor_node`,
- `create_factor`: the function name,
- `std::span<const Graph_edge> edges`: a read-only view of edge handles,
- `Minimization_function minimization_function`: callable factor behavior.

In Python-like pseudocode, the declaration says:

```python
def create_factor(edges: Sequence[GraphEdge],
                  minimization_function: Callable[..., None]) -> FactorNode:
    ...
```

The C++ version is more explicit about ownership, mutability, and exact types.

## Reading An Implementation

Consider:

```cpp
auto Factor_graph::iterate() -> bool {
  return impl_->iterate();
}
```

Read it as:

- this defines the `iterate` member function of `Factor_graph`,
- it returns `bool`,
- it delegates the actual work to the private implementation object,
- `impl_->iterate()` calls `iterate` on the object owned by `impl_`.

Many public `Factor_graph` methods are thin forwarding functions like this.
The algorithmic details live in the private implementation.

## Reading A Lambda Minimizer

Consider:

```cpp
return graph.create_factor({edge}, [value](
    std::span<Weighted_value_exchange> exchanges,
    Random_engine&) {
  exchanges[0].set(Weighted_value{value, Message_weight::infinite});
});
```

Read it as:

- create a factor connected to one edge,
- attach a lambda as the factor's minimizer,
- copy `value` into the lambda,
- accept the exchanges span and the random engine when the minimizer runs,
- ignore the random engine because this minimizer is deterministic,
- set the only outgoing exchange to the known value with infinite weight.

This is typical of the codebase: graph construction creates variables, edges,
and factors; iteration later calls the stored minimizer functions.

## When To Look Here From File Notes

File-associated notes should link back to this concept note when they encounter
syntax that would otherwise interrupt the explanation. Good link targets
include:

- "see `std::span` in [`notes/concepts/cpp_for_python_readers.md`](cpp_for_python_readers.md),"
- "see lambdas in [`notes/concepts/cpp_for_python_readers.md`](cpp_for_python_readers.md),"
- "see private implementation pattern in
  [`notes/concepts/cpp_for_python_readers.md`](cpp_for_python_readers.md)."

Those file notes should still explain what the local code is doing. This note
just keeps repeated C++ background from overwhelming every walkthrough.
