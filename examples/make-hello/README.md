# Clang and Make hello world

This example builds a `helloworld` module that imports libc++'s `std` module. A
small executable imports `helloworld` and calls its exported function.

Requirements:

- Clang with libc++ standard-library modules;
- GNU Make; and
- a built `cxx-modgraph` executable.

From the repository root:

```sh
cmake -S . -B build
cmake --build build
make -C examples/make-hello
./examples/make-hello/hello
```

The expected output is:

```text
Hello, world!
```

The Makefile includes the reusable
[`clang.mk`](../../adapters/make/clang.mk) adapter. The adapter asks Clang for the
location of `libc++.modules.json`, derives the adjacent `std.cppm` location, and
provides the BMI and object compilation recipes. Override the source when a
toolchain uses a different layout:

```sh
make CXX_MODGRAPH_STD_SOURCE=/path/to/libc++/std.cppm
```

The project lists `src/main.cpp` and declares `modules` as a local module path. The
adapter discovers `modules/helloworld.cppm` by its interface extension, generates
`build/compile_commands.json`, and runs `clang-scan-deps` to determine that the
file provides `helloworld` and imports `std`. It does not infer a module name from
the filename.

The adapter registers `std` as externally managed because it owns that BMI recipe.
`cxx-modgraph` imports the P1689 scan and generates `build/modules.mk`, which
contains prerequisites and target metadata but no compiler commands.
