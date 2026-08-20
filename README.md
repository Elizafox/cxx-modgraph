# cxx-modgraph

[![CI](https://github.com/Elizafox/cxx-modgraph/actions/workflows/ci.yml/badge.svg)](https://github.com/Elizafox/cxx-modgraph/actions/workflows/ci.yml)

`cxx-modgraph` consumes C++ module dependency facts, validates them as a directed
acyclic graph, and emits data suitable for build systems.

This project primarily focusses on:

- Deterministic dependency processing.
- Explicit, reproducible source-path resolution.
- Compiler-neutral dependency facts.
- Make, Ninja, and JSON output formats.
- P2977R1 standalone build databases for independent tooling.
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

The [`ninja-hello-simple`](examples/ninja-hello-simple) example demonstrates the
Ninja adapter. Generate its module fragment with `--emit ninja`, then use
the phony `cxx_modgraph_outputs` target or depend on the emitted object paths
directly. The reusable [`clang.ninja`](adapters/ninja/clang.ninja) file defines
the Clang compiler rules referenced by the generated fragment. GCC 16 projects
can include [`gcc.ninja`](adapters/ninja/gcc.ninja) instead; it writes module
mappers and preserves the object produced alongside each `.gcm`.

[`cxx-modgraph.ninja`](adapters/ninja/cxx-modgraph.ninja) is the generic selector.
Set `cxx_modgraph_compiler = clang` or `gcc` before including it. Unlike GNU Make,
Ninja cannot execute a compiler probe while parsing its input, so selection is
explicit:

```ninja
cxx_modgraph_compiler = gcc
cxx_modgraph_adapter_directory = adapters/ninja
include adapters/ninja/cxx-modgraph.ninja
include build/modules.ninja
```

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

Compilation arguments and working directories are retained during the join. A
complete database for language servers, analysers, indexers, and IDEs can be
emitted with `--emit p2977`.

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

Cross-compilers are supported by setting `CXX` and putting the target and sysroot
options in `CXX_MODGRAPH_CXXFLAGS`. Those flags are used for compiler detection,
dependency scanning, standard-library module discovery and compilation, and
project compilation. Keep BMI, object, scan, and rules directories separate for
each target because compiler module files are target- and option-specific. The
`cxx-modgraph` and dependency-scanner executables themselves run on the build
host; generated programs are never executed by the adapters.

```make
CXX := x86_64-w64-mingw32-g++
CXX_MODGRAPH_CXXFLAGS := -std=c++23 -fmodules
CXX_MODGRAPH_BMI_DIRECTORY := build/mingw64/bmi
CXX_MODGRAPH_OBJECT_DIRECTORY := build/mingw64/obj
CXX_MODGRAPH_SCAN_DIRECTORY := build/mingw64/scan
CXX_MODGRAPH_RULES := build/mingw64/modules.mk
```

The Ninja adapters expose the equivalent `cxx`, `cxxflags`, and (for Clang)
`bmi_directory` variables. Override them after including the adapter and before
including the emitted graph fragment.

Module partitions are represented by their P1689 logical names (for example,
`hello:detail`). Generated BMI filenames use reversible hexadecimal escape
encoding (`hello@3Adetail.pcm`), and the Make metadata records imports as
explicit `name=path` mappings. The Clang adapter therefore does not depend on
compiler filename conventions to locate partitions. The mappings include the
transitive import closure because Clang may need to load a partition's
dependencies while reading its BMI; emitted Make prerequisites intentionally
preserve only the direct P1689 edges.

## Installed module sources

Package discovery remains outside the graph core. Package managers, `pkgconf`,
or project configuration locate manifests and pass them using repeatable
`--package-metadata` options. Relative paths are resolved against the manifest;
installed and in-tree providers are then validated as one graph. Version 1
metadata names a `package` and contains `modules` with `logical-name`, `source`,
`arguments`, optional `requires`, and an optional `compatible-bmi`. Package
`dependencies`, `baseline-arguments`, local arguments, working directories,
module-set membership, and private visibility are also retained.

The compatible BMI is only an optimization: interface source and recipe remain
authoritative because BMIs are compiler- and configuration-specific.

## BMI compatibility and cache metadata

Canonical facts version 4 assigns each translation and external BMI an explicit
`bmi-compatibility` scope: compiler executable and version, target triple,
sysroot, language standard, standard library, configuration, a user policy key,
and repeatable adapter-specific keys. `--configuration debug` and
`--bmi-compatibility-key <value>` let the build system define policy without
requiring cxx-modgraph to infer a minimal compiler flag set. The other fields
have corresponding CLI options, and adapters can contribute repeatable
`--adapter-compatibility-key` values.

Providers are selected by logical name, module set, and the complete
compatibility scope. Consequently the same module (and even the same source)
may have independent debug, ASan, target, or toolchain translations.

The optional top-level `bmi-cache` array records module/module-set,
`source-digest`, `recipe-digest`, `compatibility-key`, `bmi-digest`, and optional
`object-digest`. These are cache metadata only: absence of a matching record is
a cache miss and leaves the source build in place.

## MSVC

The Make and Ninja adapter directories include `msvc.mk` and `msvc.ninja`.
They consume `/scanDependencies` P1689R5 output, generate `.ifc` paths and
`/reference name=path` mappings, and preserve the interface/object multi-output
relationship. The adapters add `/TP`, which MSVC requires for portable module
extensions such as `.cppm`.

## Explaining a graph

Graph queries remain available even when validation finds duplicate providers,
unresolved imports, or cycles:

```sh
cxx-modgraph --input dependencies.json explain module foo.bar
cxx-modgraph --input dependencies.json why app.cpp foo.bar
cxx-modgraph --input dependencies.json providers foo.bar
cxx-modgraph --input dependencies.json cycle
cxx-modgraph --input dependencies.json critical-path
```

Cycles print a deterministic concrete witness (`foo.cppm -> bar.cppm ->
foo.cppm`). Provider output names both source paths and, when recorded, scanner
and original-output configuration. The critical path is the longest chain by
translation-unit count; it is intentionally undefined for cyclic graphs.

## Graph freshness

P1689 imports record deterministic digests of every scan document and the paired
compilation database. Preserve those `inputs` when storing canonical facts, then
guard later use with `--check-fresh`:

```sh
cxx-modgraph --input build/dependencies.json --check-fresh --emit make
```

The command fails if an input is missing or changed. Scanner provenance can be
attached during import with `--scanner` and `--scanner-version`. Build adapters
should make their emitted graph depend on the P1689 files and compilation
database as the primary regeneration mechanism; the digest check is the safety
net for direct/manual invocation.

## Canonical dependency facts

The version 5 format records a source root, translation-unit source and object
paths, provided modules and their BMI paths, required module names, and external
prebuilt modules. It additionally records P1689/scanner provenance, interface
status, lookup methods, exact edge reasons, losslessly retained scan JSON, and
input digests. Version 3 adds compile and local arguments, working directories,
module sets, baseline arguments, and visibility. Version 5 adds hermetic and
remote-execution metadata. Older inputs remain accepted. Paths are normalized and serialized with `/` separators. Arrays
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

For Ninja, include the Clang rules followed by an emitted graph fragment:

```ninja
include adapters/ninja/clang.ninja
include build/modules.ninja
```

```sh
cxx-modgraph --input dependencies.json --emit ninja --output build/modules.ninja
```

Ninja dyndep is an additional lowering mode for builds where scanning happens
during the build. It emits only the Ninja dyndep file; it does not replace the
static fragment above:

```sh
cxx-modgraph --input dependencies.json --emit ninja-dyndep --output build/modules.dd
```

The consuming compile edges name `build/modules.dd` with Ninja's `dyndep`
binding. Provided BMIs become implicit outputs and directly imported BMIs become
implicit inputs.

## Optional incremental daemon

`--daemon` keeps the canonical graph and its provider index, reverse edges,
topological levels, and incremental caches in memory. The initial `--input` is
loaded normally. Thereafter, write one canonical snapshot path per line to
standard input; one JSON response is written per update:

```sh
cxx-modgraph --input build/dependencies.json --daemon
build/dependencies.next.json
```

The response contains `topology-changed` and the transitive set of affected
translation units. A line containing `quit` exits. Unchanged units are skipped.
The public `IncrementalState` API additionally exposes per-unit P1689 and compile
command digest caches and emitted backend fragment caching for embedding in a
file watcher or RPC service. Batch operation remains the default.

## Hermetic metadata

Canonical version 5 records ordered path remappings, declared environment input
digests, explicit tool identities, content digests for sources/sysroots/graph
records/outputs, and `host` versus `target` namespaces. Values are declarations:
cxx-modgraph never reads undeclared environment variables or hashes a sysroot on
its own. `remap_path`, `content_digest`, and `graph_record_digest` provide the
corresponding embedding utilities. The graph digest hashes canonical JSON with
its own `graph-digest` field cleared, so the address is deterministic and does
not depend on itself.

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
