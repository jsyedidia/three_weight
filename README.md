# Three-Weight Algorithm In C++

This repository is a C++ implementation of the Three-Weight Algorithm (TWA) as
a message-passing algorithm on factor graphs. The C++ library and namespace use
the name `twalib` for "Three-Weight Algorithm Library."

It is an MIT-licensed port of Nate Derbinsky's SwiftADMM library 
(https://github.com/natederbinsky/SwiftADMM) from Swift to C++. 

The SwiftADMM library implements both the TWA algorithm and 
the original ADMM algorithm that TWA improves on, but for simplicity 
this port only implements the TWA algorithm. See https://arxiv.org/abs/1305.1961 for a 
detailed description of the algorithms. 

## Layout

- `include/twalib/`: public headers
- `src/`: library implementation
- `apps/`: executables
- `data/`: input data files for the executables
- `tests/`: regression tests
- `docs/`: documentation for the GUI
- `notes/`: detailed explanations of the codebase

## Install, build, test, and run with Pixi

This repository is developed and tested on macOS. Linux support is expected through the Pixi environment 
but has not yet been tested. Pixi provides the C++23 compiler toolchain, CMake, Ninja, GLFW, and ImGui; 
you only need to install Pixi itself.

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

## Documentation

The `notes/` directory contains detailed prose explanations of the algorithm
and codebase, including concept guides and file-by-file walkthroughs. 
See `notes/README.md` for a suggested reading order and orientation.
