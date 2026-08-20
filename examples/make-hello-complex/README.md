# Clang and Make module graph

This example builds a small but deliberately nontrivial module graph. It includes
a primary interface, two exported partitions, an internal partition, a separate
dotted module, a module implementation unit, nested source directories, a
diamond-shaped dependency, and libc++'s external `std` module.

Requirements:

- Clang with libc++ standard-library modules;
- GNU Make; and
- a built `cxx-modgraph` executable.

From the repository root:

```sh
cmake -S . -B build
cmake --build build
make -C examples/make-hello-complex
./examples/make-hello-complex/hello
```

The expected output is:

```text
[demo] Hello, module graph!
```

The Makefile includes the reusable
[`clang.mk`](../../adapters/make/clang.mk) adapter. The adapter asks Clang for the
location of `libc++.modules.json`, derives the adjacent `std.cppm` location, and
provides the BMI and object compilation recipes. Override the source when a
toolchain uses a different layout:

```sh
make CXX_MODGRAPH_STD_SOURCE=/path/to/libc++/std.cppm
```

The project explicitly lists the ordinary translation units in `src` and declares
`modules` as a local module path. Recursive discovery finds all module interfaces,
including `modules/greeting/detail/punctuation.cppm`. Clang determines which
logical module or partition each file provides; the adapter never infers a module
name from its filename.

The scanned dependency graph is approximately:

```text
std ─> support.logging ────────────┐
std ─> greeting:punctuation ───────┼─> greeting:message ─> greeting ─┬─> main
std ─> greeting:audience ──────────┘              │                  └─> implementation
               └──────────────────────────────────┘
```

The direct `greeting:audience` import and its indirect path through
`greeting:message` produce the diamond. The internal
`greeting:punctuation` partition is intentionally stored another directory deeper
to exercise recursive discovery.

The adapter registers `std` as externally managed because it owns that BMI recipe.
`cxx-modgraph` imports the P1689 scan and generates `build/modules.mk`, which
contains prerequisites and target metadata but no compiler commands. Partition
imports are emitted as explicit logical-name-to-BMI mappings, so compilation does
not rely on partition filename conventions. The mappings include transitive
imports needed while Clang loads another BMI, while the build prerequisites remain
the direct edges reported by P1689.
