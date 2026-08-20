# cxx-modgraph

[![CI](https://github.com/Elizafox/cxx-modgraph/actions/workflows/ci.yml/badge.svg)](https://github.com/Elizafox/cxx-modgraph/actions/workflows/ci.yml)

`cxx-modgraph` consumes C++ module dependency facts, validates them as a directed
acyclic graph, and emits data suitable for build systems.

This project primarily focusses on:

- Deterministic dependency processing.
- Explicit, reproducible source-path resolution.
- Compiler-neutral dependency facts.
- Make and JSON output formats.
- Making the process of using C++ modules much more frictionless.

## Building

A C++20-or-newer compiler is required.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

An opt-in host integration test builds libc++'s `std` and `std.compat` modules,
then compiles and runs a consumer when the configured Clang toolchain exposes the
libc++ module manifest:

```sh
cmake -S . -B build -DCXX_MODGRAPH_BUILD_HOST_INTEGRATION_TESTS=ON
cmake --build build
ctest --test-dir build -R host-std-modules --output-on-failure
```

The test is reported as skipped when the configured toolchain cannot supply those
modules.

Two end-to-end GNU Make and Clang projects demonstrate the adapter:

- [`make-hello-simple`](examples/make-hello-simple) builds one custom module and
  an importing executable.
- [`make-hello-complex`](examples/make-hello-complex) builds a nested module graph
  with exported and internal partitions, a dotted module, an implementation unit,
  and a diamond dependency.
- [`make-hello-gcc`](examples/make-hello-gcc) uses GCC 16, per-source P1689 scans,
  `.gcm` outputs, generated module-mapper files, and automatic libstdc++ `std`
  module construction.

The graph library uses a deterministic implementation of Kahn's algorithm and
exposes both a topological order and parallel build levels.

## Compiler scans

P1689 dependency data can be joined with the compilation database that produced
it and emitted directly as Make facts:

```sh
clang-scan-deps -format=p1689 \
  -compilation-database=build/compile_commands.json \
  -o build/dependencies.p1689.json

cxx-modgraph \
  --input build/dependencies.p1689.json \
  --input-format p1689 \
  --compdb build/compile_commands.json \
  --bmi-dir build/bmi \
  --external-module std=build/bmi/std.pcm \
  --emit make \
  --output build/modules.mk
```

P1689 supplies the module relationships and primary outputs. The compilation
database supplies source paths for translation units, which P1689 does not record
for non-provider units. The Clang Make adapter automates this entire pipeline from
`CXX_MODGRAPH_SOURCES`. It can also discover local interface candidates from
explicit directories:

```make
CXX_MODGRAPH_SOURCES := src/main.cpp
CXX_MODGRAPH_MODULE_PATHS := modules vendor/modules
```

Files recursively beneath those directories with `.cppm`, `.ixx`, or `.mpp`
extensions are added to the scan. Their object paths retain the source directory
layout beneath the object directory, so equal basenames in different directories
do not collide. Clang determines which module each file provides, not the
filename. Projects can override `CXX_MODGRAPH_MODULE_EXTENSIONS`.

GCC emits one P1689 document per source. Repeating `--input` combines such
documents into one graph, and `--bmi-extension .gcm` selects GCC-style CMI names.
The reusable [`gcc.mk`](adapters/make/gcc.mk) adapter automates those scans for GCC
16, emits module-mapper files, and uses GNU Make grouped targets so GCC produces a
module interface's `.gcm` and object together in one race-free compilation.
Set `CXX_MODGRAPH_USE_LIBSTDCXX_STD := 1` to build GCC 16's `std` and `std.compat`
modules automatically with `--compile-std-module` and link their objects.

Projects can include [`cxx-modgraph.mk`](adapters/make/cxx-modgraph.mk) instead of
selecting an adapter directly. It probes `$(CXX)` and includes `clang.mk` or
`gcc.mk`; unsupported compilers stop with a Make error. Set
`CXX_MODGRAPH_COMPILER := clang` or `gcc` to override detection, which is useful
for compiler wrappers that do not forward preprocessing probes.

Module partitions are represented by their P1689 logical names (for example,
`hello:detail`). Generated BMI filenames use reversible hexadecimal escape
encoding (`hello@3Adetail.pcm`), and the Make metadata records imports as
explicit `name=path` mappings. The Clang adapter therefore does not depend on
compiler filename conventions to locate partitions. The mappings include the
transitive import closure because Clang may need to load a partition's
dependencies while reading its BMI; emitted Make prerequisites intentionally
preserve only the direct P1689 edges.

## Canonical dependency facts

The version 1 format records a source root, translation-unit source and object
paths, provided modules and their BMI paths, required module names, and external
prebuilt modules. Paths are normalized and serialized with `/` separators. Arrays
are sorted by stable keys when emitted so equivalent facts produce identical JSON.

See [`docs/canonical-facts.schema.json`](docs/canonical-facts.schema.json) for the
machine-readable schema. Relative paths are interpreted relative to `source-root`;
the model does not implicitly consult the process environment or search the file
system.

Normalize and validate a facts file with:

```sh
cxx-modgraph --input dependencies.json --emit json --output normalized.json
```

An explicit build root can override the value stored in an input produced in a
different location:

```sh
cxx-modgraph --input dependencies.json --source-root "$PWD" --emit json
```

Emit a recipe-free Make fragment with BMI and object prerequisites:

```sh
cxx-modgraph --input dependencies.json --source-root "$PWD" --emit make --output build/modules.mk
```

The fragment defines `CXX_MODGRAPH_BMI_TARGETS`, `CXX_MODGRAPH_OBJECT_TARGETS`,
and `CXX_MODGRAPH_OUTPUT_GROUPS`, along with target-specific source, provided
module, imported-module, and BMI metadata. Output groups associate a module
interface's BMI and object for compilers such as GCC that create both in one
invocation. The consuming Makefile remains responsible for compiler-specific
recipes, either directly or through a reusable adapter such as
[`clang.mk`](adapters/make/clang.mk) or [`gcc.mk`](adapters/make/gcc.mk).

## Contributing

Contributors must follow the [contributor's guide](CONTRIBUTING.md).

## License

`cxx-modgraph` is available under the [0BSD license](LICENSE):

```text
SPDX-License-Identifier: 0BSD
```

The build prefers a system installation of
[`nlohmann/json`](https://github.com/nlohmann/json) and falls back to the bundled
version 3.12.0 for offline builds. The bundled dependency retains its upstream
MIT license in [`vendor/nlohmann-json/LICENSE.MIT`](vendor/nlohmann-json/LICENSE.MIT).
