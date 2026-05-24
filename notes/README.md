# Notes

This directory contains prose explanations of the twalib codebase — the C++23
implementation of the Three-Weight Algorithm for message-passing on factor
graphs.

## Who These Notes Are For

The primary audience is a programmer or researcher who wants to understand
how the Three-Weight Algorithm works and how this codebase implements it. You
do not need deep C++ experience: there is a dedicated concept note that
explains the C++ features this code uses, written for readers who are more
comfortable in Python.

## Directory Structure

The notes are organized into two categories:

- **`concepts/`** — Standalone explanations of ideas that span multiple source
  files. These cover the algorithm itself, the role of weights, and relevant
  C++ idioms.
- **`src/`and `include/twalib/`** — File-by-file walkthroughs that mirror the source tree. 
  Each note covers one `.hpp` or `.cpp` file in source order.

Source notes are named after the file they document. For example,
`notes/src/graph/factor_graph.cpp.md` documents `src/graph/factor_graph.cpp`.

## Suggested Reading Order

The notes are designed to be read roughly in this order. You can skip ahead if
a topic is already familiar.

### 1. Foundations

Start here to build the mental model you need for everything else.

1. **`concepts/cpp_for_python_readers.md`** — C++ syntax and idioms used in
   this codebase (references, spans, optionals, lambdas, RAII, etc.). Skip if
   you are already comfortable with modern C++.
2. **`concepts/factor_graphs.md`** — What a factor graph is: variables,
   factors, and edges.
3. **`concepts/message_passing.md`** — How message-passing works on a factor
   graph, and what it means for factors and variables to exchange messages.
4. **`concepts/weights.md`** — The three weights (zero, standard, infinite)
   and how they let the algorithm express certainty, indifference, and
   ordinary opinions.

### 2. Core Graph Internals

These notes cover the internal data structures that store the graph. Read them
in this order because each builds on the previous one.

5. **`src/graph/edge_data.hpp.md`** — The per-edge storage: how a single edge
   holds messages traveling in both directions.
6. **`src/graph/variable_data.hpp.md`** — The per-variable storage: how a
   variable aggregates messages from its edges.
7. **`src/graph/factor_data.hpp.md`** — The per-factor storage: how a factor
   holds its edges and its minimization function.
8. **`src/graph/factor_graph.cpp.md`** — The central `Factor_graph::Impl`
   class that owns all the data and runs the algorithm.

### 3. Public API Headers

These notes cover the thin public types that users interact with. They are
lightweight handles and value types.

9. **`include/twalib/graph/variable_node.hpp.md`** — The `Variable_node`
   handle.
10. **`include/twalib/graph/factor_node.hpp.md`** — The `Factor_node` handle.
11. **`include/twalib/graph/graph_edge.hpp.md`** — The `Graph_edge` handle.
12. **`include/twalib/graph/weighted_value.hpp.md`** — The `Weighted_value`
    struct and `Message_weight` enum.
13. **`include/twalib/graph/weighted_value_exchange.hpp.md`** — The exchange
    buffer that minimizers read from and write to.
14. **`include/twalib/graph/factor_graph.hpp.md`** — The public
    `Factor_graph` interface (PImpl front end).

### 4. Minimizers

Minimizers are the "brains" of factors — they decide what messages to send.

15. **`src/minimizers/one_hot.cpp.md`** — The one-hot (simplex projection)
    minimizer: pick exactly one variable to be 1.
16. **`include/twalib/minimizers/one_hot.hpp.md`** — Its public header.
17. **`src/minimizers/in_range.cpp.md`** — The range constraint minimizer.
18. **`include/twalib/minimizers/in_range.hpp.md`** — Its public header.
19. **`src/minimizers/known_value.cpp.md`** — The known-value minimizer.
20. **`include/twalib/minimizers/known_value.hpp.md`** — Its public header.
21. **`src/minimizers/spy.cpp.md`** — The spy minimizer.
22. **`include/twalib/minimizers/spy.hpp.md`** — Its public header.

### 5. Problem Builders

Problem builders assemble a factor graph for a specific problem domain.

23. **`concepts/sudoku.md`** — How Sudoku maps onto a factor graph.
24. **`src/problems/sudoku.cpp.md`** — The basic Sudoku builder.
25. **`include/twalib/problems/sudoku.hpp.md`** — Its public header.
26. **`src/problems/compact_sudoku.cpp.md`** — The compact Sudoku builder
    (eliminates variables using givens).
27. **`include/twalib/problems/compact_sudoku.hpp.md`** — Its public header.
28. **`concepts/circle_packing.md`** — How circle packing maps onto a factor
    graph.
29. **`src/problems/circle_packing.cpp.md`** — The circle-packing builder.
30. **`include/twalib/problems/circle_packing.hpp.md`** — Its public header.

## Tips For Reading

- **Read the concept notes before the source notes.** The source notes assume
  you understand factor graphs, message-passing, and the three-weight system.
  Without that context the code walkthrough will feel arbitrary.

- **Follow cross-references.** When a source note says "see
  `concepts/weights.md`", it means the explanation there is important for
  understanding the current passage.

- **Source notes cover every non-blank line of code.** They proceed in the
  same order as the source file. You can use the Markdown headings to jump to
  a specific function or section without reading the whole note linearly.
  Because all of the code is covered in the notes, you don't really need to keep
  the source code files open as you read the notes. On the other hand, keeping
  the source files open may help orient you.

## The Paper

The algorithm is described in:

> N. Derbinsky, J. Bento, V. Elser, J.S. Yedidia, "An Improved Three-Weight Message-Passing Algorithm,"
> https://arxiv.org/abs/1305.1961

The source notes reference specific sections of the paper where relevant.
