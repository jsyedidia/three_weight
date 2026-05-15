# Three-Weight Algorithm In C++

This repository is a C++ implementation of the Three-Weight Algorithm (TWA) as
a message-passing algorithm on factor graphs. The repository is named
`three_weight`; the C++ library and namespace use the shorter name `twalib`
for “Three-Weight Algorithm library.”

It is an MIT-licensed port of Nate Derbinsky's SwiftADMM library 
(https://github.com/natederbinsky/SwiftADMM) from Swift to C++. 

The original SwiftADMM library implements both the TWA algorithm and 
the original ADMM algorithm that TWA improves on, but for simplicity 
this port only implements the TWA algorithm; it does not implement 
the original ADMM algorithm. See https://arxiv.org/abs/1305.1961 for a 
detailed description of the algorithms. 

The port was produced with substantial assistance from GPT-5.5. 
The resulting code has been reviewed, tested, and maintained by Jonathan Yedidia.

## Layout

- `include/twalib/`: public headers
- `src/`: library implementation
- `apps/`: executables
- `data/`: input data files for the executables
- `docs/`: documentation
- `tests/`: regression tests

## Install, build, test, and run with Pixi

This repository is developed and tested on macOS. Linux support is expected through the Pixi environment but has not yet been tested. Pixi provides the C++23 compiler toolchain, CMake, Ninja, GLFW, and ImGui; you only need to install Pixi itself.

If you do not have Pixi installed, you can install it with:

```sh
curl -fsSL https://pixi.sh/install.sh | sh
```

Then, from the repository root, you can install dependencies, configure, build and test with:

```sh
pixi run test
```

Run the Sudoku command-line app with:

```sh
pixi run sudoku
```

Use `--help` to see command-line options:

```sh
pixi run sudoku --help
```

Run the GUI app with:

```sh
pixi run gui
```

See `docs/gui_help.md` for instructions on using the GUI.
