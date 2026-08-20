# Clang and Make hello world

This minimal example builds one `helloworld` module that imports libc++'s `std`
module. A small executable imports `helloworld` and calls its exported function.

Requirements:

- Clang with libc++ standard-library modules;
- GNU Make; and
- a built `cxx-modgraph` executable.

From the repository root:

```sh
cmake -S . -B build
cmake --build build
make -C examples/make-hello-simple
./examples/make-hello-simple/hello
```

The expected output is:

```text
Hello, world!
```

The Makefile lists `src/main.cpp` and declares `modules` as a local module path.
The auto-detecting [`cxx-modgraph.mk`](../../adapters/make/cxx-modgraph.mk) entry
point selects the Clang adapter, which discovers
`modules/helloworld.cppm`, generates a compilation database, asks
`clang-scan-deps` for P1689 dependency data, and invokes `cxx-modgraph` to emit
`build/modules.mk`. Clang determines the logical module name from the source; the
adapter does not infer it from the filename.

The adapter registers `std` as externally managed because it owns the standard
module's BMI recipe. Override its source for a toolchain with a different libc++
layout:

```sh
make CXX_MODGRAPH_STD_SOURCE=/path/to/libc++/std.cppm
```
